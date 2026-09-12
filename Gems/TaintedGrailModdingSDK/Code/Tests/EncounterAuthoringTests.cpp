/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "EncounterPlanningService.h"
#include "FoundationService.h"
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/Json/JsonSystemComponent.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzTest/AzTest.h>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
namespace TaintedGrailModdingSDK
{
    namespace
    {
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
            PackManifest::Reflect(&context);
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

        bool PrepareCompletionService(FoundationService& service, const QString& root, AZStd::string& error)
        {
            WorkspaceModel workspace = MakeWorkspace(ToAzString(root));
            auto& profile = workspace.m_gameProfiles.front();
            profile.m_installPath = ToAzString(root + "/Game");
            profile.m_managedAssembliesPath = profile.m_installPath + "/Managed";
            profile.m_pluginPath = profile.m_installPath + "/BepInEx/plugins";
            profile.m_diagnosticsPath = ToAzString(root + "/Diagnostics");
            profile.m_extractedDataPath = ToAzString(root + "/Extracted");
            if (!QDir().mkpath(root + "/Diagnostics") || !QDir().mkpath(root + "/Extracted")
                || !QDir().mkpath(root + "/Packs/population.pack.active")) { return false; }
            return PrepareAuthoringService(service, workspace, MakePack(), error)
                && service.SaveActivePack(ToAzString(root + "/Packs/population.pack.active/pack.tgpack.json"), &error);
        }
        class EncounterAuthoringTests
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

