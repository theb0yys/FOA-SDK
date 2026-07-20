/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"

// Preserve the established governance/economy integrity implementation while
// extending its bound-document and whole-catalog entry points with population
// checks. The renamed methods remain private implementation details.
#define ReplaceFromBoundDocument ReplaceFromBoundDocumentWithoutPopulation
#define ValidateIntegrity ValidateIntegrityWithoutPopulation
#include "CatalogDatabaseIntegrityBase.inl"
#undef ValidateIntegrity
#undef ReplaceFromBoundDocument

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        void AppendUniquePopulationSubject(
            AZStd::vector<AZStd::string>& subjects,
            const AZStd::string& subject)
        {
            if (!subject.empty()
                && AZStd::find(subjects.begin(), subjects.end(), subject)
                    == subjects.end())
            {
                subjects.push_back(subject);
            }
        }

        bool ValidateEvidenceForAnyPopulationSubject(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::vector<AZStd::string>& allowedSubjects,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            AZStd::string& error)
        {
            if (allowedSubjects.empty())
            {
                error = "Population evidence validation requires at least one exact subject.";
                return false;
            }
            if (!HasUniqueStableIds(evidenceIds, false, error))
            {
                return false;
            }

            for (const AZStd::string& evidenceId : evidenceIds)
            {
                bool matched = false;
                AZStd::vector<AZStd::string> singleEvidence{ evidenceId };
                for (const AZStd::string& subject : allowedSubjects)
                {
                    AZStd::string subjectError;
                    if (ValidateEvidenceForSubject(
                            singleEvidence,
                            subject,
                            profile,
                            sourceRegistry,
                            false,
                            subjectError))
                    {
                        matched = true;
                        break;
                    }
                }
                if (!matched)
                {
                    error = "Population evidence does not prove any exact subject "
                        "represented by the authored row: " + evidenceId;
                    return false;
                }
            }
            return true;
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

            AZStd::vector<AZStd::string> allowedSubjects;
            const CatalogRecord* actorRecord = FindByRecordId(actor.m_recordId);
            AppendUniquePopulationSubject(
                allowedSubjects,
                actorRecord ? actorRecord->m_subjectRef : AZStd::string{});
            if (!actor.m_templateRecordId.empty())
            {
                const CatalogRecord* templateRecord =
                    FindByRecordId(actor.m_templateRecordId);
                AppendUniquePopulationSubject(
                    allowedSubjects,
                    templateRecord
                        ? templateRecord->m_subjectRef
                        : AZStd::string{});
            }
            AppendUniquePopulationSubject(
                allowedSubjects,
                actor.m_templateSubjectRef);
            if (!ValidateEvidenceForAnyPopulationSubject(
                    actor.m_evidenceIds,
                    allowedSubjects,
                    profile,
                    sourceRegistry,
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

            AZStd::vector<AZStd::string> allowedSubjects;
            const CatalogRecord* troopRecord = FindByRecordId(troop.m_recordId);
            AppendUniquePopulationSubject(
                allowedSubjects,
                troopRecord ? troopRecord->m_subjectRef : AZStd::string{});
            AppendUniquePopulationSubject(
                allowedSubjects,
                ResolvePopulationActorSubject(
                    *this,
                    troop.m_leaderActorRecordId,
                    troop.m_leaderActorSubjectRef));
            if (!ValidateEvidenceForAnyPopulationSubject(
                    troop.m_evidenceIds,
                    allowedSubjects,
                    profile,
                    sourceRegistry,
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

            AZStd::vector<AZStd::string> allowedSubjects;
            AppendUniquePopulationSubject(
                allowedSubjects,
                "population-troop-member:" + member.m_linkId);
            const CatalogRecord* troopRecord =
                FindByRecordId(member.m_troopRecordId);
            AppendUniquePopulationSubject(
                allowedSubjects,
                troopRecord ? troopRecord->m_subjectRef : AZStd::string{});
            AppendUniquePopulationSubject(
                allowedSubjects,
                ResolvePopulationActorSubject(
                    *this,
                    member.m_actorRecordId,
                    member.m_actorSubjectRef));
            if (!ValidateEvidenceForAnyPopulationSubject(
                    member.m_evidenceIds,
                    allowedSubjects,
                    profile,
                    sourceRegistry,
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

            AZ::u64 totalMinimum = 0;
            AZ::u64 totalMaximum = 0;
            size_t leaderRowCount = 0;
            const PopulationTroopMember* matchingLeader = nullptr;
            const AZStd::string leaderSubject = ResolvePopulationActorSubject(
                *this,
                troop.m_leaderActorRecordId,
                troop.m_leaderActorSubjectRef);
            for (const PopulationTroopMember& member : members)
            {
                totalMinimum += member.m_minimumCount;
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

            if (totalMinimum > troop.m_maximumSize
                || totalMaximum < troop.m_minimumSize)
            {
                SetError(
                    error,
                    "Population troop member ranges do not overlap the declared "
                    "troop size range: " + troop.m_recordId);
                return false;
            }
            if (!leaderSubject.empty()
                && (leaderRowCount != 1 || !matchingLeader))
            {
                SetError(
                    error,
                    "A troop leader binding requires exactly one matching leader "
                    "membership row: " + troop.m_recordId);
                return false;
            }
        }

        if (error)
        {
            error->clear();
        }
        return true;
    }
} // namespace TaintedGrailModdingSDK
