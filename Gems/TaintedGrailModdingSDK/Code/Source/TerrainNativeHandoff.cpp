/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "TerrainNativeHandoff.h"
#include <AzCore/IO/FileIO.h>
#include <AzToolsFramework/API/EditorPythonRunnerRequestsBus.h>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string Az(const QString& text)
        {
            const auto utf8 = text.toUtf8();
            return { utf8.constData(), static_cast<size_t>(utf8.size()) };
        }
        QJsonObject Fail(const QString& message)
        {
            return { { "status", "failed" }, { "message", message } };
        }
        bool MakeContained(const QString& root, const QString& relative)
        {
            QString current = root;
            for (const QString& part : relative.split('/'))
            {
                if (part.isEmpty() || part == "." || part == "..")
                {
                    return false;
                }
                current = QDir(current).filePath(part);
                if (!QFileInfo::exists(current) && !QDir().mkdir(current))
                {
                    return false;
                }
                const QFileInfo info(current);
                if (!info.isDir() || info.isSymLink() || info.canonicalFilePath() != info.absoluteFilePath())
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace
    QJsonObject PrepareNativeTerrain(
        const QString& workspace,
        const QString& locator,
        const TerrainHeightmap::ProfileBinding& profile,
        const TerrainHeightmap::ImportControl* control)
    {
        auto loaded = TerrainHeightmap::LoadWorkspaceTerrainPreview(Az(workspace), Az(locator), profile, control, true);
        if (!loaded.IsSuccess())
        {
            return Fail(QString::fromUtf8(loaded.GetError().c_str()));
        }
        const QString root = QFileInfo(workspace).canonicalFilePath();
        // Mutable game-derived assets must never be placed inside a source checkout.
        QDir ancestor(root);
        do
        {
            if (QFileInfo::exists(ancestor.filePath(".git")))
            {
                return Fail("Choose an SDK workspace outside source control for editable terrain.");
            }
        } while (ancestor.cdUp());
        const auto& value = loaded.GetValue();
        const auto& document = value.m_document;
        if (document.m_sourceBinding.m_exporterId == "importer.campaign-ground-raw")
        {
            return Fail(
                "This campaign reconstruction cannot be opened for editing. It does not preserve the original map, and game round-trip "
                "support is unverified.");
        }
        const QString workspaceId =
            QString::fromLatin1(QCryptographicHash::hash(root.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
        const QString revisionId = QString::fromLatin1(
            QCryptographicHash::hash(QByteArray(document.m_revision.m_revisionId.c_str()), QCryptographicHash::Sha256).toHex());
        const QString relative = "foa_maps/" + workspaceId + '/' + revisionId;
        if (!MakeContained(root, "EditorAssets/" + relative + "/Levels/Map") || !MakeContained(root, "Staging/TerrainNative"))
        {
            return Fail("The native terrain output must remain inside its workspace.");
        }
        const QString directory = QDir(root).filePath("EditorAssets/" + relative);
        const QString imagePath = QDir(directory).filePath("height_gsi.tif");
        if (QFileInfo::exists(imagePath))
        {
            if (QFileInfo(imagePath).canonicalFilePath() != imagePath || !QFileInfo(imagePath).isFile())
            {
                return Fail("The editable terrain image has an unsafe location.");
            }
            // Reopening never replaces the user's painted image.
        }
        else
        {
            // The pinned AP's Qt PNG loader reduces samples to RGBA8. Use the
            // same uncompressed U16 TIFF layout accepted by our bounded reader.
            QSaveFile file(imagePath);
            if (!file.open(QIODevice::WriteOnly))
            {
                return Fail("Unable to save the editable terrain image.");
            }
            QDataStream output(&file);
            output.setByteOrder(QDataStream::LittleEndian);
            output.writeRawData("II", 2);
            output << quint16(42) << quint32(8) << quint16(10);
            const quint32 pixelOffset = 8 + 2 + 10 * 12 + 4;
            const auto entry = [&output](quint16 tag, quint16 type, quint32 value)
            {
                output << tag << type << quint32(1) << value;
            };
            entry(256, 4, document.m_grid.m_width);
            entry(257, 4, document.m_grid.m_height);
            entry(258, 3, 16); // unsigned 16-bit samples
            entry(259, 3, 1); // uncompressed
            entry(262, 3, 1); // black is zero
            entry(273, 4, pixelOffset);
            entry(277, 3, 1); // one grayscale channel
            entry(278, 4, document.m_grid.m_height);
            entry(279, 4, static_cast<quint32>(value.m_samples.size() * 2));
            entry(339, 3, 1); // unsigned integer
            output << quint32(0);
            QByteArray row(static_cast<int>(document.m_grid.m_width * 2), Qt::Uninitialized);
            for (AZ::u32 y = 0; y < document.m_grid.m_height; ++y)
            {
                if (control && control->IsCancelled())
                {
                    return Fail("Opening terrain cancelled.");
                }
                for (AZ::u32 x = 0; x < document.m_grid.m_width; ++x)
                {
                    const AZ::u16 sample = value.m_samples[static_cast<size_t>(y) * document.m_grid.m_width + x];
                    row[2 * x] = static_cast<char>(sample & 255);
                    row[2 * x + 1] = static_cast<char>(sample >> 8);
                }
                if (file.write(row) != row.size())
                {
                    return Fail("Unable to write the editable terrain image.");
                }
            }
            if (output.status() != QDataStream::Ok || (control && control->IsCancelled()) || !file.commit())
            {
                return Fail("Unable to publish the editable terrain image.");
            }
        }
        const auto canonical = TerrainHeightmap::BuildCanonicalDocumentJson(document);
        const auto metadata = QJsonDocument::fromJson(QByteArray(canonical.c_str())).object();
        const QJsonObject request{ { "workspace", root },
                                   { "asset_root", QDir(root).filePath("EditorAssets") },
                                   { "asset", relative + "/height_gsi.tif.streamingimage" },
                                   { "image", imagePath },
                                   { "level", QDir(directory).filePath("Levels/Map/Map.prefab") },
                                   { "document", metadata } };
        const QString requestPath =
            QDir(root).filePath("Staging/TerrainNative/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json");
        QSaveFile file(requestPath);
        const auto bytes = QJsonDocument(request).toJson(QJsonDocument::Compact);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        {
            return Fail("Unable to prepare the native terrain handoff.");
        }
        return { { "status", "running" }, { "message", "Preparing the O3DE level..." }, { "native_request", requestPath } };
    }
    bool LaunchNativeTerrain(const QString& requestPath)
    {
        if (!AzToolsFramework::EditorPythonRunnerRequestBus::HasHandlers())
        {
            return false;
        }
        char resolved[AZ_MAX_PATH_LEN]{};
        QString script;
        if (auto* io = AZ::IO::FileIOBase::GetInstance(); io &&
            io->ResolvePath("@engroot@/scripts/foa-sdk/foa_terrain_native_editor.py", resolved, sizeof(resolved)) &&
            QFileInfo::exists(resolved))
        {
            script = QString::fromUtf8(resolved);
        }
#ifdef TG_SDK_NATIVE_TERRAIN_TOOL_SOURCE
        if (script.isEmpty() && QFileInfo::exists(TG_SDK_NATIVE_TERRAIN_TOOL_SOURCE))
        {
            script = TG_SDK_NATIVE_TERRAIN_TOOL_SOURCE;
        }
#endif
        if (script.isEmpty())
        {
            return false;
        }
        const auto filename = Az(script);
        const auto argument = Az(requestPath);
        const AZStd::vector<AZStd::string_view> args{ argument };
        bool success = false;
        AzToolsFramework::EditorPythonRunnerRequestBus::BroadcastResult(
            success,
            &AzToolsFramework::EditorPythonRunnerRequestBus::Events::ExecuteByFilenameWithArgs,
            AZStd::string_view(filename),
            args);
        return success;
    }
} // namespace TaintedGrailModdingSDK
