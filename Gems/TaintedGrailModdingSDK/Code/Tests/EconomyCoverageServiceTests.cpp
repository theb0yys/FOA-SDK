/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"
#include "EconomyCoverageService.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/std/utility/move.h>
#include <AzTest/AzTest.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        CatalogRecord MakeRecord(
            AZStd::string recordId,
            AZStd::string recordKind,
            AZStd::string subjectRef)
        {
            CatalogRecord record;
            record.m_recordId = AZStd::move(recordId);
            record.m_domain = "economy";
            record.m_recordKind = AZStd::move(recordKind);
            record.m_subjectRef = AZStd::move(subjectRef);
            record.m_nativeRefExact = "native:" + record.m_recordId;
            record.m_identityKind = "native";
            record.m_researchStage = "S5";
            record.m_confidence = "documented";
            record.m_operationalRisk = "low";
            record.m_validationState = "validated";
            record.m_stalenessState = "current";
            record.m_evidenceIds = { "evidence.record" };
            return record;
        }

        CatalogRelationship MakeRelationship(
            AZStd::string relationshipId,
            AZStd::string sourceRecordId,
            AZStd::string targetRecordId,
            AZStd::string relationshipKind,
            AZStd::string evidenceId,
            bool validated = true)
        {
            CatalogRelationship relationship;
            relationship.m_relationshipId = AZStd::move(relationshipId);
            relationship.m_fromRecordId = AZStd::move(sourceRecordId);
            relationship.m_toRecordId = AZStd::move(targetRecordId);
            relationship.m_relationshipKind = AZStd::move(relationshipKind);
            relationship.m_evidenceIds = { AZStd::move(evidenceId) };
            relationship.m_researchStage = "S5";
            relationship.m_confidence = "documented";
            relationship.m_operationalRisk = "low";
            relationship.m_validationState = validated ? "validated" : "unvalidated";
            relationship.m_stalenessState = validated ? "current" : "unknown";
            return relationship;
        }

        SourceRecord MakeSource()
        {
            SourceRecord source;
            source.m_sourceId = "source.coverage";
            source.m_fingerprint = "fingerprint.coverage";
            source.m_profileId = "profile.coverage";
            source.m_gameVersion = "version.coverage";
            source.m_branch = "branch.coverage";
            source.m_runtimeTarget = "mono";
            source.m_importerId = "importer.coverage";
            source.m_importerVersion = "1.0.0";
            return source;
        }

        EvidenceRecord MakeEvidence(AZStd::string evidenceId, AZStd::string subjectRef)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = AZStd::move(evidenceId);
            evidence.m_sourceId = "source.coverage";
            evidence.m_sourceFingerprint = "fingerprint.coverage";
            evidence.m_profileId = "profile.coverage";
            evidence.m_gameVersion = "version.coverage";
            evidence.m_branch = "branch.coverage";
            evidence.m_subjectRef = AZStd::move(subjectRef);
            evidence.m_claim = "coverage claim";
            return evidence;
        }

        const EconomyAcquisitionCoverageRow* FindRow(
            const EconomyAcquisitionCoverageSummary& summary,
            const AZStd::string& recordId)
        {
            for (const EconomyAcquisitionCoverageRow& row : summary.m_rows)
            {
                if (row.m_recordId == recordId)
                {
                    return &row;
                }
            }
            return nullptr;
        }

        const EconomyAcquisitionCoverageLane* FindLane(
            const EconomyAcquisitionCoverageRow& row,
            const AZStd::string& laneName)
        {
            for (const EconomyAcquisitionCoverageLane& lane : row.m_lanes)
            {
                if (lane.m_lane == laneName)
                {
                    return &lane;
                }
            }
            return nullptr;
        }
    } // namespace

    TEST(TaintedGrailEconomyCoverageServiceTests, ItemCoverageSeparatesVendorLootAndRewardDeterministically)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("item.z", "item", "subject:item:z"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("item.a", "item", "subject:item:a"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("vendor.a", "vendor", "subject:vendor:a"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("loot.a", "loot_source", "subject:loot:a"), &error));
        ASSERT_TRUE(catalog.UpsertRelationship(
            MakeRelationship("relationship.vendor", "item.a", "vendor.a", "sold_by", "evidence.vendor"),
            &error));
        ASSERT_TRUE(catalog.UpsertRelationship(
            MakeRelationship("relationship.loot", "item.a", "loot.a", "dropped_by", "evidence.loot", false),
            &error));

        SourceEvidenceRegistry registry;
        ASSERT_TRUE(registry.RegisterSource(MakeSource(), &error));
        ASSERT_TRUE(registry.RegisterEvidence(MakeEvidence("evidence.vendor", "subject:vendor:a"), &error));
        ASSERT_TRUE(registry.RegisterEvidence(MakeEvidence("evidence.loot", "subject:loot:a"), &error));

        EconomyCoverageService service;
        const EconomyAcquisitionCoverageSummary summary = service.BuildAcquisitionCoverage(registry, catalog, {});

        ASSERT_EQ(summary.m_rows.size(), 2);
        EXPECT_EQ(summary.m_rows[0].m_recordId, "item.a");
        EXPECT_EQ(summary.m_rows[1].m_recordId, "item.z");
        const EconomyAcquisitionCoverageRow* item = FindRow(summary, "item.a");
        ASSERT_NE(item, nullptr);
        const EconomyAcquisitionCoverageLane* vendor = FindLane(*item, "vendor");
        const EconomyAcquisitionCoverageLane* loot = FindLane(*item, "loot");
        const EconomyAcquisitionCoverageLane* reward = FindLane(*item, "reward");
        ASSERT_NE(vendor, nullptr);
        ASSERT_NE(loot, nullptr);
        ASSERT_NE(reward, nullptr);
        EXPECT_EQ(vendor->m_status, "covered");
        EXPECT_EQ(loot->m_status, "partial");
        EXPECT_EQ(reward->m_status, "missing");
        EXPECT_EQ(item->m_overallStatus, "partial");
        EXPECT_EQ(summary.m_partialSubjectCount, 1);
        EXPECT_EQ(summary.m_missingSubjectCount, 1);
    }

    TEST(TaintedGrailEconomyCoverageServiceTests, RecipeCoverageUsesLearnabilityCraftingAndRewardLanes)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("recipe.a", "recipe", "subject:recipe:a"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("knowledge.a", "knowledge_source", "subject:knowledge:a"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("station.a", "crafting_station", "subject:station:a"), &error));
        ASSERT_TRUE(catalog.UpsertRelationship(
            MakeRelationship("relationship.learn", "recipe.a", "knowledge.a", "learned_from", "evidence.learn"),
            &error));
        ASSERT_TRUE(catalog.UpsertRelationship(
            MakeRelationship("relationship.craft", "recipe.a", "station.a", "crafted_at", "evidence.craft"),
            &error));

        SourceEvidenceRegistry registry;
        ASSERT_TRUE(registry.RegisterSource(MakeSource(), &error));
        ASSERT_TRUE(registry.RegisterEvidence(MakeEvidence("evidence.learn", "subject:knowledge:a"), &error));
        ASSERT_TRUE(registry.RegisterEvidence(MakeEvidence("evidence.craft", "subject:station:a"), &error));

        EconomyCoverageService service;
        const EconomyAcquisitionCoverageSummary summary = service.BuildAcquisitionCoverage(registry, catalog, {});
        ASSERT_EQ(summary.m_rows.size(), 1);
        const EconomyAcquisitionCoverageRow& recipe = summary.m_rows[0];
        EXPECT_EQ(recipe.m_lanes.size(), 3);
        const EconomyAcquisitionCoverageLane* learnability = FindLane(recipe, "learnability");
        const EconomyAcquisitionCoverageLane* crafting = FindLane(recipe, "crafting");
        const EconomyAcquisitionCoverageLane* reward = FindLane(recipe, "reward");
        ASSERT_NE(learnability, nullptr);
        ASSERT_NE(crafting, nullptr);
        ASSERT_NE(reward, nullptr);
        EXPECT_EQ(learnability->m_status, "covered");
        EXPECT_EQ(crafting->m_status, "covered");
        EXPECT_EQ(reward->m_status, "missing");
        EXPECT_EQ(FindLane(recipe, "vendor"), nullptr);
        EXPECT_EQ(recipe.m_overallStatus, "partial");
    }

    TEST(TaintedGrailEconomyCoverageServiceTests, CoveredRelationshipCannotHideBlockedRelationshipInSameLane)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("item.a", "item", "subject:item:a"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("vendor.good", "vendor", "subject:vendor:good"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("vendor.bad", "vendor", "subject:vendor:bad"), &error));
        ASSERT_TRUE(catalog.UpsertRelationship(
            MakeRelationship("relationship.good", "item.a", "vendor.good", "sold_by", "evidence.good"),
            &error));
        ASSERT_TRUE(catalog.UpsertRelationship(
            MakeRelationship("relationship.bad", "item.a", "vendor.bad", "sold_by", "evidence.missing"),
            &error));

        SourceEvidenceRegistry registry;
        ASSERT_TRUE(registry.RegisterSource(MakeSource(), &error));
        ASSERT_TRUE(registry.RegisterEvidence(MakeEvidence("evidence.good", "subject:vendor:good"), &error));

        EconomyCoverageService service;
        const EconomyAcquisitionCoverageSummary summary = service.BuildAcquisitionCoverage(registry, catalog, {});

        ASSERT_EQ(summary.m_rows.size(), 1);
        const EconomyAcquisitionCoverageLane* vendor = FindLane(summary.m_rows[0], "vendor");
        ASSERT_NE(vendor, nullptr);
        EXPECT_EQ(vendor->m_relationshipCount, 2);
        EXPECT_EQ(vendor->m_coveredRelationshipCount, 1);
        EXPECT_EQ(vendor->m_status, "blocked");
        EXPECT_EQ(summary.m_rows[0].m_overallStatus, "blocked");
    }

    TEST(TaintedGrailEconomyCoverageServiceTests, CoverageFailsClosedForUnrelatedEvidenceAndOpenBlockersWithoutMutation)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("item.a", "item", "subject:item:a"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("vendor.a", "vendor", "subject:vendor:a"), &error));
        ASSERT_TRUE(catalog.UpsertRelationship(
            MakeRelationship("relationship.vendor", "item.a", "vendor.a", "sold_by", "evidence.unrelated"),
            &error));

        SourceEvidenceRegistry registry;
        ASSERT_TRUE(registry.RegisterSource(MakeSource(), &error));
        ASSERT_TRUE(registry.RegisterEvidence(
            MakeEvidence("evidence.unrelated", "subject:someone-else"),
            &error));

        BlockerRecord blocker;
        blocker.m_blockerId = "blocker.vendor";
        blocker.m_status = "open";
        blocker.m_subjectRef = "subject:vendor:a";
        blocker.m_reason = "Vendor evidence is incomplete.";

        const size_t recordCountBefore = catalog.GetRecords().size();
        const size_t relationshipCountBefore = catalog.GetRelationships().size();
        const size_t sourceCountBefore = registry.GetSources().size();
        const size_t evidenceCountBefore = registry.GetEvidence().size();

        EconomyCoverageService service;
        const EconomyAcquisitionCoverageSummary summary = service.BuildAcquisitionCoverage(
            registry,
            catalog,
            { blocker });

        ASSERT_EQ(summary.m_rows.size(), 1);
        const EconomyAcquisitionCoverageLane* vendor = FindLane(summary.m_rows[0], "vendor");
        ASSERT_NE(vendor, nullptr);
        EXPECT_EQ(vendor->m_status, "blocked");
        EXPECT_EQ(vendor->m_blockerIds.size(), 1);
        EXPECT_FALSE(vendor->m_reasons.empty());
        EXPECT_EQ(summary.m_rows[0].m_overallStatus, "blocked");
        EXPECT_EQ(catalog.GetRecords().size(), recordCountBefore);
        EXPECT_EQ(catalog.GetRelationships().size(), relationshipCountBefore);
        EXPECT_EQ(registry.GetSources().size(), sourceCountBefore);
        EXPECT_EQ(registry.GetEvidence().size(), evidenceCountBefore);
    }
} // namespace TaintedGrailModdingSDK
