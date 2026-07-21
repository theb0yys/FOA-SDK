/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PopulationAuthoringService.h"

#include "PathPolicyService.h"
#include "PopulationEvidenceValidation.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value)
                != values.end();
        }

        void AddUnique(
            AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            if (!value.empty() && !Contains(values, value))
            {
                values.push_back(value);
            }
        }

        bool SameProfile(
            const GameProfile& left,
            const GameProfile& right)
        {
            return left.m_profileId == right.m_profileId
                && left.m_gameVersion == right.m_gameVersion
                && left.m_branch == right.m_branch
                && left.m_runtimeTarget == right.m_runtimeTarget;
        }

        bool PackTargetsProfile(
            const PackManifest& pack,
            const GameProfile& profile)
        {
            if (pack.m_targetGameVersion.empty()
                || pack.m_targetBranch.empty())
            {
                return false;
            }
            const bool gameVersionMatches =
                pack.m_targetGameVersion == profile.m_gameVersion
                || Contains(
                    pack.m_compatibleGameVersions,
                    profile.m_gameVersion);
            return gameVersionMatches
                && pack.m_targetBranch == profile.m_branch;
        }

        bool ValidateAuthoringContext(
            const AZStd::string& workspaceRoot,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const PackManifest& activePack,
            AZStd::string& error)
        {
            const GameProfile* workspaceProfile =
                workspace.FindActiveGameProfile();
            if (!IsStableContractId(workspace.m_workspaceId)
                || workspaceRoot.empty()
                || !profile.IsConfigured()
                || !workspaceProfile
                || !SameProfile(*workspaceProfile, profile))
            {
                error = "Population authoring requires the exact configured "
                    "active workspace and game profile.";
                return false;
            }

            PathPolicyService pathPolicy;
            const AZ::Outcome<void, AZStd::string> pathValidation =
                pathPolicy.ValidateWorkspacePaths(workspace, workspaceRoot);
            if (!pathValidation.IsSuccess())
            {
                error = "Population authoring requires the path-policy-validated "
                    "canonical workspace root: "
                    + AZStd::string(pathValidation.GetError());
                return false;
            }

            if (!activePack.HasStableIdentity()
                || !activePack.UsesSupportedSchema()
                || activePack.m_runtimeActionsEnabled
                || !PackTargetsProfile(activePack, profile))
            {
                error = "Population authoring requires one stable, editor-owned "
                    "active pack compatible with the exact game profile.";
                return false;
            }
            return true;
        }

        bool ValidateAuthoredRecord(
            const CatalogRecord* record,
            const char* expectedKind,
            const PackManifest& activePack,
            AZStd::string& error)
        {
            if (!record
                || record->m_domain != "population"
                || record->m_recordKind != expectedKind)
            {
                error = "Population authoring requires an existing canonical "
                    "population/" + AZStd::string(expectedKind) + " record.";
                return false;
            }
            if (!record->m_ownerPackId.empty()
                && record->m_ownerPackId != activePack.m_packId)
            {
                error = "The authored population record belongs to a different "
                    "pack than the active authoring pack: " + record->m_recordId;
                return false;
            }
            return true;
        }

        void AddRecordEvidence(
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

        bool EvidenceIsCompleteAndBound(
            const EvidenceRecord& evidence,
            const SourceRecord& source,
            const GameProfile& profile)
        {
            return PopulationEvidenceIsCompleteAndBound(
                evidence,
                source,
                profile);
        }

        bool ValidateEvidence(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::vector<AZStd::string>& requiredSubjects,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const char* authoredArea,
            AZStd::string& error)
        {
            for (const AZStd::string& evidenceId : evidenceIds)
            {
                const EvidenceRecord* evidence =
                    sourceRegistry.FindEvidence(evidenceId);
                const SourceRecord* source = evidence
                    ? sourceRegistry.FindSource(evidence->m_sourceId)
                    : nullptr;
                if (!evidence
                    || !source
                    || !EvidenceIsCompleteAndBound(
                        *evidence,
                        *source,
                        profile))
                {
                    error = AZStd::string(authoredArea)
                        + " evidence is missing, incomplete, or bound to a "
                          "different profile: "
                        + evidenceId;
                    return false;
                }
            }
            return ValidatePopulationEvidenceCoverage(
                evidenceIds,
                requiredSubjects,
                profile,
                sourceRegistry,
                authoredArea,
                error);
        }

        AZStd::string ResolveActorSubject(
            const AZStd::string& actorRecordId,
            const AZStd::string& actorSubjectRef,
            const CatalogDatabase& catalog)
        {
            if (const CatalogRecord* actor =
                    catalog.FindByRecordId(actorRecordId))
            {
                return actor->m_subjectRef;
            }
            return actorSubjectRef;
        }

        const CatalogRecord* ResolveActorRecord(
            const AZStd::string& actorRecordId,
            const CatalogDatabase& catalog)
        {
            return actorRecordId.empty()
                ? nullptr
                : catalog.FindByRecordId(actorRecordId);
        }

        AZStd::vector<AZStd::string> ActorEvidenceSubjects(
            const PopulationActorProfile& actor,
            const CatalogDatabase& catalog)
        {
            AZStd::vector<AZStd::string> subjects;
            if (const CatalogRecord* record =
                    catalog.FindByRecordId(actor.m_recordId))
            {
                AddUnique(subjects, record->m_subjectRef);
            }
            if (const CatalogRecord* templateRecord =
                    catalog.FindByRecordId(actor.m_templateRecordId))
            {
                AddUnique(subjects, templateRecord->m_subjectRef);
            }
            AddUnique(subjects, actor.m_templateSubjectRef);
            return subjects;
        }

        AZStd::vector<AZStd::string> ActorEvidenceIds(
            const PopulationActorProfile& actor,
            const CatalogDatabase& catalog)
        {
            AZStd::vector<AZStd::string> evidenceIds;
            AppendUniquePopulationEvidenceIds(
                evidenceIds,
                actor.m_evidenceIds);
            AddRecordEvidence(
                evidenceIds,
                catalog.FindByRecordId(actor.m_recordId));
            AddRecordEvidence(
                evidenceIds,
                catalog.FindByRecordId(actor.m_templateRecordId));
            return evidenceIds;
        }

        AZStd::vector<AZStd::string> TroopEvidenceSubjects(
            const PopulationTroopProfile& troop,
            const CatalogDatabase& catalog)
        {
            AZStd::vector<AZStd::string> subjects;
            if (const CatalogRecord* record =
                    catalog.FindByRecordId(troop.m_recordId))
            {
                AddUnique(subjects, record->m_subjectRef);
            }
            AddUnique(
                subjects,
                ResolveActorSubject(
                    troop.m_leaderActorRecordId,
                    troop.m_leaderActorSubjectRef,
                    catalog));
            return subjects;
        }

        AZStd::vector<AZStd::string> TroopEvidenceIds(
            const PopulationTroopProfile& troop,
            const CatalogDatabase& catalog)
        {
            AZStd::vector<AZStd::string> evidenceIds;
            AppendUniquePopulationEvidenceIds(
                evidenceIds,
                troop.m_evidenceIds);
            AddRecordEvidence(
                evidenceIds,
                catalog.FindByRecordId(troop.m_recordId));
            AddRecordEvidence(
                evidenceIds,
                ResolveActorRecord(
                    troop.m_leaderActorRecordId,
                    catalog));
            return evidenceIds;
        }

        AZStd::vector<AZStd::string> MemberEvidenceSubjects(
            const PopulationTroopMember& member,
            const CatalogDatabase& catalog)
        {
            AZStd::vector<AZStd::string> subjects;
            AddUnique(
                subjects,
                "population-troop-member:" + member.m_linkId);
            if (const CatalogRecord* troop =
                    catalog.FindByRecordId(member.m_troopRecordId))
            {
                AddUnique(subjects, troop->m_subjectRef);
            }
            AddUnique(
                subjects,
                ResolveActorSubject(
                    member.m_actorRecordId,
                    member.m_actorSubjectRef,
                    catalog));
            return subjects;
        }

        AZStd::vector<AZStd::string> MemberEvidenceIds(
            const PopulationTroopMember& member,
            const CatalogDatabase& catalog)
        {
            AZStd::vector<AZStd::string> evidenceIds;
            AppendUniquePopulationEvidenceIds(
                evidenceIds,
                member.m_evidenceIds);
            AddRecordEvidence(
                evidenceIds,
                catalog.FindByRecordId(member.m_troopRecordId));
            AddRecordEvidence(
                evidenceIds,
                ResolveActorRecord(member.m_actorRecordId, catalog));
            return evidenceIds;
        }

        bool ValidateMemberLinkOwnership(
            const PopulationTroopMember& member,
            const CatalogDatabase& catalog,
            AZStd::string& error)
        {
            for (const PopulationTroopMember& existing :
                 catalog.GetPopulationTroopMembers())
            {
                if (existing.m_linkId == member.m_linkId
                    && existing.m_troopRecordId != member.m_troopRecordId)
                {
                    error = "Population troop-member link identity cannot move "
                        "between owning troops: " + member.m_linkId;
                    return false;
                }
            }
            return true;
        }

        AZ::Outcome<CatalogDatabase, AZStd::string> ValidateCandidate(
            CatalogDatabase candidate,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry)
        {
            AZStd::string integrityError;
            if (!candidate.ValidateIntegrity(
                    workspace,
                    profile,
                    sourceRegistry,
                    &integrityError))
            {
                return AZ::Failure(
                    AZStd::string(
                        "Population authoring candidate integrity failed: ")
                    + integrityError);
            }
            return AZ::Success(AZStd::move(candidate));
        }
    } // namespace

    AZ::Outcome<CatalogDatabase, AZStd::string>
    PopulationAuthoringService::BuildActorProfileCandidate(
        const PopulationActorProfile& actor,
        const AZStd::string& workspaceRoot,
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const PackManifest& activePack,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog) const
    {
        AZStd::string validationError;
        if (!ValidateAuthoringContext(
                workspaceRoot,
                workspace,
                profile,
                activePack,
                validationError)
            || !ValidateAuthoredRecord(
                catalog.FindByRecordId(actor.m_recordId),
                "actor",
                activePack,
                validationError)
            || !ValidateEvidence(
                ActorEvidenceIds(actor, catalog),
                ActorEvidenceSubjects(actor, catalog),
                profile,
                sourceRegistry,
                "Actor profile",
                validationError))
        {
            return AZ::Failure(AZStd::move(validationError));
        }

        CatalogDatabase candidate = catalog;
        AZStd::string catalogError;
        if (!candidate.UpsertPopulationActorProfile(actor, &catalogError))
        {
            return AZ::Failure(AZStd::move(catalogError));
        }
        return ValidateCandidate(
            AZStd::move(candidate),
            workspace,
            profile,
            sourceRegistry);
    }

    AZ::Outcome<CatalogDatabase, AZStd::string>
    PopulationAuthoringService::BuildTroopDefinitionCandidate(
        const PopulationTroopDefinition& definition,
        const AZStd::string& workspaceRoot,
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const PackManifest& activePack,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog) const
    {
        AZStd::string validationError;
        if (!ValidateAuthoringContext(
                workspaceRoot,
                workspace,
                profile,
                activePack,
                validationError)
            || !ValidateAuthoredRecord(
                catalog.FindByRecordId(definition.m_profile.m_recordId),
                "troop",
                activePack,
                validationError))
        {
            return AZ::Failure(AZStd::move(validationError));
        }
        if (definition.m_members.empty())
        {
            return AZ::Failure(AZStd::string(
                "A troop definition requires at least one typed member row."));
        }

        AZStd::vector<AZStd::string> memberIds;
        memberIds.reserve(definition.m_members.size());
        for (const PopulationTroopMember& member : definition.m_members)
        {
            if (member.m_troopRecordId
                != definition.m_profile.m_recordId)
            {
                return AZ::Failure(AZStd::string(
                    "Every troop-definition member must bind to the exact troop "
                    "profile record."));
            }
            if (!ValidateMemberLinkOwnership(
                    member,
                    catalog,
                    validationError))
            {
                return AZ::Failure(AZStd::move(validationError));
            }
            memberIds.push_back(member.m_linkId);
        }
        AZStd::sort(memberIds.begin(), memberIds.end());
        if (AZStd::adjacent_find(memberIds.begin(), memberIds.end())
            != memberIds.end())
        {
            return AZ::Failure(AZStd::string(
                "A troop definition cannot contain duplicate member-link IDs."));
        }

        AZStd::vector<AZStd::string> troopEvidenceIds =
            definition.m_profile.m_evidenceIds;
        AppendUniquePopulationEvidenceIds(
            troopEvidenceIds,
            TroopEvidenceIds(definition.m_profile, catalog));
        if (!ValidateEvidence(
                troopEvidenceIds,
                TroopEvidenceSubjects(definition.m_profile, catalog),
                profile,
                sourceRegistry,
                "Troop profile",
                validationError))
        {
            return AZ::Failure(AZStd::move(validationError));
        }
        for (const PopulationTroopMember& member : definition.m_members)
        {
            AZStd::vector<AZStd::string> memberEvidenceIds =
                member.m_evidenceIds;
            AppendUniquePopulationEvidenceIds(
                memberEvidenceIds,
                MemberEvidenceIds(member, catalog));
            if (!ValidateEvidence(
                    memberEvidenceIds,
                    MemberEvidenceSubjects(member, catalog),
                    profile,
                    sourceRegistry,
                    "Troop member",
                    validationError))
            {
                return AZ::Failure(AZStd::move(validationError));
            }
        }

        CatalogDocument document = catalog.BuildDocument(workspace, profile);
        bool replacedProfile = false;
        for (PopulationTroopProfile& existing : document.m_troopProfiles)
        {
            if (existing.m_recordId == definition.m_profile.m_recordId)
            {
                existing = definition.m_profile;
                replacedProfile = true;
                break;
            }
        }
        if (!replacedProfile)
        {
            document.m_troopProfiles.push_back(definition.m_profile);
        }

        for (const PopulationTroopMember& member : definition.m_members)
        {
            bool replacedMember = false;
            for (PopulationTroopMember& existing : document.m_troopMembers)
            {
                if (existing.m_linkId == member.m_linkId)
                {
                    existing = member;
                    replacedMember = true;
                    break;
                }
            }
            if (!replacedMember)
            {
                document.m_troopMembers.push_back(member);
            }
        }

        CatalogDatabase candidate;
        AZStd::string catalogError;
        if (!candidate.ReplaceFromBoundDocument(
                document,
                workspace,
                profile,
                sourceRegistry,
                &catalogError))
        {
            return AZ::Failure(
                AZStd::string("Population troop definition rejected: ")
                + catalogError);
        }
        return AZ::Success(AZStd::move(candidate));
    }

    AZ::Outcome<CatalogDatabase, AZStd::string>
    PopulationAuthoringService::BuildTroopProfileCandidate(
        const PopulationTroopProfile& troop,
        const AZStd::string& workspaceRoot,
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const PackManifest& activePack,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog) const
    {
        PopulationTroopDefinition definition;
        definition.m_profile = troop;
        definition.m_members =
            catalog.FindPopulationMembersForTroop(troop.m_recordId);
        if (definition.m_members.empty())
        {
            return AZ::Failure(AZStd::string(
                "A new troop must be authored atomically with its first member "
                "through the troop-definition command."));
        }
        return BuildTroopDefinitionCandidate(
            definition,
            workspaceRoot,
            workspace,
            profile,
            activePack,
            sourceRegistry,
            catalog);
    }

    AZ::Outcome<CatalogDatabase, AZStd::string>
    PopulationAuthoringService::BuildTroopMemberCandidate(
        const PopulationTroopMember& member,
        const AZStd::string& workspaceRoot,
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const PackManifest& activePack,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog) const
    {
        AZStd::string validationError;
        if (!ValidateAuthoringContext(
                workspaceRoot,
                workspace,
                profile,
                activePack,
                validationError)
            || !ValidateAuthoredRecord(
                catalog.FindByRecordId(member.m_troopRecordId),
                "troop",
                activePack,
                validationError)
            || !ValidateMemberLinkOwnership(
                member,
                catalog,
                validationError)
            || !ValidateEvidence(
                MemberEvidenceIds(member, catalog),
                MemberEvidenceSubjects(member, catalog),
                profile,
                sourceRegistry,
                "Troop member",
                validationError))
        {
            return AZ::Failure(AZStd::move(validationError));
        }

        CatalogDatabase candidate = catalog;
        AZStd::string catalogError;
        if (!candidate.UpsertPopulationTroopMember(member, &catalogError))
        {
            return AZ::Failure(AZStd::move(catalogError));
        }
        return ValidateCandidate(
            AZStd::move(candidate),
            workspace,
            profile,
            sourceRegistry);
    }
} // namespace TaintedGrailModdingSDK
