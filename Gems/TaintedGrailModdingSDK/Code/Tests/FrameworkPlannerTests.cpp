/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "AdapterResearchPipelineTestFixture.h"
#include "ExecutionFramework/FrameworkProviderService.h"
#include "FoundationService.h"
#include "FrameworkPlannerTestFixtures.h"
#include <AzCore/std/algorithm.h>
#include <chrono>

namespace TaintedGrailModdingSDK::ExecutionFramework::PlannerTests
{
    class FrameworkPlanner : public ::testing::Test
    {
    protected:
        FrameworkPlannerService m_service;
        AdapterBuildManifestRequest m_build = MakeBuildRequest();
        CE::CapabilityExecutionRequestV1 m_request = MakeRequest(m_build);
        AdapterBuildManifest m_manifest = m_service.BuildManifest(m_build);
        AdapterPackageAssemblyPreviewRequest m_package = MakePackageRequest(m_manifest);
        AdapterPackageAssemblyPreview m_packagePreview = m_service.BuildPreview(m_package);
        AdapterStagingDeploymentPreviewRequest m_deploy = MakeDeploymentRequest(m_packagePreview);
        AdapterStagingDeploymentPreview m_deploymentPreview = m_service.BuildPreview(m_deploy);
        AdapterDeploymentWorkOrderRequest m_operator = MakeOperatorRequest(m_deploymentPreview);
        void SetUp() override
        {
            ASSERT_EQ(m_manifest.m_status, AdapterBuildManifestStatus::Ready);
            ASSERT_EQ(m_packagePreview.m_status, AdapterPackageAssemblyPreviewStatus::Ready);
            ASSERT_EQ(m_deploymentPreview.m_status, AdapterStagingDeploymentPreviewStatus::Ready);
            ASSERT_EQ(m_service.BuildWorkOrder(m_operator).m_status, AdapterDeploymentWorkOrderStatus::ReviewReady);
        }
        PlannerSnapshot Package()
        {
            auto build = m_service.BindBuild(m_request, m_build);
            EXPECT_TRUE(build.IsSuccess()) << (build.IsSuccess() ? "" : build.GetError().c_str());
            if (!build.IsSuccess())
            {
                return {};
            }
            auto package = m_service.BindPackage(build.GetValue(), m_package);
            EXPECT_TRUE(package.IsSuccess()) << (package.IsSuccess() ? "" : package.GetError().c_str());
            return package.IsSuccess() ? package.TakeValue() : PlannerSnapshot{};
        }
    };

    TEST_F(FrameworkPlanner, FoundationExposesPreviewServiceWithoutExecutionSetup)
    {
        FoundationService foundation(FoundationWorkspaceLoadDependencies{});
        EXPECT_EQ(foundation.GetFrameworkExecution(), nullptr);
        EXPECT_EQ(foundation.GetFrameworkPlanners().BuildManifest(m_build).m_canonicalJson, m_manifest.m_canonicalJson);
        EXPECT_EQ(foundation.GetFrameworkExecution(), nullptr);
    }

