/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetLocalisationService.h"
#include "ModPackageIo.h"
#include "ProjectImageService.h"
#include "CatalogPersistenceService.h"
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
#include <QCoreApplication>
#include <memory>
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
        class ModPackageExportTests
            : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                if (!QCoreApplication::instance()) { m_qt = std::make_unique<QCoreApplication>(m_argc, m_argv); }
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
                if (m_usedSingleton) { FoundationService::Get().Shutdown(); }
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

            int m_argc = 1;
            char m_program[16] = "AssetTests";
            char* m_argv[2] = {m_program, nullptr};
            std::unique_ptr<QCoreApplication> m_qt;
            AZ::ComponentApplication m_application;
            AZ::IO::LocalFileIO m_fileIO;
            bool m_usedSingleton = false;
            bool m_created = false;
            bool m_installedFileIo = false;
        };
    } // namespace

    namespace
    {
        ModPackageContext Snapshot(const FoundationService& s)
        {
            ModPackageContext c; c.m_workspace=s.GetWorkspace(); c.m_workspaceFile=s.GetWorkspaceFilePath();
            c.m_root=s.GetWorkspaceRootPath(); c.m_selectedPack=s.GetActivePack()->m_packId;
            c.m_packs=s.GetPacks(); c.m_catalog=s.GetCatalog(); c.m_evidence=s.GetSourceRegistry(); return c;
        }
        bool PackageContent(FoundationService& s,const QString& root,AZStd::string& error)
        {
            if (!PrepareCompletionService(s,root,error)) { return false; }
            AZStd::string item,actor,recipe,troop,encounter,culture,faction,place,scene,gate,camp,path,quest,image,text;
            if (!s.CreateEconomyRecord("item","Package item",item,&error)
                || !s.CreateEconomyRecord("recipe","Package recipe",recipe,&error)
                || !s.CreatePopulationRecord("actor","Package actor",{},actor,&error)
                || !s.CreatePopulationRecord("troop","Package troop",actor,troop,&error)
                || !s.CreateEncounterDefinition("Package encounter",troop,encounter,&error)
                || !s.CreateCultureProfile("Package culture",culture,&error)
                || !s.CreateFactionDefinition("Package faction",faction,&error)
                || !s.CreateWorldPlace("region","Package region",{},place,&error)
                || !s.CreateWorldPlace("scene","Package scene",place,scene,&error)
                || !s.CreateWorldPlace("location","Package gate",scene,gate,&error)
                || !s.CreateWorldPlace("location","Package camp",scene,camp,&error)
                || !s.CreateWorldPath("route","Package route",scene,path,&error)
                || !s.CreateQuestDefinition("Package quest",quest,&error)) { return false; }
            auto route=s.GetCatalog().FindWorldPathDefinition(path);
            WorldPathNode first; first.m_nodeId="package.node.gate"; first.m_pathRecordId=path; first.m_locationRecordId=gate;
            auto second=first; second.m_nodeId="package.node.camp"; second.m_locationRecordId=camp;
            route.m_nodes={first,second};
            WorldPathEdge edge; edge.m_edgeId="package.edge.gate-camp"; edge.m_pathRecordId=path;
            edge.m_fromNodeId=first.m_nodeId; edge.m_toNodeId=second.m_nodeId; edge.m_travelMode="walk"; edge.m_travelCost=1.5;
            route.m_edges={edge};
            FactionDefinition society{*s.GetCatalog().FindFactionProfile(faction),{}};
            society.m_profile.m_cultureRecordId=culture;
            FactionLink link; link.m_linkId="package.link.guard"; link.m_factionRecordId=faction;
            link.m_kind="member"; link.m_targetRecordId=actor; link.m_value="leader"; society.m_links={link};
            if (!s.SaveWorldPath(route,"Package route",&error)
                || !s.SaveFactionDefinition(society,"Package faction",&error)) { return false; }
            EconomyRecipeIngredient ingredient; ingredient.m_linkId="ingredient.package.one";
            ingredient.m_recipeRecordId=recipe; ingredient.m_itemRecordId=item;
            EconomyRecipeOutput output; output.m_linkId="output.package.one"; output.m_recipeRecordId=recipe; output.m_itemRecordId=item;
            if (!s.SaveAuthoredRecipeIngredient(ingredient,&error) || !s.SaveAuthoredRecipeOutput(output,&error)) { return false; }
            QImage pixels(16,16,QImage::Format_ARGB32); pixels.fill(Qt::cyan);
            const auto source=root+"/original.png"; if (!pixels.save(source)) { return false; }
            ProjectAssetProfile asset; asset.m_provenance="Original synthetic package test image";
            asset.m_sourceRights="original_work"; asset.m_redistribution="declared_permitted";
            LocalisationEntry entry; entry.m_key="Package.Name"; entry.m_defaultLanguage="en";
            entry.m_variants={{"en","The Watch"},{"fr","La Garde"}};
            return s.SaveProjectAsset(asset,"Package icon",ToAzString(source),{},image,&error)
                && s.SaveLocalisationEntry(entry,{},text,&error)
                && s.SavePresentationBinding(item,"icon",image,{},&error)
                && s.SavePresentationBinding(actor,"portrait",image,{},&error)
                && s.SavePresentationBinding(quest,"name",text,{},&error);
        }
        QByteArray FileBytes(const QString& path)
        { QFile f(path); if (!f.open(QIODevice::ReadOnly)) { return {}; } return f.readAll(); }
        bool PutBytes(const QString& path,const QByteArray& bytes)
        { QFile f(path); return f.open(QIODevice::WriteOnly|QIODevice::Truncate)&&f.write(bytes)==bytes.size(); }
    }
    TEST_F(ModPackageExportTests, CompleteAuthoringPackageExportsAndReopensWithImagesTextAndBindings)
    {
        QTemporaryDir root,outputs; ASSERT_TRUE(root.isValid()); ASSERT_TRUE(outputs.isValid());
        FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error;
        ASSERT_TRUE(PackageContent(s,root.path(),error)) << error.c_str();
        const auto context=Snapshot(s);
        auto preview=ModPackageService::Preview(context); ASSERT_TRUE(preview.IsSuccess()) << preview.GetError().c_str();
        EXPECT_GT(preview.GetValue().m_entries.size(),4);
        const auto archive=outputs.path()+"/mod.tgmod";
        auto exported=ModPackageService::Export(context,preview.GetValue(),archive);
        ASSERT_TRUE(exported.IsSuccess()) << exported.GetError().c_str();
        auto inspected=ModPackageService::Inspect(archive); ASSERT_TRUE(inspected.IsSuccess()) << inspected.GetError().c_str();
        EXPECT_EQ(inspected.GetValue().m_fingerprint,preview.GetValue().m_fingerprint);
        EXPECT_FALSE(FileBytes(archive).contains(root.path().toUtf8()));
        const auto destination=outputs.path()+"/reopened";
        auto imported=ModPackageService::Import(archive,inspected.GetValue().m_fingerprint,context.m_workspace,destination);
        ASSERT_TRUE(imported.IsSuccess()) << imported.GetError().c_str();
        s.Shutdown();
        auto& reopened=FoundationService::Get(); m_usedSingleton=true; reopened.Initialize();
        ASSERT_TRUE(reopened.LoadWorkspace(ToAzString(imported.GetValue().m_path),&error)) << error.c_str();
        ASSERT_TRUE(reopened.LoadPack(ToAzString(destination+"/Packs/"+ToQString(context.m_selectedPack)+"/pack.tgpack.json"),&error)) << error.c_str();
        const auto& c=reopened.GetCatalog();
        EXPECT_EQ(c.GetEconomyItems().size(),1); EXPECT_EQ(c.GetEconomyRecipes().size(),1);
        EXPECT_EQ(c.GetPopulationActorProfiles().size(),1); EXPECT_EQ(c.GetPopulationTroopProfiles().size(),1);
        EXPECT_EQ(c.GetEncounterDefinitions().size(),1); EXPECT_EQ(c.GetCultureProfiles().size(),1);
        EXPECT_EQ(c.GetFactionProfiles().size(),1); EXPECT_EQ(c.GetWorldPlaces().size(),4);
        EXPECT_EQ(c.GetWorldPaths().size(),1); EXPECT_EQ(c.GetWorldPathNodes().size(),2);
        EXPECT_EQ(c.GetWorldPathEdges().size(),1); EXPECT_EQ(c.GetFactionLinks().size(),1);
        EXPECT_EQ(c.GetRecipeIngredients().size(),1); EXPECT_EQ(c.GetRecipeOutputs().size(),1);
        EXPECT_EQ(c.GetPopulationTroopMembers().size(),1);
        EXPECT_EQ(c.GetQuestProfiles().size(),1); EXPECT_EQ(c.GetProjectAssets().size(),1);
        EXPECT_EQ(c.GetLocalisationEntries().size(),1); EXPECT_EQ(c.GetPresentationBindings().size(),3);
        EXPECT_EQ(c.GetLocalisationEntries().front().m_variants.size(),2);
        EXPECT_EQ(c.GetRecords().size(),context.m_catalog.GetRecords().size());
        QImage actual; ASSERT_TRUE(reopened.ReadProjectAssetImage(c.GetProjectAssets().front().m_recordId,actual,&error)) << error.c_str();
        EXPECT_EQ(actual.pixelColor(0,0),QColor(Qt::cyan));
        auto second=ModPackageService::Preview(Snapshot(reopened)); ASSERT_TRUE(second.IsSuccess()) << second.GetError().c_str();
        EXPECT_EQ(second.GetValue().m_entries,preview.GetValue().m_entries);
        EXPECT_EQ(second.GetValue().m_fingerprint,preview.GetValue().m_fingerprint);
        for (const auto& r:c.GetRecords()) { EXPECT_TRUE(r.m_allowedUsages.empty()); EXPECT_EQ(r.m_validationState,"unvalidated"); }
    }
    TEST_F(ModPackageExportTests, ExportsAreDeterministicAndNeverReplaceAnExistingFile)
    {
        QTemporaryDir root,out; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error;
        ASSERT_TRUE(PackageContent(s,root.path(),error)) << error.c_str();
        auto c=Snapshot(s); auto p=ModPackageService::Preview(c); ASSERT_TRUE(p.IsSuccess()) << p.GetError().c_str();
        const auto one=out.path()+"/one.tgmod",two=out.path()+"/two.tgmod";
        ASSERT_TRUE(ModPackageService::Export(c,p.GetValue(),one).IsSuccess());
        ASSERT_TRUE(ModPackageService::Export(c,p.GetValue(),two).IsSuccess());
        EXPECT_EQ(FileBytes(one),FileBytes(two));
        const auto before=FileBytes(one);
        EXPECT_FALSE(ModPackageService::Export(c,p.GetValue(),one).IsSuccess());
        EXPECT_EQ(FileBytes(one),before);
        EXPECT_FALSE(ModPackageService::Export(c,p.GetValue(),root.path()+"/inside.tgmod").IsSuccess());
    }
    TEST_F(ModPackageExportTests, MissingChangedAndUnreviewedImageBlockExport)
    {
        QTemporaryDir root,out; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error;
        ASSERT_TRUE(PackageContent(s,root.path(),error)) << error.c_str();
        auto c=Snapshot(s); auto p=ModPackageService::Preview(c); ASSERT_TRUE(p.IsSuccess()) << p.GetError().c_str();
        const auto asset=c.m_catalog.GetProjectAssets().front();
        const auto path=root.path()+"/"+ToQString(asset.m_sourcePath);
        const auto good=FileBytes(path);
        ASSERT_TRUE(PutBytes(path,good+"changed"));
        EXPECT_FALSE(ModPackageService::Export(c,p.GetValue(),out.path()+"/bad.tgmod").IsSuccess());
        EXPECT_FALSE(QFileInfo::exists(out.path()+"/bad.tgmod"));
        ASSERT_TRUE(QFile::remove(path)); EXPECT_FALSE(ModPackageService::Preview(c).IsSuccess());
        ASSERT_TRUE(PutBytes(path,good));
        auto edited=asset; edited.m_redistribution="not_reviewed"; AZStd::string id;
        ASSERT_TRUE(s.SaveProjectAsset(edited,"Package icon",{},AssetLocalisationService::Revision(asset,"Package icon"),id,&error)) << error.c_str();
        EXPECT_FALSE(ModPackageService::Preview(Snapshot(s)).IsSuccess());
    }
    TEST_F(ModPackageExportTests, DependencyClosureMissingCyclesConflictsAndUnrelatedPacks)
    {
        QTemporaryDir root; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error;
        ASSERT_TRUE(PackageContent(s,root.path(),error)) << error.c_str();
        auto c=Snapshot(s); auto active=*s.GetActivePack(); auto dependency=MakePack("dependency.pack"); auto unrelated=MakePack("unrelated.pack");
        active.m_dependencies={dependency.m_packId};
        auto select=[&](AZStd::vector<PackManifest> packs) { return ModPackagePlan::Select(c.m_workspace,packs,active.m_packId,c.m_catalog,c.m_evidence); };
        EXPECT_FALSE(select({active}).IsSuccess());
        auto okay=select({unrelated,active,dependency}); ASSERT_TRUE(okay.IsSuccess()) << okay.GetError().c_str();
        EXPECT_EQ(okay.GetValue().m_packs.size(),2);
        dependency.m_dependencies={active.m_packId}; EXPECT_FALSE(select({active,dependency}).IsSuccess()); dependency.m_dependencies.clear();
        active.m_incompatibilities={dependency.m_packId}; EXPECT_FALSE(select({active,dependency}).IsSuccess()); active.m_incompatibilities.clear();
        EXPECT_FALSE(select({active,dependency,dependency}).IsSuccess());
    }
    TEST_F(ModPackageExportTests, CancelAndExistingDestinationPreserveAllSourceAndTargetBytes)
    {
        QTemporaryDir root,out; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error;
        ASSERT_TRUE(PackageContent(s,root.path(),error)) << error.c_str();
        auto c=Snapshot(s); auto p=ModPackageService::Preview(c); ASSERT_TRUE(p.IsSuccess()) << p.GetError().c_str();
        auto archive=out.path()+"/mod.tgmod"; ASSERT_TRUE(ModPackageService::Export(c,p.GetValue(),archive).IsSuccess());
        const auto before=FileBytes(ToQString(s.GetCatalogFilePath()));
        ModPackageProgress cancel; cancel.m_cancel=std::make_shared<std::atomic_bool>(true);
        EXPECT_FALSE(ModPackageService::Preview(c,cancel).IsSuccess());
        EXPECT_FALSE(ModPackageService::Export(c,p.GetValue(),out.path()+"/cancel.tgmod",cancel).IsSuccess());
        EXPECT_FALSE(ModPackageService::Import(archive,p.GetValue().m_fingerprint,c.m_workspace,out.path()+"/cancelled",cancel).IsSuccess());
        EXPECT_FALSE(QFileInfo::exists(out.path()+"/cancelled"));
        ASSERT_TRUE(QDir().mkpath(out.path()+"/existing"));
        ASSERT_TRUE(PutBytes(out.path()+"/existing/sentinel","keep"));
        EXPECT_FALSE(ModPackageService::Import(archive,p.GetValue().m_fingerprint,c.m_workspace,out.path()+"/existing").IsSuccess());
        EXPECT_EQ(FileBytes(out.path()+"/existing/sentinel"),"keep");
        EXPECT_EQ(FileBytes(ToQString(s.GetCatalogFilePath())),before);
        cancel.m_cancel->store(false);
        cancel.m_update=[flag=cancel.m_cancel](int n,const QString& phase) { if (n>=1 && phase.startsWith("Restored")) { flag->store(true); } };
        EXPECT_FALSE(ModPackageService::Import(archive,p.GetValue().m_fingerprint,c.m_workspace,out.path()+"/during",cancel).IsSuccess());
        EXPECT_FALSE(QFileInfo::exists(out.path()+"/during"));
    }
    TEST_F(ModPackageExportTests, CorruptTruncatedDuplicateFutureAndUnsafeArchivesAreRejected)
    {
        QTemporaryDir root,out; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error;
        ASSERT_TRUE(PackageContent(s,root.path(),error)) << error.c_str();
        auto c=Snapshot(s); auto p=ModPackageService::Preview(c); ASSERT_TRUE(p.IsSuccess()) << p.GetError().c_str();
        const auto good=out.path()+"/good.tgmod",bad=out.path()+"/bad.tgmod";
        ASSERT_TRUE(ModPackageService::Export(c,p.GetValue(),good).IsSuccess());
        const auto original=FileBytes(good); auto object=QJsonDocument::fromJson(original).object();
        ASSERT_TRUE(PutBytes(bad,original.left(original.size()/2))); EXPECT_FALSE(ModPackageService::Inspect(bad).IsSuccess());
        ASSERT_TRUE(PutBytes(bad,"{\"manifest\":{},\"manifest\":{}}")); EXPECT_FALSE(ModPackageService::Inspect(bad).IsSuccess());
        auto reject=[&](QJsonObject changed)
        { ASSERT_TRUE(PutBytes(bad,QJsonDocument(changed).toJson(QJsonDocument::Compact))); EXPECT_FALSE(ModPackageService::Inspect(bad).IsSuccess()); };
        auto changed=object; changed["manifest_sha256"]="sha256:"+QString(64,'0'); reject(changed);
        changed=object; changed["extra"]=true; reject(changed);
        auto manifest=object["manifest"].toObject(); manifest["version"]=2; changed=object; changed["manifest"]=manifest; reject(changed);
        changed=object; auto payloads=object["payloads"].toArray(); auto first=payloads[0].toObject(); first["base64"]="!!!";
        payloads[0]=first; changed["payloads"]=payloads; reject(changed);
        for (const QString& path:{"../escape","/absolute","C:/drive","safe/../escape","safe\\file","a/CON.json","a/nul","a/file.","a//file","a/./file"})
        { EXPECT_FALSE(ModPackageIo::SafePath(path)) << path.toStdString(); }
        for (const QString& unsafe:{"../escape", "C:/escape", "Sources/CON.json"})
        {
            auto changedInventory=object["manifest"].toObject()["entries"].toArray();
            auto changedPayloads=object["payloads"].toArray();
            auto entry=changedInventory[0].toObject(); entry["path"]=unsafe; changedInventory[0]=entry;
            auto payload=changedPayloads[0].toObject(); payload["path"]=unsafe; changedPayloads[0]=payload;
            auto changedManifest=object["manifest"].toObject(); changedManifest["entries"]=changedInventory;
            auto forged=object; forged["manifest"]=changedManifest; forged["payloads"]=changedPayloads;
            forged["manifest_sha256"]=ModPackageIo::Hash(ModPackageIo::Json(changedManifest));
            reject(forged);
        }
        EXPECT_TRUE(ModPackageIo::SafePath("Media/Owned/abc/image.png"));
        EXPECT_FALSE(ModPackageService::Import(good,"sha256:"+QString(64,'0'),c.m_workspace,out.path()+"/wrong").IsSuccess());
        auto mismatch=c.m_workspace; mismatch.m_gameProfiles.front().m_gameVersion="2.0.0";
        EXPECT_FALSE(ModPackageService::Import(good,p.GetValue().m_fingerprint,mismatch,out.path()+"/mismatch").IsSuccess());
    }
    TEST_F(ModPackageExportTests, PreviewDriftDeclarationsPrivatePathsAndFailedWritesBlockPublication)
    {
        QTemporaryDir root,out; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error;
        ASSERT_TRUE(PackageContent(s,root.path(),error)) << error.c_str();
        auto c=Snapshot(s); auto p=ModPackageService::Preview(c); ASSERT_TRUE(p.IsSuccess()) << p.GetError().c_str();
        AZStd::string id; ASSERT_TRUE(s.CreateEconomyRecord("item","Changed item",id,&error)) << error.c_str();
        EXPECT_FALSE(ModPackageService::Export(Snapshot(s),p.GetValue(),out.path()+"/stale.tgmod").IsSuccess());
        auto pack=*s.GetActivePack(); pack.m_assetPaths={"missing.png"};
        ASSERT_TRUE(s.SavePackAndActivate(pack,s.GetActivePackFilePath(),&error)) << error.c_str();
        EXPECT_FALSE(ModPackageService::Preview(Snapshot(s)).IsSuccess());
        EXPECT_FALSE(ModPackageService::Export(c,p.GetValue(),out.path()+"/missing/file.tgmod").IsSuccess());
        EXPECT_FALSE(ModPackageIo::PortableJson(QJsonObject{{"private","C:/Users/private-user/test"}}));
        EXPECT_FALSE(ModPackageIo::PortableJson(QJsonObject{{"private","/home/private/test"}}));
    }
    TEST_F(ModPackageExportTests, EntryByteAndCaseCollisionLimitsAreEnforced)
    {
        ModPackagePreview p;
        EXPECT_TRUE(ModPackageIo::Add(p,"A/one.json","one").IsSuccess());
        EXPECT_FALSE(ModPackageIo::Add(p,"a/ONE.json","two").IsSuccess());
        EXPECT_FALSE(ModPackageIo::Add(p,"A/one.json","two").IsSuccess());
        EXPECT_TRUE(ModPackageIo::Add(p,"A/one.json","one").IsSuccess());
        p={};
        const QByteArray limit(static_cast<int>(ModPackageService::MaximumDecodedBytes),'x');
        ASSERT_TRUE(ModPackageIo::Add(p,"payload.bin",limit).IsSuccess());
        EXPECT_EQ(p.m_totalBytes,ModPackageService::MaximumDecodedBytes);
        EXPECT_FALSE(ModPackageIo::Add(p,"two.json","x").IsSuccess());
        QByteArray deeplyNested="{\"x\":"+QByteArray(70,'[')+"0"+QByteArray(70,']')+"}";
        EXPECT_FALSE(ModPackageIo::Parse(deeplyNested).IsSuccess());
        QTemporaryDir out;
        ModPackageProgress cancel; cancel.m_cancel=std::make_shared<std::atomic_bool>(true);
        EXPECT_FALSE(ModPackageIo::WriteNew(out.path()+"/cancelled.tgmod","bytes",cancel).IsSuccess());
        EXPECT_FALSE(QFileInfo::exists(out.path()+"/cancelled.tgmod"));
#ifdef Q_OS_WIN
        for (const auto& suffix:{"file:stream.tgmod","CON.tgmod","file. ","folder./file.tgmod"})
        { EXPECT_TRUE(ModPackageIo::Direct(out.path()+"/"+suffix,false).isEmpty()); }
#endif
    }

    TEST_F(ModPackageExportTests, NativeDependenciesCarryOnlyExactUnverifiedIdentityAnchors)
    {
        QTemporaryDir root; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error;
        ASSERT_TRUE(PackageContent(s,root.path(),error)) << error.c_str();
        auto context=Snapshot(s);
        auto document=context.m_catalog.BuildDocument(context.m_workspace,*context.m_workspace.FindActiveGameProfile());
        const auto itemId=document.m_economyItems.front().m_recordId;
        document.m_economyItems.clear();
        const auto* original=context.m_catalog.FindByRecordId(itemId);
        ASSERT_NE(original,nullptr);
        for (auto& record:document.m_records)
        {
            if (record.m_recordId==itemId)
            {
                record.m_identityKind="native"; record.m_ownerPackId.clear();
                record.m_nativeRefExact="fixture-native-guid-0123456789abcdef";
                record.m_displayName="Native payload label must not be exported";
                record.m_sourceScopedRefs={"private-source-lookup"};
            }
        }
        CatalogDatabase withNative;
        ASSERT_TRUE(withNative.ReplaceFromBoundDocument(document,context.m_workspace,*context.m_workspace.FindActiveGameProfile(),context.m_evidence,&error)) << error.c_str();
        auto chosen=ModPackagePlan::Select(context.m_workspace,context.m_packs,context.m_selectedPack,withNative,context.m_evidence);
        ASSERT_TRUE(chosen.IsSuccess()) << chosen.GetError().c_str();
        EXPECT_TRUE(chosen.GetValue().m_catalog.m_economyItems.empty());
        const auto it=AZStd::find_if(chosen.GetValue().m_catalog.m_records.begin(),chosen.GetValue().m_catalog.m_records.end(),
            [&](const auto& r) { return r.m_recordId==itemId; });
        ASSERT_NE(it,chosen.GetValue().m_catalog.m_records.end());
        EXPECT_EQ(it->m_nativeRefExact,"fixture-native-guid-0123456789abcdef");
        EXPECT_EQ(it->m_displayName,itemId); EXPECT_TRUE(it->m_sourceScopedRefs.empty());
        EXPECT_TRUE(it->m_allowedUsages.empty()); EXPECT_EQ(it->m_confidence,"unknown");
    }

}
