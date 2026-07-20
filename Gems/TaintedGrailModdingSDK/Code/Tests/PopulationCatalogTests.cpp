/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>
#include <AzTest/AzTest.h>

#include <limits>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* ProfileId = "profile.population.catalog";
        constexpr const char* GameVersion = "1.0.0-test";
        constexpr const char* Branch = "mono";
        constexpr const char* SourceId = "source.population.catalog";
        constexpr const char* SourceFingerprint =
            "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

        constexpr const char* ActorAlphaId = "population.actor.alpha";
        constexpr const char* ActorAlphaSubject =
            "subject:population:actor:alpha";
        constexpr const char* ActorAlphaEvidence =
            "evidence.population.actor.alpha";
        constexpr const char* ActorAlphaEvidenceExtra =
            "evidence.population.actor.alpha-extra";
        constexpr const char* ActorBetaId = "population.actor.beta";
        constexpr const char* ActorBetaSubject =
            "subject:population:actor:beta";
        constexpr const char* ActorBetaEvidence =
            "evidence.population.actor.beta";
        constexpr const char* TemplateAlphaId =
            "population.actor-template.alpha";
        constexpr const char* TemplateAlphaSubject =
            "subject:population:actor-template:alpha";
        constexpr const char* TemplateAlphaEvidence =
            "evidence.population.actor-template.alpha";
        constexpr const char* UnresolvedTemplateSubject =
            "subject:population:actor-template:unresolved";
        constexpr const char* UnresolvedTemplateEvidence =
            "evidence.population.actor-template.unresolved";
        constexpr const char* UnresolvedActorSubject =
            "subject:population:actor:unresolved";
        constexpr const char* UnresolvedActorEvidence =
            "evidence.population.actor.unresolved";

        constexpr const char* TroopAlphaId = "population.troop.alpha";
        constexpr const char* TroopAlphaSubject =
            "subject:population:troop:alpha";
        constexpr const char* TroopAlphaEvidence =
            "evidence.population.troop.alpha";
        constexpr const char* TroopBetaId = "population.troop.beta";
        constexpr const char* TroopBetaSubject =
            "subject:population:troop:beta";
        constexpr const char* TroopBetaEvidence =
            "evidence.population.troop.beta";

        constexpr const char* AlphaLeaderLink =
            "population.member.alpha-leader";
        constexpr const char* AlphaLeaderEvidence =
            "evidence.population.member.alpha-leader";
        constexpr const char* AlphaGuardLink =
            "population.member.alpha-guard";
        constexpr const char* AlphaGuardEvidence =
            "evidence.population.member.alpha-guard";
        constexpr const char* BetaLeaderLink =
            "population.member.beta-leader";
        constexpr const char* BetaLeaderEvidence =
            "evidence.population.member.beta-leader";

        WorkspaceModel MakeWorkspace()
        {
            GameProfile profile;
            profile.m_profileId = ProfileId;
            profile.m_displayName = "Population catalog test profile";
            profile.m_installPath = "Game";
            profile.m_gameVersion = GameVersion;
            profile.m_branch = Branch;
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "2022.3.22f1";
            profile.m_bepInExVersion = "5.4.23.3";
            profile.m_managedAssembliesPath = "Game/Managed";
            profile.m_pluginPath = "Game/BepInEx/plugins";

            WorkspaceModel workspace;
            workspace.m_workspaceId = "workspace.population.catalog";
            workspace.m_displayName = "Population catalog test workspace";
            workspace.m_rootPath = "Workspace";
            workspace.m_outputPath = "Build";
            workspace.m_stagingPath = "Staging";
            workspace.m_deploymentPath = "Deployment";
            workspace.m_activeGameProfileId = profile.m_profileId;
            workspace.m_gameProfiles.push_back(AZStd::move(profile));
            return workspace;
        }

        SourceEvidenceRegistry MakeRegistry()
        {
            SourceRecord source;
            source.m_sourceId = SourceId;
            source.m_title = "Population catalog synthetic fixture";
            source.m_sourceKind = "synthetic-fixture";
            source.m_locator = "Tests/Fixtures/population-catalog.json";
            source.m_fingerprint = SourceFingerprint;
            source.m_profileId = ProfileId;
            source.m_gameVersion = GameVersion;
            source.m_branch = Branch;
            source.m_runtimeTarget = "Mono";
            source.m_toolName = "population-catalog-tests";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "test.population.catalog";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-20T00:00:00Z";
            source.m_importedAt = "2026-07-20T00:00:02Z";
            source.m_limitations = "Project-owned synthetic test fixture.";
            source.m_mediaType = "application/json";
            source.m_byteSize = 1024;
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

            registerEvidence(
                ActorAlphaEvidence,
                ActorAlphaSubject,
                "/actors/alpha");
            registerEvidence(
                ActorAlphaEvidenceExtra,
                ActorAlphaSubject,
                "/actors/alpha/evidence-extra");
            registerEvidence(
                ActorBetaEvidence,
                ActorBetaSubject,
                "/actors/beta");
            registerEvidence(
                TemplateAlphaEvidence,
                TemplateAlphaSubject,
                "/actor-templates/alpha");
            registerEvidence(
                UnresolvedTemplateEvidence,
                UnresolvedTemplateSubject,
                "/actor-templates/unresolved");
            registerEvidence(
                UnresolvedActorEvidence,
                UnresolvedActorSubject,
                "/actors/unresolved");
            registerEvidence(
                TroopAlphaEvidence,
                TroopAlphaSubject,
                "/troops/alpha");
            registerEvidence(
                TroopBetaEvidence,
                TroopBetaSubject,
                "/troops/beta");
            registerEvidence(
                AlphaLeaderEvidence,
                "population-troop-member:population.member.alpha-leader",
                "/troops/alpha/members/leader");
            registerEvidence(
                AlphaGuardEvidence,
                "population-troop-member:population.member.alpha-guard",
                "/troops/alpha/members/guard");
            registerEvidence(
                BetaLeaderEvidence,
                "population-troop-member:population.member.beta-leader",
                "/troops/beta/members/leader");
            return registry;
        }

        CatalogRecord MakeRecord(
            AZStd::string recordId,
            AZStd::string recordKind,
            AZStd::string subject,
            AZStd::string evidenceId)
        {
            CatalogRecord record;
            record.m_recordId = AZStd::move(recordId);
            record.m_ownerPackId = "pack.population.catalog";
            record.m_domain = "population";
            record.m_recordKind = AZStd::move(recordKind);
            record.m_subjectRef = AZStd::move(subject);
            record.m_identityKind = "synthetic";
            record.m_displayName = record.m_recordId;
            record.m_researchStage = "S5";
            record.m_confidence = "documented";
            record.m_operationalRisk = "low";
            record.m_validationState = "unvalidated";
            record.m_stalenessState = "unknown";
            record.m_forbiddenUsages = {
                "no_unvalidated_runtime_use",
                "runtime_spawn",
                "save_mutation",
            };
            record.m_evidenceIds = { AZStd::move(evidenceId) };
            record.m_createdAt = "2026-07-20T00:00:00Z";
            record.m_updatedAt = "2026-07-20T00:00:00Z";
            return record;
        }

        CatalogDatabase MakeRecordCatalog()
        {
            CatalogDatabase catalog;
            AZStd::string error;
            EXPECT_TRUE(catalog.InsertNew(
                MakeRecord(
                    ActorAlphaId,
                    "actor",
                    ActorAlphaSubject,
                    ActorAlphaEvidence),
                &error)) << error.c_str();
            EXPECT_TRUE(catalog.InsertNew(
                MakeRecord(
                    ActorBetaId,
                    "actor",
                    ActorBetaSubject,
                    ActorBetaEvidence),
                &error)) << error.c_str();
            EXPECT_TRUE(catalog.InsertNew(
                MakeRecord(
                    TemplateAlphaId,
                    "actor_template",
                    TemplateAlphaSubject,
                    TemplateAlphaEvidence),
                &error)) << error.c_str();
            EXPECT_TRUE(catalog.InsertNew(
                MakeRecord(
                    TroopAlphaId,
                    "troop",
                    TroopAlphaSubject,
                    TroopAlphaEvidence),
                &error)) << error.c_str();
            EXPECT_TRUE(catalog.InsertNew(
                MakeRecord(
                    TroopBetaId,
                    "troop",
                    TroopBetaSubject,
                    TroopBetaEvidence),
                &error)) << error.c_str();
            return catalog;
        }

        PopulationActorProfile MakeActor(
            AZStd::string recordId,
            AZStd::string evidenceId)
        {
            PopulationActorProfile actor;
            actor.m_recordId = AZStd::move(recordId);
            actor.m_actorKind = "npc";
            actor.m_archetype = "synthetic_actor";
            actor.m_minimumLevel = 1;
            actor.m_maximumLevel = 20;
            actor.m_tags = { "synthetic" };
            actor.m_evidenceIds = { AZStd::move(evidenceId) };
            return actor;
        }

        PopulationTroopProfile MakeTroop(
            AZStd::string recordId,
            AZStd::string evidenceId)
        {
            PopulationTroopProfile troop;
            troop.m_recordId = AZStd::move(recordId);
            troop.m_troopKind = "patrol";
            troop.m_minimumSize = 1;
            troop.m_maximumSize = 2;
            troop.m_formation = "line";
            troop.m_tags = { "synthetic" };
            troop.m_evidenceIds = { AZStd::move(evidenceId) };
            return troop;
        }

        PopulationTroopMember MakeMember(
            AZStd::string linkId,
            AZStd::string troopRecordId,
            AZStd::string actorRecordId,
            AZStd::string actorSubject,
            AZStd::string evidenceId,
            AZStd::string role = "melee")
        {
            PopulationTroopMember member;
            member.m_linkId = AZStd::move(linkId);
            member.m_troopRecordId = AZStd::move(troopRecordId);
            member.m_actorRecordId = AZStd::move(actorRecordId);
            member.m_actorSubjectRef = AZStd::move(actorSubject);
            member.m_role = AZStd::move(role);
            member.m_minimumCount = 1;
            member.m_maximumCount = 1;
            member.m_weight = 1.0;
            member.m_conditions = { "synthetic" };
            member.m_evidenceIds = { AZStd::move(evidenceId) };
            return member;
        }

        void ExpectActorRejected(
            CatalogDatabase& catalog,
            const PopulationActorProfile& actor)
        {
            const size_t before =
                catalog.GetPopulationActorProfiles().size();
            AZStd::string error;
            EXPECT_FALSE(catalog.UpsertPopulationActorProfile(actor, &error));
            EXPECT_FALSE(error.empty());
            EXPECT_EQ(catalog.GetPopulationActorProfiles().size(), before);
        }

        void ExpectTroopRejected(
            CatalogDatabase& catalog,
            const PopulationTroopProfile& troop)
        {
            const size_t before =
                catalog.GetPopulationTroopProfiles().size();
            AZStd::string error;
            EXPECT_FALSE(catalog.UpsertPopulationTroopProfile(troop, &error));
            EXPECT_FALSE(error.empty());
            EXPECT_EQ(catalog.GetPopulationTroopProfiles().size(), before);
        }

        void ExpectMemberRejected(
            CatalogDatabase& catalog,
            const PopulationTroopMember& member)
        {
            const size_t before =
                catalog.GetPopulationTroopMembers().size();
            AZStd::string error;
            EXPECT_FALSE(catalog.UpsertPopulationTroopMember(member, &error));
            EXPECT_FALSE(error.empty());
            EXPECT_EQ(catalog.GetPopulationTroopMembers().size(), before);
        }
    } // namespace

    TEST(PopulationCatalogTests, ClosedKindsAndRolesRoundTripAndRejectUnknownValues)
    {
        const auto actorKind = ParsePopulationActorKind("creature");
        ASSERT_TRUE(actorKind.IsSuccess());
        EXPECT_EQ(actorKind.GetValue(), PopulationActorKind::Creature);
        EXPECT_EQ(ToString(actorKind.GetValue()), "creature");

        const auto troopKind = ParsePopulationTroopKind("encounter_group");
        ASSERT_TRUE(troopKind.IsSuccess());
        EXPECT_EQ(troopKind.GetValue(), PopulationTroopKind::EncounterGroup);
        EXPECT_EQ(ToString(troopKind.GetValue()), "encounter_group");

        const auto memberRole =
            ParsePopulationTroopMemberRole("specialist");
        ASSERT_TRUE(memberRole.IsSuccess());
        EXPECT_EQ(
            memberRole.GetValue(),
            PopulationTroopMemberRole::Specialist);
        EXPECT_EQ(ToString(memberRole.GetValue()), "specialist");

        EXPECT_FALSE(ParsePopulationActorKind("NPC").IsSuccess());
        EXPECT_FALSE(ParsePopulationTroopKind("spawn_group").IsSuccess());
        EXPECT_FALSE(ParsePopulationTroopMemberRole("commander").IsSuccess());
    }

    TEST(PopulationCatalogTests, ActorProfilesAcceptExactResolvedAndUnresolvedTemplates)
    {
        CatalogDatabase catalog = MakeRecordCatalog();
        PopulationActorProfile resolved =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        resolved.m_templateRecordId = TemplateAlphaId;
        resolved.m_templateSubjectRef = TemplateAlphaSubject;
        resolved.m_tags = { "zeta", "alpha" };
        resolved.m_evidenceIds = {
            TemplateAlphaEvidence,
            ActorAlphaEvidence,
        };
        const PopulationActorProfile resolvedInput = resolved;

        AZStd::string error;
        ASSERT_TRUE(catalog.UpsertPopulationActorProfile(resolved, &error))
            << error.c_str();
        const PopulationActorProfile* storedResolved =
            catalog.FindPopulationActorProfile(ActorAlphaId);
        ASSERT_NE(storedResolved, nullptr);
        ASSERT_EQ(storedResolved->m_tags.size(), 2);
        EXPECT_EQ(storedResolved->m_tags[0], "alpha");
        EXPECT_EQ(storedResolved->m_tags[1], "zeta");
        EXPECT_EQ(resolved.m_tags[0], resolvedInput.m_tags[0]);
        EXPECT_EQ(resolved.m_tags[1], resolvedInput.m_tags[1]);
        EXPECT_EQ(
            resolved.m_evidenceIds[0],
            resolvedInput.m_evidenceIds[0]);

        PopulationActorProfile unresolved =
            MakeActor(ActorBetaId, ActorBetaEvidence);
        unresolved.m_actorKind = "animal";
        unresolved.m_templateSubjectRef = UnresolvedTemplateSubject;
        unresolved.m_evidenceIds.push_back(UnresolvedTemplateEvidence);
        ASSERT_TRUE(catalog.UpsertPopulationActorProfile(unresolved, &error))
            << error.c_str();

        const WorkspaceModel workspace = MakeWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        const SourceEvidenceRegistry registry = MakeRegistry();
        EXPECT_TRUE(catalog.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error)) << error.c_str();
    }

    TEST(PopulationCatalogTests, ActorProfilesRejectKindsBindingsBoundsAndDuplicates)
    {
        CatalogDatabase catalog = MakeRecordCatalog();

        PopulationActorProfile wrongRecord =
            MakeActor(TroopAlphaId, ActorAlphaEvidence);
        ExpectActorRejected(catalog, wrongRecord);

        PopulationActorProfile unknownKind =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        unknownKind.m_actorKind = "NPC";
        ExpectActorRejected(catalog, unknownKind);

        PopulationActorProfile partialLevel =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        partialLevel.m_minimumLevel = 0;
        ExpectActorRejected(catalog, partialLevel);

        PopulationActorProfile invertedLevel =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        invertedLevel.m_minimumLevel = 20;
        invertedLevel.m_maximumLevel = 10;
        ExpectActorRejected(catalog, invertedLevel);

        PopulationActorProfile excessiveLevel =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        excessiveLevel.m_maximumLevel = 1001;
        ExpectActorRejected(catalog, excessiveLevel);

        PopulationActorProfile wrongTemplateKind =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        wrongTemplateKind.m_templateRecordId = TroopAlphaId;
        ExpectActorRejected(catalog, wrongTemplateKind);

        PopulationActorProfile conflictingTemplate =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        conflictingTemplate.m_templateRecordId = TemplateAlphaId;
        conflictingTemplate.m_templateSubjectRef =
            UnresolvedTemplateSubject;
        ExpectActorRejected(catalog, conflictingTemplate);

        PopulationActorProfile duplicateTags =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        duplicateTags.m_tags = { "duplicate", "duplicate" };
        ExpectActorRejected(catalog, duplicateTags);

        PopulationActorProfile duplicateEvidence =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        duplicateEvidence.m_evidenceIds.push_back(ActorAlphaEvidence);
        ExpectActorRejected(catalog, duplicateEvidence);
    }

    TEST(PopulationCatalogTests, TroopsAcceptExactResolvedAndUnresolvedLeaders)
    {
        CatalogDatabase catalog = MakeRecordCatalog();
        AZStd::string error;

        PopulationTroopProfile resolved =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        resolved.m_leaderActorRecordId = ActorAlphaId;
        resolved.m_leaderActorSubjectRef = ActorAlphaSubject;
        ASSERT_TRUE(catalog.UpsertPopulationTroopProfile(resolved, &error))
            << error.c_str();

        PopulationTroopProfile unresolved =
            MakeTroop(TroopBetaId, TroopBetaEvidence);
        unresolved.m_troopKind = "reinforcement";
        unresolved.m_leaderActorSubjectRef = UnresolvedActorSubject;
        ASSERT_TRUE(catalog.UpsertPopulationTroopProfile(unresolved, &error))
            << error.c_str();

        PopulationTroopMember resolvedMember = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            ActorAlphaId,
            ActorAlphaSubject,
            AlphaLeaderEvidence,
            "leader");
        ASSERT_TRUE(catalog.UpsertPopulationTroopMember(
            resolvedMember,
            &error)) << error.c_str();

        PopulationTroopMember unresolvedMember = MakeMember(
            BetaLeaderLink,
            TroopBetaId,
            {},
            UnresolvedActorSubject,
            BetaLeaderEvidence,
            "leader");
        ASSERT_TRUE(catalog.UpsertPopulationTroopMember(
            unresolvedMember,
            &error)) << error.c_str();

        const WorkspaceModel workspace = MakeWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        const SourceEvidenceRegistry registry = MakeRegistry();
        EXPECT_TRUE(catalog.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error)) << error.c_str();
    }

    TEST(PopulationCatalogTests, TroopProfilesRejectKindsLeaderBindingsAndBounds)
    {
        CatalogDatabase catalog = MakeRecordCatalog();

        PopulationTroopProfile wrongRecord =
            MakeTroop(ActorAlphaId, TroopAlphaEvidence);
        ExpectTroopRejected(catalog, wrongRecord);

        PopulationTroopProfile unknownKind =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        unknownKind.m_troopKind = "spawn_group";
        ExpectTroopRejected(catalog, unknownKind);

        PopulationTroopProfile zeroMinimum =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        zeroMinimum.m_minimumSize = 0;
        ExpectTroopRejected(catalog, zeroMinimum);

        PopulationTroopProfile invertedRange =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        invertedRange.m_minimumSize = 3;
        invertedRange.m_maximumSize = 2;
        ExpectTroopRejected(catalog, invertedRange);

        PopulationTroopProfile excessiveRange =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        excessiveRange.m_maximumSize = 1001;
        ExpectTroopRejected(catalog, excessiveRange);

        PopulationTroopProfile wrongLeaderKind =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        wrongLeaderKind.m_leaderActorRecordId = TroopBetaId;
        ExpectTroopRejected(catalog, wrongLeaderKind);

        PopulationTroopProfile conflictingLeader =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        conflictingLeader.m_leaderActorRecordId = ActorAlphaId;
        conflictingLeader.m_leaderActorSubjectRef = ActorBetaSubject;
        ExpectTroopRejected(catalog, conflictingLeader);

        PopulationTroopProfile duplicateTags =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        duplicateTags.m_tags = { "duplicate", "duplicate" };
        ExpectTroopRejected(catalog, duplicateTags);

        PopulationTroopProfile duplicateEvidence =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        duplicateEvidence.m_evidenceIds.push_back(TroopAlphaEvidence);
        ExpectTroopRejected(catalog, duplicateEvidence);
    }

    TEST(PopulationCatalogTests, MembersRejectBindingsBoundsRolesAndDuplicateConflicts)
    {
        CatalogDatabase catalog = MakeRecordCatalog();

        PopulationTroopMember missingActor = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            {},
            {},
            AlphaLeaderEvidence);
        ExpectMemberRejected(catalog, missingActor);

        PopulationTroopMember wrongActorKind = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            TroopBetaId,
            TroopBetaSubject,
            AlphaLeaderEvidence);
        ExpectMemberRejected(catalog, wrongActorKind);

        PopulationTroopMember conflictingActor = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            ActorAlphaId,
            ActorBetaSubject,
            AlphaLeaderEvidence);
        ExpectMemberRejected(catalog, conflictingActor);

        PopulationTroopMember unknownRole = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            ActorAlphaId,
            ActorAlphaSubject,
            AlphaLeaderEvidence,
            "commander");
        ExpectMemberRejected(catalog, unknownRole);

        PopulationTroopMember zeroMinimum = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            ActorAlphaId,
            ActorAlphaSubject,
            AlphaLeaderEvidence);
        zeroMinimum.m_minimumCount = 0;
        ExpectMemberRejected(catalog, zeroMinimum);

        PopulationTroopMember invertedRange = zeroMinimum;
        invertedRange.m_minimumCount = 2;
        invertedRange.m_maximumCount = 1;
        ExpectMemberRejected(catalog, invertedRange);

        PopulationTroopMember excessiveRange = zeroMinimum;
        excessiveRange.m_minimumCount = 1;
        excessiveRange.m_maximumCount = 1001;
        ExpectMemberRejected(catalog, excessiveRange);

        PopulationTroopMember negativeWeight = excessiveRange;
        negativeWeight.m_maximumCount = 1;
        negativeWeight.m_weight = -0.1;
        ExpectMemberRejected(catalog, negativeWeight);

        PopulationTroopMember infiniteWeight = negativeWeight;
        infiniteWeight.m_weight =
            std::numeric_limits<double>::infinity();
        ExpectMemberRejected(catalog, infiniteWeight);

        PopulationTroopMember notANumberWeight = negativeWeight;
        notANumberWeight.m_weight =
            std::numeric_limits<double>::quiet_NaN();
        ExpectMemberRejected(catalog, notANumberWeight);

        PopulationTroopMember duplicateConditions = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            ActorAlphaId,
            ActorAlphaSubject,
            AlphaLeaderEvidence);
        duplicateConditions.m_conditions = { "rain", "rain" };
        ExpectMemberRejected(catalog, duplicateConditions);

        PopulationTroopMember duplicateEvidence = duplicateConditions;
        duplicateEvidence.m_conditions = { "rain" };
        duplicateEvidence.m_evidenceIds.push_back(AlphaLeaderEvidence);
        ExpectMemberRejected(catalog, duplicateEvidence);

        PopulationTroopMember leader = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            ActorAlphaId,
            ActorAlphaSubject,
            AlphaLeaderEvidence,
            "leader");
        AZStd::string error;
        ASSERT_TRUE(catalog.UpsertPopulationTroopMember(leader, &error))
            << error.c_str();

        PopulationTroopMember duplicateActor = MakeMember(
            AlphaGuardLink,
            TroopAlphaId,
            {},
            ActorAlphaSubject,
            AlphaGuardEvidence,
            "melee");
        ExpectMemberRejected(catalog, duplicateActor);

        PopulationTroopMember secondLeader = MakeMember(
            AlphaGuardLink,
            TroopAlphaId,
            ActorBetaId,
            ActorBetaSubject,
            AlphaGuardEvidence,
            "leader");
        ExpectMemberRejected(catalog, secondLeader);
    }

    TEST(PopulationCatalogTests, CollectionsAreCanonicalAndDoNotMutateInputs)
    {
        CatalogDatabase catalog = MakeRecordCatalog();
        AZStd::string error;

        PopulationActorProfile beta =
            MakeActor(ActorBetaId, ActorBetaEvidence);
        PopulationActorProfile alpha =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        alpha.m_tags = { "zeta", "alpha" };
        alpha.m_evidenceIds = {
            ActorAlphaEvidenceExtra,
            ActorAlphaEvidence,
        };
        ASSERT_TRUE(catalog.UpsertPopulationActorProfile(beta, &error))
            << error.c_str();
        ASSERT_TRUE(catalog.UpsertPopulationActorProfile(alpha, &error))
            << error.c_str();
        EXPECT_EQ(alpha.m_tags[0], "zeta");
        EXPECT_EQ(alpha.m_evidenceIds[0], ActorAlphaEvidenceExtra);

        PopulationTroopProfile betaTroop =
            MakeTroop(TroopBetaId, TroopBetaEvidence);
        PopulationTroopProfile alphaTroop =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        alphaTroop.m_tags = { "zeta", "alpha" };
        ASSERT_TRUE(catalog.UpsertPopulationTroopProfile(betaTroop, &error))
            << error.c_str();
        ASSERT_TRUE(catalog.UpsertPopulationTroopProfile(alphaTroop, &error))
            << error.c_str();
        EXPECT_EQ(alphaTroop.m_tags[0], "zeta");

        PopulationTroopMember betaMember = MakeMember(
            BetaLeaderLink,
            TroopBetaId,
            ActorBetaId,
            ActorBetaSubject,
            BetaLeaderEvidence);
        PopulationTroopMember alphaMember = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            ActorAlphaId,
            ActorAlphaSubject,
            AlphaLeaderEvidence);
        alphaMember.m_conditions = { "rain", "morning" };
        alphaMember.m_evidenceIds = {
            ActorAlphaEvidence,
            AlphaLeaderEvidence,
        };
        ASSERT_TRUE(catalog.UpsertPopulationTroopMember(betaMember, &error))
            << error.c_str();
        ASSERT_TRUE(catalog.UpsertPopulationTroopMember(alphaMember, &error))
            << error.c_str();
        EXPECT_EQ(alphaMember.m_conditions[0], "rain");
        EXPECT_EQ(alphaMember.m_evidenceIds[0], ActorAlphaEvidence);

        ASSERT_EQ(catalog.GetPopulationActorProfiles().size(), 2);
        EXPECT_EQ(
            catalog.GetPopulationActorProfiles()[0].m_recordId,
            ActorAlphaId);
        EXPECT_EQ(
            catalog.GetPopulationActorProfiles()[1].m_recordId,
            ActorBetaId);
        EXPECT_EQ(
            catalog.GetPopulationActorProfiles()[0].m_tags[0],
            "alpha");
        ASSERT_EQ(catalog.GetPopulationTroopProfiles().size(), 2);
        EXPECT_EQ(
            catalog.GetPopulationTroopProfiles()[0].m_recordId,
            TroopAlphaId);
        EXPECT_EQ(
            catalog.GetPopulationTroopProfiles()[1].m_recordId,
            TroopBetaId);
        ASSERT_EQ(catalog.GetPopulationTroopMembers().size(), 2);
        EXPECT_EQ(
            catalog.GetPopulationTroopMembers()[0].m_linkId,
            AlphaLeaderLink);
        EXPECT_EQ(
            catalog.GetPopulationTroopMembers()[1].m_linkId,
            BetaLeaderLink);
        EXPECT_EQ(
            catalog.GetPopulationTroopMembers()[0].m_conditions[0],
            "morning");

        const WorkspaceModel workspace = MakeWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        const CatalogDocument document =
            catalog.BuildDocument(workspace, *profile);
        EXPECT_EQ(document.m_actorProfiles[0].m_recordId, ActorAlphaId);
        EXPECT_EQ(document.m_troopProfiles[0].m_recordId, TroopAlphaId);
        EXPECT_EQ(document.m_troopMembers[0].m_linkId, AlphaLeaderLink);
    }

    TEST(PopulationCatalogTests, CompleteCatalogIntegrityAcceptsExactComposition)
    {
        CatalogDatabase catalog = MakeRecordCatalog();
        AZStd::string error;

        PopulationActorProfile alpha =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        PopulationActorProfile beta =
            MakeActor(ActorBetaId, ActorBetaEvidence);
        ASSERT_TRUE(catalog.UpsertPopulationActorProfile(alpha, &error))
            << error.c_str();
        ASSERT_TRUE(catalog.UpsertPopulationActorProfile(beta, &error))
            << error.c_str();

        PopulationTroopProfile troop =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        troop.m_leaderActorRecordId = ActorAlphaId;
        troop.m_leaderActorSubjectRef = ActorAlphaSubject;
        troop.m_minimumSize = 2;
        troop.m_maximumSize = 2;
        ASSERT_TRUE(catalog.UpsertPopulationTroopProfile(troop, &error))
            << error.c_str();

        PopulationTroopMember leader = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            ActorAlphaId,
            ActorAlphaSubject,
            AlphaLeaderEvidence,
            "leader");
        leader.m_required = true;
        PopulationTroopMember guard = MakeMember(
            AlphaGuardLink,
            TroopAlphaId,
            ActorBetaId,
            ActorBetaSubject,
            AlphaGuardEvidence,
            "melee");
        ASSERT_TRUE(catalog.UpsertPopulationTroopMember(guard, &error))
            << error.c_str();
        ASSERT_TRUE(catalog.UpsertPopulationTroopMember(leader, &error))
            << error.c_str();

        const WorkspaceModel workspace = MakeWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        const SourceEvidenceRegistry registry = MakeRegistry();
        EXPECT_TRUE(catalog.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error)) << error.c_str();

        const auto members =
            catalog.FindPopulationMembersForTroop(TroopAlphaId);
        ASSERT_EQ(members.size(), 2);
        EXPECT_EQ(members[0].m_linkId, AlphaGuardLink);
        EXPECT_EQ(members[1].m_linkId, AlphaLeaderLink);
    }

    TEST(PopulationCatalogTests, CompleteCatalogIntegrityRejectsEvidenceAndCompositionGaps)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        const SourceEvidenceRegistry registry = MakeRegistry();
        AZStd::string error;

        CatalogDatabase wrongEvidenceCatalog = MakeRecordCatalog();
        PopulationActorProfile wrongEvidence =
            MakeActor(ActorAlphaId, TroopAlphaEvidence);
        ASSERT_TRUE(wrongEvidenceCatalog.UpsertPopulationActorProfile(
            wrongEvidence,
            &error)) << error.c_str();
        EXPECT_FALSE(wrongEvidenceCatalog.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error));
        EXPECT_NE(error.find("evidence"), AZStd::string::npos);

        CatalogDatabase missingMemberCatalog = MakeRecordCatalog();
        PopulationTroopProfile missingMemberTroop =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        ASSERT_TRUE(missingMemberCatalog.UpsertPopulationTroopProfile(
            missingMemberTroop,
            &error)) << error.c_str();
        EXPECT_FALSE(missingMemberCatalog.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error));
        EXPECT_NE(error.find("at least one typed member"), AZStd::string::npos);

        CatalogDatabase sizeGapCatalog = MakeRecordCatalog();
        PopulationTroopProfile sizeGapTroop =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        sizeGapTroop.m_minimumSize = 2;
        sizeGapTroop.m_maximumSize = 2;
        ASSERT_TRUE(sizeGapCatalog.UpsertPopulationTroopProfile(
            sizeGapTroop,
            &error)) << error.c_str();
        PopulationTroopMember singleMember = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            ActorAlphaId,
            ActorAlphaSubject,
            AlphaLeaderEvidence);
        ASSERT_TRUE(sizeGapCatalog.UpsertPopulationTroopMember(
            singleMember,
            &error)) << error.c_str();
        EXPECT_FALSE(sizeGapCatalog.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error));
        EXPECT_NE(error.find("size range"), AZStd::string::npos);

        CatalogDatabase leaderGapCatalog = MakeRecordCatalog();
        PopulationTroopProfile leaderGapTroop =
            MakeTroop(TroopAlphaId, TroopAlphaEvidence);
        leaderGapTroop.m_leaderActorRecordId = ActorBetaId;
        leaderGapTroop.m_leaderActorSubjectRef = ActorBetaSubject;
        ASSERT_TRUE(leaderGapCatalog.UpsertPopulationTroopProfile(
            leaderGapTroop,
            &error)) << error.c_str();
        PopulationTroopMember wrongLeader = MakeMember(
            AlphaLeaderLink,
            TroopAlphaId,
            ActorAlphaId,
            ActorAlphaSubject,
            AlphaLeaderEvidence,
            "leader");
        ASSERT_TRUE(leaderGapCatalog.UpsertPopulationTroopMember(
            wrongLeader,
            &error)) << error.c_str();
        EXPECT_FALSE(leaderGapCatalog.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error));
        EXPECT_NE(error.find("matching leader"), AZStd::string::npos);
    }

    TEST(PopulationCatalogTests, PopulationUpsertsDoNotGrantGovernanceOrActionAuthority)
    {
        CatalogDatabase catalog = MakeRecordCatalog();
        const CatalogRecord* before = catalog.FindByRecordId(ActorAlphaId);
        ASSERT_NE(before, nullptr);
        const AZStd::string validationState = before->m_validationState;
        const AZStd::string stalenessState = before->m_stalenessState;
        const AZStd::vector<AZStd::string> allowedUsages =
            before->m_allowedUsages;
        const AZStd::vector<AZStd::string> forbiddenUsages =
            before->m_forbiddenUsages;
        const size_t governanceCount =
            catalog.GetGovernanceHistory().size();
        const size_t validationCount =
            catalog.GetValidationHistory().size();
        const size_t relationshipCount = catalog.GetRelationships().size();

        PopulationActorProfile actor =
            MakeActor(ActorAlphaId, ActorAlphaEvidence);
        AZStd::string error;
        ASSERT_TRUE(catalog.UpsertPopulationActorProfile(actor, &error))
            << error.c_str();

        const CatalogRecord* after = catalog.FindByRecordId(ActorAlphaId);
        ASSERT_NE(after, nullptr);
        EXPECT_EQ(after->m_validationState, validationState);
        EXPECT_EQ(after->m_stalenessState, stalenessState);
        EXPECT_EQ(after->m_allowedUsages, allowedUsages);
        EXPECT_EQ(after->m_forbiddenUsages, forbiddenUsages);
        EXPECT_TRUE(after->m_allowedUsages.empty());
        EXPECT_NE(
            AZStd::find(
                after->m_forbiddenUsages.begin(),
                after->m_forbiddenUsages.end(),
                "runtime_spawn"),
            after->m_forbiddenUsages.end());
        EXPECT_NE(
            AZStd::find(
                after->m_forbiddenUsages.begin(),
                after->m_forbiddenUsages.end(),
                "save_mutation"),
            after->m_forbiddenUsages.end());
        EXPECT_EQ(catalog.GetGovernanceHistory().size(), governanceCount);
        EXPECT_EQ(catalog.GetValidationHistory().size(), validationCount);
        EXPECT_EQ(catalog.GetRelationships().size(), relationshipCount);
    }
} // namespace TaintedGrailModdingSDK
