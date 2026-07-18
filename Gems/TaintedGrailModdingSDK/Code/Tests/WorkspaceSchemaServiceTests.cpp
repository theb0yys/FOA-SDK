/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "WorkspacePersistenceService.h"
#include "WorkspaceSchemaService.h"

#include <AzTest/AzTest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#ifndef TG_SDK_PREVIEW_TEMPLATE_ROOT
#   error TG_SDK_PREVIEW_TEMPLATE_ROOT must identify the Developer Preview fixture template directory.
#endif

namespace TaintedGrailModdingSDK
{
    namespace
    {
        WorkspaceModel MakeValidWorkspace()
        {
            WorkspaceModel workspace;
            workspace.m_workspaceId = "test.workspace";
            workspace.m_displayName = "Test Workspace";
            workspace.m_rootPath = ".";
            workspace.m_outputPath = "Build";
            workspace.m_stagingPath = "Staging";
            workspace.m_deploymentPath = "Deployment";
            workspace.m_activeGameProfileId = "test.profile";

            GameProfile profile;
            profile.m_profileId = workspace.m_activeGameProfileId;
            profile.m_displayName = "Test Profile";
            profile.m_installPath = "Game";
            profile.m_gameVersion = "1.0.0";
            profile.m_branch = "mono";
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "2022.3";
            profile.m_bepInExVersion = "5.4";
            profile.m_managedAssembliesPath = "Game/Managed";
            profile.m_pluginPath = "Game/Plugins";
            profile.m_diagnosticsPath = "Diagnostics";
            profile.m_extractedDataPath = "Extracted";
            workspace.m_gameProfiles.push_back(profile);
            return workspace;
        }

        bool WriteFile(const QString& path, const QByteArray& bytes)
        {
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && file.write(bytes) == bytes.size();
        }
    } // namespace

