/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "EncounterPlanningService.h"
#include "SocietyPlanningService.h"
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
        class SocietyAuthoringTests
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
        FactionDefinition SocietyDefinition(const FoundationService& service, const AZStd::string& id)
        {
            return {*service.GetCatalog().FindFactionProfile(id), service.GetCatalog().FindFactionLinks(id)};
        }
        FactionLink SocietyLink(const AZStd::string& id, const AZStd::string& faction,
            const AZStd::string& kind, const AZStd::string& target, const AZStd::string& value)
        {
            FactionLink link; link.m_linkId = id; link.m_factionRecordId = faction;
            link.m_kind = kind; link.m_targetRecordId = target; link.m_value = value; return link;
        }
        QByteArray SocietyCatalogBytes(const FoundationService& service)
        {
            QFile file(ToQString(service.GetCatalogFilePath()));
            if (!file.open(QIODevice::ReadOnly)) { return {}; }
            return file.readAll();
        }
    }
    TEST_F(SocietyAuthoringTests, TownGuardCultureMembershipLeadershipAndDirectedRelationshipsSurviveReopen)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, captain, guard, troop, town, bandits, culture;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Captain", {}, captain, &error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Guard", {}, guard, &error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("troop", "Patrol", captain, troop, &error)) << error.c_str();
        ASSERT_TRUE(service.CreateCultureProfile("Northlanders", culture, &error)) << error.c_str();
        auto cultureProfile = *service.GetCatalog().FindCultureProfile(culture);
        cultureProfile.m_description = "Northern settlements"; cultureProfile.m_language = "Common speech";
        ASSERT_TRUE(service.SaveCultureProfile(cultureProfile, "Northlanders", &error)) << error.c_str();
        ASSERT_TRUE(service.CreateFactionDefinition("Town Guard", town, &error)) << error.c_str();
        ASSERT_TRUE(service.CreateFactionDefinition("Bandits", bandits, &error)) << error.c_str();
        auto definition = SocietyDefinition(service, town);
        definition.m_profile.m_cultureRecordId = culture;
        definition.m_profile.m_description = "Keep the town safe";
        definition.m_profile.m_authorityNotes = "Captain commands the guard";
        definition.m_links = {SocietyLink("link.captain", town, "member", captain, "leader"),
            SocietyLink("link.guard", town, "member", guard, "member"),
            SocietyLink("link.patrol", town, "member", troop, "officer"),
            SocietyLink("link.bandits", town, "disposition", bandits, "hostile"),
            SocietyLink("link.territory", town, "jurisdiction", {}, "controls")};
        definition.m_links.back().m_targetSubjectRef = "North gate courtyard";
        definition.m_links.back().m_notes = "Guard the entrance";
        ASSERT_TRUE(service.SaveFactionDefinition(definition, "Town Guard", &error)) << error.c_str();
        auto analysis = SocietyPlanningService::Analyze(definition, service.GetCatalog());
        ASSERT_TRUE(analysis.IsValid()); EXPECT_EQ(analysis.m_members, 3); EXPECT_EQ(analysis.m_leaderName, "Captain");
        EXPECT_EQ(analysis.m_dispositions, 1); EXPECT_EQ(analysis.m_jurisdictions, 1);
        EXPECT_FALSE(analysis.m_warnings.empty()); // Territory remains an unverified authoring reference.
        EXPECT_TRUE(service.GetCatalog().FindFactionLinks(bandits).empty()); // No implicit reverse relationship.
        auto loaded = CatalogPersistenceService().Load(service.GetWorkspaceRootPath());
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        EXPECT_EQ(loaded.GetValue().m_schemaVersion, CurrentCatalogSchemaVersion);
        CatalogDatabase reopened;
        ASSERT_TRUE(reopened.ReplaceFromBoundDocument(loaded.GetValue(), service.GetWorkspace(),
            *service.GetWorkspace().FindActiveGameProfile(), service.GetSourceRegistry(), &error)) << error.c_str();
        ASSERT_EQ(reopened.FindFactionLinks(town).size(), 5);
        EXPECT_EQ(reopened.FindFactionProfile(town)->m_cultureRecordId, culture);
        EXPECT_EQ(reopened.FindCultureProfile(culture)->m_language, "Common speech");
        EXPECT_EQ(reopened.FindFactionProfile(town)->m_authorityNotes, definition.m_profile.m_authorityNotes);
        const auto* canonical = reopened.FindByRecordId(town);
        ASSERT_NE(canonical, nullptr); EXPECT_EQ(canonical->m_ownerPackId, "population.pack.active");
        EXPECT_TRUE(canonical->m_allowedUsages.empty()); EXPECT_TRUE(canonical->m_nativeRefExact.empty());
        EXPECT_EQ(reopened.GetPopulationActorProfiles().size(), 2); EXPECT_EQ(reopened.GetPopulationTroopProfiles().size(), 1);
        QTemporaryDir copy; ASSERT_TRUE(copy.isValid());
        auto saved = CatalogPersistenceService().Save(reopened.BuildDocument(service.GetWorkspace(),
            *service.GetWorkspace().FindActiveGameProfile()), ToAzString(copy.path()));
        ASSERT_TRUE(saved.IsSuccess()) << saved.GetError().c_str();
        QFile copied(ToQString(saved.GetValue())); ASSERT_TRUE(copied.open(QIODevice::ReadOnly));
        EXPECT_EQ(copied.readAll(), SocietyCatalogBytes(service));
        ASSERT_TRUE(service.ReloadSourceEvidence(&error)) << error.c_str();
        ASSERT_TRUE(service.ReloadCatalog(&error)) << error.c_str();
        EXPECT_EQ(service.GetCatalog().FindFactionLinks(town).size(), 5);
    }

    TEST_F(SocietyAuthoringTests, CompleteReplacementPreservesLinkIdsAndRemovesOnlyOwnedLinks)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, actor, town, other;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Guard", {}, actor, &error));
        ASSERT_TRUE(service.CreateFactionDefinition("Town Guard", town, &error));
        ASSERT_TRUE(service.CreateFactionDefinition("Other", other, &error));
        auto first = SocietyDefinition(service, town), second = SocietyDefinition(service, other);
        first.m_links = {SocietyLink("link.first", town, "member", actor, "member")};
        second.m_links = {SocietyLink("link.second", other, "member", actor, "leader")};
        ASSERT_TRUE(service.SaveFactionDefinition(first, "Town Guard", &error)) << error.c_str();
        ASSERT_TRUE(service.SaveFactionDefinition(second, "Other", &error)) << error.c_str();
        first.m_links.front().m_value = "officer";
        ASSERT_TRUE(service.SaveFactionDefinition(first, "Renamed Guard", &error)) << error.c_str();
        EXPECT_EQ(service.GetCatalog().FindFactionLinks(town).front().m_linkId, "link.first");
        EXPECT_EQ(service.GetCatalog().FindFactionLinks(town).front().m_value, "officer");
        first.m_links.clear();
        ASSERT_TRUE(service.SaveFactionDefinition(first, "Empty Guard", &error)) << error.c_str();
        EXPECT_TRUE(service.GetCatalog().FindFactionLinks(town).empty());
        ASSERT_EQ(service.GetCatalog().FindFactionLinks(other).size(), 1);
        EXPECT_EQ(service.GetCatalog().FindFactionLinks(other).front().m_linkId, "link.second");
        EXPECT_EQ(service.GetCatalog().GetPopulationActorProfiles().size(), 1);
        auto stealing = SocietyDefinition(service, town); stealing.m_links = second.m_links;
        stealing.m_links.front().m_factionRecordId = town;
        const auto before = SocietyCatalogBytes(service);
        EXPECT_FALSE(service.SaveFactionDefinition(stealing, "Steal", &error));
        EXPECT_EQ(SocietyCatalogBytes(service), before);
        auto kindChange = SocietyDefinition(service, other);
        kindChange.m_links.front().m_kind = "disposition"; kindChange.m_links.front().m_targetRecordId = town;
        kindChange.m_links.front().m_value = "friendly";
        EXPECT_FALSE(service.SaveFactionDefinition(kindChange, "Change kind", &error));
        EXPECT_EQ(SocietyCatalogBytes(service), before);
    }

    TEST_F(SocietyAuthoringTests, InvalidTargetsRolesDuplicatesAndOwnershipPreservePublishedDefinitions)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, firstActor, secondActor, troop, town, other, culture, id;
        EXPECT_FALSE(service.CreateFactionDefinition("Unconfigured", id, &error));
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Captain", {}, firstActor, &error));
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Officer", {}, secondActor, &error));
        ASSERT_TRUE(service.CreatePopulationRecord("troop", "Patrol", firstActor, troop, &error));
        ASSERT_TRUE(service.CreateFactionDefinition("Town Guard", town, &error));
        ASSERT_TRUE(service.CreateFactionDefinition("Bandits", other, &error));
        ASSERT_TRUE(service.CreateCultureProfile("Culture", culture, &error));
        auto good = SocietyDefinition(service, town);
        good.m_links = {SocietyLink("link.leader", town, "member", firstActor, "leader")};
        ASSERT_TRUE(service.SaveFactionDefinition(good, "Town Guard", &error)) << error.c_str();
        const auto before = SocietyCatalogBytes(service);
        const auto reject = [&](const FactionDefinition& bad)
        {
            EXPECT_FALSE(service.SaveFactionDefinition(bad, "Invalid", &error));
            EXPECT_EQ(SocietyCatalogBytes(service), before);
            EXPECT_EQ(service.GetCatalog().FindByRecordId(town)->m_displayName, "Town Guard");
        };
        auto bad = good; bad.m_links.push_back(SocietyLink("link.second", town, "member", secondActor, "leader")); reject(bad);
        bad = good; bad.m_links.front().m_targetRecordId = troop; reject(bad);
        bad = good; bad.m_links.front().m_targetRecordId = "missing.actor"; reject(bad);
        bad = good; bad.m_links.front().m_targetRecordId = culture; reject(bad);
        bad = good; bad.m_links.front().m_targetSubjectRef = "wrong:subject"; reject(bad);
        bad = good; bad.m_links.front().m_value = "invented"; reject(bad);
        bad = good; bad.m_links.push_back(bad.m_links.front()); reject(bad);
        bad = good; bad.m_links.push_back(SocietyLink("link.duplicate", town, "member", firstActor, "member")); reject(bad);
        bad = good; bad.m_links.push_back(SocietyLink("link.self", town, "disposition", town, "hostile")); reject(bad);
        bad = good; bad.m_links.push_back(SocietyLink("link.unknown", town, "disposition", other, "invented")); reject(bad);
        bad = good; bad.m_links.push_back(SocietyLink("link.jurisdiction", town, "jurisdiction", firstActor, "controls")); reject(bad);
        bad = good; bad.m_links.push_back(SocietyLink("link.blank", town, "jurisdiction", {}, "controls")); reject(bad);
        bad = good; bad.m_profile.m_cultureRecordId = "missing.culture"; reject(bad);
        bad = good; bad.m_profile.m_authorityNotes = "new\nline"; reject(bad);
        EXPECT_FALSE(service.SaveFactionDefinition(good, "", &error));
        auto otherPack = MakePack("other.pack"); ASSERT_TRUE(service.SetActivePack(otherPack, &error));
        ASSERT_TRUE(QDir().mkpath(directory.path() + "/Packs/other.pack"));
        ASSERT_TRUE(service.SaveActivePack(ToAzString(directory.path() + "/Packs/other.pack/pack.tgpack.json"), &error));
        EXPECT_FALSE(service.SaveFactionDefinition(good, "Wrong owner", &error));
        EXPECT_FALSE(service.SaveCultureProfile(*service.GetCatalog().FindCultureProfile(culture), "Wrong owner", &error));
        EXPECT_EQ(SocietyCatalogBytes(service), before);
    }

    TEST_F(SocietyAuthoringTests, EvidenceAndFailedPersistenceNeverPublishCandidateChanges)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, actor, town;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Captain", {}, actor, &error));
        ASSERT_TRUE(service.CreateFactionDefinition("Town Guard", town, &error));
        auto definition = SocietyDefinition(service, town);
        definition.m_links = {SocietyLink("link.captain", town, "member", actor, "leader")};
        ASSERT_TRUE(service.SaveFactionDefinition(definition, "Town Guard", &error)) << error.c_str();
        definition = SocietyDefinition(service, town);
        auto candidate = service.GetCatalog();
        auto bad = definition; bad.m_links.front().m_evidenceIds = {"missing.evidence"};
        ASSERT_TRUE(candidate.ReplaceFactionDefinition(bad, &error));
        EXPECT_FALSE(candidate.ValidateIntegrity(service.GetWorkspace(), *service.GetWorkspace().FindActiveGameProfile(),
            service.GetSourceRegistry(), &error));
        bad = definition; bad.m_links.front().m_evidenceIds.clear();
        EXPECT_FALSE(candidate.ReplaceFactionDefinition(bad, &error));
        const auto before = SocietyCatalogBytes(service);
        const QString path = ToQString(service.GetCatalogFilePath()), held = path + ".held";
        ASSERT_TRUE(QFile::rename(path, held)); ASSERT_TRUE(QDir().mkdir(path));
        definition.m_profile.m_authorityNotes = "Unsaved revision";
        EXPECT_FALSE(service.SaveFactionDefinition(definition, "Must not publish", &error));
        EXPECT_EQ(service.GetCatalog().FindByRecordId(town)->m_displayName, "Town Guard");
        EXPECT_TRUE(service.GetCatalog().FindFactionProfile(town)->m_authorityNotes.empty());
        ASSERT_TRUE(QDir().rmdir(path)); ASSERT_TRUE(QFile::rename(held, path));
        EXPECT_EQ(SocietyCatalogBytes(service), before);
    }

    TEST_F(SocietyAuthoringTests, SchemaThreeMigrationPreservesEncounterAndExactBackup)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, captain, encounter, faction;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Captain", {}, captain, &error));
        ASSERT_TRUE(service.CreateEncounterDefinition("Patrol", captain, encounter, &error)) << error.c_str();
        auto object = QJsonDocument::fromJson(SocietyCatalogBytes(service)).object();
        object["SchemaVersion"] = 3; object.remove("CultureProfiles"); object.remove("FactionProfiles"); object.remove("FactionLinks");
        const auto original = QJsonDocument(object).toJson();
        const QString path = ToQString(service.GetCatalogFilePath());
        QFile file(path); ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        ASSERT_EQ(file.write(original), original.size()); file.close();
        auto loaded = CatalogPersistenceService().Load(service.GetWorkspaceRootPath());
        ASSERT_TRUE(loaded.IsSuccess()); EXPECT_EQ(loaded.GetValue().m_schemaVersion, EncounterCatalogSchemaVersion);
        ASSERT_TRUE(service.ReloadCatalog(&error)) << error.c_str();
        ASSERT_TRUE(service.CreateFactionDefinition("Town Guard", faction, &error)) << error.c_str();
        EXPECT_NE(service.GetCatalog().FindEncounterDefinition(encounter), nullptr);
        const auto hash = QCryptographicHash::hash(original, QCryptographicHash::Sha256).toHex();
        QFile backup(path + ".schema-3." + QString::fromLatin1(hash) + ".backup.json");
        ASSERT_TRUE(backup.open(QIODevice::ReadOnly)); EXPECT_EQ(backup.readAll(), original);
        EXPECT_EQ(QJsonDocument::fromJson(SocietyCatalogBytes(service)).object()["SchemaVersion"].toInt(), CurrentCatalogSchemaVersion);
        EXPECT_EQ(service.GetCatalog().GetPopulationActorProfiles().size(), 1);
    }

    TEST_F(SocietyAuthoringTests, LegacySocietyPayloadsFutureVersionsDuplicateAndOversizedDocumentsAreRejected)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, town, culture;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        ASSERT_TRUE(service.CreateCultureProfile("Culture", culture, &error));
        ASSERT_TRUE(service.CreateFactionDefinition("Town Guard", town, &error));
        const auto good = service.GetCatalog().BuildDocument(service.GetWorkspace(), *service.GetWorkspace().FindActiveGameProfile());
        const auto before = SocietyCatalogBytes(service);
        auto published = service.GetCatalog();
        const auto reject = [&](const CatalogDocument& document)
        {
            EXPECT_FALSE(published.ReplaceFromBoundDocument(document, service.GetWorkspace(),
                *service.GetWorkspace().FindActiveGameProfile(), service.GetSourceRegistry(), &error));
            EXPECT_EQ(published.GetFactionProfiles().size(), 1);
        };
        for (AZ::u32 version : {1, 2, 3, 7})
        {
            auto bad = good; bad.m_schemaVersion = version; reject(bad);
            auto object = QJsonDocument::fromJson(before).object(); object["SchemaVersion"] = static_cast<int>(version);
            QFile file(ToQString(service.GetCatalogFilePath())); ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            const auto bytes = QJsonDocument(object).toJson(); ASSERT_EQ(file.write(bytes), bytes.size()); file.close();
            EXPECT_FALSE(CatalogPersistenceService().Load(service.GetWorkspaceRootPath()).IsSuccess());
        }
        auto bad = good; bad.m_factionProfiles.push_back(bad.m_factionProfiles.front()); reject(bad);
        bad = good; bad.m_cultureProfiles.clear(); bad.m_factionProfiles.front().m_cultureRecordId = culture; reject(bad);
        bad = good; bad.m_factionProfiles.resize(1001); reject(bad);
        bad = good; bad.m_cultureProfiles.resize(1001); reject(bad);
        bad = good; bad.m_factionLinks.resize(10001); reject(bad);
        auto definition = SocietyDefinition(service, town); definition.m_links.resize(385);
        EXPECT_FALSE(SocietyPlanningService::Analyze(definition, service.GetCatalog()).IsValid());
        auto profile = *service.GetCatalog().FindCultureProfile(culture); profile.m_language = AZStd::string(129, 'x');
        EXPECT_FALSE(SocietyPlanningService::ValidateCulture(profile).IsSuccess());
        profile.m_language = "valid"; profile.m_description = AZStd::string(2049, 'x');
        EXPECT_FALSE(SocietyPlanningService::ValidateCulture(profile).IsSuccess());
        published.Clear(); EXPECT_TRUE(published.GetFactionProfiles().empty()); EXPECT_TRUE(published.GetCultureProfiles().empty());
        EXPECT_TRUE(published.GetFactionLinks().empty());
    }

    TEST_F(SocietyAuthoringTests, ResolvedJurisdictionRequiresExactWorldEvidenceAndRejectsDuplicateOrOrphanLinks)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error, faction;
        ASSERT_TRUE(PrepareCompletionService(service, directory.path(), error)) << error.c_str();
        const QString sourcePath = directory.path() + "/world-fixture.json";
        const QJsonObject evidence{{"evidence_id", "evidence.world.north"}, {"subject_ref", "fixture:world:north-gate"},
            {"kind", "location"}, {"confidence", "documented"}, {"claim", "Project-owned synthetic world location."}};
        const auto bytes = QJsonDocument(QJsonObject{{"evidence", QJsonArray{evidence}}}).toJson();
        QFile source(sourcePath); ASSERT_TRUE(source.open(QIODevice::WriteOnly));
        ASSERT_EQ(source.write(bytes), bytes.size()); source.close();
        SourceImportRequest request;
        request.m_inputPath = ToAzString(sourcePath); request.m_sourceKind = "template-diagnostics";
        request.m_title = "Synthetic location"; request.m_toolName = "Society test fixture"; request.m_toolVersion = "1.0.0";
        request.m_capturedAt = "2026-09-10T12:00:00Z"; request.m_limitations = "Synthetic authoring fixture, no game data.";
        ASSERT_TRUE(service.ImportSource(request, nullptr, &error)) << error.c_str();
        CatalogPromotionRequest promotion;
        promotion.m_recordId = "world.north"; promotion.m_domain = "world"; promotion.m_recordKind = "location";
        promotion.m_subjectRef = "fixture:world:north-gate"; promotion.m_identityKind = "synthetic";
        promotion.m_ownerPackId = "population.pack.active"; promotion.m_displayName = "North gate";
        promotion.m_evidenceId = "evidence.world.north"; promotion.m_confidence = "documented"; promotion.m_researchStage = "S1";
        ASSERT_TRUE(service.PromoteEvidenceToCatalog(promotion, &error)) << error.c_str();
        ASSERT_TRUE(service.CreateFactionDefinition("Town Guard", faction, &error));
        auto definition = SocietyDefinition(service, faction);
        auto link = SocietyLink("link.world", faction, "jurisdiction", "world.north", "protects");
        link.m_targetSubjectRef = "fixture:world:north-gate"; definition.m_links.push_back(link);
        ASSERT_TRUE(service.SaveFactionDefinition(definition, "Town Guard", &error)) << error.c_str();
        const auto analysis = SocietyPlanningService::Analyze(definition, service.GetCatalog());
        ASSERT_TRUE(analysis.IsValid()); EXPECT_EQ(analysis.m_jurisdictions, 1);
        for (const auto& warning : analysis.m_warnings) { EXPECT_EQ(warning.find("Unverified territory"), AZStd::string::npos); }
        definition.m_links.front().m_targetSubjectRef = "fixture:world:other";
        EXPECT_FALSE(service.SaveFactionDefinition(definition, "Mismatch", &error));
        const auto good = service.GetCatalog().BuildDocument(service.GetWorkspace(), *service.GetWorkspace().FindActiveGameProfile());
        auto duplicate = good; duplicate.m_factionLinks.push_back(duplicate.m_factionLinks.front());
        auto catalog = service.GetCatalog();
        EXPECT_FALSE(catalog.ReplaceFromBoundDocument(duplicate, service.GetWorkspace(),
            *service.GetWorkspace().FindActiveGameProfile(), service.GetSourceRegistry(), &error));
        auto orphan = good; orphan.m_factionLinks.front().m_factionRecordId = "missing.faction";
        EXPECT_FALSE(catalog.ReplaceFromBoundDocument(orphan, service.GetWorkspace(),
            *service.GetWorkspace().FindActiveGameProfile(), service.GetSourceRegistry(), &error));
        EXPECT_EQ(catalog.FindFactionLinks(faction).size(), 1);
    }
} // namespace TaintedGrailModdingSDK
