/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"
#include "AssetLocalisationService.h"
#include "PopulationEvidenceValidation.h"
#include "ResearchContractValidation.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/sort.h>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool AssetFail(AZStd::string* error, const AZStd::string& message) { if (error) { *error = message; } return false; }
        bool IntentIds(const AZStd::vector<AZStd::string>& ids)
        {
            if (ids.empty() || ids.size() > 64) { return false; }
            AZStd::unordered_set<AZStd::string> unique;
            for (const auto& id : ids) { if (!IsStableContractId(id) || !unique.insert(id).second) { return false; } }
            return true;
        }
        template<class T> const T* FindAssetValue(const AZStd::vector<T>& values, const AZStd::string& id)
        {
            const auto it = AZStd::lower_bound(values.begin(), values.end(), id,
                [](const T& v, const AZStd::string& key) { return v.m_recordId < key; });
            return it != values.end() && it->m_recordId == id ? &*it : nullptr;
        }
        template<class T> void UpsertAssetValue(AZStd::vector<T>& values, T value)
        {
            AZStd::sort(value.m_evidenceIds.begin(), value.m_evidenceIds.end());
            auto it = AZStd::lower_bound(values.begin(), values.end(), value.m_recordId,
                [](const T& v, const AZStd::string& key) { return v.m_recordId < key; });
            if (it != values.end() && it->m_recordId == value.m_recordId) { *it = AZStd::move(value); }
            else { values.insert(it, AZStd::move(value)); }
        }
        bool IntentMatches(const AZStd::vector<AZStd::string>& ids, const AZStd::string& subject,
            const AZStd::string& revision, const SourceEvidenceRegistry& registry, const GameProfile& profile, AZStd::string* error)
        {
            AZStd::string message;
            if (!ValidatePopulationEvidenceCoverage(ids, {subject}, profile, registry, "Asset/localisation intent", message))
            { return AssetFail(error, message); }
            for (const auto& id : ids)
            {
                const auto* e = registry.FindEvidence(id);
                if (e && e->m_subjectRef == subject && e->m_claim == "User-authored presentation revision " + revision) { return true; }
            }
            return AssetFail(error, "Asset/localisation intent does not describe the current revision.");
        }
    }
    const AZStd::vector<ProjectAssetProfile>& CatalogDatabase::GetProjectAssets() const { return m_projectAssets; }
    const AZStd::vector<LocalisationEntry>& CatalogDatabase::GetLocalisationEntries() const { return m_localisationEntries; }
    const AZStd::vector<PresentationBinding>& CatalogDatabase::GetPresentationBindings() const { return m_presentationBindings; }
    const ProjectAssetProfile* CatalogDatabase::FindProjectAsset(const AZStd::string& id) const { return FindAssetValue(m_projectAssets, id); }
    const LocalisationEntry* CatalogDatabase::FindLocalisationEntry(const AZStd::string& id) const { return FindAssetValue(m_localisationEntries, id); }
    const PresentationBinding* CatalogDatabase::FindPresentationBinding(
        const AZStd::string& owner, const AZStd::string& target, const AZStd::string& slot) const
    {
        const auto id = AssetLocalisationService::BindingId(owner, target, slot);
        const auto it = AZStd::lower_bound(m_presentationBindings.begin(), m_presentationBindings.end(), id,
            [](const auto& b, const auto& key) { return b.m_bindingId < key; });
        return it != m_presentationBindings.end() && it->m_bindingId == id ? &*it : nullptr;
    }
    bool CatalogDatabase::UpsertProjectAsset(const ProjectAssetProfile& p, AZStd::string* error)
    {
        const auto* record = FindByRecordId(p.m_recordId);
        if (!record || !record->IsSynthetic() || record->m_domain != "assets" || record->m_recordKind != "image"
            || !IntentIds(p.m_evidenceIds) || !AssetLocalisationService::IsText(record->m_displayName, 512))
        { return AssetFail(error, "Images require a named pack-owned asset record and distinct intent evidence."); }
        const auto valid = AssetLocalisationService::ValidateAsset(p, record->m_ownerPackId);
        if (!valid.IsSuccess()) { return AssetFail(error, valid.GetError()); }
        if (!FindProjectAsset(p.m_recordId) && m_projectAssets.size() >= 4096)
        { return AssetFail(error, "The catalog supports at most 4096 project images."); }
        UpsertAssetValue(m_projectAssets, p); return true;
    }
    bool CatalogDatabase::UpsertLocalisationEntry(const LocalisationEntry& p, AZStd::string* error)
    {
        const auto* record = FindByRecordId(p.m_recordId);
        if (!record || !record->IsSynthetic() || record->m_domain != "localisation" || record->m_recordKind != "text"
            || record->m_displayName != p.m_key || !IntentIds(p.m_evidenceIds))
        { return AssetFail(error, "Translations require a pack-owned text record named by its key and distinct intent evidence."); }
        const auto valid = AssetLocalisationService::ValidateEntry(p);
        if (!valid.IsSuccess()) { return AssetFail(error, valid.GetError()); }
        size_t bytes = 0;
        for (const auto& v : p.m_variants) { bytes += v.m_text.size(); }
        for (const auto& entry : m_localisationEntries)
        {
            if (entry.m_recordId == p.m_recordId) { continue; }
            const auto* other = FindByRecordId(entry.m_recordId);
            if (entry.m_key == p.m_key && other && other->m_ownerPackId == record->m_ownerPackId)
            { return AssetFail(error, "This mod already contains that text key. Choose a different key or edit its existing entry."); }
            for (const auto& v : entry.m_variants) { bytes += v.m_text.size(); }
        }
        if (bytes > 16 * 1024 * 1024 || (!FindLocalisationEntry(p.m_recordId) && m_localisationEntries.size() >= 8192))
        { return AssetFail(error, "Translation catalog limit reached (8192 entries / 16 MiB of text)."); }
        UpsertAssetValue(m_localisationEntries, AssetLocalisationService::CanonicalEntry(p)); return true;
    }
    bool CatalogDatabase::UpsertPresentationBinding(const PresentationBinding& p, AZStd::string* error)
    {
        const auto valid = AssetLocalisationService::ValidateBinding(p, *this);
        if (!valid.IsSuccess()) { return AssetFail(error, valid.GetError()); }
        if (!IntentIds(p.m_evidenceIds)) { return AssetFail(error, "Assignments require distinct intent evidence."); }
        auto it = AZStd::lower_bound(m_presentationBindings.begin(), m_presentationBindings.end(), p.m_bindingId,
            [](const auto& b, const auto& id) { return b.m_bindingId < id; });
        auto canonical = p; AZStd::sort(canonical.m_evidenceIds.begin(), canonical.m_evidenceIds.end());
        if (it != m_presentationBindings.end() && it->m_bindingId == p.m_bindingId) { *it = AZStd::move(canonical); }
        else
        {
            if (m_presentationBindings.size() >= 32768) { return AssetFail(error, "The catalog supports at most 32768 presentation assignments."); }
            m_presentationBindings.insert(it, AZStd::move(canonical));
        }
        return true;
    }
    bool CatalogDatabase::LoadAssetLocalisationCollections(const CatalogDocument& document, AZStd::string* error)
    {
        if ((document.m_schemaVersion < AssetLocalisationCatalogSchemaVersion && (!document.m_projectAssets.empty()
            || !document.m_localisationEntries.empty() || !document.m_presentationBindings.empty()))
            || document.m_projectAssets.size() > 4096 || document.m_localisationEntries.size() > 8192 || document.m_presentationBindings.size() > 32768)
        { return AssetFail(error, "Asset/localisation collections require schema 7 and bounded collection sizes."); }
        AZStd::unordered_set<AZStd::string> ids;
        for (const auto& p : document.m_projectAssets)
        {
            if (!ids.insert(p.m_recordId).second) { return AssetFail(error, "Duplicate image record identity."); }
            if (!UpsertProjectAsset(p, error)) { return false; }
        }
        for (const auto& p : document.m_localisationEntries)
        {
            if (!ids.insert(p.m_recordId).second) { return AssetFail(error, "Duplicate translation record identity."); }
            if (!UpsertLocalisationEntry(p, error)) { return false; }
        }
        ids.clear();
        for (const auto& p : document.m_presentationBindings)
        {
            if (!ids.insert(p.m_bindingId).second) { return AssetFail(error, "Duplicate presentation assignment identity."); }
            if (!UpsertPresentationBinding(p, error)) { return false; }
        }
        return true;
    }
    bool CatalogDatabase::ValidateAssetLocalisationIntegrity(
        const GameProfile& profile, const SourceEvidenceRegistry& registry, AZStd::string* error) const
    {
        for (const auto& p : m_projectAssets)
        {
            const auto* record = FindByRecordId(p.m_recordId);
            if (!record || !record->IsSynthetic() || record->m_domain != "assets" || record->m_recordKind != "image")
            { return AssetFail(error, "Image record ownership or kind changed."); }
            const auto valid = AssetLocalisationService::ValidateAsset(p, record->m_ownerPackId);
            if (!valid.IsSuccess()) { return AssetFail(error, valid.GetError()); }
            if (!IntentMatches(p.m_evidenceIds, record->m_subjectRef, AssetLocalisationService::Revision(p, record->m_displayName), registry, profile, error)) { return false; }
        }
        AZStd::unordered_set<AZStd::string> keys;
        for (const auto& p : m_localisationEntries)
        {
            const auto* record = FindByRecordId(p.m_recordId);
            if (!record || !record->IsSynthetic() || record->m_domain != "localisation" || record->m_recordKind != "text"
                || record->m_displayName != p.m_key || !keys.insert(record->m_ownerPackId + "/" + p.m_key).second)
            { return AssetFail(error, "Translation ownership, text key or canonical record changed."); }
            const auto valid = AssetLocalisationService::ValidateEntry(p);
            if (!valid.IsSuccess()) { return AssetFail(error, valid.GetError()); }
            if (!IntentMatches(p.m_evidenceIds, record->m_subjectRef, AssetLocalisationService::Revision(p), registry, profile, error)) { return false; }
        }
        for (const auto& b : m_presentationBindings)
        {
            const auto valid = AssetLocalisationService::ValidateBinding(b, *this);
            if (!valid.IsSuccess()) { return AssetFail(error, valid.GetError()); }
            if (!IntentMatches(b.m_evidenceIds, "presentation:" + b.m_bindingId, AssetLocalisationService::Revision(b), registry, profile, error)) { return false; }
            AZStd::vector<AZStd::string> subjects, evidence;
            for (const auto* id : {&b.m_targetRecordId, &b.m_valueRecordId})
            {
                if (id->empty()) { continue; }
                const auto* r = FindByRecordId(*id);
                if (!r) { return AssetFail(error, "Presentation assignment refers to a missing record."); }
                AppendUniquePopulationRequiredSubject(subjects, r->m_subjectRef);
                AppendUniquePopulationEvidenceIds(evidence, r->m_evidenceIds);
            }
            AZStd::string message;
            if (!ValidatePopulationEvidenceCoverage(evidence, subjects, profile, registry, "Presentation references", message))
            { return AssetFail(error, message); }
        }
        return true;
    }
}
