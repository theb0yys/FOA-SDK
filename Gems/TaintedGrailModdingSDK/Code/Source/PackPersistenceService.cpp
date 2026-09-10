/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PackPersistenceService.h"

#include "PersistenceJsonUtils.h"

#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/std/utility/move.h>

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

namespace TaintedGrailModdingSDK
{
    AZ::Outcome<void, AZStd::string> PackPersistenceService::Save(
        const PackManifest& pack,
        const AZStd::string& filePath) const
    {
        if (filePath.empty())
        {
            return AZ::Failure(AZStd::string("Pack manifest file path is required."));
        }
        if (pack.m_schemaVersion != 1)
        {
            return AZ::Failure(AZStd::string("Unsupported TG pack manifest schema version."));
        }
        if (!pack.HasStableIdentity())
        {
            return AZ::Failure(AZStd::string("Pack ID, owner ID, and semantic version must be valid before saving."));
        }
        if (pack.m_runtimeActionsEnabled)
        {
            return AZ::Failure(AZStd::string("Runtime actions cannot be enabled in an editor-owned pack manifest."));
        }
        AZStd::string packagePathError;
        if (!pack.HasAllowedPackagePaths(&packagePathError))
        {
            return AZ::Failure(AZStd::move(packagePathError));
        }

        // Preserve the existing envelope without truncating the previous manifest.
        AZStd::string bytes;
        AZ::IO::ByteContainerStream<AZStd::string> stream(&bytes);
        const auto serialized = AZ::JsonSerializationUtils::SaveObjectToStream(&pack, stream);
        if (!serialized.IsSuccess())
        {
            return AZ::Failure(AZStd::string(serialized.GetError()));
        }

        const QString path = QString::fromUtf8(filePath.c_str());
        if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        {
            return AZ::Failure(AZStd::string("Could not create the mod manifest folder."));
        }
        QSaveFile file(path);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)
            || file.write(bytes.data(), static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size())
            || !file.commit())
        {
            return AZ::Failure(AZStd::string("Could not save the mod manifest: ")
                + file.errorString().toUtf8().constData());
        }
        return AZ::Success();
    }

    AZ::Outcome<PackManifest, AZStd::string> PackPersistenceService::Load(
        const AZStd::string& filePath) const
    {
        if (filePath.empty())
        {
            return AZ::Failure(AZStd::string("Pack manifest file path is required."));
        }

        PackManifest pack;
        const AZ::Outcome<void, AZStd::string> loadResult =
            PersistenceJsonUtils::LoadObjectFromFile(pack, filePath);
        if (!loadResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(loadResult.GetError()));
        }
        if (pack.m_schemaVersion != 1)
        {
            return AZ::Failure(AZStd::string("Unsupported TG pack manifest schema version."));
        }
        if (!pack.HasStableIdentity())
        {
            return AZ::Failure(AZStd::string("The pack manifest does not contain a valid namespaced ID, owner, and semantic version."));
        }
        if (pack.m_runtimeActionsEnabled)
        {
            return AZ::Failure(AZStd::string("The pack manifest requests runtime actions, which are disabled in the editor foundation."));
        }
        AZStd::string packagePathError;
        if (!pack.HasAllowedPackagePaths(&packagePathError))
        {
            return AZ::Failure(AZStd::move(packagePathError));
        }

        return AZ::Success(AZStd::move(pack));
    }
} // namespace TaintedGrailModdingSDK
