/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "EconomyBlockerService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool Contains(const AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        const CatalogRecord* FindRecord(const CatalogDatabase& catalog, const AZStd::string& recordId)
        {
            return recordId.empty() ? nullptr : catalog.FindByRecordId(recordId);
        }
    } // namespace

    AZStd::vector<BlockerRecord> EconomyBlockerService::Evaluate(
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog) const
    {
        AZStd::vector<BlockerRecord> blockers;

        for (const CatalogRecord& record : catalog.GetRecords())
        {
            if (record.m_domain != "economy")
            {
                continue;
            }

            if (record.m_recordKind == "item")
            {
                const EconomyItemProfile* profile = catalog.FindEconomyItem(record.m_recordId);
                if (!profile)
                {
                    blockers.push_back(MakeBlocker(
                        "economy.item-profile." + record.m_recordId,
                        "warning",
                        record.m_subjectRef,
                        "The canonical item has no typed item profile."));
                    continue;
                }

                for (const AZStd::string& evidenceId : profile->m_evidenceIds)
                {
                    const EvidenceRecord* evidence = sourceRegistry.FindEvidence(evidenceId);
                    if (!evidence)
                    {
                        blockers.push_back(MakeBlocker(
                            "economy.item-evidence." + record.m_recordId + "." + evidenceId,
                            "error",
                            record.m_subjectRef,
                            "The item profile references missing evidence: " + evidenceId));
                    }
                    else if (evidence->m_subjectRef != record.m_subjectRef)
                    {
                        blockers.push_back(MakeBlocker(
                            "economy.item-evidence-subject." + record.m_recordId + "." + evidenceId,
                            "error",
                            record.m_subjectRef,
                            "The item profile evidence belongs to a different subject: " + evidenceId));
                    }
                }

                if (profile->m_questItem)
                {
                    for (const AZStd::string& lane : {
                             AZStd::string("existing_item_grant"),
                             AZStd::string("existing_item_consume"),
                             AZStd::string("vendor_or_loot_injection"),
                             AZStd::string("quest_or_contract_reward_injection") })
                    {
                        if (Contains(record.m_allowedUsages, lane))
                        {
                            blockers.push_back(MakeBlocker(
                                "economy.quest-item-lane." + record.m_recordId + "." + lane,
                                "error",
                                record.m_subjectRef,
                                "Quest items cannot use this economy action lane without a dedicated quest-safety review: " + lane,
                                { lane }));
                        }
                    }
                }

                if (profile->m_uniqueItem)
                {
                    for (const AZStd::string& lane : {
                             AZStd::string("vendor_or_loot_injection"),
                             AZStd::string("quest_or_contract_reward_injection") })
                    {
                        if (Contains(record.m_allowedUsages, lane))
                        {
                            blockers.push_back(MakeBlocker(
                                "economy.unique-item-lane." + record.m_recordId + "." + lane,
                                "error",
                                record.m_subjectRef,
                                "Unique items cannot be distributed through this lane without uniqueness and rollback proof: " + lane,
                                { lane }));
                        }
                    }
                }

                if (record.m_identityKind == "native" && Contains(record.m_allowedUsages, "custom_item_registration"))
                {
                    blockers.push_back(MakeBlocker(
                        "economy.native-custom-item-lane." + record.m_recordId,
                        "error",
                        record.m_subjectRef,
                        "A native item record cannot use the custom_item_registration lane.",
                        { "custom_item_registration" }));
                }
                if (record.m_identityKind == "synthetic")
                {
                    for (const AZStd::string& lane : {
                             AZStd::string("existing_item_grant"),
                             AZStd::string("existing_item_consume") })
                    {
                        if (Contains(record.m_allowedUsages, lane))
                        {
                            blockers.push_back(MakeBlocker(
                                "economy.synthetic-existing-item-lane." + record.m_recordId + "." + lane,
                                "error",
                                record.m_subjectRef,
                                "A synthetic item record cannot use an existing native item lane: " + lane,
                                { lane }));
                        }
                    }
                }
            }
            else if (record.m_recordKind == "recipe")
            {
                const EconomyRecipeProfile* profile = catalog.FindEconomyRecipe(record.m_recordId);
                if (!profile)
                {
                    blockers.push_back(MakeBlocker(
                        "economy.recipe-profile." + record.m_recordId,
                        "warning",
                        record.m_subjectRef,
                        "The canonical recipe has no typed recipe profile."));
                    continue;
                }

                if (profile->m_stationRecordIds.empty())
                {
                    blockers.push_back(MakeBlocker(
                        "economy.recipe-station." + record.m_recordId,
                        "error",
                        record.m_subjectRef,
                        "The recipe has no reviewed crafting station or interaction context.",
                        { "existing_recipe_learn", "runtime_recipe_append", "custom_recipe_registration" }));
                }
                if (catalog.FindOutputsForRecipe(record.m_recordId).empty())
                {
                    blockers.push_back(MakeBlocker(
                        "economy.recipe-output." + record.m_recordId,
                        "error",
                        record.m_subjectRef,
                        "The recipe has no typed output join.",
                        { "existing_recipe_learn", "runtime_recipe_append", "custom_recipe_registration" }));
                }
                for (const AZStd::string& evidenceId : profile->m_evidenceIds)
                {
                    const EvidenceRecord* evidence = sourceRegistry.FindEvidence(evidenceId);
                    if (!evidence)
                    {
                        blockers.push_back(MakeBlocker(
                            "economy.recipe-evidence." + record.m_recordId + "." + evidenceId,
                            "error",
                            record.m_subjectRef,
                            "The recipe profile references missing evidence: " + evidenceId));
                    }
                    else if (evidence->m_subjectRef != record.m_subjectRef)
                    {
                        blockers.push_back(MakeBlocker(
                            "economy.recipe-evidence-subject." + record.m_recordId + "." + evidenceId,
                            "error",
                            record.m_subjectRef,
                            "The recipe profile evidence belongs to a different subject: " + evidenceId));
                    }
                }

                if (record.m_identityKind == "native" && Contains(record.m_allowedUsages, "custom_recipe_registration"))
                {
                    blockers.push_back(MakeBlocker(
                        "economy.native-custom-recipe-lane." + record.m_recordId,
                        "error",
                        record.m_subjectRef,
                        "A native recipe record cannot use the custom_recipe_registration lane.",
                        { "custom_recipe_registration" }));
                }
                if (record.m_identityKind == "synthetic" && Contains(record.m_allowedUsages, "existing_recipe_learn"))
                {
                    blockers.push_back(MakeBlocker(
                        "economy.synthetic-native-recipe-lane." + record.m_recordId,
                        "error",
                        record.m_subjectRef,
                        "A synthetic recipe record cannot use the existing_recipe_learn lane.",
                        { "existing_recipe_learn" }));
                }
            }
        }

        for (const EconomyRecipeIngredient& ingredient : catalog.GetRecipeIngredients())
        {
            const CatalogRecord* recipe = FindRecord(catalog, ingredient.m_recipeRecordId);
            const AZStd::string subject = recipe ? recipe->m_subjectRef : ingredient.m_recipeRecordId;
            if (!ingredient.m_itemSubjectRef.empty())
            {
                blockers.push_back(MakeBlocker(
                    "economy.ingredient-unresolved." + ingredient.m_linkId,
                    "warning",
                    subject,
                    "Recipe ingredient remains unresolved: " + ingredient.m_itemSubjectRef,
                    { "runtime_recipe_append", "custom_recipe_registration" }));
            }
            for (const AZStd::string& evidenceId : ingredient.m_evidenceIds)
            {
                if (!sourceRegistry.FindEvidence(evidenceId))
                {
                    blockers.push_back(MakeBlocker(
                        "economy.ingredient-evidence." + ingredient.m_linkId + "." + evidenceId,
                        "error",
                        subject,
                        "Recipe ingredient references missing evidence: " + evidenceId));
                }
            }
        }

        for (const EconomyRecipeOutput& output : catalog.GetRecipeOutputs())
        {
            const CatalogRecord* recipe = FindRecord(catalog, output.m_recipeRecordId);
            const AZStd::string subject = recipe ? recipe->m_subjectRef : output.m_recipeRecordId;
            if (!output.m_itemSubjectRef.empty())
            {
                blockers.push_back(MakeBlocker(
                    "economy.output-unresolved." + output.m_linkId,
                    "error",
                    subject,
                    "Recipe output remains unresolved: " + output.m_itemSubjectRef,
                    { "existing_recipe_learn", "runtime_recipe_append", "custom_recipe_registration" }));
            }
            for (const AZStd::string& evidenceId : output.m_evidenceIds)
            {
                if (!sourceRegistry.FindEvidence(evidenceId))
                {
                    blockers.push_back(MakeBlocker(
                        "economy.output-evidence." + output.m_linkId + "." + evidenceId,
                        "error",
                        subject,
                        "Recipe output references missing evidence: " + evidenceId));
                }
            }
        }

        return blockers;
    }

    BlockerRecord EconomyBlockerService::MakeBlocker(
        AZStd::string blockerId,
        AZStd::string severity,
        AZStd::string subjectRef,
        AZStd::string reason,
        AZStd::vector<AZStd::string> affectedUsages)
    {
        BlockerRecord blocker;
        blocker.m_blockerId = AZStd::move(blockerId);
        blocker.m_severity = AZStd::move(severity);
        blocker.m_area = "economy-authoring";
        blocker.m_subjectRef = AZStd::move(subjectRef);
        blocker.m_reason = AZStd::move(reason);
        blocker.m_status = "open";
        blocker.m_affectedUsages = AZStd::move(affectedUsages);
        return blocker;
    }
} // namespace TaintedGrailModdingSDK