    TEST_F(EncounterAuthoringTests, CaptainAndThreeGuardsPersistReopenAndRemoveOnlyTheirOwnEntry)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, captain, guards, id, other;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Captain", {}, captain, &error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Guard", {}, guards, &error)) << error.c_str();
        ASSERT_TRUE(service.CreateEncounterDefinition("North gate", captain, id, &error)) << error.c_str();
        ASSERT_TRUE(service.CreateEncounterDefinition("Other gate", guards, other, &error)) << error.c_str();
        auto definition = *service.GetCatalog().FindEncounterDefinition(id);
        const auto captainEntry = definition.m_entries.front().m_entryId;
        EncounterEntry guard; guard.m_entryId = "entry.north.guards"; guard.m_targetRecordId = guards;
        guard.m_minimumCount = 3; guard.m_maximumCount = 3; definition.m_entries.push_back(guard);
        definition.m_activationMode = "all_conditions"; definition.m_conditions = {"Player enters the gate"};
        definition.m_placementSubjectRef = "North gate courtyard";
        definition.m_maximumActiveInstances = 2; definition.m_populationLimit = 8;
        definition.m_cleanupNotes = "Retire the patrol when the area resets";
        definition.m_rollbackNotes = "Restore the previous patrol plan";
        ASSERT_TRUE(service.SaveEncounterDefinition(definition, "North gate patrol", &error)) << error.c_str();
        const auto preview = EncounterPlanningService().Preview(definition, service.GetCatalog());
        ASSERT_TRUE(preview.IsValid());
        EXPECT_EQ(preview.m_minimumActors, 4); EXPECT_EQ(preview.m_maximumActors, 4); EXPECT_EQ(preview.m_maximumConcurrentActors, 8);
        const auto* canonical = service.GetCatalog().FindByRecordId(id);
        ASSERT_NE(canonical, nullptr); EXPECT_EQ(canonical->m_displayName, "North gate patrol");
        EXPECT_EQ(canonical->m_ownerPackId, "population.pack.active"); EXPECT_TRUE(canonical->m_allowedUsages.empty());
        EXPECT_TRUE(canonical->m_nativeRefExact.empty());
        auto loaded = CatalogPersistenceService().Load(service.GetWorkspaceRootPath());
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        EXPECT_EQ(loaded.GetValue().m_schemaVersion, CurrentCatalogSchemaVersion);
        CatalogDatabase reopened;
        ASSERT_TRUE(reopened.ReplaceFromBoundDocument(loaded.GetValue(), service.GetWorkspace(),
            *service.GetWorkspace().FindActiveGameProfile(), service.GetSourceRegistry(), &error)) << error.c_str();
        const auto* saved = reopened.FindEncounterDefinition(id);
        ASSERT_NE(saved, nullptr); ASSERT_EQ(saved->m_entries.size(), 2);
        EXPECT_EQ(saved->m_conditions, definition.m_conditions);
        EXPECT_EQ(saved->m_cleanupNotes, definition.m_cleanupNotes);
        EXPECT_EQ(saved->m_rollbackNotes, definition.m_rollbackNotes);
        ASSERT_EQ(saved->m_evidenceIds.size(), 3);
        definition.m_entries = {definition.m_entries.front()};
        ASSERT_TRUE(service.SaveEncounterDefinition(definition, "Captain alone", &error)) << error.c_str();
        EXPECT_EQ(service.GetCatalog().FindEncounterDefinition(id)->m_entries.front().m_entryId, captainEntry);
        EXPECT_EQ(service.GetCatalog().FindEncounterDefinition(other)->m_entries.size(), 1);
        EXPECT_EQ(service.GetCatalog().GetPopulationActorProfiles().size(), 2);
        ASSERT_TRUE(service.ReloadSourceEvidence(&error)) << error.c_str();
        ASSERT_TRUE(service.ReloadCatalog(&error)) << error.c_str();
        EXPECT_EQ(service.GetCatalog().FindEncounterDefinition(id)->m_entries.size(), 1);
    }

    TEST_F(EncounterAuthoringTests, InvalidReferencesCountsModesAndOwnershipLeavePublishedStateUnchanged)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, actor, id;
        EXPECT_FALSE(service.CreateEncounterDefinition("Unconfigured", "missing", id, &error));
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Captain", {}, actor, &error)) << error.c_str();
        ASSERT_TRUE(service.CreateEncounterDefinition("Patrol", actor, id, &error)) << error.c_str();
        const auto original = *service.GetCatalog().FindEncounterDefinition(id);
        auto reject = [&](EncounterDefinition bad)
        {
            EXPECT_FALSE(service.SaveEncounterDefinition(bad, "Must not publish", &error));
            EXPECT_EQ(service.GetCatalog().FindByRecordId(id)->m_displayName, "Patrol");
            EXPECT_EQ(service.GetCatalog().FindEncounterDefinition(id)->m_entries.size(), 1);
            EXPECT_EQ(service.GetCatalog().FindEncounterDefinition(id)->m_entries.front().m_maximumCount, 1);
        };
        auto bad = original; bad.m_entries.front().m_targetRecordId = "missing.actor"; reject(bad);
        bad = original; bad.m_entries.front().m_minimumCount = 2; reject(bad);
        bad = original; bad.m_entries.front().m_maximumCount = 1001; reject(bad);
        bad = original; bad.m_entries.push_back(bad.m_entries.front()); reject(bad);
        bad = original; bad.m_entries.clear(); reject(bad);
        bad = original; bad.m_populationLimit = 1; bad.m_maximumActiveInstances = 2; reject(bad);
        bad = original; bad.m_uniqueEncounter = true; bad.m_maximumActiveInstances = 2; reject(bad);
        bad = original; bad.m_activationMode = "unknown"; reject(bad);
        bad = original; bad.m_activationMode = "all_conditions"; reject(bad);
        bad = original; bad.m_conditions = {"manual with conditions"}; reject(bad);
        bad = original; bad.m_activationMode = "all_conditions"; bad.m_conditions = {"same", "same"}; reject(bad);
        bad = original; bad.m_placementRecordId = actor; reject(bad);
        bad = original; bad.m_cleanupNotes = AZStd::string(1025, 'x'); reject(bad);
        bad = original; bad.m_recordId = actor; reject(bad);
        auto unique = *service.GetCatalog().FindPopulationActorProfile(actor); unique.m_uniqueActor = true;
        ASSERT_TRUE(service.UpsertPopulationActorProfile(unique, &error)) << error.c_str();
        bad = original; bad.m_entries.front().m_maximumCount = 2; reject(bad);
        auto pack = MakePack("population.pack.other");
        ASSERT_TRUE(service.SetActivePack(pack, &error)) << error.c_str();
        ASSERT_TRUE(QDir().mkpath(directory.path() + "/Packs/other"));
        ASSERT_TRUE(service.SaveActivePack(ToAzString(directory.path() + "/Packs/other/pack.tgpack.json"), &error)) << error.c_str();
        reject(original);
    }

    TEST_F(EncounterAuthoringTests, TroopQuantitiesUseDeclaredSizeAndRespectUniqueMemberLimits)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, captain, guard, troop, id;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Captain", {}, captain, &error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Guard", {}, guard, &error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("troop", "Patrol", captain, troop, &error)) << error.c_str();
        PopulationTroopDefinition group;
        group.m_profile = *service.GetCatalog().FindPopulationTroopProfile(troop);
        group.m_members = service.GetCatalog().FindPopulationMembersForTroop(troop);
        PopulationTroopMember member; member.m_linkId = "member.patrol.guards"; member.m_troopRecordId = troop;
        member.m_actorRecordId = guard; member.m_role = "melee"; member.m_minimumCount = 3; member.m_maximumCount = 4; member.m_required = true;
        group.m_members.push_back(member); group.m_profile.m_minimumSize = 4; group.m_profile.m_maximumSize = 5;
        ASSERT_TRUE(service.SaveAuthoredPopulationTroop(group, &error)) << error.c_str();
        ASSERT_TRUE(service.CreateEncounterDefinition("Two patrols", troop, id, &error)) << error.c_str();
        auto definition = *service.GetCatalog().FindEncounterDefinition(id);
        definition.m_entries.front().m_minimumCount = 2; definition.m_entries.front().m_maximumCount = 2;
        auto result = EncounterPlanningService().Preview(definition, service.GetCatalog());
        ASSERT_TRUE(result.IsValid()); EXPECT_EQ(result.m_minimumActors, 8); EXPECT_EQ(result.m_maximumActors, 10);
        auto unique = *service.GetCatalog().FindPopulationActorProfile(captain); unique.m_uniqueActor = true;
        ASSERT_TRUE(service.UpsertPopulationActorProfile(unique, &error)) << error.c_str();
        result = EncounterPlanningService().Preview(definition, service.GetCatalog());
        EXPECT_FALSE(result.IsValid());
        EXPECT_FALSE(service.SaveEncounterDefinition(definition, "Invalid duplicate captain", &error));
    }


    TEST_F(EncounterAuthoringTests, EntryIdentityCannotMoveBetweenEncounters)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, actor, first, second;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Guard", {}, actor, &error)) << error.c_str();
        ASSERT_TRUE(service.CreateEncounterDefinition("First", actor, first, &error)) << error.c_str();
        ASSERT_TRUE(service.CreateEncounterDefinition("Second", actor, second, &error)) << error.c_str();
        auto moved = *service.GetCatalog().FindEncounterDefinition(second);
        const auto originalId = moved.m_entries.front().m_entryId;
        moved.m_entries.front().m_entryId = service.GetCatalog().FindEncounterDefinition(first)->m_entries.front().m_entryId;
        EXPECT_FALSE(service.SaveEncounterDefinition(moved, "Second", &error));
        EXPECT_NE(error.find("another encounter"), AZStd::string::npos);
        EXPECT_EQ(service.GetCatalog().FindEncounterDefinition(second)->m_entries.front().m_entryId, originalId);
    }

    TEST_F(EncounterAuthoringTests, MaximumCompositionUsesWideTotalsAndRejectsOversizedDocuments)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, actor;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Seed", {}, actor, &error)) << error.c_str();
        CatalogDatabase catalog = service.GetCatalog();
        const auto originalRecord = *catalog.FindByRecordId(actor);
        const auto originalActor = *catalog.FindPopulationActorProfile(actor);
        EncounterDefinition definition; definition.m_recordId = "encounter.maximum";
        definition.m_maximumActiveInstances = 1000; definition.m_populationLimit = 1000000;
        for (size_t index = 0; index < 128; ++index)
        {
            auto record = originalRecord; record.m_recordId = "actor.bound." + AZStd::to_string(index);
            record.m_subjectRef = "subject:" + record.m_recordId;
            ASSERT_TRUE(catalog.InsertNew(record, &error)) << error.c_str();
            auto profile = originalActor; profile.m_recordId = record.m_recordId;
            ASSERT_TRUE(catalog.UpsertPopulationActorProfile(profile, &error)) << error.c_str();
            EncounterEntry entry; entry.m_entryId = "entry.bound." + AZStd::to_string(index);
            entry.m_targetRecordId = record.m_recordId; entry.m_minimumCount = 1000; entry.m_maximumCount = 1000;
            definition.m_entries.push_back(entry);
        }
        // Deliberately use a pure planning fixture: no evidence is published for these synthetic bound tests.
        auto result = EncounterPlanningService().Preview(definition, catalog);
        EXPECT_EQ(result.m_rows.size(), 128); EXPECT_EQ(result.m_maximumActors, 128000);
        EXPECT_EQ(result.m_maximumConcurrentActors, 128000000); EXPECT_FALSE(result.IsValid());
        definition.m_maximumActiveInstances = 1;
        EXPECT_TRUE(EncounterPlanningService().Preview(definition, catalog).IsValid());
        definition.m_entries.push_back(definition.m_entries.front());
        result = EncounterPlanningService().Preview(definition, catalog);
        EXPECT_FALSE(result.IsValid()); EXPECT_TRUE(result.m_rows.empty());
    }

    TEST_F(EncounterAuthoringTests, PersistenceFailureAndInvalidEvidenceNeverPublishAnEncounterChange)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, actor, id;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Guard", {}, actor, &error)) << error.c_str();
        ASSERT_TRUE(service.CreateEncounterDefinition("Patrol", actor, id, &error)) << error.c_str();
        auto definition = *service.GetCatalog().FindEncounterDefinition(id);
        auto document = service.GetCatalog().BuildDocument(service.GetWorkspace(), *service.GetWorkspace().FindActiveGameProfile());
        document.m_encounterDefinitions.front().m_evidenceIds = {"missing.evidence"};
        CatalogDatabase candidate = service.GetCatalog();
        EXPECT_FALSE(candidate.ReplaceFromBoundDocument(document, service.GetWorkspace(),
            *service.GetWorkspace().FindActiveGameProfile(), service.GetSourceRegistry(), &error));
        EXPECT_EQ(candidate.FindEncounterDefinition(id)->m_evidenceIds, definition.m_evidenceIds);
        document = service.GetCatalog().BuildDocument(service.GetWorkspace(), *service.GetWorkspace().FindActiveGameProfile());
        document.m_encounterDefinitions.push_back(document.m_encounterDefinitions.front());
        EXPECT_FALSE(candidate.ReplaceFromBoundDocument(document, service.GetWorkspace(),
            *service.GetWorkspace().FindActiveGameProfile(), service.GetSourceRegistry(), &error));
        const auto path = ToQString(service.GetCatalogFilePath());
        ASSERT_TRUE(QFile::rename(path, path + ".saved")); ASSERT_TRUE(QDir().mkdir(path));
        definition.m_entries.front().m_maximumCount = 2;
        EXPECT_FALSE(service.SaveEncounterDefinition(definition, "Do not publish", &error));
        EXPECT_EQ(service.GetCatalog().FindByRecordId(id)->m_displayName, "Patrol");
        EXPECT_EQ(service.GetCatalog().FindEncounterDefinition(id)->m_entries.front().m_maximumCount, 1);
        EXPECT_FALSE(service.CreateEncounterDefinition("Do not create", actor, id, &error));
        EXPECT_TRUE(id.empty()); EXPECT_EQ(service.GetCatalog().GetEncounterDefinitions().size(), 1);
    }

} // namespace TaintedGrailModdingSDK
