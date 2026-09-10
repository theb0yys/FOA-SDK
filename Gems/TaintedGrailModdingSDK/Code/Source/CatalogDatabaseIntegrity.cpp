/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"
#include "PopulationEvidenceValidation.h"

// The governance/economy implementation exposes explicit private helpers.
// Population-aware public entry points extend those helpers below.
#include "CatalogDatabaseIntegrityBase.inl"

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        void AppendUniquePopulationSubject(
            AZStd::vector<AZStd::string>& subjects,
            const AZStd::string& subject)
        {
            AppendUniquePopulationRequiredSubject(subjects, subject);
        }

        void AppendPopulationRecordEvidenceIds(
            AZStd::vector<AZStd::string>& evidenceIds,
            const CatalogRecord* record)
        {
            if (record)
            {
                AppendUniquePopulationEvidenceIds(
                    evidenceIds,
                    record->m_evidenceIds);
            }
        }

        AZStd::string ResolvePopulationActorSubject(
            const CatalogDatabase& catalog,
            const AZStd::string& actorRecordId,
            const AZStd::string& actorSubjectRef)
        {
            if (!actorRecordId.empty())
            {
                const CatalogRecord* actor = catalog.FindByRecordId(actorRecordId);
                return actor ? actor->m_subjectRef : AZStd::string{};
            }
            return actorSubjectRef;
        }

        const CatalogRecord* ResolvePopulationActorRecord(
            const CatalogDatabase& catalog,
            const AZStd::string& actorRecordId)
        {
            return actorRecordId.empty()
                ? nullptr
                : catalog.FindByRecordId(actorRecordId);
        }

        bool MemberMatchesActorSubject(
            const CatalogDatabase& catalog,
            const PopulationTroopMember& member,
            const AZStd::string& actorSubject)
        {
            return !actorSubject.empty()
                && ResolvePopulationActorSubject(
                    catalog,
                    member.m_actorRecordId,
                    member.m_actorSubjectRef) == actorSubject;
        }
    } // namespace

    bool CatalogDatabase::ReplaceFromBoundDocument(
        const CatalogDocument& document,
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const SourceEvidenceRegistry& sourceRegistry,
        AZStd::string* error)
    {
        const GameProfile* activeProfile = workspace.FindActiveGameProfile();
        if (!activeProfile
            || !SameProfile(*activeProfile, profile)
            || document.m_workspaceId != workspace.m_workspaceId
            || document.m_profileId != profile.m_profileId
            || document.m_gameVersion != profile.m_gameVersion
            || document.m_branch != profile.m_branch)
        {
            SetError(
                error,
                "Catalog document binding does not match the exact active "
                "workspace and game profile.");
            return false;
        }

        CatalogDatabase candidate;
        if (!candidate.ReplaceFromDocument(document, error)
            || !candidate.ValidateIntegrity(
                workspace,
                profile,
                sourceRegistry,
                error))
        {
            return false;
        }
        *this = AZStd::move(candidate);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool CatalogDatabase::ValidateIntegrity(
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const SourceEvidenceRegistry& sourceRegistry,
        AZStd::string* error) const
    {
        if (!ValidateIntegrityWithoutPopulation(
                workspace,
                profile,
                sourceRegistry,
                error))
        {
            return false;
        }

        AZStd::string validationError;
        for (const PopulationActorProfile& actor : m_populationActorProfiles)
        {
            if (!ValidatePopulationActorProfile(actor, &validationError))
            {
                SetError(
                    error,
                    "Population actor profile integrity failed for "
                        + actor.m_recordId + ": " + validationError);
                return false;
            }

            AZStd::vector<AZStd::string> requiredSubjects;
            AZStd::vector<AZStd::string> evidenceIds;
            AppendUniquePopulationEvidenceIds(
                evidenceIds,
                actor.m_evidenceIds);
            const CatalogRecord* actorRecord = FindByRecordId(actor.m_recordId);
            AppendUniquePopulationSubject(
                requiredSubjects,
                actorRecord ? actorRecord->m_subjectRef : AZStd::string{});
            AppendPopulationRecordEvidenceIds(evidenceIds, actorRecord);
            if (!actor.m_templateRecordId.empty())
            {
                const CatalogRecord* templateRecord =
                    FindByRecordId(actor.m_templateRecordId);
                AppendUniquePopulationSubject(
                    requiredSubjects,
                    templateRecord
                        ? templateRecord->m_subjectRef
                        : AZStd::string{});
                AppendPopulationRecordEvidenceIds(evidenceIds, templateRecord);
            }
            AppendUniquePopulationSubject(
                requiredSubjects,
                actor.m_templateSubjectRef);
            if (!ValidatePopulationEvidenceCoverage(
                    evidenceIds,
                    requiredSubjects,
                    profile,
                    sourceRegistry,
                    "Population actor profile",
                    validationError))
            {
                SetError(
                    error,
                    "Population actor evidence integrity failed for "
                        + actor.m_recordId + ": " + validationError);
                return false;
            }
        }

        for (const PopulationTroopProfile& troop : m_populationTroopProfiles)
        {
            if (!ValidatePopulationTroopProfile(troop, &validationError))
            {
                SetError(
                    error,
                    "Population troop profile integrity failed for "
                        + troop.m_recordId + ": " + validationError);
                return false;
            }

            AZStd::vector<AZStd::string> requiredSubjects;
            AZStd::vector<AZStd::string> evidenceIds;
            AppendUniquePopulationEvidenceIds(
                evidenceIds,
                troop.m_evidenceIds);
            const CatalogRecord* troopRecord = FindByRecordId(troop.m_recordId);
            AppendUniquePopulationSubject(
                requiredSubjects,
                troopRecord ? troopRecord->m_subjectRef : AZStd::string{});
            AppendPopulationRecordEvidenceIds(evidenceIds, troopRecord);
            const CatalogRecord* leaderRecord = ResolvePopulationActorRecord(
                *this,
                troop.m_leaderActorRecordId);
            AppendUniquePopulationSubject(
                requiredSubjects,
                ResolvePopulationActorSubject(
                    *this,
                    troop.m_leaderActorRecordId,
                    troop.m_leaderActorSubjectRef));
            AppendPopulationRecordEvidenceIds(evidenceIds, leaderRecord);
            if (!ValidatePopulationEvidenceCoverage(
                    evidenceIds,
                    requiredSubjects,
                    profile,
                    sourceRegistry,
                    "Population troop profile",
                    validationError))
            {
                SetError(
                    error,
                    "Population troop evidence integrity failed for "
                        + troop.m_recordId + ": " + validationError);
                return false;
            }
        }

        for (const PopulationTroopMember& member : m_populationTroopMembers)
        {
            if (!ValidatePopulationTroopMember(member, &validationError))
            {
                SetError(
                    error,
                    "Population troop-member integrity failed for "
                        + member.m_linkId + ": " + validationError);
                return false;
            }
            if (!FindPopulationTroopProfile(member.m_troopRecordId))
            {
                SetError(
                    error,
                    "Population troop-member rows require one typed troop profile: "
                        + member.m_linkId);
                return false;
            }

            AZStd::vector<AZStd::string> requiredSubjects;
            AZStd::vector<AZStd::string> evidenceIds;
            AppendUniquePopulationEvidenceIds(
                evidenceIds,
                member.m_evidenceIds);
            AppendUniquePopulationSubject(
                requiredSubjects,
                "population-troop-member:" + member.m_linkId);
            const CatalogRecord* troopRecord =
                FindByRecordId(member.m_troopRecordId);
            AppendUniquePopulationSubject(
                requiredSubjects,
                troopRecord ? troopRecord->m_subjectRef : AZStd::string{});
            AppendPopulationRecordEvidenceIds(evidenceIds, troopRecord);
            const CatalogRecord* actorRecord = ResolvePopulationActorRecord(
                *this,
                member.m_actorRecordId);
            AppendUniquePopulationSubject(
                requiredSubjects,
                ResolvePopulationActorSubject(
                    *this,
                    member.m_actorRecordId,
                    member.m_actorSubjectRef));
            AppendPopulationRecordEvidenceIds(evidenceIds, actorRecord);
            if (!ValidatePopulationEvidenceCoverage(
                    evidenceIds,
                    requiredSubjects,
                    profile,
                    sourceRegistry,
                    "Population troop member",
                    validationError))
            {
                SetError(
                    error,
                    "Population troop-member evidence integrity failed for "
                        + member.m_linkId + ": " + validationError);
                return false;
            }
        }

        for (const PopulationTroopProfile& troop : m_populationTroopProfiles)
        {
            const AZStd::vector<PopulationTroopMember> members =
                FindPopulationMembersForTroop(troop.m_recordId);
            if (members.empty())
            {
                SetError(
                    error,
                    "Population troop profiles require at least one typed member: "
                        + troop.m_recordId);
                return false;
            }

            AZ::u64 requiredMinimum = 0;
            AZ::u64 totalMaximum = 0;
            size_t leaderRowCount = 0;
            const PopulationTroopMember* matchingLeader = nullptr;
            const AZStd::string leaderSubject = ResolvePopulationActorSubject(
                *this,
                troop.m_leaderActorRecordId,
                troop.m_leaderActorSubjectRef);
            for (const PopulationTroopMember& member : members)
            {
                if (member.m_required)
                {
                    requiredMinimum += member.m_minimumCount;
                }
                totalMaximum += member.m_maximumCount;
                if (member.m_role == "leader")
                {
                    ++leaderRowCount;
                    if (MemberMatchesActorSubject(
                            *this,
                            member,
                            leaderSubject))
                    {
                        matchingLeader = &member;
                    }
                }
            }

            if (requiredMinimum > troop.m_maximumSize
                || totalMaximum < troop.m_minimumSize)
            {
                SetError(
                    error,
                    "Required population troop member ranges do not overlap the "
                    "declared troop size range: " + troop.m_recordId);
                return false;
            }
            if (!leaderSubject.empty()
                && (leaderRowCount != 1
                    || !matchingLeader
                    || !matchingLeader->m_required
                    || matchingLeader->m_minimumCount == 0))
            {
                SetError(
                    error,
                    "A troop leader binding requires exactly one matching, required "
                    "leader membership row with a positive minimum count: "
                        + troop.m_recordId);
                return false;
            }
        }

        if (!ValidateEncounterIntegrity(profile, sourceRegistry, error)) { return false; }
        if (!ValidateWorldIntegrity(profile, sourceRegistry, error)) { return false; }
        if (!ValidateQuestIntegrity(profile, sourceRegistry, error)) { return false; }
        if (!ValidateSocietyIntegrity(profile, sourceRegistry, error)) { return false; }

        if (error)
        {
            error->clear();
        }
        return true;
    }
} // namespace TaintedGrailModdingSDK
