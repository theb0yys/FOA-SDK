/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

namespace TaintedGrailModdingSDK
{
    bool CatalogDatabase::ValidatePopulationActorProfile(
        const PopulationActorProfile& profile,
        AZStd::string* error) const
    {
        if (!IsStableContractId(profile.m_recordId)
            || !IsPopulationRecord(FindByRecordId(profile.m_recordId), "actor"))
        {
            SetPopulationError(
                error,
                "Population actor profiles require one existing canonical "
                "population/actor record with stable identity.");
            return false;
        }
        if (!ParsePopulationActorKind(profile.m_actorKind).IsSuccess()
            || !IsBoundedPopulationText(profile.m_archetype, 256, false)
            || !HasValidOptionalRange(
                profile.m_minimumLevel,
                profile.m_maximumLevel,
                MaximumPopulationLevel))
        {
            SetPopulationError(
                error,
                "Population actor profiles require a supported kind, bounded "
                "archetype, and either no level range or an ordered range up to 1000.");
            return false;
        }

        const bool hasTemplateRecord = !profile.m_templateRecordId.empty();
        const bool hasTemplateSubject = !profile.m_templateSubjectRef.empty();
        const CatalogRecord* templateRecord = hasTemplateRecord
            ? FindByRecordId(profile.m_templateRecordId)
            : nullptr;
        if ((hasTemplateRecord
                && (!IsStableContractId(profile.m_templateRecordId)
                    || !IsPopulationTemplateRecord(templateRecord)))
            || (hasTemplateSubject
                && !IsBoundedPopulationText(
                    profile.m_templateSubjectRef,
                    1024,
                    false))
            || (templateRecord
                && hasTemplateSubject
                && templateRecord->m_subjectRef != profile.m_templateSubjectRef))
        {
            SetPopulationError(
                error,
                "Actor template bindings require an exact population template "
                "record or bounded exact unresolved subject, and dual bindings "
                "must agree.");
            return false;
        }

        if (!IsBoundedPopulationText(profile.m_localisationNameRef, 1024, true)
            || !IsBoundedPopulationText(
                profile.m_localisationDescriptionRef,
                1024,
                true)
            || !IsBoundedPopulationText(profile.m_portraitAssetRef, 1024, true)
            || !IsBoundedPopulationText(profile.m_modelAssetRef, 1024, true)
            || !HasUniqueBoundedPopulationValues(profile.m_tags, 128)
            || !HasUniquePopulationEvidenceIds(profile.m_evidenceIds))
        {
            SetPopulationError(
                error,
                "Actor references must be bounded, tags must be unique, and "
                "evidence identities must be non-empty, stable, and unique.");
            return false;
        }
        return true;
    }

    bool CatalogDatabase::ValidatePopulationTroopProfile(
        const PopulationTroopProfile& profile,
        AZStd::string* error) const
    {
        if (!IsStableContractId(profile.m_recordId)
            || !IsPopulationRecord(FindByRecordId(profile.m_recordId), "troop"))
        {
            SetPopulationError(
                error,
                "Population troop profiles require one existing canonical "
                "population/troop record with stable identity.");
            return false;
        }
        if (!ParsePopulationTroopKind(profile.m_troopKind).IsSuccess()
            || profile.m_minimumSize == 0
            || profile.m_minimumSize > profile.m_maximumSize
            || profile.m_maximumSize > MaximumPopulationCount
            || !IsBoundedPopulationText(profile.m_formation, 256, true))
        {
            SetPopulationError(
                error,
                "Population troop profiles require a supported kind, bounded "
                "formation, and an ordered positive size range up to 1000.");
            return false;
        }

        const bool hasLeaderRecord = !profile.m_leaderActorRecordId.empty();
        const bool hasLeaderSubject = !profile.m_leaderActorSubjectRef.empty();
        const CatalogRecord* leaderRecord = hasLeaderRecord
            ? FindByRecordId(profile.m_leaderActorRecordId)
            : nullptr;
        if ((hasLeaderRecord
                && (!IsStableContractId(profile.m_leaderActorRecordId)
                    || !IsPopulationRecord(leaderRecord, "actor")))
            || (hasLeaderSubject
                && !IsBoundedPopulationText(
                    profile.m_leaderActorSubjectRef,
                    1024,
                    false))
            || (leaderRecord
                && hasLeaderSubject
                && leaderRecord->m_subjectRef != profile.m_leaderActorSubjectRef))
        {
            SetPopulationError(
                error,
                "Troop leader bindings require an exact population actor or "
                "bounded exact unresolved subject, and dual bindings must agree.");
            return false;
        }

        if (!HasUniqueBoundedPopulationValues(profile.m_tags, 128)
            || !HasUniquePopulationEvidenceIds(profile.m_evidenceIds))
        {
            SetPopulationError(
                error,
                "Troop tags must be bounded and unique, and evidence identities "
                "must be non-empty, stable, and unique.");
            return false;
        }
        return true;
    }
} // namespace TaintedGrailModdingSDK
