/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogDatabase.h"
#include "FoundationValidationService.h"
#include "SourceEvidenceRegistry.h"
#include "WorkspacePersistenceService.h"

namespace TaintedGrailModdingSDK
{
    class FoundationService
    {
    public:
        static FoundationService& Get();

        void Initialize();
        void Shutdown();
        bool IsInitialized() const;

        void SetWorkspace(const WorkspaceModel& workspace);
        bool SaveWorkspace(const AZStd::string& filePath, AZStd::string* error = nullptr);
        bool SaveWorkspace(AZStd::string* error = nullptr);
        bool LoadWorkspace(const AZStd::string& filePath, AZStd::string* error = nullptr);
        bool UpsertPack(const PackManifest& pack, AZStd::string* error = nullptr);
        bool RegisterSource(const SourceRecord& source, AZStd::string* error = nullptr);
        bool RegisterEvidence(const EvidenceRecord& evidence, AZStd::string* error = nullptr);
        bool UpsertCatalogRecord(const CatalogRecord& record, AZStd::string* error = nullptr);

        const WorkspaceModel& GetWorkspace() const;
        const AZStd::string& GetWorkspaceFilePath() const;
        const AZStd::vector<PackManifest>& GetPacks() const;
        const SourceEvidenceRegistry& GetSourceRegistry() const;
        const CatalogDatabase& GetCatalog() const;
        const FoundationSnapshot& GetSnapshot() const;

        void RefreshSnapshot();

    private:
        FoundationService() = default;

        WorkspaceModel m_workspace;
        AZStd::string m_workspaceFilePath;
        AZStd::vector<PackManifest> m_packs;
        SourceEvidenceRegistry m_sourceRegistry;
        CatalogDatabase m_catalog;
        FoundationValidationService m_validationService;
        WorkspacePersistenceService m_workspacePersistence;
        FoundationSnapshot m_snapshot;
        bool m_initialized = false;
    };
} // namespace TaintedGrailModdingSDK
