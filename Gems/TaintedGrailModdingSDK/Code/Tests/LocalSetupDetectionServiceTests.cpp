/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "LocalSetupDetectionService.h"

#include <AzTest/AzTest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        QString CleanPath(const QString& value)
        {
            return QDir::cleanPath(QFileInfo(value).absoluteFilePath());
        }

        bool Touch(const QString& filePath, const QByteArray& data = "test")
        {
            QDir().mkpath(QFileInfo(filePath).absolutePath());
            QFile file(filePath);
            return file.open(QIODevice::WriteOnly)
                && file.write(data) == data.size();
        }

        LocalSetupDetectionService::Hints MakeHints(
            const QString& workspaceRoot,
            const QString& installRoot)
        {
            LocalSetupDetectionService::Hints hints;
            hints.m_workspaceRoot = ToAzString(workspaceRoot);
            hints.m_installPathCandidates.push_back(ToAzString(installRoot));
            return hints;
        }
    } // namespace

    TEST(LocalSetupDetectionServiceTests, MonoDetectionFillsWorkspaceAndProfileFromHints)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspaceRoot = QDir(temporary.path()).filePath("Workspace");
        const QString installRoot = QDir(temporary.path()).filePath("FoA");

        ASSERT_TRUE(Touch(QDir(installRoot).filePath("UnityPlayer.dll")));
        ASSERT_TRUE(Touch(
            QDir(installRoot).filePath("TaintedGrail.exe"),
            "Tainted Grail version 1.23.401"));
        ASSERT_TRUE(Touch(
            QDir(installRoot).filePath("Tainted Grail_Data/globalgamemanagers"),
            "Unity 6000.0.64f1"));
        ASSERT_TRUE(Touch(QDir(installRoot).filePath("Tainted Grail_Data/Managed/Assembly-CSharp.dll")));
        ASSERT_TRUE(Touch(
            QDir(installRoot).filePath("BepInEx/core/BepInEx.dll"),
            "BepInEx 5.4.23.3"));
        ASSERT_TRUE(QDir().mkpath(QDir(installRoot).filePath("BepInEx/plugins")));

        const LocalSetupDetectionService service;
        const LocalSetupDetectionService::Result result =
            service.Detect(WorkspaceModel{}, MakeHints(workspaceRoot, installRoot));

        EXPECT_TRUE(result.m_workspaceRootDetected);
        EXPECT_TRUE(result.m_gameInstallDetected);
        EXPECT_TRUE(result.m_gameProfileComplete);
        EXPECT_EQ(result.m_workspace.m_workspaceId, "tgfoa.workspace.default");
        EXPECT_EQ(
            CleanPath(ToQString(result.m_workspace.m_rootPath)),
            CleanPath(workspaceRoot));
        EXPECT_EQ(
            CleanPath(ToQString(result.m_workspace.m_outputPath)),
            CleanPath(QDir(workspaceRoot).filePath("Build")));
        EXPECT_EQ(
            CleanPath(ToQString(result.m_workspace.m_stagingPath)),
            CleanPath(QDir(workspaceRoot).filePath("Staging")));
        EXPECT_EQ(
            CleanPath(ToQString(result.m_workspace.m_deploymentPath)),
            CleanPath(QDir(workspaceRoot).filePath("Deployment")));

        const GameProfile* profile = result.m_workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        EXPECT_EQ(profile->m_profileId, "foa.mono.current");
        EXPECT_EQ(profile->m_runtimeTarget, "Mono");
        EXPECT_EQ(profile->m_branch, "mono");
        EXPECT_EQ(profile->m_gameVersion, "1.23.401");
        EXPECT_EQ(profile->m_unityVersion, "6000.0.64f1");
        EXPECT_EQ(profile->m_bepInExVersion, "5.4.23.3");
        EXPECT_EQ(CleanPath(ToQString(profile->m_installPath)), CleanPath(installRoot));
        EXPECT_EQ(
            CleanPath(ToQString(profile->m_managedAssembliesPath)),
            CleanPath(QDir(installRoot).filePath("Tainted Grail_Data/Managed")));
        EXPECT_EQ(
            CleanPath(ToQString(profile->m_pluginPath)),
            CleanPath(QDir(installRoot).filePath("BepInEx/plugins")));
        EXPECT_EQ(profile->m_dlcScopes.size(), 1);
        EXPECT_EQ(profile->m_dlcScopes.front(), "base-game");
    }

    TEST(LocalSetupDetectionServiceTests, Il2CppDetectionUsesInteropAndKeepsPluginPathEmpty)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspaceRoot = QDir(temporary.path()).filePath("Workspace");
        const QString installRoot = QDir(temporary.path()).filePath("FoA");

        ASSERT_TRUE(Touch(QDir(installRoot).filePath("UnityPlayer.dll")));
        ASSERT_TRUE(Touch(QDir(installRoot).filePath("GameAssembly.dll")));
        ASSERT_TRUE(Touch(
            QDir(installRoot).filePath("TaintedGrail.exe"),
            "Tainted Grail version 1.23.401"));
        ASSERT_TRUE(Touch(
            QDir(installRoot).filePath("TaintedGrail_Data/globalgamemanagers"),
            "Unity 6000.0.64f1"));
        ASSERT_TRUE(Touch(QDir(installRoot).filePath("BepInEx/interop/Assembly-CSharp.dll")));
        ASSERT_TRUE(Touch(
            QDir(installRoot).filePath("BepInEx/core/BepInEx.Unity.IL2CPP.dll"),
            "BepInEx 6.0.0-be.735"));
        ASSERT_TRUE(QDir().mkpath(QDir(installRoot).filePath("BepInEx/plugins")));

        const LocalSetupDetectionService service;
        const LocalSetupDetectionService::Result result =
            service.Detect(WorkspaceModel{}, MakeHints(workspaceRoot, installRoot));

        EXPECT_TRUE(result.m_workspaceRootDetected);
        EXPECT_TRUE(result.m_gameInstallDetected);
        EXPECT_TRUE(result.m_gameProfileComplete);
        const GameProfile* profile = result.m_workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        EXPECT_EQ(profile->m_profileId, "foa.il2cpp.current");
        EXPECT_EQ(profile->m_runtimeTarget, "IL2CPP");
        EXPECT_EQ(profile->m_branch, "il2cpp");
        EXPECT_EQ(profile->m_gameVersion, "1.23.401");
        EXPECT_EQ(profile->m_unityVersion, "6000.0.64f1");
        EXPECT_EQ(profile->m_bepInExVersion, "6.0.0-be.735");
        EXPECT_EQ(
            CleanPath(ToQString(profile->m_managedAssembliesPath)),
            CleanPath(QDir(installRoot).filePath("BepInEx/interop")));
        EXPECT_TRUE(profile->m_pluginPath.empty());
    }

    TEST(LocalSetupDetectionServiceTests, InvalidInstallCandidateDoesNotPretendDetected)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspaceRoot = QDir(temporary.path()).filePath("Workspace");
        const QString installRoot = QDir(temporary.path()).filePath("NotFoA");
        ASSERT_TRUE(QDir().mkpath(installRoot));

        const LocalSetupDetectionService service;
        const LocalSetupDetectionService::Result result =
            service.Detect(WorkspaceModel{}, MakeHints(workspaceRoot, installRoot));

        EXPECT_TRUE(result.m_workspaceRootDetected);
        EXPECT_FALSE(result.m_gameInstallDetected);
        EXPECT_FALSE(result.m_gameProfileComplete);
        EXPECT_FALSE(LocalSetupDetectionService::LooksLikeTaintedGrailInstall(ToAzString(installRoot)));
        const GameProfile* profile = result.m_workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        EXPECT_TRUE(profile->m_installPath.empty());
    }
} // namespace TaintedGrailModdingSDK
