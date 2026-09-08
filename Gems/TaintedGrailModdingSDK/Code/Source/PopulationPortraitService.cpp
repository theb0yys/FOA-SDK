/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "PopulationPortraitService.h"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>

namespace TaintedGrailModdingSDK
{
    AZ::Outcome<QString, QString> PopulationPortraitService::ReferenceForFile(const QString& workspaceRoot, const QString& file)
    {
        const auto root = QFileInfo(workspaceRoot).canonicalFilePath();
        const auto path = QFileInfo(file).canonicalFilePath();
        const auto sensitivity = QDir::separator() == QChar('\\') ? Qt::CaseInsensitive : Qt::CaseSensitive;
        if (root.isEmpty() || path.isEmpty() || !path.startsWith(root + '/', sensitivity))
        {
            return AZ::Failure(QStringLiteral("Choose a portrait image stored inside this authoring workspace."));
        }
        return AZ::Success(QStringLiteral("$workspace/") + QDir(root).relativeFilePath(path));
    }

    AZ::Outcome<QImage, QString> PopulationPortraitService::Read(const QString& workspaceRoot, const QString& reference)
    {
        if (reference.isEmpty())
        {
            return AZ::Failure(QStringLiteral("No portrait is assigned. The supported NPC template component supplies no portrait or model binding."));
        }
        if (!reference.startsWith("$workspace/") || reference.contains('\\')
            || reference.mid(11).split('/').contains("..") || QDir::isAbsolutePath(reference.mid(11)))
        {
            return AZ::Failure(QStringLiteral("This visual reference has no supported local portrait. Choose a workspace PNG or JPEG image."));
        }
        const auto path = QDir(workspaceRoot).filePath(reference.mid(11));
        if (!ReferenceForFile(workspaceRoot, path).IsSuccess())
        {
            return AZ::Failure(QStringLiteral("The portrait is missing or resolves outside this workspace."));
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024)
        {
            return AZ::Failure(QStringLiteral("Cannot read this portrait within the 8 MiB image limit."));
        }
        auto bytes = file.read(8 * 1024 * 1024 + 1);
        if (bytes.size() > 8 * 1024 * 1024) { return AZ::Failure(QStringLiteral("Portrait grew beyond the image limit.")); }
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const auto size = reader.size();
        if ((reader.format() != "png" && reader.format() != "jpeg") || !size.isValid()
            || size.width() > 4096 || size.height() > 4096)
        {
            return AZ::Failure(QStringLiteral("Choose a valid PNG or JPEG portrait no larger than 4096 by 4096 pixels."));
        }
        reader.setScaledSize(size.scaled(512, 512, Qt::KeepAspectRatio));
        auto image = reader.read();
        if (image.isNull()) { return AZ::Failure(QStringLiteral("The portrait could not be decoded: ") + reader.errorString()); }
        return AZ::Success(AZStd::move(image));
    }
}
