/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogDatabase.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/functional.h>

namespace TaintedGrailModdingSDK
{
    struct WorkspaceDocumentLoadResult
    {
        WorkspaceModel m_workspace;
        AZStd::string m_filePath;
    };

    struct FoundationWorkspaceLoadCandidate
    {
        WorkspaceModel m_workspace;
        AZStd::string m_workspaceFilePath;
        AZStd::string m_workspaceRootPath;
        GameProfile m_activeProfile;
        SourceEvidenceRegistry m_sourceRegistry;
        AZStd::vector<ImportIssue> m_importIssues;
        CatalogDatabase m_catalog;
        AZStd::string m_catalogFilePath;
    };

    struct FoundationWorkspaceLoadDependencies
    {
        using LoadWorkspaceCallback = AZStd::function<
            AZ::Outcome<WorkspaceDocumentLoadResult, AZStd::string>(const AZStd::string&)>;
        using ResolveWorkspaceRootCallback = AZStd::function<
            AZ::Outcome<AZStd::string, AZStd::string>(
                const WorkspaceModel&,
                const AZStd::string&)>;
        using LoadSourceEvidenceCallback = AZStd::function<
            AZ::Outcome<void, AZStd::string>(
                const AZStd::string&,
                AZStd::vector<SourceDocument>&,
                AZStd::vector<EvidenceDocument>&,
                AZStd::vector<ImportIssue>&)>;
        using CatalogExistsCallback = AZStd::function<bool(const AZStd::string&)>;
        using LoadCatalogCallback = AZStd::function<
            AZ::Outcome<CatalogDocument, AZStd::string>(const AZStd::string&)>;
        using GetCatalogPathCallback = AZStd::function<AZStd::string(const AZStd::string&)>;

        LoadWorkspaceCallback m_loadWorkspace;
        ResolveWorkspaceRootCallback m_resolveWorkspaceRoot;
        LoadSourceEvidenceCallback m_loadSourceEvidence;
        CatalogExistsCallback m_catalogExists;
        LoadCatalogCallback m_loadCatalog;
        GetCatalogPathCallback m_getCatalogPath;
    };

    class FoundationWorkspaceLoadService
    {
    public:
        FoundationWorkspaceLoadService() = default;
        explicit FoundationWorkspaceLoadService(FoundationWorkspaceLoadDependencies dependencies);

        AZ::Outcome<FoundationWorkspaceLoadCandidate, AZStd::string> BuildCandidate(
            const AZStd::string& filePath) const;

    private:
        FoundationWorkspaceLoadDependencies m_dependencies;
    };
} // namespace TaintedGrailModdingSDK
