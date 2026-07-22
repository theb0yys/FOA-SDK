/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/std/algorithm.h>

#include "AvalonAiExtension.h"
#include "RoadAtlasExtension.h"

#include <limits>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AvalonAiExtension::PackageManifest MakeBoundedAiPackage()
        {
            using namespace AvalonAiExtension;
            PackageManifest manifest;
            manifest.m_packageId = "package.ai.bounds";
            manifest.m_displayName = "AI Bounds Fixture";
            manifest.m_packageVersion = "1.0.0";
            manifest.m_requiredRuntimeApi = { 2, 0 };
            manifest.m_blackboardNamespace = "blackboard.bounds";
            manifest.m_blackboardSchemaVersion = 1;
            manifest.m_maximumPolicyCadenceMilliseconds = 250;
            manifest.m_supportedActorRoles = { "role.bounds" };
            manifest.m_requiredCapabilities = { "action.bounds" };

            BlackboardKey key;
            key.m_keyId = "blackboard-key.bounds.target";
            key.m_namespace = manifest.m_blackboardNamespace;
            key.m_name = "target";
            key.m_schemaVersion = 1;
            key.m_access = BlackboardAccess::PackageLocal;
            key.m_scope = BlackboardScope::Actor;
            key.m_valueType = "entity-ref";
            manifest.m_blackboardKeys.push_back(key);

            GoalDefinition goal;
            goal.m_goalId = "goal.bounds";
            goal.m_priority = 1;
            goal.m_conditions = {
                { "fact.bounds", PlanningComparison::Equal, 1 },
            };
            manifest.m_goals.push_back(goal);

            ActionDefinition action;
            action.m_actionId = "action.bounds";
            action.m_targetKeyId = key.m_keyId;
            action.m_requiredCapability = "action.bounds";
            action.m_interruptPolicy = InterruptPolicy::Never;
            action.m_timeoutMilliseconds = 1000;
            action.m_conditions = {
                { "fact.bounds", PlanningComparison::Equal, 1 },
            };
            action.m_effects = { { "fact.bounds", 1 } };
            manifest.m_actions.push_back(action);
            return manifest;
        }

        GameProfile MakeRoadProfile()
        {
            GameProfile profile;
            profile.m_profileId = "profile.road.bounds";
            profile.m_displayName = "Road Bounds";
            profile.m_gameVersion = "1.23.401";
            profile.m_branch = "mono";
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "6000.0.64f1";
            profile.m_bepInExVersion = "5.4.23.3";
            profile.m_installPath = "C:/fixture/game";
            profile.m_managedAssembliesPath = "C:/fixture/game/Managed";
            profile.m_pluginPath = "C:/fixture/game/BepInEx/plugins";
            profile.m_diagnosticsPath = "C:/fixture/workspace/diagnostics";
            profile.m_extractedDataPath = "C:/fixture/workspace/extracted";
            return profile;
        }

        RoadAtlasExtension::Snapshot MakeBoundedRoadSnapshot()
        {
            using namespace RoadAtlasExtension;
            EvidenceRequirement requirement;
            requirement.m_requirementId = "requirement.road.bounds";
            requirement.m_kind = EvidenceRequirementKind::SegmentGeometry;
            requirement.m_required = true;
            requirement.m_satisfied = true;
            requirement.m_evidenceIds = { "evidence.road.bounds" };

            Element segment;
            segment.m_elementId = "road.segment.bounds";
            segment.m_ownerPackId = "pack.road.bounds";
            segment.m_kind = ElementKind::Segment;
            segment.m_promotionState = PromotionState::ApprovedForPlanning;
            segment.m_displayName = "Bounds Segment";
            segment.m_regionRef = "region:bounds";
            segment.m_sceneRef = "Scene_Bounds";
            segment.m_segmentRef = "segment:bounds";
            segment.m_geometry = {
                { 1.0, 2.0, 3.0, "point:bounds:0" },
                { 4.0, 5.0, 6.0, "point:bounds:1" },
            };
            segment.m_evidenceRequirements = { requirement };

            Snapshot snapshot;
            snapshot.m_snapshotId = "snapshot.road.bounds";
            snapshot.m_profileId = "profile.road.bounds";
            snapshot.m_gameVersion = "1.23.401";
            snapshot.m_branch = "mono";
            snapshot.m_runtimeTarget = "Mono";
            snapshot.m_sourceFingerprint = "sha256:" + AZStd::string(64, '7');
            snapshot.m_capturedAtUtc = "2026-07-22T05:00:00Z";
            snapshot.m_elements = { segment };
            return snapshot;
        }

        template<class Issue>
        bool HasIssueCode(const AZStd::vector<Issue>& issues, const char* code)
        {
            return AZStd::any_of(
                issues.begin(), issues.end(),
                [code](const Issue& issue)
                {
                    return issue.m_code == code;
                });
        }
    } // namespace

    TEST(NestedCollectionShortCircuitTests, AvalonOversizedTopLevelCollectionReturnsImmediately)
    {
        auto manifest = MakeBoundedAiPackage();
        manifest.m_supportedActorRoles.resize(129, "invalid role with spaces");
        const auto result = AvalonAiExtension::ValidateAndPlan(manifest);
        ASSERT_FALSE(result.m_accepted);
        ASSERT_EQ(result.m_issues.size(), 1);
        EXPECT_EQ(result.m_issues[0].m_code, "manifest.collection-bounds");
    }

    TEST(NestedCollectionShortCircuitTests, AvalonOversizedGoalConditionsAreNotTraversed)
    {
        auto manifest = MakeBoundedAiPackage();
        manifest.m_goals[0].m_conditions.resize(
            257,
            { "invalid fact with spaces", AvalonAiExtension::PlanningComparison::Equal, 1 });
        const auto result = AvalonAiExtension::ValidateAndPlan(manifest);
        ASSERT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssueCode(result.m_issues, "goal.condition-count"));
        EXPECT_FALSE(HasIssueCode(result.m_issues, "goal.condition-invalid"));
    }

    TEST(NestedCollectionShortCircuitTests, AvalonOversizedActionClausesAreNotTraversed)
    {
        auto manifest = MakeBoundedAiPackage();
        manifest.m_actions[0].m_conditions.resize(
            257,
            { "invalid fact with spaces", AvalonAiExtension::PlanningComparison::Equal, 1 });
        manifest.m_actions[0].m_effects.resize(
            257,
            { "invalid fact with spaces", 1 });
        const auto result = AvalonAiExtension::ValidateAndPlan(manifest);
        ASSERT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssueCode(result.m_issues, "action.planning-count"));
        EXPECT_FALSE(HasIssueCode(result.m_issues, "action.condition-invalid"));
        EXPECT_FALSE(HasIssueCode(result.m_issues, "action.effect-invalid"));
    }

    TEST(NestedCollectionShortCircuitTests, RoadOversizedGeometryIsNotTraversed)
    {
        auto snapshot = MakeBoundedRoadSnapshot();
        snapshot.m_elements[0].m_geometry.resize(65537);
        for (RoadAtlasExtension::Coordinate& point : snapshot.m_elements[0].m_geometry)
        {
            point.m_x = std::numeric_limits<double>::quiet_NaN();
            point.m_pointRef = "invalid\npoint";
        }
        const auto result = RoadAtlasExtension::ValidateSnapshot(
            snapshot,
            MakeRoadProfile());
        ASSERT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssueCode(result.m_issues, "snapshot.geometry-count"));
        EXPECT_FALSE(HasIssueCode(result.m_issues, "segment.geometry-invalid"));
    }

    TEST(NestedCollectionShortCircuitTests, RoadOversizedEvidenceRequirementsAreNotTraversed)
    {
        auto snapshot = MakeBoundedRoadSnapshot();
        RoadAtlasExtension::EvidenceRequirement invalid;
        invalid.m_requirementId = "invalid requirement with spaces";
        invalid.m_kind = static_cast<RoadAtlasExtension::EvidenceRequirementKind>(99);
        invalid.m_notes = "invalid\nnotes";
        snapshot.m_elements[0].m_evidenceRequirements.resize(257, invalid);
        const auto result = RoadAtlasExtension::ValidateSnapshot(
            snapshot,
            MakeRoadProfile());
        ASSERT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssueCode(result.m_issues, "evidence.requirement-count"));
        EXPECT_FALSE(HasIssueCode(result.m_issues, "evidence.invalid"));
    }
} // namespace TaintedGrailModdingSDK
