/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"
#include "SocietyPlanningService.h"
#include "PopulationEvidenceValidation.h"
#include "ResearchContractValidation.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/sort.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool SocietyError(AZStd::string* error, const char* text) { if (error) { *error = text; } return false; }
        bool SocietyRecord(const CatalogRecord* record, const char* kind)
        {
            return record && record->m_domain == "society" && record->m_recordKind == kind
                && record->IsSynthetic() && !record->m_ownerPackId.empty();
        }
        bool SocietyEvidence(const AZStd::vector<AZStd::string>& ids)
        {
            if (ids.empty() || ids.size() > 64) { return false; }
            AZStd::unordered_set<AZStd::string> seen;
            for (const auto& id : ids) { if (!IsStableContractId(id) || !seen.insert(id).second) { return false; } }
            return true;
        }
        template<class T>
        void CanonicalSocietyEvidence(T& value) { AZStd::sort(value.m_evidenceIds.begin(), value.m_evidenceIds.end()); }
        template<class T>
        bool UniqueSocietyIds(const AZStd::vector<T>& values)
        {
            AZStd::unordered_set<AZStd::string> ids;
            for (const auto& value : values) { if (!ids.insert(value.m_recordId).second) { return false; } }
            return true;
        }
    }
    const CultureProfile* CatalogDatabase::FindCultureProfile(const AZStd::string& id) const
    {
        for (const auto& value : m_cultureProfiles) { if (value.m_recordId == id) { return &value; } }
        return nullptr;
    }
    const FactionProfile* CatalogDatabase::FindFactionProfile(const AZStd::string& id) const
    {
        for (const auto& value : m_factionProfiles) { if (value.m_recordId == id) { return &value; } }
        return nullptr;
    }
    const AZStd::vector<CultureProfile>& CatalogDatabase::GetCultureProfiles() const { return m_cultureProfiles; }
    const AZStd::vector<FactionProfile>& CatalogDatabase::GetFactionProfiles() const { return m_factionProfiles; }
    const AZStd::vector<FactionLink>& CatalogDatabase::GetFactionLinks() const { return m_factionLinks; }
    AZStd::vector<FactionLink> CatalogDatabase::FindFactionLinks(const AZStd::string& id) const
    {
        AZStd::vector<FactionLink> links;
        for (const auto& link : m_factionLinks) { if (link.m_factionRecordId == id) { links.push_back(link); } }
        return links;
    }
    bool CatalogDatabase::UpsertCultureProfile(const CultureProfile& profile, AZStd::string* error)
    {
        const auto valid = SocietyPlanningService::ValidateCulture(profile);
        if (!valid.IsSuccess()) { if (error) { *error = valid.GetError(); } return false; }
        if (!SocietyRecord(FindByRecordId(profile.m_recordId), "culture") || !SocietyEvidence(profile.m_evidenceIds))
        {
            return SocietyError(error, "Cultures require a pack-owned society/culture record and exact evidence.");
        }
        auto canonical = profile; CanonicalSocietyEvidence(canonical);
        for (auto& current : m_cultureProfiles)
        {
            if (current.m_recordId == canonical.m_recordId) { current = AZStd::move(canonical); return true; }
        }
        if (m_cultureProfiles.size() >= 1000) { return SocietyError(error, "The catalog supports at most 1000 cultures."); }
        m_cultureProfiles.push_back(AZStd::move(canonical));
        AZStd::sort(m_cultureProfiles.begin(), m_cultureProfiles.end(), [](const auto& a, const auto& b) { return a.m_recordId < b.m_recordId; });
        return true;
    }
    bool CatalogDatabase::UpsertFactionProfile(const FactionProfile& profile, AZStd::string* error)
    {
        const auto valid = SocietyPlanningService::ValidateFactionProfile(profile, *this);
        if (!valid.IsSuccess()) { if (error) { *error = valid.GetError(); } return false; }
        if (!SocietyRecord(FindByRecordId(profile.m_recordId), "faction") || !SocietyEvidence(profile.m_evidenceIds))
        {
            return SocietyError(error, "Factions require a pack-owned society/faction record and exact evidence.");
        }
        auto canonical = profile; CanonicalSocietyEvidence(canonical);
        for (auto& current : m_factionProfiles)
        {
            if (current.m_recordId == canonical.m_recordId) { current = AZStd::move(canonical); return true; }
        }
        if (m_factionProfiles.size() >= 1000) { return SocietyError(error, "The catalog supports at most 1000 factions."); }
        m_factionProfiles.push_back(AZStd::move(canonical));
        AZStd::sort(m_factionProfiles.begin(), m_factionProfiles.end(), [](const auto& a, const auto& b) { return a.m_recordId < b.m_recordId; });
        return true;
    }
    bool CatalogDatabase::ReplaceFactionDefinition(const FactionDefinition& definition, AZStd::string* error)
    {
        auto analysis = SocietyPlanningService::Analyze(definition, *this);
        if (!analysis.IsValid()) { if (error) { *error = analysis.m_errors.front(); } return false; }
        AZStd::unordered_map<AZStd::string, const FactionLink*> incoming;
        auto links = definition.m_links;
        for (auto& link : links)
        {
            if (!SocietyEvidence(link.m_evidenceIds)) { return SocietyError(error, "Faction links require bounded exact authoring evidence."); }
            CanonicalSocietyEvidence(link); incoming.emplace(link.m_linkId, &link);
        }
        size_t replacedCount = 0;
        for (const auto& existing : m_factionLinks)
        {
            if (existing.m_factionRecordId == definition.m_profile.m_recordId) { ++replacedCount; }
            const auto found = incoming.find(existing.m_linkId);
            if (found != incoming.end() && (existing.m_factionRecordId != found->second->m_factionRecordId
                || existing.m_kind != found->second->m_kind))
            {
                return SocietyError(error, "A faction link cannot move between factions or change its kind.");
            }
        }
        if (m_factionLinks.size() - replacedCount + links.size() > 10000)
        {
            return SocietyError(error, "The catalog supports at most 10000 faction links.");
        }
        if (!UpsertFactionProfile(definition.m_profile, error)) { return false; }
        m_factionLinks.erase(AZStd::remove_if(m_factionLinks.begin(), m_factionLinks.end(), [&definition](const auto& link)
        { return link.m_factionRecordId == definition.m_profile.m_recordId; }), m_factionLinks.end());
        m_factionLinks.insert(m_factionLinks.end(), links.begin(), links.end());
        AZStd::sort(m_factionLinks.begin(), m_factionLinks.end(), [](const auto& a, const auto& b) { return a.m_linkId < b.m_linkId; });
        return true;
    }
    bool CatalogDatabase::LoadSocietyCollections(const CatalogDocument& document, AZStd::string* error)
    {
        if (document.m_cultureProfiles.size() > 1000 || document.m_factionProfiles.size() > 1000
            || document.m_factionLinks.size() > 10000
            || !UniqueSocietyIds(document.m_cultureProfiles) || !UniqueSocietyIds(document.m_factionProfiles))
        {
            return SocietyError(error, "Society collections exceed their bounds or contain duplicate profile IDs.");
        }
        // Called only on an unpublished candidate. Load all faction identities before resolving directed links.
        for (const auto& culture : document.m_cultureProfiles) { if (!UpsertCultureProfile(culture, error)) { return false; } }
        for (const auto& faction : document.m_factionProfiles) { if (!UpsertFactionProfile(faction, error)) { return false; } }
        AZStd::unordered_map<AZStd::string, AZStd::vector<FactionLink>> groups;
        AZStd::unordered_set<AZStd::string> ids;
        for (const auto& link : document.m_factionLinks)
        {
            if (!ids.insert(link.m_linkId).second || !FindFactionProfile(link.m_factionRecordId)
                || !SocietyEvidence(link.m_evidenceIds))
            {
                return SocietyError(error, "Faction links contain duplicate IDs, missing owners or invalid evidence IDs.");
            }
            groups[link.m_factionRecordId].push_back(link);
        }
        for (const auto& faction : m_factionProfiles)
        {
            const auto analysis = SocietyPlanningService::Analyze({faction, groups[faction.m_recordId]}, *this);
            if (!analysis.IsValid()) { if (error) { *error = analysis.m_errors.front(); } return false; }
        }
        m_factionLinks = document.m_factionLinks;
        for (auto& link : m_factionLinks) { CanonicalSocietyEvidence(link); }
        AZStd::sort(m_factionLinks.begin(), m_factionLinks.end(), [](const auto& a, const auto& b) { return a.m_linkId < b.m_linkId; });
        return true;
    }
    bool CatalogDatabase::ValidateSocietyIntegrity(const GameProfile& profile, const SourceEvidenceRegistry& registry, AZStd::string* error) const
    {
        const auto coverage = [this, &profile, &registry, error](AZStd::vector<AZStd::string> evidence,
            AZStd::vector<AZStd::string> subjects, const AZStd::vector<AZStd::string>& records)
        {
            for (const auto& id : records)
            {
                const auto* record = FindByRecordId(id);
                if (record)
                {
                    AppendUniquePopulationRequiredSubject(subjects, record->m_subjectRef);
                    AppendUniquePopulationEvidenceIds(evidence, record->m_evidenceIds);
                }
            }
            AZStd::string message;
            if (!ValidatePopulationEvidenceCoverage(evidence, subjects, profile, registry, "Society authoring", message))
            {
                if (error) { *error = message; } return false;
            }
            return true;
        };
        for (const auto& culture : m_cultureProfiles)
        {
            if (!SocietyRecord(FindByRecordId(culture.m_recordId), "culture") || !SocietyEvidence(culture.m_evidenceIds)
                || !SocietyPlanningService::ValidateCulture(culture).IsSuccess())
            {
                return SocietyError(error, "Culture ownership, fields or evidence IDs are invalid.");
            }
            if (!coverage(culture.m_evidenceIds, {}, {culture.m_recordId})) { return false; }
        }
        AZStd::unordered_map<AZStd::string, AZStd::vector<FactionLink>> groups;
        for (const auto& link : m_factionLinks) { groups[link.m_factionRecordId].push_back(link); }
        for (const auto& faction : m_factionProfiles)
        {
            if (!SocietyRecord(FindByRecordId(faction.m_recordId), "faction") || !SocietyEvidence(faction.m_evidenceIds))
            {
                return SocietyError(error, "Faction ownership or evidence IDs are invalid.");
            }
            const auto analysis = SocietyPlanningService::Analyze({faction, groups[faction.m_recordId]}, *this);
            if (!analysis.IsValid()) { if (error) { *error = analysis.m_errors.front(); } return false; }
            if (!coverage(faction.m_evidenceIds, {}, {faction.m_recordId, faction.m_cultureRecordId})) { return false; }
        }
        for (const auto& link : m_factionLinks)
        {
            if (!FindFactionProfile(link.m_factionRecordId) || !SocietyEvidence(link.m_evidenceIds))
            {
                return SocietyError(error, "Faction link owner or evidence IDs are invalid.");
            }
            if (!coverage(link.m_evidenceIds, {"faction-link:" + link.m_linkId},
                {link.m_factionRecordId, link.m_targetRecordId})) { return false; }
        }
        return true;
    }
}
