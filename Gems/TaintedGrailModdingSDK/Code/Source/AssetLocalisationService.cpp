/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetLocalisationService.h"
#include "CatalogDatabase.h"
#include "CanonicalFingerprint.h"
#include "ResearchContractValidation.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/sort.h>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        void Add(AZStd::string& bytes, const AZStd::string& value)
        { bytes += AZStd::string::format("%zu:", value.size()); bytes += value; }
        AZ::Outcome<void, AZStd::string> Fail(const char* text) { return AZ::Failure(AZStd::string(text)); }
        bool Fingerprint(const AZStd::string& value)
        {
            return value.size() == 71 && value.substr(0, 7) == "sha256:"
                && AZStd::all_of(value.begin() + 7, value.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
        }
    }
    bool AssetLocalisationService::IsText(const AZStd::string& text, size_t maximum, bool multiline)
    {
        if (text.empty() || text.size() > maximum) { return false; }
        bool visible = false;
        for (size_t i = 0; i < text.size(); ++i)
        {
            const auto c = static_cast<unsigned char>(text[i]);
            if (c < 32 && !(multiline && (c == '\n' || c == '\t' || c == '\r'))) { return false; }
            if (c == 127) { return false; }
            visible |= c > 32;
            if (c < 128) { continue; }
            const size_t trailing = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 0;
            if (!trailing || i + trailing >= text.size()) { return false; }
            unsigned int scalar = c & (0x7f >> trailing);
            for (size_t j = 0; j < trailing; ++j)
            {
                const auto next = static_cast<unsigned char>(text[++i]);
                if ((next & 0xc0) != 0x80) { return false; }
                scalar = (scalar << 6) | (next & 0x3f);
            }
            if ((trailing == 1 && scalar < 0x80) || (trailing == 2 && scalar < 0x800) || (trailing == 3 && scalar < 0x10000)
                || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) { return false; }
        }
        return visible;
    }
    bool AssetLocalisationService::IsLanguage(const AZStd::string& value)
    {
        if (value.size() < 2 || value.size() > 16) { return false; }
        size_t letters = 0;
        for (char c : value)
        {
            if (c == '-') { if (letters < 2) { return false; } letters = 0; }
            else if (c >= 'a' && c <= 'z') { ++letters; }
            else { return false; }
        }
        return letters >= 2;
    }
    AZStd::string AssetLocalisationService::ImagePath(const AZStd::string& owner, const AZStd::string& fingerprint, const AZStd::string& mediaType)
    {
        if (!IsStableContractId(owner) || !Fingerprint(fingerprint) || (mediaType != "image/png" && mediaType != "image/jpeg")) { return {}; }
        return "Media/Owned/" + CalculateCanonicalSha256(owner).substr(7) + "/" + fingerprint.substr(7)
            + (mediaType == "image/png" ? ".png" : ".jpg");
    }
    AZStd::string AssetLocalisationService::BindingId(const AZStd::string& owner, const AZStd::string& target, const AZStd::string& slot)
    {
        AZStd::string bytes; Add(bytes, owner); Add(bytes, target); Add(bytes, slot);
        return "presentation." + CalculateCanonicalSha256(bytes).substr(7);
    }
    AZStd::string AssetLocalisationService::Revision(const ProjectAssetProfile& p, const AZStd::string& name)
    {
        AZStd::string bytes; for (const auto* value : {&p.m_recordId, &name, &p.m_sourcePath, &p.m_fingerprint, &p.m_mediaType,
            &p.m_provenance, &p.m_sourceRights, &p.m_licence, &p.m_redistribution}) { Add(bytes, *value); }
        Add(bytes, AZStd::string::format("%llu/%u/%u", static_cast<unsigned long long>(p.m_byteSize), p.m_width, p.m_height));
        return CalculateCanonicalSha256(bytes);
    }
    LocalisationEntry AssetLocalisationService::CanonicalEntry(LocalisationEntry entry)
    {
        AZStd::sort(entry.m_variants.begin(), entry.m_variants.end(), [](const auto& a, const auto& b) { return a.m_language < b.m_language; });
        AZStd::sort(entry.m_evidenceIds.begin(), entry.m_evidenceIds.end()); return entry;
    }
    AZStd::string AssetLocalisationService::Revision(const LocalisationEntry& entry)
    {
        const auto p = CanonicalEntry(entry);
        AZStd::string bytes; Add(bytes, p.m_recordId); Add(bytes, p.m_key); Add(bytes, p.m_defaultLanguage);
        for (const auto& v : p.m_variants) { Add(bytes, v.m_language); Add(bytes, v.m_text); }
        return CalculateCanonicalSha256(bytes);
    }
    AZStd::string AssetLocalisationService::Revision(const PresentationBinding& p)
    {
        AZStd::string bytes; for (const auto* value : {&p.m_bindingId, &p.m_ownerPackId, &p.m_targetRecordId, &p.m_slot, &p.m_valueRecordId}) { Add(bytes, *value); }
        return CalculateCanonicalSha256(bytes);
    }
    AZ::Outcome<void, AZStd::string> AssetLocalisationService::ValidateAsset(const ProjectAssetProfile& p, const AZStd::string& owner)
    {
        if (!IsStableContractId(p.m_recordId) || !IsStableContractId(owner) || !Fingerprint(p.m_fingerprint)
            || p.m_sourcePath.empty() || p.m_sourcePath != ImagePath(owner, p.m_fingerprint, p.m_mediaType))
        { return Fail("Image identity, fingerprint or managed workspace path is invalid."); }
        if (!p.m_byteSize || p.m_byteSize > MaximumImageBytes || !p.m_width || !p.m_height || p.m_width > 4096 || p.m_height > 4096
            || static_cast<AZ::u64>(p.m_width) * p.m_height > MaximumImagePixels)
        { return Fail("Images must be at most 8 MiB, 4096 pixels per side and 4,194,304 pixels total."); }
        if (!IsText(p.m_provenance, 2048) || (p.m_sourceRights != "original_work" && p.m_sourceRights != "licensed")
            || (p.m_sourceRights == "licensed" && !IsText(p.m_licence, 2048))
            || (!p.m_licence.empty() && !IsText(p.m_licence, 2048))
            || (p.m_redistribution != "not_reviewed" && p.m_redistribution != "declared_permitted" && p.m_redistribution != "prohibited"))
        { return Fail("Describe the image source and select source rights; licensed work also requires licence details."); }
        return AZ::Success();
    }
    AZ::Outcome<void, AZStd::string> AssetLocalisationService::ValidateEntry(const LocalisationEntry& entry)
    {
        if (!IsStableContractId(entry.m_recordId) || entry.m_key.empty() || entry.m_key.size() > 256
            || !AZStd::all_of(entry.m_key.begin(), entry.m_key.end(), [](char c)
                { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'; }) || !IsLanguage(entry.m_defaultLanguage)
            || entry.m_variants.empty() || entry.m_variants.size() > 32)
        { return Fail("Use a stable text key, a default language, and 1-32 language variants."); }
        AZStd::unordered_set<AZStd::string> languages; bool hasDefault = false;
        for (const auto& variant : entry.m_variants)
        {
            if (!IsLanguage(variant.m_language) || !languages.insert(variant.m_language).second || !IsText(variant.m_text, 8192, true))
            { return Fail("Languages must be unique lowercase identifiers; each translation needs text up to 8192 UTF-8 bytes."); }
            hasDefault |= variant.m_language == entry.m_defaultLanguage;
        }
        if (!hasDefault) { return Fail("Add a translation for the default language before saving."); }
        return AZ::Success();
    }
    AZ::Outcome<void, AZStd::string> AssetLocalisationService::ValidateBinding(const PresentationBinding& b, const CatalogDatabase& catalog)
    {
        if (!IsStableContractId(b.m_ownerPackId) || b.m_bindingId != BindingId(b.m_ownerPackId, b.m_targetRecordId, b.m_slot))
        { return Fail("Presentation assignment identity does not match its mod, target and slot."); }
        const auto* target = catalog.FindByRecordId(b.m_targetRecordId);
        if (!target || (!target->IsSynthetic() && target->m_identityKind != "native")
            || (target->IsSynthetic() && target->m_ownerPackId != b.m_ownerPackId))
        { return Fail("Choose a native target or a definition owned by this mod."); }
        const bool item = target->m_domain == "economy" && target->m_recordKind == "item";
        const bool actor = target->m_domain == "population" && target->m_recordKind == "actor";
        const bool quest = target->m_domain == "narrative" && target->m_recordKind == "quest";
        const bool image = b.m_slot == "icon" || b.m_slot == "portrait";
        if ((b.m_slot == "icon" && !item) || (b.m_slot == "portrait" && !actor)
            || (!image && (b.m_slot != "name" && b.m_slot != "description")) || (!item && !actor && !quest))
        { return Fail("Use item icons, actor portraits, or item/actor/quest name and description text."); }
        if (b.m_valueRecordId.empty()) { return AZ::Success(); }
        const auto* value = catalog.FindByRecordId(b.m_valueRecordId);
        if (!value || !value->IsSynthetic() || value->m_ownerPackId != b.m_ownerPackId
            || (image ? catalog.FindProjectAsset(b.m_valueRecordId) == nullptr : catalog.FindLocalisationEntry(b.m_valueRecordId) == nullptr))
        { return Fail("Choose an image or translation entry owned by the same mod and matching the selected slot."); }
        return AZ::Success();
    }
    LocalisedText AssetLocalisationService::Resolve(const LocalisationEntry& entry, const AZStd::string& language)
    {
        LocalisedText result; const auto valid = ValidateEntry(entry);
        if (!valid.IsSuccess()) { result.m_error = valid.GetError(); return result; }
        if (!IsLanguage(language)) { result.m_error = "Select a valid language."; return result; }
        const LocalisationVariant* fallback = nullptr;
        for (const auto& v : entry.m_variants)
        {
            if (v.m_language == language) { result.m_language = language; result.m_text = v.m_text; return result; }
            if (v.m_language == entry.m_defaultLanguage) { fallback = &v; }
        }
        if (fallback) { result.m_language = fallback->m_language; result.m_text = fallback->m_text; result.m_usedFallback = true; }
        else { result.m_error = "Default translation is missing."; }
        return result;
    }
}
