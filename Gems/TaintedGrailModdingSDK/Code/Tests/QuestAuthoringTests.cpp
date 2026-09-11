/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "QuestAuthoringService.h"
#include "WorldPlanningService.h"
#include <limits>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
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
        class QuestAuthoringTests
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




    namespace
    {
        QByteArray QuestBytes(const FoundationService& s)
        { QFile f(ToQString(s.GetCatalogFilePath())); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray{}; }
        QuestAuthoringDraft QuestDraft(const FoundationService& s, const AZStd::string& id)
        { return QuestAuthoringService::Read(*s.GetCatalog().FindQuestProfile(id)); }
        AZStd::string QuestRevision(const FoundationService& s, const AZStd::string& id)
        { return QuestAuthoringService::Revision(*s.GetCatalog().FindQuestProfile(id)); }
    }
    TEST_F(QuestAuthoringTests, CreateEditReopenPreservesIdentityMetadataAndCanonicalBytes)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error, id;
        ASSERT_TRUE(PrepareCompletionService(service, dir.path(), error));
        ASSERT_TRUE(service.CreateQuestDefinition("Quest authoring fixture", id, &error)) << error.c_str();
        auto draft = QuestDraft(service, id); const auto phaseId = draft.m_definition.m_phases.front().m_phaseId;
        draft.m_stateKeys = {{"state.fixture.visited", "boolean", "false", "Visit flag"}, {"state.fixture.count","integer","0","Count"}};
        draft.m_definition.m_conditions = {{"condition.fixture.visited","fact.equals","state.fixture.visited"}};
        draft.m_definition.m_actions = {{"action.fixture.count","counter.increment","state.fixture.count","idempotency.fixture.count"}};
        draft.m_definition.m_transitions.front().m_conditionIds = {"condition.fixture.visited"};
        draft.m_definition.m_transitions.front().m_actionIds = {"action.fixture.count"};
        draft.m_definition.m_display.m_fallbackName = "Edited quest";
        ASSERT_TRUE(service.SaveQuestDefinition(draft, QuestRevision(service,id), &error)) << error.c_str();
        const auto revision = QuestRevision(service,id), bytes = QuestAuthoringService::CanonicalProfile(draft).m_definitionJson;
        ASSERT_TRUE(service.ReloadSourceEvidence(&error)); ASSERT_TRUE(service.ReloadCatalog(&error)) << error.c_str();
        EXPECT_EQ(QuestRevision(service,id), revision);
        EXPECT_EQ(QuestDraft(service,id).m_definition.m_phases.front().m_phaseId, phaseId);
        EXPECT_EQ(service.GetCatalog().FindQuestProfile(id)->m_definitionJson, bytes);
        EXPECT_EQ(service.GetCatalog().FindByRecordId(id)->m_displayName, "Edited quest");
        auto reversed = QuestDraft(service,id); AZStd::reverse(reversed.m_labels.begin(), reversed.m_labels.end());
        AZStd::reverse(reversed.m_stateKeys.begin(), reversed.m_stateKeys.end());
        EXPECT_EQ(QuestAuthoringService::Revision(QuestAuthoringService::CanonicalProfile(reversed)), revision);
        const auto stable = QuestBytes(service); ASSERT_TRUE(service.SaveCatalog(&error)); EXPECT_EQ(QuestBytes(service), stable);
    }
    TEST_F(QuestAuthoringTests, StateTypesMissingReferencesAndMalformedGraphFailWithoutPublication)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id;
        ASSERT_TRUE(PrepareCompletionService(service,dir.path(),error)); ASSERT_TRUE(service.CreateQuestDefinition("Quest fixture",id,&error));
        const auto original=QuestDraft(service,id); const auto bytes=QuestBytes(service); const auto revision=QuestRevision(service,id);
        const auto reject=[&](const QuestAuthoringDraft& draft)
        { EXPECT_FALSE(service.SaveQuestDefinition(draft,revision,&error)); EXPECT_EQ(QuestBytes(service),bytes); EXPECT_EQ(QuestRevision(service,id),revision); };
        auto bad=original; bad.m_stateKeys={{"state.fixture.flag","boolean","1",""}}; reject(bad);
        bad=original; bad.m_stateKeys={{"state.fixture.count","integer","1000000001",""}}; reject(bad);
        bad=original; bad.m_stateKeys={{"state.fixture.flag","text","false",""}};
        bad.m_definition.m_conditions={{"condition.fixture.flag","fact.equals","state.fixture.flag"}}; reject(bad);
        bad=original; bad.m_definition.m_conditions={{"condition.fixture.missing","location.presence","subject.missing"}}; reject(bad);
        bad=original; bad.m_definition.m_transitions.front().m_toPhaseId="phase.missing"; reject(bad);
        bad=original; for (auto& phase : bad.m_definition.m_phases) { phase.m_entryPhase=false; } reject(bad);
        bad=original; bad.m_definition.m_authority.m_runtimeExecutionAllowed=true; reject(bad);
        bad=original; bad.m_labels.push_back(bad.m_labels.front()); reject(bad);
        bad=original; bad.m_stateKeys={{id,"boolean","false",""}}; reject(bad);
        bad=original; bad.m_definition.m_actions={{"action.fixture.invalid","execute.arbitrary","subject.missing","idempotency.test"}}; reject(bad);
    }
    TEST_F(QuestAuthoringTests, ExactCatalogBindingsSupportActorsItemsLocationsAndRejectWrongKinds)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id,actor,item,region,scene,location;
        ASSERT_TRUE(PrepareCompletionService(service,dir.path(),error)); ASSERT_TRUE(service.CreateQuestDefinition("Quest fixture",id,&error));
        ASSERT_TRUE(service.CreatePopulationRecord("actor","Quest actor",{},actor,&error));
        ASSERT_TRUE(service.CreateEconomyRecord("item","Quest item",item,&error));
        ASSERT_TRUE(service.CreateWorldPlace("region","Quest region",{},region,&error));
        ASSERT_TRUE(service.CreateWorldPlace("scene","Quest scene",region,scene,&error));
        ASSERT_TRUE(service.CreateWorldPlace("location","Quest place",scene,location,&error));
        auto draft=QuestDraft(service,id); draft.m_definition.m_roles.push_back({"role.fixture.actor","loc.role.actor",true}); draft.m_definition.m_roles.push_back({"role.fixture.item","loc.role.item",false});
        draft.m_definition.m_conditions={{"condition.fixture.actor","role.available","role.fixture.actor"},{"condition.fixture.place","location.presence","subject.fixture.place"}};
        draft.m_bindings={{"role.fixture.actor",actor},{"role.fixture.item",item},{"subject.fixture.place",location}};
        ASSERT_TRUE(service.SaveQuestDefinition(draft,QuestRevision(service,id),&error)) << error.c_str();
        const auto revision=QuestRevision(service,id); draft.m_bindings.front().m_recordId=item;
        EXPECT_FALSE(service.SaveQuestDefinition(draft,revision,&error)); EXPECT_EQ(QuestRevision(service,id),revision);
        draft=QuestDraft(service,id); draft.m_bindings.front().m_recordId="missing.actor"; EXPECT_FALSE(service.SaveQuestDefinition(draft,revision,&error));
        ASSERT_TRUE(service.ReloadSourceEvidence(&error)); ASSERT_TRUE(service.ReloadCatalog(&error)) << error.c_str();
        EXPECT_EQ(service.GetCatalog().FindQuestProfile(id)->m_bindings.size(),3);
    }
    TEST_F(QuestAuthoringTests, ReadOnlyInspectionAndEditableCopyKeepV1AndRejectAuthorityPayloads)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id,copy;
        ASSERT_TRUE(PrepareCompletionService(service,dir.path(),error)); ASSERT_TRUE(service.CreateQuestDefinition("Imported fixture",id,&error));
        const auto profile=*service.GetCatalog().FindQuestProfile(id); const auto filePath=dir.path()+"/fixture.tgquest.json";
        QFile file(filePath); ASSERT_TRUE(file.open(QIODevice::WriteOnly)); file.write(profile.m_definitionJson.c_str()); file.close();
        const auto before=QuestBytes(service); QuestAuthoringDraft inspected;
        ASSERT_TRUE(service.ReadQuestDocument(ToAzString(filePath),inspected,&error)); EXPECT_EQ(QuestBytes(service),before);
        ASSERT_TRUE(service.AdoptQuestDefinition(inspected,copy,&error)) << error.c_str();
        EXPECT_NE(copy,id); EXPECT_EQ(QuestDraft(service,copy).m_definition.m_phases.front().m_phaseId,inspected.m_definition.m_phases.front().m_phaseId);
        inspected.m_definition.m_authority.m_editorMutationAllowed=true;
        EXPECT_FALSE(service.AdoptQuestDefinition(inspected,copy,&error)); EXPECT_TRUE(copy.empty());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write("{\"schema\": \"unknown\"}"); file.close();
        EXPECT_FALSE(service.ReadQuestDocument(ToAzString(filePath),inspected,&error)); EXPECT_FALSE(error.empty());
        EXPECT_FALSE(service.ReadQuestDocument(ToAzString(dir.path()),inspected,&error));
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write(QByteArray(1024*1024+1,'x')); file.close();
        EXPECT_FALSE(service.ReadQuestDocument(ToAzString(filePath),inspected,&error));
    }
    TEST_F(QuestAuthoringTests, OptimisticConflictWrongPackAndFailedWriteKeepSavedQuest)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id;
        ASSERT_TRUE(PrepareCompletionService(service,dir.path(),error)); ASSERT_TRUE(service.CreateQuestDefinition("Quest fixture",id,&error));
        auto draft=QuestDraft(service,id); const auto old=QuestRevision(service,id); draft.m_definition.m_display.m_fallbackName="First edit";
        ASSERT_TRUE(service.SaveQuestDefinition(draft,old,&error)); const auto current=QuestRevision(service,id);
        draft.m_definition.m_display.m_fallbackName="Stale edit"; EXPECT_FALSE(service.SaveQuestDefinition(draft,old,&error));
        EXPECT_EQ(QuestRevision(service,id),current);
        const QString path=ToQString(service.GetCatalogFilePath()), held=path+".held"; ASSERT_TRUE(QFile::rename(path,held)); ASSERT_TRUE(QDir().mkpath(path));
        const bool saved=service.SaveQuestDefinition(draft,current,&error); EXPECT_TRUE(QDir().rmdir(path)); EXPECT_TRUE(QFile::rename(held,path));
        EXPECT_FALSE(saved); EXPECT_EQ(QuestRevision(service,id),current);
        auto pack=MakePack("quest.other.pack"); ASSERT_TRUE(service.SetActivePack(pack,&error));
        ASSERT_TRUE(QDir().mkpath(dir.path()+"/Packs/quest.other.pack"));
        ASSERT_TRUE(service.SaveActivePack(ToAzString(dir.path()+"/Packs/quest.other.pack/pack.tgpack.json"),&error));
        EXPECT_FALSE(service.SaveQuestDefinition(draft,current,&error));
    }
    TEST_F(QuestAuthoringTests, SchemaFiveMigrationRetainsWorldAndExactBackup)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id,region;
        ASSERT_TRUE(PrepareCompletionService(service,dir.path(),error)); ASSERT_TRUE(service.CreateWorldPlace("region","World retained",{},region,&error));
        auto object=QJsonDocument::fromJson(QuestBytes(service)).object(); object["SchemaVersion"]=5; object.remove("QuestProfiles");
        const auto original=QJsonDocument(object).toJson(); const QString path=ToQString(service.GetCatalogFilePath());
        QFile file(path); ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write(original); file.close();
        ASSERT_TRUE(service.ReloadCatalog(&error)); ASSERT_TRUE(service.CreateQuestDefinition("Quest after migration",id,&error)) << error.c_str();
        EXPECT_NE(service.GetCatalog().FindWorldPlace(region),nullptr);
        const auto hash=QCryptographicHash::hash(original,QCryptographicHash::Sha256).toHex();
        QFile backup(path+".schema-5."+QString::fromLatin1(hash)+".backup.json"); ASSERT_TRUE(backup.open(QIODevice::ReadOnly)); EXPECT_EQ(backup.readAll(),original);
        EXPECT_EQ(QJsonDocument::fromJson(QuestBytes(service)).object()["SchemaVersion"].toInt(),static_cast<int>(CurrentCatalogSchemaVersion));
    }
    TEST_F(QuestAuthoringTests, OldFutureDuplicateOversizedAndUnrelatedEvidenceFailAtomically)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id,second;
        ASSERT_TRUE(PrepareCompletionService(service,dir.path(),error)); ASSERT_TRUE(service.CreateQuestDefinition("Quest fixture",id,&error));
        ASSERT_TRUE(service.CreateQuestDefinition("Unrelated quest",second,&error));
        const auto good=service.GetCatalog().BuildDocument(service.GetWorkspace(),*service.GetWorkspace().FindActiveGameProfile());
        auto published=service.GetCatalog(); const auto reject=[&](const CatalogDocument& doc)
        {
            EXPECT_FALSE(published.ReplaceFromBoundDocument(doc,service.GetWorkspace(),*service.GetWorkspace().FindActiveGameProfile(),service.GetSourceRegistry(),&error));
            EXPECT_EQ(published.GetQuestProfiles().size(),2);
        };
        for (AZ::u32 version : {1u,2u,3u,4u,5u,CurrentCatalogSchemaVersion + 1}) { auto bad=good; bad.m_schemaVersion=version; reject(bad); }
        auto bad=good; bad.m_questProfiles.push_back(bad.m_questProfiles.front()); reject(bad);
        bad=good; bad.m_questProfiles.front().m_definitionJson="{}"; reject(bad);
        bad=good; bad.m_questProfiles.front().m_definitionJson=AZStd::string(1024*1024+1,'x'); reject(bad);
        bad=good; bad.m_questProfiles.front().m_stateKeys.resize(257); reject(bad);
        bad=good; bad.m_questProfiles.front().m_evidenceIds=bad.m_questProfiles.back().m_evidenceIds; reject(bad);
        bad=good; bad.m_questProfiles.resize(257); reject(bad);
        published.Clear(); EXPECT_TRUE(published.GetQuestProfiles().empty());
    }
}
