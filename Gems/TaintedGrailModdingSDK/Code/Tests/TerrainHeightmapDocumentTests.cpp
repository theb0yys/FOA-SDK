/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "TerrainHeightmapDocument.h"

#include <AzTest/AzTest.h>

#include <AzCore/std/algorithm.h>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <limits>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string Sha(char fill)
        {
            return "sha256:" + AZStd::string(64, fill);
        }

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        bool WriteFile(const QString& path, const QByteArray& bytes)
        {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && file.write(bytes) == bytes.size();
        }

        QByteArray SidecarJson(
            AZ::u32 width,
            AZ::u32 height,
            const char* byteOrder)
        {
            const AZStd::string json = AZStd::string::format(
                R"JSON({
  "schema": "foa.raw-u16-heightmap-sidecar",
  "schema_version": 1,
  "width": %u,
  "height": %u,
  "byte_order": "%s",
  "sample_spacing_x_metres": 1.0,
  "sample_spacing_y_metres": 2.0,
  "min_height_metres": -10.0,
  "max_height_metres": 50.0,
  "handedness": "right-handed",
  "up_axis": "z",
  "forward_axis": "y",
  "row_zero_orientation": "north",
  "sample_position": "cell-center",
  "source_to_canonical_transform": [
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
  ]
})JSON",
                width,
                height,
                byteOrder);
            return QByteArray(json.data(), static_cast<int>(json.size()));
        }

        TerrainHeightmap::RawHeightmapImportRequest MakeImportRequest(
            const QTemporaryDir& temporary,
            const QString& rawPath,
            const QString& sidecarPath,
            const char* operationId = "terrain-import.synthetic")
        {
            TerrainHeightmap::RawHeightmapImportRequest request;
            request.m_workspaceRoot = ToAzString(temporary.path());
            request.m_rawInputPath = ToAzString(rawPath);
            request.m_sidecarPath = ToAzString(sidecarPath);
            request.m_mapIdentity.m_mapId = "terrain-map.synthetic-import";
            request.m_mapIdentity.m_displayName = "Synthetic Import Terrain";
            request.m_mapIdentity.m_publicAliases = { "Horns of the South" };
            request.m_profileBinding.m_profileId = "profile.foa.synthetic";
            request.m_profileBinding.m_gameVersion = "1.0.0";
            request.m_profileBinding.m_branch = "mono";
            request.m_profileBinding.m_runtimeTarget = "Mono";
            request.m_profileBinding.m_profileFingerprint = Sha('a');
            request.m_operationId = operationId;
            request.m_createdAtUtc = "2026-07-31T04:20:00Z";
            return request;
        }

        QByteArray ReadAll(const AZStd::string& path)
        {
            QFile file(QString::fromUtf8(path.c_str()));
            if (!file.open(QIODevice::ReadOnly))
            {
                return {};
            }
            return file.readAll();
        }

        TerrainHeightmap::Tile MakeTile(
            const char* tileId,
            AZ::u32 originX,
            AZ::u32 originY,
            AZ::u32 width,
            AZ::u32 height,
            const char* relativePath,
            char hashFill)
        {
            TerrainHeightmap::Tile tile;
            tile.m_tileId = tileId;
            tile.m_originX = originX;
            tile.m_originY = originY;
            tile.m_width = width;
            tile.m_height = height;
            tile.m_relativePath = relativePath;
            tile.m_byteSize = static_cast<AZ::u64>(width) * height * 2u;
            tile.m_sha256 = Sha(hashFill);
            return tile;
        }

        TerrainHeightmap::TerrainHeightmapDocumentV1 MakeDocument()
        {
            TerrainHeightmap::TerrainHeightmapDocumentV1 document;
            document.m_documentId = "terrain-document.synthetic-horns";
            document.m_mapIdentity.m_mapId = "terrain-map.horns-of-the-south";
            document.m_mapIdentity.m_displayName = "Synthetic Horns Terrain";
            document.m_mapIdentity.m_publicAliases = { "Horns of the South" };

            document.m_profileBinding.m_profileId = "profile.foa.synthetic";
            document.m_profileBinding.m_gameVersion = "1.0.0";
            document.m_profileBinding.m_branch = "mono";
            document.m_profileBinding.m_runtimeTarget = "Mono";
            document.m_profileBinding.m_profileFingerprint = Sha('a');

            document.m_sourceBinding.m_sourceKind = "user-exported-raw-u16-le";
            document.m_sourceBinding.m_sourceContainerSha256 = Sha('b');
            document.m_sourceBinding.m_sourceObjectIdentifier = "synthetic-height.raw";
            document.m_sourceBinding.m_exporterId = "exporter.synthetic-heightmap";
            document.m_sourceBinding.m_exporterVersion = "1.0.0";
            document.m_sourceBinding.m_configurationFingerprint = Sha('c');
            document.m_sourceBinding.m_redactedRootToken = "source-root.synthetic";
            document.m_sourceBinding.m_relativeLocator = "Sources/Terrain/synthetic-height.raw";

            document.m_grid.m_width = 4;
            document.m_grid.m_height = 4;
            document.m_grid.m_sampleSpacingXMetres = 1.0;
            document.m_grid.m_sampleSpacingYMetres = 1.0;

            document.m_sampleEncoding.m_format = "u16";
            document.m_sampleEncoding.m_byteOrder = "little-endian";
            document.m_sampleEncoding.m_storageOrder = "row-major";
            document.m_sampleEncoding.m_bitsPerSample = 16;
            document.m_sampleEncoding.m_unsignedInteger = true;

            document.m_verticalMapping.m_minHeightMetres = -128.0;
            document.m_verticalMapping.m_maxHeightMetres = 512.0;

            document.m_coordinateSpace.m_handedness = "right-handed";
            document.m_coordinateSpace.m_upAxis = "z";
            document.m_coordinateSpace.m_forwardAxis = "y";
            document.m_coordinateSpace.m_rowZeroOrientation = "north";
            document.m_coordinateSpace.m_samplePosition = "cell-center";
            document.m_coordinateSpace.m_sourceToCanonicalTransform = {
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0,
            };

            document.m_tiles = {
                MakeTile(
                    "terrain-tile.synthetic.0-0",
                    0,
                    0,
                    2,
                    2,
                    "Tiles/00-00.terrain.u16le",
                    'd'),
                MakeTile(
                    "terrain-tile.synthetic.2-0",
                    2,
                    0,
                    2,
                    2,
                    "Tiles/02-00.terrain.u16le",
                    'e'),
                MakeTile(
                    "terrain-tile.synthetic.0-2",
                    0,
                    2,
                    2,
                    2,
                    "Tiles/00-02.terrain.u16le",
                    'f'),
                MakeTile(
                    "terrain-tile.synthetic.2-2",
                    2,
                    2,
                    2,
                    2,
                    "Tiles/02-02.terrain.u16le",
                    '1'),
            };

            document.m_provenance.m_createdAtUtc = "2026-07-31T03:10:00Z";
            document.m_provenance.m_importerId = "importer.terrain-heightmap.synthetic";
            document.m_provenance.m_importerVersion = "1.0.0";
            document.m_provenance.m_sourceEvidenceId = "evidence.terrain.synthetic-source";
            document.m_provenance.m_limitations = "Synthetic fixture only.";
            document.m_legalState = "user-exported-local-only";
            document.m_revision.m_revisionId = "terrain-revision.synthetic.1";
            document.m_revision.m_operationFingerprint = Sha('2');
            document.m_revision.m_createdAtUtc = "2026-07-31T03:10:00Z";
            document.m_localPayloadState = "workspace-local-derived";
            return document;
        }

        bool HasIssue(
            const TerrainHeightmap::ValidationResult& result,
            const AZStd::string& code)
        {
            return AZStd::any_of(
                result.m_issues.begin(),
                result.m_issues.end(),
                [&code](const TerrainHeightmap::ValidationIssue& issue)
                {
                    return issue.m_code == code;
                });
        }
    } // namespace

    TEST(TerrainHeightmapDocumentTests, SyntheticDocumentIsAcceptedAsLocalOnlyAndInert)
    {
        const auto result = TerrainHeightmap::ValidateDocument(MakeDocument());
        EXPECT_TRUE(result.m_accepted);
        EXPECT_EQ(result.m_totalSamples, 16);
        EXPECT_EQ(result.m_tileCount, 4);
        EXPECT_FALSE(result.m_runtimeUseAllowed);
        EXPECT_FALSE(result.m_deploymentAllowed);
        EXPECT_FALSE(result.m_publicationAllowed);
        EXPECT_FALSE(result.m_packagingAllowed);
        EXPECT_FALSE(result.m_gameWriteAllowed);
        EXPECT_FALSE(result.m_evidencePromotionAllowed);
        EXPECT_FALSE(result.m_canonicalFingerprint.empty());
    }

    TEST(TerrainHeightmapDocumentTests, UnsupportedSchemaVersionFailsClosed)
    {
        auto document = MakeDocument();
        document.m_schemaVersion = 2;
        const auto result = TerrainHeightmap::ValidateDocument(document);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "schema.unsupported-version"));
    }

    TEST(TerrainHeightmapDocumentTests, DisplayAliasCannotBecomeNativeIdentity)
    {
        auto document = MakeDocument();
        document.m_mapIdentity.m_mapId = "Horns of the South";
        const auto displayNameResult = TerrainHeightmap::ValidateDocument(document);
        EXPECT_FALSE(displayNameResult.m_accepted);
        EXPECT_TRUE(HasIssue(displayNameResult, "identity.invalid"));

        document = MakeDocument();
        document.m_mapIdentity.m_nativeIdentityEvidenceId = "evidence.native-map";
        const auto nativeClaimResult = TerrainHeightmap::ValidateDocument(document);
        EXPECT_FALSE(nativeClaimResult.m_accepted);
        EXPECT_TRUE(HasIssue(nativeClaimResult, "identity.invalid"));
    }

    TEST(TerrainHeightmapDocumentTests, UnknownInputOrEncodingEnumsFailClosed)
    {
        auto source = MakeDocument();
        source.m_sourceBinding.m_sourceKind = "unity-assets";
        EXPECT_FALSE(TerrainHeightmap::ValidateDocument(source).m_accepted);

        auto encoding = MakeDocument();
        encoding.m_sampleEncoding.m_byteOrder = "big-endian";
        const auto result = TerrainHeightmap::ValidateDocument(encoding);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "encoding.invalid"));
    }

    TEST(TerrainHeightmapDocumentTests, DimensionsAndOverflowAreBounded)
    {
        auto zero = MakeDocument();
        zero.m_grid.m_width = 0;
        EXPECT_FALSE(TerrainHeightmap::ValidateDocument(zero).m_accepted);

        auto huge = MakeDocument();
        huge.m_grid.m_width = 32768;
        huge.m_grid.m_height = 32768;
        const auto hugeResult = TerrainHeightmap::ValidateDocument(huge);
        EXPECT_FALSE(hugeResult.m_accepted);
        EXPECT_TRUE(HasIssue(hugeResult, "grid.invalid"));
    }

    TEST(TerrainHeightmapDocumentTests, NonFiniteOrInvertedVerticalRangeFailsClosed)
    {
        auto inverted = MakeDocument();
        inverted.m_verticalMapping.m_maxHeightMetres =
            inverted.m_verticalMapping.m_minHeightMetres;
        const auto invertedResult = TerrainHeightmap::ValidateDocument(inverted);
        EXPECT_FALSE(invertedResult.m_accepted);
        EXPECT_TRUE(HasIssue(invertedResult, "vertical.invalid"));

        auto nonFinite = MakeDocument();
        nonFinite.m_verticalMapping.m_minHeightMetres =
            std::numeric_limits<double>::infinity();
        EXPECT_FALSE(TerrainHeightmap::ValidateDocument(nonFinite).m_accepted);
    }

    TEST(TerrainHeightmapDocumentTests, TileCoverageOverlapOrderAndMetadataFailClosed)
    {
        auto gap = MakeDocument();
        gap.m_tiles.pop_back();
        const auto gapResult = TerrainHeightmap::ValidateDocument(gap);
        EXPECT_FALSE(gapResult.m_accepted);
        EXPECT_TRUE(HasIssue(gapResult, "tile.coverage"));

        auto overlap = MakeDocument();
        overlap.m_tiles[1].m_originX = 1;
        const auto overlapResult = TerrainHeightmap::ValidateDocument(overlap);
        EXPECT_FALSE(overlapResult.m_accepted);
        EXPECT_TRUE(HasIssue(overlapResult, "tile.overlap"));

        auto unordered = MakeDocument();
        AZStd::reverse(unordered.m_tiles.begin(), unordered.m_tiles.end());
        const auto unorderedResult = TerrainHeightmap::ValidateDocument(unordered);
        EXPECT_FALSE(unorderedResult.m_accepted);
        EXPECT_TRUE(HasIssue(unorderedResult, "tile.order"));

        auto badBytes = MakeDocument();
        badBytes.m_tiles[0].m_byteSize += 1;
        const auto badBytesResult = TerrainHeightmap::ValidateDocument(badBytes);
        EXPECT_FALSE(badBytesResult.m_accepted);
        EXPECT_TRUE(HasIssue(badBytesResult, "tile.invalid"));
    }

    TEST(TerrainHeightmapDocumentTests, TilePayloadPathsRejectTraversalPrivatePathsAndCaseCollisions)
    {
        auto traversal = MakeDocument();
        traversal.m_tiles[0].m_relativePath = "../Tiles/00-00.terrain.u16le";
        EXPECT_FALSE(TerrainHeightmap::ValidateDocument(traversal).m_accepted);

        auto absolute = MakeDocument();
        absolute.m_tiles[0].m_relativePath = "Z:/blocked/00-00.terrain.u16le";
        EXPECT_FALSE(TerrainHeightmap::ValidateDocument(absolute).m_accepted);

        auto unc = MakeDocument();
        unc.m_tiles[0].m_relativePath = "\\\\server\\share\\00-00.terrain.u16le";
        EXPECT_FALSE(TerrainHeightmap::ValidateDocument(unc).m_accepted);

        auto uri = MakeDocument();
        uri.m_tiles[0].m_relativePath = "file:///private/00-00.terrain.u16le";
        EXPECT_FALSE(TerrainHeightmap::ValidateDocument(uri).m_accepted);

        auto ads = MakeDocument();
        ads.m_tiles[0].m_relativePath = "Tiles/00-00.terrain.u16le:ads";
        EXPECT_FALSE(TerrainHeightmap::ValidateDocument(ads).m_accepted);

        auto reserved = MakeDocument();
        reserved.m_tiles[0].m_relativePath = "Tiles/CON.terrain.u16le";
        EXPECT_FALSE(TerrainHeightmap::ValidateDocument(reserved).m_accepted);

        auto collision = MakeDocument();
        collision.m_tiles[1].m_relativePath = "tiles/00-00.TERRAIN.U16LE";
        const auto collisionResult = TerrainHeightmap::ValidateDocument(collision);
        EXPECT_FALSE(collisionResult.m_accepted);
        EXPECT_TRUE(HasIssue(collisionResult, "tile.path-case-collision"));
    }

    TEST(TerrainHeightmapDocumentTests, SourceBindingRejectsAbsolutePrivatePaths)
    {
        auto document = MakeDocument();
        document.m_sourceBinding.m_relativeLocator =
            "Z:/blocked/exported-height.raw";
        const auto result = TerrainHeightmap::ValidateDocument(document);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "source.invalid"));
    }

    TEST(TerrainHeightmapDocumentTests, AuthorityFlagsCannotBePromotedByValidation)
    {
        auto document = MakeDocument();
        document.m_authority.m_runtimeUseAllowed = true;
        document.m_authority.m_deploymentAllowed = true;
        document.m_authority.m_packagingAllowed = true;
        document.m_authority.m_gameWriteAllowed = true;
        const auto result = TerrainHeightmap::ValidateDocument(document);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "authority.forbidden"));
        EXPECT_TRUE(result.m_runtimeUseAllowed);
        EXPECT_TRUE(result.m_deploymentAllowed);
        EXPECT_TRUE(result.m_packagingAllowed);
        EXPECT_TRUE(result.m_gameWriteAllowed);
    }

    TEST(TerrainHeightmapDocumentTests, CanonicalDocumentFingerprintIgnoresTileInputOrdering)
    {
        auto first = MakeDocument();
        auto second = first;
        AZStd::reverse(second.m_tiles.begin(), second.m_tiles.end());
        EXPECT_EQ(
            TerrainHeightmap::CalculateDocumentFingerprint(first),
            TerrainHeightmap::CalculateDocumentFingerprint(second));
    }

    TEST(TerrainHeightmapDocumentTests, CanonicalDocumentChangesWithTileFingerprint)
    {
        auto first = MakeDocument();
        auto second = first;
        second.m_tiles[0].m_sha256 = Sha('3');
        EXPECT_NE(
            TerrainHeightmap::CalculateDocumentFingerprint(first),
            TerrainHeightmap::CalculateDocumentFingerprint(second));
    }

    TEST(TerrainHeightmapDocumentTests, WorkspaceStagingPlanUsesContainedRelativePaths)
    {
        TerrainHeightmap::WorkspaceStagingPlan plan;
        const auto result = TerrainHeightmap::BuildWorkspaceStagingPlan(
            MakeDocument(),
            "terrain-import.synthetic",
            plan);
        EXPECT_TRUE(result.m_accepted);
        EXPECT_TRUE(TerrainHeightmap::IsSafeWorkspaceRelativePath(
            plan.m_stagingManifestRelativePath));
        EXPECT_TRUE(TerrainHeightmap::IsSafeWorkspaceRelativePath(
            plan.m_publishedManifestRelativePath));
        EXPECT_NE(plan.m_stagingManifestRelativePath.find("Staging/Terrain/"), AZStd::string::npos);
        EXPECT_NE(plan.m_publishedManifestRelativePath.find("Derived/Terrain/"), AZStd::string::npos);
        EXPECT_NE(plan.m_sourceObservationRelativePath.find("SourceObservations/Terrain/"), AZStd::string::npos);
    }

    TEST(TerrainHeightmapDocumentTests, WorkspaceStagingPlanRejectsUnsafeOperationIds)
    {
        TerrainHeightmap::WorkspaceStagingPlan plan;
        const auto result = TerrainHeightmap::BuildWorkspaceStagingPlan(
            MakeDocument(),
            "../terrain-import",
            plan);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "staging.operation-id"));
        EXPECT_TRUE(plan.m_stagingManifestRelativePath.empty());
    }

    TEST(TerrainHeightmapDocumentTests, PackageGuardExcludesLocalTerrainArtifacts)
    {
        EXPECT_FALSE(TerrainHeightmap::ValidateTerrainPackagePath(
            "Derived/Terrain/terrain-map.horns/Revisions/terrain.tgheightmap.json").m_allowed);
        EXPECT_FALSE(TerrainHeightmap::ValidateTerrainPackagePath(
            "Staging/Terrain/import/Tiles/00.terrain.u16le").m_allowed);
        EXPECT_FALSE(TerrainHeightmap::ValidateTerrainPackagePath(
            "SourceObservations/Terrain/import/source-observation.json").m_allowed);
        EXPECT_FALSE(TerrainHeightmap::ValidateTerrainPackagePath(
            "Preview/synthetic._gsi").m_allowed);
    }

    TEST(TerrainHeightmapDocumentTests, PackageGuardAllowsUnrelatedSafePackagePaths)
    {
        const auto result = TerrainHeightmap::ValidateTerrainPackagePath(
            "Packs/synthetic/content.tgpack.json");
        EXPECT_TRUE(result.m_allowed);
        EXPECT_TRUE(result.m_issues.empty());
    }

    TEST(TerrainHeightmapDocumentTests, LittleEndianRawSidecarImportPublishesManifestAndTile)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("synthetic.raw");
        const QString sidecarPath = QDir(temporary.path()).filePath("synthetic.raw.json");
        const QByteArray rawBytes(
            "\x01\x00\x02\x00\x03\x00\x04\x00",
            8);
        ASSERT_TRUE(WriteFile(rawPath, rawBytes));
        ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(2, 2, "little-endian")));

        auto outcome = TerrainHeightmap::ImportRawHeightmapToWorkspace(
            MakeImportRequest(temporary, rawPath, sidecarPath));
        ASSERT_TRUE(outcome.IsSuccess()) << outcome.GetError().c_str();
        const auto& result = outcome.GetValue();

        EXPECT_TRUE(TerrainHeightmap::ValidateDocument(result.m_document).m_accepted);
        ASSERT_EQ(result.m_publishedTilePaths.size(), 1);
        EXPECT_TRUE(QFileInfo(QString::fromUtf8(
            result.m_publishedManifestPath.c_str())).exists());
        EXPECT_EQ(ReadAll(result.m_publishedTilePaths.front()), rawBytes);

        const QByteArray manifestBytes = ReadAll(result.m_publishedManifestPath);
        EXPECT_TRUE(manifestBytes.contains("\"schema\":\"foa.terrain-heightmap\""));
        EXPECT_FALSE(manifestBytes.contains(rawPath.toUtf8()));
        EXPECT_FALSE(manifestBytes.contains(temporary.path().toUtf8()));
        EXPECT_TRUE(QFileInfo(QString::fromUtf8(
            result.m_sourceObservationPath.c_str())).exists());
    }

    TEST(TerrainHeightmapDocumentTests, BigEndianRawImportConvertsTilesToLittleEndian)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("synthetic.u16");
        const QString sidecarPath = QDir(temporary.path()).filePath("synthetic.u16.json");
        const QByteArray bigEndianBytes(
            "\x12\x34\xAB\xCD",
            4);
        const QByteArray littleEndianBytes(
            "\x34\x12\xCD\xAB",
            4);
        ASSERT_TRUE(WriteFile(rawPath, bigEndianBytes));
        ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(2, 1, "big-endian")));

        auto outcome = TerrainHeightmap::ImportRawHeightmapToWorkspace(
            MakeImportRequest(temporary, rawPath, sidecarPath, "terrain-import.big-endian"));
        ASSERT_TRUE(outcome.IsSuccess()) << outcome.GetError().c_str();
        const auto& result = outcome.GetValue();

        EXPECT_EQ(result.m_document.m_sourceBinding.m_sourceKind, "user-exported-raw-u16-be");
        ASSERT_EQ(result.m_publishedTilePaths.size(), 1);
        EXPECT_EQ(ReadAll(result.m_publishedTilePaths.front()), littleEndianBytes);
        EXPECT_TRUE(TerrainHeightmap::ValidateDocument(result.m_document).m_accepted);
    }

    TEST(TerrainHeightmapDocumentTests, RawImportSplitsEdgeTilesDeterministically)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("wide.r16");
        const QString sidecarPath = QDir(temporary.path()).filePath("wide.r16.json");
        QByteArray rawBytes;
        rawBytes.resize(1025 * 2);
        for (int index = 0; index < rawBytes.size(); ++index)
        {
            rawBytes[index] = static_cast<char>(index & 0xff);
        }
        ASSERT_TRUE(WriteFile(rawPath, rawBytes));
        ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(1025, 1, "little-endian")));

        auto outcome = TerrainHeightmap::ImportRawHeightmapToWorkspace(
            MakeImportRequest(temporary, rawPath, sidecarPath, "terrain-import.edge-tile"));
        ASSERT_TRUE(outcome.IsSuccess()) << outcome.GetError().c_str();
        const auto& document = outcome.GetValue().m_document;
        ASSERT_EQ(document.m_tiles.size(), 2);
        EXPECT_EQ(document.m_tiles[0].m_width, 1024);
        EXPECT_EQ(document.m_tiles[1].m_originX, 1024);
        EXPECT_EQ(document.m_tiles[1].m_width, 1);
        EXPECT_TRUE(TerrainHeightmap::ValidateDocument(document).m_accepted);
    }

    TEST(TerrainHeightmapDocumentTests, RawImportRequiresValidSidecarByteOrder)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("synthetic.raw");
        const QString sidecarPath = QDir(temporary.path()).filePath("synthetic.raw.json");
        ASSERT_TRUE(WriteFile(rawPath, QByteArray("\x00\x00", 2)));
        ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(1, 1, "native")));

        const auto outcome = TerrainHeightmap::ImportRawHeightmapToWorkspace(
            MakeImportRequest(temporary, rawPath, sidecarPath, "terrain-import.bad-sidecar"));
        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_NE(outcome.GetError().find("byte_order"), AZStd::string::npos);
    }

    TEST(TerrainHeightmapDocumentTests, RawImportRequiresExistingSidecarFile)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("synthetic.raw");
        const QString sidecarPath = QDir(temporary.path()).filePath("missing.raw.json");
        ASSERT_TRUE(WriteFile(rawPath, QByteArray("\x00\x00", 2)));

        const auto outcome = TerrainHeightmap::ImportRawHeightmapToWorkspace(
            MakeImportRequest(temporary, rawPath, sidecarPath, "terrain-import.missing-sidecar"));
        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_NE(outcome.GetError().find("sidecar"), AZStd::string::npos);
        EXPECT_FALSE(QFileInfo(QDir(temporary.path()).filePath("Derived/Terrain")).exists());
    }

    TEST(TerrainHeightmapDocumentTests, RawImportRejectsIncorrectSourceByteSize)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("truncated.raw");
        const QString sidecarPath = QDir(temporary.path()).filePath("truncated.raw.json");
        ASSERT_TRUE(WriteFile(rawPath, QByteArray("\x00\x00", 2)));
        ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(2, 2, "little-endian")));

        const auto outcome = TerrainHeightmap::ImportRawHeightmapToWorkspace(
            MakeImportRequest(temporary, rawPath, sidecarPath, "terrain-import.truncated"));
        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_NE(outcome.GetError().find("byte size"), AZStd::string::npos);
        EXPECT_FALSE(QFileInfo(QDir(temporary.path()).filePath("Derived/Terrain")).exists());
    }

    TEST(TerrainHeightmapDocumentTests, RawImportRejectsProtectedAndUnsupportedInputSuffixes)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("payload.assets");
        const QString sidecarPath = QDir(temporary.path()).filePath("payload.assets.json");
        ASSERT_TRUE(WriteFile(rawPath, QByteArray("\x00\x00", 2)));
        ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(1, 1, "little-endian")));

        const auto outcome = TerrainHeightmap::ImportRawHeightmapToWorkspace(
            MakeImportRequest(temporary, rawPath, sidecarPath, "terrain-import.protected"));
        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_NE(outcome.GetError().find("prohibited"), AZStd::string::npos);
    }

    TEST(TerrainHeightmapDocumentTests, RawImportWillNotOverwritePublishedRevision)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("synthetic.raw");
        const QString sidecarPath = QDir(temporary.path()).filePath("synthetic.raw.json");
        ASSERT_TRUE(WriteFile(rawPath, QByteArray("\x00\x00", 2)));
        ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(1, 1, "little-endian")));
        const auto request = MakeImportRequest(
            temporary,
            rawPath,
            sidecarPath,
            "terrain-import.no-overwrite");

        ASSERT_TRUE(TerrainHeightmap::ImportRawHeightmapToWorkspace(request).IsSuccess());
        const auto second = TerrainHeightmap::ImportRawHeightmapToWorkspace(request);
        EXPECT_FALSE(second.IsSuccess());
        EXPECT_NE(second.GetError().find("already exists"), AZStd::string::npos);
    }
} // namespace TaintedGrailModdingSDK
