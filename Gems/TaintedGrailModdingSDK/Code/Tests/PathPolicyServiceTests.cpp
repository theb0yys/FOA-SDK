/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PathPolicyService.h"

#include <AzTest/AzTest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <filesystem>
#include <string>
#include <system_error>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        std::filesystem::path FromUtf8(const AZStd::string& value)
        {
#if defined(__cpp_lib_char8_t)
            return std::filesystem::path(std::u8string(
                reinterpret_cast<const char8_t*>(value.data()),
                value.size()));
#else
            return std::filesystem::u8path(value.c_str());
#endif
        }

        bool Touch(const QString& filePath)
        {
            QDir().mkpath(QFileInfo(filePath).absolutePath());
            QFile file(filePath);
            return file.open(QIODevice::WriteOnly) && file.write("test") == 4;
        }

        WorkspaceModel RelativeWorkspace()
        {
            WorkspaceModel workspace;
            workspace.m_workspaceId = "test.workspace";
            workspace.m_rootPath = ".";
            return workspace;
        }

        GameProfile ValidatedProfile(
            const AZStd::string& profileId,
            const AZStd::string& installPath)
        {
            GameProfile profile;
            profile.m_profileId = profileId;
            profile.m_displayName = profileId;
            profile.m_installPath = installPath;
            profile.m_gameVersion = "1.0.0";
            profile.m_branch = "mono";
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "2022.3";
            profile.m_bepInExVersion = "5.4";
            profile.m_managedAssembliesPath = installPath + "/Managed";
            profile.m_pluginPath = installPath + "/Plugins";
            profile.m_diagnosticsPath = AZStd::string("Diagnostics/") + profileId;
            profile.m_extractedDataPath = AZStd::string("Extracted/") + profileId;
            return profile;
        }

        WorkspaceModel ValidatedWorkspace(const QString& workspacePath)
        {
            WorkspaceModel workspace;
            workspace.m_workspaceId = "test.workspace";
            workspace.m_displayName = "Test Workspace";
            workspace.m_rootPath = ToAzString(workspacePath);
            workspace.m_outputPath = "Build";
            workspace.m_stagingPath = "Staging";
            workspace.m_deploymentPath = "Deployment";
            workspace.m_activeGameProfileId = "test.active";
            workspace.m_gameProfiles.push_back(ValidatedProfile("test.active", "GameActive"));
            workspace.m_gameProfiles.push_back(ValidatedProfile("test.inactive", "GameInactive"));
            return workspace;
        }

        bool PrepareValidatedWorkspaceTree(const QString& workspacePath)
        {
            const QStringList directories = {
                "Build",
                "Staging",
                "Deployment",
                "Diagnostics/test.active",
                "Diagnostics/test.inactive",
                "Extracted/test.active",
                "Extracted/test.inactive",
                "GameActive/Managed",
                "GameActive/Plugins",
                "GameInactive/Managed",
                "GameInactive/Plugins",
            };
            for (const QString& directory : directories)
            {
                if (!QDir().mkpath(QDir(workspacePath).filePath(directory)))
                {
                    return false;
                }
            }
            return Touch(QDir(workspacePath).filePath("GameActive/Managed/Game.dll"))
                && Touch(QDir(workspacePath).filePath("GameInactive/Managed/Game.dll"));
        }
    } // namespace

    TEST(PathPolicyServiceTests, WorkspaceDocumentRequiresCanonicalSuffix)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString invalidPath = QDir(temporary.path()).filePath("workspace.json");
        ASSERT_TRUE(Touch(invalidPath));

        PathPolicyService policy;
        const auto result = policy.ResolveWorkspaceDocumentPath(ToAzString(invalidPath), true);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find(".tgworkspace.json"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, RelativeWorkspaceRootUsesWorkspaceDocumentDirectory)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString documentPath = QDir(temporary.path()).filePath("preview.tgworkspace.json");
        ASSERT_TRUE(Touch(documentPath));

        PathPolicyService policy;
        const auto result = policy.ResolveWorkspaceRoot(
            RelativeWorkspace(),
            ToAzString(documentPath),
            true);
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
        EXPECT_TRUE(PathPolicyService::IsCanonicalPathContained(
            result.GetValue(),
            result.GetValue(),
            false));
    }

    TEST(PathPolicyServiceTests, PackManifestInsideWorkspaceIsAccepted)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString documentPath = QDir(temporary.path()).filePath("preview.tgworkspace.json");
        const QString packPath = QDir(temporary.path()).filePath("Packs/test.pack.tgpack.json");
        ASSERT_TRUE(Touch(documentPath));
        ASSERT_TRUE(Touch(packPath));

        PathPolicyService policy;
        const auto result = policy.ResolvePackManifestPath(
            RelativeWorkspace(),
            ToAzString(documentPath),
            ToAzString(packPath),
            true);
        EXPECT_TRUE(result.IsSuccess()) << result.GetError().c_str();
    }

    TEST(PathPolicyServiceTests, LexicalPrefixOutsideWorkspaceIsRejected)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        const QString outsidePath = QDir(temporary.path()).filePath(
            "workspace-escape/test.tgpack.json");
        QDir().mkpath(workspacePath);
        const QString documentPath = QDir(workspacePath).filePath(
            "workspace.tgworkspace.json");
        ASSERT_TRUE(Touch(documentPath));
        ASSERT_TRUE(Touch(outsidePath));

        WorkspaceModel workspace;
        workspace.m_workspaceId = "test.workspace";
        workspace.m_rootPath = ToAzString(workspacePath);
        PathPolicyService policy;
        const auto result = policy.ResolvePackManifestPath(
            workspace,
            ToAzString(documentPath),
            ToAzString(outsidePath),
            true);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("canonical workspace root"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, SymlinkEscapeIsRejectedAfterCanonicalization)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        const QString outsidePath = QDir(temporary.path()).filePath("outside");
        QDir().mkpath(workspacePath);
        QDir().mkpath(outsidePath);
        const QString documentPath = QDir(workspacePath).filePath(
            "workspace.tgworkspace.json");
        ASSERT_TRUE(Touch(documentPath));
        ASSERT_TRUE(Touch(QDir(outsidePath).filePath("escape.tgpack.json")));

        const std::filesystem::path linkPath = FromUtf8(
            ToAzString(QDir(workspacePath).filePath("linked-outside")));
        const std::filesystem::path targetPath = FromUtf8(ToAzString(outsidePath));
        std::error_code error;
        std::filesystem::create_directory_symlink(targetPath, linkPath, error);
        if (error)
        {
            GTEST_SKIP() << "Filesystem links are unavailable for this test host: "
                         << error.message();
        }

        WorkspaceModel workspace;
        workspace.m_workspaceId = "test.workspace";
        workspace.m_rootPath = ToAzString(workspacePath);
        PathPolicyService policy;
        const auto result = policy.ResolvePackManifestPath(
            workspace,
            ToAzString(documentPath),
            ToAzString(QDir(workspacePath).filePath(
                "linked-outside/escape.tgpack.json")),
            true);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("canonical workspace root"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, WindowsCaseFoldUsesPathComponents)
    {
        EXPECT_TRUE(PathPolicyService::IsCanonicalPathContained(
            "C:/Workspace",
            "c:/workspace/Packs/test.tgpack.json",
            true));
        EXPECT_FALSE(PathPolicyService::IsCanonicalPathContained(
            "C:/Workspace",
            "c:/workspace-escape/test.tgpack.json",
            true));
    }

    TEST(PathPolicyServiceTests, PackManifestSuffixIsRequired)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString documentPath = QDir(temporary.path()).filePath("preview.tgworkspace.json");
        const QString packPath = QDir(temporary.path()).filePath("Packs/test.json");
        ASSERT_TRUE(Touch(documentPath));
        ASSERT_TRUE(Touch(packPath));

        PathPolicyService policy;
        const auto result = policy.ResolvePackManifestPath(
            RelativeWorkspace(),
            ToAzString(documentPath),
            ToAzString(packPath),
            true);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find(".tgpack.json"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, ExtensionDocumentIsBoundToItsCanonicalRoot)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        ASSERT_TRUE(QDir().mkpath(workspacePath));
        const QString canonicalRoot = QFileInfo(workspacePath).canonicalFilePath();
        ASSERT_FALSE(canonicalRoot.isEmpty());

        PathPolicyService policy;
        const auto result = policy.ResolveExtensionDocumentPath(
            ToAzString(canonicalRoot),
            "extension.road-atlas",
            "road-atlas.snapshot.json",
            false);
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
        EXPECT_TRUE(PathPolicyService::IsCanonicalPathContained(
            ToAzString(canonicalRoot), result.GetValue(), false));
    }

    TEST(PathPolicyServiceTests, ExtensionDocumentSymlinkEscapeIsRejected)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        const QString outsidePath = QDir(temporary.path()).filePath("outside");
        ASSERT_TRUE(QDir().mkpath(QDir(workspacePath).filePath("Extensions")));
        ASSERT_TRUE(QDir().mkpath(outsidePath));

        const std::filesystem::path linkPath = FromUtf8(ToAzString(
            QDir(workspacePath).filePath("Extensions/extension.road-atlas")));
        const std::filesystem::path targetPath = FromUtf8(ToAzString(outsidePath));
        std::error_code error;
        std::filesystem::create_directory_symlink(targetPath, linkPath, error);
        if (error)
        {
            GTEST_SKIP() << "Filesystem links are unavailable for this test host: "
                         << error.message();
        }

        const QString canonicalRoot = QFileInfo(workspacePath).canonicalFilePath();
        ASSERT_FALSE(canonicalRoot.isEmpty());
        PathPolicyService policy;
        const auto result = policy.ResolveExtensionDocumentPath(
            ToAzString(canonicalRoot),
            "extension.road-atlas",
            "road-atlas.snapshot.json",
            false);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("canonical extension root"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, ExtensionDocumentRequiresJsonSuffix)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        ASSERT_TRUE(QDir().mkpath(workspacePath));
        const QString canonicalRoot = QFileInfo(workspacePath).canonicalFilePath();
        ASSERT_FALSE(canonicalRoot.isEmpty());

        PathPolicyService policy;
        const auto result = policy.ResolveExtensionDocumentPath(
            ToAzString(canonicalRoot),
            "extension.road-atlas",
            "payload.bin",
            false);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find(".json"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, EveryConfiguredProfilePathCanValidate)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        ASSERT_TRUE(PrepareValidatedWorkspaceTree(workspacePath));

        const QString canonicalRoot = QFileInfo(workspacePath).canonicalFilePath();
        ASSERT_FALSE(canonicalRoot.isEmpty());
        PathPolicyService policy;
        const auto result = policy.ValidateWorkspacePaths(
            ValidatedWorkspace(workspacePath),
            ToAzString(canonicalRoot));
        EXPECT_TRUE(result.IsSuccess()) << result.GetError().c_str();
    }

    TEST(PathPolicyServiceTests, ManagedAssembliesRejectsNonAssemblyFiles)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        ASSERT_TRUE(PrepareValidatedWorkspaceTree(workspacePath));
        ASSERT_TRUE(QFile::remove(QDir(workspacePath).filePath("GameActive/Managed/Game.dll")));
        ASSERT_TRUE(Touch(QDir(workspacePath).filePath("GameActive/Managed/readme.txt")));

        const QString canonicalRoot = QFileInfo(workspacePath).canonicalFilePath();
        ASSERT_FALSE(canonicalRoot.isEmpty());
        PathPolicyService policy;
        const auto result = policy.ValidateWorkspacePaths(
            ValidatedWorkspace(workspacePath),
            ToAzString(canonicalRoot));

        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("assembly file"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, WorkspaceOwnedPathEscapeIsRejected)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        ASSERT_TRUE(PrepareValidatedWorkspaceTree(workspacePath));

        WorkspaceModel workspace = ValidatedWorkspace(workspacePath);
        workspace.m_outputPath = "../outside-build";
        const QString canonicalRoot = QFileInfo(workspacePath).canonicalFilePath();
        ASSERT_FALSE(canonicalRoot.isEmpty());
        PathPolicyService policy;
        const auto result = policy.ValidateWorkspacePaths(workspace, ToAzString(canonicalRoot));
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("OutputPath"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, ActiveManagedAssembliesEscapeFromInstallIsRejected)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        ASSERT_TRUE(PrepareValidatedWorkspaceTree(workspacePath));

        WorkspaceModel workspace = ValidatedWorkspace(workspacePath);
        workspace.m_gameProfiles.front().m_managedAssembliesPath = "Other/Managed";
        const QString canonicalRoot = QFileInfo(workspacePath).canonicalFilePath();
        ASSERT_FALSE(canonicalRoot.isEmpty());
        PathPolicyService policy;
        const auto result = policy.ValidateWorkspacePaths(workspace, ToAzString(canonicalRoot));
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("test.active"), AZStd::string::npos);
        EXPECT_NE(result.GetError().find("ManagedAssembliesPath"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, InactiveDiagnosticsEscapeIsRejected)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        ASSERT_TRUE(PrepareValidatedWorkspaceTree(workspacePath));

        WorkspaceModel workspace = ValidatedWorkspace(workspacePath);
        workspace.m_gameProfiles[1].m_diagnosticsPath = "../outside-diagnostics";
        const QString canonicalRoot = QFileInfo(workspacePath).canonicalFilePath();
        ASSERT_FALSE(canonicalRoot.isEmpty());
        PathPolicyService policy;
        const auto result = policy.ValidateWorkspacePaths(workspace, ToAzString(canonicalRoot));
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("test.inactive"), AZStd::string::npos);
        EXPECT_NE(result.GetError().find("DiagnosticsPath"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, InactiveManagedAssembliesEscapeFromInstallIsRejected)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        ASSERT_TRUE(PrepareValidatedWorkspaceTree(workspacePath));

        WorkspaceModel workspace = ValidatedWorkspace(workspacePath);
        workspace.m_gameProfiles[1].m_managedAssembliesPath = "OtherGame/Managed";
        const QString canonicalRoot = QFileInfo(workspacePath).canonicalFilePath();
        ASSERT_FALSE(canonicalRoot.isEmpty());
        PathPolicyService policy;
        const auto result = policy.ValidateWorkspacePaths(workspace, ToAzString(canonicalRoot));
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("test.inactive"), AZStd::string::npos);
        EXPECT_NE(result.GetError().find("ManagedAssembliesPath"), AZStd::string::npos);
    }

    TEST(PathPolicyServiceTests, MonoPluginEscapeFromInstallIsRejected)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspacePath = QDir(temporary.path()).filePath("workspace");
        ASSERT_TRUE(PrepareValidatedWorkspaceTree(workspacePath));

        WorkspaceModel workspace = ValidatedWorkspace(workspacePath);
        workspace.m_gameProfiles.front().m_pluginPath = "Other/Plugins";
        const QString canonicalRoot = QFileInfo(workspacePath).canonicalFilePath();
        ASSERT_FALSE(canonicalRoot.isEmpty());
        PathPolicyService policy;
        const auto result = policy.ValidateWorkspacePaths(workspace, ToAzString(canonicalRoot));
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_NE(result.GetError().find("test.active"), AZStd::string::npos);
        EXPECT_NE(result.GetError().find("PluginPath"), AZStd::string::npos);
    }
} // namespace TaintedGrailModdingSDK
