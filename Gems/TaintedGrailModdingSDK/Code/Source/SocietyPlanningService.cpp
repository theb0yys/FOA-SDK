/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "SocietyPlanningService.h"
#include "CatalogDatabase.h"
#include "ResearchContractValidation.h"
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/sort.h>

namespace TaintedGrailModdingSDK
{
    bool SocietyPlanningService::IsSingleLine(const AZStd::string& text, size_t maximumBytes, bool emptyAllowed)
    {
        if (text.size() > maximumBytes || (!emptyAllowed && text.empty())) { return false; }
        for (unsigned char c : text) { if (c < 0x20 || c == 0x7f) { return false; } }
        return true;
    }
    AZ::Outcome<void, AZStd::string> SocietyPlanningService::ValidateCulture(const CultureProfile& profile)
    {
        if (!IsStableContractId(profile.m_recordId) || !IsSingleLine(profile.m_description, 2048)
            || !IsSingleLine(profile.m_language, 128))
        {
            return AZ::Failure(AZStd::string("Culture requires a stable ID, a single-line description up to 2048 bytes and language up to 128 bytes."));
        }
        return AZ::Success();
    }
    AZ::Outcome<void, AZStd::string> SocietyPlanningService::ValidateFactionProfile(
        const FactionProfile& profile, const CatalogDatabase& catalog)
    {
        if (!IsStableContractId(profile.m_recordId) || !IsSingleLine(profile.m_description, 2048)
            || !IsSingleLine(profile.m_authorityNotes, 2048))
        {
            return AZ::Failure(AZStd::string("Faction requires a stable ID and single-line description/authority notes up to 2048 bytes."));
        }
        if (!profile.m_cultureRecordId.empty())
        {
            const auto* record = catalog.FindByRecordId(profile.m_cultureRecordId);
            if (!record || record->m_domain != "society" || record->m_recordKind != "culture"
                || !catalog.FindCultureProfile(profile.m_cultureRecordId))
            {
                return AZ::Failure(AZStd::string("Choose an existing saved culture."));
            }
        }
        return AZ::Success();
    }
    FactionAnalysis SocietyPlanningService::Analyze(const FactionDefinition& definition, const CatalogDatabase& catalog)
    {
        FactionAnalysis result;
        const auto intrinsic = ValidateFactionProfile(definition.m_profile, catalog);
        if (!intrinsic.IsSuccess()) { result.m_errors.push_back(intrinsic.GetError()); }
        if (definition.m_links.size() > 384)
        {
            result.m_errors.push_back("A faction supports at most 128 memberships, 128 relationships and 128 jurisdiction plans.");
            return result;
        }
        AZStd::unordered_set<AZStd::string> ids, members, dispositions, jurisdictions;
        AZ::u32 leaders = 0;
        for (const auto& link : definition.m_links)
        {
            auto error = [&result](const char* text) { result.m_errors.push_back(text); };
            if (!IsStableContractId(link.m_linkId) || link.m_factionRecordId != definition.m_profile.m_recordId
                || !ids.insert(link.m_linkId).second)
            {
                error("Every faction link needs a distinct stable ID and the exact owning faction."); continue;
            }
            if (!IsSingleLine(link.m_notes, 1024) || !IsSingleLine(link.m_targetSubjectRef, 1024))
            {
                error("Link notes and jurisdiction references must be single-line text up to 1024 bytes."); continue;
            }
            const auto* target = catalog.FindByRecordId(link.m_targetRecordId);
            if (!link.m_targetRecordId.empty() && (!IsStableContractId(link.m_targetRecordId) || !target))
            {
                error("A selected faction link target is missing from the catalog."); continue;
            }
            if (target && !link.m_targetSubjectRef.empty() && link.m_targetSubjectRef != target->m_subjectRef)
            {
                error("The link target and its exact subject disagree."); continue;
            }
            if (link.m_kind == "member")
            {
                ++result.m_members;
                if (!target || target->m_domain != "population"
                    || (target->m_recordKind != "actor" && target->m_recordKind != "troop")
                    || (target->m_recordKind == "actor" && !catalog.FindPopulationActorProfile(target->m_recordId))
                    || (target->m_recordKind == "troop" && !catalog.FindPopulationTroopProfile(target->m_recordId)))
                {
                    error("Membership requires a saved actor or troop profile."); continue;
                }
                if (!members.insert(target->m_recordId).second) { error("An actor or troop can appear only once in this faction."); }
                if (link.m_value != "member" && link.m_value != "officer" && link.m_value != "leader")
                {
                    error("Choose member, officer or leader for membership.");
                }
                if (link.m_value == "leader")
                {
                    ++leaders; result.m_leaderName = target->m_displayName;
                    if (target->m_recordKind != "actor") { error("The faction leader must be an individual actor."); }
                }
            }
            else if (link.m_kind == "disposition")
            {
                ++result.m_dispositions;
                if (!target || target->m_domain != "society" || target->m_recordKind != "faction"
                    || !catalog.FindFactionProfile(target->m_recordId))
                {
                    error("A relationship requires another saved faction."); continue;
                }
                if (target->m_recordId == definition.m_profile.m_recordId) { error("A faction cannot declare a relationship with itself."); }
                if (!dispositions.insert(target->m_recordId).second) { error("Only one directed relationship is allowed per target faction."); }
                if (link.m_value != "friendly" && link.m_value != "neutral" && link.m_value != "hostile")
                {
                    error("Choose friendly, neutral or hostile for the relationship.");
                }
            }
            else if (link.m_kind == "jurisdiction")
            {
                ++result.m_jurisdictions;
                if (target && (target->m_domain != "world" || (target->m_recordKind != "location"
                    && target->m_recordKind != "scene" && target->m_recordKind != "region")))
                {
                    error("Jurisdiction must reference a world location, scene or region."); continue;
                }
                if (!target && !IsSingleLine(link.m_targetSubjectRef, 1024, false))
                {
                    error("Choose a world location or enter an unverified territory reference."); continue;
                }
                const auto key = target ? "record:" + target->m_recordId : "reference:" + link.m_targetSubjectRef;
                if (!jurisdictions.insert(key).second) { error("Only one jurisdiction plan is allowed per territory reference."); }
                if (link.m_value != "controls" && link.m_value != "claims" && link.m_value != "protects")
                {
                    error("Choose controls, claims or protects for jurisdiction.");
                }
                if (!target) { result.m_warnings.push_back("Unverified territory reference: " + link.m_targetSubjectRef); }
            }
            else { error("Unsupported faction link kind."); }
            if (target && target->IsBlocked()) { result.m_warnings.push_back("Open catalog blockers for " + target->m_displayName + "."); }
        }
        if (result.m_members > 128 || result.m_dispositions > 128 || result.m_jurisdictions > 128)
        {
            result.m_errors.push_back("Each faction link category is limited to 128 entries.");
        }
        if (leaders > 1) { result.m_errors.push_back("A faction may have only one declared leader."); }
        if (!result.m_members) { result.m_warnings.push_back("This faction has no members yet."); }
        if (!leaders) { result.m_warnings.push_back("No faction leader is assigned."); }
        if (!result.m_jurisdictions) { result.m_warnings.push_back("No jurisdiction is assigned."); }
        const auto* culture = catalog.FindByRecordId(definition.m_profile.m_cultureRecordId);
        if (culture && culture->IsBlocked()) { result.m_warnings.push_back("The selected culture has open catalog blockers."); }
        AZStd::sort(result.m_errors.begin(), result.m_errors.end());
        AZStd::sort(result.m_warnings.begin(), result.m_warnings.end());
        return result;
    }
}
