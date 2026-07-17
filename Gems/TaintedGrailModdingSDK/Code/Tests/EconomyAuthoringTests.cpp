/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"
#include "EconomyAuthoringService.h"

#include <AzCore/std/algorithm.h>
#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        CatalogRecord MakeEconomyRecord(
            AZStd::string recordId,
            AZStd::string recordKind,
            AZStd::string subjectRef)
        {
            CatalogRecord record;
            record.m_recordId = AZStd::move(recordId);
            record.m_domain = "economy";
            record.m_recordKind = AZStd::move(recordKind);
            record.m_subjectRef = AZStd::move(subjectRef);
            record.m_nativeRefExact = "native:";
            record.m_nativeRefExact += record.m_recordId;
            record.m_identityKind = "native";
            record.m_researchStage = "S5";
            record.m_confidence = "documented";
            record.m_operationalRisk = "low";
            record.m_validationState = "unvalidated";
            record.m_stalenessState = "unknown";
            record.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
            record.m_evidenceIds = { "evidence.test" };
            return record;
        }

        EconomyRecipeProfile MakeRecipeProfile()
        {
            EconomyRecipeProfile profile;
            profile.m_recordId = "recipe.test";
            profile.m_recipeType = "handcrafting";
            profile.m_recipeTab = "general";
            profile.m_stationRecordIds = { "station.test" };
            profile.m_unlockMode = "learned";
            profile.m_duplicateKey = "recipe.test.duplicate";
            profile.m_persistenceMode = "native_template";
            profile.m_evidenceIds = { "evidence.test" };
            return profile;
        }
    } // namespace

    TEST(TaintedGrailEconomyAuthoringTests, ItemProfileRequiresCanonicalEconomyItem)
    {
        CatalogDatabase catalog;
        EconomyItemProfile profile;
        profile.m_recordId = "item.test";
        profile.m_category = "material";
        profile.m_evidenceIds = { "evidence.test" };

        AZStd::string error;
        EXPECT_FALSE(catalog.UpsertEconomyItem(profile, &error));
        EXPECT_NE(error.find("canonical economy/item"), AZStd::string::npos);

        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("item.test", "item", "subject:item:test"),
            &error));
        EXPECT_TRUE(catalog.UpsertEconomyItem(profile, &error));
        ASSERT_NE(catalog.FindEconomyItem("item.test"), nullptr);
    }

    TEST(TaintedGrailEconomyAuthoringTests, RecipeStationsRequireCanonicalStationRecords)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("recipe.test", "recipe", "subject:recipe:test"),
            &error));

        EconomyRecipeProfile profile = MakeRecipeProfile();
        EXPECT_FALSE(catalog.UpsertEconomyRecipe(profile, &error));
        EXPECT_NE(error.find("station"), AZStd::string::npos);

        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("station.test", "crafting_station", "subject:station:test"),
            &error));
        EXPECT_TRUE(catalog.UpsertEconomyRecipe(profile, &error));
    }

    TEST(TaintedGrailEconomyAuthoringTests, IngredientAndOutputJoinsValidateIdentityQuantityAndChance)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("recipe.test", "recipe", "subject:recipe:test"),
            &error));
        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("station.test", "crafting_station", "subject:station:test"),
            &error));
        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("item.test", "item", "subject:item:test"),
            &error));
        ASSERT_TRUE(catalog.UpsertEconomyRecipe(MakeRecipeProfile(), &error));

        EconomyRecipeIngredient ingredient;
        ingredient.m_linkId = "ingredient.test";
        ingredient.m_recipeRecordId = "recipe.test";
        ingredient.m_itemRecordId = "item.test";
        ingredient.m_itemSubjectRef = "subject:item:duplicate";
        ingredient.m_quantity = 1;
        ingredient.m_evidenceIds = { "evidence.test" };
        EXPECT_FALSE(catalog.UpsertRecipeIngredient(ingredient, &error));
        EXPECT_NE(error.find("exactly one"), AZStd::string::npos);

        ingredient.m_itemSubjectRef.clear();
        EXPECT_TRUE(catalog.UpsertRecipeIngredient(ingredient, &error));

        EconomyRecipeOutput output;
        output.m_linkId = "output.test";
        output.m_recipeRecordId = "recipe.test";
        output.m_itemRecordId = "item.test";
        output.m_quantity = 1;
        output.m_chance = 1.1;
        output.m_evidenceIds = { "evidence.test" };
        EXPECT_FALSE(catalog.UpsertRecipeOutput(output, &error));
        EXPECT_NE(error.find("chance"), AZStd::string::npos);

        output.m_chance = 1.0;
        EXPECT_TRUE(catalog.UpsertRecipeOutput(output, &error));
        EXPECT_EQ(catalog.FindIngredientsForRecipe("recipe.test").size(), 1);
        EXPECT_EQ(catalog.FindOutputsForRecipe("recipe.test").size(), 1);
    }

    TEST(TaintedGrailEconomyAuthoringTests, EconomyDataRoundTripsThroughCanonicalDocument)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("item.test", "item", "subject:item:test"),
            &error));
        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("recipe.test", "recipe", "subject:recipe:test"),
            &error));
        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("station.test", "crafting_station", "subject:station:test"),
            &error));

        EconomyItemProfile item;
        item.m_recordId = "item.test";
        item.m_category = "material";
        item.m_evidenceIds = { "evidence.test" };
        ASSERT_TRUE(catalog.UpsertEconomyItem(item, &error));
        ASSERT_TRUE(catalog.UpsertEconomyRecipe(MakeRecipeProfile(), &error));

        EconomyRecipeOutput output;
        output.m_linkId = "output.test";
        output.m_recipeRecordId = "recipe.test";
        output.m_itemRecordId = "item.test";
        output.m_quantity = 2;
        output.m_chance = 1.0;
        output.m_evidenceIds = { "evidence.test" };
        ASSERT_TRUE(catalog.UpsertRecipeOutput(output, &error));

        GameProfile profile;
        profile.m_profileId = "foa.test";
        profile.m_gameVersion = "test-version";
        profile.m_branch = "mono";
        WorkspaceModel workspace;
        workspace.m_workspaceId = "workspace.test";

        const CatalogDocument document = catalog.BuildDocument(workspace, profile);
        CatalogDatabase restored;
        ASSERT_TRUE(restored.ReplaceFromDocument(document, &error));
        EXPECT_EQ(restored.GetEconomyItems().size(), 1);
        EXPECT_EQ(restored.GetEconomyRecipes().size(), 1);
        EXPECT_EQ(restored.GetRecipeOutputs().size(), 1);
    }

    TEST(TaintedGrailEconomyAuthoringTests, AcquisitionRelationshipStartsUnvalidatedAndForbidden)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("item.test", "item", "subject:item:test"),
            &error));
        ASSERT_TRUE(catalog.InsertNew(
            MakeEconomyRecord("vendor.test", "vendor", "subject:vendor:test"),
            &error));

        EconomyAcquisitionRequest request;
        request.m_relationshipId = "relationship.item-vendor";
        request.m_sourceRecordId = "item.test";
        request.m_targetRecordId = "vendor.test";
        request.m_relationshipKind = "sold_by";
        request.m_evidenceIds = { "evidence.test" };

        EconomyAuthoringService service;
        AZ::Outcome<CatalogRelationship, AZStd::string> result = service.BuildAcquisitionRelationship(request, catalog);
        ASSERT_TRUE(result.IsSuccess());
        const CatalogRelationship& relationship = result.GetValue();
        EXPECT_EQ(relationship.m_validationState, "unvalidated");
        EXPECT_TRUE(relationship.m_allowedUsages.empty());
        EXPECT_NE(
            AZStd::find(
                relationship.m_forbiddenUsages.begin(),
                relationship.m_forbiddenUsages.end(),
                "no_unvalidated_runtime_use"),
            relationship.m_forbiddenUsages.end());
    }

    TEST(TaintedGrailEconomyAuthoringTests, ActionLaneMatrixReflectsGovernedAllowedAndForbiddenState)
    {
        CatalogRecord record = MakeEconomyRecord("item.test", "item", "subject:item:test");
        record.m_validationState = "validated";
        record.m_stalenessState = "current";
        record.m_forbiddenUsages = { "custom_item_registration" };
        record.m_allowedUsages = { "existing_item_grant" };

        EconomyAuthoringService service;
        const AZStd::vector<EconomyActionLaneStatus> lanes = service.BuildActionLaneMatrix(record);
        ASSERT_EQ(lanes.size(), 6);
        EXPECT_EQ(lanes[0].m_lane, "existing_item_grant");
        EXPECT_EQ(lanes[0].m_status, "allowed");
        EXPECT_EQ(lanes[2].m_lane, "custom_item_registration");
        EXPECT_EQ(lanes[2].m_status, "forbidden");
    }
} // namespace TaintedGrailModdingSDK
