/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoAInstallDiscoveryService.h"
#include "LocalSetupDetectionService.h"

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

        bool WriteFile(const QString& path, const QByteArray& contents = QByteArray("fixture"))
        {
            if (!QDir().mkpath(QFileInfo(path).absolutePath()))
            {
                return false;
            }
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && file.write(contents) == contents.size();
        }

        QString BuildCurrentFoAFixture(QTemporaryDir& temporary, QString& steamRoot)
        {
            steamRoot = QDir(temporary.path()).filePath("Steam");
            const QString installRoot = QDir(steamRoot).filePath(
                "steamapps/common/FallOfAvalonFixture");
            QDir().mkpath(installRoot);
            WriteFile(QDir(installRoot).filePath("Fall of Avalon.exe"));
            WriteFile(QDir(installRoot).filePath("Fall of Avalon_Data/Managed/Assembly-CSharp.dll"));
            QDir().mkpath(QDir(installRoot).filePath("BepInEx/plugins"));

            const QByteArray manifest =
                "\"AppState\"\n"
                "{\n"
                "  \"appid\" \"1466060\"\n"
                "  \"installdir\" \"FallOfAvalonFixture\"\n"
                "}\n";
            WriteFile(
                QDir(steamRoot).filePath("steamapps/appmanifest_1466060.acf"),
                manifest);
            return installRoot;
        }
    } // namespace

    TEST(FoAInstallDiscoveryServiceTests, SteamManifestDiscoversCurrentFallOfAvalonInstall)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        QString steamRoot;
        const QString installRoot = BuildCurrentFoAFixture(temporary, steamRoot);

        const FoAInstallDiscoveryService::Result result =
            FoAInstallDiscoveryService::DiscoverFromSteamRoots({ ToAzString(steamRoot) });

        ASSERT_EQ(result.m_installPathCandidates.size(), 1);
        EXPECT_EQ(
            QFileInfo(QString::fromUtf8(result.m_installPathCandidates.front().c_str())).canonicalFilePath(),
            QFileInfo(installRoot).canonicalFilePath());
    }

    TEST(FoAInstallDiscoveryServiceTests, CurrentFallOfAvalonLayoutProducesConfiguredMonoProfile)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        QString steamRoot;
        const QString installRoot = BuildCurrentFoAFixture(temporary, steamRoot);

        EXPECT_TRUE(LocalSetupDetectionService::LooksLikeTaintedGrailInstall(
            ToAzString(installRoot)));

        WorkspaceModel workspace;
        LocalSetupDetectionService::Hints hints;
        hints.m_workspaceRoot = ToAzString(QDir(temporary.path()).filePath("Workspace"));
        hints.m_installPathCandidates.push_back(ToAzString(installRoot));

        const LocalSetupDetectionService detector;
        const LocalSetupDetectionService::Result result = detector.Detect(workspace, hints);

        ASSERT_TRUE(result.m_gameInstallDetected);
        ASSERT_TRUE(result.m_gameProfileComplete);
        const GameProfile* profile = result.m_workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        EXPECT_EQ(profile->m_runtimeTarget, "Mono");
        EXPECT_TRUE(profile->m_managedAssembliesPath.find("Fall of Avalon_Data/Managed")
            != AZStd::string::npos);
        EXPECT_TRUE(profile->m_pluginPath.find("BepInEx/plugins") != AZStd::string::npos);
    }
} // namespace TaintedGrailModdingSDK
