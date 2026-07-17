/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

namespace TaintedGrailModdingSDK
{
    FoundationService& FoundationService::Get()
    {
        static FoundationService instance;
        return instance;
    }

    void FoundationService::Initialize()
    {
        if (m_initialized)
        {
            return;
        }

        m_workspace.m_workspaceId = "tgfoa.workspace.default";
        m_workspace.m_displayName = "Tainted Grail Modding Workspace";
        m_initialized = true;
        RefreshSnapshot();
    }

    void FoundationService::Shutdown()
    {
        m_workspace = {};
        m_packs.clear();
        m_sourceRegistry.Clear();
        m_catalog.Clear();
        m_snapshot = {};
        m_initialized = false;
    }

    bool FoundationService::IsInitialized() const
    {
        return m_initialized;
    }

    void FoundationService::SetWorkspace(const WorkspaceModel& workspace)
    {
        m_workspace = workspace;
        RefreshSnapshot();
    }

    bool FoundationService::UpsertPack(const PackManifest& pack, AZStd::string* error)
    {
        if (pack.m_packId.empty())
        {
            if (error)
            {
                *error = "Pack ID is required.";
            }
            return false;
        }

        for (PackManifest& existing : m_packs)
        {
            if (existing.m_packId == pack.m_packId)
            {
                existing = pack;
                RefreshSnapshot();
                return true;
            }
        }

        m_packs.push_back(pack);
        RefreshSnapshot();
        return true;
    }

    bool FoundationService::RegisterSource(const SourceRecord& source, AZStd::string* error)
    {
        const bool result = m_sourceRegistry.RegisterSource(source, error);
        if (result)
        {
            RefreshSnapshot();
        }
        return result;
    }

    bool FoundationService::RegisterEvidence(const EvidenceRecord& evidence, AZStd::string* error)
    {
        const bool result = m_sourceRegistry.RegisterEvidence(evidence, error);
        if (result)
        {
            RefreshSnapshot();
        }
        return result;
    }

    bool FoundationService::UpsertCatalogRecord(const CatalogRecord& record, AZStd::string* error)
    {
        const bool result = m_catalog.Upsert(record, error);
        if (result)
        {
            RefreshSnapshot();
        }
        return result;
    }

    const WorkspaceModel& FoundationService::GetWorkspace() const
    {
        return m_workspace;
    }

    const AZStd::vector<PackManifest>& FoundationService::GetPacks() const
    {
        return m_packs;
    }

    const SourceEvidenceRegistry& FoundationService::GetSourceRegistry() const
    {
        return m_sourceRegistry;
    }

    const CatalogDatabase& FoundationService::GetCatalog() const
    {
        return m_catalog;
    }

    const FoundationSnapshot& FoundationService::GetSnapshot() const
    {
        return m_snapshot;
    }

    void FoundationService::RefreshSnapshot()
    {
        m_snapshot = {};
        m_snapshot.m_workspaceName = m_workspace.m_displayName;
        m_snapshot.m_gameProfileCount = aznumeric_cast<AZ::u64>(m_workspace.m_gameProfiles.size());
        m_snapshot.m_packCount = aznumeric_cast<AZ::u64>(m_packs.size());
        m_snapshot.m_sourceCount = aznumeric_cast<AZ::u64>(m_sourceRegistry.GetSources().size());
        m_snapshot.m_evidenceCount = aznumeric_cast<AZ::u64>(m_sourceRegistry.GetEvidence().size());
        m_snapshot.m_catalogRecordCount = aznumeric_cast<AZ::u64>(m_catalog.GetRecords().size());
        m_snapshot.m_domainCoverage = m_catalog.BuildCoverage();
        m_snapshot.m_blockers = m_validationService.Evaluate(m_workspace, m_packs, m_sourceRegistry, m_catalog);
        m_snapshot.m_openBlockerCount = aznumeric_cast<AZ::u64>(m_snapshot.m_blockers.size());

        if (const GameProfile* profile = m_workspace.FindActiveGameProfile())
        {
            m_snapshot.m_activeGameProfile = profile->m_displayName;
            m_snapshot.m_gameVersion = profile->m_gameVersion;
            m_snapshot.m_branch = profile->m_branch;
        }
        else
        {
            m_snapshot.m_activeGameProfile = "Not configured";
            m_snapshot.m_gameVersion = "Unknown";
            m_snapshot.m_branch = "Unknown";
        }
    }
} // namespace TaintedGrailModdingSDK
