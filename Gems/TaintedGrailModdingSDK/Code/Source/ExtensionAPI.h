/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    class CatalogDatabase;
    class FoundationService;
    class SourceEvidenceRegistry;

    namespace ExtensionAPI
    {
        enum class Capability
        {
            ReadActiveProfile,
            QueryCatalog,
            SubmitCandidateEvidence,
        };

        struct ExtensionDeclaration
        {
            AZStd::string m_extensionId;
            AZStd::string m_displayName;
            AZStd::string m_version;
            AZStd::vector<AZStd::string> m_supportedGameVersions;
            AZStd::vector<AZStd::string> m_supportedBranches;
            AZStd::vector<Capability> m_capabilities;
        };

        struct ProfileView
        {
            AZStd::string m_profileId;
            AZStd::string m_displayName;
            AZStd::string m_gameVersion;
            AZStd::string m_branch;
            AZStd::string m_runtimeTarget;
            AZStd::string m_unityVersion;
            AZStd::string m_bepInExVersion;
            AZStd::vector<AZStd::string> m_dlcScopes;
        };

        class Service
        {
        public:
            static constexpr size_t MaximumExtensionCount = 256;
            static constexpr size_t MaximumCatalogQueryResults = 512;

            bool RegisterExtension(
                const ExtensionDeclaration& declaration,
                AZStd::string* error = nullptr);

            AZStd::vector<ExtensionDeclaration> GetRegisteredExtensions() const;

            bool GetActiveProfile(
                const AZStd::string& extensionId,
                ProfileView& profile,
                AZStd::string* error = nullptr) const;

            bool QueryCatalog(
                const AZStd::string& extensionId,
                const CatalogQuery& query,
                AZStd::vector<CatalogRecord>& records,
                size_t maximumResults = MaximumCatalogQueryResults,
                AZStd::string* error = nullptr) const;

            bool SubmitCandidateEvidence(
                const AZStd::string& extensionId,
                const EvidenceRecord& evidence,
                AZStd::string* error = nullptr);

        private:
            friend class ::TaintedGrailModdingSDK::FoundationService;

            Service(
                const WorkspaceModel& workspace,
                const CatalogDatabase& catalog,
                SourceEvidenceRegistry& sourceRegistry);

            const ExtensionDeclaration* FindExtension(
                const AZStd::string& extensionId) const;
            bool Authorize(
                const AZStd::string& extensionId,
                Capability capability,
                const GameProfile*& profile,
                AZStd::string* error) const;

            const WorkspaceModel& m_workspace;
            const CatalogDatabase& m_catalog;
            SourceEvidenceRegistry& m_sourceRegistry;
            AZStd::vector<ExtensionDeclaration> m_extensions;
        };
    } // namespace ExtensionAPI
} // namespace TaintedGrailModdingSDK
