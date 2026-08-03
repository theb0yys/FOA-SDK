/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <ExtensionAPI.h>
#include <TerrainHeightmapDocument.h>

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TerrainAuthoring
{
    constexpr const char* TerrainAuthoringExtensionId = "extension.terrain-authoring";
    constexpr const char* TerrainAuthoringDisplayName = "Terrain Authoring";
    constexpr const char* TerrainAuthoringVersion = "0.1.0";
    constexpr const char* TerrainAuthoringGateId = "WA-TH-002";

    enum class TerrainAuthoringCommandKind
    {
        ImportLocalHeightmap,
        ValidateCandidate,
        OpenDocument,
        SaveRevision,
        RevertRevision,
        UndoEdit,
        RedoEdit,
    };

    struct TerrainAuthoringCommandDescriptor
    {
        TerrainAuthoringCommandKind m_kind = TerrainAuthoringCommandKind::OpenDocument;
        AZStd::string m_commandId;
        AZStd::string m_contractName;
        bool m_requiresActiveProfile = true;
        bool m_requiresUserSelectedLocalSource = false;
        bool m_requiresValidatedTerrainDocument = false;
        bool m_writesWorkspaceRevision = false;
        bool m_availableInShell = false;
        bool m_localOnly = true;
        bool m_invokesPreview = false;
        bool m_invokesAssetProcessor = false;
        bool m_invokesRuntime = false;
    };

    struct TerrainAuthoringAuthorityState
    {
        bool m_runtimeUseAllowed = false;
        bool m_deploymentAllowed = false;
        bool m_publicationAllowed = false;
        bool m_packagingAllowed = false;
        bool m_gameWriteAllowed = false;
        bool m_evidencePromotionAllowed = false;
        bool m_directFoAInstallScanAllowed = false;
        bool m_externalProcessAllowed = false;
        bool m_roadAtlasMutationAllowed = false;
    };

    struct TerrainAuthoringServiceStatus
    {
        AZStd::string m_gateId = TerrainAuthoringGateId;
        AZStd::string m_requiredSchemaId =
            TaintedGrailModdingSDK::TerrainHeightmap::TerrainHeightmapSchemaId;
        AZ::u32 m_requiredSchemaVersion =
            TaintedGrailModdingSDK::TerrainHeightmap::TerrainHeightmapSchemaVersion;
        bool m_visiblePaneRegistered = false;
        bool m_previewProjectionEnabled = false;
        bool m_assetProcessorProjectionEnabled = false;
        TerrainAuthoringAuthorityState m_authority;
    };

    class TerrainAuthoringService
    {
    public:
        virtual ~TerrainAuthoringService() = default;

        virtual TerrainAuthoringServiceStatus GetStatus() const = 0;
        virtual AZStd::vector<TerrainAuthoringCommandDescriptor> GetCommandDescriptors() const = 0;
        virtual bool ValidateCandidateDocument(
            const TaintedGrailModdingSDK::TerrainHeightmap::TerrainHeightmapDocumentV1& document,
            TaintedGrailModdingSDK::TerrainHeightmap::ValidationResult& result,
            AZStd::string* error) const = 0;
    };

    TaintedGrailModdingSDK::ExtensionAPI::ExtensionDeclaration BuildExtensionDeclaration();
    AZStd::vector<TerrainAuthoringCommandDescriptor> BuildCommandDescriptors();
    TerrainAuthoringServiceStatus BuildInitialServiceStatus();
    bool ValidateShellContract(AZStd::string* error = nullptr);
} // namespace TerrainAuthoring