    TEST_F(FrameworkPlanner, FullChainRetainsExactOwnerBytesAndAllV1FlagsRemainInert)
    {
        auto package = Package();
        auto result = m_service.BindDeployment(package, m_deploy, m_operator);
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
        const auto& snapshot = result.GetValue();
        const auto& request = snapshot.GetRequest();
        EXPECT_TRUE(CE::Validate(request).IsSuccess());
        EXPECT_NE(request.m_fingerprint, package.GetRequest().m_fingerprint);
        auto build = snapshot.ReadSource(PlannerSourceKind::Build, CE::Phase::BUILD, request);
        auto pkg = snapshot.ReadSource(PlannerSourceKind::Package, CE::Phase::PACKAGE, request);
        auto deploy = snapshot.ReadSource(PlannerSourceKind::Deployment, CE::Phase::DEPLOY, request);
        auto order = snapshot.ReadSource(PlannerSourceKind::WorkOrder, CE::Phase::DEPLOY, request);
        ASSERT_TRUE(build.IsSuccess());
        ASSERT_TRUE(pkg.IsSuccess());
        ASSERT_TRUE(deploy.IsSuccess());
        ASSERT_TRUE(order.IsSuccess());
        EXPECT_EQ(build.GetValue()->m_canonicalJson, m_manifest.m_canonicalJson);
        EXPECT_EQ(pkg.GetValue()->m_canonicalJson, m_packagePreview.m_canonicalJson);
        EXPECT_EQ(deploy.GetValue()->m_canonicalJson, m_deploymentPreview.m_canonicalJson);
        const auto workOrder = m_service.BuildWorkOrder(m_operator);
        EXPECT_EQ(order.GetValue()->m_canonicalJson, workOrder.m_canonicalJson);
        EXPECT_FALSE(workOrder.m_executionAllowed);
        EXPECT_FALSE(workOrder.m_copyAllowed);
        EXPECT_FALSE(workOrder.m_deleteAllowed);
        EXPECT_FALSE(workOrder.m_backupAllowed);
        EXPECT_FALSE(workOrder.m_restoreAllowed);
        EXPECT_FALSE(workOrder.m_deploymentAllowed);
        EXPECT_FALSE(workOrder.m_launchAllowed);
        EXPECT_FALSE(m_manifest.m_buildAllowed);
        EXPECT_FALSE(m_packagePreview.m_assemblyAllowed);
        EXPECT_FALSE(m_packagePreview.m_archiveAllowed);
        EXPECT_FALSE(m_packagePreview.m_deploymentAllowed);
        EXPECT_FALSE(m_deploymentPreview.m_stagingMutationAllowed);
        EXPECT_FALSE(m_deploymentPreview.m_deploymentMutationAllowed);
        EXPECT_FALSE(m_deploymentPreview.m_rollbackExecutionAllowed);
        EXPECT_FALSE(m_deploymentPreview.m_launchAllowed);
        for (const auto& step : workOrder.m_steps)
        {
            EXPECT_FALSE(step.m_executionAllowed);
        }
        EXPECT_FALSE(package.ReadSource(PlannerSourceKind::Build, CE::Phase::BUILD, request).IsSuccess());
    }

    TEST_F(FrameworkPlanner, LegacyProseStaysExactInMemoryWhileOnlyDigestReferenceEntersM1)
    {
        ASSERT_NE(m_manifest.m_canonicalJson.find(';'), AZStd::string::npos);
        CE::PhaseExtensionReferenceV1 raw;
        raw.m_id = "test.planner.source";
        raw.m_extensionContractId = "test.planner.v1";
        raw.m_canonicalJson = m_manifest.m_canonicalJson;
        raw.m_extensionFingerprint = CalculateCanonicalSha256(raw.m_canonicalJson);
        EXPECT_FALSE(CE::Canonicalize(raw).IsSuccess());
        auto bound = m_service.BindBuild(m_request, m_build);
        ASSERT_TRUE(bound.IsSuccess()) << bound.GetError().c_str();
        auto source = bound.GetValue().ReadSource(PlannerSourceKind::Build, CE::Phase::BUILD, bound.GetValue().GetRequest());
        ASSERT_TRUE(source.IsSuccess());
        const auto& value = *source.GetValue();
        EXPECT_EQ(value.m_canonicalJson, m_manifest.m_canonicalJson);
        EXPECT_TRUE(CE::Validate(value.m_reference).IsSuccess());
        EXPECT_NE(value.m_reference.m_canonicalJson.find(CalculateCanonicalSha256(value.m_canonicalJson)), AZStd::string::npos);
        EXPECT_EQ(value.m_reference.m_canonicalJson.find(';'), AZStd::string::npos);
        EXPECT_EQ(bound.GetValue().GetRequest().m_options.back().m_value, value.m_reference.m_fingerprint);
    }

    TEST_F(FrameworkPlanner, RepeatedPreviewIsDeterministicAndDoesNotMutateInputs)
    {
        auto first = m_service.BindBuild(m_request, m_build);
        auto second = m_service.BindBuild(m_request, m_build);
        ASSERT_TRUE(first.IsSuccess());
        ASSERT_TRUE(second.IsSuccess());
        EXPECT_EQ(first.GetValue().GetRequest().m_fingerprint, second.GetValue().GetRequest().m_fingerprint);
        EXPECT_TRUE(m_request.m_options.empty());
        EXPECT_EQ(m_build.m_plan.m_canonicalJson, AdapterWorkOrderPlanningService{}.SerializeCanonicalPlan(m_build.m_plan));
        auto reordered = m_build;
        AZStd::reverse(reordered.m_materials.begin(), reordered.m_materials.end());
        auto third = m_service.BindBuild(m_request, reordered);
        ASSERT_TRUE(third.IsSuccess());
        EXPECT_EQ(first.GetValue().GetRequest().m_fingerprint, third.GetValue().GetRequest().m_fingerprint);
    }

