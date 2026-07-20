/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

namespace
{
        ReadyFixture MakeReadyFixture(
            bool reverseCatalogOrder = false,
            bool includeVendorRelationshipProof = true)
        {
            ReadyFixture fixture;
            fixture.m_workspace = MakeWorkspace();
            fixture.m_packs = { MakePack() };

            AZStd::string error;
            EXPECT_TRUE(fixture.m_adapterRegistry.RegisterDeclaration(
                MakeDeclaration(),
                &error)) << error.c_str();
            EXPECT_TRUE(fixture.m_sourceRegistry.RegisterSource(
                MakeSource(),
                &error)) << error.c_str();
            RegisterEvidence(
                fixture.m_sourceRegistry,
                MakeEvidence(
                    "evidence.adapter.identity",
                    "adapter:owner.work-order-adapter"));
            for (AdapterCapability capability : AllCapabilities)
            {
                RegisterEvidence(
                    fixture.m_sourceRegistry,
                    MakeEvidence(
                        "evidence.adapter.capability." + ToString(capability),
                        "adapter:owner.work-order-adapter:capability:"
                            + ToString(capability)));
            }

            CatalogRecord nativeItem = MakeRecord(
                "item.native",
                "subject:work-order:item:native",
                "item",
                "source_scoped",
                {
                    "existing_item_grant",
                    "vendor_or_loot_injection",
                    "quest_or_contract_reward_injection",
                });
            CatalogRecord syntheticItem = MakeRecord(
                "item.synthetic",
                "subject:work-order:item:synthetic",
                "item",
                "synthetic",
                { "custom_item_registration" });
            CatalogRecord nativeRecipe = MakeRecord(
                "recipe.native",
                "subject:work-order:recipe:native",
                "recipe",
                "source_scoped",
                { "existing_recipe_learn", "runtime_recipe_append" });
            CatalogRecord syntheticRecipe = MakeRecord(
                "recipe.synthetic",
                "subject:work-order:recipe:synthetic",
                "recipe",
                "synthetic",
                { "custom_recipe_registration" });
            CatalogRecord station = MakeRecord(
                "station.one",
                "subject:work-order:station:one",
                "station",
                "source_scoped",
                {});
            CatalogRecord vendor = MakeRecord(
                "vendor.one",
                "subject:work-order:vendor:one",
                "vendor",
                "source_scoped",
                {});
            CatalogRecord loot = MakeRecord(
                "loot.one",
                "subject:work-order:loot:one",
                "loot_source",
                "source_scoped",
                {});
            CatalogRecord reward = MakeRecord(
                "reward.one",
                "subject:work-order:reward:one",
                "reward_source",
                "source_scoped",
                {});

            AZStd::vector<CatalogRecord> records = {
                nativeItem,
                syntheticItem,
                nativeRecipe,
                syntheticRecipe,
                station,
                vendor,
                loot,
                reward,
            };
            if (reverseCatalogOrder)
            {
                AZStd::reverse(records.begin(), records.end());
            }
            for (const CatalogRecord& record : records)
            {
                RegisterEvidence(
                    fixture.m_sourceRegistry,
                    MakeEvidence(
                        record.m_evidenceIds.front(),
                        record.m_subjectRef));
            }

            auto addRecord = [&fixture, &nativeItem, &syntheticItem, &nativeRecipe,
                              &syntheticRecipe](const CatalogRecord& record)
            {
                if (record.m_recordId == nativeItem.m_recordId)
                {
                    AddRecordWithProof(
                        fixture,
                        record,
                        {
                            "existing_item_grant",
                            "vendor_or_loot_injection",
                            "quest_or_contract_reward_injection",
                        });
                }
                else if (record.m_recordId == syntheticItem.m_recordId)
                {
                    AddRecordWithProof(
                        fixture,
                        record,
                        { "custom_item_registration" });
                }
                else if (record.m_recordId == nativeRecipe.m_recordId)
                {
                    AddRecordWithProof(
                        fixture,
                        record,
                        { "existing_recipe_learn", "runtime_recipe_append" });
                }
                else if (record.m_recordId == syntheticRecipe.m_recordId)
                {
                    AddRecordWithProof(
                        fixture,
                        record,
                        { "custom_recipe_registration" });
                }
                else
                {
                    AZStd::string insertError;
                    EXPECT_TRUE(fixture.m_catalog.InsertNew(record, &insertError))
                        << insertError.c_str();
                }
            };
            for (const CatalogRecord& record : records)
            {
                addRecord(record);
            }

            EconomyItemProfile itemProfile;
            itemProfile.m_recordId = syntheticItem.m_recordId;
            itemProfile.m_category = "test";
            itemProfile.m_subtype = "synthetic";
            itemProfile.m_localisationNameRef = "loc:item.synthetic:name";
            itemProfile.m_evidenceIds = syntheticItem.m_evidenceIds;
            EXPECT_TRUE(fixture.m_catalog.UpsertEconomyItem(itemProfile, &error))
                << error.c_str();

            EconomyRecipeProfile nativeRecipeProfile;
            nativeRecipeProfile.m_recordId = nativeRecipe.m_recordId;
            nativeRecipeProfile.m_recipeType = "craft";
            nativeRecipeProfile.m_recipeTab = "test";
            nativeRecipeProfile.m_stationRecordIds = { station.m_recordId };
            nativeRecipeProfile.m_duplicateKey = "recipe.native.test";
            nativeRecipeProfile.m_persistenceMode = "native_template";
            nativeRecipeProfile.m_evidenceIds = nativeRecipe.m_evidenceIds;
            EXPECT_TRUE(fixture.m_catalog.UpsertEconomyRecipe(
                nativeRecipeProfile,
                &error)) << error.c_str();

            EconomyRecipeProfile syntheticRecipeProfile;
            syntheticRecipeProfile.m_recordId = syntheticRecipe.m_recordId;
            syntheticRecipeProfile.m_recipeType = "craft";
            syntheticRecipeProfile.m_recipeTab = "test";
            syntheticRecipeProfile.m_stationRecordIds = { station.m_recordId };
            syntheticRecipeProfile.m_duplicateKey = "recipe.synthetic.test";
            syntheticRecipeProfile.m_persistenceMode = "custom_template";
            syntheticRecipeProfile.m_evidenceIds = syntheticRecipe.m_evidenceIds;
            EXPECT_TRUE(fixture.m_catalog.UpsertEconomyRecipe(
                syntheticRecipeProfile,
                &error)) << error.c_str();

            EconomyRecipeOutput nativeOutput;
            nativeOutput.m_linkId = "output.recipe.native";
            nativeOutput.m_recipeRecordId = nativeRecipe.m_recordId;
            nativeOutput.m_itemRecordId = nativeItem.m_recordId;
            nativeOutput.m_evidenceIds = { "evidence.output.recipe.native" };
            RegisterEvidence(
                fixture.m_sourceRegistry,
                MakeEvidence(
                    nativeOutput.m_evidenceIds.front(),
                    "economy-recipe-output:" + nativeOutput.m_linkId));
            EXPECT_TRUE(fixture.m_catalog.UpsertRecipeOutput(nativeOutput, &error))
                << error.c_str();

            EconomyRecipeOutput syntheticOutput;
            syntheticOutput.m_linkId = "output.recipe.synthetic";
            syntheticOutput.m_recipeRecordId = syntheticRecipe.m_recordId;
            syntheticOutput.m_itemRecordId = syntheticItem.m_recordId;
            syntheticOutput.m_evidenceIds = { "evidence.output.recipe.synthetic" };
            RegisterEvidence(
                fixture.m_sourceRegistry,
                MakeEvidence(
                    syntheticOutput.m_evidenceIds.front(),
                    "economy-recipe-output:" + syntheticOutput.m_linkId));
            EXPECT_TRUE(fixture.m_catalog.UpsertRecipeOutput(syntheticOutput, &error))
                << error.c_str();

            const CatalogRelationship vendorRelationship = MakeRelationship(
                "relationship.vendor",
                nativeItem,
                vendor,
                "sold_by");
            const CatalogRelationship lootRelationship = MakeRelationship(
                "relationship.loot",
                nativeItem,
                loot,
                "dropped_by");
            const CatalogRelationship rewardRelationship = MakeRelationship(
                "relationship.reward",
                nativeItem,
                reward,
                "rewarded_by");
            for (const CatalogRelationship* relationship : {
                     &vendorRelationship,
                     &lootRelationship,
                     &rewardRelationship })
            {
                RegisterEvidence(
                    fixture.m_sourceRegistry,
                    MakeEvidence(
                        relationship->m_evidenceIds.front(),
                        "relationship:" + relationship->m_relationshipId));
            }
            AddRelationshipWithProof(
                fixture,
                vendorRelationship,
                includeVendorRelationshipProof);
            AddRelationshipWithProof(fixture, lootRelationship, true);
            AddRelationshipWithProof(fixture, rewardRelationship, true);
            return fixture;
        }

} // namespace
