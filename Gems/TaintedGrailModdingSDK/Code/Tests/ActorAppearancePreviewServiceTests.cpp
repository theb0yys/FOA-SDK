/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorAppearancePreviewService.h"
#include "CatalogDatabase.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        CatalogRecord MakeRecord(
            const char* recordId,
            const char* domain,
            const char* kind,
            const char* subject)
        {
            CatalogRecord record;
            record.m_recordId = recordId;
            record.m_ownerPackId = "pack.stage8.tests";
            record.m_domain = domain;
            record.m_recordKind = kind;
            record.m_subjectRef = subject;
            record.m_identityKind = "synthetic";
            record.m_displayName = recordId;
            record.m_researchStage = "S2";
            record.m_confidence = "documented";
            record.m_operationalRisk = "low";
            record.m_validationState = "unvalidated";
            record.m_stalenessState = "current";
            record.m_evidenceIds = { "evidence.stage8" };
            record.m_createdAt = "2026-08-05T00:00:00Z";
            record.m_updatedAt = "2026-08-05T00:00:00Z";
            return record;
        }

        CatalogRelationship MakeEquipment(
            const char* relationshipId,
            const char* actorId,
            const char* itemId,
            const char* itemSubject,
            const char* kind)
        {
            CatalogRelationship relationship;
            relationship.m_relationshipId = relationshipId;
            relationship.m_fromRecordId = actorId;
            relationship.m_toRecordId = itemId;
            relationship.m_targetSubjectRef = itemSubject;
            relationship.m_relationshipKind = kind;
            relationship.m_evidenceIds = { "evidence.stage8" };
            relationship.m_researchStage = "S2";
            relationship.m_confidence = "documented";
            relationship.m_operationalRisk = "low";
            relationship.m_validationState = "unvalidated";
            relationship.m_stalenessState = "current";
            relationship.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
            relationship.m_createdAt = "2026-08-05T00:00:00Z";
            relationship.m_updatedAt = "2026-08-05T00:00:00Z";
            return relationship;
        }

        CatalogDatabase MakeCatalog()
        {
            CatalogDatabase catalog;
            AZStd::string error;
            EXPECT_TRUE(catalog.InsertNew(
                MakeRecord("actor.stage8", "population", "actor", "subject:actor:stage8"), &error))
                << error.c_str();
            EXPECT_TRUE(catalog.InsertNew(
                MakeRecord("item.sword", "economy", "item", "subject:item:sword"), &error))
                << error.c_str();

            PopulationActorProfile actor;
            actor.m_recordId = "actor.stage8";
            actor.m_actorKind = "npc";
            actor.m_archetype = "test";
            actor.m_minimumLevel = 1;
            actor.m_maximumLevel = 1;
            actor.m_portraitAssetRef = "{11111111-1111-1111-1111-111111111111}:1";
            actor.m_modelAssetRef = "{22222222-2222-2222-2222-222222222222}:2";
            actor.m_evidenceIds = { "evidence.stage8" };
            EXPECT_TRUE(catalog.UpsertPopulationActorProfile(actor, &error)) << error.c_str();

            EconomyItemProfile item;
            item.m_recordId = "item.sword";
            item.m_category = "weapon";
            item.m_assetRef = "{33333333-3333-3333-3333-333333333333}:3";
            item.m_evidenceIds = { "evidence.stage8" };
            EXPECT_TRUE(catalog.UpsertEconomyItem(item, &error)) << error.c_str();
            return catalog;
        }
    } // namespace

    TEST(ActorAppearancePreviewServiceTests, BuildsDeterministicReferenceOnlyView)
    {
        CatalogDatabase catalog = MakeCatalog();
        AZStd::string error;
        EXPECT_TRUE(catalog.UpsertRelationship(
            MakeEquipment(
                "relationship.stage8.main-hand",
                "actor.stage8",
                "item.sword",
                "subject:item:sword",
                "equips_main_hand"),
            &error)) << error.c_str();

        const ActorAppearancePreviewView view =
            ActorAppearancePreviewService::BuildView(catalog, "actor.stage8");

        EXPECT_EQ(view.m_state, ActorAppearancePreviewState::Partial);
        ASSERT_EQ(view.m_equipment.size(), 1);
        EXPECT_EQ(view.m_equipment[0].m_slot, "main_hand");
        EXPECT_EQ(
            view.m_equipment[0].m_itemAssetRef,
            "{33333333-3333-3333-3333-333333333333}:3");
        EXPECT_TRUE(view.m_equipment[0].m_blockers.empty());
        EXPECT_EQ(view.m_relationshipsExamined, 1);
    }

    TEST(ActorAppearancePreviewServiceTests, BlocksTwoHandAndOneHandConflict)
    {
        CatalogDatabase catalog = MakeCatalog();
        AZStd::string error;
        EXPECT_TRUE(catalog.UpsertRelationship(
            MakeEquipment(
                "relationship.stage8.main-hand",
                "actor.stage8",
                "item.sword",
                "subject:item:sword",
                "equips_main_hand"),
            &error)) << error.c_str();
        EXPECT_TRUE(catalog.UpsertRelationship(
            MakeEquipment(
                "relationship.stage8.two-hand",
                "actor.stage8",
                "item.sword",
                "subject:item:sword",
                "equips_two_hand"),
            &error)) << error.c_str();

        const ActorAppearancePreviewView view =
            ActorAppearancePreviewService::BuildView(catalog, "actor.stage8");
        EXPECT_EQ(view.m_state, ActorAppearancePreviewState::Blocked);
        EXPECT_FALSE(view.m_blockers.empty());
    }

    TEST(ActorAppearancePreviewServiceTests, PerformanceGuardExaminesEachRelationshipOnce)
    {
        CatalogDatabase catalog = MakeCatalog();
        AZStd::string error;
        for (size_t index = 0; index < 1000; ++index)
        {
            CatalogRelationship relationship;
            relationship.m_relationshipId = AZStd::string::format(
                "relationship.stage8.unrelated.%zu", index);
            relationship.m_fromRecordId = "item.sword";
            relationship.m_toRecordId = "actor.stage8";
            relationship.m_relationshipKind = "unrelated";
            relationship.m_evidenceIds = { "evidence.stage8" };
            relationship.m_researchStage = "S2";
            relationship.m_confidence = "documented";
            relationship.m_operationalRisk = "low";
            relationship.m_validationState = "unvalidated";
            relationship.m_stalenessState = "current";
            relationship.m_createdAt = "2026-08-05T00:00:00Z";
            relationship.m_updatedAt = "2026-08-05T00:00:00Z";
            EXPECT_TRUE(catalog.UpsertRelationship(relationship, &error)) << error.c_str();
        }

        const ActorAppearancePreviewView first =
            ActorAppearancePreviewService::BuildView(catalog, "actor.stage8");
        const ActorAppearancePreviewView second =
            ActorAppearancePreviewService::BuildView(catalog, "actor.stage8");
        EXPECT_EQ(first.m_relationshipsExamined, catalog.GetRelationships().size());
        EXPECT_EQ(second.m_relationshipsExamined, first.m_relationshipsExamined);
        EXPECT_EQ(second.m_equipment.size(), first.m_equipment.size());
    }
} // namespace TaintedGrailModdingSDK
