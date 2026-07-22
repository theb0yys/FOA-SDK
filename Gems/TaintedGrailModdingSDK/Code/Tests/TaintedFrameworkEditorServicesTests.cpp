/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <AzTest/AzTest.h>

#include "FoundationService.h"
#include "TaintedFrameworkEditorServices.h"
#include "TaintedFrameworkKnowledge.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        WorkspaceModel MakeWorkspace(
            AZStd::string gameVersion = "1.23.401",
            AZStd::string branch = "Mono",
            AZStd::string runtime = "Mono",
            AZStd::string bepInExVersion = "5.4.23.3")
        {
            GameProfile profile;
            profile.m_profileId = "profile.foa.tainted-framework.editor-services";
            profile.m_displayName = "Tainted Framework editor-service fixture";
            profile.m_gameVersion = AZStd::move(gameVersion);
            profile.m_branch = AZStd::move(branch);
            profile.m_runtimeTarget = AZStd::move(runtime);
            profile.m_bepInExVersion = AZStd::move(bepInExVersion);

            WorkspaceModel workspace;
            workspace.m_workspaceId = "workspace.tainted-framework.editor-services";
            workspace.m_displayName = "Tainted Framework editor-service fixture";
            workspace.m_activeGameProfileId = profile.m_profileId;
            workspace.m_gameProfiles.push_back(AZStd::move(profile));
            return workspace;
        }
    } // namespace

    using namespace TaintedFrameworkEditorServices;

    TEST(TaintedFrameworkEditorServicesTests, ExactMonoEvidenceIsReady)
    {
        Service service;
        const auto result = service.EvaluateCompatibility(
            "1.23.401", "Mono", "Mono", "5.4.23.3");
        EXPECT_EQ(result.m_status, ReadinessStatus::Ready);
        EXPECT_TRUE(result.m_blockers.empty());
        EXPECT_EQ(result.m_bepInExVersion, "5.4.23.3");
        EXPECT_EQ(result.m_evidencePath, "mods/tainted-framework/release/manifest.md");
    }

    TEST(TaintedFrameworkEditorServicesTests, Il2CppRemainsBlocked)
    {
        Service service;
        const auto result = service.EvaluateCompatibility(
            "unknown", "IL2CPP", "IL2CPP", "6");
        EXPECT_EQ(result.m_status, ReadinessStatus::Blocked);
        ASSERT_EQ(result.m_blockers.size(), 1);
        EXPECT_EQ(result.m_blockers[0], "upstream_branch_blocked");
    }

    TEST(TaintedFrameworkEditorServicesTests, VersionDriftIsUnsupported)
    {
        Service service;
        const auto result = service.EvaluateCompatibility(
            "1.23.402", "Mono", "Mono", "5.4.23.3");
        EXPECT_EQ(result.m_status, ReadinessStatus::Unsupported);
        ASSERT_EQ(result.m_blockers.size(), 1);
        EXPECT_EQ(result.m_blockers[0], "exact_game_version_not_evidence_backed");
    }

    TEST(TaintedFrameworkEditorServicesTests, BepInExVersionDriftIsUnsupported)
    {
        Service service;
        const auto result = service.EvaluateCompatibility(
            "1.23.401", "Mono", "Mono", "5.4.24.0");
        EXPECT_EQ(result.m_status, ReadinessStatus::Unsupported);
        ASSERT_EQ(result.m_blockers.size(), 1);
        EXPECT_EQ(
            result.m_blockers[0],
            "exact_bepinex_version_not_evidence_backed");
    }

    TEST(TaintedFrameworkEditorServicesTests, OnlyRuntimeReportIsConsumerReady)
    {
        Service service;
        const auto surfaces = service.GetApiSurfaceDecisions();
        size_t readyCount = 0;
        for (const auto& surface : surfaces)
        {
            if (surface.m_consumerReady)
            {
                ++readyCount;
                EXPECT_EQ(surface.m_surfaceId, "runtime-report");
                EXPECT_EQ(surface.m_consumerBinding, "mods/Tainted-Diagnostic Tool");
            }
            else
            {
                EXPECT_FALSE(surface.m_blockers.empty());
            }
        }
        EXPECT_EQ(readyCount, 1);
    }

    TEST(TaintedFrameworkEditorServicesTests, BlockedSurfaceHasStableReason)
    {
        Service service;
        const auto surfaces = service.GetApiSurfaceDecisions();
        const auto found = AZStd::find_if(
            surfaces.begin(),
            surfaces.end(),
            [](const ApiSurfaceDecision& surface)
            {
                return surface.m_surfaceId == "report-export";
            });
        ASSERT_NE(found, surfaces.end());
        EXPECT_FALSE(found->m_consumerReady);
        ASSERT_EQ(found->m_blockers.size(), 1);
        EXPECT_EQ(found->m_blockers[0], "surface_status_blocked");
    }

    TEST(TaintedFrameworkEditorServicesTests, ActivationPlanIsReadOnlyAndDeterministic)
    {
        Service service;
        const auto first = service.BuildActivationPlan(
            "1.23.401", "Mono", "Mono", "5.4.23.3");
        const auto second = service.BuildActivationPlan(
            "1.23.401", "Mono", "Mono", "5.4.23.3");
        EXPECT_EQ(first.m_planId, second.m_planId);
        EXPECT_EQ(
            first.m_planId,
            "tainted-framework-editor-plan:0.1.33:Mono:Mono:1.23.401:5.4.23.3");
        EXPECT_EQ(first.m_extensionId, "extension.tainted-framework");
        EXPECT_FALSE(first.m_runtimeInvocationAllowed);
        EXPECT_FALSE(first.m_fileWriteAllowed);
        EXPECT_FALSE(first.m_catalogMutationAllowed);
        EXPECT_TRUE(first.m_candidateEvidenceSubmissionEligible);
        EXPECT_EQ(first.m_configuration.size(), 3);
        EXPECT_FALSE(first.m_diagnostics.empty());
    }

    TEST(TaintedFrameworkEditorServicesTests, BlockedPlanIsNotEvidenceEligible)
    {
        Service service;
        const auto plan = service.BuildActivationPlan(
            "unknown", "IL2CPP", "IL2CPP", "6");
        EXPECT_EQ(plan.m_compatibility.m_status, ReadinessStatus::Blocked);
        EXPECT_FALSE(plan.m_candidateEvidenceSubmissionEligible);
        EXPECT_FALSE(plan.m_runtimeInvocationAllowed);
        EXPECT_FALSE(plan.m_fileWriteAllowed);
        EXPECT_FALSE(plan.m_catalogMutationAllowed);
    }

    TEST(TaintedFrameworkEditorServicesTests, SanitizedProfileBindingIsExact)
    {
        FoundationService foundation(FoundationWorkspaceLoadDependencies{});
        foundation.SetWorkspace(MakeWorkspace());
        AZStd::string error;
        ASSERT_TRUE(TaintedFrameworkKnowledge::RegisterExtensionConsumer(
            foundation.GetExtensionAPI(),
            &error)) << error.c_str();

        ExtensionAPI::ProfileView profile;
        ASSERT_TRUE(foundation.GetExtensionAPI().GetActiveProfile(
            "extension.tainted-framework",
            profile,
            &error)) << error.c_str();

        const auto plan = foundation.GetTaintedFrameworkEditorServices()
                              .BuildActivationPlan(profile);
        EXPECT_EQ(plan.m_compatibility.m_status, ReadinessStatus::Ready);
        EXPECT_EQ(plan.m_compatibility.m_bepInExVersion, "5.4.23.3");
        EXPECT_TRUE(plan.m_candidateEvidenceSubmissionEligible);
        EXPECT_FALSE(plan.m_runtimeInvocationAllowed);
    }

    TEST(TaintedFrameworkEditorServicesTests, SanitizedProfileBepInExDriftFailsClosed)
    {
        FoundationService foundation(FoundationWorkspaceLoadDependencies{});
        foundation.SetWorkspace(MakeWorkspace(
            "1.23.401", "Mono", "Mono", "5.4.24.0"));
        AZStd::string error;
        ASSERT_TRUE(TaintedFrameworkKnowledge::RegisterExtensionConsumer(
            foundation.GetExtensionAPI(),
            &error)) << error.c_str();

        ExtensionAPI::ProfileView profile;
        ASSERT_TRUE(foundation.GetExtensionAPI().GetActiveProfile(
            "extension.tainted-framework",
            profile,
            &error)) << error.c_str();

        const auto plan = foundation.GetTaintedFrameworkEditorServices()
                              .BuildActivationPlan(profile);
        EXPECT_EQ(plan.m_compatibility.m_status, ReadinessStatus::Unsupported);
        EXPECT_FALSE(plan.m_candidateEvidenceSubmissionEligible);
        ASSERT_EQ(plan.m_compatibility.m_blockers.size(), 1);
        EXPECT_EQ(
            plan.m_compatibility.m_blockers[0],
            "exact_bepinex_version_not_evidence_backed");
    }

    TEST(TaintedFrameworkEditorServicesTests, FoundationOwnsPersistentService)
    {
        FoundationService foundation(FoundationWorkspaceLoadDependencies{});
        auto& first = foundation.GetTaintedFrameworkEditorServices();
        auto& second = foundation.GetTaintedFrameworkEditorServices();
        EXPECT_EQ(&first, &second);
    }
} // namespace TaintedGrailModdingSDK
