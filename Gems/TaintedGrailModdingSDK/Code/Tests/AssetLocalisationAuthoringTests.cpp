/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetLocalisationService.h"
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
        class AssetLocalisationAuthoringTests
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
            bool m_created = false;
            bool m_installedFileIo = false;
        };
    } // namespace






    namespace
    {
        LocalisationEntry TextEntry()
        { LocalisationEntry p; p.m_key="Item.Sword.Name"; p.m_defaultLanguage="en"; p.m_variants={{"fr","\xc3\x89p\xc3\xa9""e du guet"},{"en","Watch sword"}}; return p; }
        ProjectAssetProfile ImageEntry()
        { ProjectAssetProfile p; p.m_provenance="Synthetic test artwork"; p.m_sourceRights="original_work"; return p; }
        QString MakeImage(const QString& root, const QString& name="source.png", QRgb colour=qRgb(42,100,180))
        { QImage image(64,48,QImage::Format_ARGB32); image.fill(colour); const auto file=root+"/"+name; return image.save(file,"PNG") ? file : QString{}; }
        QByteArray Bytes(const QString& path)
        { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray{}; }
        bool Write(const QString& path,const QByteArray& bytes)
        { QFile f(path); return f.open(QIODevice::WriteOnly|QIODevice::Truncate) && f.write(bytes)==bytes.size(); }
        AZStd::string TextRevision(const FoundationService& s,const AZStd::string& id)
        { return AssetLocalisationService::Revision(*s.GetCatalog().FindLocalisationEntry(id)); }
        AZStd::string BindingRevision(const FoundationService& s,const AZStd::string& target,const AZStd::string& slot)
        {
            const auto* b=s.GetCatalog().FindPresentationBinding(s.GetActivePack()->m_packId,target,slot);
            return b ? AssetLocalisationService::Revision(*b) : AZStd::string{};
        }
    }
    TEST_F(AssetLocalisationAuthoringTests, TextValidationFallbackAndCanonicalRevision)
    {
        auto p=TextEntry(); p.m_recordId="text.test.entry";
        ASSERT_TRUE(AssetLocalisationService::ValidateEntry(p).IsSuccess());
        const auto exact=AssetLocalisationService::Resolve(p,"fr"), fallback=AssetLocalisationService::Resolve(p,"de");
        EXPECT_EQ(exact.m_text,"\xc3\x89p\xc3\xa9""e du guet"); EXPECT_FALSE(exact.m_usedFallback);
        EXPECT_EQ(fallback.m_text,"Watch sword"); EXPECT_EQ(fallback.m_language,"en"); EXPECT_TRUE(fallback.m_usedFallback);
        EXPECT_FALSE(AssetLocalisationService::Resolve(p,"INVALID").IsSuccess());
        const auto revision=AssetLocalisationService::Revision(p); AZStd::swap(p.m_variants[0],p.m_variants[1]);
        EXPECT_EQ(AssetLocalisationService::Revision(p),revision); p.m_variants[0].m_text+="!";
        EXPECT_NE(AssetLocalisationService::Revision(p),revision);
        for (const auto* language : {"e","EN","en-","en--gb","en-1","en_gb"})
        { auto bad=p; bad.m_defaultLanguage=language; EXPECT_FALSE(AssetLocalisationService::ValidateEntry(bad).IsSuccess()); }
        for (const auto* key : {"","space key","../bad/key","key\nline"})
        { auto bad=p; bad.m_key=key; EXPECT_FALSE(AssetLocalisationService::ValidateEntry(bad).IsSuccess()); }
        for (const auto* bytes : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"})
        { EXPECT_FALSE(AssetLocalisationService::IsText(bytes, 8192, true)); }
        EXPECT_TRUE(AssetLocalisationService::IsText("\xf0\x9f\x9b\xa1", 8192, true));
        auto bad=p; bad.m_variants.push_back(bad.m_variants.front()); EXPECT_FALSE(AssetLocalisationService::ValidateEntry(bad).IsSuccess());
        bad=p; bad.m_defaultLanguage="de"; EXPECT_FALSE(AssetLocalisationService::ValidateEntry(bad).IsSuccess());
        bad=p; bad.m_variants[0].m_text=AZStd::string(8193,'x'); EXPECT_FALSE(AssetLocalisationService::ValidateEntry(bad).IsSuccess());
        bad=p; bad.m_variants[0].m_text=" \n\t"; EXPECT_FALSE(AssetLocalisationService::ValidateEntry(bad).IsSuccess());
        bad=p; bad.m_variants.resize(33); EXPECT_FALSE(AssetLocalisationService::ValidateEntry(bad).IsSuccess());
    }
    TEST_F(AssetLocalisationAuthoringTests, TextCreateEditReopenDuplicateAndStaleWrite)
    {
        QTemporaryDir dir; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id;
        ASSERT_TRUE(PrepareCompletionService(s,dir.path(),error)) << error.c_str();
        ASSERT_TRUE(s.SaveLocalisationEntry(TextEntry(),{},id,&error)) << error.c_str();
        const auto revision=TextRevision(s,id); const auto initial=Bytes(ToQString(s.GetCatalogFilePath()));
        AZStd::string duplicate; EXPECT_FALSE(s.SaveLocalisationEntry(TextEntry(),{},duplicate,&error)); EXPECT_TRUE(duplicate.empty());
        EXPECT_EQ(Bytes(ToQString(s.GetCatalogFilePath())),initial);
        auto p=*s.GetCatalog().FindLocalisationEntry(id); p.m_variants.front().m_text="Updated";
        AZStd::string updated; ASSERT_TRUE(s.SaveLocalisationEntry(p,revision,updated,&error)) << error.c_str(); EXPECT_EQ(updated,id);
        const auto savedRevision=TextRevision(s,id);
        EXPECT_FALSE(s.SaveLocalisationEntry(p,revision,updated,&error));
        ASSERT_TRUE(s.ReloadSourceEvidence(&error)); ASSERT_TRUE(s.ReloadCatalog(&error)) << error.c_str();
        EXPECT_EQ(TextRevision(s,id),savedRevision);
    }
    TEST_F(AssetLocalisationAuthoringTests, ImageCreatePreviewReopenAndMissingFileRepair)
    {
        QTemporaryDir dir; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id;
        ASSERT_TRUE(PrepareCompletionService(s,dir.path(),error)); const auto source=MakeImage(dir.path()); ASSERT_FALSE(source.isEmpty());
        const auto original=Bytes(source); ASSERT_TRUE(s.SaveProjectAsset(ImageEntry(),"Watch portrait",ToAzString(source),{},id,&error)) << error.c_str();
        auto p=*s.GetCatalog().FindProjectAsset(id); EXPECT_EQ(p.m_width,64); EXPECT_EQ(p.m_height,48);
        EXPECT_FALSE(QDir::isAbsolutePath(ToQString(p.m_sourcePath))); EXPECT_EQ(Bytes(source),original);
        QImage preview; ASSERT_TRUE(s.ReadProjectAssetImage(id,preview,&error)); EXPECT_EQ(preview.pixel(0,0),qRgb(42,100,180));
        ASSERT_TRUE(s.ReloadSourceEvidence(&error)); ASSERT_TRUE(s.ReloadCatalog(&error)) << error.c_str(); ASSERT_TRUE(s.ReadProjectAssetImage(id,preview,&error));
        const auto managed=dir.path()+"/"+ToQString(p.m_sourcePath); ASSERT_TRUE(QFile::remove(managed));
        EXPECT_FALSE(s.ReadProjectAssetImage(id,preview,&error)); EXPECT_TRUE(preview.isNull());
        ASSERT_TRUE(s.ReloadCatalog(&error)) << error.c_str();
        AZStd::string same; ASSERT_TRUE(s.SaveProjectAsset(p,"Watch portrait",ToAzString(source),AssetLocalisationService::Revision(p,"Watch portrait"),same,&error)) << error.c_str();
        EXPECT_EQ(same,id); EXPECT_TRUE(s.ReadProjectAssetImage(id,preview,&error));
        EXPECT_EQ(Bytes(source),original);
    }
    TEST_F(AssetLocalisationAuthoringTests, ImageContentSignatureSizeCorruptionAndProtectedPaths)
    {
        QTemporaryDir dir; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error;
        ASSERT_TRUE(PrepareCompletionService(s,dir.path(),error)); const auto root=ToAzString(dir.path()); const auto& w=s.GetWorkspace();
        const auto disguised=MakeImage(dir.path(),"image.data"); ASSERT_FALSE(disguised.isEmpty());
        EXPECT_TRUE(ProjectImageService::ReadSource(ToAzString(disguised),w,root).IsSuccess());
        QImage jpeg(24,16,QImage::Format_RGB32); jpeg.fill(Qt::red); ASSERT_TRUE(jpeg.save(dir.path()+"/photo.jpg","JPEG"));
        EXPECT_TRUE(ProjectImageService::ReadSource(ToAzString(dir.path()+"/photo.jpg"),w,root).IsSuccess());
        const auto invalid=dir.path()+"/bad.png"; ASSERT_TRUE(Write(invalid,"not an image"));
        EXPECT_FALSE(ProjectImageService::ReadSource(ToAzString(invalid),w,root).IsSuccess());
        ASSERT_TRUE(Write(invalid,Bytes(disguised).left(35))); EXPECT_FALSE(ProjectImageService::ReadSource(ToAzString(invalid),w,root).IsSuccess());
        ASSERT_TRUE(Write(invalid,QByteArray(8*1024*1024+1,'x'))); EXPECT_FALSE(ProjectImageService::ReadSource(ToAzString(invalid),w,root).IsSuccess());
        QImage huge(4096,1025,QImage::Format_RGB32); huge.fill(Qt::blue); ASSERT_TRUE(huge.save(invalid,"PNG"));
        EXPECT_FALSE(ProjectImageService::ReadSource(ToAzString(invalid),w,root).IsSuccess());
        for (const auto* folder : {"Game","Extracted","Sources","PreviewArtifacts"})
        {
            const auto path=dir.path()+"/"+folder; ASSERT_TRUE(QDir().mkpath(path)); const auto file=MakeImage(path);
            EXPECT_FALSE(ProjectImageService::ReadSource(ToAzString(file),w,root).IsSuccess()) << folder;
        }
        EXPECT_FALSE(ProjectImageService::ReadSource("relative.png",w,root).IsSuccess());
        EXPECT_FALSE(ProjectImageService::ReadSource(ToAzString(disguised),w,"relative").IsSuccess());
    }
    TEST_F(AssetLocalisationAuthoringTests, ChangedManagedBytesAndExistingCollisionAreNotOverwritten)
    {
        QTemporaryDir dir; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id;
        ASSERT_TRUE(PrepareCompletionService(s,dir.path(),error)); const auto source=MakeImage(dir.path());
        ASSERT_TRUE(s.SaveProjectAsset(ImageEntry(),"Icon",ToAzString(source),{},id,&error)) << error.c_str();
        const auto p=*s.GetCatalog().FindProjectAsset(id); const auto path=dir.path()+"/"+ToQString(p.m_sourcePath);
        const auto replacement=Bytes(MakeImage(dir.path(),"other.png",qRgb(200,20,40))); ASSERT_TRUE(Write(path,replacement));
        QImage preview; EXPECT_FALSE(s.ReadProjectAssetImage(id,preview,&error)); EXPECT_TRUE(preview.isNull());
        AZStd::string out; EXPECT_FALSE(s.SaveProjectAsset(p,"Icon",ToAzString(source),AssetLocalisationService::Revision(p,"Icon"),out,&error));
        EXPECT_EQ(Bytes(path),replacement);
        auto damaged=p; damaged.m_sourcePath="../escape.png"; EXPECT_FALSE(AssetLocalisationService::ValidateAsset(damaged,s.GetActivePack()->m_packId).IsSuccess());
        damaged=p; damaged.m_sourceRights="licensed"; damaged.m_licence.clear(); EXPECT_FALSE(AssetLocalisationService::ValidateAsset(damaged,s.GetActivePack()->m_packId).IsSuccess());
    }
    TEST_F(AssetLocalisationAuthoringTests, AssignReplaceClearReopenAndRejectWrongTypes)
    {
        QTemporaryDir dir; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error,text,image,actor,quest;
        ASSERT_TRUE(PrepareCompletionService(s,dir.path(),error));
        ASSERT_TRUE(s.CreatePopulationRecord("actor","Watch keeper",{},actor,&error)) << error.c_str();
        ASSERT_TRUE(s.CreateQuestDefinition("Watch quest",quest,&error)) << error.c_str();
        ASSERT_TRUE(s.SaveLocalisationEntry(TextEntry(),{},text,&error)) << error.c_str();
        ASSERT_TRUE(s.SaveProjectAsset(ImageEntry(),"Portrait",ToAzString(MakeImage(dir.path())),{},image,&error)) << error.c_str();
        ASSERT_TRUE(s.SavePresentationBinding(actor,"portrait",image,{},&error)) << error.c_str();
        ASSERT_TRUE(s.SavePresentationBinding(quest,"name",text,{},&error)) << error.c_str();
        const auto revision=BindingRevision(s,actor,"portrait");
        EXPECT_FALSE(s.SavePresentationBinding(actor,"portrait",text,revision,&error));
        EXPECT_FALSE(s.SavePresentationBinding(quest,"portrait",image,{},&error));
        EXPECT_FALSE(s.SavePresentationBinding("missing.target","name",text,{},&error));
        EXPECT_FALSE(s.SavePresentationBinding(actor,"portrait",image,{},&error));
        const auto bindingId=s.GetCatalog().FindPresentationBinding(s.GetActivePack()->m_packId,actor,"portrait")->m_bindingId;
        ASSERT_TRUE(s.SavePresentationBinding(actor,"portrait",{},revision,&error)) << error.c_str();
        EXPECT_FALSE(s.SavePresentationBinding(actor,"portrait",image,revision,&error));
        ASSERT_TRUE(s.ReloadSourceEvidence(&error)); ASSERT_TRUE(s.ReloadCatalog(&error)) << error.c_str();
        const auto* b=s.GetCatalog().FindPresentationBinding(s.GetActivePack()->m_packId,actor,"portrait");
        ASSERT_NE(b,nullptr); EXPECT_EQ(b->m_bindingId,bindingId); EXPECT_TRUE(b->m_valueRecordId.empty());
        ASSERT_TRUE(s.SavePresentationBinding(actor,"portrait",image,AssetLocalisationService::Revision(*b),&error)) << error.c_str();
    }
    TEST_F(AssetLocalisationAuthoringTests, PackOwnershipAndDuplicateKeysAreScoped)
    {
        QTemporaryDir dir; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id,actor,other;
        ASSERT_TRUE(PrepareCompletionService(s,dir.path(),error)); ASSERT_TRUE(s.SaveLocalisationEntry(TextEntry(),{},id,&error));
        ASSERT_TRUE(s.CreatePopulationRecord("actor","Owned actor",{},actor,&error));
        const auto saved=*s.GetCatalog().FindLocalisationEntry(id); const auto revision=TextRevision(s,id);
        ASSERT_TRUE(s.SetActivePack(MakePack("asset.other.pack"),&error)); ASSERT_TRUE(QDir().mkpath(dir.path()+"/Packs/asset.other.pack"));
        ASSERT_TRUE(s.SaveActivePack(ToAzString(dir.path()+"/Packs/asset.other.pack/pack.tgpack.json"),&error));
        EXPECT_FALSE(s.SaveLocalisationEntry(saved,revision,other,&error));
        ASSERT_TRUE(s.SaveLocalisationEntry(TextEntry(),{},other,&error)) << error.c_str(); EXPECT_NE(id,other);
        EXPECT_FALSE(s.SavePresentationBinding(actor,"name",other,{},&error));
        AZStd::string otherActor; ASSERT_TRUE(s.CreatePopulationRecord("actor","Other actor",{},otherActor,&error));
        EXPECT_FALSE(s.SavePresentationBinding(otherActor,"name",id,{},&error));
        EXPECT_TRUE(s.SavePresentationBinding(otherActor,"name",other,{},&error)) << error.c_str();
    }
    TEST_F(AssetLocalisationAuthoringTests, CatalogWriteFailureKeepsPublishedTextAndAssignments)
    {
        QTemporaryDir dir; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id,actor;
        ASSERT_TRUE(PrepareCompletionService(s,dir.path(),error)); ASSERT_TRUE(s.SaveLocalisationEntry(TextEntry(),{},id,&error));
        ASSERT_TRUE(s.CreatePopulationRecord("actor","Actor",{},actor,&error)); ASSERT_TRUE(s.SavePresentationBinding(actor,"name",id,{},&error));
        auto p=*s.GetCatalog().FindLocalisationEntry(id); const auto revision=TextRevision(s,id); p.m_variants.front().m_text="Should not persist";
        const auto path=ToQString(s.GetCatalogFilePath()),held=path+".held"; const auto before=Bytes(path);
        ASSERT_TRUE(QFile::rename(path,held)); ASSERT_TRUE(QDir().mkpath(path));
        AZStd::string updated; EXPECT_FALSE(s.SaveLocalisationEntry(p,revision,updated,&error));
        EXPECT_FALSE(s.SavePresentationBinding(actor,"name",{},BindingRevision(s,actor,"name"),&error));
        EXPECT_EQ(TextRevision(s,id),revision); EXPECT_EQ(s.GetCatalog().FindPresentationBinding(s.GetActivePack()->m_packId,actor,"name")->m_valueRecordId,id);
        EXPECT_TRUE(QDir().rmdir(path)); EXPECT_TRUE(QFile::rename(held,path)); EXPECT_EQ(Bytes(path),before);
    }
    TEST_F(AssetLocalisationAuthoringTests, SchemaSixMigrationPreservesQuestAndVerifiedBackup)
    {
        QTemporaryDir dir; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error,quest,id;
        ASSERT_TRUE(PrepareCompletionService(s,dir.path(),error)); ASSERT_TRUE(s.CreateQuestDefinition("Existing quest",quest,&error));
        const auto path=ToQString(s.GetCatalogFilePath()); auto object=QJsonDocument::fromJson(Bytes(path)).object();
        object["SchemaVersion"]=6; const auto original=QJsonDocument(object).toJson(); ASSERT_TRUE(Write(path,original));
        ASSERT_TRUE(s.ReloadCatalog(&error)) << error.c_str(); ASSERT_TRUE(s.SaveLocalisationEntry(TextEntry(),{},id,&error)) << error.c_str();
        const auto hash=QCryptographicHash::hash(original,QCryptographicHash::Sha256).toHex();
        EXPECT_EQ(Bytes(path+".schema-6."+QString::fromLatin1(hash)+".backup.json"),original);
        ASSERT_NE(s.GetCatalog().FindQuestProfile(quest),nullptr);
        EXPECT_EQ(QJsonDocument::fromJson(Bytes(path)).object()["SchemaVersion"].toInt(),7);
        ASSERT_TRUE(s.ReloadSourceEvidence(&error)); ASSERT_TRUE(s.ReloadCatalog(&error));
        object=QJsonDocument::fromJson(Bytes(path)).object(); object["SchemaVersion"]=6;
        ASSERT_TRUE(Write(path,QJsonDocument(object).toJson())); EXPECT_FALSE(s.ReloadCatalog(&error));
        EXPECT_NE(s.GetCatalog().FindLocalisationEntry(id),nullptr);
    }
    TEST_F(AssetLocalisationAuthoringTests, DuplicateIdentityAndUnreviewedRevisionCannotReplaceCatalog)
    {
        QTemporaryDir dir; FoundationService s(FoundationWorkspaceLoadDependencies{}); AZStd::string error,id;
        ASSERT_TRUE(PrepareCompletionService(s,dir.path(),error)); ASSERT_TRUE(s.SaveLocalisationEntry(TextEntry(),{},id,&error));
        const auto path=ToQString(s.GetCatalogFilePath()); const auto original=Bytes(path); const auto revision=TextRevision(s,id);
        auto object=QJsonDocument::fromJson(original).object(); auto entries=object["LocalisationEntries"].toArray();
        ASSERT_EQ(entries.size(),1); entries.append(entries[0]); object["LocalisationEntries"]=entries;
        ASSERT_TRUE(Write(path,QJsonDocument(object).toJson())); EXPECT_FALSE(s.ReloadCatalog(&error)); EXPECT_EQ(TextRevision(s,id),revision);
        object=QJsonDocument::fromJson(original).object(); entries=object["LocalisationEntries"].toArray();
        auto entry=entries[0].toObject(); auto variants=entry["Variants"].toArray(); auto variant=variants[0].toObject(); variant["Text"]="Tampered";
        variants[0]=variant; entry["Variants"]=variants; entries[0]=entry; object["LocalisationEntries"]=entries;
        ASSERT_TRUE(Write(path,QJsonDocument(object).toJson())); EXPECT_FALSE(s.ReloadCatalog(&error)); EXPECT_EQ(TextRevision(s,id),revision);
        ASSERT_TRUE(Write(path,original));
    }

}
