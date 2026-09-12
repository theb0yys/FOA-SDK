/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "TerrainImportHost.h"
#include "TerrainCampaignExportHost.h"
#include "TerrainNativeHandoff.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <algorithm>
#include <chrono>
#include <exception>
#include <vector>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        using namespace TerrainHeightmap;
        AZStd::string Az(const QString& value)
        {
            const auto bytes = value.toUtf8();
            return { bytes.constData(), static_cast<size_t>(bytes.size()) };
        }
        QString Qt(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }
        QJsonObject Failure(const QString& message)
        {
            return { { "status", "failed" }, { "issue_code", "terrain.operation-failed" }, { "message", message } };
        }
        QJsonObject Preview(const QString& workspace, const QString& locator, const ProfileBinding& profile, const ImportControl* control)
        {
            auto result = LoadWorkspaceTerrainPreview(Az(workspace), Az(locator), profile, control);
            if (!result.IsSuccess())
            {
                return Failure(Qt(result.GetError()));
            }
            const auto& value = result.GetValue();
            QImage image(static_cast<int>(value.m_width), static_cast<int>(value.m_height), QImage::Format_Grayscale8);
            for (AZ::u32 y = 0; y < value.m_height; ++y)
            {
                std::copy_n(
                    value.m_grayscale.data() + static_cast<size_t>(y) * value.m_width, value.m_width, image.scanLine(static_cast<int>(y)));
            }
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            if (!image.save(&buffer, "PNG"))
            {
                return Failure("Unable to create terrain preview.");
            }
            return { { "status", "complete" },
                     { "message", "Terrain revision opened." },
                     { "name", Qt(value.m_document.m_mapIdentity.m_displayName) },
                     { "revision", Qt(value.m_document.m_revision.m_revisionId) },
                     { "locator", locator },
                     { "width", static_cast<int>(value.m_document.m_grid.m_width) },
                     { "height", static_cast<int>(value.m_document.m_grid.m_height) },
                     { "preview", QString::fromLatin1(bytes.toBase64()) },
                     { "note",
                       value.m_document.m_sourceBinding.m_exporterId == "importer.campaign-ground-raw"
                           ? "Unsupported campaign reconstruction. Editing and game round-trip are unavailable. " +
                               Qt(value.m_document.m_provenance.m_limitations)
                           : QString() } };
        }
        QJsonObject Inventory(const QString& workspace, const ProfileBinding& profile, const ImportControl* control)
        {
            const QString root = QDir(workspace).filePath("Derived/Terrain");
            if (QFileInfo(root).exists() && QFileInfo(root).canonicalFilePath() != QFileInfo(root).absoluteFilePath())
            {
                return Failure("The terrain inventory must remain inside its workspace.");
            }
            QStringList directories{ root };
            int count = 0;
            int rejected = 0;
            std::vector<QJsonObject> rows;
            for (int directory = 0; directory < directories.size(); ++directory)
            {
                QDirIterator entries(directories[directory], QDir::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks);
                while (entries.hasNext())
                {
                    if (control->IsCancelled())
                    {
                        return Failure("Terrain refresh cancelled.");
                    }
                    const QString path = entries.next();
                    if (++count > 4096)
                    {
                        return Failure("The terrain revision inventory exceeds its scan limit.");
                    }
                    const QFileInfo entry(path);
                    if (entry.canonicalFilePath() != entry.absoluteFilePath())
                    {
                        ++rejected;
                        continue;
                    }
                    if (entry.isDir())
                    {
                        if (entry.fileName() != "Tiles" && QDir(root).relativeFilePath(path).count('/') < 4)
                        {
                            directories.append(path);
                        }
                        continue;
                    }
                    if (!path.endsWith("/terrain.tgheightmap.json"))
                    {
                        continue;
                    }
                    const QFileInfo info(path);
                    if (info.size() > 4 * 1024 * 1024 || info.canonicalFilePath() != info.absoluteFilePath())
                    {
                        ++rejected;
                        continue;
                    }
                    QFile file(path);
                    if (!file.open(QIODevice::ReadOnly))
                    {
                        ++rejected;
                        continue;
                    }
                    const auto data = file.read(4 * 1024 * 1024 + 1);
                    auto parsed = ParseDocumentJson(AZStd::string(data.constData(), static_cast<size_t>(data.size())));
                    if (!parsed.IsSuccess() || parsed.GetValue().m_profileBinding.m_profileFingerprint != profile.m_profileFingerprint ||
                        parsed.GetValue().m_profileBinding.m_profileId != profile.m_profileId ||
                        parsed.GetValue().m_profileBinding.m_gameVersion != profile.m_gameVersion ||
                        parsed.GetValue().m_profileBinding.m_branch != profile.m_branch ||
                        parsed.GetValue().m_profileBinding.m_runtimeTarget != profile.m_runtimeTarget)
                    {
                        ++rejected;
                        continue;
                    }
                    const auto& document = parsed.GetValue();
                    rows.push_back(
                        { { "revision", Qt(document.m_revision.m_revisionId) },
                          { "name",
                            Qt(document.m_mapIdentity.m_displayName) +
                                (document.m_sourceBinding.m_exporterId == "importer.campaign-ground-raw" ? " (unsupported reconstruction)"
                                                                                                         : "") },
                          { "created", Qt(document.m_revision.m_createdAtUtc) },
                          { "locator", QDir(workspace).relativeFilePath(path) } });
                }
            }
            std::sort(
                rows.begin(),
                rows.end(),
                [](const auto& a, const auto& b)
                {
                    return a.value("created").toString() > b.value("created").toString();
                });
            QJsonArray result;
            for (size_t index = 0; index < std::min<size_t>(64, rows.size()); ++index)
            {
                result.append(rows[index]);
            }
            return { { "status", "complete" },
                     { "rows", result },
                     { "rejected", rejected },
                     { "message",
                       rejected ? "Some revisions are invalid or belong to a different profile." : "Saved terrain revisions loaded." } };
        }
    } // namespace

    TerrainImportHost::~TerrainImportHost()
    {
        if (m_operation)
        {
            m_operation->m_cancelled = true;
        }
        if (m_future.valid())
        {
            m_future.wait();
        }
        if (!m_nativeResult.isEmpty())
        {
            QFile cancel(m_nativeResult.left(m_nativeResult.size() - 12) + ".cancel");
            if (cancel.open(QIODevice::WriteOnly))
            {
                cancel.write("cancel");
            }
        }
    }

    void TerrainImportHost::CollectCompleted()
    {
        if (!m_future.valid() || m_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            return;
        }
        try
        {
            m_snapshot = m_future.get();
        } catch (const std::exception&)
        {
            m_snapshot = Failure("Terrain operation failed unexpectedly.");
        }
        if (m_discardResult)
        {
            m_discardResult = false;
            m_operation.reset();
            m_snapshot = Failure("The workspace or profile changed. Refresh the terrain revisions.");
            return;
        }
        const QString locator = m_snapshot.take("locator").toString();
        if (!locator.isEmpty())
        {
            m_revisionPaths[m_snapshot.value("revision").toString()] = locator;
        }
        if (m_snapshot.contains("rows"))
        {
            QJsonArray rows;
            m_revisionPaths.clear();
            for (const auto& value : m_snapshot.value("rows").toArray())
            {
                auto row = value.toObject();
                const QString path = row.take("locator").toString();
                const QString revision = row.value("revision").toString();
                if (m_revisionPaths.contains(revision))
                {
                    m_snapshot = Failure("Duplicate terrain revision identities were found.");
                    m_revisionPaths.clear();
                    m_operation.reset();
                    return;
                }
                m_revisionPaths[revision] = path;
                rows.append(row);
            }
            m_snapshot["rows"] = rows;
        }
        m_operation.reset();
        const QString nativeRequest = m_snapshot.take("native_request").toString();
        if (!nativeRequest.isEmpty())
        {
            m_nativeResult = nativeRequest + ".result.json";
            if (!LaunchNativeTerrain(nativeRequest))
            {
                m_nativeResult.clear();
                m_snapshot = Failure("The native Editor terrain handoff is unavailable. Check the installed Editor scripts.");
            }
        }
    }

    QJsonObject TerrainImportHost::Dispatch(
        const QJsonObject& request,
        const QString& workspace,
        const QString& gameInstall,
        const TerrainHeightmap::ProfileBinding& profile,
        const QString& unityVersion)
    {
        const QString action = request.value("action").toString();
        const bool refreshProvider = action == "refresh" || action == "import-campaign";
        if (action == "import-campaign")
        {
            return Failure(
                "Campaign map import is unavailable: faithful source preservation and game round-trip support have not been verified.");
        }
        const QString profileIdentity = Qt(profile.m_profileFingerprint) + '|' + Qt(profile.m_profileId) + '|' + Qt(profile.m_gameVersion) +
            '|' + Qt(profile.m_branch) + '|' + Qt(profile.m_runtimeTarget) + '|' + gameInstall + '|' + unityVersion;
        if (!m_workspace.isEmpty() && (m_workspace != workspace || m_profileIdentity != profileIdentity))
        {
            if (m_operation)
            {
                m_operation->m_cancelled = true;
            }
            m_discardResult = m_future.valid();
            if (!m_nativeResult.isEmpty())
            {
                QFile cancel(m_nativeResult.left(m_nativeResult.size() - 12) + ".cancel");
                if (cancel.open(QIODevice::WriteOnly))
                {
                    cancel.write("cancel");
                }
                m_nativeResult.clear();
            }
            m_revisionPaths.clear();
            m_snapshot = Failure("The workspace or profile changed. Refresh the terrain revisions.");
        }
        m_workspace = workspace;
        m_profileIdentity = profileIdentity;
        CollectCompleted();
        if (!m_nativeResult.isEmpty())
        {
            if (action == "cancel")
            {
                QFile cancel(m_nativeResult.left(m_nativeResult.size() - 12) + ".cancel");
                if (cancel.open(QIODevice::WriteOnly))
                {
                    cancel.write("cancel");
                }
            }
            QFile result(m_nativeResult);
            if (result.open(QIODevice::ReadOnly) && result.size() <= 8192)
            {
                m_snapshot = QJsonDocument::fromJson(result.read(8193)).object();
                m_nativeResult.clear();
            }
            else
            {
                return { { "status", "running" }, { "busy", true }, { "message", "Opening terrain in the O3DE level viewport..." } };
            }
        }
        if (action == "cancel")
        {
            if (m_operation)
            {
                m_operation->m_cancelled = true;
            }
        }
        else if (action != "poll")
        {
            if (m_future.valid())
            {
                auto response = m_snapshot;
                response["busy"] = true;
                response["progress"] = m_operation ? m_operation->m_progress.load() : 0;
                return response;
            }
            if (action != "import" && action != "refresh" && action != "open" && action != "open-editor")
            {
                return Failure("This terrain operation is unavailable.");
            }
            const QString requestedSource = request.value("source").toString();
            const QString locator = m_revisionPaths.value(request.value("revision").toString());
            if ((action == "open" || action == "open-editor") && locator.isEmpty())
            {
                return Failure("Select a saved terrain revision first.");
            }
            m_operation = std::make_shared<Operation>();
            auto operation = m_operation;
            m_snapshot = { { "status", "running" }, { "message", action == "import" ? "Importing terrain..." : "Loading terrain..." } };
            m_future = std::async(
                std::launch::async,
                [=]() -> QJsonObject
                {
                    ImportControl control;
                    control.m_cancelled = [operation]()
                    {
                        return operation->m_cancelled.load();
                    };
                    control.m_progress = [operation](AZ::u64 done, AZ::u64 total)
                    {
                        operation->m_progress = total ? static_cast<int>(done * 90 / total) : 0;
                    };
                    if (action == "refresh")
                    {
                        return Inventory(workspace, profile, &control);
                    }
                    if (action == "open")
                    {
                        return Preview(workspace, locator, profile, &control);
                    }
                    if (action == "open-editor")
                    {
                        return PrepareNativeTerrain(workspace, locator, profile, &control);
                    }
                    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
                    const QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ssZ");
                    QString source = requestedSource;
                    const QFileInfo input(source);
                    const QString sourceCanonical = input.canonicalFilePath();
                    const QString gameCanonical = QFileInfo(gameInstall).canonicalFilePath();
                    if (sourceCanonical.isEmpty() || source.size() > 4096 ||
                        (!gameCanonical.isEmpty() &&
                         (sourceCanonical == gameCanonical || sourceCanonical.startsWith(gameCanonical + '/', Qt::CaseInsensitive))))
                    {
                        return Failure("Choose a local exported heightmap outside the game installation.");
                    }
                    const QString sidecar = source + ".json";
                    if (!QFileInfo(sidecar).isFile())
                    {
                        return Failure("The heightmap needs its exported metadata sidecar beside it (the same filename plus .json).");
                    }
                    QFile metadataFile(sidecar);
                    if (!metadataFile.open(QIODevice::ReadOnly) || metadataFile.size() > 4 * 1024 * 1024)
                    {
                        return Failure("The heightmap metadata is unreadable or exceeds its size limit.");
                    }
                    const auto sourceMetadata = QJsonDocument::fromJson(metadataFile.read(4 * 1024 * 1024 + 1)).object();
                    if (sourceMetadata.value("source_extraction").toObject().value("schema").toString() ==
                        "foa.campaign-heightmap-export.receipt")
                    {
                        return Failure(
                            "This is an unsupported campaign reconstruction. Reimporting it as a local heightmap does not preserve the "
                            "original map.");
                    }
                    RawHeightmapImportRequest raw;
                    raw.m_workspaceRoot = Az(workspace);
                    raw.m_rawInputPath = Az(source);
                    raw.m_sidecarPath = Az(sidecar);
                    raw.m_mapIdentity.m_mapId = Az("terrain-map.local." + id);
                    raw.m_mapIdentity.m_displayName = Az(input.completeBaseName().left(128));
                    raw.m_profileBinding = profile;
                    raw.m_operationId = Az("terrain-import." + id);
                    raw.m_createdAtUtc = Az(timestamp);
                    raw.m_control = &control;
                    QString publishedPath;
                    QString publishedRevision;
                    const QString suffix = input.suffix().toLower();
                    if (suffix == "raw" || suffix == "u16" || suffix == "r16")
                    {
                        auto imported = ImportRawHeightmapToWorkspace(raw);
                        if (!imported.IsSuccess())
                        {
                            return Failure(control.IsCancelled() ? "Terrain import cancelled." : Qt(imported.GetError()));
                        }
                        publishedPath = Qt(imported.GetValue().m_publishedManifestPath);
                        publishedRevision = Qt(imported.GetValue().m_document.m_revision.m_revisionId);
                    }
                    else if (suffix == "png" || suffix == "tif" || suffix == "tiff")
                    {
                        ImageHeightmapImportRequest image;
                        image.m_workspaceRoot = raw.m_workspaceRoot;
                        image.m_imageInputPath = raw.m_rawInputPath;
                        image.m_mapIdentity = raw.m_mapIdentity;
                        image.m_profileBinding = profile;
                        image.m_operationId = raw.m_operationId;
                        image.m_createdAtUtc = raw.m_createdAtUtc;
                        image.m_control = &control;
                        auto metadata = ReadImageImportSidecar(raw.m_sidecarPath, image);
                        if (!metadata.IsSuccess())
                        {
                            return Failure(Qt(metadata.GetError()));
                        }
                        auto imported = ImportImageHeightmapToWorkspace(image);
                        if (!imported.IsSuccess())
                        {
                            return Failure(control.IsCancelled() ? "Terrain import cancelled." : Qt(imported.GetError()));
                        }
                        publishedPath = Qt(imported.GetValue().m_publishedManifestPath);
                        publishedRevision = Qt(imported.GetValue().m_document.m_revision.m_revisionId);
                    }
                    else
                    {
                        return Failure("Choose a 16-bit PNG, TIFF, RAW, U16, or R16 heightmap.");
                    }
                    // The import is committed. Cancellation may stop preview, but cannot undo that successful commit.
                    auto response = Preview(workspace, QDir(workspace).relativeFilePath(publishedPath), profile, nullptr);
                    if (response.value("status").toString() == "failed")
                    {
                        response["status"] = "complete";
                        response["issue_code"] = "terrain.preview-unavailable";
                        response["message"] = "Terrain imported. Preview could not open: " + response.value("message").toString();
                    }
                    else
                    {
                        response["message"] = "Terrain imported and saved to your workspace.";
                    }
                    response["created"] = timestamp;
                    response["name"] = Qt(raw.m_mapIdentity.m_displayName);
                    response["revision"] = publishedRevision;
                    response["locator"] = QDir(workspace).relativeFilePath(publishedPath);
                    return response;
                });
        }
        auto response = m_snapshot;
        if (refreshProvider)
        {
            response["campaigns"] = QJsonArray{};
        }
        response["busy"] = m_future.valid() || !m_nativeResult.isEmpty();
        response["progress"] = m_operation ? m_operation->m_progress.load() : 100;
        return response;
    }
} // namespace TaintedGrailModdingSDK
