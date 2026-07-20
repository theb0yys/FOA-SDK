/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

namespace TaintedGrailModdingSDK
{
    bool CatalogDatabase::ValidatePopulationTroopMember(
        const PopulationTroopMember& member,
        AZStd::string* error) const
    {
        if (!IsStableContractId(member.m_linkId)
            || !IsStableContractId(member.m_troopRecordId)
            || !IsPopulationRecord(
                FindByRecordId(member.m_troopRecordId),
                "troop"))
        {
            SetPopulationError(
                error,
                "Population troop members require stable membership identity "
                "and one existing canonical population/troop owner.");
            return false;
        }

        const bool hasActorRecord = !member.m_actorRecordId.empty();
        const bool hasActorSubject = !member.m_actorSubjectRef.empty();
        const CatalogRecord* actorRecord = hasActorRecord
            ? FindByRecordId(member.m_actorRecordId)
            : nullptr;
        if ((!hasActorRecord && !hasActorSubject)
            || (hasActorRecord
                && (!IsStableContractId(member.m_actorRecordId)
                    || !IsPopulationRecord(actorRecord, "actor")))
            || (hasActorSubject
                && !IsBoundedPopulationText(
                    member.m_actorSubjectRef,
                    1024,
                    false))
            || (actorRecord
                && hasActorSubject
                && actorRecord->m_subjectRef != member.m_actorSubjectRef))
        {
            SetPopulationError(
                error,
                "Troop membership requires a resolved population actor, an exact "
                "unresolved actor subject, or two agreeing forms of the same binding.");
            return false;
        }

        if (!ParsePopulationTroopMemberRole(member.m_role).IsSuccess()
            || member.m_minimumCount == 0
            || member.m_minimumCount > member.m_maximumCount
            || member.m_maximumCount > MaximumPopulationCount
            || !IsFiniteNonNegative(member.m_weight)
            || !HasUniqueBoundedPopulationValues(member.m_conditions, 256)
            || !HasUniquePopulationEvidenceIds(member.m_evidenceIds))
        {
            SetPopulationError(
                error,
                "Troop membership requires a supported role, ordered positive "
                "counts up to 1000, finite non-negative weight, unique conditions, "
                "and stable unique evidence identities.");
            return false;
        }

        const AZStd::string memberSubject = ResolveActorSubject(
            *this,
            member.m_actorRecordId,
            member.m_actorSubjectRef);
        for (const PopulationTroopMember& existing : m_populationTroopMembers)
        {
            if (existing.m_linkId == member.m_linkId
                || existing.m_troopRecordId != member.m_troopRecordId)
            {
                continue;
            }
            const AZStd::string existingSubject = ResolveActorSubject(
                *this,
                existing.m_actorRecordId,
                existing.m_actorSubjectRef);
            if (!memberSubject.empty() && memberSubject == existingSubject)
            {
                SetPopulationError(
                    error,
                    "A troop cannot contain duplicate membership rows for the "
                    "same exact actor subject.");
                return false;
            }
            if (member.m_role == "leader" && existing.m_role == "leader")
            {
                SetPopulationError(
                    error,
                    "A troop can contain at most one typed leader membership row.");
                return false;
            }
        }
        return true;
    }
} // namespace TaintedGrailModdingSDK