    TEST_F(FrameworkPlanner, ReadyBuildStillNeedsExactPackProfileAndRequest)
    {
        auto changed = m_request;
        changed.m_packId = "pack.other";
        ASSERT_TRUE(Seal(changed));
        EXPECT_FALSE(m_service.BindBuild(changed, m_build).IsSuccess());
        changed = m_request;
        changed.m_profileFingerprint = Fingerprint('f');
        ASSERT_TRUE(Seal(changed));
        EXPECT_FALSE(m_service.BindBuild(changed, m_build).IsSuccess());
        changed = m_request;
        changed.m_fingerprint = Fingerprint('e');
        EXPECT_FALSE(m_service.BindBuild(changed, m_build).IsSuccess());
        changed = m_request;
        changed.m_header.m_version = 2;
        EXPECT_FALSE(m_service.BindBuild(changed, m_build).IsSuccess());
        changed = m_request;
        changed.m_terminalPhase = CE::Phase::LAUNCH;
        ASSERT_TRUE(Seal(changed));
        EXPECT_FALSE(m_service.BindBuild(changed, m_build).IsSuccess());
    }

    TEST_F(FrameworkPlanner, ChangedRequestCannotReadBoundSourceEvenAfterResealing)
    {
        const auto snapshot = Package();
        for (int field = 0; field < 5; ++field)
        {
            auto request = snapshot.GetRequest();
            if (field == 0)
            {
                request.m_workspaceId = "workspace.other";
            }
            if (field == 1)
            {
                request.m_packId = "pack.other";
            }
            if (field == 2)
            {
                request.m_profileFingerprint = Fingerprint('f');
            }
            if (field == 3)
            {
                request.m_id = "request.other";
            }
            if (field == 4)
            {
                request.m_options.pop_back();
            }
            ASSERT_TRUE(Seal(request));
            EXPECT_FALSE(snapshot.ReadSource(PlannerSourceKind::Build, CE::Phase::BUILD, request).IsSuccess());
        }
        EXPECT_FALSE(snapshot.ReadSource(PlannerSourceKind::Build, CE::Phase::PACKAGE, snapshot.GetRequest()).IsSuccess());
        EXPECT_FALSE(snapshot.ReadSource(PlannerSourceKind::WorkOrder, CE::Phase::DEPLOY, snapshot.GetRequest()).IsSuccess());
    }

    TEST_F(FrameworkPlanner, CannotOverwriteReservedOptionsOrBindStagesTwice)
    {
        auto build = m_service.BindBuild(m_request, m_build);
        ASSERT_TRUE(build.IsSuccess());
        EXPECT_FALSE(m_service.BindBuild(build.GetValue().GetRequest(), m_build).IsSuccess());
        const auto snapshot = Package();
        EXPECT_FALSE(m_service.BindPackage(snapshot, m_package).IsSuccess());
        EXPECT_FALSE(m_service.BindDeployment(build.GetValue(), m_deploy, m_operator).IsSuccess());
        EXPECT_FALSE(m_service.BindPackage(PlannerSnapshot{}, m_package).IsSuccess());
    }

    TEST_F(FrameworkPlanner, BuildRefusalRemainsInspectableAndCannotBind)
    {
        m_build.m_materials.clear();
        const auto direct = AdapterBuildManifestService{}.BuildManifest(m_build);
        const auto wrapped = m_service.BuildManifest(m_build);
        EXPECT_NE(direct.m_status, AdapterBuildManifestStatus::Ready);
        EXPECT_EQ(wrapped.m_canonicalJson, direct.m_canonicalJson);
        EXPECT_EQ(wrapped.m_reasons, direct.m_reasons);
        EXPECT_FALSE(m_service.BindBuild(m_request, m_build).IsSuccess());
    }

