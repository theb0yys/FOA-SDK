/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogGovernanceService.h"
#include "PopulationActionLaneService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>
#include <AzTest/AzTest.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* ProfileId = "profile.population.lanes";
        constexpr const char* GameVersion = "1.0.0-test";
        constexpr const char* Branch = "mono";
        constexpr const char* PackId = "pack.population.lanes";
        constexpr const char* WorkspaceRoot = "/workspace/population-lanes";
        constexpr const char* SourceId = "source.population.lanes";
        constexpr const char* SourceFingerprint =
            "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

        constexpr const char* ActorId = "population.actor.lanes";
        constexpr const char* ActorSubject =
            "subject:population:actor:lanes";
        constexpr const char* ActorEvidence =
            "evidence.population.actor.lanes";
        constexpr const char* TroopId = "population.troop.lanes";
        constexpr const char* TroopSubject =
            "subject:population:troop:lanes";
        constexpr const char* TroopEvidence =
            "evidence.population.troop.lanes";
        constexpr const char* MemberId = "population.member.lanes";
        constexpr const char* MemberEvidence =
            "evidence.population.member.lanes";

        WorkspaceModel MakeWorkspace()
        {
            GameProfile profile;
            profile.m_profileId = ProfileId;
            profile.m_displayName = "Population action-lane test profile";
            profile.m_installPath = "/game";
            profile.m_gameVersion = GameVersion;
            profile.m_branch = Branch;
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "2022.3.22f1";
            profile.m_bepInExVersion = "5.4.23.3";
            profile.m_managedAssembliesPath = "/game/Managed";
            profile.m_pluginPath = "/game/BepInEx/plugins";

            WorkspaceModel workspace;
            workspace.m_workspaceId = "workspace.population.lanes";
            workspace.m_displayName = "Population action-lane tests";
            workspace.m_rootPath = WorkspaceRoot;
            workspace.m_outputPath = "/workspace/population-lanes/Build";
            workspace.m_stagingPath = "/workspace/population-lanes/Staging";
            workspace.m_deploymentPath =
                "/workspace/population-lanes/Deployment";
            workspace.m_activeGameProfileId = profile.m_profileId;
            workspace.m_gameProfiles.push_back(AZStd::move(profile));
            return workspace;
        }

        PackManifest MakePack(AZStd::string packId = PackId)
        {
            PackManifest pack;
            pack.m_schemaVersion = 1;
            pack.m_packId = AZStd::move(packId);
            pack.m_displayName = "Population action-lane test pack";
            pack.m_ownerId = "owner.population-tests";
            pack.m_version = "1.0.0";
            pack.m_targetGameVersion = GameVersion;
            pack.m_targetBranch = Branch;
            pack.m_runtimeActionsEnabled = false;
            return pack;
        }

        SourceEvidenceRegistry MakeRegistry()
        {
            SourceRecord source;
            source.m_sourceId = SourceId;
            source.m_title = "Population action-lane synthetic fixture";
            source.m_sourceKind = "synthetic-fixture";
            source.m_locator = "Tests/Fixtures/population-action-lanes.json";
            source.m_fingerprint = SourceFingerprint;
            source.m_profileId = ProfileId;
            source.m_gameVersion = GameVersion;
            source.m_branch = Branch;
            source.m_runtimeTarget = "Mono";
            source.m_toolName = "population-action-lane-tests";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "test.population.action-lanes";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-20T00:00:00Z";
            source.m_importedAt = "2026-07-20T00:00:02Z";
            source.m_limitations = "Project-owned synthetic test fixture.";
            source.m_mediaType = "application/json";
            source.m_byteSize = 512;
            source.m_importStatus = "imported";

            SourceEvidenceRegistry registry;
            AZStd::string error;
            EXPECT_TRUE(registry.RegisterSource(source, &error))
                << error.c_str();

            const auto registerEvidence =
                [&registry, &source, &error](
                    const char* evidenceId,
                    const char* subject,
                    const char* recordPath)
                {
                    EvidenceRecord evidence;
                    evidence.m_evidenceId = evidenceId;
                    evidence.m_sourceId = source.m_sourceId;
                    evidence.m_sourceFingerprint = source.m_fingerprint;
                    evidence.m_profileId = source.m_profileId;
                    evidence.m_gameVersion = source.m_gameVersion;
                    evidence.m_branch = source.m_branch;
                    evidence.m_subjectRef = subject;
                    evidence.m_claim =
                        "Synthetic exact-subject population claim.";
                    evidence.m_evidenceKind = "structured_record";
                    evidence.m_confidence = "documented";
                    evidence.m_locator = source.m_locator;
                    evidence.m_recordPath = recordPath;
                    evidence.m_extractedAt = "2026-07-20T00:00:01Z";
                    EXPECT_TRUE(registry.RegisterEvidence(evidence, &error))
                        << error.c_str();
                };

            registerEvidence(ActorEvidence, ActorSubject, "/actors/lanes");
            registerEvidence(TroopEvidence, TroopSubject, "/troops/lanes");
            registerEvidence(
                MemberEvidence,
                "population-troop-member:population.member.lanes",
                "/troops/lanes/members/leader");
            return registry;
        }

        CatalogRecord MakePopulationRecord(
            AZStd::string recordId,
            AZStd::string recordKind,
            AZStd::string subjectRef,
            AZStd::string evidenceId)
        {
            CatalogRecord record;
            record.m_recordId = AZStd::move(recordId);
            record.m_ownerPackId = PackId;
            record.m_domain = "population";
            record.m_recordKind = AZStd::move(recordKind);
            record.m_subjectRef = AZStd::move(subjectRef);
            record.m_identityKind = "synthetic";
            record.m_displayName = record.m_recordId;
            record.m_researchStage = "S5";
            record.m_confidence = "documented";
            record.m_operationalRisk = "low";
            record.m_validationState = "unvalidated";
            record.m_stalenessState = "unknown";
            record.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
            record.m_evidenceIds = { AZStd::move(evidenceId) };
            record.m_createdAt = "2026-07-20T00:00:00Z";
            record.m_updatedAt = "2026-07-20T00:00:00Z";
            return record;
        }

        CatalogDatabase NormalizeGovernanceTimes(
            const CatalogDatabase& catalog,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& registry)
        {
            CatalogDocument document = catalog.BuildDocument(
                workspace,
                *workspace.FindActiveGameProfile());
            for (CatalogValidationEvent& validation :
                 document.m_validationHistory)
            {
                validation.m_checkedAt = "2026-07-20T00:00:03Z";
            }

            constexpr const char* GovernanceTimes[] = {
                "2026-07-20T00:00:04Z",
                "2026-07-20T00:00:05Z",
                "2026-07-20T00:00:06Z",
                "2026-07-20T00:00:07Z",
                "2026-07-20T00:00:08Z",
                "2026-07-20T00:00:09Z",
                "2026-07-20T00:00:10Z",
                "2026-07-20T00:00:11Z",
                "2026-07-20T00:00:12Z",
                "2026-07-20T00:00:13Z",
            };
            EXPECT_TRUE(
                document.m_governanceHistory.size()
                <= AZ_ARRAY_SIZE(GovernanceTimes));
            if (document.m_governanceHistory.size()
                > AZ_ARRAY_SIZE(GovernanceTimes))
            {
                return {};
            }
            for (size_t index = 0;
                 index < document.m_governanceHistory.size();
                 ++index)
            {
                document.m_governanceHistory[index].m_decidedAt =
                    GovernanceTimes[index];
            }

            CatalogDatabase normalized;
            AZStd::string error;
            EXPECT_TRUE(normalized.ReplaceFromBoundDocument(
                document,
                workspace,
                *workspace.FindActiveGameProfile(),
                registry,
                &error)) << error.c_str();
            return normalized;
        }

        CatalogDatabase GrantUsages(
            CatalogDatabase catalog,
            const AZStd::string& recordId,
            const AZStd::string& evidenceId,
            const AZStd::vector<AZStd::string>& usages,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& registry)
        {
            CatalogGovernanceService service;
            CatalogValidationRequest validation;
            validation.m_subjectKind = "record";
            validation.m_subjectId = recordId;
            validation.m_state = "validated";
            validation.m_method = "synthetic-unit-test";
            validation.m_evidenceIds = { evidenceId };
            validation.m_validator = "population-action-lane-tests";
            auto validationResult = service.ApplyValidation(
                validation,
                workspace,
                registry,
                catalog);
            EXPECT_TRUE(validationResult.IsSuccess())
                << (validationResult.IsSuccess()
                    ? ""
                    : validationResult.GetError().c_str());
            if (!validationResult.IsSuccess())
            {
                return {};
            }
            CatalogValidationApplyResult validated =
                validationResult.TakeValue();
            catalog = AZStd::move(validated.m_catalog);

            CatalogGovernanceRequest current;
            current.m_subjectKind = "record";
            current.m_subjectId = recordId;
            current.m_axis = "staleness";
            current.m_value = "current";
            current.m_evidenceIds = { evidenceId };
            current.m_reviewer = "population-action-lane-tests";
            auto currentResult = service.ApplyDecision(
                current,
                workspace,
                registry,
                catalog);
            EXPECT_TRUE(currentResult.IsSuccess())
                << (currentResult.IsSuccess()
                    ? ""
                    : currentResult.GetError().c_str());
            if (!currentResult.IsSuccess())
            {
                return {};
            }
            catalog = currentResult.TakeValue().m_catalog;

            for (const AZStd::string& usage : usages)
            {
                CatalogGovernanceRequest permission;
                permission.m_subjectKind = "record";
                permission.m_subjectId = recordId;
                permission.m_axis = "permission";
                permission.m_value = "allow";
                permission.m_usage = usage;
                permission.m_evidenceIds = { evidenceId };
                permission.m_validationIds = {
                    validated.m_event.m_validationId,
                };
                permission.m_reviewer = "population-action-lane-tests";
                auto permissionResult = service.ApplyDecision(
                    permission,
                    workspace,
                    registry,
                    catalog);
                EXPECT_TRUE(permissionResult.IsSuccess())
                    << (permissionResult.IsSuccess()
                        ? ""
                        : permissionResult.GetError().c_str());
                if (!permissionResult.IsSuccess())
                {
                    return {};
                }
                catalog = permissionResult.TakeValue().m_catalog;
            }
            return NormalizeGovernanceTimes(catalog, workspace, registry);
        }

        CatalogDatabase ForbidUsage(
            CatalogDatabase catalog,
            const AZStd::string& recordId,
            const AZStd::string& evidenceId,
            const AZStd::string& usage,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& registry)
        {
            CatalogGovernanceRequest permission;
            permission.m_subjectKind = "record";
            permission.m_subjectId = recordId;
            permission.m_axis = "permission";
            permission.m_value = "forbid";
            permission.m_usage = usage;
            permission.m_evidenceIds = { evidenceId };
            permission.m_reviewer = "population-action-lane-tests";
            CatalogGovernanceService service;
            auto result = service.ApplyDecision(
                permission,
                workspace,
                registry,
                catalog);
            EXPECT_TRUE(result.IsSuccess())
                << (result.IsSuccess() ? "" : result.GetError().c_str());
            return result.IsSuccess()
                ? NormalizeGovernanceTimes(
                    result.GetValue().m_catalog,
                    workspace,
                    registry)
                : CatalogDatabase{};
        }

        CatalogDatabase MakeActorCatalog(
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& registry,
            bool withProfile,
            const AZStd::vector<AZStd::string>& usages)
        {
            CatalogDatabase catalog;
            AZStd::string error;
            EXPECT_TRUE(catalog.InsertNew(
                MakePopulationRecord(
                    ActorId,
                    "actor",
                    ActorSubject,
                    ActorEvidence),
                &error)) << error.c_str();
            if (withProfile)
            {
                PopulationActorProfile actor;
                actor.m_recordId = ActorId;
                actor.m_actorKind = "npc";
                actor.m_archetype = "synthetic_test_actor";
                actor.m_minimumLevel = 1;
                actor.m_maximumLevel = 1;
                actor.m_evidenceIds = { ActorEvidence };
                EXPECT_TRUE(catalog.UpsertPopulationActorProfile(actor, &error))
                    << error.c_str();
            }
            return GrantUsages(
                AZStd::move(catalog),
                ActorId,
                ActorEvidence,
                usages,
                workspace,
                registry);
        }

        CatalogDatabase MakeTroopCatalog(
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& registry,
            const AZStd::vector<AZStd::string>& usages)
        {
            CatalogDatabase catalog;
            AZStd::string error;
            EXPECT_TRUE(catalog.InsertNew(
                MakePopulationRecord(
                    ActorId,
                    "actor",
                    ActorSubject,
                    ActorEvidence),
                &error)) << error.c_str();
            EXPECT_TRUE(catalog.InsertNew(
                MakePopulationRecord(
                    TroopId,
                    "troop",
                    TroopSubject,
                    TroopEvidence),
                &error)) << error.c_str();

            PopulationTroopProfile troop;
            troop.m_recordId = TroopId;
            troop.m_troopKind = "party";
            troop.m_leaderActorRecordId = ActorId;
            troop.m_minimumSize = 1;
            troop.m_maximumSize = 1;
            troop.m_evidenceIds = { TroopEvidence };
            EXPECT_TRUE(catalog.UpsertPopulationTroopProfile(troop, &error))
                << error.c_str();

            PopulationTroopMember member;
            member.m_linkId = MemberId;
            member.m_troopRecordId = TroopId;
            member.m_actorRecordId = ActorId;
            member.m_role = "leader";
            member.m_minimumCount = 1;
            member.m_maximumCount = 1;
            member.m_weight = 1.0;
            member.m_required = true;
            member.m_evidenceIds = { MemberEvidence };
            EXPECT_TRUE(catalog.UpsertPopulationTroopMember(member, &error))
                << error.c_str();

            return GrantUsages(
                AZStd::move(catalog),
                TroopId,
                TroopEvidence,
                usages,
                workspace,
                registry);
        }

        const PopulationActionLaneDecision* FindLane(
            const AZStd::vector<PopulationActionLaneDecision>& lanes,
            PopulationActionLane lane)
        {
            for (const PopulationActionLaneDecision& decision : lanes)
            {
                if (decision.m_lane == lane)
                {
                    return &decision;
                }
            }
            return nullptr;
        }

        BlockerRecord MakeBlocker(
            AZStd::string blockerId,
            AZStd::string subjectRef,
            AZStd::string reason,
            AZStd::vector<AZStd::string> usages,
            AZStd::string status = "open")
        {
            BlockerRecord blocker;
            blocker.m_blockerId = AZStd::move(blockerId);
            blocker.m_severity = "error";
            blocker.m_area = "population-test";
            blocker.m_subjectRef = AZStd::move(subjectRef);
            blocker.m_reason = AZStd::move(reason);
            blocker.m_status = AZStd::move(status);
            blocker.m_affectedUsages = AZStd::move(usages);
            return blocker;
        }
    } // namespace

    TEST(PopulationActionLaneServiceTests, ClosedVocabularyUsesDeterministicContractValues)
    {
        EXPECT_EQ(ToString(PopulationActionLane::Display), "display");
        EXPECT_EQ(
            ToString(PopulationActionLane::AuthorProfile),
            "author_profile");
        EXPECT_EQ(
            ToString(PopulationActionLane::ComposeTroop),
            "compose_troop");
        EXPECT_EQ(ToString(PopulationActionLane::Planning), "planning");
        EXPECT_EQ(
            ToString(PopulationActionLane::SpawnCandidate),
            "spawn_candidate");
        EXPECT_EQ(
            ToString(PopulationActionLane::RuntimeSpawn),
            "runtime_spawn");
        EXPECT_EQ(
            ToString(PopulationActionLane::SaveMutation),
            "save_mutation");
        EXPECT_TRUE(
            ToString(static_cast<PopulationActionLane>(255)).empty());

        EXPECT_EQ(ToString(PopulationActionLaneState::Allowed), "allowed");
        EXPECT_EQ(ToString(PopulationActionLaneState::Unset), "unset");
        EXPECT_EQ(
            ToString(PopulationActionLaneState::Forbidden),
            "forbidden");
        EXPECT_EQ(ToString(PopulationActionLaneState::Blocked), "blocked");
        EXPECT_EQ(
            ToString(PopulationActionLaneState::Unavailable),
            "unavailable");
        EXPECT_EQ(
            ToString(PopulationActionLaneState::NotApplicable),
            "not_applicable");
        EXPECT_TRUE(
            ToString(static_cast<PopulationActionLaneState>(255)).empty());
    }

    TEST(PopulationActionLaneServiceTests, FixedLaneOrderAndHardUnavailableRuntimeLanes)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const PackManifest pack = MakePack();
        const SourceEvidenceRegistry registry = MakeRegistry();
        const AZStd::vector<AZStd::string> usages = {
            "display",
            "author_profile",
            "planning",
            "spawn_candidate",
            "runtime_spawn",
            "save_mutation",
        };
        const CatalogDatabase catalog = MakeActorCatalog(
            workspace,
            registry,
            true,
            usages);

        PopulationActionLaneService service;
        auto result = service.BuildActionLaneMatrix(
            ActorId,
            workspace,
            WorkspaceRoot,
            &pack,
            registry,
            catalog,
            {});
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
        const auto& lanes = result.GetValue();
        ASSERT_EQ(lanes.size(), 7);
        EXPECT_EQ(lanes[0].m_lane, PopulationActionLane::Display);
        EXPECT_EQ(lanes[1].m_lane, PopulationActionLane::AuthorProfile);
        EXPECT_EQ(lanes[2].m_lane, PopulationActionLane::ComposeTroop);
        EXPECT_EQ(lanes[3].m_lane, PopulationActionLane::Planning);
        EXPECT_EQ(lanes[4].m_lane, PopulationActionLane::SpawnCandidate);
        EXPECT_EQ(lanes[5].m_lane, PopulationActionLane::RuntimeSpawn);
        EXPECT_EQ(lanes[6].m_lane, PopulationActionLane::SaveMutation);
        EXPECT_EQ(lanes[0].m_state, PopulationActionLaneState::Allowed);
        EXPECT_EQ(lanes[1].m_state, PopulationActionLaneState::Allowed);
        EXPECT_EQ(
            lanes[2].m_state,
            PopulationActionLaneState::NotApplicable);
        EXPECT_EQ(lanes[3].m_state, PopulationActionLaneState::Allowed);
        EXPECT_EQ(lanes[4].m_state, PopulationActionLaneState::Allowed);
        EXPECT_EQ(
            lanes[5].m_state,
            PopulationActionLaneState::Unavailable);
        EXPECT_EQ(
            lanes[6].m_state,
            PopulationActionLaneState::Unavailable);
    }

    TEST(PopulationActionLaneServiceTests, AuthoringRequiresResolvedWorkspacePackAndExactEvidence)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const PackManifest pack = MakePack();
        const SourceEvidenceRegistry registry = MakeRegistry();
        const CatalogDatabase catalog = MakeActorCatalog(
            workspace,
            registry,
            false,
            { "display", "author_profile", "planning" });

        PopulationActionLaneService service;
        auto ready = service.BuildActionLaneMatrix(
            ActorId,
            workspace,
            WorkspaceRoot,
            &pack,
            registry,
            catalog,
            {});
        ASSERT_TRUE(ready.IsSuccess()) << ready.GetError().c_str();
        ASSERT_NE(
            FindLane(ready.GetValue(), PopulationActionLane::AuthorProfile),
            nullptr);
        EXPECT_EQ(
            FindLane(ready.GetValue(), PopulationActionLane::AuthorProfile)
                ->m_state,
            PopulationActionLaneState::Allowed);
        EXPECT_EQ(
            FindLane(ready.GetValue(), PopulationActionLane::Planning)
                ->m_state,
            PopulationActionLaneState::Blocked);

        auto noResolvedRoot = service.BuildActionLaneMatrix(
            ActorId,
            workspace,
            {},
            &pack,
            registry,
            catalog,
            {});
        ASSERT_TRUE(noResolvedRoot.IsSuccess());
        EXPECT_EQ(
            FindLane(
                noResolvedRoot.GetValue(),
                PopulationActionLane::AuthorProfile)->m_state,
            PopulationActionLaneState::Blocked);

        SourceEvidenceRegistry emptyRegistry;
        auto noExactEvidence = service.BuildActionLaneMatrix(
            ActorId,
            workspace,
            WorkspaceRoot,
            &pack,
            emptyRegistry,
            catalog,
            {});
        ASSERT_TRUE(noExactEvidence.IsSuccess());
        EXPECT_EQ(
            FindLane(
                noExactEvidence.GetValue(),
                PopulationActionLane::Display)->m_state,
            PopulationActionLaneState::Allowed);
        const PopulationActionLaneDecision* author = FindLane(
            noExactEvidence.GetValue(),
            PopulationActionLane::AuthorProfile);
        ASSERT_NE(author, nullptr);
        EXPECT_EQ(author->m_state, PopulationActionLaneState::Blocked);
        EXPECT_NE(
            AZStd::find_if(
                author->m_reasons.begin(),
                author->m_reasons.end(),
                [](const AZStd::string& reason)
                {
                    return reason.find("exact-subject record evidence")
                        != AZStd::string::npos;
                }),
            author->m_reasons.end());
    }

    TEST(PopulationActionLaneServiceTests, TroopCompositionPlanningAndSpawnCandidacyFailClosed)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const PackManifest pack = MakePack();
        const SourceEvidenceRegistry registry = MakeRegistry();
        const CatalogDatabase catalog = MakeTroopCatalog(
            workspace,
            registry,
            {
                "author_profile",
                "compose_troop",
                "planning",
                "spawn_candidate",
            });

        PopulationActionLaneService service;
        auto ready = service.BuildActionLaneMatrix(
            TroopId,
            workspace,
            WorkspaceRoot,
            &pack,
            registry,
            catalog,
            {});
        ASSERT_TRUE(ready.IsSuccess()) << ready.GetError().c_str();
        EXPECT_EQ(
            FindLane(ready.GetValue(), PopulationActionLane::ComposeTroop)
                ->m_state,
            PopulationActionLaneState::Allowed);
        EXPECT_EQ(
            FindLane(ready.GetValue(), PopulationActionLane::Planning)->m_state,
            PopulationActionLaneState::Allowed);
        EXPECT_EQ(
            FindLane(
                ready.GetValue(),
                PopulationActionLane::SpawnCandidate)->m_state,
            PopulationActionLaneState::Allowed);

        const AZStd::vector<BlockerRecord> spawnBlockers = {
            MakeBlocker(
                "blocker.troop-spawn",
                TroopSubject,
                "Troop spawn candidacy remains blocked.",
                {}),
        };
        auto blockedSpawn = service.BuildActionLaneMatrix(
            TroopId,
            workspace,
            WorkspaceRoot,
            &pack,
            registry,
            catalog,
            spawnBlockers);
        ASSERT_TRUE(blockedSpawn.IsSuccess());
        EXPECT_EQ(
            FindLane(
                blockedSpawn.GetValue(),
                PopulationActionLane::Planning)->m_state,
            PopulationActionLaneState::Allowed);
        EXPECT_EQ(
            FindLane(
                blockedSpawn.GetValue(),
                PopulationActionLane::SpawnCandidate)->m_state,
            PopulationActionLaneState::Blocked);

        const PackManifest otherPack = MakePack("pack.population.other");
        auto wrongPack = service.BuildActionLaneMatrix(
            TroopId,
            workspace,
            WorkspaceRoot,
            &otherPack,
            registry,
            catalog,
            {});
        ASSERT_TRUE(wrongPack.IsSuccess());
        EXPECT_EQ(
            FindLane(
                wrongPack.GetValue(),
                PopulationActionLane::ComposeTroop)->m_state,
            PopulationActionLaneState::Blocked);
        EXPECT_EQ(
            FindLane(wrongPack.GetValue(), PopulationActionLane::Planning)
                ->m_state,
            PopulationActionLaneState::Allowed);
    }

    TEST(PopulationActionLaneServiceTests, SpawnCandidateRejectsUnvalidatedStaleMissingConflictedAndSupersededRecords)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const PackManifest pack = MakePack();
        const SourceEvidenceRegistry registry = MakeRegistry();
        const CatalogDatabase baseline = MakeActorCatalog(
            workspace,
            registry,
            true,
            {});
        const CatalogRecord* baselineRecord =
            baseline.FindByRecordId(ActorId);
        ASSERT_NE(baselineRecord, nullptr);

        AZStd::vector<CatalogRecord> rejectedRecords;
        CatalogRecord unvalidated = *baselineRecord;
        unvalidated.m_validationState = "unvalidated";
        rejectedRecords.push_back(unvalidated);

        CatalogRecord stale = *baselineRecord;
        stale.m_stalenessState = "stale";
        rejectedRecords.push_back(stale);

        CatalogRecord missing = *baselineRecord;
        missing.m_missingRefs = { "population.reference.missing" };
        rejectedRecords.push_back(missing);

        CatalogRecord conflicted = *baselineRecord;
        conflicted.m_conflictRefs = { "population.reference.conflicted" };
        rejectedRecords.push_back(conflicted);

        CatalogRecord superseded = *baselineRecord;
        superseded.m_supersededByRecordId = "population.actor.replacement";
        rejectedRecords.push_back(superseded);

        PopulationActionLaneService service;
        for (const CatalogRecord& rejected : rejectedRecords)
        {
            CatalogDatabase catalog = baseline;
            AZStd::string error;
            ASSERT_TRUE(catalog.Upsert(rejected, &error)) << error.c_str();

            auto result = service.BuildActionLaneMatrix(
                ActorId,
                workspace,
                WorkspaceRoot,
                &pack,
                registry,
                catalog,
                {});
            ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
            const PopulationActionLaneDecision* planning = FindLane(
                result.GetValue(),
                PopulationActionLane::Planning);
            const PopulationActionLaneDecision* spawn = FindLane(
                result.GetValue(),
                PopulationActionLane::SpawnCandidate);
            ASSERT_NE(planning, nullptr);
            ASSERT_NE(spawn, nullptr);
            EXPECT_EQ(planning->m_state, PopulationActionLaneState::Unset);
            EXPECT_EQ(spawn->m_state, PopulationActionLaneState::Blocked);
            EXPECT_NE(
                AZStd::find_if(
                    spawn->m_reasons.begin(),
                    spawn->m_reasons.end(),
                    [](const AZStd::string& reason)
                    {
                        return reason.find(
                            "validated, current, unresolved-free, "
                            "non-superseded") != AZStd::string::npos;
                    }),
                spawn->m_reasons.end());
        }
    }

    TEST(PopulationActionLaneServiceTests, ForbiddenAndRelevantBlockersTakePrecedenceDeterministically)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const PackManifest pack = MakePack();
        const SourceEvidenceRegistry registry = MakeRegistry();
        CatalogDatabase catalog = MakeActorCatalog(
            workspace,
            registry,
            true,
            {
                "display",
                "author_profile",
                "planning",
                "spawn_candidate",
                "runtime_spawn",
            });
        catalog = ForbidUsage(
            AZStd::move(catalog),
            ActorId,
            ActorEvidence,
            "planning",
            workspace,
            registry);

        const AZStd::vector<BlockerRecord> blockers = {
            MakeBlocker(
                "blocker.z",
                ActorSubject,
                "Second authoring blocker.",
                { "author_profile" }),
            MakeBlocker(
                "blocker.a",
                ActorSubject,
                "First authoring blocker.",
                { "author_profile" }),
            MakeBlocker(
                "blocker.closed",
                ActorSubject,
                "Closed blockers are not active.",
                { "author_profile" },
                "resolved"),
            MakeBlocker(
                "blocker.global",
                ActorSubject,
                "Subject-wide spawn blocker.",
                {}),
            MakeBlocker(
                "blocker.unrelated",
                "subject:population:actor:other",
                "Unrelated blocker.",
                { "display" }),
        };
        const CatalogRecord* before = catalog.FindByRecordId(ActorId);
        ASSERT_NE(before, nullptr);
        const AZStd::vector<AZStd::string> beforeAllowed =
            before->m_allowedUsages;
        const AZStd::vector<AZStd::string> beforeForbidden =
            before->m_forbiddenUsages;
        const size_t workspaceProfileCount = workspace.m_gameProfiles.size();
        const AZStd::string workspaceId = workspace.m_workspaceId;
        const AZStd::string packId = pack.m_packId;
        const AZStd::string packVersion = pack.m_version;
        const size_t sourceCountBefore = registry.GetSources().size();
        const size_t evidenceCountBefore = registry.GetEvidence().size();
        const size_t recordCountBefore = catalog.GetRecords().size();
        const size_t actorProfileCountBefore =
            catalog.GetPopulationActorProfiles().size();
        const size_t validationCountBefore =
            catalog.GetValidationHistory().size();
        const size_t governanceCountBefore =
            catalog.GetGovernanceHistory().size();
        const AZStd::vector<BlockerRecord> blockersBefore = blockers;

        PopulationActionLaneService service;
        auto result = service.BuildActionLaneMatrix(
            ActorId,
            workspace,
            WorkspaceRoot,
            &pack,
            registry,
            catalog,
            blockers);
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
        const auto& lanes = result.GetValue();
        EXPECT_EQ(
            FindLane(lanes, PopulationActionLane::Display)->m_state,
            PopulationActionLaneState::Allowed);
        const PopulationActionLaneDecision* author =
            FindLane(lanes, PopulationActionLane::AuthorProfile);
        ASSERT_NE(author, nullptr);
        EXPECT_EQ(author->m_state, PopulationActionLaneState::Blocked);
        ASSERT_EQ(author->m_blockerIds.size(), 2);
        EXPECT_EQ(author->m_blockerIds[0], "blocker.a");
        EXPECT_EQ(author->m_blockerIds[1], "blocker.z");
        EXPECT_EQ(
            FindLane(lanes, PopulationActionLane::Planning)->m_state,
            PopulationActionLaneState::Forbidden);
        EXPECT_EQ(
            FindLane(lanes, PopulationActionLane::SpawnCandidate)->m_state,
            PopulationActionLaneState::Blocked);
        EXPECT_EQ(
            FindLane(lanes, PopulationActionLane::RuntimeSpawn)->m_state,
            PopulationActionLaneState::Unavailable);

        const CatalogRecord* after = catalog.FindByRecordId(ActorId);
        ASSERT_NE(after, nullptr);
        EXPECT_EQ(after->m_allowedUsages, beforeAllowed);
        EXPECT_EQ(after->m_forbiddenUsages, beforeForbidden);
        ASSERT_EQ(blockers.size(), blockersBefore.size());
        for (size_t index = 0; index < blockers.size(); ++index)
        {
            EXPECT_EQ(
                blockers[index].m_blockerId,
                blockersBefore[index].m_blockerId);
            EXPECT_EQ(
                blockers[index].m_severity,
                blockersBefore[index].m_severity);
            EXPECT_EQ(blockers[index].m_area, blockersBefore[index].m_area);
            EXPECT_EQ(
                blockers[index].m_subjectRef,
                blockersBefore[index].m_subjectRef);
            EXPECT_EQ(blockers[index].m_reason, blockersBefore[index].m_reason);
            EXPECT_EQ(blockers[index].m_status, blockersBefore[index].m_status);
            EXPECT_EQ(
                blockers[index].m_affectedUsages,
                blockersBefore[index].m_affectedUsages);
        }

        auto repeated = service.BuildActionLaneMatrix(
            ActorId,
            workspace,
            WorkspaceRoot,
            &pack,
            registry,
            catalog,
            blockers);
        ASSERT_TRUE(repeated.IsSuccess()) << repeated.GetError().c_str();
        ASSERT_EQ(repeated.GetValue().size(), lanes.size());
        for (size_t index = 0; index < lanes.size(); ++index)
        {
            EXPECT_EQ(repeated.GetValue()[index].m_lane, lanes[index].m_lane);
            EXPECT_EQ(repeated.GetValue()[index].m_state, lanes[index].m_state);
            EXPECT_EQ(
                repeated.GetValue()[index].m_blockerIds,
                lanes[index].m_blockerIds);
            EXPECT_EQ(
                repeated.GetValue()[index].m_reasons,
                lanes[index].m_reasons);
        }

        EXPECT_EQ(workspace.m_gameProfiles.size(), workspaceProfileCount);
        EXPECT_EQ(workspace.m_workspaceId, workspaceId);
        EXPECT_EQ(pack.m_packId, packId);
        EXPECT_EQ(pack.m_version, packVersion);
        EXPECT_EQ(registry.GetSources().size(), sourceCountBefore);
        EXPECT_EQ(registry.GetEvidence().size(), evidenceCountBefore);
        EXPECT_EQ(catalog.GetRecords().size(), recordCountBefore);
        EXPECT_EQ(
            catalog.GetPopulationActorProfiles().size(),
            actorProfileCountBefore);
        EXPECT_EQ(
            catalog.GetValidationHistory().size(),
            validationCountBefore);
        EXPECT_EQ(
            catalog.GetGovernanceHistory().size(),
            governanceCountBefore);
    }

    TEST(PopulationActionLaneServiceTests, UnrelatedBlockersDoNotPoisonSubjectAndInputsRemainUnchanged)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const PackManifest pack = MakePack();
        const SourceEvidenceRegistry registry = MakeRegistry();
        const CatalogDatabase catalog = MakeActorCatalog(
            workspace,
            registry,
            true,
            { "author_profile" });
        const AZStd::vector<BlockerRecord> blockers = {
            MakeBlocker(
                "blocker.other-subject",
                "subject:population:actor:other",
                "Another actor is blocked.",
                { "author_profile" }),
        };

        const size_t sourceCountBefore = registry.GetSources().size();
        const size_t evidenceCountBefore = registry.GetEvidence().size();
        const size_t recordCountBefore = catalog.GetRecords().size();
        const AZStd::vector<BlockerRecord> blockersBefore = blockers;

        PopulationActionLaneService service;
        auto result = service.BuildActionLaneMatrix(
            ActorId,
            workspace,
            WorkspaceRoot,
            &pack,
            registry,
            catalog,
            blockers);
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
        EXPECT_EQ(
            FindLane(
                result.GetValue(),
                PopulationActionLane::AuthorProfile)->m_state,
            PopulationActionLaneState::Allowed);

        EXPECT_EQ(registry.GetSources().size(), sourceCountBefore);
        EXPECT_EQ(registry.GetEvidence().size(), evidenceCountBefore);
        EXPECT_EQ(catalog.GetRecords().size(), recordCountBefore);
        ASSERT_EQ(blockers.size(), blockersBefore.size());
        EXPECT_EQ(
            blockers.front().m_blockerId,
            blockersBefore.front().m_blockerId);
        EXPECT_EQ(
            blockers.front().m_affectedUsages,
            blockersBefore.front().m_affectedUsages);
    }

    TEST(PopulationActionLaneServiceTests, MissingOrWrongSelectionFailsClosed)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const PackManifest pack = MakePack();
        const SourceEvidenceRegistry registry = MakeRegistry();
        PopulationActionLaneService service;

        CatalogDatabase catalog;
        EXPECT_FALSE(service.BuildActionLaneMatrix(
            "population.actor.missing",
            workspace,
            WorkspaceRoot,
            &pack,
            registry,
            catalog,
            {}).IsSuccess());

        CatalogRecord economy = MakePopulationRecord(
            "economy.item.wrong",
            "item",
            "subject:economy:item:wrong",
            ActorEvidence);
        economy.m_domain = "economy";
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(economy, &error)) << error.c_str();
        EXPECT_FALSE(service.BuildActionLaneMatrix(
            economy.m_recordId,
            workspace,
            WorkspaceRoot,
            &pack,
            registry,
            catalog,
            {}).IsSuccess());
    }
} // namespace TaintedGrailModdingSDK
