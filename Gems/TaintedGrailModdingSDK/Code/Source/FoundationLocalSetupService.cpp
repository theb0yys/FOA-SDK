/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

#include "FoAInstallDiscoveryService.h"
#include "LocalSetupDetectionService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        struct LegacyToolProfileHints
        {
            QString m_workspaceRoot;
            QString m_installPath;
        };

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString LocalAppDataRoot()
        {
            const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
            return localAppData.isEmpty() ? QDir::homePath() : localAppData;
        }

        AZStd::string DefaultWorkspaceRoot()
        {
            return ToAzString(QDir(LocalAppDataRoot()).filePath("FOA-SDK/Workspace"));
        }

        LegacyToolProfileHints ReadLegacyToolProfileHints()
        {
            LegacyToolProfileHints hints;
            const QString profilePath = QDir(LocalAppDataRoot()).filePath(
                "FOA-SDK/ToolWizard/tool-profile.local.json");
            QFile file(profilePath);
            if (!file.exists() || file.size() > 1024 * 1024 || !file.open(QIODevice::ReadOnly))
            {
                return hints;
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                return hints;
            }

            const QJsonObject root = document.object();
            hints.m_workspaceRoot = root.value("workspace_root").toString().trimmed();
            hints.m_installPath = root.value("tainted_grail_install_path").toString().trimmed();
            return hints;
        }

        void AddUnique(
            AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            if (!value.empty()
                && AZStd::find(values.begin(), values.end(), value) == values.end())
            {
                values.push_back(value);
            }
        }

        void AddNote(
            FoundationLocalSetupResult& result,
            const AZStd::string& note)
        {
            AddUnique(result.m_notes, note);
        }

        QString ResolveDirectoryValue(const QString& workspaceRoot, const AZStd::string& value)
        {
            const QString text = ToQString(value).trimmed();
            if (text.isEmpty())
            {
                return {};
            }

            const QFileInfo info(text);
            const QString absolutePath =
                info.isAbsolute() ? info.absoluteFilePath() : QDir(workspaceRoot).filePath(text);
            return QDir::cleanPath(absolutePath);
        }

        bool IsSameOrChildDirectory(const QString& root, const QString& path)
        {
            QString normalizedRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
            QString normalizedPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#if defined(Q_OS_WIN)
            normalizedRoot = normalizedRoot.toCaseFolded();
            normalizedPath = normalizedPath.toCaseFolded();
#endif
            if (normalizedPath == normalizedRoot)
            {
                return true;
            }
            if (!normalizedRoot.endsWith('/'))
            {
                normalizedRoot += '/';
            }
            return normalizedPath.startsWith(normalizedRoot);
        }

        bool EnsureWorkspaceDirectories(
            const WorkspaceModel& workspace,
            AZStd::string& error)
        {
            const QString workspaceRoot = ResolveDirectoryValue(QString(), workspace.m_rootPath);
            if (workspaceRoot.isEmpty() || !QDir().mkpath(workspaceRoot))
            {
                error = "FOA-SDK could not create its automatic workspace root.";
                return false;
            }

            auto ensureWorkspaceOwnedDirectory =
                [&workspaceRoot, &error](
                    const char* label,
                    const AZStd::string& value,
                    bool mustBeDedicatedChild)
                {
                    const QString directoryPath = ResolveDirectoryValue(workspaceRoot, value);
                    if (directoryPath.isEmpty()
                        || !IsSameOrChildDirectory(workspaceRoot, directoryPath)
                        || (mustBeDedicatedChild
                            && QDir::cleanPath(directoryPath)
                                == QDir::cleanPath(QFileInfo(workspaceRoot).absoluteFilePath())))
                    {
                        error = AZStd::string("Automatic ") + label
                            + " must remain inside the FOA-SDK workspace root.";
                        return false;
                    }
                    if (!QDir().mkpath(directoryPath))
                    {
                        error = AZStd::string("FOA-SDK could not create its automatic ")
                            + label + ".";
                        return false;
                    }
                    return true;
                };

            if (!ensureWorkspaceOwnedDirectory("build directory", workspace.m_outputPath, true)
                || !ensureWorkspaceOwnedDirectory("staging directory", workspace.m_stagingPath, true)
                || !ensureWorkspaceOwnedDirectory("deployment directory", workspace.m_deploymentPath, true))
            {
                return false;
            }

            if (const GameProfile* profile = workspace.FindActiveGameProfile())
            {
                if (!profile->m_diagnosticsPath.empty()
                    && !ensureWorkspaceOwnedDirectory(
                        "diagnostics directory",
                        profile->m_diagnosticsPath,
                        false))
                {
                    return false;
                }
                if (!profile->m_extractedDataPath.empty()
                    && !ensureWorkspaceOwnedDirectory(
                        "extracted-data directory",
                        profile->m_extractedDataPath,
                        false))
                {
                    return false;
                }
            }
            return true;
        }

        AZStd::string WorkspaceFilePathForRoot(const AZStd::string& workspaceRoot)
        {
            const QString resolved = ResolveDirectoryValue(QString(), workspaceRoot);
            if (resolved.isEmpty())
            {
                return {};
            }
            return ToAzString(QDir(resolved).filePath("foa-sdk.tgworkspace.json"));
        }
    } // namespace

    FoundationLocalSetupResult FoundationService::RefreshLocalSetup(
        const AZStd::string& explicitInstallPath,
        const AZStd::string& workspaceRootHint)
    {
        FoundationLocalSetupResult result;
        const LegacyToolProfileHints legacyHints = ReadLegacyToolProfileHints();

        AZStd::string defaultWorkspaceRoot = workspaceRootHint;
        if (defaultWorkspaceRoot.empty() && !m_workspace.m_rootPath.empty())
        {
            defaultWorkspaceRoot = m_workspace.m_rootPath;
        }
        if (defaultWorkspaceRoot.empty() && !legacyHints.m_workspaceRoot.isEmpty())
        {
            defaultWorkspaceRoot = ToAzString(legacyHints.m_workspaceRoot);
        }
        if (defaultWorkspaceRoot.empty())
        {
            defaultWorkspaceRoot = DefaultWorkspaceRoot();
        }

        const AZStd::string automaticWorkspaceFile =
            WorkspaceFilePathForRoot(defaultWorkspaceRoot);
        if (m_workspaceFilePath.empty()
            && explicitInstallPath.empty()
            && !automaticWorkspaceFile.empty()
            && QFileInfo::exists(ToQString(automaticWorkspaceFile)))
        {
            AZStd::string loadError;
            if (LoadWorkspace(automaticWorkspaceFile, &loadError))
            {
                AddNote(result, "Existing automatic FOA-SDK workspace loaded.");
            }
            else
            {
                AddNote(
                    result,
                    AZStd::string("Existing automatic workspace could not be loaded: ")
                        + loadError);
            }
        }

        LocalSetupDetectionService::Hints hints;
        hints.m_workspaceRoot = !workspaceRootHint.empty()
            ? workspaceRootHint
            : (!m_workspace.m_rootPath.empty() ? m_workspace.m_rootPath : defaultWorkspaceRoot);

        if (const GameProfile* profile = m_workspace.FindActiveGameProfile())
        {
            AddUnique(hints.m_installPathCandidates, profile->m_installPath);
        }
        AddUnique(hints.m_installPathCandidates, explicitInstallPath);
        AddUnique(
            hints.m_installPathCandidates,
            ToAzString(legacyHints.m_installPath.trimmed()));

        const FoAInstallDiscoveryService discoveryService;
        const FoAInstallDiscoveryService::Result discovery = discoveryService.Discover();
        for (const AZStd::string& candidate : discovery.m_installPathCandidates)
        {
            AddUnique(hints.m_installPathCandidates, candidate);
        }
        for (const AZStd::string& note : discovery.m_notes)
        {
            AddNote(result, note);
        }

        const LocalSetupDetectionService detectionService;
        const LocalSetupDetectionService::Result detected =
            detectionService.Detect(m_workspace, hints);
        result.m_changed = detected.m_changed;
        result.m_gameInstallDetected = detected.m_gameInstallDetected;
        result.m_gameProfileComplete = detected.m_gameProfileComplete;
        for (const AZStd::string& note : detected.m_notes)
        {
            AddNote(result, note);
        }

        if (!detected.m_gameProfileComplete)
        {
            RefreshSnapshot();
            return result;
        }

        const bool needsPersistence = detected.m_changed || m_workspaceFilePath.empty();
        if (!needsPersistence)
        {
            result.m_persisted = true;
            RefreshSnapshot();
            return result;
        }

        AZStd::string directoryError;
        if (!EnsureWorkspaceDirectories(detected.m_workspace, directoryError))
        {
            result.m_error = AZStd::move(directoryError);
            RefreshSnapshot();
            return result;
        }

        AZStd::string targetPath = m_workspaceFilePath;
        if (targetPath.empty())
        {
            targetPath = WorkspaceFilePathForRoot(detected.m_workspace.m_rootPath);
        }
        if (targetPath.empty())
        {
            result.m_error = "FOA-SDK could not resolve its automatic workspace file.";
            RefreshSnapshot();
            return result;
        }

        const WorkspaceModel previousWorkspace = m_workspace;
        m_workspace = detected.m_workspace;
        AZStd::string saveError;
        if (!SaveWorkspace(targetPath, &saveError))
        {
            m_workspace = previousWorkspace;
            result.m_error = AZStd::string("Automatic workspace save failed: ") + saveError;
            RefreshSnapshot();
            return result;
        }

        result.m_persisted = true;
        return result;
    }
} // namespace TaintedGrailModdingSDK
