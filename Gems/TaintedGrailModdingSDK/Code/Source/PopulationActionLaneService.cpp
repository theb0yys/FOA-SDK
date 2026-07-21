/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PopulationActionLaneService.h"

#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr PopulationActionLane ActionLanes[] = {
            PopulationActionLane::Display,
            PopulationActionLane::AuthorProfile,
            PopulationActionLane::ComposeTroop,
            PopulationActionLane::Planning,
            PopulationActionLane::SpawnCandidate,
            PopulationActionLane::RuntimeSpawn,
            PopulationActionLane::SaveMutation,
        };

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

        void SortUnique(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        bool PackTargetsProfile(
            const PackManifest& pack,
            const GameProfile& profile)
        {
            const bool versionMatches =
                pack.m_targetGameVersion == profile.m_gameVersion
                || Contains(
                    pack.m_compatibleGameVersions,
                    profile.m_gameVersion);
            return !pack.m_targetGameVersion.empty()
                && !pack.m_targetBranch.empty()
                && versionMatches
                && pack.m_targetBranch == profile.m_branch;
        }

        bool HasActiveAuthoringContext(
            const WorkspaceModel& workspace,
            const AZStd::string& resolvedWorkspaceRoot,
            const PackManifest* activePack,
            const CatalogRecord& record,
            AZStd::string& reason)
        {
            const GameProfile* profile = workspace.FindActiveGameProfile();
            if (!IsStableContractId(workspace.m_workspaceId)
                || resolvedWorkspaceRoot.empty()
                || !profile
                || !profile->IsConfigured())
            {
                reason = "No saved or loaded path-policy-resolved workspace "
                    "root and exact configured active game profile is available "
                    "for population authoring.";
                return false;
            }
            if (!activePack
                || !activePack->HasStableIdentity()
                || !activePack->UsesSupportedSchema()
                || activePack->m_runtimeActionsEnabled
                || !PackTargetsProfile(*activePack, *profile))
            {
                reason = "No stable editor-owned active pack targets the exact active game profile.";
                return false;
            }
            if (!record.m_ownerPackId.empty()
                && record.m_ownerPackId != activePack->m_packId)
            {
                reason = "The selected population record belongs to a different pack than the active authoring pack.";
                return false;
            }
            return true;
        }

        bool HasExactRecordEvidence(
            const CatalogRecord& record,
            const GameProfile* profile,
            const SourceEvidenceRegistry& sourceRegistry)
        {
            if (!profile || record.m_evidenceIds.empty())
            {
                return false;
            }

            AZStd::vector<AZStd::string> evidenceIds = record.m_evidenceIds;
            AZStd::sort(evidenceIds.begin(), evidenceIds.end());
            if (AZStd::adjacent_find(evidenceIds.begin(), evidenceIds.end())
                != evidenceIds.end())
            {
                return false;
            }

            for (const AZStd::string& evidenceId : evidenceIds)
            {
                const EvidenceRecord* evidence =
                    sourceRegistry.FindEvidence(evidenceId);
                const SourceRecord* source = evidence
                    ? sourceRegistry.FindSource(evidence->m_sourceId)
                    : nullptr;
                if (!evidence
                    || !source
                    || !IsUsableImportStatus(source->m_importStatus)
                    || !IsSha256Fingerprint(evidence->m_sourceFingerprint)
                    || evidence->m_sourceId != source->m_sourceId
                    || evidence->m_sourceFingerprint != source->m_fingerprint
                    || evidence->m_profileId != profile->m_profileId
                    || evidence->m_gameVersion != profile->m_gameVersion
                    || evidence->m_branch != profile->m_branch
                    || source->m_profileId != profile->m_profileId
                    || source->m_gameVersion != profile->m_gameVersion
                    || source->m_branch != profile->m_branch
                    || source->m_runtimeTarget != profile->m_runtimeTarget
                    || evidence->m_subjectRef != record.m_subjectRef
                    || evidence->m_claim.empty()
                    || evidence->m_evidenceKind.empty()
                    || evidence->m_locator.empty()
                    || evidence->m_recordPath.empty()
                    || !IsStrictUtcTimestamp(evidence->m_extractedAt))
                {
                    return false;
                }
            }
            return true;
        }

        bool IsCurrentValidated(const CatalogRecord& record)
        {
            return record.m_validationState == "validated"
                && record.m_stalenessState == "current"
                && record.m_missingRefs.empty()
                && record.m_conflictRefs.empty()
                && record.m_supersededByRecordId.empty();
        }

        bool HasPersistedPopulationDefinition(
            const CatalogRecord& record,
            const CatalogDatabase& catalog)
        {
            if (record.m_recordKind == "actor")
            {
                return catalog.FindPopulationActorProfile(record.m_recordId)
                    != nullptr;
            }
            return catalog.FindPopulationTroopProfile(record.m_recordId)
                    != nullptr
                && !catalog.FindPopulationMembersForTroop(record.m_recordId).empty();
        }

        bool BlockerTargetsRecord(
            const BlockerRecord& blocker,
            const CatalogRecord& record)
        {
            return blocker.m_subjectRef == record.m_subjectRef
                || blocker.m_subjectRef == record.m_recordId
                || blocker.m_subjectRef == "record:" + record.m_recordId;
        }

        bool BlockerAffectsLane(
            const BlockerRecord& blocker,
            const AZStd::string& laneName,
            PopulationActionLane lane)
        {
            if (Contains(blocker.m_affectedUsages, laneName))
            {
                return true;
            }
            // Subject-wide blockers are relevant to spawn candidacy. Authoring,
            // display, composition, and planning remain independently governed.
            return blocker.m_affectedUsages.empty()
                && lane == PopulationActionLane::SpawnCandidate;
        }

        void CollectBlockers(
            const AZStd::vector<BlockerRecord>& blockers,
            const CatalogRecord& record,
            PopulationActionLane lane,
            PopulationActionLaneDecision& decision)
        {
            const AZStd::string laneName = ToString(lane);
            for (const BlockerRecord& blocker : blockers)
            {
                if (blocker.m_status == "open"
                    && BlockerTargetsRecord(blocker, record)
                    && BlockerAffectsLane(blocker, laneName, lane))
                {
                    AddUnique(decision.m_blockerIds, blocker.m_blockerId);
                    AddUnique(decision.m_reasons, blocker.m_reason);
                }
            }
        }

        bool RequiresAuthoringContext(PopulationActionLane lane)
        {
            return lane == PopulationActionLane::AuthorProfile
                || lane == PopulationActionLane::ComposeTroop;
        }

        bool RequiresPersistedDefinition(PopulationActionLane lane)
        {
            return lane == PopulationActionLane::Planning
                || lane == PopulationActionLane::SpawnCandidate;
        }

        bool RequiresCatalogIntegrity(PopulationActionLane lane)
        {
            return lane != PopulationActionLane::Display
                && lane != PopulationActionLane::RuntimeSpawn
                && lane != PopulationActionLane::SaveMutation;
        }

        PopulationActionLaneDecision BuildLane(
            PopulationActionLane lane,
            const CatalogRecord& record,
            bool activeAuthoringContext,
            const AZStd::string& authoringContextReason,
            bool catalogIntegrityValid,
            const AZStd::string& catalogIntegrityReason,
            bool exactRecordEvidenceValid,
            bool hasPersistedDefinition,
            const AZStd::vector<BlockerRecord>& blockers)
        {
            PopulationActionLaneDecision decision;
            decision.m_lane = lane;
            const AZStd::string laneName = ToString(lane);

            if (lane == PopulationActionLane::RuntimeSpawn)
            {
                decision.m_state = PopulationActionLaneState::Unavailable;
                AddUnique(
                    decision.m_reasons,
                    "Runtime spawning is unavailable because no reviewed runtime "
                    "adapter, cleanup, or rollback contract exists.");
                return decision;
            }
            if (lane == PopulationActionLane::SaveMutation)
            {
                decision.m_state = PopulationActionLaneState::Unavailable;
                AddUnique(
                    decision.m_reasons,
                    "Save mutation is unavailable because this editor slice defines no game-save contract.");
                return decision;
            }
            if (lane == PopulationActionLane::ComposeTroop
                && record.m_recordKind != "troop")
            {
                decision.m_state = PopulationActionLaneState::NotApplicable;
                AddUnique(
                    decision.m_reasons,
                    "Troop composition applies only to canonical population/troop records.");
                return decision;
            }

            CollectBlockers(blockers, record, lane, decision);
            if (Contains(record.m_forbiddenUsages, laneName))
            {
                decision.m_state = PopulationActionLaneState::Forbidden;
                AddUnique(
                    decision.m_reasons,
                    "Catalog governance explicitly forbids this population action lane.");
            }
            else
            {
                if (RequiresCatalogIntegrity(lane) && !catalogIntegrityValid)
                {
                    AddUnique(decision.m_reasons, catalogIntegrityReason);
                }
                if (RequiresAuthoringContext(lane) && !activeAuthoringContext)
                {
                    AddUnique(decision.m_reasons, authoringContextReason);
                }
                if (RequiresAuthoringContext(lane)
                    && !exactRecordEvidenceValid)
                {
                    AddUnique(
                        decision.m_reasons,
                        "Population authoring requires complete exact-subject "
                        "record evidence bound to the active source fingerprint "
                        "and game profile.");
                }
                if (RequiresPersistedDefinition(lane) && !hasPersistedDefinition)
                {
                    AddUnique(
                        decision.m_reasons,
                        "The selected population record has no complete "
                        "persisted typed profile or troop composition.");
                }
                if (lane == PopulationActionLane::SpawnCandidate
                    && !IsCurrentValidated(record))
                {
                    AddUnique(
                        decision.m_reasons,
                        "Spawn candidacy requires a validated, current, "
                        "unresolved-free, non-superseded catalog record.");
                }

                if (!decision.m_blockerIds.empty()
                    || !decision.m_reasons.empty())
                {
                    decision.m_state = PopulationActionLaneState::Blocked;
                }
                else if (Contains(record.m_allowedUsages, laneName))
                {
                    decision.m_state = PopulationActionLaneState::Allowed;
                }
                else
                {
                    decision.m_state = PopulationActionLaneState::Unset;
                }
            }

            SortUnique(decision.m_blockerIds);
            SortUnique(decision.m_reasons);
            return decision;
        }
    } // namespace

    AZStd::string ToString(PopulationActionLane lane)
    {
        switch (lane)
        {
        case PopulationActionLane::Display:
            return "display";
        case PopulationActionLane::AuthorProfile:
            return "author_profile";
        case PopulationActionLane::ComposeTroop:
            return "compose_troop";
        case PopulationActionLane::Planning:
            return "planning";
        case PopulationActionLane::SpawnCandidate:
            return "spawn_candidate";
        case PopulationActionLane::RuntimeSpawn:
            return "runtime_spawn";
        case PopulationActionLane::SaveMutation:
            return "save_mutation";
        }
        return {};
    }

    AZStd::string ToString(PopulationActionLaneState state)
    {
        switch (state)
        {
        case PopulationActionLaneState::Allowed:
            return "allowed";
        case PopulationActionLaneState::Unset:
            return "unset";
        case PopulationActionLaneState::Forbidden:
            return "forbidden";
        case PopulationActionLaneState::Blocked:
            return "blocked";
        case PopulationActionLaneState::Unavailable:
            return "unavailable";
        case PopulationActionLaneState::NotApplicable:
            return "not_applicable";
        }
        return {};
    }

    AZ::Outcome<AZStd::vector<PopulationActionLaneDecision>, AZStd::string>
    PopulationActionLaneService::BuildActionLaneMatrix(
        const AZStd::string& recordId,
        const WorkspaceModel& workspace,
        const AZStd::string& resolvedWorkspaceRoot,
        const PackManifest* activePack,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog,
        const AZStd::vector<BlockerRecord>& blockers) const
    {
        const CatalogRecord* record = catalog.FindByRecordId(recordId);
        if (!record
            || record->m_domain != "population"
            || (record->m_recordKind != "actor"
                && record->m_recordKind != "troop"))
        {
            return AZ::Failure(
                AZStd::string(
                    "Population action lanes require one existing canonical "
                    "population/actor or population/troop record."));
        }

        AZStd::string authoringContextReason;
        const bool activeAuthoringContext = HasActiveAuthoringContext(
            workspace,
            resolvedWorkspaceRoot,
            activePack,
            *record,
            authoringContextReason);

        const GameProfile* profile = workspace.FindActiveGameProfile();
        AZStd::string catalogIntegrityError;
        const bool catalogIntegrityValid = profile
            && catalog.ValidateIntegrity(
                workspace,
                *profile,
                sourceRegistry,
                &catalogIntegrityError);
        const AZStd::string catalogIntegrityReason = catalogIntegrityValid
            ? AZStd::string{}
            : AZStd::string(
                "The published catalog and exact authoring evidence failed integrity validation: ")
                + (catalogIntegrityError.empty()
                    ? AZStd::string("no exact active profile is available")
                    : catalogIntegrityError);

        const bool exactRecordEvidenceValid = HasExactRecordEvidence(
            *record,
            profile,
            sourceRegistry);

        const bool hasPersistedDefinition =
            HasPersistedPopulationDefinition(*record, catalog);

        AZStd::vector<PopulationActionLaneDecision> decisions;
        decisions.reserve(AZ_ARRAY_SIZE(ActionLanes));
        for (PopulationActionLane lane : ActionLanes)
        {
            decisions.push_back(BuildLane(
                lane,
                *record,
                activeAuthoringContext,
                authoringContextReason,
                catalogIntegrityValid,
                catalogIntegrityReason,
                exactRecordEvidenceValid,
                hasPersistedDefinition,
                blockers));
        }
        return AZ::Success(AZStd::move(decisions));
    }
} // namespace TaintedGrailModdingSDK
