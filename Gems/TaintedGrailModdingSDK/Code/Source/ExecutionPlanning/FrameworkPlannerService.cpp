/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "FrameworkPlannerService.h"
#include "../CanonicalFingerprint.h"
#include "../ExecutionFramework/FrameworkExecutionEvidenceProjection.h"
#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
        template<class T>
        CE::ContractValidation SealPreviewValue(T& value)
        {
            auto canonical = CE::Canonicalize(value);
            if (!canonical.IsSuccess())
            {
                return AZ::Failure(canonical.GetError());
            }
            value.m_fingerprint = canonical.GetValue().m_fingerprint;
            return CE::Validate(value);
        }
        const char* Key(PlannerSourceKind kind)
        {
            switch (kind)
            {
            case PlannerSourceKind::Build:
                return "foa.planner.build";
            case PlannerSourceKind::Package:
                return "foa.planner.package";
            case PlannerSourceKind::Deployment:
                return "foa.planner.deployment";
            case PlannerSourceKind::WorkOrder:
                return "foa.planner.workorder";
            }
            return "";
        }
        template<class T>
        bool Bounded(const AZStd::vector<T>& values)
        {
            return values.size() <= CE::MaximumCollection;
        }
        bool Inert(const AdapterWorkOrderPlan& plan)
        {
            return plan.m_formatVersion == 1 && !plan.m_executionAllowed && Bounded(plan.m_steps) &&
                AZStd::all_of(
                       plan.m_steps.begin(),
                       plan.m_steps.end(),
                       [](const auto& step)
                       {
                           return !step.m_executionAllowed && Bounded(step.m_arguments);
                       });
        }
        bool Inert(const AdapterPackageAssemblyPreview& preview)
        {
            return preview.m_formatVersion == 1 && !preview.m_assemblyAllowed && !preview.m_archiveAllowed &&
                !preview.m_deploymentAllowed && Bounded(preview.m_layout) && Bounded(preview.m_blockers) && Bounded(preview.m_omissions) &&
                Bounded(preview.m_collisions);
        }
        bool Inert(const AdapterStagingDeploymentPreview& preview)
        {
            return preview.m_formatVersion == 1 && !preview.m_stagingMutationAllowed && !preview.m_deploymentMutationAllowed &&
                !preview.m_rollbackExecutionAllowed && !preview.m_launchAllowed && Bounded(preview.m_additions) &&
                Bounded(preview.m_replacements) && Bounded(preview.m_removals) && Bounded(preview.m_unchanged) &&
                Bounded(preview.m_conflicts) && Bounded(preview.m_backups) && Bounded(preview.m_rollbackSteps) &&
                Bounded(preview.m_blockers);
        }
    } // namespace

    AdapterWorkOrderPlanSet FrameworkPlannerService::BuildPlans(
        const WorkspaceModel& workspace,
        const AZStd::vector<PackManifest>& packs,
        const AdapterContractRegistry& adapters,
        const SourceEvidenceRegistry& sources,
        const CatalogDatabase& catalog,
        const AZStd::vector<BlockerRecord>& blockers) const
    {
        return AdapterWorkOrderPlanningService{}.BuildPlans(workspace, packs, adapters, sources, catalog, blockers);
    }
    AdapterBuildManifest FrameworkPlannerService::BuildManifest(const AdapterBuildManifestRequest& request) const
    {
        return AdapterBuildManifestService{}.BuildManifest(request);
    }
    AdapterPackageAssemblyPreview FrameworkPlannerService::BuildPreview(const AdapterPackageAssemblyPreviewRequest& request) const
    {
        return AdapterPackageAssemblyPreviewService{}.BuildPreview(request);
    }
    AdapterStagingDeploymentPreview FrameworkPlannerService::BuildPreview(const AdapterStagingDeploymentPreviewRequest& request) const
    {
        return AdapterStagingDeploymentPreviewService{}.BuildPreview(request);
    }
    AdapterDeploymentWorkOrder FrameworkPlannerService::BuildWorkOrder(const AdapterDeploymentWorkOrderRequest& request) const
    {
        return AdapterDeploymentWorkOrderService{}.BuildWorkOrder(request);
    }
    AdapterPostDeploymentVerificationReport FrameworkPlannerService::BuildReport(
        const AdapterDeploymentWorkOrder& workOrder,
        const AdapterDeploymentExecutionResultEnvelope& envelope,
        const AdapterDeploymentExecutionEvidenceReturn& evidence) const
    {
        return AdapterPostDeploymentVerificationService{}.BuildReport(workOrder, envelope, evidence);
    }

    AZ::Outcome<const PlannerSource*, AZStd::string> PlannerSnapshot::ReadSource(
        PlannerSourceKind kind, CE::Phase phase, const CE::CapabilityExecutionRequestV1& request) const
    {
        if (m_sources.empty() || !CE::Validate(request).IsSuccess() || request.m_fingerprint != m_request.m_fingerprint)
        {
            return AZ::Failure(AZStd::string("Planner source requires the exact bound request; create a new preview after changes."));
        }
        const auto canonical = CE::Canonicalize(request);
        const auto expected = CE::Canonicalize(m_request);
        if (!canonical.IsSuccess() || !expected.IsSuccess() || canonical.GetValue().m_json != expected.GetValue().m_json)
        {
            return AZ::Failure(AZStd::string("Planner request canonical bytes have changed."));
        }
        for (const auto& source : m_sources)
        {
            if (source.m_kind == kind && source.m_phase == phase)
            {
                return AZ::Success(&source.m_value);
            }
        }
        return AZ::Failure(AZStd::string("This snapshot has no source for the requested planner and phase."));
    }

    AZ::Outcome<void, AZStd::string> FrameworkPlannerService::Append(
        PlannerSnapshot& snapshot, PlannerSourceKind kind, CE::Phase phase, const AZStd::string& json)
    {
        if (json.empty() || json.size() > CE::MaximumEmbeddedBytes || snapshot.m_request.m_options.size() >= CE::MaximumCollection ||
            snapshot.m_sources.size() >= 4)
        {
            return AZ::Failure(AZStd::string("Planner source exceeds the execution contract bounds."));
        }
        const AZStd::string key = Key(kind);
        if (key.empty() ||
            AZStd::any_of(
                snapshot.m_request.m_options.begin(),
                snapshot.m_request.m_options.end(),
                [&](const auto& option)
                {
                    return option.m_id == key;
                }))
        {
            return AZ::Failure(AZStd::string("Planner source option is already bound."));
        }
        PlannerSnapshot::Source source;
        source.m_kind = kind;
        source.m_phase = phase;
        source.m_value.m_canonicalJson = json;
        auto& extension = source.m_value.m_reference;
        extension.m_id = key + ".source";
        extension.m_extensionContractId = key + ".reference.v1";
        // Legacy diagnostic prose is not an M1 execution payload. Preserve it above,
        // and bind only this explicit digest reference through the frozen M1 contract.
        extension.m_canonicalJson =
            "{\"sourceContractId\":\"" + key + ".v1\",\"sourceFingerprint\":\"" + CalculateCanonicalSha256(json) + "\"}";
        extension.m_extensionFingerprint = CalculateCanonicalSha256(extension.m_canonicalJson);
        if (const auto sealed = SealPreviewValue(extension); !sealed.IsSuccess())
        {
            return AZ::Failure(AZStd::string("Planner source cannot bind: " + sealed.GetError()));
        }
        CE::OptionV1 option;
        option.m_id = key;
        option.m_value = extension.m_fingerprint;
        if (const auto sealed = SealPreviewValue(option); !sealed.IsSuccess())
        {
            return AZ::Failure(AZStd::string("Planner source option is invalid."));
        }
        snapshot.m_request.m_options.push_back(AZStd::move(option));
        if (const auto sealed = SealPreviewValue(snapshot.m_request); !sealed.IsSuccess())
        {
            return AZ::Failure(AZStd::string("Bound planner request exceeds execution contract limits."));
        }
        snapshot.m_sources.push_back(AZStd::move(source));
        return AZ::Success();
    }

    AZ::Outcome<PlannerSnapshot, AZStd::string> FrameworkPlannerService::BindBuild(
        const CE::CapabilityExecutionRequestV1& request, const AdapterBuildManifestRequest& input) const
    {
        if (!CE::Validate(request).IsSuccess() || request.m_terminalPhase < CE::Phase::BUILD ||
            request.m_terminalPhase > CE::Phase::DEPLOY || request.m_packId != input.m_pack.m_packId ||
            request.m_profileFingerprint != ProfileFingerprint(input.m_profile) || !Inert(input.m_plan) || !Bounded(input.m_materials) ||
            !Bounded(input.m_dependencies) || !Bounded(input.m_expectedOutputs))
        {
            return AZ::Failure(AZStd::string("Build preview requires an exact pack/profile, inert V1 plan and bounded inputs."));
        }
        if (AZStd::any_of(
                request.m_options.begin(),
                request.m_options.end(),
                [](const auto& option)
                {
                    return option.m_id.starts_with("foa.planner.");
                }))
        {
            return AZ::Failure(AZStd::string("Create a new unbound request instead of replacing planner source options."));
        }
        const auto manifest = BuildManifest(input);
        if (manifest.m_status != AdapterBuildManifestStatus::Ready)
        {
            return AZ::Failure(AZStd::string("Build manifest is not ready: ") + ToString(manifest.m_status));
        }
        PlannerSnapshot snapshot;
        snapshot.m_request = request;
        const auto result = Append(snapshot, PlannerSourceKind::Build, CE::Phase::BUILD, manifest.m_canonicalJson);
        if (!result.IsSuccess())
        {
            return AZ::Failure(result.GetError());
        }
        return AZ::Success(AZStd::move(snapshot));
    }

    AZ::Outcome<PlannerSnapshot, AZStd::string> FrameworkPlannerService::BindPackage(
        const PlannerSnapshot& build, const AdapterPackageAssemblyPreviewRequest& input) const
    {
        const auto source = build.ReadSource(PlannerSourceKind::Build, CE::Phase::BUILD, build.GetRequest());
        if (!source.IsSuccess() || build.m_sources.size() != 1 || build.m_request.m_terminalPhase < CE::Phase::PACKAGE ||
            input.m_manifest.m_formatVersion != 1 || input.m_manifest.m_buildAllowed || input.m_inventory.m_formatVersion != 1 ||
            !Bounded(input.m_inventory.m_entries) || !Bounded(input.m_manifest.m_materials) || !Bounded(input.m_manifest.m_dependencies) ||
            !Bounded(input.m_manifest.m_expectedOutputs) || input.m_manifest.m_canonicalJson != source.GetValue()->m_canonicalJson ||
            AdapterBuildManifestService{}.SerializeCanonicalManifest(input.m_manifest) != source.GetValue()->m_canonicalJson)
        {
            return AZ::Failure(AZStd::string("Package preview requires the exact bound V1 build manifest and bounded inventory."));
        }
        const auto preview = BuildPreview(input);
        if (preview.m_status != AdapterPackageAssemblyPreviewStatus::Ready)
        {
            return AZ::Failure(AZStd::string("Package preview is not ready: ") + ToString(preview.m_status));
        }
        auto snapshot = build;
        const auto result = Append(snapshot, PlannerSourceKind::Package, CE::Phase::PACKAGE, preview.m_canonicalJson);
        if (!result.IsSuccess())
        {
            return AZ::Failure(result.GetError());
        }
        return AZ::Success(AZStd::move(snapshot));
    }

    AZ::Outcome<PlannerSnapshot, AZStd::string> FrameworkPlannerService::BindDeployment(
        const PlannerSnapshot& package,
        const AdapterStagingDeploymentPreviewRequest& input,
        const AdapterDeploymentWorkOrderRequest& operatorInput) const
    {
        const auto source = package.ReadSource(PlannerSourceKind::Package, CE::Phase::PACKAGE, package.GetRequest());
        if (!source.IsSuccess() || package.m_sources.size() != 2 || package.m_request.m_terminalPhase != CE::Phase::DEPLOY ||
            !Inert(input.m_packagePreview) || input.m_targetInventory.m_formatVersion != 1 || !Bounded(input.m_targetInventory.m_entries) ||
            input.m_packagePreview.m_canonicalJson != source.GetValue()->m_canonicalJson ||
            AdapterPackageAssemblyPreviewService{}.SerializeCanonicalPreview(input.m_packagePreview) != source.GetValue()->m_canonicalJson)
        {
            return AZ::Failure(AZStd::string("Deployment preview requires the exact bound V1 package and bounded target inventory."));
        }
        const auto preview = BuildPreview(input);
        if (preview.m_status != AdapterStagingDeploymentPreviewStatus::Ready)
        {
            return AZ::Failure(AZStd::string("Deployment preview is not ready: ") + ToString(preview.m_status));
        }
        if (!Inert(operatorInput.m_preview) || !Bounded(operatorInput.m_preflightEvidence) ||
            operatorInput.m_preview.m_canonicalJson != preview.m_canonicalJson ||
            AdapterStagingDeploymentPreviewService{}.SerializeCanonicalPreview(operatorInput.m_preview) != preview.m_canonicalJson)
        {
            return AZ::Failure(AZStd::string("Operator work order must reference the exact current deployment preview."));
        }
        const auto workOrder = BuildWorkOrder(operatorInput);
        if (workOrder.m_status != AdapterDeploymentWorkOrderStatus::ReviewReady)
        {
            return AZ::Failure(AZStd::string("Deployment work order is not ready: ") + ToString(workOrder.m_status));
        }
        auto snapshot = package;
        auto result = Append(snapshot, PlannerSourceKind::Deployment, CE::Phase::DEPLOY, preview.m_canonicalJson);
        if (result.IsSuccess())
        {
            result = Append(snapshot, PlannerSourceKind::WorkOrder, CE::Phase::DEPLOY, workOrder.m_canonicalJson);
        }
        if (!result.IsSuccess())
        {
            return AZ::Failure(result.GetError());
        }
        return AZ::Success(AZStd::move(snapshot));
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
