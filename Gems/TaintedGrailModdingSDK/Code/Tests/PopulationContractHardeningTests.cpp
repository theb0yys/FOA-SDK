/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"
#include "PopulationAuthoringService.h"
#include "PopulationEvidenceValidation.h"

#include <AzCore/std/utility/move.h>
#include <AzTest/AzTest.h>

#include <QByteArray>
#include <QDir>
#include <QTemporaryDir>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* SourceId = "source.population.contract-hardening";
        constexpr const char* SourceFingerprint =
            "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        constexpr const char* LeaderId = "population.actor.contract-leader";
        constexpr const char* LeaderSubject =
            "subject:population:actor:contract-leader";
        constexpr const char* GuardId = "population.actor.contract-guard";
        constexpr const char* GuardSubject =
            "subject:population:actor:contract-guard";
        constexpr const char* TroopId = "population.troop.contract-patrol";
        constexpr const char* TroopSubject =
            "subject:population:troop:contract-patrol";
        constexpr const char* LeaderMemberId =
            "population.member.contract-leader";
        constexpr const char* GuardMemberId =
            "population.member.contract-guard";

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray bytes = value.toUtf8();
            return { bytes.constData(), static_cast<size_t>(bytes.size()) };
        }

        GameProfile MakeProfile()
        {
            GameProfile profile;
            profile.m_profileId = "profile.population.contract-hardening";
            profile.m_displayName = "Population contract-hardening profile";
            profile.m_installPath = "Game";
            profile.m_gameVersion = "1.0.0";
            profile.m_branch = "mono";
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "2022.3.22f1";
            profile.m_bepInExVersion = "5.4.23.3";
            profile.m_managedAssembliesPath = "Game/Managed";
            profile.m_pluginPath = "Game/BepInEx/plugins";
            return profile;
        }

        WorkspaceModel MakeWorkspace(const AZStd::string& root)
        {
            WorkspaceModel workspace;
            workspace.m_workspaceId = "workspace.population.contract-hardening";
            workspace.m_displayName = "Population contract-hardening workspace";
            workspace.m_rootPath = root;
            workspace.m_outputPath = "Build";
            workspace.m_stagingPath = "Staging";
            workspace.m_deploymentPath = "Deployment";
            workspace.m_activeGameProfileId =
                "profile.population.contract-hardening";
            workspace.m_gameProfiles.push_back(MakeProfile());
            return workspace;
        }

        PackManifest MakePack()
        {
            PackManifest pack;
            pack.m_packId = "pack.population.contract-hardening";
            pack.m_displayName = "Population contract-hardening pack";
            pack.m_ownerId = "population-contract-owner";
            pack.m_version = "1.0.0";
            pack.m_targetGameVersion = "1.0.0";
            pack.m_targetBranch = "mono";
            pack.m_runtimeActionsEnabled = false;
            return pack;
        }

        CatalogRecord MakeRecord(
            AZStd::string recordId,
            AZStd::string kind,
            AZStd::string subject,
            AZStd::string evidenceId)
        {
            CatalogRecord record;
            record.m_recordId = AZStd::move(recordId);
            record.m_ownerPackId = "pack.population.contract-hardening";
            record.m_domain = "population";
            record.m_recordKind = AZStd::move(kind);
            record.m_subjectRef = AZStd::move(subject);
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

        SourceRecord MakeSource(const GameProfile& profile)
        {
            SourceRecord source;
            source.m_sourceId = SourceId;
            source.m_title = "Population contract-hardening fixture";
            source.m_sourceKind = "synthetic-fixture";
            source.m_locator = "Tests/Fixtures/population-contract-hardening.json";
            source.m_fingerprint = SourceFingerprint;
            source.m_profileId = profile.m_profileId;
            source.m_gameVersion = profile.m_gameVersion;
            source.m_branch = profile.m_branch;
            source.m_runtimeTarget = profile.m_runtimeTarget;
            source.m_toolName = "population-contract-tests";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "test.population.contract-hardening";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-20T00:00:00Z";
            source.m_importedAt = "2026-07-20T00:00:02Z";
            source.m_limitations = "Project-owned synthetic fixture.";
            source.m_mediaType = "application/json";
            source.m_byteSize = 1024;
            source.m_importStatus = "imported";
            return source;
        }

        EvidenceRecord MakeEvidence(
            const GameProfile& profile,
            AZStd::string evidenceId,
            AZStd::string subject,
            AZStd::string path)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = AZStd::move(evidenceId);
            evidence.m_sourceId = SourceId;
            evidence.m_sourceFingerprint = SourceFingerprint;
            evidence.m_profileId = profile.m_profileId;
            evidence.m_gameVersion = profile.m_gameVersion;
            evidence.m_branch = profile.m_branch;
            evidence.m_subjectRef = AZStd::move(subject);
            evidence.m_claim = "Synthetic exact population binding.";
            evidence.m_evidenceKind = "structured_record";
            evidence.m_confidence = "documented";
            evidence.m_locator = "Tests/Fixtures/population-contract-hardening.json";
            evidence.m_recordPath = AZStd::move(path);
            evidence.m_extractedAt = "2026-07-20T00:00:01Z";
            return evidence;
        }

        void RegisterEvidence(
            SourceEvidenceRegistry& registry,
            const GameProfile& profile,
            const AZStd::string& evidenceId,
            const AZStd::string& subject,
            const AZStd::string& path)
        {
            AZStd::string error;
            EXPECT_TRUE(registry.RegisterEvidence(
                MakeEvidence(profile, evidenceId, subject, path),
                &error)) << error.c_str();
        }

        SourceEvidenceRegistry MakeRegistry(const GameProfile& profile)
        {
            SourceEvidenceRegistry registry;
            AZStd::string error;
            EXPECT_TRUE(registry.RegisterSource(MakeSource(profile), &error))
                << error.c_str();
            RegisterEvidence(
                registry,
                profile,
                "evidence.population.contract-leader",
                LeaderSubject,
                "/actors/leader");
            RegisterEvidence(
                registry,
                profile,
                "evidence.population.contract-guard",
                GuardSubject,
                "/actors/guard");
            RegisterEvidence(
                registry,
                profile,
                "evidence.population.contract-troop",
                TroopSubject,
                "/troops/patrol");
            RegisterEvidence(
                registry,
                profile,
                "evidence.population.contract-leader-member",
                "population-troop-member:population.member.contract-leader",
                "/troops/patrol/members/leader");
            RegisterEvidence(
                registry,
                profile,
                "evidence.population.contract-guard-member",
                "population-troop-member:population.member.contract-guard",
                "/troops/patrol/members/guard");
            return registry;
        }

        CatalogDatabase MakeCatalog()
        {
            CatalogDatabase catalog;
            AZStd::string error;
            EXPECT_TRUE(catalog.InsertNew(
                MakeRecord(
                    LeaderId,
                    "actor",
                    LeaderSubject,
                    "evidence.population.contract-leader"),
                &error)) << error.c_str();
            EXPECT_TRUE(catalog.InsertNew(
                MakeRecord(
                    GuardId,
                    "actor",
                    GuardSubject,
                    "evidence.population.contract-guard"),
                &error)) << error.c_str();
            EXPECT_TRUE(catalog.InsertNew(
                MakeRecord(
                    TroopId,
                    "troop",
                    TroopSubject,
                    "evidence.population.contract-troop"),
                &error)) << error.c_str();
            return catalog;
        }

        PopulationActorProfile MakeActor(
            const AZStd::string& recordId,
            const AZStd::string& evidenceId)
        {
            PopulationActorProfile actor;
            actor.m_recordId = recordId;
            actor.m_actorKind = "npc";
            actor.m_archetype = "contract-test";
            actor.m_minimumLevel = 1;
            actor.m_maximumLevel = 10;
            actor.m_evidenceIds = { evidenceId };
            return actor;
        }

        PopulationTroopMember MakeMember(
            AZStd::string linkId,
            AZStd::string actorId,
            AZStd::string actorSubject,
            AZStd::string role,
            bool required,
            AZ::u32 minimum,
            AZStd::string evidenceId)
        {
            PopulationTroopMember member;
            member.m_linkId = AZStd::move(linkId);
            member.m_troopRecordId = TroopId;
            member.m_actorRecordId = AZStd::move(actorId);
            member.m_actorSubjectRef = AZStd::move(actorSubject);
            member.m_role = AZStd::move(role);
            member.m_minimumCount = minimum;
            member.m_maximumCount = 1;
            member.m_weight = 1.0;
            member.m_required = required;
            member.m_evidenceIds = { AZStd::move(evidenceId) };
            return member;
        }
    } // namespace

    TEST(PopulationContractHardeningTests, EveryRequiredSubjectMustHaveEvidence)
    {
        const GameProfile profile = MakeProfile();
        SourceEvidenceRegistry registry = MakeRegistry(profile);
        AZStd::string error;

        EXPECT_FALSE(ValidatePopulationEvidenceCoverage(
            { "evidence.population.contract-leader" },
            { LeaderSubject, TroopSubject },
            profile,
            registry,
            "Population contract test",
            error));
        EXPECT_NE(error.find("missing evidence for exact subject"), AZStd::string::npos);

        EXPECT_TRUE(ValidatePopulationEvidenceCoverage(
            {
                "evidence.population.contract-leader",
                "evidence.population.contract-troop",
            },
            { LeaderSubject, TroopSubject },
            profile,
            registry,
            "Population contract test",
            error)) << error.c_str();
    }

    TEST(PopulationContractHardeningTests, OptionalMemberMayUseZeroMinimum)
    {
        CatalogDatabase catalog = MakeCatalog();
        PopulationTroopMember optional = MakeMember(
            GuardMemberId,
            GuardId,
            GuardSubject,
            "support",
            false,
            0,
            "evidence.population.contract-guard-member");
        AZStd::string error;

        EXPECT_TRUE(catalog.ValidatePopulationTroopMember(optional, &error))
            << error.c_str();
        optional.m_required = true;
        EXPECT_FALSE(catalog.ValidatePopulationTroopMember(optional, &error));
        EXPECT_NE(error.find("positive minimum for required rows"), AZStd::string::npos);
    }

    TEST(PopulationContractHardeningTests, OptionalMinimumDoesNotRaiseTroopRequirement)
    {
        const WorkspaceModel workspace = MakeWorkspace("Workspace");
        const GameProfile& profile = workspace.m_gameProfiles.front();
        SourceEvidenceRegistry registry = MakeRegistry(profile);
        CatalogDatabase catalog = MakeCatalog();
        AZStd::string error;

        EXPECT_TRUE(catalog.UpsertPopulationActorProfile(
            MakeActor(
                LeaderId,
                "evidence.population.contract-leader"),
            &error)) << error.c_str();
        EXPECT_TRUE(catalog.UpsertPopulationActorProfile(
            MakeActor(
                GuardId,
                "evidence.population.contract-guard"),
            &error)) << error.c_str();

        PopulationTroopProfile troop;
        troop.m_recordId = TroopId;
        troop.m_troopKind = "patrol";
        troop.m_leaderActorRecordId = LeaderId;
        troop.m_leaderActorSubjectRef = LeaderSubject;
        troop.m_minimumSize = 1;
        troop.m_maximumSize = 1;
        troop.m_evidenceIds = { "evidence.population.contract-troop" };
        EXPECT_TRUE(catalog.UpsertPopulationTroopProfile(troop, &error))
            << error.c_str();

        EXPECT_TRUE(catalog.UpsertPopulationTroopMember(
            MakeMember(
                LeaderMemberId,
                LeaderId,
                LeaderSubject,
                "leader",
                true,
                1,
                "evidence.population.contract-leader-member"),
            &error)) << error.c_str();
        EXPECT_TRUE(catalog.UpsertPopulationTroopMember(
            MakeMember(
                GuardMemberId,
                GuardId,
                GuardSubject,
                "support",
                false,
                1,
                "evidence.population.contract-guard-member"),
            &error)) << error.c_str();

        EXPECT_TRUE(catalog.ValidateIntegrity(
            workspace,
            profile,
            registry,
            &error)) << error.c_str();
    }

    TEST(PopulationContractHardeningTests, PublicAuthoringRejectsUnvalidatedRoot)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string missingRoot = ToAzString(
            QDir(temporary.path()).filePath(QStringLiteral("missing-root")));
        const WorkspaceModel workspace = MakeWorkspace(missingRoot);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        PopulationAuthoringService service;
        const auto result = service.BuildActorProfileCandidate(
            MakeActor(
                LeaderId,
                "evidence.population.contract-leader"),
            missingRoot,
            workspace,
            profile,
            pack,
            SourceEvidenceRegistry{},
            CatalogDatabase{});

        ASSERT_FALSE(result.IsSuccess());
        EXPECT_NE(
            result.GetError().find("path-policy-validated canonical workspace root"),
            AZStd::string::npos);
    }
} // namespace TaintedGrailModdingSDK
