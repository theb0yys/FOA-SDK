/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "CanonicalFingerprint.h"
#include "ExecutionFramework/FrameworkExecutionEvidenceProjection.h"
#include "ExecutionPlanning/FrameworkPlannerService.h"
#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK::ExecutionFramework::PlannerTests
{

    inline AZStd::string Fingerprint(char digit)
    {
        return AZStd::string("sha256:") + AZStd::string(64, digit);
    }

    inline AdapterBuildMaterial Material(
        AZStd::string id,
        AZStd::string role,
        AZStd::string locator,
        char fingerprintDigit,
        bool includeInPackage = false,
        bool redistributable = true)
    {
        AdapterBuildMaterial material;
        material.m_materialId = AZStd::move(id);
        material.m_role = AZStd::move(role);
        material.m_locator = AZStd::move(locator);
        material.m_mediaType = "application/octet-stream";
        material.m_fingerprint = Fingerprint(fingerprintDigit);
        material.m_required = true;
        material.m_includeInPackage = includeInPackage;
        material.m_redistributable = redistributable;
        return material;
    }

    inline AdapterBuildExpectedOutput Output(AZStd::string path, AZStd::string role, AZStd::string mediaType = "application/octet-stream")
    {
        AdapterBuildExpectedOutput output;
        output.m_relativePath = AZStd::move(path);
        output.m_role = AZStd::move(role);
        output.m_mediaType = AZStd::move(mediaType);
        output.m_redistributable = true;
        return output;
    }

    inline AdapterBuildManifestRequest MakeBuildRequest()
    {
        AdapterBuildManifestRequest request;
        request.m_pack.m_packId = "owner.preview-pack";
        request.m_pack.m_displayName = "Preview Pack";
        request.m_pack.m_version = "1.2.3";
        request.m_pack.m_requiredAdapterVersion = "0.4.0";
        request.m_pack.m_buildConfiguration = "Profile";

        request.m_profile.m_profileId = "foa.mono.preview";
        request.m_profile.m_gameVersion = "1.0.9";
        request.m_profile.m_branch = "mono";
        request.m_profile.m_runtimeTarget = "Mono";
        request.m_profile.m_unityVersion = "2022.3.22f1";
        request.m_profile.m_bepInExVersion = "5.4.23.3";

        request.m_declaration.m_adapterId = "owner.foa-adapter";
        request.m_declaration.m_displayName = "FoA Adapter";
        request.m_declaration.m_version = "0.4.1";
        request.m_declaration.m_runtimeTargets = { "Mono" };

        request.m_plan.m_planId = "workorder.plan:owner.preview-pack:owner.foa-adapter:foa.mono.preview";
        request.m_plan.m_packId = request.m_pack.m_packId;
        request.m_plan.m_packVersion = request.m_pack.m_version;
        request.m_plan.m_adapterId = request.m_declaration.m_adapterId;
        request.m_plan.m_adapterVersion = request.m_declaration.m_version;
        request.m_plan.m_requiredAdapterVersion = request.m_pack.m_requiredAdapterVersion;
        request.m_plan.m_profileId = request.m_profile.m_profileId;
        request.m_plan.m_gameVersion = request.m_profile.m_gameVersion;
        request.m_plan.m_branch = request.m_profile.m_branch;
        request.m_plan.m_runtimeTarget = request.m_profile.m_runtimeTarget;
        request.m_plan.m_executionAllowed = false;

        AdapterWorkOrderStep step;
        step.m_stepId = request.m_plan.m_planId + ":step:item_grant:record:item.preview";
        step.m_sequence = 1;
        step.m_capability = "item_grant";
        step.m_subjectKind = "record";
        step.m_subjectId = "item.preview";
        step.m_executionAllowed = false;
        request.m_plan.m_steps.push_back(step);
        AdapterWorkOrderPlanningService planningService;
        request.m_plan.m_canonicalJson = planningService.SerializeCanonicalPlan(request.m_plan);

        request.m_environment.m_builderId = "tg.builder.dotnet";
        request.m_environment.m_builderVersion = "1.0.0";
        request.m_environment.m_sourceCommit = AZStd::string(40, 'a');
        request.m_environment.m_o3deRevision = AZStd::string(40, 'b');
        request.m_environment.m_configuration = "Profile";
        request.m_environment.m_targetFramework = "net472";
        request.m_environment.m_compilerId = "dotnet.csharp";
        request.m_environment.m_compilerVersion = "8.0.0";
        request.m_environment.m_deterministicBuild = true;
        request.m_environment.m_continuousIntegrationBuild = true;
        request.m_environment.m_pathMapEnabled = true;

        request.m_planFingerprint = CalculateCanonicalSha256(request.m_plan.m_canonicalJson);
        request.m_pluginGuid = "owner.preview-plugin";
        request.m_pluginName = "Preview Plugin";
        request.m_pluginVersion = "1.2.3";
        request.m_packageRoot = "BepInEx/plugins/owner.preview-pack";

        AdapterBuildDependency dependency;
        dependency.m_pluginId = "bepinex.core";
        dependency.m_version = "5.4.23";
        dependency.m_kind = AdapterBuildDependencyKind::Hard;
        request.m_dependencies.push_back(dependency);

        request.m_materials = {
            Material("material.plan", "work_order_plan", "Reports/WorkOrders/preview.json", '1', false, false),
            Material("material.source", "source_tree", "Source", '2', false, false),
            Material("material.dependencies", "dependency_lock", "Build/dependencies.lock.json", '3'),
            Material("material.toolchain", "toolchain_lock", "Build/toolchain.lock.json", '4'),
            Material("material.license", "license", "LICENSE", '5', true, true),
        };
        request.m_materials.front().m_fingerprint = request.m_planFingerprint;

        const AZStd::string root = request.m_packageRoot + "/";
        request.m_expectedOutputs = {
            Output(root + "owner.preview-pack.dll", "plugin_binary"),    Output(root + "README.md", "readme", "text/markdown"),
            Output(root + "CHANGELOG.md", "changelog", "text/markdown"), Output(root + "MANIFEST.md", "manifest", "text/markdown"),
            Output(root + "LICENSE", "license", "text/plain"),
        };
        return request;
    }
    inline AdapterDeploymentWorkOrderRequest MakeOperatorRequest(const AdapterStagingDeploymentPreview& preview)
    {
        AdapterDeploymentWorkOrderRequest request;
        request.m_preview = preview;
        request.m_previewFingerprint = CalculateCanonicalSha256(preview.m_canonicalJson);

        request.m_confirmation.m_confirmationId = "owner.confirmation";
        request.m_confirmation.m_previewId = preview.m_previewId;
        request.m_confirmation.m_previewFingerprint = request.m_previewFingerprint;
        request.m_confirmation.m_decision = AdapterDeploymentConfirmationDecision::Confirmed;
        request.m_confirmation.m_scope = AdapterDeploymentConfirmationScope::FullPreview;
        request.m_confirmation.m_reviewer = "release-reviewer";
        request.m_confirmation.m_evidenceIds = { "evidence.confirmation" };
        request.m_confirmation.m_issuedAtUtc = "2026-07-19T12:00:00Z";
        request.m_confirmation.m_expiresAtUtc = "2026-07-19T14:00:00Z";

        request.m_maintenanceWindow.m_windowId = "owner.window";
        request.m_maintenanceWindow.m_previewId = preview.m_previewId;
        request.m_maintenanceWindow.m_previewFingerprint = request.m_previewFingerprint;
        request.m_maintenanceWindow.m_startAtUtc = "2026-07-19T12:30:00Z";
        request.m_maintenanceWindow.m_endAtUtc = "2026-07-19T13:30:00Z";
        request.m_maintenanceWindow.m_operatorGroup = "release-operators";
        request.m_maintenanceWindow.m_evidenceIds = { "evidence.window" };
        request.m_evaluatedAtUtc = "2026-07-19T13:00:00Z";

        AZStd::vector<AdapterDeploymentPreflightKind> kinds = {
            AdapterDeploymentPreflightKind::PackageIntegrity,
            AdapterDeploymentPreflightKind::TargetInventory,
            AdapterDeploymentPreflightKind::RollbackReadiness,
            AdapterDeploymentPreflightKind::OperatorReadiness,
        };
        if (!preview.m_backups.empty())
        {
            kinds.push_back(AdapterDeploymentPreflightKind::BackupReadiness);
        }

        for (size_t index = 0; index < kinds.size(); ++index)
        {
            AdapterDeploymentPreflightEvidence evidence;
            evidence.m_preflightId = AZStd::string::format("owner.preflight-%llu", static_cast<unsigned long long>(index + 1));
            evidence.m_kind = kinds[index];
            evidence.m_status = AdapterDeploymentPreflightStatus::Passed;
            evidence.m_previewId = preview.m_previewId;
            evidence.m_previewFingerprint = request.m_previewFingerprint;
            evidence.m_checkedAtUtc = "2026-07-19T12:45:00Z";
            evidence.m_checker = "preflight-checker";
            evidence.m_evidenceIds = {
                AZStd::string::format("evidence.preflight-%llu", static_cast<unsigned long long>(index + 1)),
            };
            request.m_preflightEvidence.push_back(AZStd::move(evidence));
        }
        return request;
    }

    inline CE::CapabilityExecutionRequestV1 MakeRequest(const AdapterBuildManifestRequest& input)
    {
        CE::CapabilityExecutionRequestV1 request;
        request.m_id = "request.planner";
        request.m_workspaceId = "workspace.planner";
        request.m_packId = input.m_pack.m_packId;
        request.m_profileFingerprint = ProfileFingerprint(input.m_profile);
        request.m_capabilityId = "capability.planner";
        request.m_terminalPhase = CE::Phase::DEPLOY;
        EXPECT_TRUE(Seal(request));
        return request;
    }
    inline AdapterPackageAssemblyPreviewRequest MakePackageRequest(const AdapterBuildManifest& manifest)
    {
        AdapterPackageAssemblyPreviewRequest request;
        request.m_manifest = manifest;
        request.m_review.m_reviewId = "review.planner";
        request.m_review.m_manifestId = manifest.m_manifestId;
        request.m_review.m_manifestFingerprint = CalculateCanonicalSha256(manifest.m_canonicalJson);
        request.m_review.m_decision = AdapterBuildManifestReviewDecision::Accepted;
        request.m_review.m_reviewer = "reviewer.synthetic";
        request.m_review.m_evidenceIds = { "evidence.synthetic" };
        request.m_inventory.m_inventoryId = "inventory.planner";
        request.m_inventory.m_manifestId = manifest.m_manifestId;
        request.m_inventory.m_manifestFingerprint = request.m_review.m_manifestFingerprint;
        request.m_inventory.m_packId = manifest.m_packId;
        request.m_inventory.m_packageRoot = manifest.m_packageRoot;
        size_t index = 0;
        for (const auto& output : manifest.m_expectedOutputs)
        {
            AdapterStagingInventoryEntry entry;
            entry.m_entryId = AZStd::string::format("entry.planner.%zu", ++index);
            entry.m_stagingPath = "Staging/" + output.m_relativePath;
            entry.m_packagePath = output.m_relativePath;
            entry.m_role = output.m_role;
            entry.m_mediaType = output.m_mediaType;
            entry.m_outputFingerprint = Fingerprint('1');
            entry.m_byteSize = 123;
            entry.m_projectOwned = true;
            entry.m_redistributable = true;
            request.m_inventory.m_entries.push_back(entry);
        }
        return request;
    }
    inline AdapterStagingDeploymentPreviewRequest MakeDeploymentRequest(const AdapterPackageAssemblyPreview& package)
    {
        AdapterStagingDeploymentPreviewRequest request;
        request.m_packagePreview = package;
        request.m_packagePreviewFingerprint = CalculateCanonicalSha256(package.m_canonicalJson);
        auto& inventory = request.m_targetInventory;
        inventory.m_inventoryId = "inventory.target";
        inventory.m_inventoryFingerprint = Fingerprint('2');
        inventory.m_packagePreviewId = package.m_previewId;
        inventory.m_packagePreviewFingerprint = request.m_packagePreviewFingerprint;
        inventory.m_packId = package.m_packId;
        inventory.m_targetRoot = package.m_packageRoot;
        inventory.m_backupRoot = "Backups/planner";
        auto& review = request.m_targetReview;
        review.m_reviewId = "review.target";
        review.m_inventoryId = inventory.m_inventoryId;
        review.m_inventoryFingerprint = inventory.m_inventoryFingerprint;
        review.m_decision = AdapterDeploymentTargetReviewDecision::Accepted;
        review.m_reviewer = "reviewer.synthetic";
        review.m_evidenceIds = { "evidence.target" };
        return request;
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework::PlannerTests
