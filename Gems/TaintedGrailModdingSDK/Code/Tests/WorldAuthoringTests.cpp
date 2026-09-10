/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "EncounterPlanningService.h"
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
        class WorldAuthoringTests
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
        struct WorldFixture { AZStd::string region, scene, otherScene, gate, bend, camp, road, route; };
        bool MakeWorld(FoundationService& service, WorldFixture& ids, AZStd::string& error)
        {
            if (!service.CreateWorldPlace("region", "Northlands", {}, ids.region, &error)
                || !service.CreateWorldPlace("scene", "Town", ids.region, ids.scene, &error)
                || !service.CreateWorldPlace("scene", "Other scene", ids.region, ids.otherScene, &error)
                || !service.CreateWorldPlace("location", "North Gate", ids.scene, ids.gate, &error)
                || !service.CreateWorldPlace("location", "Road Bend", ids.scene, ids.bend, &error)
                || !service.CreateWorldPlace("location", "Bandit Camp", ids.scene, ids.camp, &error)
                || !service.CreateWorldPath("road", "North Road", ids.scene, ids.road, &error)
                || !service.CreateWorldPath("route", "Guard Patrol", ids.scene, ids.route, &error)) { return false; }
            double coordinate = 0;
            for (const auto& id : {ids.gate, ids.bend, ids.camp})
            {
                auto place = *service.GetCatalog().FindWorldPlace(id); place.m_hasPosition = true;
                place.m_x = coordinate; place.m_z = coordinate / 2; coordinate += 100;
                if (!service.SaveWorldPlace(place, service.GetCatalog().FindByRecordId(id)->m_displayName, &error)) { return false; }
            }
            return true;
        }
        WorldPathDefinition WorldGraph(const FoundationService& service, const WorldFixture& ids)
        {
            auto path = service.GetCatalog().FindWorldPathDefinition(ids.route); path.m_profile.m_roadRecordId = ids.road;
            path.m_profile.m_travelConstraints = "Guards travel during daylight";
            const AZStd::string locations[] = {ids.gate, ids.bend, ids.camp};
            const AZStd::string nodes[] = {"world.node.gate", "world.node.bend", "world.node.camp"};
            for (int index = 0; index < 3; ++index)
            {
                WorldPathNode node; node.m_nodeId = nodes[index]; node.m_pathRecordId = ids.route;
                node.m_locationRecordId = locations[index]; path.m_nodes.push_back(node);
            }
            for (int index = 0; index < 2; ++index)
            {
                WorldPathEdge edge; edge.m_edgeId = index == 0 ? "world.edge.gate-bend" : "world.edge.bend-camp";
                edge.m_pathRecordId = ids.route; edge.m_fromNodeId = nodes[index]; edge.m_toNodeId = nodes[index + 1];
                edge.m_bidirectional = index == 0; edge.m_travelMode = index == 0 ? "walk" : "ride"; edge.m_travelCost = index + 1.5;
                path.m_edges.push_back(edge);
            }
            return path;
        }
        QByteArray WorldBytes(const FoundationService& service)
        { QFile file(ToQString(service.GetCatalogFilePath())); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{}; }
    }
    TEST_F(WorldAuthoringTests, PlacesRoadAndDirectedRouteSurviveReopenAndDeterministicSave)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error; WorldFixture ids;
        ASSERT_TRUE(PrepareCompletionService(service, dir.path(), error)) << error.c_str();
        ASSERT_TRUE(MakeWorld(service, ids, error)) << error.c_str();
        auto path = WorldGraph(service, ids);
        ASSERT_TRUE(service.SaveWorldPath(path, "Guard Patrol", &error)) << error.c_str();
        const auto analysis = WorldPlanningService::Analyze(path, "route", service.GetCatalog());
        ASSERT_TRUE(analysis.IsValid()); EXPECT_TRUE(analysis.m_usesPlanPositions); EXPECT_EQ(analysis.m_components, 1);
        ASSERT_TRUE(service.ReloadSourceEvidence(&error)); ASSERT_TRUE(service.ReloadCatalog(&error)) << error.c_str();
        const auto loaded = service.GetCatalog().FindWorldPathDefinition(ids.route);
        ASSERT_EQ(loaded.m_nodes.size(), 3); ASSERT_EQ(loaded.m_edges.size(), 2);
        EXPECT_EQ(loaded.m_profile.m_roadRecordId, ids.road); EXPECT_EQ(loaded.m_profile.m_travelConstraints, path.m_profile.m_travelConstraints);
        EXPECT_EQ(service.GetCatalog().FindWorldPlace(ids.bend)->m_x, 100);
        EXPECT_EQ(service.GetCatalog().FindWorldPlace(ids.camp)->m_parentRecordId, ids.scene);
        EXPECT_TRUE(service.GetCatalog().FindByRecordId(ids.route)->m_allowedUsages.empty());
        EXPECT_TRUE(service.GetCatalog().FindByRecordId(ids.gate)->m_nativeRefExact.empty());
        EXPECT_EQ(service.GetCatalog().FindByRecordId(ids.gate)->m_ownerPackId, "population.pack.active");
        QTemporaryDir copy; const auto saved = CatalogPersistenceService().Save(
            service.GetCatalog().BuildDocument(service.GetWorkspace(), *service.GetWorkspace().FindActiveGameProfile()), ToAzString(copy.path()));
        ASSERT_TRUE(saved.IsSuccess()); QFile file(ToQString(saved.GetValue())); ASSERT_TRUE(file.open(QIODevice::ReadOnly)); EXPECT_EQ(file.readAll(), WorldBytes(service));
    }
    TEST_F(WorldAuthoringTests, InvalidGraphHierarchyCoordinatesAndLimitsPreserveSavedCatalog)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error; WorldFixture ids;
        ASSERT_TRUE(PrepareCompletionService(service, dir.path(), error)); ASSERT_TRUE(MakeWorld(service, ids, error)) << error.c_str();
        const auto path = WorldGraph(service, ids); ASSERT_TRUE(service.SaveWorldPath(path, "Guard Patrol", &error)) << error.c_str();
        const auto bytes = WorldBytes(service);
        const auto reject = [&](const WorldPathDefinition& value)
        { EXPECT_FALSE(service.SaveWorldPath(value, "Invalid edit", &error)); EXPECT_EQ(WorldBytes(service), bytes); };
        auto bad = path; bad.m_edges.front().m_toNodeId = "missing.node"; reject(bad);
        bad = path; bad.m_edges.front().m_toNodeId = bad.m_edges.front().m_fromNodeId; reject(bad);
        bad = path; bad.m_edges.push_back(bad.m_edges.front()); bad.m_edges.back().m_edgeId = "world.edge.duplicate"; reject(bad);
        bad = path; bad.m_edges.push_back(bad.m_edges.front()); bad.m_edges.back().m_edgeId = "world.edge.reverse";
        AZStd::swap(bad.m_edges.back().m_fromNodeId, bad.m_edges.back().m_toNodeId); bad.m_edges.back().m_bidirectional = false; reject(bad);
        bad = path; bad.m_nodes.back().m_locationRecordId = ids.gate; reject(bad);
        bad = path; bad.m_nodes.front().m_pathRecordId = ids.road; reject(bad);
        bad = path; bad.m_profile.m_sceneRecordId = ids.otherScene; reject(bad);
        bad = path; bad.m_edges.front().m_travelMode = "teleport"; reject(bad);
        for (double cost : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        { bad = path; bad.m_edges.front().m_travelCost = cost; reject(bad); }
        bad = path; bad.m_nodes.resize(129); reject(bad);
        bad = path; bad.m_edges.resize(257); reject(bad);
        bad = path; bad.m_profile.m_travelConstraints = AZStd::string(2049, 'x'); reject(bad);
        auto place = *service.GetCatalog().FindWorldPlace(ids.gate); place.m_parentRecordId = ids.otherScene;
        EXPECT_FALSE(service.SaveWorldPlace(place, "Moved gate", &error)); EXPECT_EQ(WorldBytes(service), bytes);
        place = *service.GetCatalog().FindWorldPlace(ids.gate); place.m_x = std::numeric_limits<double>::quiet_NaN();
        EXPECT_FALSE(service.SaveWorldPlace(place, "Invalid position", &error));
        place = *service.GetCatalog().FindWorldPlace(ids.region); place.m_parentRecordId = ids.scene;
        EXPECT_FALSE(service.SaveWorldPlace(place, "Region cycle", &error)); EXPECT_EQ(WorldBytes(service), bytes);
        AZStd::string created; EXPECT_FALSE(service.CreateWorldPlace("location", "Orphan", {}, created, &error)); EXPECT_TRUE(created.empty());
        EXPECT_FALSE(service.CreateWorldPlace("region", AZStd::string(513, 'x'), {}, created, &error));
    }
    TEST_F(WorldAuthoringTests, GraphReplacementOwnershipAndEvidenceAreStrict)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error; WorldFixture ids;
        ASSERT_TRUE(PrepareCompletionService(service, dir.path(), error)); ASSERT_TRUE(MakeWorld(service, ids, error)) << error.c_str();
        auto path = WorldGraph(service, ids); ASSERT_TRUE(service.SaveWorldPath(path, "Guard Patrol", &error)) << error.c_str();
        auto collision = path; collision.m_edges.front().m_edgeId = ids.road;
        EXPECT_FALSE(service.SaveWorldPath(collision, "Canonical ID collision", &error));
        auto stealing = path; stealing.m_profile = *service.GetCatalog().FindWorldPath(ids.road);
        for (auto& n : stealing.m_nodes) { n.m_pathRecordId = ids.road; } for (auto& e : stealing.m_edges) { e.m_pathRecordId = ids.road; }
        const auto bytes = WorldBytes(service); EXPECT_FALSE(service.SaveWorldPath(stealing, "Steal IDs", &error)); EXPECT_EQ(WorldBytes(service), bytes);
        auto place = *service.GetCatalog().FindWorldPlace(ids.gate);
        place.m_evidenceIds = service.GetCatalog().FindWorldPlace(ids.region)->m_evidenceIds;
        auto candidate = service.GetCatalog(); ASSERT_TRUE(candidate.UpsertWorldPlace(place, &error));
        EXPECT_FALSE(candidate.ValidateIntegrity(service.GetWorkspace(), *service.GetWorkspace().FindActiveGameProfile(), service.GetSourceRegistry(), &error));
        candidate = service.GetCatalog(); auto saved = candidate.FindWorldPathDefinition(ids.route);
        saved.m_edges.front().m_evidenceIds = saved.m_nodes.front().m_evidenceIds;
        ASSERT_TRUE(candidate.ReplaceWorldPath(saved, &error));
        EXPECT_FALSE(candidate.ValidateIntegrity(service.GetWorkspace(), *service.GetWorkspace().FindActiveGameProfile(), service.GetSourceRegistry(), &error));
        path.m_edges.clear(); path.m_nodes.pop_back(); path.m_nodes.front().m_notes = "Changed waypoint";
        ASSERT_TRUE(service.SaveWorldPath(path, "Short patrol", &error)) << error.c_str();
        const auto edited = service.GetCatalog().FindWorldPathDefinition(ids.route);
        EXPECT_EQ(edited.m_nodes.size(), 2); EXPECT_TRUE(edited.m_edges.empty()); EXPECT_TRUE(service.GetCatalog().FindWorldPathDefinition(ids.road).m_nodes.empty());
        EXPECT_EQ(service.GetCatalog().FindByRecordId(ids.route)->m_displayName, "Short patrol");
        auto pack = MakePack("world.other.pack"); ASSERT_TRUE(service.SetActivePack(pack, &error));
        ASSERT_TRUE(QDir().mkpath(dir.path() + "/Packs/world.other.pack"));
        ASSERT_TRUE(service.SaveActivePack(ToAzString(dir.path() + "/Packs/world.other.pack/pack.tgpack.json"), &error));
        EXPECT_FALSE(service.SaveWorldPath(path, "Wrong owner", &error));
        EXPECT_FALSE(service.SaveWorldPlace(*service.GetCatalog().FindWorldPlace(ids.gate), "Wrong owner", &error));
    }
    TEST_F(WorldAuthoringTests, WorldReferencesConnectFactionJurisdictionAndEncounterPlacement)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error; WorldFixture ids;
        ASSERT_TRUE(PrepareCompletionService(service, dir.path(), error)); ASSERT_TRUE(MakeWorld(service, ids, error));
        AZStd::string faction, actor, encounter;
        ASSERT_TRUE(service.CreateFactionDefinition("Town Guard", faction, &error));
        FactionDefinition definition{*service.GetCatalog().FindFactionProfile(faction), {}};
        FactionLink link; link.m_linkId = "world.jurisdiction.gate"; link.m_factionRecordId = faction; link.m_kind = "jurisdiction";
        link.m_targetRecordId = ids.gate; link.m_targetSubjectRef = service.GetCatalog().FindByRecordId(ids.gate)->m_subjectRef; link.m_value = "protects";
        definition.m_links.push_back(link); ASSERT_TRUE(service.SaveFactionDefinition(definition, "Town Guard", &error)) << error.c_str();
        ASSERT_TRUE(service.CreatePopulationRecord("actor", "Guard", {}, actor, &error));
        ASSERT_TRUE(service.CreateEncounterDefinition("Gate watch", actor, encounter, &error));
        auto plan = *service.GetCatalog().FindEncounterDefinition(encounter); plan.m_placementRecordId = ids.gate; plan.m_placementSubjectRef = link.m_targetSubjectRef;
        ASSERT_TRUE(service.SaveEncounterDefinition(plan, "Gate watch", &error)) << error.c_str();
        ASSERT_TRUE(service.ReloadSourceEvidence(&error)); ASSERT_TRUE(service.ReloadCatalog(&error)) << error.c_str();
        EXPECT_EQ(service.GetCatalog().FindFactionLinks(faction).front().m_targetRecordId, ids.gate);
        EXPECT_EQ(service.GetCatalog().FindEncounterDefinition(encounter)->m_placementRecordId, ids.gate);
    }
    TEST_F(WorldAuthoringTests, SchemaFourMigrationRetainsSocietyAndExactOriginalBackup)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error, culture, faction, region;
        ASSERT_TRUE(PrepareCompletionService(service, dir.path(), error));
        ASSERT_TRUE(service.CreateCultureProfile("Culture", culture, &error)); ASSERT_TRUE(service.CreateFactionDefinition("Guard", faction, &error));
        auto object = QJsonDocument::fromJson(WorldBytes(service)).object(); object["SchemaVersion"] = 4;
        for (const auto& key : {"WorldPlaces", "WorldPaths", "WorldPathNodes", "WorldPathEdges"}) { object.remove(key); }
        const auto original = QJsonDocument(object).toJson(); const QString path = ToQString(service.GetCatalogFilePath());
        QFile file(path); ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); ASSERT_EQ(file.write(original), original.size()); file.close();
        ASSERT_TRUE(service.ReloadCatalog(&error)) << error.c_str(); ASSERT_TRUE(service.CreateWorldPlace("region", "Northlands", {}, region, &error)) << error.c_str();
        EXPECT_NE(service.GetCatalog().FindCultureProfile(culture), nullptr); EXPECT_NE(service.GetCatalog().FindFactionProfile(faction), nullptr);
        const auto hash = QCryptographicHash::hash(original, QCryptographicHash::Sha256).toHex();
        QFile backup(path + ".schema-4." + QString::fromLatin1(hash) + ".backup.json"); ASSERT_TRUE(backup.open(QIODevice::ReadOnly)); EXPECT_EQ(backup.readAll(), original);
        EXPECT_EQ(QJsonDocument::fromJson(WorldBytes(service)).object()["SchemaVersion"].toInt(), 5);
    }
    TEST_F(WorldAuthoringTests, MalformedOldFutureAndDuplicateWorldCollectionsAreRejectedAtomically)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error; WorldFixture ids;
        ASSERT_TRUE(PrepareCompletionService(service, dir.path(), error)); ASSERT_TRUE(MakeWorld(service, ids, error));
        ASSERT_TRUE(service.SaveWorldPath(WorldGraph(service, ids), "Patrol", &error)) << error.c_str();
        const auto good = service.GetCatalog().BuildDocument(service.GetWorkspace(), *service.GetWorkspace().FindActiveGameProfile());
        auto published = service.GetCatalog(); const auto reject = [&](const CatalogDocument& value)
        {
            EXPECT_FALSE(published.ReplaceFromBoundDocument(value, service.GetWorkspace(), *service.GetWorkspace().FindActiveGameProfile(), service.GetSourceRegistry(), &error));
            EXPECT_EQ(published.GetWorldPlaces().size(), good.m_worldPlaces.size()); EXPECT_EQ(published.GetWorldPathEdges().size(), 2);
        };
        const auto bytes = WorldBytes(service);
        for (AZ::u32 version : {1, 2, 3, 4, 6})
        {
            auto bad = good; bad.m_schemaVersion = version; reject(bad);
            auto object = QJsonDocument::fromJson(bytes).object(); object["SchemaVersion"] = static_cast<int>(version);
            QFile file(ToQString(service.GetCatalogFilePath())); ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            const auto raw = QJsonDocument(object).toJson(); ASSERT_EQ(file.write(raw), raw.size()); file.close();
            EXPECT_FALSE(CatalogPersistenceService().Load(service.GetWorkspaceRootPath()).IsSuccess());
        }
        auto bad = good; bad.m_worldPlaces.push_back(bad.m_worldPlaces.front()); reject(bad);
        bad = good; bad.m_worldPaths.resize(1001); reject(bad);
        bad = good; bad.m_worldPlaces.resize(5001); reject(bad);
        bad = good; bad.m_worldPathNodes.resize(10001); reject(bad);
        bad = good; bad.m_worldPathEdges.resize(20001); reject(bad);
        bad = good; bad.m_worldPathNodes.push_back(bad.m_worldPathNodes.front()); reject(bad);
        bad = good; bad.m_worldPathEdges.front().m_pathRecordId = "missing.path"; reject(bad);
        bad = good; bad.m_worldPlaces.front().m_evidenceIds = {"missing.evidence"}; reject(bad);
        published.Clear(); EXPECT_TRUE(published.GetWorldPlaces().empty()); EXPECT_TRUE(published.GetWorldPathNodes().empty());
        EXPECT_TRUE(published.GetWorldPaths().empty()); EXPECT_TRUE(published.GetWorldPathEdges().empty());
    }
    TEST_F(WorldAuthoringTests, FailedSaveKeepsPublishedGraphAndSchematicFallbackIsExplicit)
    {
        QTemporaryDir dir; FoundationService service(FoundationWorkspaceLoadDependencies{}); AZStd::string error; WorldFixture ids;
        ASSERT_TRUE(PrepareCompletionService(service, dir.path(), error)); ASSERT_TRUE(MakeWorld(service, ids, error));
        auto path = WorldGraph(service, ids); ASSERT_TRUE(service.SaveWorldPath(path, "Patrol", &error));
        auto place = *service.GetCatalog().FindWorldPlace(ids.gate); place.m_hasPosition = false; place.m_x = 0; place.m_z = 0;
        ASSERT_TRUE(service.SaveWorldPlace(place, "North Gate", &error));
        auto analysis = WorldPlanningService::Analyze(path, "route", service.GetCatalog()); EXPECT_TRUE(analysis.IsValid()); EXPECT_FALSE(analysis.m_usesPlanPositions); EXPECT_FALSE(analysis.m_warnings.empty());
        const auto bytes = WorldBytes(service); const QString filePath = ToQString(service.GetCatalogFilePath()), held = filePath + ".held";
        ASSERT_TRUE(QFile::rename(filePath, held)); ASSERT_TRUE(QDir().mkdir(filePath));
        path.m_edges.clear(); EXPECT_FALSE(service.SaveWorldPath(path, "Unsaved", &error));
        EXPECT_EQ(service.GetCatalog().FindWorldPathDefinition(ids.route).m_edges.size(), 2); EXPECT_EQ(service.GetCatalog().FindByRecordId(ids.route)->m_displayName, "Patrol");
        ASSERT_TRUE(QDir().rmdir(filePath)); ASSERT_TRUE(QFile::rename(held, filePath)); EXPECT_EQ(WorldBytes(service), bytes);
    }
}
