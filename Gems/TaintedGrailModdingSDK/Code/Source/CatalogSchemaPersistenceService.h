/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogPersistenceService.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    class CatalogSchemaMigrationService final
    {
    public:
        AZ::Outcome<CatalogDocument, AZStd::string> MigrateToCurrent(
            CatalogDocument document) const
        {
            if (document.m_schemaVersion == CurrentCatalogSchemaVersion)
            {
                return AZ::Success(AZStd::move(document));
            }
            if (document.m_schemaVersion != LegacyCatalogSchemaVersion)
            {
                return AZ::Failure(AZStd::string(
                    "Unsupported canonical catalog schema version; this SDK "
                    "migrates schema 1 and persists schema 2."));
            }
            if (!document.m_actorProfiles.empty()
                || !document.m_troopProfiles.empty()
                || !document.m_troopMembers.empty())
            {
                return AZ::Failure(AZStd::string(
                    "Catalog schema 1 cannot contain population collections."));
            }

            document.m_actorProfiles.clear();
            document.m_troopProfiles.clear();
            document.m_troopMembers.clear();
            document.m_schemaVersion = CurrentCatalogSchemaVersion;
            return AZ::Success(AZStd::move(document));
        }
    };

    //! Migration-aware durable boundary used by Foundation services.
    class CatalogSchemaPersistenceService final
    {
    public:
        AZStd::string GetCatalogPath(const AZStd::string& workspaceRoot) const
        {
            return m_persistence.GetCatalogPath(workspaceRoot);
        }

        bool Exists(const AZStd::string& workspaceRoot) const
        {
            return m_persistence.Exists(workspaceRoot);
        }

        AZ::Outcome<AZStd::string, AZStd::string> Save(
            const CatalogDocument& document,
            const AZStd::string& workspaceRoot) const
        {
            const auto migration = m_migration.MigrateToCurrent(document);
            if (!migration.IsSuccess())
            {
                return AZ::Failure(AZStd::string(migration.GetError()));
            }
            return m_persistence.Save(migration.GetValue(), workspaceRoot);
        }

        AZ::Outcome<CatalogDocument, AZStd::string> Load(
            const AZStd::string& workspaceRoot) const
        {
            auto loaded = m_persistence.Load(workspaceRoot);
            if (!loaded.IsSuccess())
            {
                return AZ::Failure(AZStd::string(loaded.GetError()));
            }
            return m_migration.MigrateToCurrent(loaded.TakeValue());
        }

    private:
        CatalogPersistenceService m_persistence;
        CatalogSchemaMigrationService m_migration;
    };
} // namespace TaintedGrailModdingSDK