    TEST_F(FrameworkPlanner, TamperedV1VersionsAndFlagsCannotBind)
    {
        auto changed = m_build;
        changed.m_plan.m_executionAllowed = true;
        EXPECT_FALSE(m_service.BindBuild(m_request, changed).IsSuccess());
        changed = m_build;
        changed.m_plan.m_steps.front().m_executionAllowed = true;
        EXPECT_FALSE(m_service.BindBuild(m_request, changed).IsSuccess());
        changed = m_build;
        changed.m_plan.m_formatVersion = 2;
        EXPECT_FALSE(m_service.BindBuild(m_request, changed).IsSuccess());
        auto build = m_service.BindBuild(m_request, m_build);
        ASSERT_TRUE(build.IsSuccess());
        auto pkg = m_package;
        pkg.m_manifest.m_buildAllowed = true;
        EXPECT_FALSE(m_service.BindPackage(build.GetValue(), pkg).IsSuccess());
        pkg = m_package;
        pkg.m_inventory.m_formatVersion = 2;
        EXPECT_FALSE(m_service.BindPackage(build.GetValue(), pkg).IsSuccess());
        auto snapshot = Package();
        auto deployment = m_deploy;
        deployment.m_packagePreview.m_archiveAllowed = true;
        EXPECT_FALSE(m_service.BindDeployment(snapshot, deployment, m_operator).IsSuccess());
        auto order = m_operator;
        order.m_preview.m_deploymentMutationAllowed = true;
        EXPECT_FALSE(m_service.BindDeployment(snapshot, m_deploy, order).IsSuccess());
    }

    TEST_F(FrameworkPlanner, ManifestDriftCannotBeHiddenBehindCachedCanonicalBytes)
    {
        auto build = m_service.BindBuild(m_request, m_build);
        ASSERT_TRUE(build.IsSuccess());
        auto pkg = m_package;
        pkg.m_manifest.m_environment.m_compilerVersion = "9.0.0";
        EXPECT_FALSE(m_service.BindPackage(build.GetValue(), pkg).IsSuccess());
        pkg = m_package;
        pkg.m_manifest.m_canonicalJson += " ";
        EXPECT_FALSE(m_service.BindPackage(build.GetValue(), pkg).IsSuccess());
        pkg = m_package;
        pkg.m_inventory.m_manifestFingerprint = Fingerprint('f');
        EXPECT_FALSE(m_service.BindPackage(build.GetValue(), pkg).IsSuccess());
    }

    TEST_F(FrameworkPlanner, PackageMissingOutputAndReviewRefusalsRemainExact)
    {
        auto build = m_service.BindBuild(m_request, m_build);
        ASSERT_TRUE(build.IsSuccess());
        m_package.m_inventory.m_entries.pop_back();
        const auto direct = AdapterPackageAssemblyPreviewService{}.BuildPreview(m_package);
        const auto wrapped = m_service.BuildPreview(m_package);
        EXPECT_EQ(wrapped.m_canonicalJson, direct.m_canonicalJson);
        EXPECT_EQ(wrapped.m_status, AdapterPackageAssemblyPreviewStatus::OutputMissing);
        EXPECT_FALSE(wrapped.m_omissions.empty());
        EXPECT_FALSE(m_service.BindPackage(build.GetValue(), m_package).IsSuccess());
        m_package = MakePackageRequest(m_manifest);
        m_package.m_review.m_decision = AdapterBuildManifestReviewDecision::Rejected;
        EXPECT_EQ(m_service.BuildPreview(m_package).m_status, AdapterPackageAssemblyPreviewStatus::ManifestUnreviewed);
        EXPECT_FALSE(m_service.BindPackage(build.GetValue(), m_package).IsSuccess());
    }

    TEST_F(FrameworkPlanner, DeploymentRejectsPackageDriftAndTargetMismatch)
    {
        const auto snapshot = Package();
        auto deployment = m_deploy;
        deployment.m_packagePreview.m_layout.front().m_outputDigest = Fingerprint('f');
        EXPECT_FALSE(m_service.BindDeployment(snapshot, deployment, m_operator).IsSuccess());
        deployment = m_deploy;
        deployment.m_targetInventory.m_packagePreviewFingerprint = Fingerprint('f');
        const auto direct = AdapterStagingDeploymentPreviewService{}.BuildPreview(deployment);
        EXPECT_EQ(m_service.BuildPreview(deployment).m_canonicalJson, direct.m_canonicalJson);
        EXPECT_FALSE(m_service.BindDeployment(snapshot, deployment, m_operator).IsSuccess());
        deployment = m_deploy;
        deployment.m_targetInventory.m_inventoryFingerprint = Fingerprint('f');
        EXPECT_FALSE(m_service.BindDeployment(snapshot, deployment, m_operator).IsSuccess());
    }

