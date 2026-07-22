/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include "AvalonAiExtension.h"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AvalonAiExtension::PackageManifest MakeAiPackage()
        {
            using namespace AvalonAiExtension;
            PackageManifest manifest;
            manifest.m_packageId = "package.ai.companion";
            manifest.m_displayName = "Companion Authoring Fixture";
            manifest.m_packageVersion = "1.0.0";
            manifest.m_requiredRuntimeApi = { 2, 0 };
            manifest.m_blackboardNamespace = "blackboard.companion";
            manifest.m_blackboardSchemaVersion = 1;
            manifest.m_maximumPolicyCadenceMilliseconds = 250;
            manifest.m_supportedActorRoles = { "role.companion" };
            manifest.m_requiredCapabilities = { "action.follow" };

            BlackboardKey key;
            key.m_namespace = manifest.m_blackboardNamespace;
            key.m_name = "distance";
            key.m_schemaVersion = 1;
            key.m_access = BlackboardAccess::PackageLocal;
            key.m_scope = BlackboardScope::Actor;
            key.m_valueType = "float";
            manifest.m_blackboardKeys.push_back(key);

            GoalDefinition goal;
            goal.m_goalId = "goal.follow-player";
            goal.m_priority = 10;
            goal.m_conditions = {
                { "fact.player-visible", PlanningComparison::Equal, 1 },
            };
            manifest.m_goals.push_back(goal);

            ActionDefinition action;
            action.m_actionId = "action.follow-player";
            action.m_conditions = {
                { "fact.player-visible", PlanningComparison::Equal, 1 },
            };
            action.m_effects = { { "fact.near-player", 1 } };
            action.m_targetKeyId = "target.player";
            action.m_requiredCapability = "action.follow";
            action.m_interruptPolicy = InterruptPolicy::OnInvalidTarget;
            action.m_timeoutMilliseconds = 5000;
            manifest.m_actions.push_back(action);
            return manifest;
        }
    } // namespace

    TEST(AvalonAiExtensionTests, KnowledgePinsApiTwoAuthoringSurface)
    {
        const auto& pack = AvalonAiExtension::GetCanonicalKnowledgePack();
        EXPECT_EQ(pack.m_version, "0.8.0");
        EXPECT_EQ(pack.m_apiMajor, 2);
        EXPECT_EQ(pack.m_apiMinor, 0);
        EXPECT_EQ(pack.m_sources.size(), 6);
        EXPECT_FALSE(pack.m_excludedRuntimeSurfaces.empty());
    }

    TEST(AvalonAiExtensionTests, ValidPackageBuildsDeterministicInertPlan)
    {
        const auto result = AvalonAiExtension::ValidateAndPlan(MakeAiPackage());
        ASSERT_TRUE(result.m_accepted);
        EXPECT_EQ(result.m_plan.m_packageId, "package.ai.companion");
        EXPECT_EQ(result.m_plan.m_goalIds.size(), 1);
        EXPECT_EQ(result.m_plan.m_actionIds.size(), 1);
        EXPECT_FALSE(result.m_plan.m_canonicalFingerprint.empty());
        EXPECT_FALSE(result.m_plan.m_executionAllowed);
        EXPECT_FALSE(result.m_plan.m_runtimeMutationAllowed);
    }

    TEST(AvalonAiExtensionTests, CanonicalFingerprintChangesWithPlanningEffect)
    {
        auto first = MakeAiPackage();
        auto second = first;
        second.m_actions[0].m_effects[0].m_assignedValue = 2;
        EXPECT_NE(
            AvalonAiExtension::CalculatePackageFingerprint(first),
            AvalonAiExtension::CalculatePackageFingerprint(second));
    }

    TEST(AvalonAiExtensionTests, NonPackageLocalKeyAndUnknownCapabilityFailClosed)
    {
        auto manifest = MakeAiPackage();
        manifest.m_blackboardKeys[0].m_access =
            AvalonAiExtension::BlackboardAccess::Authoritative;
        manifest.m_actions[0].m_requiredCapability = "action.teleport";
        const auto result = AvalonAiExtension::ValidateAndPlan(manifest);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_FALSE(result.m_issues.empty());
    }

    TEST(AvalonAiExtensionTests, RuntimeHostAndExecutionFlagsAreRejected)
    {
        auto manifest = MakeAiPackage();
        manifest.m_hostLinked = true;
        manifest.m_executionEnabled = true;
        const auto result = AvalonAiExtension::ValidateAndPlan(manifest);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_FALSE(result.m_plan.m_executionAllowed);
    }
} // namespace TaintedGrailModdingSDK
