/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationWorkspaceLoadService.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZ::Outcome<void, AZStd::string> ValidateSourceBinding(
            const SourceRecord& source,
            const GameProfile& profile)
        {
            if (source.m_profileId != profile.m_profileId
                || source.m_gameVersion != profile.m_gameVersion
                || source.m_branch != profile.m_branch
                || source.m_runtimeTarget != profile.m_runtimeTarget)
            {
                return AZ::Failure(
                    AZStd::string(
                        "Source document is bound to a different active game profile: ")
                    + source.m_sourceId);
            }
            return AZ::Success();
        }

        AZ::Outcome<void, AZStd::string> ValidateEvidenceBinding(
            const EvidenceDocument& document,
            const SourceRecord& source,
            const GameProfile& profile)
        {
            if (document.m_sourceId != source.m_sourceId
                || document.m_sourceFingerprint != source.m_fingerprint
                || document.m_profileId != source.m_profileId
                || document.m_gameVersion != source.m_gameVersion
                || document.m_branch != source.m_branch)
            {
                return AZ::Failure(AZStd::string(
                    "Evidence document header does not match its registered source identity and fingerprint."));
            }
            if (document.m_profileId != profile.m_profileId
                || document.m_gameVersion != profile.m_gameVersion
                || document.m_branch != profile.m_branch)
            {
                return AZ::Failure(AZStd::string(
                    "Evidence document is bound to a different active game profile."));
            }
            for (const EvidenceRecord& evidence : document.m_evidence)
            {
                if (evidence.m_sourceId != document.m_sourceId
                    || evidence.m_sourceFingerprint
                        != document.m_sourceFingerprint
                    || evidence.m_profileId != document.m_profileId
                    || evidence.m_gameVersion != document.m_gameVersion
                    || evidence.m_branch != document.m_branch)
                {
                    return AZ::Failure(
                        AZStd::string(
                            "Evidence record does not match its parent document binding: ")
                        + evidence.m_evidenceId);
                }
            }
            return AZ::Success();
        }

        AZ::Outcome<void, AZStd::string> RejectLoadErrors(
            const AZStd::vector<ImportIssue>& issues)
        {
            for (const ImportIssue& issue : issues)
            {
                if (issue.m_severity == "error")
                {
                    return AZ::Failure(
                        AZStd::string("Workspace source/evidence load failed: ")
                        + issue.m_code + ": " + issue.m_message);
                }
            }
            return AZ::Success();
        }
    } // namespace

    FoundationWorkspaceLoadService::FoundationWorkspaceLoadService(
        FoundationWorkspaceLoadDependencies dependencies)
        : m_dependencies(AZStd::move(dependencies))
    {
    }

    AZ::Outcome<FoundationWorkspaceLoadCandidate, AZStd::string>
    FoundationWorkspaceLoadService::BuildCandidate(
        const AZStd::string& filePath) const
    {
        if (!m_dependencies.m_loadWorkspace
            || !m_dependencies.m_resolveWorkspaceRoot
            || !m_dependencies.m_loadSourceEvidence
            || !m_dependencies.m_catalogExists
            || !m_dependencies.m_loadCatalog
            || !m_dependencies.m_getCatalogPath)
        {
            return AZ::Failure(AZStd::string(
                "Workspace load dependencies are incomplete; no live state was changed."));
        }

        auto workspaceLoad = m_dependencies.m_loadWorkspace(filePath);
        if (!workspaceLoad.IsSuccess())
        {
            return AZ::Failure(AZStd::string(workspaceLoad.GetError()));
        }
        WorkspaceDocumentLoadResult loaded = workspaceLoad.TakeValue();

        auto rootResult = m_dependencies.m_resolveWorkspaceRoot(
            loaded.m_workspace,
            loaded.m_filePath);
        if (!rootResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(rootResult.GetError()));
        }

        const GameProfile* activeProfile =
            loaded.m_workspace.FindActiveGameProfile();
        if (!activeProfile || !activeProfile->IsConfigured())
        {
            return AZ::Failure(AZStd::string(
                "A validated configured active game profile is required before publication."));
        }
        const GameProfile activeProfileCopy = *activeProfile;

        AZStd::vector<SourceDocument> sourceDocuments;
        AZStd::vector<EvidenceDocument> evidenceDocuments;
        AZStd::vector<ImportIssue> loadIssues;
        auto sourceLoad = m_dependencies.m_loadSourceEvidence(
            rootResult.GetValue(),
            sourceDocuments,
            evidenceDocuments,
            loadIssues);
        if (!sourceLoad.IsSuccess())
        {
            return AZ::Failure(AZStd::string(sourceLoad.GetError()));
        }
        auto issueCheck = RejectLoadErrors(loadIssues);
        if (!issueCheck.IsSuccess())
        {
            return AZ::Failure(AZStd::string(issueCheck.GetError()));
        }

        SourceEvidenceRegistry registry;
        for (const SourceDocument& document : sourceDocuments)
        {
            auto binding =
                ValidateSourceBinding(document.m_source, activeProfileCopy);
            if (!binding.IsSuccess())
            {
                return AZ::Failure(AZStd::string(binding.GetError()));
            }
            AZStd::string registryError;
            if (!registry.RegisterSource(document.m_source, &registryError))
            {
                return AZ::Failure(
                    AZStd::string(
                        "Source registry candidate rejected a document: ")
                    + registryError);
            }
            loadIssues.insert(
                loadIssues.end(),
                document.m_issues.begin(),
                document.m_issues.end());
        }

        for (const EvidenceDocument& document : evidenceDocuments)
        {
            const SourceRecord* source = registry.FindSource(document.m_sourceId);
            if (!source)
            {
                return AZ::Failure(
                    AZStd::string(
                        "Evidence document references an unknown source: ")
                    + document.m_sourceId);
            }
            auto binding =
                ValidateEvidenceBinding(document, *source, activeProfileCopy);
            if (!binding.IsSuccess())
            {
                return AZ::Failure(AZStd::string(binding.GetError()));
            }
            for (const EvidenceRecord& evidence : document.m_evidence)
            {
                AZStd::string registryError;
                if (!registry.RegisterEvidence(evidence, &registryError))
                {
                    return AZ::Failure(
                        AZStd::string(
                            "Evidence registry candidate rejected a document: ")
                        + registryError);
                }
            }
            loadIssues.insert(
                loadIssues.end(),
                document.m_issues.begin(),
                document.m_issues.end());
        }

        auto postRegistryIssues = RejectLoadErrors(loadIssues);
        if (!postRegistryIssues.IsSuccess())
        {
            return AZ::Failure(AZStd::string(postRegistryIssues.GetError()));
        }

        CatalogDatabase catalog;
        const AZStd::string catalogPath =
            m_dependencies.m_getCatalogPath(rootResult.GetValue());
        if (m_dependencies.m_catalogExists(rootResult.GetValue()))
        {
            auto catalogLoad =
                m_dependencies.m_loadCatalog(rootResult.GetValue());
            if (!catalogLoad.IsSuccess())
            {
                return AZ::Failure(AZStd::string(catalogLoad.GetError()));
            }
            AZStd::string catalogError;
            if (!catalog.ReplaceFromBoundDocument(
                    catalogLoad.GetValue(),
                    loaded.m_workspace,
                    activeProfileCopy,
                    registry,
                    &catalogError))
            {
                return AZ::Failure(
                    AZStd::string("Catalog candidate validation failed: ")
                    + catalogError);
            }
        }

        FoundationWorkspaceLoadCandidate candidate;
        candidate.m_workspace = AZStd::move(loaded.m_workspace);
        candidate.m_workspaceFilePath = AZStd::move(loaded.m_filePath);
        candidate.m_workspaceRootPath = rootResult.TakeValue();
        candidate.m_activeProfile = activeProfileCopy;
        candidate.m_sourceRegistry = AZStd::move(registry);
        candidate.m_importIssues = AZStd::move(loadIssues);
        candidate.m_catalog = AZStd::move(catalog);
        candidate.m_catalogFilePath = catalogPath;
        return AZ::Success(AZStd::move(candidate));
    }
} // namespace TaintedGrailModdingSDK
