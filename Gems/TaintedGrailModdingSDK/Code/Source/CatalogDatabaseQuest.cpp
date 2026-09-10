/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CatalogDatabase.h"
#include "QuestAuthoringService.h"
#include "PopulationEvidenceValidation.h"
#include "ResearchContractValidation.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/sort.h>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool QFail(AZStd::string* error, const AZStd::string& message) { if (error) { *error = message; } return false; }
    }
    const AZStd::vector<QuestAuthoringProfile>& CatalogDatabase::GetQuestProfiles() const { return m_questProfiles; }
    const QuestAuthoringProfile* CatalogDatabase::FindQuestProfile(const AZStd::string& id) const
    {
        const auto it = AZStd::lower_bound(m_questProfiles.begin(), m_questProfiles.end(), id,
            [](const auto& p, const auto& key) { return p.m_recordId < key; });
        return it != m_questProfiles.end() && it->m_recordId == id ? &*it : nullptr;
    }
    bool CatalogDatabase::UpsertQuestProfile(const QuestAuthoringProfile& input, AZStd::string* error)
    {
        const auto* record = FindByRecordId(input.m_recordId);
        if (!record || !record->IsSynthetic() || record->m_domain != "narrative" || record->m_recordKind != "quest"
            || record->m_ownerPackId.empty() || input.m_evidenceIds.empty() || input.m_evidenceIds.size() > 64)
        { return QFail(error, "Quest authoring requires a pack-owned canonical quest and bounded intent evidence."); }
        if (input.m_definitionJson.size() > 1024 * 1024) { return QFail(error, "Quest definition exceeds 1 MiB."); }
        QuestDefinitionV1 definition;
        const auto parsed = ParseQuestDefinitionJsonV1(input.m_definitionJson, definition);
        if (!parsed.IsValid()) { return QFail(error, "QuestDefinition is invalid: " + parsed.m_issues.front().m_code); }
        if (definition.m_questId != input.m_recordId || definition.m_ownerPackId != record->m_ownerPackId)
        { return QFail(error, "Quest identity or pack ownership does not match its canonical record."); }
        auto draft = QuestAuthoringService::Read(input);
        const auto result = QuestAuthoringService::Inspect(draft, *this);
        if (!result.IsValid()) { return QFail(error, result.m_errors.front()); }
        AZStd::unordered_set<AZStd::string> evidence;
        for (const auto& id : input.m_evidenceIds)
        { if (!IsStableContractId(id) || !evidence.insert(id).second) { return QFail(error, "Quest evidence IDs must be distinct stable identities."); } }
        auto canonical = QuestAuthoringService::CanonicalProfile(draft);
        canonical.m_evidenceIds = input.m_evidenceIds; AZStd::sort(canonical.m_evidenceIds.begin(), canonical.m_evidenceIds.end());
        size_t bytes = canonical.m_definitionJson.size();
        for (const auto& q : m_questProfiles) { if (q.m_recordId != input.m_recordId) { bytes += q.m_definitionJson.size(); } }
        if (bytes > 8 * 1024 * 1024 || (!FindQuestProfile(input.m_recordId) && m_questProfiles.size() >= 256))
        { return QFail(error, "Quest catalog limit reached (256 definitions / 8 MiB)."); }
        auto it = AZStd::lower_bound(m_questProfiles.begin(), m_questProfiles.end(), input.m_recordId,
            [](const auto& q, const auto& key) { return q.m_recordId < key; });
        if (it != m_questProfiles.end() && it->m_recordId == input.m_recordId) { *it = AZStd::move(canonical); }
        else { m_questProfiles.insert(it, AZStd::move(canonical)); }
        return true;
    }
    bool CatalogDatabase::LoadQuestCollections(const CatalogDocument& document, AZStd::string* error)
    {
        if (document.m_questProfiles.size() > 256 || (document.m_schemaVersion < QuestCatalogSchemaVersion && !document.m_questProfiles.empty()))
        { return QFail(error, "Quest collections require schema 6 and at most 256 unique quests."); }
        AZStd::unordered_set<AZStd::string> ids;
        for (const auto& q : document.m_questProfiles)
        {
            if (!ids.insert(q.m_recordId).second) { return QFail(error, "Duplicate quest identity."); }
            if (!UpsertQuestProfile(q, error)) { return false; }
        }
        return true;
    }
    bool CatalogDatabase::ValidateQuestIntegrity(const GameProfile& profile, const SourceEvidenceRegistry& registry, AZStd::string* error) const
    {
        for (const auto& q : m_questProfiles)
        {
            const auto* record = FindByRecordId(q.m_recordId);
            if (!record) { return QFail(error, "Quest record is missing."); }
            const auto draft = QuestAuthoringService::Read(q);
            if (!record->IsSynthetic() || record->m_domain != "narrative" || record->m_recordKind != "quest"
                || record->m_ownerPackId != draft.m_definition.m_ownerPackId || record->m_recordId != draft.m_definition.m_questId)
            { return QFail(error, "Quest ownership or identity changed."); }
            bool currentIntent = false;
            for (const auto& id : q.m_evidenceIds)
            {
                const auto* evidence = registry.FindEvidence(id);
                if (evidence && evidence->m_subjectRef == record->m_subjectRef
                    && evidence->m_claim.find(QuestAuthoringService::Revision(q)) != AZStd::string::npos) { currentIntent = true; }
            }
            if (!currentIntent) { return QFail(error, "Quest intent does not describe the current authored revision."); }
            const auto result = QuestAuthoringService::Inspect(draft, *this);
            if (!result.IsValid()) { return QFail(error, result.m_errors.front()); }
            AZStd::string message;
            if (!ValidatePopulationEvidenceCoverage(q.m_evidenceIds, {record->m_subjectRef}, profile, registry, "Quest intent", message))
            { return QFail(error, message); }
            AZStd::vector<AZStd::string> subjects; AZStd::vector<AZStd::string> evidence;
            const auto include = [&](const AZStd::string& id)
            {
                if (const auto* r = FindByRecordId(id))
                { AppendUniquePopulationRequiredSubject(subjects, r->m_subjectRef); AppendUniquePopulationEvidenceIds(evidence, r->m_evidenceIds); }
            };
            for (const auto& b : q.m_bindings) { include(b.m_recordId); }
            for (const auto& c : draft.m_definition.m_conditions) { include(c.m_subjectId); }
            for (const auto& a : draft.m_definition.m_actions) { include(a.m_subjectId); }
            if (!subjects.empty() && !ValidatePopulationEvidenceCoverage(evidence, subjects, profile, registry, "Quest references", message))
            { return QFail(error, message); }
        }
        return true;
    }
}
