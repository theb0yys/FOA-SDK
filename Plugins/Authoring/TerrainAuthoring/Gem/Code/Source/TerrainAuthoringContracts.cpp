/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "TerrainAuthoringContracts.h"

#include <AzCore/std/algorithm.h>

namespace TerrainAuthoring
{
    namespace
    {
        void SetError(AZStd::string* error, const char* message)
        {
            if (error)
            {
                *error = message;
            }
        }

        bool StartsWith(const AZStd::string& value, const char* prefix)
        {
            const AZStd::string expected(prefix);
            return value.size() >= expected.size()
                && value.substr(0, expected.size()) == expected;
        }

        TerrainAuthoringCommandDescriptor Command(
            TerrainAuthoringCommandKind kind,
            const char* commandId,
            const char* contractName,
            bool requiresUserSelectedLocalSource,
            bool requiresValidatedTerrainDocument,
            bool writesWorkspaceRevision)
        {
            TerrainAuthoringCommandDescriptor descriptor;
            descriptor.m_kind = kind;
            descriptor.m_commandId = commandId;
            descriptor.m_contractName = contractName;
            descriptor.m_requiresUserSelectedLocalSource = requiresUserSelectedLocalSource;
            descriptor.m_requiresValidatedTerrainDocument = requiresValidatedTerrainDocument;
            descriptor.m_writesWorkspaceRevision = writesWorkspaceRevision;
            descriptor.m_availableInShell = false;
            descriptor.m_localOnly = true;
            descriptor.m_invokesPreview = false;
            descriptor.m_invokesAssetProcessor = false;
            descriptor.m_invokesRuntime = false;
            return descriptor;
        }

        bool HasBlockedAuthority(const TerrainAuthoringAuthorityState& authority)
        {
            return !authority.m_runtimeUseAllowed
                && !authority.m_deploymentAllowed
                && !authority.m_publicationAllowed
                && !authority.m_packagingAllowed
                && !authority.m_gameWriteAllowed
                && !authority.m_evidencePromotionAllowed
                && !authority.m_directFoAInstallScanAllowed
                && !authority.m_externalProcessAllowed
                && !authority.m_roadAtlasMutationAllowed;
        }
    } // namespace

    TaintedGrailModdingSDK::ExtensionAPI::ExtensionDeclaration BuildExtensionDeclaration()
    {
        TaintedGrailModdingSDK::ExtensionAPI::ExtensionDeclaration declaration;
        declaration.m_extensionId = TerrainAuthoringExtensionId;
        declaration.m_displayName = TerrainAuthoringDisplayName;
        declaration.m_version = TerrainAuthoringVersion;
        declaration.m_supportedGameVersions = { "1.23.401" };
        declaration.m_supportedBranches = { "il2cpp", "mono" };
        declaration.m_capabilities = {
            TaintedGrailModdingSDK::ExtensionAPI::Capability::ReadActiveProfile,
        };
        return declaration;
    }

    AZStd::vector<TerrainAuthoringCommandDescriptor> BuildCommandDescriptors()
    {
        return {
            Command(
                TerrainAuthoringCommandKind::ImportLocalHeightmap,
                "terrain-authoring.import-local-heightmap",
                "Explicit user-selected local TerrainHeightmapDocumentV1 import",
                true,
                false,
                true),
            Command(
                TerrainAuthoringCommandKind::ValidateCandidate,
                "terrain-authoring.validate-candidate",
                "Validate candidate foa.terrain-heightmap document and payload inventory",
                true,
                false,
                false),
            Command(
                TerrainAuthoringCommandKind::OpenDocument,
                "terrain-authoring.open-document",
                "Open validated local terrain document revision",
                false,
                true,
                false),
            Command(
                TerrainAuthoringCommandKind::SaveRevision,
                "terrain-authoring.save-revision",
                "Persist workspace-owned terrain revision after validation",
                false,
                true,
                true),
            Command(
                TerrainAuthoringCommandKind::RevertRevision,
                "terrain-authoring.revert-revision",
                "Revert to a validated workspace-owned terrain revision",
                false,
                true,
                true),
            Command(
                TerrainAuthoringCommandKind::UndoEdit,
                "terrain-authoring.undo-edit",
                "Undo workspace-owned terrain edit delta",
                false,
                true,
                true),
            Command(
                TerrainAuthoringCommandKind::RedoEdit,
                "terrain-authoring.redo-edit",
                "Redo workspace-owned terrain edit delta",
                false,
                true,
                true),
        };
    }

    TerrainAuthoringServiceStatus BuildInitialServiceStatus()
    {
        return {};
    }

    bool ValidateShellContract(AZStd::string* error)
    {
        const auto declaration = BuildExtensionDeclaration();
        if (declaration.m_extensionId != TerrainAuthoringExtensionId
            || declaration.m_displayName != TerrainAuthoringDisplayName
            || declaration.m_version != TerrainAuthoringVersion)
        {
            SetError(error, "Terrain Authoring extension identity drifted.");
            return false;
        }
        if (declaration.m_capabilities.size() != 1
            || declaration.m_capabilities[0]
                != TaintedGrailModdingSDK::ExtensionAPI::Capability::ReadActiveProfile)
        {
            SetError(error, "Terrain Authoring shell must remain profile-read-only.");
            return false;
        }

        const TerrainAuthoringServiceStatus status = BuildInitialServiceStatus();
        if (status.m_requiredSchemaId
                != TaintedGrailModdingSDK::TerrainHeightmap::TerrainHeightmapSchemaId
            || status.m_requiredSchemaVersion
                != TaintedGrailModdingSDK::TerrainHeightmap::TerrainHeightmapSchemaVersion
            || status.m_visiblePaneRegistered
            || status.m_previewProjectionEnabled
            || status.m_assetProcessorProjectionEnabled
            || !HasBlockedAuthority(status.m_authority))
        {
            SetError(error, "Terrain Authoring shell status must remain non-UI and non-authoritative.");
            return false;
        }

        AZStd::vector<AZStd::string> seen;
        for (const TerrainAuthoringCommandDescriptor& command : BuildCommandDescriptors())
        {
            if (command.m_commandId.empty()
                || !StartsWith(command.m_commandId, "terrain-authoring.")
                || AZStd::find(seen.begin(), seen.end(), command.m_commandId) != seen.end()
                || command.m_availableInShell
                || !command.m_localOnly
                || command.m_invokesPreview
                || command.m_invokesAssetProcessor
                || command.m_invokesRuntime)
            {
                SetError(error, "Terrain Authoring shell command contracts drifted.");
                return false;
            }
            seen.push_back(command.m_commandId);
        }

        if (seen.size() != 7)
        {
            SetError(error, "Terrain Authoring shell command inventory is incomplete.");
            return false;
        }
        if (error)
        {
            error->clear();
        }
        return true;
    }
} // namespace TerrainAuthoring
