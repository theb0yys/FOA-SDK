/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogPersistenceService.h"
#include "FoundationNotificationBus.h"
#include "FoundationService.h"
#include "PopulationAuthoringService.h"

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/Json/JsonSystemComponent.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>
#include <AzCore/std/utility/pair.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzTest/AzTest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
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

        void ReflectPopulationPersistenceTypes(AZ::SerializeContext& context)
        {
            GameProfile::Reflect(&context);
            WorkspaceModel::Reflect(&context);
            SourceRecord::Reflect(&context);
            EvidenceRecord::Reflect(&context);
            ImportIssue::Reflect(&context);
            SourceDocument::Reflect(&context);
            EvidenceDocument::Reflect(&context);
            CatalogRecord::Reflect(&context);
            CatalogRelationship::Reflect(&context);
            CatalogValidationEvent::Reflect(&context);
            CatalogGovernanceEvent::Reflect(&context);
            EconomyItemProfile::Reflect(&context);
            EconomyRecipeProfile::Reflect(&context);
            EconomyRecipeIngredient::Reflect(&context);
            EconomyRecipeOutput::Reflect(&context);
            CatalogDocument::Reflect(&context);
        }

        GameProfile MakeProfile(
            AZStd::string profileId = "population.profile",
            AZStd::string gameVersion = "1.0.0",
            AZStd::string branch = "mono")
        {
            GameProfile profile;
            profile.m_profileId = AZStd::move(profileId);
            profile.m_displayName = "Population test profile";
            profile.m_installPath = "Game";
            profile.m_gameVersion = AZStd::move(gameVersion);
            profile.m_branch = AZStd::move(branch);
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
            AZStd::string subjectRef,
            AZStd::string recordPath = "/records/0")
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
            evidence.m_recordPath = AZStd::move(recordPath);
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

        PopulationTroopMember MakeMember(
            AZStd::string linkId,
            const AZStd::string& troopRecordId,
            AZStd::string actorRecordId,
            AZStd::string actorSubjectRef,
            AZStd::string role,
            AZStd::string evidenceId)
        {
            PopulationTroopMember member;
            member.m_linkId = AZStd::move(linkId);
            member.m_troopRecordId = troopRecordId;
            member.m_actorRecordId = AZStd::move(actorRecordId);
            member.m_actorSubjectRef = AZStd::move(actorSubjectRef);
            member.m_role = AZStd::move(role);
            member.m_minimumCount = 1;
            member.m_maximumCount = 1;
            member.m_weight = 1.0;
            member.m_required = member.m_role == "leader";
            member.m_conditions = { "rain", "morning" };
            member.m_evidenceIds = { AZStd::move(evidenceId) };
            return member;
        }

        PopulationTroopDefinition MakeTroopDefinition(
            const AZStd::string& troopRecordId,
            const AZStd::string& troopEvidenceId,
            const AZStd::string& leaderRecordId,
            const AZStd::string& leaderSubjectRef,
            PopulationTroopMember leader)
        {
            PopulationTroopDefinition definition;
            definition.m_profile.m_recordId = troopRecordId;
            definition.m_profile.m_troopKind = "patrol";
            definition.m_profile.m_leaderActorRecordId = leaderRecordId;
            definition.m_profile.m_leaderActorSubjectRef = leaderSubjectRef;
            definition.m_profile.m_minimumSize = 1;
            definition.m_profile.m_maximumSize = 1;
            definition.m_profile.m_formation = "single-file";
            definition.m_profile.m_tags = { "night", "forest" };
            definition.m_profile.m_evidenceIds = { troopEvidenceId };
            definition.m_members.push_back(AZStd::move(leader));
            return definition;
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

        bool PrepareWorkspaceStorage(const AZStd::string& root)
        {
            const QDir rootDirectory(ToQString(root));
            for (const QString& relative : {
                     QStringLiteral("Build"),
                     QStringLiteral("Staging"),
                     QStringLiteral("Deployment"),
                     QStringLiteral("Game/Managed"),
                     QStringLiteral("Game/BepInEx/plugins") })
            {
                if (!rootDirectory.mkpath(relative))
                {
                    return false;
                }
            }

            QFile assembly(rootDirectory.filePath(
                QStringLiteral("Game/Managed/SyntheticFixture.dll")));
            if (!assembly.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                return false;
            }
            return assembly.write("synthetic-test-assembly") > 0;
        }

        bool PrepareAuthoringService(
            FoundationService& service,
            const WorkspaceModel& workspace,
            const PackManifest& pack,
            AZStd::string& error)
        {
            if (!PrepareWorkspaceStorage(workspace.m_rootPath))
            {
                error = "Unable to prepare synthetic population workspace storage.";
                return false;
            }
            service.Initialize();
            service.SetWorkspace(workspace);
            const AZStd::string workspacePath = ToAzString(
                QDir(ToQString(workspace.m_rootPath)).filePath(
                    QStringLiteral("population.tgworkspace.json")));
            return service.SaveWorkspace(workspacePath, &error)
                && service.SetActivePack(pack, &error);
        }

        class FoundationChangeCounter final
            : public FoundationNotificationBus::Handler
        {
        public:
            FoundationChangeCounter()
            {
                BusConnect();
            }

            ~FoundationChangeCounter()
            {
                BusDisconnect();
            }

            void OnFoundationChanged() override
            {
                ++m_count;
            }

            int m_count = 0;
        };

        class FoundationActorPublicationObserver final
            : public FoundationNotificationBus::Handler
        {
        public:
            FoundationActorPublicationObserver(
                const FoundationService& service,
                AZStd::string workspaceRoot,
                AZStd::string actorRecordId)
                : m_service(service)
                , m_workspaceRoot(AZStd::move(workspaceRoot))
                , m_actorRecordId(AZStd::move(actorRecordId))
            {
                BusConnect();
            }

            ~FoundationActorPublicationObserver()
            {
                BusDisconnect();
            }

            void OnFoundationChanged() override
            {
                ++m_count;
                m_liveCatalogWasPublished =
                    m_service.GetCatalog().FindPopulationActorProfile(
                        m_actorRecordId) != nullptr;
                CatalogPersistenceService persistence;
                const auto loaded = persistence.Load(m_workspaceRoot);
                m_committedFileWasVisible = loaded.IsSuccess()
                    && loaded.GetValue().m_actorProfiles.size() == 1
                    && loaded.GetValue().m_actorProfiles.front().m_recordId
                        == m_actorRecordId;
            }

            const FoundationService& m_service;
            AZStd::string m_workspaceRoot;
            AZStd::string m_actorRecordId;
            int m_count = 0;
            bool m_liveCatalogWasPublished = false;
            bool m_committedFileWasVisible = false;
        };

        class PopulationAuthoringTests
            : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                AZ::ComponentApplication::Descriptor descriptor;
                descriptor.m_useExistingAllocator = true;
                AZ::ComponentApplication::StartupParameters startup;
                startup.m_loadStaticModules = false;
                startup.m_loadDynamicModules = false;
                startup.m_loadAssetCatalog = false;
                startup.m_loadSettingsRegistry = false;
                ASSERT_NE(m_application.Create(descriptor, startup), nullptr);
                m_created = true;

                if (!AZ::IO::FileIOBase::GetInstance())
                {
                    AZ::IO::FileIOBase::SetInstance(&m_fileIO);
                    m_installedFileIo = true;
                }

                AZ::SerializeContext* serializeContext =
                    m_application.GetSerializeContext();
                AZ::JsonRegistrationContext* jsonRegistrationContext =
                    m_application.GetJsonRegistrationContext();
                ASSERT_NE(serializeContext, nullptr);
                ASSERT_NE(jsonRegistrationContext, nullptr);
                AZ::JsonSystemComponent::Reflect(serializeContext);
                AZ::JsonSystemComponent::Reflect(jsonRegistrationContext);
                ReflectPopulationPersistenceTypes(*serializeContext);
            }

            void TearDown() override
            {
                if (AZ::SerializeContext* serializeContext =
                        m_application.GetSerializeContext())
                {
                    serializeContext->EnableRemoveReflection();
                    ReflectPopulationPersistenceTypes(*serializeContext);
                    AZ::JsonSystemComponent::Reflect(serializeContext);
                    serializeContext->DisableRemoveReflection();
                }
                if (AZ::JsonRegistrationContext* jsonRegistrationContext =
                        m_application.GetJsonRegistrationContext())
                {
                    jsonRegistrationContext->EnableRemoveReflection();
                    AZ::JsonSystemComponent::Reflect(jsonRegistrationContext);
                    jsonRegistrationContext->DisableRemoveReflection();
                }
                if (m_created)
                {
                    m_application.Destroy();
                }
                if (m_installedFileIo)
                {
                    AZ::IO::FileIOBase::SetInstance(nullptr);
                }
            }

            AZ::ComponentApplication m_application;
            AZ::IO::LocalFileIO m_fileIO;
            bool m_created = false;
            bool m_installedFileIo = false;
        };
    } // namespace

    TEST_F(PopulationAuthoringTests, ActorProfilePersistsPublishesSnapshotAndNotifiesOnce)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error;
        ASSERT_TRUE(PrepareAuthoringService(service, workspace, pack, error))
            << error.c_str();
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

        const PopulationActorProfile actor = MakeActorProfile(
            "population.actor.preview-leader",
            "population.evidence.actor");
        FoundationActorPublicationObserver observer(
            service,
            root,
            actor.m_recordId);
        ASSERT_TRUE(service.UpsertPopulationActorProfile(actor, &error))
            << error.c_str();
        EXPECT_EQ(observer.m_count, 1);
        EXPECT_TRUE(observer.m_liveCatalogWasPublished);
        EXPECT_TRUE(observer.m_committedFileWasVisible);
        ASSERT_TRUE(service.UpsertPopulationActorProfile(actor, &error))
            << error.c_str();
        EXPECT_EQ(observer.m_count, 2);
        EXPECT_TRUE(observer.m_liveCatalogWasPublished);
        EXPECT_TRUE(observer.m_committedFileWasVisible);

        const PopulationActorProfile* stored =
            service.GetCatalog().FindPopulationActorProfile(actor.m_recordId);
        ASSERT_NE(stored, nullptr);
        ASSERT_EQ(stored->m_tags.size(), 2);
        EXPECT_EQ(stored->m_tags.front(), "alpha");
        EXPECT_EQ(actor.m_tags.front(), "zeta");
        EXPECT_EQ(service.GetSnapshot().m_populationActorProfileCount, 1);
        EXPECT_EQ(service.GetSnapshot().m_populationTroopProfileCount, 0);
        EXPECT_EQ(service.GetSnapshot().m_populationTroopMemberCount, 0);
        EXPECT_EQ(service.GetCatalog().GetPopulationActorProfiles().size(), 1);
        const CatalogRecord* canonical =
            service.GetCatalog().FindByRecordId(actor.m_recordId);
        ASSERT_NE(canonical, nullptr);
        EXPECT_TRUE(canonical->m_allowedUsages.empty());
        EXPECT_EQ(
            canonical->m_forbiddenUsages,
            AZStd::vector<AZStd::string>{ "no_unvalidated_runtime_use" });

        CatalogPersistenceService persistence;
        const auto loaded = persistence.Load(root);
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        ASSERT_EQ(loaded.GetValue().m_actorProfiles.size(), 1);
        EXPECT_EQ(
            loaded.GetValue().m_actorProfiles.front().m_recordId,
            actor.m_recordId);
    }

    TEST_F(PopulationAuthoringTests, TroopDefinitionIsAtomicOrderIndependentAndPreservesOmittedMembers)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error;
        ASSERT_TRUE(PrepareAuthoringService(service, workspace, pack, error))
            << error.c_str();
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
        for (const auto& record : {
                 MakePopulationRecord(
                     "population.actor.preview-leader",
                     "actor",
                     "FoA/Actors/PreviewLeader",
                     "population.evidence.actor-leader",
                     pack.m_packId),
                 MakePopulationRecord(
                     "population.actor.preview-guard",
                     "actor",
                     "FoA/Actors/PreviewGuard",
                     "population.evidence.actor-guard",
                     pack.m_packId),
                 MakePopulationRecord(
                     "population.troop.preview-patrol",
                     "troop",
                     "FoA/Troops/PreviewPatrol",
                     "population.evidence.troop",
                     pack.m_packId) })
        {
            ASSERT_TRUE(PromoteSyntheticRecord(
                service,
                record.m_evidenceIds.front(),
                record.m_recordId,
                record.m_recordKind,
                record.m_subjectRef,
                record.m_ownerPackId,
                error)) << error.c_str();
        }

        PopulationTroopMember leader = MakeMember(
            "population.member.patrol-leader",
            "population.troop.preview-patrol",
            "population.actor.preview-leader",
            "FoA/Actors/PreviewLeader",
            "leader",
            "population.evidence.actor-leader");
        PopulationTroopMember guard = MakeMember(
            "population.member.patrol-guard",
            "population.troop.preview-patrol",
            "population.actor.preview-guard",
            "FoA/Actors/PreviewGuard",
            "melee",
            "population.evidence.actor-guard");
        PopulationTroopDefinition definition = MakeTroopDefinition(
            "population.troop.preview-patrol",
            "population.evidence.troop",
            "population.actor.preview-leader",
            "FoA/Actors/PreviewLeader",
            leader);
        definition.m_profile.m_minimumSize = 2;
        definition.m_profile.m_maximumSize = 2;
        definition.m_members.push_back(guard);

        FoundationChangeCounter counter;
        ASSERT_TRUE(service.UpsertPopulationTroopDefinition(definition, &error))
            << error.c_str();
        EXPECT_EQ(counter.m_count, 1);

        PopulationTroopDefinition swap = definition;
        swap.m_profile.m_leaderActorRecordId =
            "population.actor.preview-guard";
        swap.m_profile.m_leaderActorSubjectRef =
            "FoA/Actors/PreviewGuard";
        PopulationTroopMember formerLeader = leader;
        formerLeader.m_role = "melee";
        formerLeader.m_required = false;
        PopulationTroopMember newLeader = guard;
        newLeader.m_role = "leader";
        newLeader.m_required = true;
        swap.m_members = { newLeader, formerLeader };
        ASSERT_TRUE(service.UpsertPopulationTroopDefinition(swap, &error))
            << error.c_str();
        EXPECT_EQ(counter.m_count, 2);
        EXPECT_EQ(swap.m_members.front().m_linkId, newLeader.m_linkId);

        PopulationTroopDefinition additive = swap;
        additive.m_profile.m_formation = "wedge";
        additive.m_members = { newLeader };
        ASSERT_TRUE(service.UpsertPopulationTroopDefinition(additive, &error))
            << error.c_str();
        EXPECT_EQ(counter.m_count, 3);

        const auto storedMembers =
            service.GetCatalog().FindPopulationMembersForTroop(
                additive.m_profile.m_recordId);
        ASSERT_EQ(storedMembers.size(), 2);
        EXPECT_EQ(
            storedMembers.front().m_linkId,
            "population.member.patrol-guard");
        EXPECT_EQ(
            storedMembers.back().m_linkId,
            "population.member.patrol-leader");
        EXPECT_EQ(storedMembers.front().m_role, "leader");
        EXPECT_EQ(storedMembers.back().m_role, "melee");
        EXPECT_EQ(service.GetSnapshot().m_populationTroopProfileCount, 1);
        EXPECT_EQ(service.GetSnapshot().m_populationTroopMemberCount, 2);
    }

    TEST_F(PopulationAuthoringTests, StandaloneUpdatesPreserveCompleteCatalogIntegrity)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error;
        ASSERT_TRUE(PrepareAuthoringService(service, workspace, pack, error))
            << error.c_str();
        ASSERT_TRUE(RegisterEvidence(
            service,
            profile,
            {
                MakeEvidence(
                    profile,
                    "population.evidence.actor",
                    "FoA/Actors/Leader"),
                MakeEvidence(
                    profile,
                    "population.evidence.troop",
                    "FoA/Troops/Patrol"),
                MakeEvidence(
                    profile,
                    "population.evidence.troop-two",
                    "FoA/Troops/PatrolTwo"),
            },
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor",
            "population.actor.leader",
            "actor",
            "FoA/Actors/Leader",
            pack.m_packId,
            error)) << error.c_str();
        for (const auto& troop : {
                 AZStd::pair<AZStd::string, AZStd::string>{
                     "population.troop.patrol", "population.evidence.troop" },
                 AZStd::pair<AZStd::string, AZStd::string>{
                     "population.troop.patrol-two", "population.evidence.troop-two" } })
        {
            const AZStd::string subject = troop.first == "population.troop.patrol"
                ? "FoA/Troops/Patrol"
                : "FoA/Troops/PatrolTwo";
            ASSERT_TRUE(PromoteSyntheticRecord(
                service,
                troop.second,
                troop.first,
                "troop",
                subject,
                pack.m_packId,
                error)) << error.c_str();
        }

        PopulationTroopMember member = MakeMember(
            "population.member.patrol-leader",
            "population.troop.patrol",
            "population.actor.leader",
            "FoA/Actors/Leader",
            "leader",
            "population.evidence.actor");
        PopulationTroopDefinition definition = MakeTroopDefinition(
            "population.troop.patrol",
            "population.evidence.troop",
            "population.actor.leader",
            "FoA/Actors/Leader",
            member);
        ASSERT_TRUE(service.UpsertPopulationTroopDefinition(definition, &error))
            << error.c_str();

        FoundationChangeCounter counter;
        PopulationTroopProfile profileUpdate = definition.m_profile;
        profileUpdate.m_formation = "wedge";
        profileUpdate.m_tags = { "zeta", "alpha" };
        ASSERT_TRUE(service.UpsertPopulationTroopProfile(profileUpdate, &error))
            << error.c_str();
        EXPECT_EQ(counter.m_count, 1);
        ASSERT_NE(
            service.GetCatalog().FindPopulationTroopProfile(
                profileUpdate.m_recordId),
            nullptr);
        EXPECT_EQ(
            service.GetCatalog().FindPopulationTroopProfile(
                profileUpdate.m_recordId)->m_tags.front(),
            "alpha");

        PopulationTroopMember memberUpdate = member;
        memberUpdate.m_weight = 2.0;
        memberUpdate.m_conditions = { "night", "forest" };
        ASSERT_TRUE(service.UpsertPopulationTroopMember(memberUpdate, &error))
            << error.c_str();
        EXPECT_EQ(counter.m_count, 2);
        EXPECT_DOUBLE_EQ(
            service.GetCatalog().FindPopulationMembersForTroop(
                member.m_troopRecordId).front().m_weight,
            2.0);

        PopulationTroopProfile unbootstrapped = definition.m_profile;
        unbootstrapped.m_recordId = "population.troop.patrol-two";
        unbootstrapped.m_evidenceIds = {
            "population.evidence.troop-two",
        };
        EXPECT_FALSE(service.UpsertPopulationTroopProfile(
            unbootstrapped,
            &error));
        EXPECT_NE(error.find("first member"), AZStd::string::npos);
        EXPECT_EQ(counter.m_count, 2);
        EXPECT_EQ(
            service.GetCatalog().FindPopulationTroopProfile(
                unbootstrapped.m_recordId),
            nullptr);
    }

    TEST_F(PopulationAuthoringTests, StructuralFailuresAndLinkOwnerMovesDoNotPublish)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error;
        ASSERT_TRUE(PrepareAuthoringService(service, workspace, pack, error))
            << error.c_str();
        ASSERT_TRUE(RegisterEvidence(
            service,
            profile,
            {
                MakeEvidence(
                    profile,
                    "population.evidence.actor",
                    "FoA/Actors/Leader"),
                MakeEvidence(
                    profile,
                    "population.evidence.troop-one",
                    "FoA/Troops/One"),
                MakeEvidence(
                    profile,
                    "population.evidence.troop-two",
                    "FoA/Troops/Two"),
            },
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor",
            "population.actor.leader",
            "actor",
            "FoA/Actors/Leader",
            pack.m_packId,
            error)) << error.c_str();
        for (const auto& values : {
                 AZStd::pair<const char*, const char*>{
                     "population.troop.one", "population.evidence.troop-one" },
                 AZStd::pair<const char*, const char*>{
                     "population.troop.two", "population.evidence.troop-two" } })
        {
            const AZStd::string subject = values.first == AZStd::string("population.troop.one")
                ? "FoA/Troops/One"
                : "FoA/Troops/Two";
            ASSERT_TRUE(PromoteSyntheticRecord(
                service,
                values.second,
                values.first,
                "troop",
                subject,
                pack.m_packId,
                error)) << error.c_str();
        }

        PopulationTroopMember memberOne = MakeMember(
            "population.member.shared-link",
            "population.troop.one",
            "population.actor.leader",
            "FoA/Actors/Leader",
            "leader",
            "population.evidence.actor");
        PopulationTroopDefinition one = MakeTroopDefinition(
            "population.troop.one",
            "population.evidence.troop-one",
            "population.actor.leader",
            "FoA/Actors/Leader",
            memberOne);
        ASSERT_TRUE(service.UpsertPopulationTroopDefinition(one, &error))
            << error.c_str();

        PopulationTroopDefinition two = MakeTroopDefinition(
            "population.troop.two",
            "population.evidence.troop-two",
            "population.actor.leader",
            "FoA/Actors/Leader",
            MakeMember(
                "population.member.two-leader",
                "population.troop.two",
                "population.actor.leader",
                "FoA/Actors/Leader",
                "leader",
                "population.evidence.actor"));
        FoundationChangeCounter counter;

        PopulationTroopDefinition empty = two;
        empty.m_members.clear();
        EXPECT_FALSE(service.UpsertPopulationTroopDefinition(empty, &error));
        EXPECT_NE(error.find("at least one"), AZStd::string::npos);

        PopulationTroopDefinition wrongTroop = two;
        wrongTroop.m_members.front().m_troopRecordId = "population.troop.one";
        EXPECT_FALSE(service.UpsertPopulationTroopDefinition(wrongTroop, &error));
        EXPECT_NE(error.find("exact troop"), AZStd::string::npos);

        PopulationTroopDefinition duplicate = two;
        duplicate.m_members.push_back(duplicate.m_members.front());
        EXPECT_FALSE(service.UpsertPopulationTroopDefinition(duplicate, &error));
        EXPECT_NE(error.find("duplicate member-link"), AZStd::string::npos);
        EXPECT_EQ(counter.m_count, 0);
        EXPECT_EQ(
            service.GetCatalog().FindPopulationTroopProfile(
                two.m_profile.m_recordId),
            nullptr);

        ASSERT_TRUE(service.UpsertPopulationTroopDefinition(two, &error))
            << error.c_str();
        EXPECT_EQ(counter.m_count, 1);
        PopulationTroopMember moved = memberOne;
        moved.m_troopRecordId = two.m_profile.m_recordId;
        EXPECT_FALSE(service.UpsertPopulationTroopMember(moved, &error));
        EXPECT_NE(error.find("cannot move"), AZStd::string::npos);
        EXPECT_EQ(counter.m_count, 1);
        EXPECT_EQ(
            service.GetCatalog().FindPopulationMembersForTroop(
                one.m_profile.m_recordId).size(),
            1);
        EXPECT_EQ(
            service.GetCatalog().FindPopulationMembersForTroop(
                two.m_profile.m_recordId).size(),
            1);
    }

    TEST_F(PopulationAuthoringTests, ContextEvidenceAndPrimaryOwnershipFailClosed)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();

        AZStd::string error;
        {
            FoundationService unsaved(FoundationWorkspaceLoadDependencies{});
            unsaved.Initialize();
            unsaved.SetWorkspace(workspace);
            ASSERT_TRUE(unsaved.SetActivePack(pack, &error)) << error.c_str();
            FoundationChangeCounter unsavedCounter;
            EXPECT_FALSE(unsaved.UpsertPopulationActorProfile(
                MakeActorProfile(
                    "population.actor.preview",
                    "population.evidence.actor"),
                &error));
            EXPECT_NE(error.find("saved or loaded"), AZStd::string::npos);
            EXPECT_EQ(unsavedCounter.m_count, 0);
        }

        FoundationService service(FoundationWorkspaceLoadDependencies{});
        ASSERT_TRUE(PrepareAuthoringService(service, workspace, pack, error))
            << error.c_str();
        service.ClearActivePack();
        {
            FoundationChangeCounter counter;
            EXPECT_FALSE(service.UpsertPopulationActorProfile(
                MakeActorProfile(
                    "population.actor.preview",
                    "population.evidence.actor"),
                &error));
            EXPECT_NE(error.find("active pack"), AZStd::string::npos);
            EXPECT_EQ(counter.m_count, 0);
        }
        ASSERT_TRUE(service.SetActivePack(pack, &error)) << error.c_str();
        ASSERT_TRUE(RegisterEvidence(
            service,
            profile,
            {
                MakeEvidence(
                    profile,
                    "population.evidence.actor",
                    "FoA/Actors/Preview"),
                MakeEvidence(
                    profile,
                    "population.evidence.wrong",
                    "FoA/Actors/Unrelated"),
                MakeEvidence(
                    profile,
                    "population.evidence.foreign",
                    "FoA/Actors/Foreign"),
            },
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor",
            "population.actor.preview",
            "actor",
            "FoA/Actors/Preview",
            pack.m_packId,
            error)) << error.c_str();

        PopulationActorProfile actor = MakeActorProfile(
            "population.actor.preview",
            "population.evidence.wrong");
        {
            FoundationChangeCounter counter;
            EXPECT_FALSE(service.UpsertPopulationActorProfile(actor, &error));
            EXPECT_NE(error.find("allowed exact subject"), AZStd::string::npos);
            actor.m_evidenceIds = {
                "population.evidence.actor",
                "population.evidence.actor",
            };
            EXPECT_FALSE(service.UpsertPopulationActorProfile(actor, &error));
            EXPECT_NE(error.find("unique"), AZStd::string::npos);
            EXPECT_EQ(counter.m_count, 0);
        }

        PackManifest wrongBranch = pack;
        wrongBranch.m_targetBranch = "il2cpp";
        ASSERT_TRUE(service.SetActivePack(wrongBranch, &error))
            << error.c_str();
        actor.m_evidenceIds = { "population.evidence.actor" };
        {
            FoundationChangeCounter counter;
            EXPECT_FALSE(service.UpsertPopulationActorProfile(actor, &error));
            EXPECT_NE(error.find("compatible"), AZStd::string::npos);
            EXPECT_EQ(counter.m_count, 0);
        }
        ASSERT_TRUE(service.SetActivePack(pack, &error)) << error.c_str();

        const PackManifest foreignPack = MakePack("population.pack.foreign");
        ASSERT_TRUE(service.UpsertPack(foreignPack, &error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.foreign",
            "population.actor.foreign",
            "actor",
            "FoA/Actors/Foreign",
            foreignPack.m_packId,
            error)) << error.c_str();
        {
            FoundationChangeCounter counter;
            EXPECT_FALSE(service.UpsertPopulationActorProfile(
                MakeActorProfile(
                    "population.actor.foreign",
                    "population.evidence.foreign"),
                &error));
            EXPECT_NE(error.find("different pack"), AZStd::string::npos);
            EXPECT_EQ(counter.m_count, 0);
        }
        EXPECT_EQ(
            service.GetCatalog().FindPopulationActorProfile(
                "population.actor.preview"),
            nullptr);
        EXPECT_EQ(
            service.GetCatalog().FindPopulationActorProfile(
                "population.actor.foreign"),
            nullptr);
    }

    TEST_F(PopulationAuthoringTests, WrongProfileSourceAndCrossPackReferencesAreHandledExplicitly)
    {
        const AZStd::string root = "Workspace";
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& activeProfile = workspace.m_gameProfiles.front();
        const GameProfile foreignProfile = MakeProfile(
            "population.profile.foreign",
            "2.0.0");
        const PackManifest pack = MakePack();

        SourceEvidenceRegistry foreignRegistry;
        AZStd::string error;
        ASSERT_TRUE(foreignRegistry.RegisterSource(
            MakeSource(foreignProfile),
            &error)) << error.c_str();
        ASSERT_TRUE(foreignRegistry.RegisterEvidence(
            MakeEvidence(
                foreignProfile,
                "population.evidence.actor",
                "FoA/Actors/Preview"),
            &error)) << error.c_str();
        CatalogDatabase catalog;
        ASSERT_TRUE(catalog.InsertNew(
            MakePopulationRecord(
                "population.actor.preview",
                "actor",
                "FoA/Actors/Preview",
                "population.evidence.actor",
                pack.m_packId),
            &error)) << error.c_str();
        PopulationAuthoringService authoring;
        const auto wrongProfile = authoring.BuildActorProfileCandidate(
            MakeActorProfile(
                "population.actor.preview",
                "population.evidence.actor"),
            root,
            workspace,
            activeProfile,
            pack,
            foreignRegistry,
            catalog);
        EXPECT_FALSE(wrongProfile.IsSuccess());
        EXPECT_NE(
            wrongProfile.GetError().find("active profile"),
            AZStd::string::npos);
        EXPECT_TRUE(catalog.GetPopulationActorProfiles().empty());

        SourceEvidenceRegistry registry;
        ASSERT_TRUE(registry.RegisterSource(MakeSource(activeProfile), &error))
            << error.c_str();
        for (const EvidenceRecord& evidence : {
                 MakeEvidence(
                     activeProfile,
                     "population.evidence.primary",
                     "FoA/Actors/Primary"),
                 MakeEvidence(
                     activeProfile,
                     "population.evidence.template",
                     "FoA/Templates/Foreign"),
             })
        {
            ASSERT_TRUE(registry.RegisterEvidence(evidence, &error))
                << error.c_str();
        }
        CatalogDatabase crossPack;
        ASSERT_TRUE(crossPack.InsertNew(
            MakePopulationRecord(
                "population.actor.primary",
                "actor",
                "FoA/Actors/Primary",
                "population.evidence.primary",
                pack.m_packId),
            &error)) << error.c_str();
        ASSERT_TRUE(crossPack.InsertNew(
            MakePopulationRecord(
                "population.template.foreign",
                "template",
                "FoA/Templates/Foreign",
                "population.evidence.template",
                "population.pack.foreign"),
            &error)) << error.c_str();
        PopulationActorProfile actor = MakeActorProfile(
            "population.actor.primary",
            "population.evidence.primary");
        actor.m_templateRecordId = "population.template.foreign";
        actor.m_templateSubjectRef = "FoA/Templates/Foreign";
        actor.m_evidenceIds.push_back("population.evidence.template");
        const auto allowedReference = authoring.BuildActorProfileCandidate(
            actor,
            root,
            workspace,
            activeProfile,
            pack,
            registry,
            crossPack);
        ASSERT_TRUE(allowedReference.IsSuccess())
            << allowedReference.GetError().c_str();
        EXPECT_NE(
            allowedReference.GetValue().FindPopulationActorProfile(
                actor.m_recordId),
            nullptr);
        EXPECT_TRUE(crossPack.GetPopulationActorProfiles().empty());

        PackManifest compatiblePack = pack;
        compatiblePack.m_targetGameVersion = "0.9.0";
        compatiblePack.m_compatibleGameVersions = { "1.0.0" };
        const auto compatibleReference = authoring.BuildActorProfileCandidate(
            actor,
            root,
            workspace,
            activeProfile,
            compatiblePack,
            registry,
            crossPack);
        ASSERT_TRUE(compatibleReference.IsSuccess())
            << compatibleReference.GetError().c_str();
        EXPECT_TRUE(crossPack.GetPopulationActorProfiles().empty());
    }

    TEST_F(PopulationAuthoringTests, CandidateContextRejectsUnboundWorkspaceAndPackWithoutMutation)
    {
        const AZStd::string root = "Workspace";
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        SourceEvidenceRegistry registry;
        AZStd::string error;
        ASSERT_TRUE(registry.RegisterSource(MakeSource(profile), &error))
            << error.c_str();
        ASSERT_TRUE(registry.RegisterEvidence(
            MakeEvidence(
                profile,
                "population.evidence.actor",
                "FoA/Actors/Preview"),
            &error)) << error.c_str();
        CatalogDatabase catalog;
        ASSERT_TRUE(catalog.InsertNew(
            MakePopulationRecord(
                "population.actor.preview",
                "actor",
                "FoA/Actors/Preview",
                "population.evidence.actor",
                pack.m_packId),
            &error)) << error.c_str();
        const PopulationActorProfile actor = MakeActorProfile(
            "population.actor.preview",
            "population.evidence.actor");
        PopulationAuthoringService authoring;

        EXPECT_FALSE(authoring.BuildActorProfileCandidate(
            actor,
            {},
            workspace,
            profile,
            pack,
            registry,
            catalog).IsSuccess());

        WorkspaceModel unstableWorkspace = workspace;
        unstableWorkspace.m_workspaceId.clear();
        EXPECT_FALSE(authoring.BuildActorProfileCandidate(
            actor,
            root,
            unstableWorkspace,
            profile,
            pack,
            registry,
            catalog).IsSuccess());

        PackManifest missingTargets = pack;
        missingTargets.m_targetGameVersion.clear();
        missingTargets.m_targetBranch.clear();
        EXPECT_FALSE(authoring.BuildActorProfileCandidate(
            actor,
            root,
            workspace,
            profile,
            missingTargets,
            registry,
            catalog).IsSuccess());

        PackManifest incompatible = pack;
        incompatible.m_targetGameVersion = "2.0.0";
        EXPECT_FALSE(authoring.BuildActorProfileCandidate(
            actor,
            root,
            workspace,
            profile,
            incompatible,
            registry,
            catalog).IsSuccess());

        PackManifest runtimeEnabled = pack;
        runtimeEnabled.m_runtimeActionsEnabled = true;
        EXPECT_FALSE(authoring.BuildActorProfileCandidate(
            actor,
            root,
            workspace,
            profile,
            runtimeEnabled,
            registry,
            catalog).IsSuccess());

        GameProfile mismatchedProfile = profile;
        mismatchedProfile.m_branch = "il2cpp";
        EXPECT_FALSE(authoring.BuildActorProfileCandidate(
            actor,
            root,
            workspace,
            mismatchedProfile,
            pack,
            registry,
            catalog).IsSuccess());
        EXPECT_TRUE(catalog.GetPopulationActorProfiles().empty());
        EXPECT_EQ(registry.GetEvidence().size(), 1);
        EXPECT_EQ(actor.m_tags.front(), "zeta");
    }

    TEST_F(PopulationAuthoringTests, PersistenceFailureLeavesPublishedCatalogUnchangedAndSilent)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string root = ToAzString(temporary.path());
        const WorkspaceModel workspace = MakeWorkspace(root);
        const GameProfile& profile = workspace.m_gameProfiles.front();
        const PackManifest pack = MakePack();
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error;
        ASSERT_TRUE(PrepareAuthoringService(service, workspace, pack, error))
            << error.c_str();
        ASSERT_TRUE(RegisterEvidence(
            service,
            profile,
            { MakeEvidence(
                profile,
                "population.evidence.actor",
                "FoA/Actors/Preview") },
            error)) << error.c_str();
        ASSERT_TRUE(PromoteSyntheticRecord(
            service,
            "population.evidence.actor",
            "population.actor.preview",
            "actor",
            "FoA/Actors/Preview",
            pack.m_packId,
            error)) << error.c_str();

        const PopulationActorProfile actor = MakeActorProfile(
            "population.actor.preview",
            "population.evidence.actor");
        const GameProfile* activeProfile =
            service.GetWorkspace().FindActiveGameProfile();
        const PackManifest* activePack = service.GetActivePack();
        ASSERT_NE(activeProfile, nullptr);
        ASSERT_NE(activePack, nullptr);
        PopulationAuthoringService authoring;
        const auto validCandidate = authoring.BuildActorProfileCandidate(
            actor,
            service.GetWorkspaceRootPath(),
            service.GetWorkspace(),
            *activeProfile,
            *activePack,
            service.GetSourceRegistry(),
            service.GetCatalog());
        ASSERT_TRUE(validCandidate.IsSuccess())
            << validCandidate.GetError().c_str();

        const QString catalogDirectoryPath =
            QDir(temporary.path()).filePath(QStringLiteral("Catalog"));
        QDir catalogDirectory(catalogDirectoryPath);
        ASSERT_TRUE(catalogDirectory.removeRecursively());
        QFile blocker(catalogDirectoryPath);
        ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
        ASSERT_GT(blocker.write("blocked"), 0);
        blocker.close();

        FoundationChangeCounter counter;
        EXPECT_FALSE(service.UpsertPopulationActorProfile(actor, &error));
        EXPECT_NE(
            error.find("Unable to create the catalog directory"),
            AZStd::string::npos);
        EXPECT_EQ(counter.m_count, 0);
        EXPECT_EQ(
            service.GetCatalog().FindPopulationActorProfile(actor.m_recordId),
            nullptr);
        EXPECT_NE(
            service.GetCatalog().FindByRecordId(actor.m_recordId),
            nullptr);
        EXPECT_EQ(service.GetSnapshot().m_populationActorProfileCount, 0);
    }
} // namespace TaintedGrailModdingSDK