    TEST(WorkspaceSchemaServiceTests, StableWorkspaceIdIsRequired)
    {
        WorkspaceModel workspace = MakeValidWorkspace();
        workspace.m_workspaceId = "Invalid Workspace";
        WorkspaceSchemaService schema;
        const auto result = schema.Validate(workspace);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("WorkspaceId"), AZStd::string::npos);
    }

    TEST(WorkspaceSchemaServiceTests, ProfileIdsMustBeUnique)
    {
        WorkspaceModel workspace = MakeValidWorkspace();
        workspace.m_gameProfiles.push_back(workspace.m_gameProfiles.front());
        WorkspaceSchemaService schema;
        const auto result = schema.Validate(workspace);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("duplicate ProfileId"), AZStd::string::npos);
    }

    TEST(WorkspaceSchemaServiceTests, ActiveProfileMustBindExactly)
    {
        WorkspaceModel workspace = MakeValidWorkspace();
        workspace.m_activeGameProfileId = "missing.profile";
        WorkspaceSchemaService schema;
        const auto result = schema.Validate(workspace);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("ActiveGameProfileId"), AZStd::string::npos);
    }

    TEST(WorkspaceSchemaServiceTests, UnknownSchemaVersionIsRejected)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString path = QDir(temporary.path()).filePath("unknown.tgworkspace.json");
        const QByteArray json = R"JSON({
  "SchemaVersion": 99,
  "WorkspaceId": "test.workspace",
  "DisplayName": "Test",
  "RootPath": ".",
  "OutputPath": "Build",
  "StagingPath": "Staging",
  "DeploymentPath": "Deployment",
  "ActiveGameProfileId": "test.profile",
  "GameProfiles": []
})JSON";
        ASSERT_TRUE(WriteFile(path, json));
        WorkspacePersistenceService persistence;
        const auto result = persistence.Load(path.toUtf8().constData());
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("unsupported"), AZStd::string::npos);
    }

    TEST(WorkspaceSchemaServiceTests, UnknownSchemaVersionCannotHideBehindLegacyEnvelope)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString path = QDir(temporary.path()).filePath("unknown-envelope.tgworkspace.json");
        const QByteArray json = R"JSON({
  "Type": "TaintedGrailModdingSDK::WorkspaceModel",
  "SchemaVersion": 99
})JSON";
        ASSERT_TRUE(WriteFile(path, json));
        WorkspacePersistenceService persistence;
        const auto result = persistence.Load(path.toUtf8().constData());
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("unsupported"), AZStd::string::npos);
    }

    TEST(WorkspaceSchemaServiceTests, MalformedLegacyWorkspaceHasMigrationError)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString path = QDir(temporary.path()).filePath("malformed-legacy.tgworkspace.json");
        const QByteArray json = R"JSON({
  "WorkspaceId": 42,
  "GameProfiles": []
})JSON";
        ASSERT_TRUE(WriteFile(path, json));
        WorkspacePersistenceService persistence;
        const auto result = persistence.Load(path.toUtf8().constData());
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("Legacy workspace schema 0"), AZStd::string::npos);
    }

    TEST(WorkspaceSchemaServiceTests, UnsafeLegacyWorkspaceIsClearlyRejected)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString path = QDir(temporary.path()).filePath("unsafe-legacy.tgworkspace.json");
        const QByteArray json = R"JSON({
  "WorkspaceId": "test.workspace",
  "DisplayName": "Test",
  "RootPath": ".",
  "OutputPath": "Build",
  "StagingPath": "Staging",
  "DeploymentPath": "Deployment",
  "ActiveGameProfileId": "test.profile",
  "GameProfiles": []
})JSON";
        ASSERT_TRUE(WriteFile(path, json));
        WorkspacePersistenceService persistence;
        const auto result = persistence.Load(path.toUtf8().constData());
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("Legacy workspace schema 0"), AZStd::string::npos);
    }

    TEST(WorkspaceSchemaServiceTests, SchemaOneRoundTripIsStable)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString path = QDir(temporary.path()).filePath("roundtrip.tgworkspace.json");
        const WorkspaceModel original = MakeValidWorkspace();
        WorkspacePersistenceService persistence;
        auto save = persistence.Save(original, path.toUtf8().constData());
        ASSERT_TRUE(save.IsSuccess()) << save.GetError().c_str();

        QFile savedFile(path);
        ASSERT_TRUE(savedFile.open(QIODevice::ReadOnly));
        const QByteArray savedBytes = savedFile.readAll();
        EXPECT_TRUE(savedBytes.contains("\"SchemaVersion\": 1"));

        auto loaded = persistence.Load(path.toUtf8().constData());
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        EXPECT_EQ(loaded.GetValue().m_workspaceId, original.m_workspaceId);
        EXPECT_EQ(loaded.GetValue().m_activeGameProfileId, original.m_activeGameProfileId);
        ASSERT_EQ(loaded.GetValue().m_gameProfiles.size(), 1);
        EXPECT_EQ(
            loaded.GetValue().m_gameProfiles.front().m_profileId,
            original.m_gameProfiles.front().m_profileId);
    }

    TEST(WorkspaceSchemaServiceTests, PreviewFixtureLegacyShapeMigratesAndRoundTrips)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString templatePath = QDir(QString::fromUtf8(TG_SDK_PREVIEW_TEMPLATE_ROOT)).filePath(
            "preview.tgworkspace.json");
        QFile templateFile(templatePath);
        ASSERT_TRUE(templateFile.open(QIODevice::ReadOnly));
        QByteArray legacyBytes = templateFile.readAll();
        legacyBytes.replace("  \"SchemaVersion\": 1,\n", "");
        legacyBytes.replace("  \"SchemaVersion\": 1\r\n", "");

        const QString legacyPath = QDir(temporary.path()).filePath("legacy-preview.tgworkspace.json");
        ASSERT_TRUE(WriteFile(legacyPath, legacyBytes));
        WorkspacePersistenceService persistence;
        auto migrated = persistence.Load(legacyPath.toUtf8().constData());
        ASSERT_TRUE(migrated.IsSuccess()) << migrated.GetError().c_str();
        EXPECT_EQ(migrated.GetValue().m_workspaceId, "preview.workspace");
        EXPECT_EQ(migrated.GetValue().m_activeGameProfileId, "preview.profile.windows-x64");

        const QString schemaOnePath = QDir(temporary.path()).filePath("preview-schema-one.tgworkspace.json");
        auto save = persistence.Save(migrated.GetValue(), schemaOnePath.toUtf8().constData());
        ASSERT_TRUE(save.IsSuccess()) << save.GetError().c_str();
        auto reopened = persistence.Load(schemaOnePath.toUtf8().constData());
        ASSERT_TRUE(reopened.IsSuccess()) << reopened.GetError().c_str();
        EXPECT_EQ(reopened.GetValue().m_workspaceId, migrated.GetValue().m_workspaceId);
        EXPECT_EQ(reopened.GetValue().m_gameProfiles.size(), migrated.GetValue().m_gameProfiles.size());
    }
} // namespace TaintedGrailModdingSDK