    TEST_F(FrameworkPlanner, ExpiredOrStaleOperatorWorkOrderCannotBind)
    {
        const auto snapshot = Package();
        auto order = m_operator;
        order.m_evaluatedAtUtc = "2026-07-19T15:00:00Z";
        auto direct = AdapterDeploymentWorkOrderService{}.BuildWorkOrder(order);
        EXPECT_EQ(direct.m_status, AdapterDeploymentWorkOrderStatus::ConfirmationExpired);
        EXPECT_EQ(m_service.BuildWorkOrder(order).m_canonicalJson, direct.m_canonicalJson);
        EXPECT_FALSE(m_service.BindDeployment(snapshot, m_deploy, order).IsSuccess());
        order = m_operator;
        order.m_preview.m_targetInventoryFingerprint = Fingerprint('f');
        EXPECT_FALSE(m_service.BindDeployment(snapshot, m_deploy, order).IsSuccess());
        order = m_operator;
        order.m_previewFingerprint = Fingerprint('f');
        EXPECT_FALSE(m_service.BindDeployment(snapshot, m_deploy, order).IsSuccess());
        order = m_operator;
        order.m_preflightEvidence.clear();
        EXPECT_FALSE(m_service.BindDeployment(snapshot, m_deploy, order).IsSuccess());
    }

    TEST_F(FrameworkPlanner, FullPreviewDoesNotMakeM3DeploymentSupported)
    {
        CE::CapabilityDescriptorV1 descriptor;
        descriptor.m_id = "descriptor.deploy";
        descriptor.m_capabilityId = m_request.m_capabilityId;
        descriptor.m_requiredPhases = { CE::Phase::BUILD, CE::Phase::PACKAGE, CE::Phase::DEPLOY };
        descriptor.m_terminalPhase = CE::Phase::DEPLOY;
        descriptor.m_sideEffects = { CE::SideEffect::INSTALLATION_MUTATION };
        descriptor.m_saveImpact = CE::SideEffect::READ_ONLY;
        descriptor.m_rollbackRequired = CE::RollbackSupport::EXACT_RESTORE;
        ASSERT_TRUE(Seal(descriptor));
        EXPECT_FALSE(FrameworkProviderService::Supported(descriptor));
    }

    TEST_F(FrameworkPlanner, BoundsRejectOneOverAndDoNotTruncateSource)
    {
        auto changed = m_build;
        changed.m_materials.resize(CE::MaximumCollection + 1, changed.m_materials.front());
        EXPECT_FALSE(m_service.BindBuild(m_request, changed).IsSuccess());
        auto build = m_service.BindBuild(m_request, m_build);
        ASSERT_TRUE(build.IsSuccess());
        auto pkg = m_package;
        pkg.m_inventory.m_entries.resize(CE::MaximumCollection + 1, pkg.m_inventory.m_entries.front());
        EXPECT_FALSE(m_service.BindPackage(build.GetValue(), pkg).IsSuccess());
        auto snapshot = Package();
        auto order = m_operator;
        order.m_preflightEvidence.resize(CE::MaximumCollection + 1, order.m_preflightEvidence.front());
        EXPECT_FALSE(m_service.BindDeployment(snapshot, m_deploy, order).IsSuccess());
        changed = m_build;
        changed.m_pluginName.assign(CE::MaximumEmbeddedBytes + 1, 'a');
        EXPECT_FALSE(m_service.BindBuild(m_request, changed).IsSuccess());
    }

    TEST_F(FrameworkPlanner, MaximumCanonicalSourceIsExactAndOneOverIsRejected)
    {
        auto input = m_build;
        input.m_pluginName.append(CE::MaximumEmbeddedBytes - m_manifest.m_canonicalJson.size(), 'a');
        const auto manifest = m_service.BuildManifest(input);
        ASSERT_EQ(manifest.m_canonicalJson.size(), CE::MaximumEmbeddedBytes);
        const auto started = std::chrono::steady_clock::now();
        const auto bound = m_service.BindBuild(m_request, input);
        ASSERT_TRUE(bound.IsSuccess()) << bound.GetError().c_str();
        const auto source = bound.GetValue().ReadSource(PlannerSourceKind::Build, CE::Phase::BUILD, bound.GetValue().GetRequest());
        ASSERT_TRUE(source.IsSuccess());
        EXPECT_EQ(source.GetValue()->m_canonicalJson, manifest.m_canonicalJson);
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        printf("M4 maximum source: %zu bytes bound/read in %.6fs\n", manifest.m_canonicalJson.size(), seconds);
        EXPECT_LT(seconds, 5.0);
        input.m_pluginName += 'a';
        EXPECT_FALSE(m_service.BindBuild(m_request, input).IsSuccess());
    }

