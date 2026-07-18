/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"
#include "EconomyDuplicateDetectionService.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/std/utility/move.h>
#include <AzTest/AzTest.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        CatalogRecord MakePackRecord(
            AZStd::string recordId,
            AZStd::string ownerPackId,
            AZStd::string recordKind,
            AZStd::string subjectRef,
            AZStd::string evidenceId)
        {
            CatalogRecord record;
            record.m_recordId = AZStd::move(recordId);
            record.m_ownerPackId = AZStd::move(ownerPackId);
            record.m_domain = "economy";
            record.m_recordKind = AZStd::move(recordKind);
            record.m_subjectRef = AZStd::move(subjectRef);
            record.m_identityKind = "synthetic";
            record.m_researchStage = "S5";
            record.m_confidence = "documented";
            record.m_operationalRisk = "low";
            record.m_validationState = "validated";
            record.m_stalenessState = "current";
            record.m_evidenceIds = { AZStd::move(evidenceId) };
            return record;
        }

        EconomyItemProfile MakeItemProfile(AZStd::string recordId, AZStd::string evidenceId)
        {
            EconomyItemProfile profile;
            profile.m_recordId = AZStd::move(recordId);
            profile.m_category = "material";
            profile.m_evidenceIds = { AZStd::move(evidenceId) };
            return profile;
        }

        EconomyRecipeProfile MakeRecipeProfile(
            AZStd::string recordId,
            AZStd::string duplicateKey,
            AZStd::string evidenceId)
        {
            EconomyRecipeProfile profile;
            profile.m_recordId = AZStd::move(recordId);
            profile.m_recipeType = "handcrafting";
            profile.m_duplicateKey = AZStd::move(duplicateKey);
            profile.m_persistenceMode = "custom_template";
            profile.m_evidenceIds = { AZStd::move(evidenceId) };
            return profile;
        }

        SourceRecord MakeSource()
        {
            SourceRecord source;
            source.m_sourceId = "source.duplicates";
            source.m_fingerprint = "fingerprint.duplicates";
            source.m_profileId = "profile.duplicates";
            source.m_gameVersion = "version.duplicates";
            source.m_branch = "branch.duplicates";
            source.m_runtimeTarget = "mono";
            source.m_importerId = "importer.duplicates";
            source.m_importerVersion = "1.0.0";
            return source;
        }

        EvidenceRecord MakeEvidence(AZStd::string evidenceId, AZStd::string subjectRef)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = AZStd::move(evidenceId);
            evidence.m_sourceId = "source.duplicates";
            evidence.m_sourceFingerprint = "fingerprint.duplicates";
            evidence.m_profileId = "profile.duplicates";
            evidence.m_gameVersion = "version.duplicates";
            evidence.m_branch = "branch.duplicates";
            evidence.m_subjectRef = AZStd::move(subjectRef);
            evidence.m_claim = "duplicate analysis claim";
            return evidence;
        }

        const EconomyDuplicateGroup* FindGroup(
            const EconomyDuplicateReport& report,
            const AZStd::string& signal,
            const AZStd::string& matchKey)
        {
            for (const EconomyDuplicateGroup& group : report.m_groups)
            {
                if (group.m_signal == signal && group.m_matchKey == matchKey)
                {
                    return &group;
                }
            }
            return nullptr;
        }

        void RegisterEvidence(
            SourceEvidenceRegistry& registry,
            const AZStd::vector<EvidenceRecord>& evidence,
            AZStd::string& error)
        {
            ASSERT_TRUE(registry.RegisterSource(MakeSource(), &error));
            for (const EvidenceRecord& record : evidence)
            {
                ASSERT_TRUE(registry.RegisterEvidence(record, &error));
            }
        }
    } // namespace

    TEST(TaintedGrailEconomyDuplicateDetectionTests, ExactSubjectRefFindsCrossPackItemsWithoutDisplayNameMatching)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        CatalogRecord itemB = MakePackRecord(
            "item.pack-b",
            "pack.b",
            "item",
            "subject:item:shared",
            "evidence.item-b");
        itemB.m_displayName = "Shared display label";
        CatalogRecord itemA = MakePackRecord(
            "item.pack-a",
            "pack.a",
            "item",
            "subject:item:shared",
            "evidence.item-a");
        itemA.m_displayName = "Different display label";
        CatalogRecord unrelated = MakePackRecord(
            "item.pack-c",
            "pack.c",
            "item",
            "subject:item:unrelated",
            "evidence.item-c");
        unrelated.m_displayName = "Shared display label";

        ASSERT_TRUE(catalog.InsertNew(itemB, &error));
        ASSERT_TRUE(catalog.InsertNew(itemA, &error));
        ASSERT_TRUE(catalog.InsertNew(unrelated, &error));
        ASSERT_TRUE(catalog.UpsertEconomyItem(MakeItemProfile("item.pack-a", "evidence.item-a"), &error));
        ASSERT_TRUE(catalog.UpsertEconomyItem(MakeItemProfile("item.pack-b", "evidence.item-b"), &error));
        ASSERT_TRUE(catalog.UpsertEconomyItem(MakeItemProfile("item.pack-c", "evidence.item-c"), &error));

        SourceEvidenceRegistry registry;
        RegisterEvidence(
            registry,
            {
                MakeEvidence("evidence.item-a", "subject:item:shared"),
                MakeEvidence("evidence.item-b", "subject:item:shared"),
                MakeEvidence("evidence.item-c", "subject:item:unrelated"),
            },
            error);

        EconomyDuplicateDetectionService service;
        const EconomyDuplicateReport report = service.BuildCrossPackDuplicateReport(registry, catalog, {});

        ASSERT_EQ(report.m_duplicateGroupCount, 1);
        ASSERT_EQ(report.m_reviewRequiredGroupCount, 1);
        const EconomyDuplicateGroup* group = FindGroup(report, "subject_ref", "subject:item:shared");
        ASSERT_NE(group, nullptr);
        EXPECT_EQ(group->m_status, "review_required");
        ASSERT_EQ(group->m_recordIds.size(), 2);
        EXPECT_EQ(group->m_recordIds[0], "item.pack-a");
        EXPECT_EQ(group->m_recordIds[1], "item.pack-b");
        EXPECT_EQ(FindGroup(report, "subject_ref", "Shared display label"), nullptr);
    }

    TEST(TaintedGrailEconomyDuplicateDetectionTests, ExactRecipeDuplicateKeyFindsDifferentSubjectsAcrossPacks)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakePackRecord(
            "recipe.pack-b",
            "pack.b",
            "recipe",
            "subject:recipe:b",
            "evidence.recipe-b"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakePackRecord(
            "recipe.pack-a",
            "pack.a",
            "recipe",
            "subject:recipe:a",
            "evidence.recipe-a"), &error));
        ASSERT_TRUE(catalog.UpsertEconomyRecipe(
            MakeRecipeProfile("recipe.pack-a", "recipe.key.shared", "evidence.recipe-a"),
            &error));
        ASSERT_TRUE(catalog.UpsertEconomyRecipe(
            MakeRecipeProfile("recipe.pack-b", "recipe.key.shared", "evidence.recipe-b"),
            &error));

        SourceEvidenceRegistry registry;
        RegisterEvidence(
            registry,
            {
                MakeEvidence("evidence.recipe-a", "subject:recipe:a"),
                MakeEvidence("evidence.recipe-b", "subject:recipe:b"),
            },
            error);

        EconomyDuplicateDetectionService service;
        const EconomyDuplicateReport report = service.BuildCrossPackDuplicateReport(registry, catalog, {});

        ASSERT_EQ(report.m_duplicateGroupCount, 1);
        const EconomyDuplicateGroup* group = FindGroup(
            report,
            "recipe_duplicate_key",
            "recipe.key.shared");
        ASSERT_NE(group, nullptr);
        EXPECT_EQ(group->m_recordKind, "recipe");
        EXPECT_EQ(group->m_status, "review_required");
        ASSERT_EQ(group->m_packIds.size(), 2);
        EXPECT_EQ(group->m_packIds[0], "pack.a");
        EXPECT_EQ(group->m_packIds[1], "pack.b");
    }

    TEST(TaintedGrailEconomyDuplicateDetectionTests, SamePackAndCaseDifferentKeysDoNotCreateCrossPackGroups)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakePackRecord(
            "recipe.same-pack-a",
            "pack.a",
            "recipe",
            "subject:recipe:a",
            "evidence.a"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakePackRecord(
            "recipe.same-pack-b",
            "pack.a",
            "recipe",
            "subject:recipe:b",
            "evidence.b"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakePackRecord(
            "recipe.case-different",
            "pack.b",
            "recipe",
            "subject:recipe:c",
            "evidence.c"), &error));
        ASSERT_TRUE(catalog.UpsertEconomyRecipe(
            MakeRecipeProfile("recipe.same-pack-a", "Recipe.Key", "evidence.a"),
            &error));
        ASSERT_TRUE(catalog.UpsertEconomyRecipe(
            MakeRecipeProfile("recipe.same-pack-b", "Recipe.Key", "evidence.b"),
            &error));
        ASSERT_TRUE(catalog.UpsertEconomyRecipe(
            MakeRecipeProfile("recipe.case-different", "recipe.key", "evidence.c"),
            &error));

        SourceEvidenceRegistry registry;
        RegisterEvidence(
            registry,
            {
                MakeEvidence("evidence.a", "subject:recipe:a"),
                MakeEvidence("evidence.b", "subject:recipe:b"),
                MakeEvidence("evidence.c", "subject:recipe:c"),
            },
            error);

        EconomyDuplicateDetectionService service;
        const EconomyDuplicateReport report = service.BuildCrossPackDuplicateReport(registry, catalog, {});
        EXPECT_EQ(report.m_scannedPackOwnedRecordCount, 3);
        EXPECT_EQ(report.m_duplicateGroupCount, 0);
    }

    TEST(TaintedGrailEconomyDuplicateDetectionTests, CandidateHealthEscalatesGroupFromPartialToBlocked)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        CatalogRecord partial = MakePackRecord(
            "item.partial",
            "pack.a",
            "item",
            "subject:item:shared",
            "evidence.partial");
        partial.m_validationState = "unvalidated";
        partial.m_stalenessState = "unknown";
        ASSERT_TRUE(catalog.InsertNew(partial, &error));
        ASSERT_TRUE(catalog.InsertNew(MakePackRecord(
            "item.ready",
            "pack.b",
            "item",
            "subject:item:shared",
            "evidence.ready"), &error));
        ASSERT_TRUE(catalog.UpsertEconomyItem(MakeItemProfile("item.partial", "evidence.partial"), &error));
        ASSERT_TRUE(catalog.UpsertEconomyItem(MakeItemProfile("item.ready", "evidence.ready"), &error));

        SourceEvidenceRegistry registry;
        RegisterEvidence(
            registry,
            {
                MakeEvidence("evidence.partial", "subject:item:shared"),
                MakeEvidence("evidence.ready", "subject:item:shared"),
            },
            error);

        EconomyDuplicateDetectionService service;
        EconomyDuplicateReport report = service.BuildCrossPackDuplicateReport(registry, catalog, {});
        ASSERT_EQ(report.m_partialGroupCount, 1);
        ASSERT_EQ(report.m_groups.size(), 1);
        EXPECT_EQ(report.m_groups[0].m_status, "partial");

        BlockerRecord blocker;
        blocker.m_blockerId = "blocker.duplicate";
        blocker.m_status = "open";
        blocker.m_subjectRef = "subject:item:shared";
        blocker.m_reason = "Duplicate ownership needs review.";
        report = service.BuildCrossPackDuplicateReport(registry, catalog, { blocker });
        ASSERT_EQ(report.m_blockedGroupCount, 1);
        ASSERT_EQ(report.m_groups.size(), 1);
        EXPECT_EQ(report.m_groups[0].m_status, "blocked");
        EXPECT_EQ(report.m_groups[0].m_blockerIds.size(), 1);
    }

    TEST(TaintedGrailEconomyDuplicateDetectionTests, ReportIsDeterministicAndDoesNotMutateInputs)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakePackRecord(
            "item.z",
            "pack.z",
            "item",
            "subject:item:shared",
            "evidence.z"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakePackRecord(
            "recipe.z",
            "pack.z",
            "recipe",
            "subject:recipe:z",
            "evidence.recipe-z"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakePackRecord(
            "item.a",
            "pack.a",
            "item",
            "subject:item:shared",
            "evidence.a"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakePackRecord(
            "recipe.a",
            "pack.a",
            "recipe",
            "subject:recipe:a",
            "evidence.recipe-a"), &error));
        ASSERT_TRUE(catalog.UpsertEconomyItem(MakeItemProfile("item.z", "evidence.z"), &error));
        ASSERT_TRUE(catalog.UpsertEconomyItem(MakeItemProfile("item.a", "evidence.a"), &error));
        ASSERT_TRUE(catalog.UpsertEconomyRecipe(
            MakeRecipeProfile("recipe.z", "duplicate.recipe", "evidence.recipe-z"),
            &error));
        ASSERT_TRUE(catalog.UpsertEconomyRecipe(
            MakeRecipeProfile("recipe.a", "duplicate.recipe", "evidence.recipe-a"),
            &error));

        SourceEvidenceRegistry registry;
        RegisterEvidence(
            registry,
            {
                MakeEvidence("evidence.z", "subject:item:shared"),
                MakeEvidence("evidence.a", "subject:item:shared"),
                MakeEvidence("evidence.recipe-z", "subject:recipe:z"),
                MakeEvidence("evidence.recipe-a", "subject:recipe:a"),
            },
            error);

        const size_t recordCountBefore = catalog.GetRecords().size();
        const size_t itemCountBefore = catalog.GetEconomyItems().size();
        const size_t recipeCountBefore = catalog.GetEconomyRecipes().size();
        const size_t sourceCountBefore = registry.GetSources().size();
        const size_t evidenceCountBefore = registry.GetEvidence().size();

        EconomyDuplicateDetectionService service;
        const EconomyDuplicateReport report = service.BuildCrossPackDuplicateReport(registry, catalog, {});

        ASSERT_EQ(report.m_groups.size(), 2);
        EXPECT_EQ(report.m_groups[0].m_signal, "recipe_duplicate_key");
        EXPECT_EQ(report.m_groups[1].m_signal, "subject_ref");
        EXPECT_EQ(report.m_groups[0].m_recordIds[0], "recipe.a");
        EXPECT_EQ(report.m_groups[1].m_recordIds[0], "item.a");
        EXPECT_EQ(catalog.GetRecords().size(), recordCountBefore);
        EXPECT_EQ(catalog.GetEconomyItems().size(), itemCountBefore);
        EXPECT_EQ(catalog.GetEconomyRecipes().size(), recipeCountBefore);
        EXPECT_EQ(registry.GetSources().size(), sourceCountBefore);
        EXPECT_EQ(registry.GetEvidence().size(), evidenceCountBefore);
    }
} // namespace TaintedGrailModdingSDK
