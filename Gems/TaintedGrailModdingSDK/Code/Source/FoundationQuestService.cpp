/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "FoundationService.h"
#include "QuestAuthoringService.h"
#include <AzCore/std/algorithm.h>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString QQ(const AZStd::string& v) { return QString::fromUtf8(v.c_str()); }
        AZStd::string QA(const QString& v) { const auto b = v.toUtf8(); return {b.constData(), static_cast<size_t>(b.size())}; }
        AZStd::string QId(const char* prefix) { return QA(QString::fromUtf8(prefix) + QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-')); }
        bool QError(AZStd::string* error, const AZStd::string& message) { if (error) { *error = message; } return false; }
    }
    bool FoundationService::ReadQuestDocument(const AZStd::string& path, QuestAuthoringDraft& draft, AZStd::string* error) const
    {
        draft = {}; QFile file(QQ(path)); const QFileInfo info(file);
        if (!info.isFile() || info.size() < 1 || info.size() > 1024 * 1024 || !file.open(QIODevice::ReadOnly))
        { return QError(error, "Choose a readable QuestDefinition JSON file up to 1 MiB."); }
        const auto bytes = file.read(1024 * 1024 + 1);
        if (bytes.size() != info.size() || bytes.size() > 1024 * 1024)
        { return QError(error, "Quest document changed size or exceeded the read limit."); }
        const auto result = ParseQuestDefinitionJsonV1(
            AZStd::string(bytes.constData(), static_cast<size_t>(bytes.size())), draft.m_definition);
        if (!result.IsValid())
        {
            AZStd::string message = "Cannot adopt this document:";
            for (const auto& issue : result.m_issues) { message += "\n" + issue.m_code + ": " + issue.m_propertyPath; }
            return QError(error, message);
        }
        if (error) { error->clear(); } return true;
    }
    bool FoundationService::CreateQuestDefinition(const AZStd::string& name, AZStd::string& id, AZStd::string* error)
    {
        id.clear(); QuestAuthoringDraft draft; auto& d = draft.m_definition;
        d.m_questId = QId("quest.custom."); d.m_contentVersion = "1.0.0"; d.m_ownerModuleId = "module.quests";
        d.m_display = {"loc.quest.name", "loc.quest.summary", name, ""}; d.m_lifecycle = "registered"; d.m_minimumSdkVersion = "1.0.0";
        const auto start = QId("phase."), end = QId("phase."), transition = QId("transition."), outcome = QId("outcome.");
        const auto role = QId("role."), objective = QId("objective."), requirement = QId("binding.");
        d.m_roles = {{role, "loc.role.participant", false}};
        d.m_objectives = {{objective, start, "loc.objective.complete", {}, {}}};
        d.m_bindingRequirements = {{requirement, role, "subject.actor", "usage.quest-participant"}};
        d.m_phases = {{start, "loc.phase.start", true, false, {}, {objective}}, {end, "loc.phase.end", false, true, {}, {}}};
        d.m_transitions = {{transition, start, end, "trigger.completed", 0, {}, {}, false}};
        d.m_outcomes = {{outcome, end, "loc.outcome.complete"}};
        draft.m_labels = {{start, "Start"}, {end, "Complete"}, {transition, "Finish"}, {outcome, "Completed"},
            {role, "Participant"}, {objective, "Complete the objective"}, {requirement, "Participant binding"}};
        if (!CommitQuestDefinition(draft, {}, true, error)) { return false; } id = d.m_questId; return true;
    }
    bool FoundationService::AdoptQuestDefinition(QuestAuthoringDraft draft, AZStd::string& id, AZStd::string* error)
    {
        id.clear(); const auto old = draft.m_definition.m_questId; const auto replacement = QId("quest.custom.");
        draft.m_definition.m_questId = replacement; draft.m_definition.m_questFingerprint.clear();
        for (auto& a : draft.m_definition.m_actions) { if (a.m_subjectId == old) { a.m_subjectId = replacement; } }
        for (auto& c : draft.m_definition.m_conditions) { if (c.m_subjectId == old) { c.m_subjectId = replacement; } }
        for (auto& label : draft.m_labels) { if (label.m_id == old) { label.m_id = replacement; } }
        for (auto& binding : draft.m_bindings) { if (binding.m_subjectId == old) { binding.m_subjectId = replacement; } }
        if (!CommitQuestDefinition(draft, {}, true, error)) { return false; } id = replacement; return true;
    }
    bool FoundationService::SaveQuestDefinition(const QuestAuthoringDraft& draft, const AZStd::string& expectedRevision, AZStd::string* error)
    { return CommitQuestDefinition(draft, expectedRevision, false, error); }
    bool FoundationService::CommitQuestDefinition(QuestAuthoringDraft draft, const AZStd::string& expectedRevision, bool creating, AZStd::string* error)
    {
        const auto* pack = GetActivePack(); const auto* profile = m_workspace.FindActiveGameProfile();
        if (!pack || !profile || !profile->IsConfigured() || GetActivePackFilePath().empty()
            || !pack->HasStableIdentity() || !pack->UsesSupportedSchema() || pack->m_runtimeActionsEnabled
            || pack->m_targetBranch != profile->m_branch || (pack->m_targetGameVersion != profile->m_gameVersion
                && AZStd::find(pack->m_compatibleGameVersions.begin(), pack->m_compatibleGameVersions.end(), profile->m_gameVersion) == pack->m_compatibleGameVersions.end()))
        { return QError(error, "Choose and save a compatible authoring mod first."); }
        auto& d = draft.m_definition;
        const auto name = QQ(d.m_display.m_fallbackName).trimmed();
        if (name.isEmpty()) { return QError(error, "Enter a quest name before saving."); }
        d.m_display.m_fallbackName = QA(name);
        const auto* old = m_catalog.FindQuestProfile(d.m_questId); const auto* record = m_catalog.FindByRecordId(d.m_questId);
        if (creating)
        {
            if (record) { return QError(error, "This quest identity already exists."); }
            d.m_ownerPackId = pack->m_packId;
        }
        else if (!old || !record || record->m_ownerPackId != pack->m_packId || !record->IsSynthetic()
            || d.m_ownerPackId != pack->m_packId || expectedRevision.empty() || QuestAuthoringService::Revision(*old) != expectedRevision)
        { return QError(error, "The saved quest or active mod changed. Revert and reopen the latest quest before saving."); }
        d.m_questFingerprint.clear();
        const auto inspection = QuestAuthoringService::Inspect(draft, m_catalog);
        if (!inspection.IsValid()) { return QError(error, inspection.m_errors.front()); }
        auto persisted = QuestAuthoringService::CanonicalProfile(draft);
        const auto subject = creating ? "pack:" + pack->m_packId + "/" + d.m_questId : record->m_subjectRef;
        const auto evidenceId = QId("evidence.authored.quest.");
        const QJsonArray rows{QJsonObject{{"evidence_id", QQ(evidenceId)}, {"subject_ref", QQ(subject)},
            {"kind", "quest"}, {"confidence", "documented"},
            {"claim", "User-authored inert quest definition and metadata; revision " + QQ(QuestAuthoringService::Revision(persisted))},
            {"definition", QJsonDocument::fromJson(QQ(persisted.m_definitionJson).toUtf8()).object()}}};
        SourceEvidenceRegistry registry = m_sourceRegistry; SourceImportResult imported;
        if (!PrepareAuthoredPopulationEvidence(QA(QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact))), registry, imported, error)) { return false; }
        persisted.m_evidenceIds = {evidenceId};
        CatalogDatabase candidate = m_catalog;
        if (creating)
        {
            CatalogPromotionRequest request; request.m_recordId = d.m_questId; request.m_domain = "narrative"; request.m_recordKind = "quest";
            request.m_subjectRef = subject; request.m_identityKind = "synthetic"; request.m_ownerPackId = pack->m_packId;
            request.m_displayName = d.m_display.m_fallbackName; request.m_evidenceId = evidenceId; request.m_confidence = "documented"; request.m_researchStage = "S1";
            const auto promoted = m_catalogPromotion.BuildReviewedRecord(request, m_workspace, m_packs, registry);
            if (!promoted.IsSuccess()) { return QError(error, promoted.GetError()); }
            if (!candidate.InsertNew(promoted.GetValue(), error)) { return false; }
        }
        else
        {
            CatalogRecord updated = *record; updated.m_displayName = d.m_display.m_fallbackName;
            if (!candidate.Upsert(updated, error)) { return false; }
        }
        if (!candidate.UpsertQuestProfile(persisted, error)) { return false; }
        return CommitPopulationIntake(candidate, AZStd::move(registry), imported, error);
    }
}
