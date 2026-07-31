/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK::TerrainHeightmap
{
    constexpr const char* TerrainHeightmapSchemaId = "foa.terrain-heightmap";
    constexpr AZ::u32 TerrainHeightmapSchemaVersion = 1;
    constexpr AZ::u32 TerrainHeightmapNominalTileSize = 1024;
    constexpr AZ::u32 TerrainHeightmapMaximumWidth = 32768;
    constexpr AZ::u32 TerrainHeightmapMaximumHeight = 32768;
    constexpr AZ::u64 TerrainHeightmapMaximumTotalSamples = 268435456;
    constexpr AZ::u64 TerrainHeightmapMaximumTileCount = 1024;

    struct ProfileBinding
    {
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_profileFingerprint;
    };

    struct MapIdentity
    {
        AZStd::string m_mapId;
        AZStd::string m_displayName;
        AZStd::vector<AZStd::string> m_publicAliases;
        AZStd::string m_nativeIdentityEvidenceId;
    };

    struct SourceBinding
    {
        AZStd::string m_sourceKind;
        AZStd::string m_sourceContainerSha256;
        AZStd::string m_sourceObjectIdentifier;
        AZStd::string m_sourceSubresourceSha256;
        AZStd::string m_exporterId;
        AZStd::string m_exporterVersion;
        AZStd::string m_configurationFingerprint;
        AZStd::string m_redactedRootToken;
        AZStd::string m_relativeLocator;
    };

    struct Grid
    {
        AZ::u32 m_width = 0;
        AZ::u32 m_height = 0;
        double m_sampleSpacingXMetres = 0.0;
        double m_sampleSpacingYMetres = 0.0;
    };

    struct SampleEncoding
    {
        AZStd::string m_format;
        AZStd::string m_byteOrder;
        AZStd::string m_storageOrder;
        AZ::u32 m_bitsPerSample = 0;
        bool m_unsignedInteger = false;
    };

    struct VerticalMapping
    {
        double m_minHeightMetres = 0.0;
        double m_maxHeightMetres = 0.0;
    };

    struct CoordinateSpace
    {
        AZStd::string m_handedness;
        AZStd::string m_upAxis;
        AZStd::string m_forwardAxis;
        AZStd::string m_rowZeroOrientation;
        AZStd::string m_samplePosition;
        AZStd::vector<double> m_sourceToCanonicalTransform;
    };

    struct Tile
    {
        AZStd::string m_tileId;
        AZ::u32 m_originX = 0;
        AZ::u32 m_originY = 0;
        AZ::u32 m_width = 0;
        AZ::u32 m_height = 0;
        AZStd::string m_relativePath;
        AZ::u64 m_byteSize = 0;
        AZStd::string m_sha256;
    };

    struct Provenance
    {
        AZStd::string m_createdAtUtc;
        AZStd::string m_importerId;
        AZStd::string m_importerVersion;
        AZStd::string m_sourceEvidenceId;
        AZStd::string m_limitations;
    };

    struct Revision
    {
        AZStd::string m_revisionId;
        AZStd::string m_parentDocumentFingerprint;
        AZStd::string m_operationFingerprint;
        AZStd::string m_createdAtUtc;
    };

    struct Authority
    {
        bool m_runtimeUseAllowed = false;
        bool m_deploymentAllowed = false;
        bool m_publicationAllowed = false;
        bool m_packagingAllowed = false;
        bool m_gameWriteAllowed = false;
        bool m_evidencePromotionAllowed = false;
    };

    struct TerrainHeightmapDocumentV1
    {
        AZStd::string m_schema = TerrainHeightmapSchemaId;
        AZ::u32 m_schemaVersion = TerrainHeightmapSchemaVersion;
        AZStd::string m_documentId;
        MapIdentity m_mapIdentity;
        ProfileBinding m_profileBinding;
        SourceBinding m_sourceBinding;
        Grid m_grid;
        SampleEncoding m_sampleEncoding;
        VerticalMapping m_verticalMapping;
        CoordinateSpace m_coordinateSpace;
        AZStd::vector<Tile> m_tiles;
        Provenance m_provenance;
        AZStd::string m_legalState;
        Revision m_revision;
        AZStd::string m_localPayloadState;
        Authority m_authority;
    };

    struct ValidationIssue
    {
        AZStd::string m_locator;
        AZStd::string m_code;
        AZStd::string m_message;
    };

    struct ValidationResult
    {
        bool m_accepted = false;
        AZ::u64 m_totalSamples = 0;
        AZ::u64 m_tileCount = 0;
        AZStd::vector<ValidationIssue> m_issues;
        AZStd::string m_canonicalFingerprint;
        bool m_runtimeUseAllowed = false;
        bool m_deploymentAllowed = false;
        bool m_publicationAllowed = false;
        bool m_packagingAllowed = false;
        bool m_gameWriteAllowed = false;
        bool m_evidencePromotionAllowed = false;
    };

    struct WorkspaceStagingPlan
    {
        AZStd::string m_operationId;
        AZStd::string m_stagingManifestRelativePath;
        AZStd::string m_stagingTileRootRelativePath;
        AZStd::string m_publishedManifestRelativePath;
        AZStd::string m_publishedTileRootRelativePath;
        AZStd::string m_sourceObservationRelativePath;
    };

    struct PackageGuardResult
    {
        bool m_allowed = false;
        AZStd::vector<ValidationIssue> m_issues;
    };

    struct RawHeightmapImportRequest
    {
        AZStd::string m_workspaceRoot;
        AZStd::string m_rawInputPath;
        AZStd::string m_sidecarPath;
        MapIdentity m_mapIdentity;
        ProfileBinding m_profileBinding;
        AZStd::string m_operationId;
        AZStd::string m_createdAtUtc;
        AZStd::string m_importerId = "importer.terrain-heightmap.raw-u16";
        AZStd::string m_importerVersion = "1.0.0";
    };

    struct RawHeightmapImportResult
    {
        TerrainHeightmapDocumentV1 m_document;
        WorkspaceStagingPlan m_stagingPlan;
        AZStd::string m_publishedManifestPath;
        AZStd::vector<AZStd::string> m_publishedTilePaths;
        AZStd::string m_sourceObservationPath;
        AZStd::string m_sourceFingerprint;
        AZStd::string m_sidecarFingerprint;
        AZ::u64 m_sourceByteSize = 0;
        AZ::u64 m_tileCount = 0;
    };

    struct ImageHeightmapImportRequest
    {
        AZStd::string m_workspaceRoot;
        AZStd::string m_imageInputPath;
        MapIdentity m_mapIdentity;
        ProfileBinding m_profileBinding;
        Grid m_gridMetadata;
        VerticalMapping m_verticalMapping;
        CoordinateSpace m_coordinateSpace;
        AZStd::string m_operationId;
        AZStd::string m_createdAtUtc;
        AZStd::string m_importerId = "importer.terrain-heightmap.image-u16";
        AZStd::string m_importerVersion = "1.0.0";
    };

    struct ImageHeightmapImportResult
    {
        TerrainHeightmapDocumentV1 m_document;
        WorkspaceStagingPlan m_stagingPlan;
        AZStd::string m_publishedManifestPath;
        AZStd::vector<AZStd::string> m_publishedTilePaths;
        AZStd::string m_sourceObservationPath;
        AZStd::string m_sourceFingerprint;
        AZStd::string m_metadataFingerprint;
        AZ::u64 m_sourceByteSize = 0;
        AZ::u64 m_tileCount = 0;
    };

    ValidationResult ValidateDocument(const TerrainHeightmapDocumentV1& document);
    AZStd::string BuildCanonicalDocumentJson(const TerrainHeightmapDocumentV1& document);
    AZStd::string CalculateDocumentFingerprint(const TerrainHeightmapDocumentV1& document);

    bool IsSafeWorkspaceRelativePath(const AZStd::string& relativePath);

    ValidationResult BuildWorkspaceStagingPlan(
        const TerrainHeightmapDocumentV1& document,
        const AZStd::string& operationId,
        WorkspaceStagingPlan& plan);

    PackageGuardResult ValidateTerrainPackagePath(const AZStd::string& relativePath);

    AZ::Outcome<RawHeightmapImportResult, AZStd::string> ImportRawHeightmapToWorkspace(
        const RawHeightmapImportRequest& request);

    AZ::Outcome<ImageHeightmapImportResult, AZStd::string> ImportImageHeightmapToWorkspace(
        const ImageHeightmapImportRequest& request);
} // namespace TaintedGrailModdingSDK::TerrainHeightmap
