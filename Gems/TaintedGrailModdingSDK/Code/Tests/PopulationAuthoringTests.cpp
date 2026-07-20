/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationNotificationBus.h"
#include "FoundationService.h"
#include "PopulationAuthoringService.h"

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzTest/AzTest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* SourceId = "population.source.fixture";
        constexpr const char* SourceFingerprint =
            "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray bytes = value.toUtf8();
            return { bytes.constData(), static_cast<size_t>(bytes.size()) };
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(
                value.c_str(),
                static_cast<int>(value.size()));
        }

        template<class Value>
        void ReflectIfMissing(AZ::SerializeContext& context)
        {
            if (!context.FindClassData(azrtti_typeid<Value>()))
            {
                Value::Reflect(&context);
            }
        }

        bool EnsureCatalogReflection()
        {
            AZ::SerializeContext* context = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(
                context,
                &AZ::ComponentApplicationBus::Events::GetSerializeContext);
            if (!context)
            {
                return false;
            }
            ReflectIfMissing<CatalogRecord>(*context);
            ReflectIfMissing<CatalogRelationship>(*context);
            ReflectIfMissing<CatalogValidationEvent>(*context);
            ReflectIfMissing<CatalogGovernanceEvent>(*context);
            ReflectIfMissing<EconomyItemProfile>(*context);
            ReflectIfMissing<EconomyRecipeProfile>(*context);
            ReflectIfMissing<EconomyRecipeIngredient>(*context);
            ReflectIfMissing<EconomyRecipeOutput>(*context);
            ReflectIfMissing<CatalogDocument>(*context);
            return true;
        }

        GameProfile MakeProfile(
            AZStd::string profileId = "population.profile",
            AZStd::string gameVersion = "1.0.0")
        {
            GameProfile profile;
            profile.m_profileId = AZStd::move(profileId);
            profile.m_displayName = "Population test profile";
            profile.m_installPath = "Game";
            profile.m_gameVersion = AZStd::move(gameVersion);
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
            workspace.m_workspaceId = "population.workspace";
            workspace.m_displayName = "Population workspace";
            workspace.m_rootPath = root;
            workspace.m_outputPath = "Build";
            workspace.m_stagingPath = "Staging";
            workspace.m_deploymentPath = "Deployment";
            workspace.m_activeGameProfileId = "population.profile";
            workspace.m_gameProfiles.push_back(MakeProfile());
            return workspace;
        }

        PackManifest MakePack(
            AZStd::string packId = "population.pack.active")
        {
            PackManifest pack;
            pack.m_packId = AZStd::move(packId);
            pack.m_displayName = "Population test pack";
            pack.m_ownerId = "population-owner";
            pack.m_version = "1.0.0";
            pack.m_targetGameVersion = "1.0.0";
            pack.m_targetBranch = "mono";
            pack.m_runtimeActionsEnabled = false;
            return pack;
        }

        SourceRecord MakeSource(const GameProfile& profile)
        {
            SourceRecord source;
            source.m_sourceId = SourceId;
            source.m_title = "Population authoring fixture";
            source.m_sourceKind = "synthetic-fixture";
            source.m_locator = "Sources/population-authoring.json";
            source.m_fingerprint = SourceFingerprint;
            source.m_profileId = profile.m_profileId;
            source.m_gameVersion = profile.m_gameVersion;
            source.m_branch = profile.m_branch;
            source.m_runtimeTarget = profile.m_runtimeTarget;
            source.m_toolName = "fixture";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "population.fixture-importer";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-20T12:00:00Z";
            source.m_importedAt = "2026-07-20T12:01:00Z";
            source.m_limitations = "Project-owned synthetic authoring data.";
            source.m_mediaType = "application/json";
            source.m_byteSize = 1024;
            source.m_importStatus = "imported";
            return source;
        }

        EvidenceRecord MakeEvidence(
            const GameProfile& profile,
            AZStd::string evidenceId,
            AZStd::string subjectRef)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = AZStd::move(evidenceId);
            evidence.m_sourceId = SourceId;
            evidence.m_sourceFingerprint = SourceFingerprint;
            evidence.m_profileId = profile.m_profileId;
            evidence.m_gameVersion = profile.m_gameVersion;
            evidence.m_branch = profile.m_branch;
            evidence.m_subjectRef = AZStd::move(subjectRef);
            evidence.m_claim = "Synthetic exact-subject population evidence.";
            evidence.m_evidenceKind = "structured_record";
            evidence.m_confidence = "documented";
            evidence.m_locator = "Sources/population-authoring.json";
            evidence.m_recordPath = "/records/0";
            evidence.m_extractedAt = "2026-07-20T12:00:30Z";
            return evidence;
        }

        CatalogRecord MakePopulationRecord(
            AZStd::string recordId,
            AZStd::string recordKind,
            AZStd::string subjectRef,
            AZStd::string evidenceId,
            const AZStd::string& ownerPackId)
        {
            CatalogRecord record;
            record.m_recordId = AZStd::move(recordId);
            record.m_ownerPackId = ownerPackId;
            record.m_domain = "population";
            record.m_recordKind = AZStd::move(recordKind);
            record.m_subjectRef = AZStd::move(subjectRef);
            record.m_identityKind = ownerPackId.empty()
                ? "native"
                : "synthetic";
            if (ownerPackId.empty())
            {
                record.m_nativeRefExact = "native:" + record.m_recordId;
            }
            record.m_displayName = record.m_recordId;
            record.m_researchStage = "reviewed";
            record.m_confidence = "documented";
            record.m_operationalRisk = "unknown";
            record.m_validationState = "unvalidated";
            record.m_stalenessState = "unknown";
            record.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
            record.m_evidenceIds = { AZStd::move(evidenceId) };
            return record;
        }

        bool RegisterEvidence(
            FoundationService& service,
            const GameProfile& profile,
            const AZStd::vector<EvidenceRecord>& evidence,
            AZStd::string& error)
        {
            if (!service.RegisterSource(MakeSource(profile), &error))
            {
                return false;
            }
            for (const EvidenceRecord& record : evidence)
            {
                if (!service.RegisterEvidence(record, &error))
                {
                    return false;
                }
            }
            return true;
        }

        bool PromoteSyntheticRecord(
            FoundationService& service,
            const AZStd::string& evidenceId,
            const AZStd::string& recordId,
            const AZStd::string& recordKind,
            const AZStd::string& subjectRef,
            const AZStd::string& ownerPackId,
            AZStd::string& error)
        {
            CatalogPromotionRequest request;
            request.m_evidenceId = evidenceId;
            request.m_recordId = recordId;
            request.m_ownerPackId = ownerPackId;
            request.m_domain = "population";
            request.m_recordKind = recordKind;
            request.m_subjectRef = subjectRef;
            request.m_identityKind = "synthetic";
            request.m_displayName = recordId;
            return service.PromoteEvidenceToCatalog(request, &error);
        }

        PopulationActorProfile MakeActorProfile(
            AZStd::string recordId,
            AZStd::string evidenceId)
        {
            PopulationActorProfile actor;
            actor.m_recordId = AZStd::move(recordId);
            actor.m_actorKind = "npc";
            actor.m_archetype = "population-test-actor";
            actor.m_minimumLevel = 1;
            actor.m_maximumLevel = 10;
            actor.m_tags = { "zeta", "alpha" };
            actor.m_evidenceIds = { AZStd::move(evidenceId) };
            return actor;
        }

        PopulationTroopDefinition MakeTroopDefinition(
            AZStd::string actorRecordId,
            AZStd::string actorSubjectRef,
            AZStd::string actorEvidenceId,
            AZStd::string troopRecordId,
            AZStd::string troopEvidenceId)
        {
            PopulationTroopDefinition definition;
            definition.m_profile.m_recordId = AZStd::move(troopRecordId);
            definition.m_profile.m_troopKind = "patrol";
            definition.m_profile.m_leaderActorRecordId = actorRecordId;
            definition.m_profile.m_leaderActorSubjectRef = actorSubjectRef;
            definition.m_profile.m_minimumSize = 1;
            definition.m_profile.m_maximumSize = 1;
            definition.m_profile.m_formation = "single-file";
            definition.m_profile.m_evidenceIds = {
                AZStd::move(troopEvidenceId),
            };

            PopulationTroopMember member;
            member.m_linkId = "population.member.patrol-leader";
            member.m_troopRecordId = definition.m_profile.m_recordId;
            member.m_actorRecordId = AZStd::move(actorRecordId);
            member.m_actorSubjectRef = AZStd::move(actorSubjectRef);
            member.m_role = "leader";
            member.m_minimumCount = 1;
            member.m_maximumCount = 1;
            member.m_weight = 1.0;
            member.m_required = true;
            member.m_conditions = { "rain", "morning" };
            member.m_evidenceIds = { AZStd::move(actorEvidenceId) };
            definition.m_members.push_back(AZStd::move(member));
            return definition;
        }

        class FoundationChangeCounter final
            : public FoundationNotificationBus::Handler
        {
        public:
            void OnFoundationChanged() override
            {
                ++m_count;
            }

            int m_count = 0;
        };
    } // namespace

    TEST(PopulationAuthoringTests, ActorProfilePersistsAndNotifiesOnce)
    {
        ASSERT_TRUE(EnsureCatalogReflection());
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        service.Initialize();
        service.SetWorkspace(workspace);
        AZStd::string error;
        ASSERT_TRUE(service.SetActivePack(pack, &error)) << error.c_str();
        ASSERT_TRUE(RegisterEvidence(
            service,
            profile,
            { MakeEvidence(
                profile,
                "population.evidence.actor",
                "FoA/Actors/PreviewLeader") },
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor",
            "population.actor.preview-leader",
            "actor",
            "FoA/Actors/PreviewLeader",
            pack.m_packId,
            error)) << error.c_str();

        FoundationChangeCounter counter;
        counter.BusConnect();
        const PopulationActorProfile actor = MakeActorProfile(
            "population.actor.preview-leader",
            "population.evidence.actor");
        EXPECT_TRUE(service.UpsertPopulationActorProfile(actor, &error))
            << error.c_str();
        EXPECT_EQ(counter.m_count, 1);
        counter.BusDisconnect();

        const PopulationActorProfile* stored =
            service.GetCatalog().FindPopulationActorProfile(actor.m_recordId);
        ASSERT_NE(stored, nullptr);
        ASSERT_EQ(stored->m_tags.size(), 2);
        EXPECT_EQ(stored->m_tags.front(), "alpha");
        EXPECT_FALSE(service.GetCatalogFilePath().empty());

        CatalogSchemaPersistenceService persistence;
        auto loaded = persistence.Load(root);
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        ASSERT_EQ(loaded.GetValue().m_actorProfiles.size(), 1);
        EXPECT_EQ(
            loaded.GetValue().m_actorProfiles.front().m_recordId,
            actor.m_recordId);
    }

    TEST(PopulationAuthoringTests, TroopDefinitionIsAtomicAndReplacesMemberSet)
    {
        ASSERT_TRUE(EnsureCatalogReflection());
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        service.Initialize();
        service.SetWorkspace(workspace);
        AZStd::string error;
        ASSERT_TRUE(service.SetActivePack(pack, &error)) << error.c_str();
        ASSERT_TRUE(RegisterEvidence(
            service,
            profile,
            {
                MakeEvidence(
                    profile,
                    "population.evidence.actor-leader",
                    "FoA/Actors/PreviewLeader"),
                MakeEvidence(
                    profile,
                    "population.evidence.actor-guard",
                    "FoA/Actors/PreviewGuard"),
                MakeEvidence(
                    profile,
                    "population.evidence.troop",
                    "FoA/Troops/PreviewPatrol"),
            },
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor-leader",
            "population.actor.preview-leader",
            "actor",
            "FoA/Actors/PreviewLeader",
            pack.m_packId,
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor-guard",
            "population.actor.preview-guard",
            "actor",
            "FoA/Actors/PreviewGuard",
            pack.m_packId,
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.troop",
            "population.troop.preview-patrol",
            "troop",
            "FoA/Troops/PreviewPatrol",
            pack.m_packId,
            error)) << error.c_str();

        PopulationTroopDefinition definition = MakeTroopDefinition(
            "population.actor.preview-leader",
            "FoA/Actors/PreviewLeader",
            "population.evidence.actor-leader",
            "population.troop.preview-patrol",
            "population.evidence.troop");
        definition.m_profile.m_minimumSize = 2;
        definition.m_profile.m_maximumSize = 2;
        PopulationTroopMember guard;
        guard.m_linkId = "population.member.patrol-guard";
        guard.m_troopRecordId = definition.m_profile.m_recordId;
        guard.m_actorRecordId = "population.actor.preview-guard";
        guard.m_actorSubjectRef = "FoA/Actors/PreviewGuard";
        guard.m_role = "melee";
        guard.m_evidenceIds = { "population.evidence.actor-guard" };
        definition.m_members.push_back(guard);

        FoundationChangeCounter counter;
        counter.BusConnect();
        ASSERT_TRUE(service.UpsertPopulationTroopDefinition(
            definition,
            &error)) << error.c_str();
        EXPECT_EQ(counter.m_count, 1);
        ASSERT_EQ(
            service.GetCatalog().FindPopulationMembersForTroop(
                definition.m_profile.m_recordId).size(),
            2);

        definition.m_profile.m_minimumSize = 1;
        definition.m_profile.m_maximumSize = 1;
        definition.m_members.resize(1);
        ASSERT_TRUE(service.UpsertPopulationTroopDefinition(
            definition,
            &error)) << error.c_str();
        EXPECT_EQ(counter.m_count, 2);
        counter.BusDisconnect();

        const auto storedMembers =
            service.GetCatalog().FindPopulationMembersForTroop(
                definition.m_profile.m_recordId);
        ASSERT_EQ(storedMembers.size(), 1);
        EXPECT_EQ(
            storedMembers.front().m_linkId,
            "population.member.patrol-leader");
        EXPECT_EQ(
            service.GetCatalog().FindPopulationTroopProfile(
                definition.m_profile.m_recordId)->m_maximumSize,
            1);
    }

    TEST(PopulationAuthoringTests, WrongSubjectEvidenceFailsWithoutPublication)
    {
        ASSERT_TRUE(EnsureCatalogReflection());
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        service.Initialize();
        service.SetWorkspace(workspace);
        AZStd::string error;
        ASSERT_TRUE(service.SetActivePack(pack, &error)) << error.c_str();
        ASSERT_TRUE(RegisterEvidence(
            service,
            profile,
            {
                MakeEvidence(
                    profile,
                    "population.evidence.actor",
                    "FoA/Actors/PreviewLeader"),
                MakeEvidence(
                    profile,
                    "population.evidence.wrong",
                    "FoA/Troops/Unrelated"),
            },
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor",
            "population.actor.preview-leader",
            "actor",
            "FoA/Actors/PreviewLeader",
            pack.m_packId,
            error)) << error.c_str();

        FoundationChangeCounter counter;
        counter.BusConnect();
        const PopulationActorProfile actor = MakeActorProfile(
            "population.actor.preview-leader",
            "population.evidence.wrong");
        EXPECT_FALSE(service.UpsertPopulationActorProfile(actor, &error));
        EXPECT_NE(error.find("allowed exact subject"), AZStd::string::npos);
        EXPECT_EQ(counter.m_count, 0);
        counter.BusDisconnect();
        EXPECT_EQ(
            service.GetCatalog().FindPopulationActorProfile(actor.m_recordId),
            nullptr);
    }

    TEST(PopulationAuthoringTests, ActivePackAndRecordOwnershipAreEnforced)
    {
        ASSERT_TRUE(EnsureCatalogReflection());
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest activePack = MakePack();
        const PackManifest foreignPack = MakePack("population.pack.foreign");
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        service.Initialize();
        service.SetWorkspace(workspace);
        AZStd::string error;
        ASSERT_TRUE(service.SetActivePack(activePack, &error)) << error.c_str();
        ASSERT_TRUE(service.UpsertPack(foreignPack, &error)) << error.c_str();
        ASSERT_TRUE(RegisterEvidence(
            service,
            profile,
            { MakeEvidence(
                profile,
                "population.evidence.actor",
                "FoA/Actors/ForeignActor") },
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor",
            "population.actor.foreign",
            "actor",
            "FoA/Actors/ForeignActor",
            foreignPack.m_packId,
            error)) << error.c_str();

        const PopulationActorProfile actor = MakeActorProfile(
            "population.actor.foreign",
            "population.evidence.actor");
        EXPECT_FALSE(service.UpsertPopulationActorProfile(actor, &error));
        EXPECT_NE(error.find("different pack"), AZStd::string::npos);
        EXPECT_EQ(
            service.GetCatalog().FindPopulationActorProfile(actor.m_recordId),
            nullptr);

        service.ClearActivePack();
        EXPECT_FALSE(service.UpsertPopulationActorProfile(actor, &error));
        EXPECT_NE(error.find("active pack"), AZStd::string::npos);
    }

    TEST(PopulationAuthoringTests, WrongProfileEvidenceFailsClosed)
    {
        const AZStd::string root = "Workspace";
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& activeProfile = workspace.m_gameProfiles.front();
        const GameProfile foreignProfile = MakeProfile(
            "population.profile.foreign",
            "2.0.0");
        const PackManifest pack = MakePack();

        SourceEvidenceRegistry registry;
        AZStd::string error;
        ASSERT_TRUE(registry.RegisterSource(
            MakeSource(foreignProfile),
            &error)) << error.c_str();
        ASSERT_TRUE(registry.RegisterEvidence(
            MakeEvidence(
                foreignProfile,
                "population.evidence.actor",
                "FoA/Actors/PreviewLeader"),
            &error)) << error.c_str();

        CatalogDatabase catalog;
        ASSERT_TRUE(catalog.InsertNew(
            MakePopulationRecord(
                "population.actor.preview-leader",
                "actor",
                "FoA/Actors/PreviewLeader",
                "population.evidence.actor",
                pack.m_packId),
            &error)) << error.c_str();

        PopulationAuthoringService authoring;
        const auto candidate = authoring.BuildActorProfileCandidate(
            MakeActorProfile(
                "population.actor.preview-leader",
                "population.evidence.actor"),
            root,
            workspace,
            activeProfile,
            pack,
            registry,
            catalog);
        EXPECT_FALSE(candidate.IsSuccess());
        EXPECT_NE(
            candidate.GetError().find("active profile"),
            AZStd::string::npos);
    }

    TEST(PopulationAuthoringTests, InvalidTroopCompositionDoesNotPublish)
    {
        ASSERT_TRUE(EnsureCatalogReflection());
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        service.Initialize();
        service.SetWorkspace(workspace);
        AZStd::string error;
        ASSERT_TRUE(service.SetActivePack(pack, &error)) << error.c_str();
        ASSERT_TRUE(RegisterEvidence(
            service,
            profile,
            {
                MakeEvidence(
                    profile,
                    "population.evidence.actor",
                    "FoA/Actors/PreviewLeader"),
                MakeEvidence(
                    profile,
                    "population.evidence.troop",
                    "FoA/Troops/PreviewPatrol"),
            },
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor",
            "population.actor.preview-leader",
            "actor",
            "FoA/Actors/PreviewLeader",
            pack.m_packId,
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.troop",
            "population.troop.preview-patrol",
            "troop",
            "FoA/Troops/PreviewPatrol",
            pack.m_packId,
            error)) << error.c_str();

        PopulationTroopDefinition definition = MakeTroopDefinition(
            "population.actor.preview-leader",
            "FoA/Actors/PreviewLeader",
            "population.evidence.actor",
            "population.troop.preview-patrol",
            "population.evidence.troop");
        definition.m_profile.m_minimumSize = 2;
        definition.m_profile.m_maximumSize = 2;

        FoundationChangeCounter counter;
        counter.BusConnect();
        EXPECT_FALSE(service.UpsertPopulationTroopDefinition(
            definition,
            &error));
        EXPECT_NE(error.find("size range"), AZStd::string::npos);
        EXPECT_EQ(counter.m_count, 0);
        counter.BusDisconnect();
        EXPECT_EQ(
            service.GetCatalog().FindPopulationTroopProfile(
                definition.m_profile.m_recordId),
            nullptr);
        EXPECT_TRUE(
            service.GetCatalog().FindPopulationMembersForTroop(
                definition.m_profile.m_recordId).empty());
    }

    TEST(PopulationAuthoringTests, FailedPersistenceLeavesPublishedCatalogUnchanged)
    {
        ASSERT_TRUE(EnsureCatalogReflection());
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        service.Initialize();
        service.SetWorkspace(workspace);
        AZStd::string error;
        ASSERT_TRUE(service.SetActivePack(pack, &error)) << error.c_str();
        ASSERT_TRUE(RegisterEvidence(
            service,
            profile,
            { MakeEvidence(
                profile,
                "population.evidence.actor",
                "FoA/Actors/PreviewLeader") },
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor",
            "population.actor.preview-leader",
            "actor",
            "FoA/Actors/PreviewLeader",
            pack.m_packId,
            error)) << error.c_str();

        const QString catalogDirectoryPath =
            QDir(temporary.path()).filePath(QStringLiteral("Catalog"));
        QDir catalogDirectory(catalogDirectoryPath);
        ASSERT_TRUE(catalogDirectory.removeRecursively());
        QFile blocker(catalogDirectoryPath);
        ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
        ASSERT_GT(blocker.write("blocked"), 0);
        blocker.close();

        FoundationChangeCounter counter;
        counter.BusConnect();
        const PopulationActorProfile actor = MakeActorProfile(
            "population.actor.preview-leader",
            "population.evidence.actor");
        EXPECT_FALSE(service.UpsertPopulationActorProfile(actor, &error));
        EXPECT_EQ(counter.m_count, 0);
        counter.BusDisconnect();
        EXPECT_EQ(
            service.GetCatalog().FindPopulationActorProfile(actor.m_recordId),
            nullptr);
        EXPECT_NE(
            service.GetCatalog().FindByRecordId(actor.m_recordId),
            nullptr);
    }
} // namespace TaintedGrailModdingSDK
