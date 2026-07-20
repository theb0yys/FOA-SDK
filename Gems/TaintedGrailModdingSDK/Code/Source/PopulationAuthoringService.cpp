/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PopulationAuthoringService.h"

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
            const bool gameVersionMatches =
                (pack.m_targetGameVersion.empty()
                    && pack.m_compatibleGameVersions.empty())
                || pack.m_targetGameVersion == profile.m_gameVersion
                || Contains(
                    pack.m_compatibleGameVersions,
                    profile.m_gameVersion);
            return gameVersionMatches
                && (pack.m_targetBranch.empty()
                    || pack.m_targetBranch == profile.m_branch);
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
            if (workspace.m_workspaceId.empty()
                || workspaceRoot.empty()
                || !profile.IsConfigured()
                || !workspaceProfile
                || !SameProfile(*workspaceProfile, profile))
            {
                error = "Population authoring requires the exact configured "
                    "active workspace and game profile.";
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

        bool EvidenceIsCompleteAndBound(
            const EvidenceRecord& evidence,
            const SourceRecord& source,
            const GameProfile& profile,
            const AZStd::vector<AZStd::string>& allowedSubjects)
        {
            return Contains(allowedSubjects, evidence.m_subjectRef)
                && !evidence.m_claim.empty()
                && !evidence.m_evidenceKind.empty()
                && !evidence.m_confidence.empty()
                && !evidence.m_locator.empty()
                && !evidence.m_recordPath.empty()
                && IsSha256Fingerprint(evidence.m_sourceFingerprint)
                && IsStrictUtcTimestamp(evidence.m_extractedAt)
                && IsSha256Fingerprint(source.m_fingerprint)
                && IsStrictUtcTimestamp(source.m_capturedAt)
                && IsStrictUtcTimestamp(source.m_importedAt)
                && IsUsableImportStatus(source.m_importStatus)
                && evidence.m_sourceId == source.m_sourceId
                && evidence.m_sourceFingerprint == source.m_fingerprint
                && evidence.m_profileId == profile.m_profileId
                && evidence.m_gameVersion == profile.m_gameVersion
                && evidence.m_branch == profile.m_branch
                && source.m_profileId == profile.m_profileId
                && source.m_gameVersion == profile.m_gameVersion
                && source.m_branch == profile.m_branch
                && source.m_runtimeTarget == profile.m_runtimeTarget
                && source.m_capturedAt <= evidence.m_extractedAt
                && evidence.m_extractedAt <= source.m_importedAt;
        }

        bool ValidateEvidence(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::vector<AZStd::string>& allowedSubjects,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const char* authoredArea,
            AZStd::string& error)
        {
            if (evidenceIds.empty() || allowedSubjects.empty())
            {
                error = AZStd::string(authoredArea)
                    + " authoring requires exact-subject evidence.";
                return false;
            }

            AZStd::vector<AZStd::string> sortedEvidenceIds = evidenceIds;
            AZStd::sort(sortedEvidenceIds.begin(), sortedEvidenceIds.end());
            if (AZStd::adjacent_find(
                    sortedEvidenceIds.begin(),
                    sortedEvidenceIds.end()) != sortedEvidenceIds.end())
            {
                error = AZStd::string(authoredArea)
                    + " authoring evidence IDs must be unique.";
                return false;
            }

            for (const AZStd::string& evidenceId : sortedEvidenceIds)
            {
                if (!IsStableContractId(evidenceId))
                {
                    error = AZStd::string(authoredArea)
                        + " authoring evidence IDs must be stable bounded identities.";
                    return false;
                }
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
                        profile,
                        allowedSubjects))
                {
                    error = AZStd::string(authoredArea)
                        + " evidence does not prove an allowed exact subject in "
                        "the active profile: " + evidenceId;
                    return false;
                }
            }
            return true;
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
                actor.m_evidenceIds,
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
            memberIds.push_back(member.m_linkId);
        }
        AZStd::sort(memberIds.begin(), memberIds.end());
        if (AZStd::adjacent_find(memberIds.begin(), memberIds.end())
            != memberIds.end())
        {
            return AZ::Failure(AZStd::string(
                "A troop definition cannot contain duplicate member-link IDs."));
        }

        if (!ValidateEvidence(
                definition.m_profile.m_evidenceIds,
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
            if (!ValidateEvidence(
                    member.m_evidenceIds,
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
        document.m_troopProfiles.erase(
            AZStd::remove_if(
                document.m_troopProfiles.begin(),
                document.m_troopProfiles.end(),
                [&definition](const PopulationTroopProfile& troop)
                {
                    return troop.m_recordId
                        == definition.m_profile.m_recordId;
                }),
            document.m_troopProfiles.end());
        document.m_troopMembers.erase(
            AZStd::remove_if(
                document.m_troopMembers.begin(),
                document.m_troopMembers.end(),
                [&definition](const PopulationTroopMember& member)
                {
                    return member.m_troopRecordId
                        == definition.m_profile.m_recordId;
                }),
            document.m_troopMembers.end());
        document.m_troopProfiles.push_back(definition.m_profile);
        document.m_troopMembers.insert(
            document.m_troopMembers.end(),
            definition.m_members.begin(),
            definition.m_members.end());

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
            || !ValidateEvidence(
                member.m_evidenceIds,
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
