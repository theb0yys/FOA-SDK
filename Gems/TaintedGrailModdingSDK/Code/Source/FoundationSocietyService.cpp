/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"
#include "SocietyPlanningService.h"
#include <AzCore/std/algorithm.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString SocietyQ(const AZStd::string& value) { return QString::fromUtf8(value.c_str()); }
        AZStd::string SocietyA(const QString& value)
        {
            const auto bytes = value.toUtf8(); return {bytes.constData(), static_cast<size_t>(bytes.size())};
        }
        AZStd::string SocietyId(const char* prefix)
        {
            return SocietyA(QString::fromUtf8(prefix) + QUuid::createUuid().toString(QUuid::WithoutBraces));
        }
        bool SocietyFail(AZStd::string* error, const char* message) { if (error) { *error = message; } return false; }
        QJsonObject SocietyIntent(const AZStd::string& subject, const QJsonObject& values)
        {
            return {{"evidence_id", SocietyQ(SocietyId("evidence.authored.society."))}, {"subject_ref", SocietyQ(subject)},
                {"kind", "society"}, {"confidence", "documented"},
                {"claim", "User-authored society intent: " + QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact))}};
        }
    }
    bool FoundationService::CreateCultureProfile(const AZStd::string& name, AZStd::string& recordId, AZStd::string* error)
    {
        recordId.clear(); CultureProfile profile; profile.m_recordId = SocietyId("custom.culture.");
        if (!CommitAuthoredSociety(profile, {}, name, true, true, error)) { return false; }
        recordId = profile.m_recordId; return true;
    }
    bool FoundationService::SaveCultureProfile(const CultureProfile& profile, const AZStd::string& name, AZStd::string* error)
    {
        return CommitAuthoredSociety(profile, {}, name, true, false, error);
    }
    bool FoundationService::CreateFactionDefinition(const AZStd::string& name, AZStd::string& recordId, AZStd::string* error)
    {
        recordId.clear(); FactionDefinition definition; definition.m_profile.m_recordId = SocietyId("custom.faction.");
        if (!CommitAuthoredSociety({}, definition, name, false, true, error)) { return false; }
        recordId = definition.m_profile.m_recordId; return true;
    }
    bool FoundationService::SaveFactionDefinition(const FactionDefinition& definition, const AZStd::string& name, AZStd::string* error)
    {
        return CommitAuthoredSociety({}, definition, name, false, false, error);
    }
    bool FoundationService::CommitAuthoredSociety(CultureProfile culture, FactionDefinition faction,
        const AZStd::string& name, bool isCulture, bool creating, AZStd::string* error)
    {
        const auto* profile = m_workspace.FindActiveGameProfile(); const auto* pack = GetActivePack();
        if (!profile || !profile->IsConfigured() || !pack || GetActivePackFilePath().empty()
            || !pack->HasStableIdentity() || !pack->UsesSupportedSchema() || pack->m_runtimeActionsEnabled
            || pack->m_targetBranch != profile->m_branch || (pack->m_targetGameVersion != profile->m_gameVersion
                && AZStd::find(pack->m_compatibleGameVersions.begin(), pack->m_compatibleGameVersions.end(), profile->m_gameVersion)
                    == pack->m_compatibleGameVersions.end()))
        {
            return SocietyFail(error, "Save a compatible authoring mod before creating or editing factions and cultures.");
        }
        const auto trimmedName = SocietyQ(name).trimmed();
        if (trimmedName.isEmpty() || !SocietyPlanningService::IsSingleLine(name, 512, false))
        {
            return SocietyFail(error, "Enter a single-line name up to 512 bytes.");
        }
        const auto& id = isCulture ? culture.m_recordId : faction.m_profile.m_recordId;
        const char* kind = isCulture ? "culture" : "faction";
        const auto* existing = m_catalog.FindByRecordId(id);
        const bool hasProfile = isCulture ? m_catalog.FindCultureProfile(id) != nullptr : m_catalog.FindFactionProfile(id) != nullptr;
        if ((creating && existing) || (!creating && (!existing || !hasProfile || existing->m_domain != "society"
            || existing->m_recordKind != kind || !existing->IsSynthetic() || existing->m_ownerPackId != pack->m_packId)))
        {
            return SocietyFail(error, "Select a faction or culture owned by the active mod; other definitions were not changed.");
        }
        if (isCulture)
        {
            const auto valid = SocietyPlanningService::ValidateCulture(culture);
            if (!valid.IsSuccess()) { if (error) { *error = valid.GetError(); } return false; }
        }
        else
        {
            const auto valid = SocietyPlanningService::Analyze(faction, m_catalog);
            if (!valid.IsValid()) { if (error) { *error = valid.m_errors.front(); } return false; }
        }
        const AZStd::string subject = creating ? "pack:" + pack->m_packId + "/" + id : existing->m_subjectRef;
        QJsonArray rows, linkIds;
        if (!isCulture)
        {
            for (auto& link : faction.m_links)
            {
                const QJsonObject values{{"link_id", SocietyQ(link.m_linkId)}, {"faction_record_id", SocietyQ(id)},
                    {"kind", SocietyQ(link.m_kind)}, {"target_record_id", SocietyQ(link.m_targetRecordId)},
                    {"target_subject_ref", SocietyQ(link.m_targetSubjectRef)}, {"value", SocietyQ(link.m_value)}, {"notes", SocietyQ(link.m_notes)}};
                const auto row = SocietyIntent("faction-link:" + link.m_linkId, values);
                link.m_evidenceIds = {SocietyA(row.value("evidence_id").toString())};
                rows.append(row); linkIds.append(SocietyQ(link.m_linkId));
            }
        }
        const QJsonObject values = isCulture
            ? QJsonObject{{"record_id", SocietyQ(id)}, {"name", trimmedName}, {"description", SocietyQ(culture.m_description)},
                {"language", SocietyQ(culture.m_language)}}
            : QJsonObject{{"record_id", SocietyQ(id)}, {"name", trimmedName}, {"description", SocietyQ(faction.m_profile.m_description)},
                {"culture_record_id", SocietyQ(faction.m_profile.m_cultureRecordId)},
                {"authority_notes", SocietyQ(faction.m_profile.m_authorityNotes)}, {"link_ids", linkIds}};
        const auto row = SocietyIntent(subject, values);
        const auto evidenceId = SocietyA(row.value("evidence_id").toString()); rows.append(row);
        if (isCulture) { culture.m_evidenceIds = {evidenceId}; } else { faction.m_profile.m_evidenceIds = {evidenceId}; }
        SourceEvidenceRegistry registry = m_sourceRegistry; SourceImportResult imported;
        if (!PrepareAuthoredPopulationEvidence(SocietyA(QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact))),
                registry, imported, error)) { return false; }
        CatalogDatabase candidate = m_catalog;
        if (creating)
        {
            CatalogPromotionRequest request;
            request.m_recordId = id; request.m_domain = "society"; request.m_recordKind = kind;
            request.m_subjectRef = subject; request.m_identityKind = "synthetic"; request.m_ownerPackId = pack->m_packId;
            request.m_displayName = SocietyA(trimmedName); request.m_evidenceId = evidenceId;
            request.m_confidence = "documented"; request.m_researchStage = "S1";
            const auto record = m_catalogPromotion.BuildReviewedRecord(request, m_workspace, m_packs, registry);
            if (!record.IsSuccess()) { if (error) { *error = record.GetError(); } return false; }
            if (!candidate.InsertNew(record.GetValue(), error)) { return false; }
        }
        else
        {
            CatalogRecord record = *existing; record.m_displayName = SocietyA(trimmedName);
            if (!candidate.Upsert(record, error)) { return false; }
        }
        if (isCulture)
        {
            if (!candidate.UpsertCultureProfile(culture, error)) { return false; }
        }
        else if (!candidate.ReplaceFactionDefinition(faction, error)) { return false; }
        return CommitPopulationIntake(candidate, AZStd::move(registry), imported, error);
    }
}
