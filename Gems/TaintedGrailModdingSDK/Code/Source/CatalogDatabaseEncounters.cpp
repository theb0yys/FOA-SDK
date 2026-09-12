/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CatalogDatabase.h"
#include "EncounterPlanningService.h"
#include "PopulationEvidenceValidation.h"
#include "ResearchContractValidation.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/sort.h>

namespace TaintedGrailModdingSDK
{
    const EncounterDefinition* CatalogDatabase::FindEncounterDefinition(const AZStd::string& recordId) const
    {
        for (const auto& definition : m_encounterDefinitions)
        {
            if (definition.m_recordId == recordId) { return &definition; }
        }
        return nullptr;
    }
    const AZStd::vector<EncounterDefinition>& CatalogDatabase::GetEncounterDefinitions() const { return m_encounterDefinitions; }

    bool CatalogDatabase::UpsertEncounterDefinition(const EncounterDefinition& definition, AZStd::string* error)
    {
        const auto* record = FindByRecordId(definition.m_recordId);
        if (!record || record->m_domain != "population" || record->m_recordKind != "encounter"
            || !record->IsSynthetic() || record->m_ownerPackId.empty())
        {
            if (error) { *error = "Encounter definitions require an existing pack-owned synthetic population/encounter record."; }
            return false;
        }
        auto preview = EncounterPlanningService().Preview(definition, *this);
        if (!preview.IsValid())
        {
            if (error) { *error = preview.m_errors.front(); }
            return false;
        }
        if (definition.m_evidenceIds.empty() || definition.m_evidenceIds.size() > 256)
        {
            if (error) { *error = "Encounter definitions require bounded exact authoring evidence."; }
            return false;
        }
        AZStd::unordered_set<AZStd::string> incomingEntries;
        for (const auto& entry : definition.m_entries) { incomingEntries.insert(entry.m_entryId); }
        for (const auto& current : m_encounterDefinitions)
        {
            if (current.m_recordId == definition.m_recordId) { continue; }
            for (const auto& existing : current.m_entries)
            {
                if (incomingEntries.contains(existing.m_entryId))
                {
                    if (error) { *error = "An encounter entry identity cannot be moved from another encounter."; }
                    return false;
                }
            }
        }
        EncounterDefinition canonical = definition;
        AZStd::sort(canonical.m_entries.begin(), canonical.m_entries.end(), [](const auto& a, const auto& b) { return a.m_entryId < b.m_entryId; });
        AZStd::sort(canonical.m_conditions.begin(), canonical.m_conditions.end());
        AZStd::sort(canonical.m_evidenceIds.begin(), canonical.m_evidenceIds.end());
        if (AZStd::adjacent_find(canonical.m_evidenceIds.begin(), canonical.m_evidenceIds.end()) != canonical.m_evidenceIds.end())
        {
            if (error) { *error = "Encounter evidence IDs must be unique."; }
            return false;
        }
        for (const auto& id : canonical.m_evidenceIds)
        {
            if (!IsStableContractId(id)) { if (error) { *error = "Invalid encounter evidence identity."; } return false; }
        }
        for (auto& current : m_encounterDefinitions)
        {
            if (current.m_recordId == canonical.m_recordId) { current = AZStd::move(canonical); return true; }
        }
        if (m_encounterDefinitions.size() >= 10000)
        {
            if (error) { *error = "The catalog supports at most 10,000 encounters."; }
            return false;
        }
        m_encounterDefinitions.push_back(AZStd::move(canonical));
        AZStd::sort(m_encounterDefinitions.begin(), m_encounterDefinitions.end(), [](const auto& a, const auto& b) { return a.m_recordId < b.m_recordId; });
        return true;
    }

    bool CatalogDatabase::ValidateEncounterIntegrity(const GameProfile& profile, const SourceEvidenceRegistry& registry, AZStd::string* error) const
    {
        for (const auto& definition : m_encounterDefinitions)
        {
            const auto* record = FindByRecordId(definition.m_recordId);
            auto preview = EncounterPlanningService().Preview(definition, *this);
            if (!record || record->m_domain != "population" || record->m_recordKind != "encounter"
                || !record->IsSynthetic() || record->m_ownerPackId.empty() || !preview.IsValid())
            {
                if (error) { *error = preview.IsValid() ? "Encounter canonical ownership is invalid." : preview.m_errors.front(); }
                return false;
            }
            AZStd::vector<AZStd::string> subjects, evidence = definition.m_evidenceIds;
            const auto addRecord = [&subjects, &evidence](const CatalogRecord* bound)
            {
                if (bound)
                {
                    AppendUniquePopulationRequiredSubject(subjects, bound->m_subjectRef);
                    AppendUniquePopulationEvidenceIds(evidence, bound->m_evidenceIds);
                }
            };
            addRecord(record);
            addRecord(FindByRecordId(definition.m_placementRecordId));
            for (const auto& entry : definition.m_entries)
            {
                AppendUniquePopulationRequiredSubject(subjects, "encounter-entry:" + entry.m_entryId);
                addRecord(FindByRecordId(entry.m_targetRecordId));
            }
            AZStd::string message;
            if (!ValidatePopulationEvidenceCoverage(evidence, subjects, profile, registry, "Encounter definition", message))
            {
                if (error) { *error = message; }
                return false;
            }
        }
        return true;
    }
}
