/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    FoundationService::FoundationService()
        : m_extensionApi(m_workspace, m_catalog, m_sourceRegistry)
        , m_workspacePersistence(m_pathPolicy)
        , m_packPersistence(
            m_pathPolicy,
            [this]() -> const WorkspaceModel&
            {
                return m_workspace;
            },
            [this]() -> const AZStd::string&
            {
                return m_workspaceFilePath;
            })
    {
        FoundationWorkspaceLoadDependencies dependencies;
        dependencies.m_loadWorkspace = [this](const AZStd::string& filePath)
            -> AZ::Outcome<WorkspaceDocumentLoadResult, AZStd::string>
        {
            WorkspaceDocumentLoadResult result;
            auto loaded = m_workspacePersistence.LoadCandidate(filePath, result.m_filePath);
            if (!loaded.IsSuccess())
            {
                return AZ::Failure(AZStd::string(loaded.GetError()));
            }
            result.m_workspace = loaded.TakeValue();
            return AZ::Success(AZStd::move(result));
        };
        dependencies.m_resolveWorkspaceRoot = [this](
            const WorkspaceModel& workspace,
            const AZStd::string& workspaceFilePath)
            -> AZ::Outcome<AZStd::string, AZStd::string>
        {
            auto root = m_pathPolicy.ResolveWorkspaceRoot(workspace, workspaceFilePath, true);
            if (!root.IsSuccess())
            {
                return AZ::Failure(AZStd::string(root.GetError()));
            }
            auto validation = m_pathPolicy.ValidateWorkspacePaths(workspace, root.GetValue());
            if (!validation.IsSuccess())
            {
                return AZ::Failure(AZStd::string(validation.GetError()));
            }
            return root;
        };
        dependencies.m_loadSourceEvidence = [this](
            const AZStd::string& workspaceRoot,
            AZStd::vector<SourceDocument>& sourceDocuments,
            AZStd::vector<EvidenceDocument>& evidenceDocuments,
            AZStd::vector<ImportIssue>& loadIssues)
        {
            return m_sourceEvidencePersistence.LoadWorkspaceDocuments(
                workspaceRoot,
                sourceDocuments,
                evidenceDocuments,
                loadIssues);
        };
        dependencies.m_catalogExists = [this](const AZStd::string& workspaceRoot)
        {
            return m_catalogPersistence.Exists(workspaceRoot);
        };
        dependencies.m_loadCatalog = [this](const AZStd::string& workspaceRoot)
        {
            return m_catalogPersistence.Load(workspaceRoot);
        };
        dependencies.m_getCatalogPath = [this](const AZStd::string& workspaceRoot)
        {
            return m_catalogPersistence.GetCatalogPath(workspaceRoot);
        };
        m_workspaceLoadService = FoundationWorkspaceLoadService(AZStd::move(dependencies));
    }

    FoundationService::FoundationService(
        FoundationWorkspaceLoadDependencies workspaceLoadDependencies)
        : m_extensionApi(m_workspace, m_catalog, m_sourceRegistry)
        , m_workspacePersistence(m_pathPolicy)
        , m_packPersistence(
            m_pathPolicy,
            [this]() -> const WorkspaceModel&
            {
                return m_workspace;
            },
            [this]() -> const AZStd::string&
            {
                return m_workspaceFilePath;
            })
        , m_workspaceLoadService(AZStd::move(workspaceLoadDependencies))
    {
    }
} // namespace TaintedGrailModdingSDK
