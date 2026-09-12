/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ProjectImageService.h"
#include "AssetLocalisationService.h"
#include "PathPolicyService.h"
#include <AzCore/Utils/Utils.h>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QTemporaryFile>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString IQ(const AZStd::string& v) { return QString::fromUtf8(v.data(), static_cast<int>(v.size())); }
        AZStd::string IA(const QString& v) { const auto b = v.toUtf8(); return {b.constData(), static_cast<size_t>(b.size())}; }
        AZStd::string ImageError(const char* text) { return AZStd::string(text); }
        QString Resolved(QString path)
        {
            path = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
            QStringList suffix;
            while (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink())
            {
                const QFileInfo part(path); suffix.prepend(part.fileName());
                const auto parent = part.absolutePath(); if (parent == path) { return {}; } path = parent;
            }
            const auto canonical = QFileInfo(path).canonicalFilePath();
            return canonical.isEmpty() ? QString{} : QDir::cleanPath(canonical + (suffix.isEmpty() ? "" : "/" + suffix.join('/')));
        }
        bool Inside(const QString& path, const QString& root)
        {
            const auto p = Resolved(path), r = Resolved(root);
            return !p.isEmpty() && !r.isEmpty() && PathPolicyService::IsCanonicalPathContained(IA(r), IA(p),
#ifdef Q_OS_WIN
                true
#else
                false
#endif
            );
        }
        bool Protected(const QString& file, const WorkspaceModel& workspace, const AZStd::string& root)
        {
            const auto engine = AZ::Utils::GetEnginePath();
            if (!engine.empty() && Inside(file, QString::fromUtf8(engine.c_str()))) { return true; }
            for (const auto& p : workspace.m_gameProfiles)
            {
                for (const auto* protectedRoot : {&p.m_installPath, &p.m_managedAssembliesPath, &p.m_pluginPath, &p.m_extractedDataPath})
                {
                    if (protectedRoot->empty()) { continue; }
                    const auto absolute = QDir(IQ(root)).absoluteFilePath(IQ(*protectedRoot));
                    if (Inside(file, absolute)) { return true; }
                }
            }
            return Inside(file, QDir::home().filePath("Saved Games"))
                || Inside(file, QDir(IQ(root)).filePath("PreviewArtifacts"))
                || Inside(file, QDir(IQ(root)).filePath("Sources"));
        }
        AZStd::string Hash(const QByteArray& bytes)
        { return "sha256:" + IA(QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())); }
        AZ::Outcome<ProjectImage, AZStd::string> Decode(const QString& file)
        {
            QFile input(file); const QFileInfo info(input);
            const auto maximum = static_cast<qint64>(AssetLocalisationService::MaximumImageBytes);
            if (!info.isFile() || info.size() < 1 || info.size() > maximum || !input.open(QIODevice::ReadOnly))
            { return AZ::Failure(ImageError("Choose a readable PNG or JPEG image up to 8 MiB.")); }
            ProjectImage result; result.m_bytes = input.read(maximum + 1);
            if (result.m_bytes.size() != info.size() || result.m_bytes.size() > maximum || input.error() != QFileDevice::NoError)
            { return AZ::Failure(ImageError("The image changed while reading or exceeded the byte limit.")); }
            QBuffer buffer(&result.m_bytes); buffer.open(QIODevice::ReadOnly); QImageReader reader(&buffer);
            reader.setDecideFormatFromContent(true); reader.setAutoTransform(true);
            const auto format = reader.format().toLower(); const auto size = reader.size();
            if ((format != "png" && format != "jpeg") || !size.isValid() || size.width() > 4096 || size.height() > 4096
                || static_cast<AZ::u64>(size.width()) * size.height() > AssetLocalisationService::MaximumImagePixels || reader.imageCount() > 1)
            { return AZ::Failure(ImageError("Use a single PNG/JPEG image up to 4096 pixels per side and 4,194,304 pixels total.")); }
            result.m_image = reader.read();
            if (result.m_image.isNull()) { return AZ::Failure(ImageError("The image contents could not be decoded. Choose an undamaged PNG or JPEG.")); }
            auto& p = result.m_metadata; p.m_fingerprint = Hash(result.m_bytes);
            p.m_byteSize = static_cast<AZ::u64>(result.m_bytes.size());
            p.m_width = static_cast<AZ::u32>(result.m_image.width()); p.m_height = static_cast<AZ::u32>(result.m_image.height());
            p.m_mediaType = format == "png" ? "image/png" : "image/jpeg";
            return AZ::Success(AZStd::move(result));
        }
    }
    AZ::Outcome<ProjectImage, AZStd::string> ProjectImageService::ReadSource(
        const AZStd::string& file, const WorkspaceModel& workspace, const AZStd::string& root)
    {
        if (file.empty() || root.empty() || !QDir::isAbsolutePath(IQ(file)) || !QDir::isAbsolutePath(IQ(root)) || Protected(IQ(file), workspace, root))
        { return AZ::Failure(ImageError("Choose your own image outside game, engine, extraction and save folders.")); }
        return Decode(IQ(file));
    }
    AZ::Outcome<ProjectImage, AZStd::string> ProjectImageService::ReadManaged(
        const ProjectAssetProfile& asset, const AZStd::string& owner, const WorkspaceModel& workspace, const AZStd::string& root)
    {
        const auto valid = AssetLocalisationService::ValidateAsset(asset, owner);
        if (!valid.IsSuccess()) { return AZ::Failure(AZStd::string(valid.GetError())); }
        const auto path = QDir(IQ(root)).filePath(IQ(asset.m_sourcePath));
        if (root.empty() || !Inside(path, IQ(root))) { return AZ::Failure(ImageError("The managed image path escapes its workspace.")); }
        auto read = ReadSource(IA(path), workspace, root);
        if (!read.IsSuccess()) { return read; }
        const auto& actual = read.GetValue().m_metadata;
        if (actual.m_fingerprint != asset.m_fingerprint || actual.m_byteSize != asset.m_byteSize || actual.m_mediaType != asset.m_mediaType
            || actual.m_width != asset.m_width || actual.m_height != asset.m_height)
        { return AZ::Failure(ImageError("The saved image is missing or changed. Choose the correct image and save it again.")); }
        return read;
    }
    AZ::Outcome<AZStd::string, AZStd::string> ProjectImageService::Store(
        const ProjectImage& image, const AZStd::string& owner, const WorkspaceModel& workspace, const AZStd::string& root)
    {
        const auto relative = AssetLocalisationService::ImagePath(owner, image.m_metadata.m_fingerprint, image.m_metadata.m_mediaType);
        const auto path = QDir(IQ(root)).filePath(IQ(relative)); const auto directory = QFileInfo(path).absolutePath();
        if (root.empty() || !QDir::isAbsolutePath(IQ(root)) || relative.empty() || !QFileInfo(IQ(root)).isDir() || Protected(IQ(root), workspace, root)
            || Protected(directory, workspace, root) || !Inside(directory, IQ(root))
            || image.m_bytes.isEmpty() || image.m_bytes.size() > static_cast<qint64>(AssetLocalisationService::MaximumImageBytes)
            || Hash(image.m_bytes) != image.m_metadata.m_fingerprint)
        { return AZ::Failure(ImageError("Cannot store this image inside the active authoring workspace.")); }
        if (!QDir().mkpath(directory) || !Inside(directory, IQ(root)) || !Inside(path, IQ(root)))
        { return AZ::Failure(ImageError("Cannot create the managed image folder inside the workspace.")); }
        if (QFileInfo::exists(path))
        {
            QFile existing(path);
            if (!existing.open(QIODevice::ReadOnly) || existing.size() != image.m_bytes.size()
                || existing.read(image.m_bytes.size() + 1) != image.m_bytes)
            { return AZ::Failure(ImageError("A different file occupies the managed image path. Existing content was preserved.")); }
            return AZ::Success(relative);
        }
        QTemporaryFile pending(directory + "/.image-XXXXXX.tmp");
        if (!pending.open() || pending.write(image.m_bytes) != image.m_bytes.size() || !pending.flush())
        { return AZ::Failure(ImageError("Could not write the complete image; the catalog was not changed.")); }
        pending.close();
        if (!pending.rename(path))
        { return AZ::Failure(ImageError("Could not commit the image without replacing existing content.")); }
        pending.setAutoRemove(false);
        return AZ::Success(relative);
    }
}
