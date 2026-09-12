/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"
#include "AssetLocalisationService.h"
#include "ProjectImageService.h"
#include <AzCore/std/algorithm.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QUuid>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString PQ(const AZStd::string& value) { return QString::fromUtf8(value.data(), static_cast<int>(value.size())); }
        AZStd::string PA(const QString& value) { const auto bytes = value.toUtf8(); return {bytes.constData(), static_cast<size_t>(bytes.size())}; }
        AZStd::string PresentationId(const char* prefix)
        { return PA(QString::fromUtf8(prefix) + QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-')); }
        bool PresentationError(AZStd::string* error, const AZStd::string& message)
        { if (error) { *error = message; } return false; }
    }
    bool FoundationService::CanAuthorPresentation(AZStd::string* error) const
    {
        const auto* pack = GetActivePack(); const auto* profile = m_workspace.FindActiveGameProfile();
        if (!pack || !profile || !profile->IsConfigured() || m_workspaceFilePath.empty() || m_workspaceRootPath.empty()
            || GetActivePackFilePath().empty() || !pack->HasStableIdentity() || !pack->UsesSupportedSchema() || pack->m_runtimeActionsEnabled
            || pack->m_targetBranch != profile->m_branch || (pack->m_targetGameVersion != profile->m_gameVersion
                && AZStd::find(pack->m_compatibleGameVersions.begin(), pack->m_compatibleGameVersions.end(), profile->m_gameVersion) == pack->m_compatibleGameVersions.end()))
        { return PresentationError(error, "Choose and save a compatible authoring mod before editing assets or translations."); }
        return true;
    }
    bool FoundationService::SaveProjectAsset(ProjectAssetProfile asset, const AZStd::string& name,
        const AZStd::string& sourceFile, const AZStd::string& expectedRevision, AZStd::string& id, AZStd::string* error)
    {
        id.clear(); if (!CanAuthorPresentation(error)) { return false; }
        const auto* pack = GetActivePack(); const bool creating = asset.m_recordId.empty();
        const auto* old = m_catalog.FindProjectAsset(asset.m_recordId); const auto* record = m_catalog.FindByRecordId(asset.m_recordId);
        if ((!creating && (!old || !record || !record->IsSynthetic() || record->m_ownerPackId != pack->m_packId
                || expectedRevision.empty() || AssetLocalisationService::Revision(*old, record->m_displayName) != expectedRevision))
            || (creating && !expectedRevision.empty()))
        { return PresentationError(error, "This image or active mod changed. Revert and reopen the latest image before saving."); }
        const auto label = PA(PQ(name).trimmed());
        if (!AssetLocalisationService::IsText(label, 512)) { return PresentationError(error, "Enter an image name up to 512 UTF-8 bytes."); }
        if (creating && sourceFile.empty()) { return PresentationError(error, "Choose an image before saving."); }
        auto image = sourceFile.empty()
            ? ProjectImageService::ReadManaged(*old, pack->m_packId, m_workspace, m_workspaceRootPath)
            : ProjectImageService::ReadSource(sourceFile, m_workspace, m_workspaceRootPath);
        if (!image.IsSuccess()) { return PresentationError(error, image.GetError()); }
        if (creating) { asset.m_recordId = PresentationId("asset.custom."); }
        const auto& metadata = image.GetValue().m_metadata;
        asset.m_fingerprint = metadata.m_fingerprint; asset.m_byteSize = metadata.m_byteSize;
        asset.m_width = metadata.m_width; asset.m_height = metadata.m_height; asset.m_mediaType = metadata.m_mediaType;
        asset.m_sourcePath = AssetLocalisationService::ImagePath(pack->m_packId, asset.m_fingerprint, asset.m_mediaType);
        const auto valid = AssetLocalisationService::ValidateAsset(asset, pack->m_packId);
        if (!valid.IsSuccess()) { return PresentationError(error, valid.GetError()); }
        const auto stored = ProjectImageService::Store(image.GetValue(), pack->m_packId, m_workspace, m_workspaceRootPath);
        if (!stored.IsSuccess()) { return PresentationError(error, stored.GetError()); }
        if (!CommitPresentationEdit(asset, {}, {}, label, 0, error)) { return false; }
        id = asset.m_recordId; return true;
    }
    bool FoundationService::SaveLocalisationEntry(LocalisationEntry entry, const AZStd::string& expectedRevision,
        AZStd::string& id, AZStd::string* error)
    {
        id.clear(); if (!CanAuthorPresentation(error)) { return false; }
        const auto* pack = GetActivePack(); const bool creating = entry.m_recordId.empty();
        const auto* old = m_catalog.FindLocalisationEntry(entry.m_recordId); const auto* record = m_catalog.FindByRecordId(entry.m_recordId);
        if ((!creating && (!old || !record || !record->IsSynthetic() || record->m_ownerPackId != pack->m_packId
                || expectedRevision.empty() || AssetLocalisationService::Revision(*old) != expectedRevision)) || (creating && !expectedRevision.empty()))
        { return PresentationError(error, "This translation or active mod changed. Revert and reopen the latest entry before saving."); }
        if (creating) { entry.m_recordId = PresentationId("text.custom."); }
        const auto valid = AssetLocalisationService::ValidateEntry(entry);
        if (!valid.IsSuccess()) { return PresentationError(error, valid.GetError()); }
        if (!CommitPresentationEdit({}, entry, {}, entry.m_key, 1, error)) { return false; }
        id = entry.m_recordId; return true;
    }
    bool FoundationService::SavePresentationBinding(const AZStd::string& target, const AZStd::string& slot,
        const AZStd::string& value, const AZStd::string& expectedRevision, AZStd::string* error)
    {
        if (!CanAuthorPresentation(error)) { return false; }
        PresentationBinding binding; binding.m_ownerPackId = GetActivePack()->m_packId;
        binding.m_targetRecordId = target; binding.m_slot = slot; binding.m_valueRecordId = value;
        binding.m_bindingId = AssetLocalisationService::BindingId(binding.m_ownerPackId, target, slot);
        const auto* old = m_catalog.FindPresentationBinding(binding.m_ownerPackId, target, slot);
        if ((old && (expectedRevision.empty() || AssetLocalisationService::Revision(*old) != expectedRevision))
            || (!old && !expectedRevision.empty()))
        { return PresentationError(error, "This assignment changed. Select the target again before saving."); }
        const auto valid = AssetLocalisationService::ValidateBinding(binding, m_catalog);
        if (!valid.IsSuccess()) { return PresentationError(error, valid.GetError()); }
        if (!value.empty() && (slot == "icon" || slot == "portrait"))
        {
            const auto* asset = m_catalog.FindProjectAsset(value);
            auto read = ProjectImageService::ReadManaged(*asset, binding.m_ownerPackId, m_workspace, m_workspaceRootPath);
            if (!read.IsSuccess()) { return PresentationError(error, read.GetError()); }
        }
        return CommitPresentationEdit({}, {}, binding, {}, 2, error);
    }
    bool FoundationService::ReadProjectAssetImage(const AZStd::string& id, QImage& image, AZStd::string* error) const
    {
        image = {};
        const auto* asset = m_catalog.FindProjectAsset(id); const auto* record = m_catalog.FindByRecordId(id); const auto* pack = GetActivePack();
        if (!asset || !record || !pack || record->m_ownerPackId != pack->m_packId)
        { return PresentationError(error, "Select an image owned by the active mod."); }
        auto read = ProjectImageService::ReadManaged(*asset, pack->m_packId, m_workspace, m_workspaceRootPath);
        if (!read.IsSuccess()) { return PresentationError(error, read.GetError()); }
        image = read.GetValue().m_image; if (error) { error->clear(); } return true;
    }
    bool FoundationService::CommitPresentationEdit(ProjectAssetProfile asset, LocalisationEntry entry,
        PresentationBinding binding, const AZStd::string& name, int kind, AZStd::string* error)
    {
        const auto* pack = GetActivePack();
        const auto id = kind == 0 ? asset.m_recordId : entry.m_recordId;
        const auto* existing = kind == 2 ? nullptr : m_catalog.FindByRecordId(id);
        const auto subject = kind == 2 ? "presentation:" + binding.m_bindingId
            : existing ? existing->m_subjectRef : "pack:" + pack->m_packId + "/" + id;
        const auto revision = kind == 0 ? AssetLocalisationService::Revision(asset, name)
            : kind == 1 ? AssetLocalisationService::Revision(entry) : AssetLocalisationService::Revision(binding);
        const auto evidenceId = PresentationId("evidence.authored.presentation.");
        const QJsonArray rows{QJsonObject{{"evidence_id", PQ(evidenceId)}, {"subject_ref", PQ(subject)}, {"kind", "presentation"},
            {"confidence", "documented"}, {"claim", "User-authored presentation revision " + PQ(revision)}}};
        SourceEvidenceRegistry registry = m_sourceRegistry; SourceImportResult imported;
        if (!PrepareAuthoredPopulationEvidence(PA(QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact))), registry, imported, error)) { return false; }
        CatalogDatabase candidate = m_catalog;
        if (kind != 2)
        {
            if (!existing)
            {
                CatalogPromotionRequest request; request.m_recordId = id; request.m_ownerPackId = pack->m_packId;
                request.m_domain = kind == 0 ? "assets" : "localisation"; request.m_recordKind = kind == 0 ? "image" : "text";
                request.m_subjectRef = subject; request.m_identityKind = "synthetic"; request.m_displayName = name;
                request.m_evidenceId = evidenceId; request.m_confidence = "documented"; request.m_researchStage = "S1";
                const auto promoted = m_catalogPromotion.BuildReviewedRecord(request, m_workspace, m_packs, registry);
                if (!promoted.IsSuccess()) { return PresentationError(error, promoted.GetError()); }
                if (!candidate.InsertNew(promoted.GetValue(), error)) { return false; }
            }
            else
            {
                CatalogRecord record = *existing; record.m_displayName = name;
                if (!candidate.Upsert(record, error)) { return false; }
            }
        }
        asset.m_evidenceIds = {evidenceId}; entry.m_evidenceIds = {evidenceId}; binding.m_evidenceIds = {evidenceId};
        if (kind == 0 && !candidate.UpsertProjectAsset(asset, error)) { return false; }
        if (kind == 1 && !candidate.UpsertLocalisationEntry(entry, error)) { return false; }
        if (kind == 2 && !candidate.UpsertPresentationBinding(binding, error)) { return false; }
        return CommitPopulationIntake(candidate, AZStd::move(registry), imported, error);
    }
}
