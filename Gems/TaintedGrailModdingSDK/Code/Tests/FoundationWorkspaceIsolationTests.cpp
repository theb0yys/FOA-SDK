/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"
#include "FoundationValidationService.h"
#include "PackagePathValidation.h"
#include "PackPersistenceService.h"

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/Json/JsonSystemComponent.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzTest/AzTest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        bool WriteFile(const QString& path, const QByteArray& bytes)
        {
            if (!QDir().mkpath(QFileInfo(path).absolutePath()))
            {
                return false;
            }
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && file.write(bytes) == bytes.size();
        }

        AZStd::string Fingerprint(char digit)
        {
            return AZStd::string("sha256:") + AZStd::string(64, digit);
        }

        GameProfile MakeProfile(AZStd::string id, AZStd::string version)
        {
            GameProfile profile;
            profile.m_profileId = AZStd::move(id);
            profile.m_displayName = profile.m_profileId;
            profile.m_installPath = "Game";
            profile.m_gameVersion = AZStd::move(version);
            profile.m_branch = "mono";
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "2022.3.22f1";
            profile.m_bepInExVersion = "5.4.23.3";
            profile.m_managedAssembliesPath = "Game/Managed";
            profile.m_pluginPath = "Game/BepInEx/plugins";
            return profile;
        }

        WorkspaceModel MakeWorkspace(
            AZStd::string id,
            AZStd::string profileId,
            AZStd::string version)
        {
            WorkspaceModel workspace;
            workspace.m_workspaceId = AZStd::move(id);
            workspace.m_displayName = workspace.m_workspaceId;
            workspace.m_rootPath = "Workspace";
            workspace.m_outputPath = "Workspace/Output";
            workspace.m_stagingPath = "Workspace/Staging";
            workspace.m_deploymentPath = "Workspace/Deployment";
            workspace.m_activeGameProfileId = profileId;
            workspace.m_gameProfiles.push_back(
                MakeProfile(AZStd::move(profileId), AZStd::move(version)));
            return workspace;
        }

        SourceDocument MakeSourceDocument(const GameProfile& profile)
        {
            SourceDocument document;
            SourceRecord& source = document.m_source;
            source.m_sourceId = "source.workspace-a";
            source.m_title = "Workspace A source";
            source.m_sourceKind = "json-register";
            source.m_locator = "Sources/source.workspace-a/input.json";
            source.m_fingerprint = Fingerprint('a');
            source.m_profileId = profile.m_profileId;
            source.m_gameVersion = profile.m_gameVersion;
            source.m_branch = profile.m_branch;
            source.m_runtimeTarget = profile.m_runtimeTarget;
            source.m_toolName = "fixture";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "tg.structured-json";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-19T12:00:00Z";
            source.m_importedAt = "2026-07-19T12:00:01Z";
            source.m_limitations = "Synthetic test data.";
            source.m_mediaType = "application/json";
            source.m_byteSize = 64;
            source.m_importStatus = "imported";
            return document;
        }

        EvidenceDocument MakeEvidenceDocument(
            const SourceDocument& sourceDocument)
        {
            const SourceRecord& source = sourceDocument.m_source;
            EvidenceDocument document;
            document.m_sourceId = source.m_sourceId;
            document.m_sourceFingerprint = source.m_fingerprint;
            document.m_profileId = source.m_profileId;
            document.m_gameVersion = source.m_gameVersion;
            document.m_branch = source.m_branch;

            EvidenceRecord evidence;
            evidence.m_evidenceId = "evidence.workspace-a";
            evidence.m_sourceId = source.m_sourceId;
            evidence.m_sourceFingerprint = source.m_fingerprint;
            evidence.m_profileId = source.m_profileId;
            evidence.m_gameVersion = source.m_gameVersion;
            evidence.m_branch = source.m_branch;
            evidence.m_subjectRef = "subject:workspace-a";
            evidence.m_claim = "Synthetic workspace A claim.";
            evidence.m_evidenceKind = "structured_record";
            evidence.m_confidence = "documented";
            evidence.m_locator = source.m_locator;
            evidence.m_recordPath = "/records/0";
            evidence.m_extractedAt = "2026-07-19T12:00:01Z";
            document.m_evidence.push_back(evidence);
            return document;
        }

        CatalogDocument MakeCatalogDocument(
            const WorkspaceModel& workspace,
            const GameProfile& profile)
        {
            CatalogDocument document;
            document.m_workspaceId = workspace.m_workspaceId;
            document.m_profileId = profile.m_profileId;
            document.m_gameVersion = profile.m_gameVersion;
            document.m_branch = profile.m_branch;

            CatalogRecord record;
            record.m_recordId = "record.workspace-a";
            record.m_domain = "test";
            record.m_recordKind = "test-record";
            record.m_subjectRef = "subject:workspace-a";
            record.m_nativeRefExact = "native-workspace-a";
            record.m_identityKind = "native";
            record.m_displayName = "Workspace A record";
            record.m_researchStage = "reviewed";
            record.m_confidence = "documented";
            record.m_operationalRisk = "unknown";
            record.m_validationState = "unvalidated";
            record.m_stalenessState = "unknown";
            record.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
            record.m_evidenceIds = { "evidence.workspace-a" };
            document.m_records.push_back(record);
            return document;
        }

        PackManifest MakePack()
        {
            PackManifest pack;
            pack.m_packId = "owner.workspace-a-pack";
            pack.m_displayName = "Workspace A Pack";
            pack.m_ownerId = "owner";
            pack.m_version = "1.0.0";
            pack.m_runtimeActionsEnabled = false;
            return pack;
        }

        PackManifest MakePackageGuardPack()
        {
            PackManifest pack = MakePack();
            pack.m_targetGameVersion = "1.0.0";
            pack.m_targetBranch = "mono";
            pack.m_requiredCoreVersion = "1.0.0";
            pack.m_requiredAdapterVersion = "1.0.0";
            pack.m_saveImpact = "none";
            pack.m_contentDefinitionPaths = {
                "Content/Definitions/synthetic.tgcontent.json",
            };
            pack.m_assetPaths = {
                "Assets/Synthetic/asset.json",
            };
            pack.m_localisationPaths = {
                "Localization/en_us.lang.json",
            };
            pack.m_buildConfiguration = "Profile";
            pack.m_releaseChannel = "development";
            return pack;
        }

        QByteArray PackJsonWithAssetPath(const char* assetPath)
        {
            QByteArray json = R"({
  "AssetPaths": [")";
            json += assetPath;
            json += R"("],
  "BuildConfiguration": "Profile",
  "CompatibleGameVersions": [],
  "ContentDefinitionPaths": ["Content/Definitions/synthetic.tgcontent.json"],
  "Dependencies": [],
  "DisplayName": "Terrain Package Guard",
  "DlcScopes": [],
  "Incompatibilities": [],
  "LocalisationPaths": ["Localization/en_us.lang.json"],
  "OwnerId": "owner",
  "PackId": "owner.terrain-package-guard",
  "ReleaseChannel": "development",
  "RequiredAdapterVersion": "1.0.0",
  "RequiredCoreVersion": "1.0.0",
  "RequiredMods": [],
  "RuntimeActionsEnabled": false,
  "SaveImpact": "none",
  "SchemaVersion": 1,
  "TargetBranch": "mono",
  "TargetGameVersion": "1.0.0",
  "Version": "1.0.0"
})";
            return json;
        }

        bool HasBlockerWithId(
            const AZStd::vector<BlockerRecord>& blockers,
            const AZStd::string& blockerId)
        {
            for (const BlockerRecord& blocker : blockers)
            {
                if (blocker.m_blockerId == blockerId)
                {
                    return true;
                }
            }
            return false;
        }

        FoundationWorkspaceLoadDependencies MakeLoadDependencies(
            const WorkspaceModel& workspaceA,
            const WorkspaceModel& workspaceB)
        {
            FoundationWorkspaceLoadDependencies dependencies;
            dependencies.m_loadWorkspace =
                [workspaceA, workspaceB](const AZStd::string& filePath)
                    -> AZ::Outcome<WorkspaceDocumentLoadResult, AZStd::string>
                {
                    WorkspaceDocumentLoadResult result;
                    result.m_workspace = filePath.find("workspace-b")
                            != AZStd::string::npos
                        ? workspaceB
                        : workspaceA;
                    result.m_filePath = filePath;
                    return AZ::Success(AZStd::move(result));
                };
            dependencies.m_resolveWorkspaceRoot =
                [](const WorkspaceModel& workspace, const AZStd::string&)
                    -> AZ::Outcome<AZStd::string, AZStd::string>
                {
                    return AZ::Success(
                        AZStd::string("root/") + workspace.m_workspaceId);
                };
            dependencies.m_loadSourceEvidence =
                [workspaceA](
                    const AZStd::string& root,
                    AZStd::vector<SourceDocument>& sources,
                    AZStd::vector<EvidenceDocument>& evidence,
                    AZStd::vector<ImportIssue>&)
                    -> AZ::Outcome<void, AZStd::string>
                {
                    if (root.find(workspaceA.m_workspaceId)
                        != AZStd::string::npos)
                    {
                        const GameProfile& profile = workspaceA.m_gameProfiles.front();
                        sources.push_back(MakeSourceDocument(profile));
                        evidence.push_back(MakeEvidenceDocument(sources.front()));
                    }
                    return AZ::Success();
                };
            dependencies.m_catalogExists =
                [workspaceA](const AZStd::string& root)
                {
                    return root.find(workspaceA.m_workspaceId)
                        != AZStd::string::npos;
                };
            dependencies.m_loadCatalog =
                [workspaceA](const AZStd::string&)
                    -> AZ::Outcome<CatalogDocument, AZStd::string>
                {
                    return AZ::Success(MakeCatalogDocument(
                        workspaceA,
                        workspaceA.m_gameProfiles.front()));
                };
            dependencies.m_getCatalogPath =
                [](const AZStd::string& root)
                {
                    return root + "/Catalog/catalog.tgcatalog.json";
                };
            return dependencies;
        }

        class PackPersistenceServiceTests
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
                PackManifest::Reflect(serializeContext);
            }

            void TearDown() override
            {
                if (AZ::SerializeContext* serializeContext =
                        m_application.GetSerializeContext())
                {
                    serializeContext->EnableRemoveReflection();
                    PackManifest::Reflect(serializeContext);
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

    TEST(FoundationWorkspaceIsolationTests, SetWorkspaceClearsAllPriorWorkspaceStateAndSaveTarget)
    {
        const WorkspaceModel workspaceA =
            MakeWorkspace("workspace.a", "profile.a", "1.0.0");
        const WorkspaceModel workspaceB =
            MakeWorkspace("workspace.b", "profile.b", "2.0.0");
        FoundationService service(MakeLoadDependencies(workspaceA, workspaceB));
        service.Initialize();

        AZStd::string error;
        ASSERT_TRUE(service.LoadWorkspace("workspace-a.tgworkspace.json", &error))
            << error.c_str();
        ASSERT_TRUE(service.SetActivePack(MakePack(), &error)) << error.c_str();
        ASSERT_FALSE(service.GetSourceRegistry().GetSources().empty());
        ASSERT_FALSE(service.GetCatalog().GetRecords().empty());
        ASSERT_FALSE(service.GetWorkspaceFilePath().empty());

        service.SetWorkspace(workspaceB);

        EXPECT_EQ(service.GetWorkspace().m_workspaceId, workspaceB.m_workspaceId);
        EXPECT_TRUE(service.GetWorkspaceFilePath().empty());
        EXPECT_TRUE(service.GetWorkspaceRootPath().empty());
        EXPECT_TRUE(service.GetPacks().empty());
        EXPECT_EQ(service.GetActivePack(), nullptr);
        EXPECT_TRUE(service.GetActivePackFilePath().empty());
        EXPECT_TRUE(service.GetSourceRegistry().GetSources().empty());
        EXPECT_TRUE(service.GetSourceRegistry().GetEvidence().empty());
        EXPECT_TRUE(service.GetImportIssues().empty());
        EXPECT_TRUE(service.GetCatalog().GetRecords().empty());
        EXPECT_FALSE(service.SaveWorkspace(&error));
    }

    TEST(FoundationWorkspaceIsolationTests, SuccessfulLoadDropsPacksAndResearchFromPreviousWorkspace)
    {
        const WorkspaceModel workspaceA =
            MakeWorkspace("workspace.a", "profile.a", "1.0.0");
        const WorkspaceModel workspaceB =
            MakeWorkspace("workspace.b", "profile.b", "2.0.0");
        FoundationService service(MakeLoadDependencies(workspaceA, workspaceB));
        service.Initialize();

        AZStd::string error;
        ASSERT_TRUE(service.LoadWorkspace("workspace-a.tgworkspace.json", &error))
            << error.c_str();
        ASSERT_TRUE(service.SetActivePack(MakePack(), &error)) << error.c_str();
        ASSERT_FALSE(service.GetSourceRegistry().GetSources().empty());
        ASSERT_FALSE(service.GetCatalog().GetRecords().empty());

        ASSERT_TRUE(service.LoadWorkspace("workspace-b.tgworkspace.json", &error))
            << error.c_str();

        EXPECT_EQ(service.GetWorkspace().m_workspaceId, workspaceB.m_workspaceId);
        EXPECT_EQ(service.GetWorkspaceFilePath(), "workspace-b.tgworkspace.json");
        EXPECT_TRUE(service.GetPacks().empty());
        EXPECT_EQ(service.GetActivePack(), nullptr);
        EXPECT_TRUE(service.GetActivePackFilePath().empty());
        EXPECT_TRUE(service.GetSourceRegistry().GetSources().empty());
        EXPECT_TRUE(service.GetSourceRegistry().GetEvidence().empty());
        EXPECT_TRUE(service.GetCatalog().GetRecords().empty());
    }

    TEST(PackManifestPackagePathPolicyTests, SharedValidatorRejectsLocalTerrainWorkspaceArtifacts)
    {
        EXPECT_TRUE(IsLocalTerrainPackageExcludedPath(
            "Derived/Terrain/terrain-map/Revisions/revision/fingerprint/terrain.tgheightmap.json"));
        EXPECT_TRUE(IsLocalTerrainPackageExcludedPath(
            "SourceObservations/Terrain/import/source-observation.json"));
        EXPECT_TRUE(IsLocalTerrainPackageExcludedPath(
            "Staging/Terrain/import/fingerprint/Tiles/00000000-00000000.terrain.u16le"));
        EXPECT_TRUE(IsLocalTerrainPackageExcludedPath(
            "Generated/Terrain/terrain-map_gsi.png"));

        PackagePathIdentity identity;
        AZStd::string error;
        EXPECT_FALSE(TryValidatePackManifestPackagePath(
            "Derived/Terrain/terrain-map/Revisions/revision/fingerprint/terrain.tgheightmap.json",
            identity,
            &error));
        EXPECT_NE(error.find("local terrain"), AZStd::string::npos)
            << error.c_str();

        EXPECT_FALSE(TryValidatePackManifestPackagePath(
            "../Derived/Terrain/escape.terrain.u16le",
            identity,
            &error));
        EXPECT_FALSE(error.empty())
            << error.c_str();

        EXPECT_TRUE(TryValidatePackManifestPackagePath(
            "Content/Definitions/synthetic.tgcontent.json",
            identity,
            &error))
            << error.c_str();
        EXPECT_EQ(identity.m_normalizedPath, "Content/Definitions/synthetic.tgcontent.json");
    }

    TEST(PackManifestPackagePathPolicyTests, PackManifestRejectsTerrainPathsInEveryPackagePathList)
    {
        AZStd::string error;
        EXPECT_TRUE(MakePackageGuardPack().HasAllowedPackagePaths(&error))
            << error.c_str();

        PackManifest contentPath = MakePackageGuardPack();
        contentPath.m_contentDefinitionPaths = {
            "Derived/Terrain/terrain-map/Revisions/revision/fingerprint/terrain.tgheightmap.json",
        };
        EXPECT_FALSE(contentPath.HasAllowedPackagePaths(&error));
        EXPECT_NE(error.find("ContentDefinitionPaths"), AZStd::string::npos)
            << error.c_str();

        PackManifest assetPath = MakePackageGuardPack();
        assetPath.m_assetPaths = {
            "SourceObservations/Terrain/import/source-observation.json",
        };
        EXPECT_FALSE(assetPath.HasAllowedPackagePaths(&error));
        EXPECT_NE(error.find("AssetPaths"), AZStd::string::npos)
            << error.c_str();

        PackManifest localisationPath = MakePackageGuardPack();
        localisationPath.m_localisationPaths = {
            "Staging/Terrain/import/fingerprint/Tiles/00000000-00000000.terrain.u16le",
        };
        EXPECT_FALSE(localisationPath.HasAllowedPackagePaths(&error));
        EXPECT_NE(error.find("LocalisationPaths"), AZStd::string::npos)
            << error.c_str();
    }

    TEST_F(
        PackPersistenceServiceTests,
        SaveAndLoadRejectTerrainPackageCandidates)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString packPath = QDir(temporary.path()).filePath(
            "Packs/owner.terrain-package-guard.tgpack.json");

        PackManifest invalid = MakePackageGuardPack();
        invalid.m_assetPaths = {
            "Derived/Terrain/terrain-map/Revisions/revision/fingerprint/Tiles/00000000-00000000.terrain.u16le",
        };

        PackPersistenceService service;
        const auto save = service.Save(invalid, ToAzString(packPath));
        EXPECT_FALSE(save.IsSuccess());
        EXPECT_NE(save.GetError().find("local-only terrain"), AZStd::string::npos)
            << save.GetError().c_str();
        EXPECT_FALSE(QFileInfo::exists(packPath));

        ASSERT_TRUE(WriteFile(
            packPath,
            PackJsonWithAssetPath(
                "SourceObservations/Terrain/import/source-observation.json")));
        const auto load = service.Load(ToAzString(packPath));
        EXPECT_FALSE(load.IsSuccess());
        EXPECT_NE(load.GetError().find("local-only terrain"), AZStd::string::npos)
            << load.GetError().c_str();
    }

    TEST(PackManifestPackagePathPolicyTests, FoundationRejectsActivePackWithTerrainPackageCandidates)
    {
        PackManifest invalid = MakePackageGuardPack();
        invalid.m_assetPaths = {
            "Derived/Terrain/terrain-map/Revisions/revision/fingerprint/Tiles/00000000-00000000.terrain.u16le",
        };

        FoundationService service(FoundationWorkspaceLoadDependencies{});
        AZStd::string error;
        EXPECT_FALSE(service.SetActivePack(invalid, &error));
        EXPECT_NE(error.find("local-only terrain"), AZStd::string::npos)
            << error.c_str();
        EXPECT_EQ(service.GetActivePack(), nullptr);
    }

    TEST(PackManifestPackagePathPolicyTests, FoundationValidationReportsTerrainPackagePathBlocker)
    {
        PackManifest invalid = MakePackageGuardPack();
        invalid.m_assetPaths = {
            "Derived/Terrain/terrain-map/Revisions/revision/fingerprint/Tiles/00000000-00000000.terrain.u16le",
        };

        FoundationValidationService service;
        const AZStd::vector<BlockerRecord> blockers = service.Evaluate(
            MakeWorkspace("owner.workspace", "owner.profile", "1.0.0"),
            { invalid },
            SourceEvidenceRegistry{},
            {},
            CatalogDatabase{});

        EXPECT_TRUE(HasBlockerWithId(
            blockers,
            "foundation.pack.package-paths.owner.workspace-a-pack"));
    }
} // namespace TaintedGrailModdingSDK
