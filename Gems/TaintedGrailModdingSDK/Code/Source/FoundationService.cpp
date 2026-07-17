/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

#include "FoundationNotificationBus.h"

#include <AzCore/std/utility/move.h>

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
        m_workspaceFilePath.clear();
        m_packs.clear();
        m_activePackId.clear();
        m_activePackFilePath.clear();
        m_sourceRegistry.Clear();
        m_importIssues.clear();
        m_catalog.Clear();
        m_catalogFilePath.clear();
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

    bool FoundationService::SaveWorkspace(const AZStd::string& filePath, AZStd::string* error)
    {
        const AZ::Outcome<void, AZStd::string> result = m_workspacePersistence.Save(m_workspace, filePath);
        if (!result.IsSuccess())
        {
            if (error)
            {
                *error = result.GetError();
            }
            return false;
        }

        m_workspaceFilePath = filePath;
        RefreshSnapshot();
        return true;
    }

    bool FoundationService::SaveWorkspace(AZStd::string* error)
    {
        if (m_workspaceFilePath.empty())
        {
            if (error)
            {
                *error = "Use Save Workspace As before saving this workspace.";
            }
            return false;
        }

        return SaveWorkspace(m_workspaceFilePath, error);
    }

    bool FoundationService::LoadWorkspace(const AZStd::string& filePath, AZStd::string* error)
    {
        AZ::Outcome<WorkspaceModel, AZStd::string> result = m_workspacePersistence.Load(filePath);
        if (!result.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(result.GetError());
            }
            return false;
        }

        m_workspace = result.TakeValue();
        m_workspaceFilePath = filePath;
        if (!ReloadSourceEvidence(error))
        {
            return false;
        }
        if (!ReloadCatalog(error))
        {
            return false;
        }
        RefreshSnapshot();
        return true;
    }

    bool FoundationService::UpsertPack(const PackManifest& pack, AZStd::string* error)
    {
        if (!pack.HasStableIdentity())
        {
            if (error)
            {
                *error = "Pack ID must be namespaced and lowercase, owner ID is required, and version must be semantic.";
            }
            return false;
        }
        if (pack.m_runtimeActionsEnabled)
        {
            if (error)
            {
                *error = "Runtime actions cannot be enabled in an editor-owned pack manifest.";
            }
            return false;
        }

        if (PackManifest* existing = FindPackById(pack.m_packId))
        {
            *existing = pack;
        }
        else
        {
            m_packs.push_back(pack);
        }
        RefreshSnapshot();
        return true;
    }

    bool FoundationService::SetActivePack(const PackManifest& pack, AZStd::string* error)
    {
        const AZStd::string previousActiveId = m_activePackId;
        if (!UpsertPack(pack, error))
        {
            return false;
        }

        m_activePackId = pack.m_packId;
        if (m_activePackId != previousActiveId)
        {
            m_activePackFilePath.clear();
        }
        RefreshSnapshot();
        return true;
    }

    bool FoundationService::SaveActivePack(const AZStd::string& filePath, AZStd::string* error)
    {
        const PackManifest* pack = GetActivePack();
        if (!pack)
        {
            if (error)
            {
                *error = "Apply or open a pack before saving a manifest.";
            }
            return false;
        }

        const AZ::Outcome<void, AZStd::string> result = m_packPersistence.Save(*pack, filePath);
        if (!result.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(result.GetError());
            }
            return false;
        }

        m_activePackFilePath = filePath;
        RefreshSnapshot();
        return true;
    }

    bool FoundationService::SaveActivePack(AZStd::string* error)
    {
        if (m_activePackFilePath.empty())
        {
            if (error)
            {
                *error = "Use Save Pack As before saving this pack manifest.";
            }
            return false;
        }
        return SaveActivePack(m_activePackFilePath, error);
    }

    bool FoundationService::LoadPack(const AZStd::string& filePath, AZStd::string* error)
    {
        AZ::Outcome<PackManifest, AZStd::string> result = m_packPersistence.Load(filePath);
        if (!result.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(result.GetError());
            }
            return false;
        }

        PackManifest pack = result.TakeValue();
        if (!UpsertPack(pack, error))
        {
            return false;
        }

        m_activePackId = pack.m_packId;
        m_activePackFilePath = filePath;
        RefreshSnapshot();
        return true;
    }

    void FoundationService::ClearActivePack()
    {
        m_activePackId.clear();
        m_activePackFilePath.clear();
        RefreshSnapshot();
    }

    bool FoundationService::ImportSource(
        const SourceImportRequest& request,
        SourceEvidenceDocumentPaths* savedPaths,
        AZStd::string* error)
    {
        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured())
        {
            if (error)
            {
                *error = "Configure an exact active FoA game profile before importing sources.";
            }
            return false;
        }
        if (m_workspace.m_rootPath.empty())
        {
            if (error)
            {
                *error = "Configure a workspace root before importing sources.";
            }
            return false;
        }

        AZ::Outcome<SourceImportResult, AZStd::string> importResult = m_sourceImportService.Import(request, *profile);
        if (!importResult.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(importResult.GetError());
            }
            return false;
        }

        SourceImportResult imported = importResult.TakeValue();
        SourceEvidenceRegistry candidateRegistry = m_sourceRegistry;
        AZStd::string registryError;
        if (!candidateRegistry.RegisterSource(imported.m_sourceDocument.m_source, &registryError))
        {
            if (error)
            {
                *error = registryError;
            }
            return false;
        }
        for (const EvidenceRecord& evidence : imported.m_evidenceDocument.m_evidence)
        {
            if (!candidateRegistry.RegisterEvidence(evidence, &registryError))
            {
                if (error)
                {
                    *error = registryError;
                }
                return false;
            }
        }

        AZ::Outcome<SourceEvidenceDocumentPaths, AZStd::string> saveResult = m_sourceEvidencePersistence.SaveDocuments(
            imported.m_sourceDocument,
            imported.m_evidenceDocument,
            m_workspace.m_rootPath);
        if (!saveResult.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(saveResult.GetError());
            }
            return false;
        }

        m_sourceRegistry = AZStd::move(candidateRegistry);
        m_importIssues.insert(
            m_importIssues.end(),
            imported.m_sourceDocument.m_issues.begin(),
            imported.m_sourceDocument.m_issues.end());
        m_importIssues.insert(
            m_importIssues.end(),
            imported.m_evidenceDocument.m_issues.begin(),
            imported.m_evidenceDocument.m_issues.end());
        if (savedPaths)
        {
            *savedPaths = saveResult.TakeValue();
        }
        RefreshSnapshot();
        return true;
    }

    bool FoundationService::ReloadSourceEvidence(AZStd::string* error)
    {
        if (m_workspace.m_rootPath.empty())
        {
            m_sourceRegistry.Clear();
            m_importIssues.clear();
            RefreshSnapshot();
            return true;
        }

        AZStd::vector<SourceDocument> sourceDocuments;
        AZStd::vector<EvidenceDocument> evidenceDocuments;
        AZStd::vector<ImportIssue> loadIssues;
        const AZ::Outcome<void, AZStd::string> loadResult = m_sourceEvidencePersistence.LoadWorkspaceDocuments(
            m_workspace.m_rootPath,
            sourceDocuments,
            evidenceDocuments,
            loadIssues);
        if (!loadResult.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(loadResult.GetError());
            }
            return false;
        }

        SourceEvidenceRegistry rebuiltRegistry;
        AZStd::vector<ImportIssue> rebuiltIssues = AZStd::move(loadIssues);
        for (const SourceDocument& document : sourceDocuments)
        {
            rebuiltIssues.insert(
                rebuiltIssues.end(),
                document.m_issues.begin(),
                document.m_issues.end());

            AZStd::string registryError;
            if (!rebuiltRegistry.RegisterSource(document.m_source, &registryError))
            {
                rebuiltIssues.push_back(MakeRegistryIssue(
                    "error",
                    "registry.source-load-rejected",
                    registryError,
                    document.m_source.m_locator));
            }
        }

        for (const EvidenceDocument& document : evidenceDocuments)
        {
            rebuiltIssues.insert(
                rebuiltIssues.end(),
                document.m_issues.begin(),
                document.m_issues.end());
            for (const EvidenceRecord& evidence : document.m_evidence)
            {
                AZStd::string registryError;
                if (!rebuiltRegistry.RegisterEvidence(evidence, &registryError))
                {
                    rebuiltIssues.push_back(MakeRegistryIssue(
                        "error",
                        "registry.evidence-load-rejected",
                        registryError,
                        evidence.m_locator));
                }
            }
        }

        m_sourceRegistry = AZStd::move(rebuiltRegistry);
        m_importIssues = AZStd::move(rebuiltIssues);
        RefreshSnapshot();
        return true;
    }

    AZStd::vector<SourceImporterContract> FoundationService::GetSourceImporterContracts() const
    {
        return m_sourceImportService.GetContracts();
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

    const AZStd::string& FoundationService::GetWorkspaceFilePath() const
    {
        return m_workspaceFilePath;
    }

    const AZStd::vector<PackManifest>& FoundationService::GetPacks() const
    {
        return m_packs;
    }

    const PackManifest* FoundationService::GetActivePack() const
    {
        return FindPackById(m_activePackId);
    }

    const AZStd::string& FoundationService::GetActivePackFilePath() const
    {
        return m_activePackFilePath;
    }

    const SourceEvidenceRegistry& FoundationService::GetSourceRegistry() const
    {
        return m_sourceRegistry;
    }

    const AZStd::vector<ImportIssue>& FoundationService::GetImportIssues() const
    {
        return m_importIssues;
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
        m_snapshot.m_workspaceFilePath = m_workspaceFilePath;
        m_snapshot.m_gameProfileCount = static_cast<AZ::u64>(m_workspace.m_gameProfiles.size());
        m_snapshot.m_packCount = static_cast<AZ::u64>(m_packs.size());
        m_snapshot.m_sourceCount = static_cast<AZ::u64>(m_sourceRegistry.GetSources().size());
        m_snapshot.m_evidenceCount = static_cast<AZ::u64>(m_sourceRegistry.GetEvidence().size());
        m_snapshot.m_importIssues = m_importIssues;
        for (const ImportIssue& issue : m_importIssues)
        {
            if (issue.m_severity == "error")
            {
                ++m_snapshot.m_importErrorCount;
            }
            else if (issue.m_severity == "warning")
            {
                ++m_snapshot.m_importWarningCount;
            }
        }

        m_snapshot.m_catalogRecordCount = static_cast<AZ::u64>(m_catalog.GetRecords().size());
        m_snapshot.m_catalogRelationshipCount = static_cast<AZ::u64>(m_catalog.GetRelationships().size());
        m_snapshot.m_catalogValidationCount = static_cast<AZ::u64>(m_catalog.GetValidationHistory().size());
        m_snapshot.m_catalogGovernanceCount = static_cast<AZ::u64>(m_catalog.GetGovernanceHistory().size());
        for (const CatalogRecord& record : m_catalog.GetRecords())
        {
            if (record.m_stalenessState == "potentially_stale" || record.m_stalenessState == "stale")
            {
                ++m_snapshot.m_staleCatalogSubjectCount;
            }
            m_snapshot.m_allowedUsageCount += static_cast<AZ::u64>(record.m_allowedUsages.size());
            m_snapshot.m_forbiddenUsageCount += static_cast<AZ::u64>(record.m_forbiddenUsages.size());
        }
        for (const CatalogRelationship& relationship : m_catalog.GetRelationships())
        {
            if (relationship.m_stalenessState == "potentially_stale" || relationship.m_stalenessState == "stale")
            {
                ++m_snapshot.m_staleCatalogSubjectCount;
            }
            m_snapshot.m_allowedUsageCount += static_cast<AZ::u64>(relationship.m_allowedUsages.size());
            m_snapshot.m_forbiddenUsageCount += static_cast<AZ::u64>(relationship.m_forbiddenUsages.size());
        }

        m_snapshot.m_domainCoverage = m_catalog.BuildCoverage();
        m_snapshot.m_blockers = m_validationService.Evaluate(
            m_workspace,
            m_packs,
            m_sourceRegistry,
            m_importIssues,
            m_catalog);
        AZStd::vector<BlockerRecord> governanceBlockers = m_governanceBlockerService.Evaluate(
            m_workspace,
            m_sourceRegistry,
            m_catalog);
        m_snapshot.m_blockers.insert(
            m_snapshot.m_blockers.end(),
            governanceBlockers.begin(),
            governanceBlockers.end());
        m_snapshot.m_openBlockerCount = static_cast<AZ::u64>(m_snapshot.m_blockers.size());

        if (const GameProfile* profile = m_workspace.FindActiveGameProfile())
        {
            m_snapshot.m_activeGameProfile = profile->m_displayName;
            m_snapshot.m_gameVersion = profile->m_gameVersion;
            m_snapshot.m_branch = profile->m_branch;
            m_snapshot.m_runtimeTarget = profile->m_runtimeTarget;
            m_snapshot.m_unityVersion = profile->m_unityVersion;
            m_snapshot.m_bepInExVersion = profile->m_bepInExVersion;
        }
        else
        {
            m_snapshot.m_activeGameProfile = "Not configured";
            m_snapshot.m_gameVersion = "Unknown";
            m_snapshot.m_branch = "Unknown";
            m_snapshot.m_runtimeTarget = "Unknown";
            m_snapshot.m_unityVersion = "Unknown";
            m_snapshot.m_bepInExVersion = "Unknown";
        }

        if (const PackManifest* pack = GetActivePack())
        {
            m_snapshot.m_activePackId = pack->m_packId;
            m_snapshot.m_activePackName = pack->m_displayName;
            m_snapshot.m_activePackVersion = pack->m_version;
            m_snapshot.m_activePackFilePath = m_activePackFilePath;
        }
        else
        {
            m_snapshot.m_activePackId = "Not configured";
            m_snapshot.m_activePackName = "Not configured";
            m_snapshot.m_activePackVersion = "Unknown";
            m_snapshot.m_activePackFilePath = "Not saved";
        }

        FoundationNotificationBus::Broadcast(&FoundationNotifications::OnFoundationChanged);
    }

    PackManifest* FoundationService::FindPackById(const AZStd::string& packId)
    {
        for (PackManifest& pack : m_packs)
        {
            if (pack.m_packId == packId)
            {
                return &pack;
            }
        }
        return nullptr;
    }

    const PackManifest* FoundationService::FindPackById(const AZStd::string& packId) const
    {
        for (const PackManifest& pack : m_packs)
        {
            if (pack.m_packId == packId)
            {
                return &pack;
            }
        }
        return nullptr;
    }

    ImportIssue FoundationService::MakeRegistryIssue(
        AZStd::string severity,
        AZStd::string code,
        AZStd::string message,
        AZStd::string locator)
    {
        ImportIssue issue;
        issue.m_issueId = "issue.registry.";
        issue.m_issueId += code;
        issue.m_issueId += ".";
        issue.m_issueId += locator;
        issue.m_severity = AZStd::move(severity);
        issue.m_code = AZStd::move(code);
        issue.m_message = AZStd::move(message);
        issue.m_locator = AZStd::move(locator);
        return issue;
    }
} // namespace TaintedGrailModdingSDK