    TEST_F(FrameworkPlanner, ReplacementRemovalAndInverseRollbackRemainExact)
    {
        auto deployment = m_deploy;
        const auto& layout = m_packagePreview.m_layout.front();
        AdapterDeploymentTargetEntry target;
        target.m_entryId = "target.replaced";
        target.m_targetPath = layout.m_packagePath;
        target.m_role = layout.m_role;
        target.m_mediaType = layout.m_mediaType;
        target.m_fingerprint = Fingerprint('8');
        target.m_byteSize = 256;
        target.m_ownerPackId = m_manifest.m_packId;
        target.m_projectOwned = true;
        target.m_managed = true;
        target.m_replaceable = true;
        target.m_removable = true;
        deployment.m_targetInventory.m_entries.push_back(target);
        target.m_entryId = "target.removed";
        target.m_targetPath = m_manifest.m_packageRoot + "/obsolete.dll";
        deployment.m_targetInventory.m_entries.push_back(target);
        const auto direct = AdapterStagingDeploymentPreviewService{}.BuildPreview(deployment);
        ASSERT_EQ(direct.m_status, AdapterStagingDeploymentPreviewStatus::Ready);
        ASSERT_EQ(direct.m_replacements.size(), 1);
        ASSERT_EQ(direct.m_removals.size(), 1);
        ASSERT_EQ(direct.m_backups.size(), 2);
        auto order = MakeOperatorRequest(direct);
        const auto result = m_service.BindDeployment(Package(), deployment, order);
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
        const auto source = result.GetValue().ReadSource(PlannerSourceKind::Deployment, CE::Phase::DEPLOY, result.GetValue().GetRequest());
        ASSERT_TRUE(source.IsSuccess());
        EXPECT_EQ(source.GetValue()->m_canonicalJson, direct.m_canonicalJson);
        order.m_preview.m_rollbackSteps.front().m_restoreFingerprint = Fingerprint('f');
        EXPECT_FALSE(m_service.BindDeployment(Package(), deployment, order).IsSuccess());
    }

    TEST_F(FrameworkPlanner, BindingCostIsBoundedAndWrapperPreservesDirectResult)
    {
        using Clock = std::chrono::steady_clock;
        const auto directStart = Clock::now();
        for (int i = 0; i < 100; ++i)
        {
            EXPECT_EQ(AdapterBuildManifestService{}.BuildManifest(m_build).m_canonicalJson, m_manifest.m_canonicalJson);
        }
        const auto direct = std::chrono::duration<double>(Clock::now() - directStart).count();
        const auto wrapperStart = Clock::now();
        for (int i = 0; i < 100; ++i)
        {
            EXPECT_EQ(m_service.BuildManifest(m_build).m_canonicalJson, m_manifest.m_canonicalJson);
        }
        const auto wrapper = std::chrono::duration<double>(Clock::now() - wrapperStart).count();
        const auto bindingStart = Clock::now();
        for (int i = 0; i < 100; ++i)
        {
            ASSERT_TRUE(m_service.BindBuild(m_request, m_build).IsSuccess());
        }
        const auto binding = std::chrono::duration<double>(Clock::now() - bindingStart).count();
        printf(
            "M4 performance: 100 previews direct=%.6fs wrapper=%.6fs bindings=%.6fs source=%zu bytes\n",
            direct,
            wrapper,
            binding,
            m_manifest.m_canonicalJson.size());
        EXPECT_LT(binding, 5.0);
        EXPECT_LT(wrapper, 5.0);
    }

    TEST(FrameworkPlannerAssessment, PreservesReportsAndIndependentObservationRefusals)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        FrameworkPlannerService service;
        AdapterPostDeploymentVerifierEvidenceService canonical;
        auto compare = [&](const auto& evidence)
        {
            const auto direct =
                AdapterPostDeploymentVerificationService{}.BuildReport(fixture.m_workOrder, fixture.m_executionEnvelope, evidence);
            const auto wrapped = service.BuildReport(fixture.m_workOrder, fixture.m_executionEnvelope, evidence);
            EXPECT_EQ(canonical.SerializeCanonicalReport(wrapped), canonical.SerializeCanonicalReport(direct));
            EXPECT_FALSE(wrapped.m_verifierExecuted);
            EXPECT_FALSE(wrapped.m_evidencePromoted);
            EXPECT_FALSE(wrapped.m_releasePublished);
            EXPECT_FALSE(wrapped.m_launchPerformed);
            EXPECT_FALSE(wrapped.m_adapterCalled);
        };
        compare(fixture.m_executionEvidence);
        auto rejected = fixture.m_executionEvidence;
        rejected.m_accepted = false;
        compare(rejected);
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework::PlannerTests
