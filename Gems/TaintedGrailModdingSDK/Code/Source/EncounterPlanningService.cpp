/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "EncounterPlanningService.h"
#include "ResearchContractValidation.h"
#include <AzCore/std/sort.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool Text(const AZStd::string& value, size_t limit, bool empty = true)
        {
            if (value.size() > limit || (!empty && value.empty())) { return false; }
            for (unsigned char c : value) { if (c < 0x20 || c == 0x7f) { return false; } }
            return true;
        }
    }
    EncounterPreview EncounterPlanningService::Preview(const EncounterDefinition& d, const CatalogDatabase& catalog) const
    {
        EncounterPreview result;
        auto error = [&result](AZStd::string value) { result.m_errors.push_back(AZStd::move(value)); };
        if (!IsStableContractId(d.m_recordId)) { error("An encounter requires a stable canonical record identity."); }
        if (d.m_entries.empty() || d.m_entries.size() > 128)
        {
            error("Add between 1 and 128 distinct actors or troops to the encounter.");
            return result;
        }
        if (!Text(d.m_placementSubjectRef, 1024) || !Text(d.m_cleanupNotes, 1024) || !Text(d.m_rollbackNotes, 1024))
        {
            error("Placement and cleanup/rollback notes must be single-line text up to 1024 bytes.");
        }
        if (d.m_maximumActiveInstances < 1 || d.m_maximumActiveInstances > 1000
            || d.m_populationLimit < 1 || d.m_populationLimit > 1000000)
        {
            error("Use 1 to 1000 active instances and a population limit from 1 to 1,000,000.");
        }
        if (d.m_uniqueEncounter && d.m_maximumActiveInstances != 1)
        {
            error("A unique encounter must have exactly one maximum active instance.");
        }
        if (d.m_activationMode != "manual" && d.m_activationMode != "all_conditions")
        {
            error("Choose manual activation or all listed conditions.");
        }
        if ((d.m_activationMode == "all_conditions" && d.m_conditions.empty())
            || (d.m_activationMode == "manual" && !d.m_conditions.empty()) || d.m_conditions.size() > 64)
        {
            error("Manual activation has no conditions; conditional activation requires 1 to 64 conditions.");
        }
        AZStd::unordered_set<AZStd::string> conditions;
        for (const auto& condition : d.m_conditions)
        {
            if (!Text(condition, 256, false) || !conditions.insert(condition).second)
            {
                error("Activation condition descriptions must be bounded, non-empty and distinct.");
            }
        }
        if (!d.m_conditions.empty())
        {
            result.m_warnings.push_back("Conditions are authoring descriptions; the Editor does not evaluate game state.");
        }
        if (!d.m_placementRecordId.empty())
        {
            const auto* placement = catalog.FindByRecordId(d.m_placementRecordId);
            if (!placement || placement->m_domain != "world"
                || (placement->m_recordKind != "location" && placement->m_recordKind != "scene"
                    && placement->m_recordKind != "region")
                || (!d.m_placementSubjectRef.empty() && d.m_placementSubjectRef != placement->m_subjectRef))
            {
                error("The placement must identify an existing world location, scene or region with an agreeing exact subject.");
            }
            else if (placement->IsBlocked()) { result.m_warnings.push_back("The selected placement record has open review blockers."); }
        }
        else
        {
            result.m_warnings.push_back(d.m_placementSubjectRef.empty()
                ? "No placement is assigned yet. This plan can be saved for further authoring."
                : "The placement subject is unverified and is not bound to a saved world record.");
        }
        if (d.m_cleanupNotes.empty()) { result.m_warnings.push_back("Cleanup requirements have not been described."); }
        if (d.m_rollbackNotes.empty()) { result.m_warnings.push_back("Rollback requirements have not been described."); }

        AZStd::unordered_set<AZStd::string> entryIds, targets, uniqueActors;
        AZStd::unordered_map<AZStd::string, AZ::u64> uniqueCounts;
        for (const auto& actor : catalog.GetPopulationActorProfiles())
        {
            if (actor.m_uniqueActor) { uniqueActors.insert(actor.m_recordId); }
        }
        for (const auto& entry : d.m_entries)
        {
            if (!IsStableContractId(entry.m_entryId) || !entryIds.insert(entry.m_entryId).second
                || !IsStableContractId(entry.m_targetRecordId) || !targets.insert(entry.m_targetRecordId).second)
            {
                error("Every composition row needs a distinct stable entry ID and a distinct actor/troop target.");
                continue;
            }
            if (entry.m_minimumCount < 1 || entry.m_minimumCount > entry.m_maximumCount || entry.m_maximumCount > 1000)
            {
                error("Actor and troop quantities must be ordered positive ranges up to 1000.");
                continue;
            }
            const auto* record = catalog.FindByRecordId(entry.m_targetRecordId);
            if (!record || record->m_domain != "population"
                || (record->m_recordKind != "actor" && record->m_recordKind != "troop"))
            {
                error("Missing actor or troop reference: " + entry.m_targetRecordId);
                continue;
            }
            EncounterPreviewRow row;
            row.m_entryId = entry.m_entryId; row.m_targetRecordId = record->m_recordId;
            row.m_name = record->m_displayName; row.m_kind = record->m_recordKind;
            row.m_minimumActors = entry.m_minimumCount; row.m_maximumActors = entry.m_maximumCount;
            if (record->m_recordKind == "actor")
            {
                if (!catalog.FindPopulationActorProfile(record->m_recordId))
                {
                    error("The selected actor has no saved typed profile: " + record->m_displayName);
                }
                if (uniqueActors.contains(record->m_recordId)) { uniqueCounts[record->m_recordId] += entry.m_maximumCount; }
            }
            else
            {
                const auto* troop = catalog.FindPopulationTroopProfile(record->m_recordId);
                if (!troop) { error("The selected troop has no saved typed profile: " + record->m_displayName); continue; }
                row.m_minimumActors *= troop->m_minimumSize;
                row.m_maximumActors *= troop->m_maximumSize;
                for (const auto& member : catalog.FindPopulationMembersForTroop(record->m_recordId))
                {
                    if (member.m_actorRecordId.empty())
                    {
                        result.m_warnings.push_back("Troop " + record->m_displayName + " contains an unresolved actor subject: " + member.m_actorSubjectRef);
                    }
                    else if (uniqueActors.contains(member.m_actorRecordId))
                    {
                        uniqueCounts[member.m_actorRecordId] += static_cast<AZ::u64>(entry.m_maximumCount) * member.m_maximumCount;
                    }
                }
            }
            if (record->IsBlocked()) { result.m_warnings.push_back("Open catalog review blockers for " + record->m_displayName + "."); }
            result.m_minimumActors += row.m_minimumActors;
            result.m_maximumActors += row.m_maximumActors;
            result.m_rows.push_back(AZStd::move(row));
        }
        result.m_maximumConcurrentActors = result.m_maximumActors * d.m_maximumActiveInstances;
        if (result.m_maximumConcurrentActors > d.m_populationLimit)
        {
            error("The population limit is below the maximum actors across all active instances.");
        }
        for (const auto& [id, count] : uniqueCounts)
        {
            if (count * d.m_maximumActiveInstances > 1) { error("A unique actor may occur more than once: " + id); }
        }
        AZStd::sort(result.m_errors.begin(), result.m_errors.end());
        AZStd::sort(result.m_warnings.begin(), result.m_warnings.end());
        return result;
    }
}
