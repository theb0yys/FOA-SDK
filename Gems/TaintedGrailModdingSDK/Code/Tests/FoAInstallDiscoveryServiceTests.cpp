/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoAInstallDiscoveryService.h"
#include "FoundationService.h"
#include "LocalSetupDetectionService.h"

#include <AzTest/AzTest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
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

    TEST(FoAInstallDiscoveryServiceTests, SteamMetadataFindsGameInAnotherLibrary)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        QString libraryRoot;
        const QString installRoot = BuildCurrentFoAFixture(temporary, libraryRoot);
        const QString clientRoot = QDir(temporary.path()).filePath("Custom Steam Client");
        const QByteArray escapedPath = libraryRoot.toUtf8().replace("\\", "\\\\");
        ASSERT_TRUE(WriteFile(QDir(clientRoot).filePath("steamapps/libraryfolders.vdf"),
            "\"libraryfolders\" { \"1\" { \"path\" \"" + escapedPath + "\" } }"));

        const auto result = FoAInstallDiscoveryService::DiscoverFromSteamRoots({ ToAzString(clientRoot) });
        ASSERT_EQ(result.m_installPathCandidates.size(), 1);
        EXPECT_EQ(QFileInfo(QString::fromUtf8(result.m_installPathCandidates.front().c_str())).canonicalFilePath(),
            QFileInfo(installRoot).canonicalFilePath());
    }

    class FoundationLocalSetupIntegrationTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(m_temporary.isValid());
            m_hadLocalAppData = qEnvironmentVariableIsSet("LOCALAPPDATA");
            m_localAppData = qgetenv("LOCALAPPDATA");
            qputenv("LOCALAPPDATA", m_temporary.path().toUtf8());
            FoundationService::Get().Shutdown();
        }

        void TearDown() override
        {
            FoundationService::Get().Shutdown();
            if (m_hadLocalAppData)
            {
                qputenv("LOCALAPPDATA", m_localAppData);
            }
            else
            {
                qunsetenv("LOCALAPPDATA");
            }
        }

        QTemporaryDir m_temporary;
        QByteArray m_localAppData;
        bool m_hadLocalAppData = false;
    };

    TEST_F(FoundationLocalSetupIntegrationTests, ManualSelectionIgnoresUninitializedLegacyWorkspaceAndSurvivesRestart)
    {
        QString steamRoot;
        const QString installRoot = BuildCurrentFoAFixture(m_temporary, steamRoot);
        const QString obsoleteRoot = QDir(m_temporary.path()).filePath("ObsoleteWorkspace");
        // A file makes the stale legacy location deterministically unwritable as a directory.
        ASSERT_TRUE(WriteFile(obsoleteRoot));
        const QJsonObject legacy{
            { "workspace_root", obsoleteRoot },
            { "tainted_grail_install_path", QDir(m_temporary.path()).filePath("MissingGame") },
        };
        ASSERT_TRUE(WriteFile(QDir(m_temporary.path()).filePath("FOA-SDK/ToolWizard/tool-profile.local.json"),
            QJsonDocument(legacy).toJson()));

        auto& service = FoundationService::Get();
        const auto selected = service.RefreshLocalSetup(ToAzString(installRoot));
        ASSERT_TRUE(selected.IsReady()) << selected.m_error.c_str();
        const QString workspaceFile = QDir(m_temporary.path()).filePath("FOA-SDK/Workspace/foa-sdk.tgworkspace.json");
        ASSERT_TRUE(QFileInfo(workspaceFile).isFile());
        service.Shutdown();
        const auto reopened = service.RefreshLocalSetup();
        ASSERT_TRUE(reopened.IsReady()) << reopened.m_error.c_str();
        ASSERT_NE(service.GetWorkspace().FindActiveGameProfile(), nullptr);
        EXPECT_EQ(QFileInfo(QString::fromUtf8(service.GetWorkspace().FindActiveGameProfile()->m_installPath.c_str())).canonicalFilePath(),
            QFileInfo(installRoot).canonicalFilePath());
        EXPECT_TRUE(QFileInfo(obsoleteRoot).isFile());
    }

    TEST_F(FoundationLocalSetupIntegrationTests, ManualSelectionReplacesOldInstallAndDerivedPaths)
    {
        QString steamRoot;
        const QString firstRoot = BuildCurrentFoAFixture(m_temporary, steamRoot);
        QTemporaryDir second;
        ASSERT_TRUE(second.isValid());
        const QString secondRoot = BuildCurrentFoAFixture(second, steamRoot);
        auto& service = FoundationService::Get();
        ASSERT_TRUE(service.RefreshLocalSetup(ToAzString(firstRoot)).IsReady());
        const auto selected = service.RefreshLocalSetup(ToAzString(secondRoot));
        ASSERT_TRUE(selected.IsReady()) << selected.m_error.c_str();
        const GameProfile* profile = service.GetWorkspace().FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        EXPECT_EQ(QFileInfo(QString::fromUtf8(profile->m_installPath.c_str())).canonicalFilePath(),
            QFileInfo(secondRoot).canonicalFilePath());
        EXPECT_EQ(QFileInfo(QString::fromUtf8(profile->m_managedAssembliesPath.c_str())).canonicalFilePath(),
            QFileInfo(QDir(secondRoot).filePath("Fall of Avalon_Data/Managed")).canonicalFilePath());
        EXPECT_EQ(QFileInfo(QString::fromUtf8(profile->m_pluginPath.c_str())).canonicalFilePath(),
            QFileInfo(QDir(secondRoot).filePath("BepInEx/plugins")).canonicalFilePath());
        service.Shutdown();
        ASSERT_TRUE(service.RefreshLocalSetup().IsReady());
        EXPECT_EQ(QFileInfo(QString::fromUtf8(service.GetWorkspace().FindActiveGameProfile()->m_installPath.c_str())).canonicalFilePath(),
            QFileInfo(secondRoot).canonicalFilePath());
    }

    TEST_F(FoundationLocalSetupIntegrationTests, InvalidManualSelectionReportsErrorAndPreservesWorkspace)
    {
        QString steamRoot;
        const QString installRoot = BuildCurrentFoAFixture(m_temporary, steamRoot);
        auto& service = FoundationService::Get();
        ASSERT_TRUE(service.RefreshLocalSetup(ToAzString(installRoot)).IsReady());
        const AZStd::string previous = service.GetWorkspace().FindActiveGameProfile()->m_installPath;
        const auto result = service.RefreshLocalSetup(ToAzString(m_temporary.path()));
        EXPECT_FALSE(result.IsReady());
        EXPECT_FALSE(result.m_error.empty());
        EXPECT_EQ(service.GetWorkspace().FindActiveGameProfile()->m_installPath, previous);
    }

    TEST(FoundationLocalSetupResultTests, ReadyRequiresDetectedGameCompleteProfileAndPersistence)
    {
        FoundationLocalSetupResult result;
        EXPECT_FALSE(result.IsReady());

        result.m_gameProfileComplete = true;
        EXPECT_FALSE(result.IsReady());

        result.m_gameInstallDetected = true;
        EXPECT_FALSE(result.IsReady());

        result.m_persisted = true;
        EXPECT_TRUE(result.IsReady());

        result.m_error = "persistence failure";
        EXPECT_FALSE(result.IsReady());
    }
} // namespace TaintedGrailModdingSDK
