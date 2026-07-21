/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExtensionAPI.h"

#include "CatalogDatabase.h"
#include "ResearchContractValidation.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK::ExtensionAPI
{
    namespace
    {
        void SetError(AZStd::string* error, AZStd::string message)
        {
            if (error)
            {
                *error = AZStd::move(message);
            }
        }

        bool IsBoundedText(
            const AZStd::string& value,
            size_t maximumLength,
            bool allowEmpty = false)
        {
            if ((!allowEmpty && value.empty()) || value.size() > maximumLength)
            {
                return false;
            }
            for (char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (byte < 0x20 || byte == 0x7f)
                {
                    return false;
                }
            }
            return true;
        }

        bool IsKnownCapability(Capability capability)
        {
            switch (capability)
            {
            case Capability::ReadActiveProfile:
            case Capability::QueryCatalog:
            case Capability::SubmitCandidateEvidence:
                return true;
            default:
                return false;
            }
        }

        bool ContainsCapability(
            const ExtensionDeclaration& declaration,
            Capability capability)
        {
            return AZStd::find(
                declaration.m_capabilities.begin(),
                declaration.m_capabilities.end(),
                capability) != declaration.m_capabilities.end();
        }

        bool ContainsString(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        template<class Value>
        bool HasDuplicates(AZStd::vector<Value> values)
        {
            AZStd::sort(values.begin(), values.end());
            return AZStd::adjacent_find(values.begin(), values.end()) != values.end();
        }

        bool ValidateCompatibilityValues(
            const AZStd::vector<AZStd::string>& values,
            const char* label,
            AZStd::string* error)
        {
            if (values.empty() || values.size() > 64 || HasDuplicates(values))
            {
                SetError(error, AZStd::string(label) + " declarations must be non-empty, unique, and bounded.");
                return false;
            }
            for (const AZStd::string& value : values)
            {
                if (!IsBoundedText(value, 128))
                {
                    SetError(error, AZStd::string(label) + " declarations contain invalid text.");
                    return false;
                }
            }
            return true;
        }

        bool ValidateDeclaration(
            const ExtensionDeclaration& declaration,
            AZStd::string* error)
        {
            if (!IsStableContractId(declaration.m_extensionId)
                || !IsBoundedText(declaration.m_displayName, 256)
                || !IsStrictSemanticVersion(declaration.m_version))
            {
                SetError(error, "Extension registration requires a stable ID, bounded display name, and strict semantic version.");
                return false;
            }
            if (!ValidateCompatibilityValues(
                    declaration.m_supportedGameVersions,
                    "Game-version",
                    error)
                || !ValidateCompatibilityValues(
                    declaration.m_supportedBranches,
                    "Branch",
                    error))
            {
                return false;
            }
            if (declaration.m_capabilities.empty()
                || declaration.m_capabilities.size() > 16
                || HasDuplicates(declaration.m_capabilities))
            {
                SetError(error, "Extension capabilities must be non-empty, unique, and bounded.");
                return false;
            }
            for (Capability capability : declaration.m_capabilities)
            {
                if (!IsKnownCapability(capability))
                {
                    SetError(error, "Extension registration contains an unknown capability.");
                    return false;
                }
            }
            return true;
        }
    } // namespace

    Service::Service(
        const WorkspaceModel& workspace,
        const CatalogDatabase& catalog,
        SourceEvidenceRegistry& sourceRegistry)
        : m_workspace(workspace)
        , m_catalog(catalog)
        , m_sourceRegistry(sourceRegistry)
    {
    }

    bool Service::RegisterExtension(
        const ExtensionDeclaration& declaration,
        AZStd::string* error)
    {
        if (!ValidateDeclaration(declaration, error))
        {
            return false;
        }
        if (m_extensions.size() >= MaximumExtensionCount)
        {
            SetError(error, "Extension registry capacity is exhausted.");
            return false;
        }
        if (FindExtension(declaration.m_extensionId))
        {
            SetError(error, "Extension identity is already registered.");
            return false;
        }

        ExtensionDeclaration canonical = declaration;
        AZStd::sort(
            canonical.m_supportedGameVersions.begin(),
            canonical.m_supportedGameVersions.end());
        AZStd::sort(
            canonical.m_supportedBranches.begin(),
            canonical.m_supportedBranches.end());
        AZStd::sort(
            canonical.m_capabilities.begin(),
            canonical.m_capabilities.end());
        m_extensions.push_back(AZStd::move(canonical));
        AZStd::sort(
            m_extensions.begin(),
            m_extensions.end(),
            [](const ExtensionDeclaration& left, const ExtensionDeclaration& right)
            {
                return left.m_extensionId < right.m_extensionId;
            });
        if (error)
        {
            error->clear();
        }
        return true;
    }

    AZStd::vector<ExtensionDeclaration> Service::GetRegisteredExtensions() const
    {
        return m_extensions;
    }

    const ExtensionDeclaration* Service::FindExtension(
        const AZStd::string& extensionId) const
    {
        const auto found = AZStd::lower_bound(
            m_extensions.begin(),
            m_extensions.end(),
            extensionId,
            [](const ExtensionDeclaration& declaration, const AZStd::string& identity)
            {
                return declaration.m_extensionId < identity;
            });
        return found != m_extensions.end() && found->m_extensionId == extensionId
            ? &*found
            : nullptr;
    }

    bool Service::Authorize(
        const AZStd::string& extensionId,
        Capability capability,
        const GameProfile*& profile,
        AZStd::string* error) const
    {
        const ExtensionDeclaration* declaration = FindExtension(extensionId);
        if (!declaration)
        {
            SetError(error, "Extension operation requires prior deterministic registration.");
            return false;
        }
        if (!ContainsCapability(*declaration, capability))
        {
            SetError(error, "Extension did not declare the required capability.");
            return false;
        }
        profile = m_workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured())
        {
            SetError(error, "Extension operation requires one configured active game profile.");
            return false;
        }
        if (!ContainsString(declaration->m_supportedGameVersions, profile->m_gameVersion)
            || !ContainsString(declaration->m_supportedBranches, profile->m_branch))
        {
            SetError(error, "Extension declaration does not support the exact active game version and branch.");
            return false;
        }
        return true;
    }

    bool Service::GetActiveProfile(
        const AZStd::string& extensionId,
        ProfileView& profile,
        AZStd::string* error) const
    {
        const GameProfile* activeProfile = nullptr;
        if (!Authorize(
                extensionId,
                Capability::ReadActiveProfile,
                activeProfile,
                error))
        {
            return false;
        }

        profile = {};
        profile.m_profileId = activeProfile->m_profileId;
        profile.m_displayName = activeProfile->m_displayName;
        profile.m_gameVersion = activeProfile->m_gameVersion;
        profile.m_branch = activeProfile->m_branch;
        profile.m_runtimeTarget = activeProfile->m_runtimeTarget;
        profile.m_unityVersion = activeProfile->m_unityVersion;
        profile.m_bepInExVersion = activeProfile->m_bepInExVersion;
        profile.m_dlcScopes = activeProfile->m_dlcScopes;
        AZStd::sort(profile.m_dlcScopes.begin(), profile.m_dlcScopes.end());
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool Service::QueryCatalog(
        const AZStd::string& extensionId,
        const CatalogQuery& query,
        AZStd::vector<CatalogRecord>& records,
        size_t maximumResults,
        AZStd::string* error) const
    {
        const GameProfile* activeProfile = nullptr;
        if (!Authorize(
                extensionId,
                Capability::QueryCatalog,
                activeProfile,
                error))
        {
            return false;
        }
        (void)activeProfile;
        if (maximumResults == 0 || maximumResults > MaximumCatalogQueryResults)
        {
            SetError(error, "Catalog query result limit is outside the public ExtensionAPI bound.");
            return false;
        }

        records = m_catalog.Query(query);
        if (records.size() > maximumResults)
        {
            records.resize(maximumResults);
        }
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool Service::SubmitCandidateEvidence(
        const AZStd::string& extensionId,
        const EvidenceRecord& evidence,
        AZStd::string* error)
    {
        const GameProfile* profile = nullptr;
        if (!Authorize(
                extensionId,
                Capability::SubmitCandidateEvidence,
                profile,
                error))
        {
            return false;
        }

        const SourceRecord* source = m_sourceRegistry.FindSource(evidence.m_sourceId);
        if (!source
            || !IsUsableImportStatus(source->m_importStatus)
            || !IsStableContractId(evidence.m_evidenceId)
            || !IsStableContractId(evidence.m_sourceId)
            || !IsSha256Fingerprint(evidence.m_sourceFingerprint)
            || evidence.m_sourceFingerprint != source->m_fingerprint
            || evidence.m_profileId != profile->m_profileId
            || evidence.m_gameVersion != profile->m_gameVersion
            || evidence.m_branch != profile->m_branch
            || source->m_profileId != profile->m_profileId
            || source->m_gameVersion != profile->m_gameVersion
            || source->m_branch != profile->m_branch
            || source->m_runtimeTarget != profile->m_runtimeTarget
            || !IsBoundedText(evidence.m_subjectRef, 512)
            || !IsBoundedText(evidence.m_claim, 4096)
            || !IsBoundedText(evidence.m_evidenceKind, 128)
            || !IsBoundedText(evidence.m_confidence, 64)
            || !IsBoundedText(evidence.m_locator, 1024)
            || !IsBoundedText(evidence.m_recordPath, 1024)
            || !IsStrictUtcTimestamp(evidence.m_extractedAt))
        {
            SetError(error, "Candidate evidence must be complete and bound to one imported source and the exact active profile.");
            return false;
        }

        return m_sourceRegistry.RegisterEvidence(evidence, error);
    }
} // namespace TaintedGrailModdingSDK::ExtensionAPI
