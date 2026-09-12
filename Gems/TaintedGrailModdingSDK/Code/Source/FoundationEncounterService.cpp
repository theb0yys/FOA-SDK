/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "FoundationService.h"
#include "EncounterPlanningService.h"
#include <AzCore/std/algorithm.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString Q(const AZStd::string& value) { return QString::fromUtf8(value.c_str()); }
        AZStd::string A(const QString& value)
        {
            const auto bytes = value.toUtf8();
            return {bytes.constData(), static_cast<size_t>(bytes.size())};
        }
        AZStd::string Id(const char* prefix) { return A(QString::fromUtf8(prefix) + QUuid::createUuid().toString(QUuid::WithoutBraces)); }
        bool Fail(AZStd::string* error, const char* message) { if (error) { *error = message; } return false; }
        QJsonObject Intent(const AZStd::string& subject, const QJsonObject& values)
        {
            return {{"evidence_id", Q(Id("evidence.authored.encounter."))}, {"subject_ref", Q(subject)},
                {"kind", "encounter"}, {"confidence", "documented"},
                {"claim", "User-authored encounter intent: " + QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact))}};
        }
    }

    bool FoundationService::CreateEncounterDefinition(const AZStd::string& name, const AZStd::string& initialTargetId,
        AZStd::string& recordId, AZStd::string* error)
    {
        recordId.clear();
        EncounterDefinition definition;
        definition.m_recordId = Id("custom.encounter.");
        EncounterEntry entry;
        entry.m_entryId = Id("custom.encounter-entry."); entry.m_targetRecordId = initialTargetId;
        definition.m_entries.push_back(entry);
        if (!CommitAuthoredEncounter(definition, name, true, error)) { return false; }
        recordId = definition.m_recordId;
        return true;
    }

    bool FoundationService::SaveEncounterDefinition(const EncounterDefinition& definition, const AZStd::string& name,
        AZStd::string* error)
    {
        return CommitAuthoredEncounter(definition, name, false, error);
    }

    bool FoundationService::CommitAuthoredEncounter(const EncounterDefinition& definition, const AZStd::string& name,
        bool creating, AZStd::string* error)
    {
        const auto* profile = m_workspace.FindActiveGameProfile();
        const auto* pack = GetActivePack();
        if (!profile || !profile->IsConfigured() || !pack || GetActivePackFilePath().empty()
            || !pack->HasStableIdentity() || !pack->UsesSupportedSchema() || pack->m_runtimeActionsEnabled
            || pack->m_targetBranch != profile->m_branch
            || (pack->m_targetGameVersion != profile->m_gameVersion
                && AZStd::find(pack->m_compatibleGameVersions.begin(), pack->m_compatibleGameVersions.end(), profile->m_gameVersion)
                    == pack->m_compatibleGameVersions.end()))
        {
            return Fail(error, "Save a compatible authoring mod before creating or changing an encounter.");
        }
        const auto trimmedName = Q(name).trimmed();
        if (trimmedName.isEmpty() || name.size() > 512)
        {
            return Fail(error, "Enter an encounter name up to 512 bytes.");
        }
        for (unsigned char c : name)
        {
            if (c < 0x20 || c == 0x7f) { return Fail(error, "Encounter names must be single-line text."); }
        }
        const auto* existing = m_catalog.FindByRecordId(definition.m_recordId);
        if ((creating && existing) || (!creating && (!existing || !m_catalog.FindEncounterDefinition(definition.m_recordId)
            || existing->m_recordKind != "encounter" || existing->m_domain != "population"
            || !existing->IsSynthetic() || existing->m_ownerPackId != pack->m_packId)))
        {
            return Fail(error, "Choose an encounter owned by the active mod; this definition was not changed.");
        }
        const auto preview = EncounterPlanningService().Preview(definition, m_catalog);
        if (!preview.IsValid()) { if (error) { *error = preview.m_errors.front(); } return false; }
        EncounterDefinition authored = definition;
        authored.m_evidenceIds.clear();
        const AZStd::string subject = creating ? "pack:" + pack->m_packId + "/" + definition.m_recordId : existing->m_subjectRef;
        QJsonArray conditions, entries, rows;
        for (const auto& condition : authored.m_conditions) { conditions.append(Q(condition)); }
        for (const auto& entry : authored.m_entries)
        {
            const QJsonObject data{{"entry_id", Q(entry.m_entryId)}, {"encounter_record_id", Q(authored.m_recordId)},
                {"target_record_id", Q(entry.m_targetRecordId)}, {"minimum_count", static_cast<int>(entry.m_minimumCount)},
                {"maximum_count", static_cast<int>(entry.m_maximumCount)}};
            entries.append(data);
            const auto row = Intent("encounter-entry:" + entry.m_entryId, data);
            rows.append(row); authored.m_evidenceIds.push_back(A(row.value("evidence_id").toString()));
        }
        const QJsonObject data{{"record_id", Q(authored.m_recordId)}, {"name", trimmedName},
            {"placement_record_id", Q(authored.m_placementRecordId)}, {"placement_subject", Q(authored.m_placementSubjectRef)},
            {"activation_mode", Q(authored.m_activationMode)}, {"conditions", conditions},
            {"maximum_active_instances", static_cast<int>(authored.m_maximumActiveInstances)},
            {"population_limit", static_cast<int>(authored.m_populationLimit)}, {"unique_encounter", authored.m_uniqueEncounter},
            {"cleanup_notes", Q(authored.m_cleanupNotes)}, {"rollback_notes", Q(authored.m_rollbackNotes)}, {"entries", entries}};
        const auto intent = Intent(subject, data);
        const auto intentId = A(intent.value("evidence_id").toString());
        rows.append(intent); authored.m_evidenceIds.push_back(intentId);

        SourceEvidenceRegistry registry = m_sourceRegistry;
        SourceImportResult imported;
        if (!PrepareAuthoredPopulationEvidence(A(QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact))),
                registry, imported, error)) { return false; }
        CatalogDatabase candidate = m_catalog;
        if (creating)
        {
            CatalogPromotionRequest promotion;
            promotion.m_recordId = authored.m_recordId; promotion.m_domain = "population"; promotion.m_recordKind = "encounter";
            promotion.m_subjectRef = subject; promotion.m_identityKind = "synthetic"; promotion.m_ownerPackId = pack->m_packId;
            promotion.m_displayName = A(trimmedName); promotion.m_evidenceId = intentId;
            promotion.m_confidence = "documented"; promotion.m_researchStage = "S1";
            auto record = m_catalogPromotion.BuildReviewedRecord(promotion, m_workspace, m_packs, registry);
            if (!record.IsSuccess()) { if (error) { *error = record.GetError(); } return false; }
            if (!candidate.InsertNew(record.GetValue(), error)) { return false; }
        }
        else
        {
            CatalogRecord record = *existing;
            record.m_displayName = A(trimmedName);
            // Original canonical evidence stays attached; the definition contains fresh complete intent.
            if (!candidate.Upsert(record, error)) { return false; }
        }
        if (!candidate.UpsertEncounterDefinition(authored, error)) { return false; }
        return CommitPopulationIntake(candidate, AZStd::move(registry), imported, error);
    }
} // namespace TaintedGrailModdingSDK
