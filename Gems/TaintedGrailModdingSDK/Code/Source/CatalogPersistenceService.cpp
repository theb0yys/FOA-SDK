/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogPersistenceService.h"

#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/std/utility/move.h>

#include <QByteArray>
#include <QDir>
#include <QFileInfo>

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

        AZ::Outcome<void, AZStd::string> ValidatePersistedIdentity(const CatalogDocument& document)
        {
            for (const CatalogRecord& record : document.m_records)
            {
                const bool knownKind = record.m_identityKind == "native"
                    || record.m_identityKind == "synthetic"
                    || record.m_identityKind == "composite"
                    || record.m_identityKind == "source_scoped";
                if (!knownKind)
                {
                    return AZ::Failure(AZStd::string(
                        "Canonical catalog contains an unsupported identity kind."));
                }
                if (record.m_identityKind == "native" && !record.m_ownerPackId.empty())
                {
                    return AZ::Failure(AZStd::string(
                        "Native catalog records must not claim custom pack ownership."));
                }
                if (record.m_identityKind == "synthetic" && !record.m_nativeRefExact.empty())
                {
                    return AZ::Failure(AZStd::string(
                        "Synthetic catalog records must not borrow an exact native reference."));
                }
            }
            return AZ::Success();
        }
    } // namespace

    AZStd::string CatalogPersistenceService::GetCatalogPath(const AZStd::string& workspaceRoot) const
    {
        if (workspaceRoot.empty())
        {
            return {};
        }
        return ToAzString(QDir(ToQString(workspaceRoot)).filePath(QStringLiteral("Catalog/catalog.tgcatalog.json")));
    }

    bool CatalogPersistenceService::Exists(const AZStd::string& workspaceRoot) const
    {
        const AZStd::string path = GetCatalogPath(workspaceRoot);
        return !path.empty() && QFileInfo::exists(ToQString(path));
    }

    AZ::Outcome<AZStd::string, AZStd::string> CatalogPersistenceService::Save(
        const CatalogDocument& document,
        const AZStd::string& workspaceRoot) const
    {
        if (workspaceRoot.empty())
        {
            return AZ::Failure(AZStd::string("A workspace root is required before the canonical catalog can be saved."));
        }
        if (!document.UsesSupportedSchema())
        {
            return AZ::Failure(AZStd::string("Unsupported canonical catalog schema version."));
        }
        if (document.m_workspaceId.empty() || document.m_profileId.empty()
            || document.m_gameVersion.empty() || document.m_branch.empty())
        {
            return AZ::Failure(AZStd::string(
                "Canonical catalog documents require workspace, profile, game version, and branch binding."));
        }
        const AZ::Outcome<void, AZStd::string> identityResult = ValidatePersistedIdentity(document);
        if (!identityResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(identityResult.GetError()));
        }

        const AZStd::string path = GetCatalogPath(workspaceRoot);
        const QString directory = QFileInfo(ToQString(path)).absolutePath();
        if (!QDir().mkpath(directory))
        {
            return AZ::Failure(AZStd::string("Unable to create the catalog directory inside the workspace."));
        }

        const AZ::Outcome<void, AZStd::string> saveResult = AZ::JsonSerializationUtils::SaveObjectToFile(&document, path);
        if (!saveResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(saveResult.GetError()));
        }
        return AZ::Success(path);
    }

    AZ::Outcome<CatalogDocument, AZStd::string> CatalogPersistenceService::Load(
        const AZStd::string& workspaceRoot) const
    {
        const AZStd::string path = GetCatalogPath(workspaceRoot);
        if (path.empty())
        {
            return AZ::Failure(AZStd::string("A workspace root is required before the canonical catalog can be loaded."));
        }
        if (!QFileInfo::exists(ToQString(path)))
        {
            return AZ::Failure(AZStd::string("The workspace does not contain a canonical catalog document."));
        }

        CatalogDocument document;
        const AZ::Outcome<void, AZStd::string> loadResult = AZ::JsonSerializationUtils::LoadObjectFromFile(document, path);
        if (!loadResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(loadResult.GetError()));
        }
        if (!document.UsesSupportedSchema())
        {
            return AZ::Failure(AZStd::string("Unsupported canonical catalog schema version."));
        }
        const AZ::Outcome<void, AZStd::string> identityResult = ValidatePersistedIdentity(document);
        if (!identityResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(identityResult.GetError()));
        }
        return AZ::Success(AZStd::move(document));
    }
} // namespace TaintedGrailModdingSDK
