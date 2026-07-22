/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/std/algorithm.h>

#include "RoadAtlasExtension.h"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        GameProfile MakeRoadProfile()
        {
            GameProfile profile;
            profile.m_profileId = "profile.foa.road";
            profile.m_displayName = "FoA Road";
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

        RoadAtlasExtension::EvidenceRequirement MakeRequirement()
        {
            RoadAtlasExtension::EvidenceRequirement requirement;
            requirement.m_requirementId = "requirement.road.geometry";
            requirement.m_kind =
                RoadAtlasExtension::EvidenceRequirementKind::SegmentGeometry;
            requirement.m_required = true;
            requirement.m_satisfied = true;
            requirement.m_evidenceIds = { "evidence.road.geometry" };
            return requirement;
        }

        RoadAtlasExtension::Snapshot MakeRoadSnapshot()
        {
            RoadAtlasExtension::Element road;
            road.m_elementId = "road.element.main";
            road.m_ownerPackId = "pack.road.fixture";
            road.m_kind = RoadAtlasExtension::ElementKind::Road;
            road.m_promotionState = RoadAtlasExtension::PromotionState::Confirmed;
            road.m_displayName = "Main Road";
            road.m_regionRef = "region:horns-of-the-south";
            road.m_sceneRef = "CampaignMap_HOS";
            road.m_roadRef = "ROAD-HOS-MAIN";
            road.m_connectedSegmentRefs = { "segment:main" };

            RoadAtlasExtension::Element segment;
            segment.m_elementId = "road.segment.main";
            segment.m_ownerPackId = "pack.road.fixture";
            segment.m_kind = RoadAtlasExtension::ElementKind::Segment;
            segment.m_promotionState =
                RoadAtlasExtension::PromotionState::ApprovedForPlanning;
            segment.m_displayName = "Main Segment";
            segment.m_regionRef = "region:horns-of-the-south";
            segment.m_sceneRef = "CampaignMap_HOS";
            segment.m_roadRef = "ROAD-HOS-MAIN";
            segment.m_segmentRef = "segment:main";
            segment.m_fromElementRef = road.m_elementId;
            segment.m_toElementRef = road.m_elementId;
            segment.m_coordinateRef = "coordinate:main";
            segment.m_geometry = {
                { 1.0, 2.0, 3.0, "point:main:0" },
                { 4.0, 5.0, 6.0, "point:main:1" },
            };
            segment.m_evidenceRequirements = { MakeRequirement() };

            RoadAtlasExtension::Snapshot snapshot;
            snapshot.m_snapshotId = "snapshot.road.fixture";
            snapshot.m_profileId = "profile.foa.road";
            snapshot.m_gameVersion = "1.23.401";
            snapshot.m_branch = "mono";
            snapshot.m_runtimeTarget = "Mono";
            snapshot.m_sourceFingerprint = "sha256:" + AZStd::string(64, '4');
            snapshot.m_capturedAtUtc = "2026-07-21T20:00:00Z";
            snapshot.m_elements = { segment, road };
            return snapshot;
        }
    } // namespace

    TEST(RoadAtlasExtensionTests, KnowledgePinsSevenUpstreamContractSources)
    {
        const auto& pack = RoadAtlasExtension::GetCanonicalKnowledgePack();
        EXPECT_EQ(pack.m_version, "0.8.4");
        EXPECT_EQ(
            pack.m_upstreamCommit,
            "d7e740e7f167b73152b53409e483dab07d80d048");
        EXPECT_EQ(pack.m_sources.size(), 7);
        EXPECT_EQ(pack.m_gateOrder.front(), "inventory-and-names");
    }

    TEST(RoadAtlasExtensionTests, EvidenceBoundSnapshotIsPlanningReadyButInert)
    {
        const auto result = RoadAtlasExtension::ValidateSnapshot(
            MakeRoadSnapshot(), MakeRoadProfile());
        EXPECT_TRUE(result.m_accepted);
        EXPECT_TRUE(result.m_canUseForPlanning);
        EXPECT_FALSE(result.m_runtimeMutationAllowed);
        EXPECT_EQ(result.m_roadCount, 1);
        EXPECT_EQ(result.m_segmentCount, 1);
        EXPECT_FALSE(result.m_canonicalFingerprint.empty());
    }

    TEST(RoadAtlasExtensionTests, PlanningApprovalRequiresNonEmptyRequiredEvidenceSet)
    {
        auto snapshot = MakeRoadSnapshot();
        snapshot.m_elements[0].m_evidenceRequirements.clear();
        const auto result = RoadAtlasExtension::ValidateSnapshot(
            snapshot, MakeRoadProfile());
        EXPECT_FALSE(result.m_accepted);
        EXPECT_FALSE(result.m_canUseForPlanning);
        EXPECT_TRUE(AZStd::any_of(
            result.m_issues.begin(), result.m_issues.end(),
            [](const RoadAtlasExtension::ValidationIssue& issue)
            {
                return issue.m_code == "evidence.requirements-unsatisfied";
            }));
    }

    TEST(RoadAtlasExtensionTests, CanonicalFingerprintIgnoresElementOrdering)
    {
        auto first = MakeRoadSnapshot();
        auto second = first;
        AZStd::reverse(second.m_elements.begin(), second.m_elements.end());
        EXPECT_EQ(
            RoadAtlasExtension::CalculateSnapshotFingerprint(first),
            RoadAtlasExtension::CalculateSnapshotFingerprint(second));
    }

    TEST(RoadAtlasExtensionTests, CanonicalFingerprintChangesWithEvidence)
    {
        auto first = MakeRoadSnapshot();
        auto second = first;
        second.m_elements[0].m_evidenceRequirements[0].m_evidenceIds[0] =
            "evidence.road.geometry.changed";
        EXPECT_NE(
            RoadAtlasExtension::CalculateSnapshotFingerprint(first),
            RoadAtlasExtension::CalculateSnapshotFingerprint(second));
    }

    TEST(RoadAtlasExtensionTests, MissingGeometryAndRuntimeAuthorityFailClosed)
    {
        auto snapshot = MakeRoadSnapshot();
        snapshot.m_elements[0].m_geometry.clear();
        snapshot.m_runtimeMutationAllowed = true;
        const auto result = RoadAtlasExtension::ValidateSnapshot(
            snapshot, MakeRoadProfile());
        EXPECT_FALSE(result.m_accepted);
        EXPECT_FALSE(result.m_canUseForPlanning);
        EXPECT_FALSE(result.m_issues.empty());
    }

    TEST(RoadAtlasExtensionTests, MalformedEvidenceIdFailsClosed)
    {
        auto snapshot = MakeRoadSnapshot();
        snapshot.m_elements[0].m_evidenceRequirements[0].m_evidenceIds = {
            "invalid evidence id",
        };
        const auto result = RoadAtlasExtension::ValidateSnapshot(
            snapshot, MakeRoadProfile());
        EXPECT_FALSE(result.m_accepted);
        EXPECT_FALSE(result.m_canUseForPlanning);
    }

    TEST(RoadAtlasExtensionTests, OutOfRangeEnumValuesFailClosed)
    {
        auto promotion = MakeRoadSnapshot();
        promotion.m_elements[0].m_promotionState =
            static_cast<RoadAtlasExtension::PromotionState>(99);
        EXPECT_FALSE(RoadAtlasExtension::ValidateSnapshot(
            promotion, MakeRoadProfile()).m_accepted);

        auto requirement = MakeRoadSnapshot();
        requirement.m_elements[0].m_evidenceRequirements[0].m_kind =
            static_cast<RoadAtlasExtension::EvidenceRequirementKind>(99);
        EXPECT_FALSE(RoadAtlasExtension::ValidateSnapshot(
            requirement, MakeRoadProfile()).m_accepted);
    }

    TEST(RoadAtlasExtensionTests, NestedCollectionBoundsFailClosed)
    {
        auto tags = MakeRoadSnapshot();
        tags.m_elements[0].m_tags.resize(257, "tag.road");
        EXPECT_FALSE(RoadAtlasExtension::ValidateSnapshot(
            tags, MakeRoadProfile()).m_accepted);

        auto evidence = MakeRoadSnapshot();
        evidence.m_elements[0].m_evidenceRequirements[0].m_evidenceIds.resize(
            257,
            "evidence.road.geometry");
        EXPECT_FALSE(RoadAtlasExtension::ValidateSnapshot(
            evidence, MakeRoadProfile()).m_accepted);
    }

    TEST(RoadAtlasExtensionTests, ControlCharactersInNestedNotesFailClosed)
    {
        auto snapshot = MakeRoadSnapshot();
        snapshot.m_elements[0].m_evidenceRequirements[0].m_notes = "bad\nnotes";
        EXPECT_FALSE(RoadAtlasExtension::ValidateSnapshot(
            snapshot, MakeRoadProfile()).m_accepted);
    }

    TEST(RoadAtlasExtensionTests, GeometryLimitFailsClosedWithoutOverflow)
    {
        auto snapshot = MakeRoadSnapshot();
        snapshot.m_elements[0].m_geometry.resize(65537);
        for (RoadAtlasExtension::Coordinate& point : snapshot.m_elements[0].m_geometry)
        {
            point.m_pointRef = "point.geometry.fixture";
        }
        const auto result = RoadAtlasExtension::ValidateSnapshot(
            snapshot, MakeRoadProfile());
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(AZStd::any_of(
            result.m_issues.begin(), result.m_issues.end(),
            [](const RoadAtlasExtension::ValidationIssue& issue)
            {
                return issue.m_code == "snapshot.geometry-count";
            }));
    }

    TEST(RoadAtlasExtensionTests, SanitizedExtensionProfileBindingValidatesWithoutPrivatePaths)
    {
        const auto profile = MakeRoadProfile();
        const RoadAtlasExtension::ProfileBinding binding{
            profile.m_profileId,
            profile.m_gameVersion,
            profile.m_branch,
            profile.m_runtimeTarget,
        };
        const auto result = RoadAtlasExtension::ValidateSnapshot(
            MakeRoadSnapshot(), binding);
        EXPECT_TRUE(result.m_accepted);
        EXPECT_TRUE(result.m_canUseForPlanning);
    }
} // namespace TaintedGrailModdingSDK
