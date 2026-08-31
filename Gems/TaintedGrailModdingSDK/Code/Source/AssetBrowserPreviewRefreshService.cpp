/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetBrowserPreviewRefreshService.h"

#include "FoundationModels.h"
#include "FoundationService.h"
#include "PathPolicyService.h"

#include <AzCore/IO/FileIO.h>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/API/EditorPythonRunnerRequestsBus.h>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QStringList>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr qint64 MaximumEvidenceDocumentBytes = 32 * 1024 * 1024;
        constexpr int MaximumEvidenceCandidates = 4096;
        constexpr const char* ImportProofFileName = "foa-o3de-asset-processor-import-proof.json";
        constexpr const char* ImportProofDocumentKind = "foa-o3de-asset-processor-import-proof";
        constexpr const char* PaneModelFileName = "foa-asset-browser-pane-model.json";
        constexpr const char* PaneModelDocumentKind = "foa-asset-browser-pane-model";
        constexpr const char* InstalledPaneRefreshTool = "@engroot@/scripts/foa-sdk/foa_asset_browser_pane_refresh.py";

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        struct ExactProfileContext
        {
            QString m_workspacePath;
            QString m_extractedRoot;
            QString m_profileId;
            QString m_gameVersion;
            QString m_branch;
            QString m_runtimeTarget;
        };

        struct EvidenceCandidate
        {
            QString m_path;
            QString m_capturedAt;
            QString m_identity;
            QDateTime m_modified;
        };

        AZ::Outcome<ExactProfileContext, AZStd::string> ResolveContext()
        {
            const FoundationService& foundation = FoundationService::Get();
            const WorkspaceModel& workspace = foundation.GetWorkspace();
            const GameProfile* profile = workspace.FindActiveGameProfile();
            if (!profile)
            {
                return AZ::Failure(AZStd::string("No active Fall of Avalon profile is available for item visuals."));
            }
            if (foundation.GetWorkspaceFilePath().empty())
            {
                return AZ::Failure(AZStd::string("The current mod workspace must be saved before item visuals can be refreshed."));
            }

            const QFileInfo workspaceInfo(ToQString(foundation.GetWorkspaceFilePath()));
            const QString workspacePath = workspaceInfo.canonicalFilePath();
            if (workspacePath.isEmpty() || !workspaceInfo.isFile())
            {
                return AZ::Failure(AZStd::string("The current mod workspace file is unavailable."));
            }
            if (profile->m_extractedDataPath.empty())
            {
                return AZ::Failure(AZStd::string("The active Fall of Avalon profile has no generated-data location."));
            }

            const QString configuredExtracted = ToQString(profile->m_extractedDataPath);
            const QString extractedPath = QFileInfo(configuredExtracted).isAbsolute()
                ? QDir::cleanPath(configuredExtracted)
                : QDir(workspaceInfo.absolutePath()).absoluteFilePath(configuredExtracted);
            QFileInfo extractedInfo(extractedPath);
            if (!extractedInfo.exists() && !QDir().mkpath(extractedPath))
            {
                return AZ::Failure(AZStd::string("FOA-SDK could not create the generated item-visual data folder."));
            }
            extractedInfo.refresh();
            if (!extractedInfo.isDir())
            {
                return AZ::Failure(AZStd::string("The generated item-visual data location is not a directory."));
            }
            QString extractedRoot = extractedInfo.canonicalFilePath();
            if (extractedRoot.isEmpty())
            {
                extractedRoot = extractedInfo.absoluteFilePath();
            }

            ExactProfileContext context;
            context.m_workspacePath = workspacePath;
            context.m_extractedRoot = extractedRoot;
            context.m_profileId = ToQString(profile->m_profileId);
            context.m_gameVersion = ToQString(profile->m_gameVersion);
            context.m_branch = ToQString(profile->m_branch);
            context.m_runtimeTarget = ToQString(profile->m_runtimeTarget);
            return AZ::Success(context);
        }

        bool LoadExactProfileDocument(
            const QString& path,
            const QString& expectedFileName,
            const QString& expectedKind,
            const ExactProfileContext& context,
            QJsonObject& root)
        {
            const QFileInfo info(path);
            const QString canonicalPath = info.canonicalFilePath();
            if (canonicalPath.isEmpty()
                || info.fileName() != expectedFileName
                || !PathPolicyService::IsCanonicalPathContained(
                    ToAzString(context.m_extractedRoot),
                    ToAzString(canonicalPath),
                    true))
            {
                return false;
            }

            QFile file(canonicalPath);
            if (!file.open(QIODevice::ReadOnly)
                || file.size() <= 0
                || file.size() > MaximumEvidenceDocumentBytes)
            {
                return false;
            }
            const QByteArray payload = file.readAll();
            file.close();

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                return false;
            }

            root = document.object();
            return root.value(QStringLiteral("SchemaVersion")).toInt(-1) == 1
                && root.value(QStringLiteral("DocumentKind")).toString() == expectedKind
                && root.value(QStringLiteral("ProfileId")).toString() == context.m_profileId
                && root.value(QStringLiteral("GameVersion")).toString() == context.m_gameVersion
                && root.value(QStringLiteral("Branch")).toString() == context.m_branch
                && root.value(QStringLiteral("RuntimeTarget")).toString() == context.m_runtimeTarget;
        }

        bool IsNewer(const EvidenceCandidate& candidate, const EvidenceCandidate& current)
        {
            if (current.m_path.isEmpty())
            {
                return true;
            }
            if (!candidate.m_capturedAt.isEmpty() && candidate.m_capturedAt != current.m_capturedAt)
            {
                return current.m_capturedAt.isEmpty() || candidate.m_capturedAt > current.m_capturedAt;
            }
            return candidate.m_modified > current.m_modified;
        }

        AZ::Outcome<EvidenceCandidate, AZStd::string> FindLatestImportProof(const ExactProfileContext& context)
        {
            const QString o3deRoot = QDir(context.m_extractedRoot).filePath(QStringLiteral("PreviewArtifacts/O3DE"));
            if (!QFileInfo(o3deRoot).isDir())
            {
                return AZ::Failure(AZStd::string(
                    "No generated O3DE preview products are available yet. Build or import item preview products, then refresh again."));
            }

            EvidenceCandidate best;
            int candidateCount = 0;
            QDirIterator iterator(
                o3deRoot,
                QStringList{ QString::fromUtf8(ImportProofFileName) },
                QDir::Files | QDir::Readable,
                QDirIterator::Subdirectories);
            while (iterator.hasNext())
            {
                const QString path = iterator.next();
                if (++candidateCount > MaximumEvidenceCandidates)
                {
                    return AZ::Failure(AZStd::string("Too many item-preview import proofs were found; refresh was stopped safely."));
                }

                QJsonObject root;
                if (!LoadExactProfileDocument(
                        path,
                        QString::fromUtf8(ImportProofFileName),
                        QString::fromUtf8(ImportProofDocumentKind),
                        context,
                        root))
                {
                    continue;
                }

                const QString proofId = root.value(QStringLiteral("ImportProofId")).toString();
                if (proofId.isEmpty())
                {
                    continue;
                }

                const QFileInfo info(path);
                EvidenceCandidate candidate;
                candidate.m_path = info.canonicalFilePath();
                candidate.m_capturedAt = root.value(QStringLiteral("CapturedAt")).toString();
                candidate.m_identity = proofId;
                candidate.m_modified = info.lastModified();
                if (IsNewer(candidate, best))
                {
                    best = candidate;
                }
            }

            if (best.m_path.isEmpty())
            {
                return AZ::Failure(AZStd::string(
                    "No exact-profile O3DE preview import proof is available yet. Build or import item preview products, then refresh again."));
            }
            return AZ::Success(best);
        }

        QString ResolvePaneRefreshToolPath()
        {
            if (AZ::IO::FileIOBase* fileIo = AZ::IO::FileIOBase::GetDirectInstance())
            {
                char resolvedPath[AZ_MAX_PATH_LEN] = { 0 };
                if (fileIo->ResolvePath(InstalledPaneRefreshTool, resolvedPath, AZ_MAX_PATH_LEN))
                {
                    const QFileInfo installed(QString::fromUtf8(resolvedPath));
                    if (installed.isFile())
                    {
                        const QString canonical = installed.canonicalFilePath();
                        return canonical.isEmpty() ? installed.absoluteFilePath() : canonical;
                    }
                }
            }

#if defined(TG_SDK_ASSET_BROWSER_PANE_REFRESH_TOOL_SOURCE)
            const QFileInfo source(QString::fromUtf8(TG_SDK_ASSET_BROWSER_PANE_REFRESH_TOOL_SOURCE));
            if (source.isFile())
            {
                const QString canonical = source.canonicalFilePath();
                return canonical.isEmpty() ? source.absoluteFilePath() : canonical;
            }
#endif
            return {};
        }

        AZ::Outcome<void, AZStd::string> ExecutePaneModelGenerator(
            const ExactProfileContext& context,
            const EvidenceCandidate& proof)
        {
            const QString toolPath = ResolvePaneRefreshToolPath();
            if (toolPath.isEmpty())
            {
                return AZ::Failure(AZStd::string("FOA-SDK's item-visual refresh tool is missing from this installation."));
            }
            if (!AzToolsFramework::EditorPythonRunnerRequestBus::HasHandlers())
            {
                return AZ::Failure(AZStd::string("FOA-SDK's embedded authoring runtime is not ready to refresh item visuals."));
            }

            AZStd::vector<AZStd::string> ownedArgs;
            ownedArgs.emplace_back("--workspace");
            ownedArgs.emplace_back(ToAzString(context.m_workspacePath));
            ownedArgs.emplace_back("--import-proof");
            ownedArgs.emplace_back(ToAzString(proof.m_path));
            ownedArgs.emplace_back("--replace");

            AZStd::vector<AZStd::string_view> args;
            args.reserve(ownedArgs.size());
            for (const AZStd::string& value : ownedArgs)
            {
                args.emplace_back(value);
            }

            const AZStd::string toolPathUtf8 = ToAzString(toolPath);
            bool succeeded = false;
            AzToolsFramework::EditorPythonRunnerRequestBus::BroadcastResult(
                succeeded,
                &AzToolsFramework::EditorPythonRunnerRequestBus::Events::ExecuteByFilenameWithArgs,
                AZStd::string_view(toolPathUtf8),
                args);
            if (!succeeded)
            {
                return AZ::Failure(AZStd::string("FOA-SDK could not regenerate the item-visual Asset Browser model."));
            }
            return AZ::Success();
        }

        AZ::Outcome<EvidenceCandidate, AZStd::string> FindLatestPaneModel(
            const ExactProfileContext& context,
            const AZStd::string& sourceImportProofId)
        {
            const QString assetBrowserRoot = QDir(context.m_extractedRoot).filePath(QStringLiteral("PreviewArtifacts/AssetBrowser"));
            if (!QFileInfo(assetBrowserRoot).isDir())
            {
                return AZ::Failure(AZStd::string("Item-visual refresh completed without producing an Asset Browser model."));
            }

            EvidenceCandidate best;
            int candidateCount = 0;
            QDirIterator iterator(
                assetBrowserRoot,
                QStringList{ QString::fromUtf8(PaneModelFileName) },
                QDir::Files | QDir::Readable,
                QDirIterator::Subdirectories);
            while (iterator.hasNext())
            {
                const QString path = iterator.next();
                if (++candidateCount > MaximumEvidenceCandidates)
                {
                    return AZ::Failure(AZStd::string("Too many generated item-visual models were found; refresh was stopped safely."));
                }

                QJsonObject root;
                if (!LoadExactProfileDocument(
                        path,
                        QString::fromUtf8(PaneModelFileName),
                        QString::fromUtf8(PaneModelDocumentKind),
                        context,
                        root)
                    || root.value(QStringLiteral("SourceImportProofId")).toString() != ToQString(sourceImportProofId))
                {
                    continue;
                }

                const QFileInfo info(path);
                EvidenceCandidate candidate;
                candidate.m_path = info.canonicalFilePath();
                candidate.m_capturedAt = root.value(QStringLiteral("CapturedAt")).toString();
                candidate.m_identity = root.value(QStringLiteral("AssetBrowserModelId")).toString();
                candidate.m_modified = info.lastModified();
                if (!candidate.m_identity.isEmpty() && IsNewer(candidate, best))
                {
                    best = candidate;
                }
            }

            if (best.m_path.isEmpty())
            {
                return AZ::Failure(AZStd::string("Item-visual refresh did not produce a valid exact-profile Asset Browser model."));
            }
            return AZ::Success(best);
        }
    } // namespace

    AZ::Outcome<AssetBrowserPreviewRefreshResult, AZStd::string>
    AssetBrowserPreviewRefreshService::RefreshActiveProfileModel() const
    {
        auto contextOutcome = ResolveContext();
        if (!contextOutcome.IsSuccess())
        {
            return AZ::Failure(contextOutcome.TakeError());
        }
        const ExactProfileContext context = contextOutcome.TakeValue();

        auto proofOutcome = FindLatestImportProof(context);
        if (!proofOutcome.IsSuccess())
        {
            return AZ::Failure(proofOutcome.TakeError());
        }
        const EvidenceCandidate proof = proofOutcome.TakeValue();

        auto generationOutcome = ExecutePaneModelGenerator(context, proof);
        if (!generationOutcome.IsSuccess())
        {
            return AZ::Failure(generationOutcome.TakeError());
        }

        auto modelOutcome = FindLatestPaneModel(context, ToAzString(proof.m_identity));
        if (!modelOutcome.IsSuccess())
        {
            return AZ::Failure(modelOutcome.TakeError());
        }
        const EvidenceCandidate model = modelOutcome.TakeValue();

        AssetBrowserPreviewRefreshResult result;
        result.m_importProofPath = ToAzString(proof.m_path);
        result.m_modelPath = ToAzString(model.m_path);
        return AZ::Success(result);
    }
} // namespace TaintedGrailModdingSDK
