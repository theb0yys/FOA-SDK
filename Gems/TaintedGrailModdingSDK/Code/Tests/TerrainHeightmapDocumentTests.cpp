/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "TerrainHeightmapDocument.h"
#include "TerrainImportHost.h"
#include "TerrainNativeHandoff.h"
#include "TerrainCampaignExportHost.h"
#include <thread>
#include <chrono>
#include <QJsonArray>

#include <AzTest/AzTest.h>

#include <AzCore/std/algorithm.h>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDirIterator>
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

        AZStd::string IndexedSha(AZ::u32 index)
        {
            constexpr char Hex[] = "0123456789abcdef";
            AZStd::string hash = "sha256:";
            for (AZ::u32 offset = 0; offset < 64; ++offset)
            {
                hash.push_back(Hex[(index + offset) % 16]);
            }
            return hash;
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

        TerrainHeightmap::ImageHeightmapImportRequest MakeImageImportRequest(
            const QTemporaryDir& temporary,
            const QString& imagePath,
            const char* operationId = "terrain-import.synthetic-image")
        {
            TerrainHeightmap::ImageHeightmapImportRequest request;
            request.m_workspaceRoot = ToAzString(temporary.path());
            request.m_imageInputPath = ToAzString(imagePath);
            request.m_mapIdentity.m_mapId = "terrain-map.synthetic-image-import";
            request.m_mapIdentity.m_displayName = "Synthetic Image Terrain";
            request.m_mapIdentity.m_publicAliases = { "Horns of the South" };
            request.m_profileBinding.m_profileId = "profile.foa.synthetic";
            request.m_profileBinding.m_gameVersion = "1.0.0";
            request.m_profileBinding.m_branch = "mono";
            request.m_profileBinding.m_runtimeTarget = "Mono";
            request.m_profileBinding.m_profileFingerprint = Sha('a');
            request.m_gridMetadata.m_sampleSpacingXMetres = 1.0;
            request.m_gridMetadata.m_sampleSpacingYMetres = 2.0;
            request.m_verticalMapping.m_minHeightMetres = -10.0;
            request.m_verticalMapping.m_maxHeightMetres = 50.0;
            request.m_coordinateSpace.m_handedness = "right-handed";
            request.m_coordinateSpace.m_upAxis = "z";
            request.m_coordinateSpace.m_forwardAxis = "y";
            request.m_coordinateSpace.m_rowZeroOrientation = "north";
            request.m_coordinateSpace.m_samplePosition = "cell-center";
            request.m_coordinateSpace.m_sourceToCanonicalTransform = {
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0,
            };
            request.m_operationId = operationId;
            request.m_createdAtUtc = "2026-07-31T04:25:00Z";
            return request;
        }

        bool WriteGrayscale16Image(
            const QString& path,
            const QByteArray& format,
            AZ::u32 width,
            AZ::u32 height,
            const AZStd::vector<quint16>& samples)
        {
            if (samples.size() != static_cast<size_t>(width) * height)
            {
                return false;
            }
            QDir().mkpath(QFileInfo(path).absolutePath());
            QImage image(
                static_cast<int>(width),
                static_cast<int>(height),
                QImage::Format_Grayscale16);
            for (AZ::u32 y = 0; y < height; ++y)
            {
                quint16* row = reinterpret_cast<quint16*>(image.scanLine(y));
                for (AZ::u32 x = 0; x < width; ++x)
                {
                    row[x] = samples[(static_cast<size_t>(y) * width) + x];
                }
            }
            QImageWriter writer(path, format);
            return writer.write(image);
        }

        bool WriteGrayscale8Image(
            const QString& path,
            const QByteArray& format)
        {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QImage image(2, 2, QImage::Format_Grayscale8);
            image.fill(128);
            QImageWriter writer(path, format);
            return writer.write(image);
        }

        QByteArray LittleEndianSamples(const AZStd::vector<quint16>& samples)
        {
            QByteArray bytes;
            bytes.resize(static_cast<int>(samples.size() * 2u));
            char* output = bytes.data();
            for (size_t index = 0; index < samples.size(); ++index)
            {
                output[index * 2u] = static_cast<char>(samples[index] & 0xffu);
                output[(index * 2u) + 1u] =
                    static_cast<char>((samples[index] >> 8u) & 0xffu);
            }
            return bytes;
        }

        void AppendLe16(QByteArray& bytes, quint16 value)
        {
            bytes.append(static_cast<char>(value & 0xffu));
            bytes.append(static_cast<char>((value >> 8u) & 0xffu));
        }

        void AppendLe32(QByteArray& bytes, AZ::u32 value)
        {
            bytes.append(static_cast<char>(value & 0xffu));
            bytes.append(static_cast<char>((value >> 8u) & 0xffu));
            bytes.append(static_cast<char>((value >> 16u) & 0xffu));
            bytes.append(static_cast<char>((value >> 24u) & 0xffu));
        }

        void AppendTiffEntry(
            QByteArray& bytes,
            quint16 tag,
            quint16 type,
            AZ::u32 count,
            AZ::u32 value)
        {
            AppendLe16(bytes, tag);
            AppendLe16(bytes, type);
            AppendLe32(bytes, count);
            AppendLe32(bytes, value);
        }

        QByteArray Tiff16ImageBytes(
            AZ::u32 width,
            AZ::u32 height,
            const AZStd::vector<quint16>& samples)
        {
            constexpr quint16 ShortType = 3;
            constexpr quint16 LongType = 4;
            constexpr quint16 EntryCount = 10;
            constexpr AZ::u32 IfdOffset = 8;
            constexpr AZ::u32 ImageOffset = IfdOffset + 2u + (EntryCount * 12u) + 4u;
            const AZ::u32 sampleBytes = static_cast<AZ::u32>(samples.size() * 2u);

            QByteArray bytes;
            bytes.append("II", 2);
            AppendLe16(bytes, 42);
            AppendLe32(bytes, IfdOffset);
            AppendLe16(bytes, EntryCount);
            AppendTiffEntry(bytes, 256, LongType, 1, width);
            AppendTiffEntry(bytes, 257, LongType, 1, height);
            AppendTiffEntry(bytes, 258, ShortType, 1, 16);
            AppendTiffEntry(bytes, 259, ShortType, 1, 1);
            AppendTiffEntry(bytes, 262, ShortType, 1, 1);
            AppendTiffEntry(bytes, 273, LongType, 1, ImageOffset);
            AppendTiffEntry(bytes, 277, ShortType, 1, 1);
            AppendTiffEntry(bytes, 278, LongType, 1, height);
            AppendTiffEntry(bytes, 279, LongType, 1, sampleBytes);
            AppendTiffEntry(bytes, 339, ShortType, 1, 1);
            AppendLe32(bytes, 0);
            bytes.append(LittleEndianSamples(samples));
            return bytes;
        }

        QByteArray OversizedPngHeader()
        {
            return QByteArray(
                "\x89PNG\r\n\x1A\n"
                "\x00\x00\x00\x0D"
                "IHDR"
                "\x00\x00\x80\x01"
                "\x00\x00\x00\x01"
                "\x10\x00\x00\x00\x00"
                "\x1D\x0F\x72\x89"
                "\x00\x00\x00\x00"
                "IEND"
                "\xAE\x42\x60\x82",
                45);
        }

        QByteArray HugeMetadataPngHeader()
        {
            QByteArray bytes(
                "\x89PNG\r\n\x1A\n"
                "\x00\x00\x00\x0D"
                "IHDR"
                "\x00\x00\x00\x01"
                "\x00\x00\x00\x01"
                "\x10\x00\x00\x00\x00"
                "\x1D\x0F\x72\x89"
                "\x00\x00\x00\x00"
                "IEND"
                "\xAE\x42\x60\x82",
                45);
            bytes.append(QByteArray(17 * 1024 * 1024, 'm'));
            return bytes;
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

        TerrainHeightmap::TerrainHeightmapDocumentV1 MakeLargeBoundDocument(
            AZ::u32 width = 16385,
            AZ::u32 height = 16385)
        {
            auto document = MakeDocument();
            document.m_documentId = "terrain-document.synthetic-wa-th-large-bound";
            document.m_mapIdentity.m_mapId = "terrain-map.synthetic-wa-th-large-bound";
            document.m_mapIdentity.m_displayName = "Synthetic WA-TH Large Bound Terrain";
            document.m_sourceBinding.m_sourceObjectIdentifier = "synthetic-wa-th-large-bound.raw";
            document.m_sourceBinding.m_relativeLocator = "Sources/Terrain/synthetic-wa-th-large-bound.raw";
            document.m_grid.m_width = width;
            document.m_grid.m_height = height;
            document.m_tiles.clear();

            AZ::u32 tileIndex = 0;
            for (AZ::u32 originY = 0; originY < height;
                 originY += TerrainHeightmap::TerrainHeightmapNominalTileSize)
            {
                const AZ::u32 tileHeight = AZStd::min(
                    TerrainHeightmap::TerrainHeightmapNominalTileSize,
                    height - originY);
                for (AZ::u32 originX = 0; originX < width;
                     originX += TerrainHeightmap::TerrainHeightmapNominalTileSize)
                {
                    const AZ::u32 tileWidth = AZStd::min(
                        TerrainHeightmap::TerrainHeightmapNominalTileSize,
                        width - originX);
                    TerrainHeightmap::Tile tile;
                    tile.m_tileId = AZStd::string::format(
                        "terrain-tile.synthetic-wa-th.%04u",
                        tileIndex);
                    tile.m_originX = originX;
                    tile.m_originY = originY;
                    tile.m_width = tileWidth;
                    tile.m_height = tileHeight;
                    tile.m_relativePath = AZStd::string::format(
                        "Tiles/wa-th-large-bound/%04u.terrain.u16le",
                        tileIndex);
                    tile.m_byteSize = static_cast<AZ::u64>(tileWidth)
                        * static_cast<AZ::u64>(tileHeight)
                        * 2u;
                    tile.m_sha256 = IndexedSha(tileIndex);
                    document.m_tiles.push_back(AZStd::move(tile));
                    ++tileIndex;
                }
            }
            return document;
        }

        AZ::u64 MaxTileByteSize(const AZStd::vector<TerrainHeightmap::Tile>& tiles)
        {
            AZ::u64 maxBytes = 0;
            for (const auto& tile : tiles)
            {
                maxBytes = AZStd::max(maxBytes, tile.m_byteSize);
            }
            return maxBytes;
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

    TEST(TerrainHeightmapDocumentTests, WaThLargeImportBoundStaysWithinDeterministicPerformanceCardinality)
    {
        constexpr AZ::u32 WaThLargeWidth = 16385;
        constexpr AZ::u32 WaThLargeHeight = 16385;
        constexpr AZ::u64 WaThLargeSamples =
            static_cast<AZ::u64>(WaThLargeWidth) * WaThLargeHeight;
        constexpr AZ::u64 WaThLargeSourceBytes = WaThLargeSamples * 2u;
        constexpr AZ::u64 WaThMemoryBudgetBytes = 768ull * 1024ull * 1024ull;
        constexpr AZ::u64 WaThFullTileBytes =
            static_cast<AZ::u64>(TerrainHeightmap::TerrainHeightmapNominalTileSize)
            * TerrainHeightmap::TerrainHeightmapNominalTileSize
            * 2u;
        constexpr AZ::u64 ExpectedTileColumns =
            (WaThLargeWidth + TerrainHeightmap::TerrainHeightmapNominalTileSize - 1u)
            / TerrainHeightmap::TerrainHeightmapNominalTileSize;
        constexpr AZ::u64 ExpectedTileRows =
            (WaThLargeHeight + TerrainHeightmap::TerrainHeightmapNominalTileSize - 1u)
            / TerrainHeightmap::TerrainHeightmapNominalTileSize;

        const auto document = MakeLargeBoundDocument();
        const auto result = TerrainHeightmap::ValidateDocument(document);

        EXPECT_TRUE(result.m_accepted);
        EXPECT_EQ(result.m_totalSamples, WaThLargeSamples);
        EXPECT_EQ(result.m_totalSamples, TerrainHeightmap::TerrainHeightmapMaximumTotalSamples);
        EXPECT_EQ(result.m_tileCount, ExpectedTileColumns * ExpectedTileRows);
        EXPECT_EQ(result.m_tileCount, 289ull);
        EXPECT_LE(result.m_tileCount, TerrainHeightmap::TerrainHeightmapMaximumTileCount);
        EXPECT_EQ(WaThLargeSourceBytes, 536936450ull);
        EXPECT_LE(WaThLargeSourceBytes, WaThMemoryBudgetBytes);
        EXPECT_EQ(MaxTileByteSize(document.m_tiles), WaThFullTileBytes);
        EXPECT_LE(WaThFullTileBytes, WaThMemoryBudgetBytes);
        ASSERT_FALSE(document.m_tiles.empty());
        EXPECT_EQ(document.m_tiles.back().m_originX, 16384);
        EXPECT_EQ(document.m_tiles.back().m_originY, 16384);
        EXPECT_EQ(document.m_tiles.back().m_width, 1);
        EXPECT_EQ(document.m_tiles.back().m_height, 1);
    }

    TEST(TerrainHeightmapDocumentTests, WaThLargeImportBoundRejectsOneSampleBeyondMaximum)
    {
        const auto document = MakeLargeBoundDocument(16386, 16385);
        const auto result = TerrainHeightmap::ValidateDocument(document);

        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "grid.invalid"));
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

    TEST(TerrainHeightmapDocumentTests, RawImportProducesByteIdenticalSyntheticOutputsAcrossThreeRuns)
    {
        AZStd::vector<QByteArray> manifests;
        AZStd::vector<QByteArray> observations;
        AZStd::vector<QByteArray> tilePayloads;
        AZStd::vector<AZStd::string> documentFingerprints;
        AZStd::vector<AZStd::string> sourceFingerprints;
        AZStd::vector<AZStd::string> sidecarFingerprints;
        AZStd::vector<AZStd::string> tileFingerprints;
        const QByteArray rawBytes(
            "\x10\x00\x20\x00\x30\x00\x40\x00",
            8);

        for (int runIndex = 0; runIndex < 3; ++runIndex)
        {
            QTemporaryDir temporary;
            ASSERT_TRUE(temporary.isValid());
            const QString rawPath = QDir(temporary.path()).filePath("deterministic.raw");
            const QString sidecarPath = QDir(temporary.path()).filePath("deterministic.raw.json");
            ASSERT_TRUE(WriteFile(rawPath, rawBytes));
            ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(2, 2, "little-endian")));

            auto outcome = TerrainHeightmap::ImportRawHeightmapToWorkspace(
                MakeImportRequest(
                    temporary,
                    rawPath,
                    sidecarPath,
                    "terrain-import.deterministic-performance"));
            ASSERT_TRUE(outcome.IsSuccess()) << outcome.GetError().c_str();
            const auto& result = outcome.GetValue();
            ASSERT_EQ(result.m_publishedTilePaths.size(), 1);
            ASSERT_EQ(result.m_document.m_tiles.size(), 1);
            EXPECT_TRUE(TerrainHeightmap::ValidateDocument(result.m_document).m_accepted);

            manifests.push_back(ReadAll(result.m_publishedManifestPath));
            observations.push_back(ReadAll(result.m_sourceObservationPath));
            tilePayloads.push_back(ReadAll(result.m_publishedTilePaths.front()));
            documentFingerprints.push_back(
                TerrainHeightmap::CalculateDocumentFingerprint(result.m_document));
            sourceFingerprints.push_back(result.m_sourceFingerprint);
            sidecarFingerprints.push_back(result.m_sidecarFingerprint);
            tileFingerprints.push_back(result.m_document.m_tiles.front().m_sha256);
        }

        ASSERT_EQ(manifests.size(), 3);
        EXPECT_EQ(manifests[0], manifests[1]);
        EXPECT_EQ(manifests[1], manifests[2]);
        EXPECT_EQ(observations[0], observations[1]);
        EXPECT_EQ(observations[1], observations[2]);
        EXPECT_EQ(tilePayloads[0], tilePayloads[1]);
        EXPECT_EQ(tilePayloads[1], tilePayloads[2]);
        EXPECT_EQ(documentFingerprints[0], documentFingerprints[1]);
        EXPECT_EQ(documentFingerprints[1], documentFingerprints[2]);
        EXPECT_EQ(sourceFingerprints[0], sourceFingerprints[1]);
        EXPECT_EQ(sourceFingerprints[1], sourceFingerprints[2]);
        EXPECT_EQ(sidecarFingerprints[0], sidecarFingerprints[1]);
        EXPECT_EQ(sidecarFingerprints[1], sidecarFingerprints[2]);
        EXPECT_EQ(tileFingerprints[0], tileFingerprints[1]);
        EXPECT_EQ(tileFingerprints[1], tileFingerprints[2]);
    }

    TEST(TerrainHeightmapDocumentTests, Png16ImageImportPublishesCanonicalLittleEndianTile)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("synthetic.png");
        const AZStd::vector<quint16> samples = {
            0x0001,
            0x0102,
            0xABCD,
            0xFF00,
        };
        ASSERT_TRUE(WriteGrayscale16Image(imagePath, "png", 2, 2, samples));

        auto outcome = TerrainHeightmap::ImportImageHeightmapToWorkspace(
            MakeImageImportRequest(temporary, imagePath));
        ASSERT_TRUE(outcome.IsSuccess()) << outcome.GetError().c_str();
        const auto& result = outcome.GetValue();

        EXPECT_EQ(result.m_document.m_sourceBinding.m_sourceKind, "user-exported-png16");
        EXPECT_EQ(result.m_document.m_grid.m_width, 2);
        EXPECT_EQ(result.m_document.m_grid.m_height, 2);
        EXPECT_FALSE(result.m_document.m_authority.m_runtimeUseAllowed);
        EXPECT_FALSE(result.m_document.m_authority.m_deploymentAllowed);
        EXPECT_FALSE(result.m_document.m_authority.m_packagingAllowed);
        EXPECT_TRUE(TerrainHeightmap::ValidateDocument(result.m_document).m_accepted);
        ASSERT_EQ(result.m_publishedTilePaths.size(), 1);
        EXPECT_EQ(ReadAll(result.m_publishedTilePaths.front()), LittleEndianSamples(samples));

        const QByteArray manifestBytes = ReadAll(result.m_publishedManifestPath);
        EXPECT_TRUE(manifestBytes.contains("\"source_kind\":\"user-exported-png16\""));
        EXPECT_FALSE(manifestBytes.contains(imagePath.toUtf8()));
        EXPECT_FALSE(manifestBytes.contains(temporary.path().toUtf8()));

        const QByteArray observationBytes = ReadAll(result.m_sourceObservationPath);
        EXPECT_TRUE(observationBytes.contains("\"metadata_sha256\":\"sha256:"));
        EXPECT_FALSE(observationBytes.contains("\"sidecar_sha256\""));
        EXPECT_FALSE(observationBytes.contains(imagePath.toUtf8()));
    }

    TEST(TerrainHeightmapDocumentTests, Tiff16ImageImportPublishesCanonicalLittleEndianTile)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("synthetic.tiff");
        const AZStd::vector<quint16> samples = {
            0x1001,
            0x2002,
        };
        ASSERT_TRUE(WriteFile(imagePath, Tiff16ImageBytes(2, 1, samples)));

        auto outcome = TerrainHeightmap::ImportImageHeightmapToWorkspace(
            MakeImageImportRequest(
                temporary,
                imagePath,
                "terrain-import.synthetic-tiff"));
        ASSERT_TRUE(outcome.IsSuccess()) << outcome.GetError().c_str();
        const auto& result = outcome.GetValue();

        EXPECT_EQ(result.m_document.m_sourceBinding.m_sourceKind, "user-exported-tiff16");
        ASSERT_EQ(result.m_publishedTilePaths.size(), 1);
        EXPECT_EQ(ReadAll(result.m_publishedTilePaths.front()), LittleEndianSamples(samples));
        EXPECT_TRUE(TerrainHeightmap::ValidateDocument(result.m_document).m_accepted);
    }

    TEST(TerrainHeightmapDocumentTests, ImageImportRejectsUnsupportedBitDepth)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("grayscale8.png");
        ASSERT_TRUE(WriteGrayscale8Image(imagePath, "png"));

        const auto outcome = TerrainHeightmap::ImportImageHeightmapToWorkspace(
            MakeImageImportRequest(
                temporary,
                imagePath,
                "terrain-import.grayscale8"));
        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_NE(outcome.GetError().find("16-bit grayscale"), AZStd::string::npos);
        EXPECT_FALSE(QFileInfo(QDir(temporary.path()).filePath("Derived/Terrain")).exists());
    }

    TEST(TerrainHeightmapDocumentTests, ImageImportRejectsMalformedImage)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("malformed.png");
        ASSERT_TRUE(WriteFile(imagePath, QByteArray("not-a-png", 9)));

        const auto outcome = TerrainHeightmap::ImportImageHeightmapToWorkspace(
            MakeImageImportRequest(
                temporary,
                imagePath,
                "terrain-import.malformed-image"));
        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_TRUE(
            outcome.GetError().find("PNG or TIFF image content") != AZStd::string::npos
            || outcome.GetError().find("decode") != AZStd::string::npos);
        EXPECT_FALSE(QFileInfo(QDir(temporary.path()).filePath("Derived/Terrain")).exists());
    }

    TEST(TerrainHeightmapDocumentTests, ImageImportRejectsOversizedMetadataBeforeDecode)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("oversized.png");
        ASSERT_TRUE(WriteFile(imagePath, OversizedPngHeader()));

        const auto outcome = TerrainHeightmap::ImportImageHeightmapToWorkspace(
            MakeImageImportRequest(
                temporary,
                imagePath,
                "terrain-import.oversized-image"));
        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_NE(outcome.GetError().find("dimensions"), AZStd::string::npos);
        EXPECT_FALSE(QFileInfo(QDir(temporary.path()).filePath("Derived/Terrain")).exists());
    }

    TEST(TerrainHeightmapDocumentTests, ImageImportRejectsHugeMetadataBeforeDecode)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("huge-metadata.png");
        ASSERT_TRUE(WriteFile(imagePath, HugeMetadataPngHeader()));

        const auto outcome = TerrainHeightmap::ImportImageHeightmapToWorkspace(
            MakeImageImportRequest(
                temporary,
                imagePath,
                "terrain-import.huge-metadata-image"));
        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_NE(outcome.GetError().find("metadata"), AZStd::string::npos);
        EXPECT_FALSE(QFileInfo(QDir(temporary.path()).filePath("Derived/Terrain")).exists());
    }

    TEST(TerrainHeightmapDocumentTests, ImageImportRequiresValidCallerMetadata)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("metadata.png");
        ASSERT_TRUE(WriteGrayscale16Image(
            imagePath,
            "png",
            1,
            1,
            AZStd::vector<quint16>{ 0x0007 }));
        auto request = MakeImageImportRequest(
            temporary,
            imagePath,
            "terrain-import.bad-image-metadata");
        request.m_gridMetadata.m_sampleSpacingXMetres = 0.0;

        const auto outcome = TerrainHeightmap::ImportImageHeightmapToWorkspace(request);
        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_NE(outcome.GetError().find("metadata"), AZStd::string::npos);
        EXPECT_FALSE(QFileInfo(QDir(temporary.path()).filePath("Derived/Terrain")).exists());
    }

    TEST(TerrainHeightmapDocumentTests, ImageImportRejectsProtectedAndUnsupportedInputSuffixes)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("payload.assets");
        ASSERT_TRUE(WriteFile(imagePath, QByteArray("\x00\x00", 2)));

        const auto outcome = TerrainHeightmap::ImportImageHeightmapToWorkspace(
            MakeImageImportRequest(
                temporary,
                imagePath,
                "terrain-import.protected-image"));
        EXPECT_FALSE(outcome.IsSuccess());
        EXPECT_NE(outcome.GetError().find("prohibited"), AZStd::string::npos);
    }

    TEST(TerrainHeightmapDocumentTests, ImageImportWillNotOverwritePublishedRevision)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("no-overwrite.png");
        ASSERT_TRUE(WriteGrayscale16Image(
            imagePath,
            "png",
            1,
            1,
            AZStd::vector<quint16>{ 0x002A }));
        const auto request = MakeImageImportRequest(
            temporary,
            imagePath,
            "terrain-import.no-overwrite-image");

        ASSERT_TRUE(TerrainHeightmap::ImportImageHeightmapToWorkspace(request).IsSuccess());
        const auto second = TerrainHeightmap::ImportImageHeightmapToWorkspace(request);
        EXPECT_FALSE(second.IsSuccess());
        EXPECT_NE(second.GetError().find("already exists"), AZStd::string::npos);
    }

    TEST(TerrainHeightmapDocumentTests, SavedDocumentParserRoundTripsAndRejectsSchemaDrift)
    {
        auto document = MakeDocument();
        const auto canonical = TerrainHeightmap::BuildCanonicalDocumentJson(document);
        auto parsed = TerrainHeightmap::ParseDocumentJson(canonical);
        ASSERT_TRUE(parsed.IsSuccess()) << parsed.GetError().c_str();
        EXPECT_EQ(TerrainHeightmap::BuildCanonicalDocumentJson(parsed.GetValue()), canonical);
        const auto original = QJsonDocument::fromJson(QByteArray(canonical.data(), static_cast<int>(canonical.size()))).object();
        for (const auto* field : {"schema", "authority", "tiles", "profile_binding"})
        {
            auto changed = original;
            changed.remove(field);
            EXPECT_FALSE(TerrainHeightmap::ParseDocumentJson(ToAzString(QJsonDocument(changed).toJson())).IsSuccess()) << field;
        }
        auto changed = original;
        changed["unknown"] = true;
        EXPECT_FALSE(TerrainHeightmap::ParseDocumentJson(ToAzString(QJsonDocument(changed).toJson())).IsSuccess());
        changed = original;
        auto authority = changed.value("authority").toObject();
        authority["game_write_allowed"] = "false";
        changed["authority"] = authority;
        EXPECT_FALSE(TerrainHeightmap::ParseDocumentJson(ToAzString(QJsonDocument(changed).toJson())).IsSuccess());
    }

    TEST(TerrainHeightmapDocumentTests, PreviewReopensSamplesAndRejectsChangedTilesOrProfiles)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("preview.raw");
        const QString sidecarPath = rawPath + ".json";
        ASSERT_TRUE(WriteFile(rawPath, LittleEndianSamples({0, 32768, 65535, 16384})));
        ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(2, 2, "little-endian")));
        const auto request = MakeImportRequest(temporary, rawPath, sidecarPath);
        auto imported = TerrainHeightmap::ImportRawHeightmapToWorkspace(request);
        ASSERT_TRUE(imported.IsSuccess()) << imported.GetError().c_str();
        const auto relative = ToAzString(QDir(temporary.path()).relativeFilePath(QString::fromUtf8(imported.GetValue().m_publishedManifestPath.c_str())));
        auto preview = TerrainHeightmap::LoadWorkspaceTerrainPreview(request.m_workspaceRoot, relative, request.m_profileBinding);
        ASSERT_TRUE(preview.IsSuccess()) << preview.GetError().c_str();
        EXPECT_EQ(preview.GetValue().m_width, 2);
        EXPECT_EQ(preview.GetValue().m_height, 2);
        EXPECT_EQ(preview.GetValue().m_grayscale, (AZStd::vector<AZ::u8>{0, 128, 255, 64}));
        auto stale = request.m_profileBinding;
        stale.m_gameVersion = "different";
        EXPECT_FALSE(TerrainHeightmap::LoadWorkspaceTerrainPreview(request.m_workspaceRoot, relative, stale).IsSuccess());
        EXPECT_FALSE(TerrainHeightmap::LoadWorkspaceTerrainPreview(request.m_workspaceRoot, "../escape.json", request.m_profileBinding).IsSuccess());
        ASSERT_TRUE(WriteFile(QString::fromUtf8(imported.GetValue().m_publishedTilePaths.front().c_str()), QByteArray(8, '\0')));
        auto changed = TerrainHeightmap::LoadWorkspaceTerrainPreview(request.m_workspaceRoot, relative, request.m_profileBinding);
        ASSERT_FALSE(changed.IsSuccess());
        EXPECT_NE(changed.GetError().find("fingerprint"), AZStd::string::npos);
    }

    TEST(TerrainHeightmapDocumentTests, RawCancellationBeforePublicationRollsBackAndCanRetry)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("cancel.raw");
        const QString sidecarPath = rawPath + ".json";
        ASSERT_TRUE(WriteFile(rawPath, QByteArray(2050, '\0')));
        ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(1025, 1, "little-endian")));
        auto request = MakeImportRequest(temporary, rawPath, sidecarPath);
        bool cancelled = false;
        TerrainHeightmap::ImportControl control;
        control.m_cancelled = [&]() { return cancelled; };
        control.m_progress = [&](AZ::u64 done, AZ::u64) { if (done > 0) { cancelled = true; } };
        request.m_control = &control;
        EXPECT_FALSE(TerrainHeightmap::ImportRawHeightmapToWorkspace(request).IsSuccess());
        QDirIterator files(temporary.path(), {"terrain.tgheightmap.json", "source-observation.json"}, QDir::Files, QDirIterator::Subdirectories);
        EXPECT_FALSE(files.hasNext());
        request.m_control = nullptr;
        auto retry = TerrainHeightmap::ImportRawHeightmapToWorkspace(request);
        ASSERT_TRUE(retry.IsSuccess()) << retry.GetError().c_str();
        EXPECT_EQ(retry.GetValue().m_tileCount, 2);
    }

    TEST(TerrainHeightmapDocumentTests, ImageSidecarRequiresScaleAndMatchesImageDimensions)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("metadata.png");
        ASSERT_TRUE(WriteGrayscale16Image(imagePath, "PNG", 2, 2, {0, 16384, 32768, 65535}));
        const QString sidecarPath = imagePath + ".json";
        auto object = QJsonDocument::fromJson(SidecarJson(2, 2, "little-endian")).object();
        object["schema"] = "foa.terrain-heightmap.image-import-metadata";
        object.remove("byte_order");
        ASSERT_TRUE(WriteFile(sidecarPath, QJsonDocument(object).toJson()));
        auto request = MakeImageImportRequest(temporary, imagePath);
        ASSERT_TRUE(TerrainHeightmap::ReadImageImportSidecar(ToAzString(sidecarPath), request).IsSuccess());
        EXPECT_EQ(request.m_gridMetadata.m_width, 2);
        auto imported = TerrainHeightmap::ImportImageHeightmapToWorkspace(request);
        ASSERT_TRUE(imported.IsSuccess()) << imported.GetError().c_str();
        object["width"] = 3;
        ASSERT_TRUE(WriteFile(sidecarPath, QJsonDocument(object).toJson()));
        ASSERT_TRUE(TerrainHeightmap::ReadImageImportSidecar(ToAzString(sidecarPath), request).IsSuccess());
        EXPECT_FALSE(TerrainHeightmap::ImportImageHeightmapToWorkspace(request).IsSuccess());
        object.remove("min_height_metres");
        ASSERT_TRUE(WriteFile(sidecarPath, QJsonDocument(object).toJson()));
        EXPECT_FALSE(TerrainHeightmap::ReadImageImportSidecar(ToAzString(sidecarPath), request).IsSuccess());
    }

    TEST(TerrainHeightmapDocumentTests, ImageCancellationAfterFinalTileDoesNotPublish)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString imagePath = QDir(temporary.path()).filePath("cancel.png");
        ASSERT_TRUE(WriteGrayscale16Image(imagePath, "PNG", 2, 2, {0, 16384, 32768, 65535}));
        auto request = MakeImageImportRequest(temporary, imagePath);
        bool cancelled = false;
        TerrainHeightmap::ImportControl control;
        control.m_cancelled = [&]() { return cancelled; };
        control.m_progress = [&](AZ::u64, AZ::u64) { cancelled = true; };
        request.m_control = &control;
        EXPECT_FALSE(TerrainHeightmap::ImportImageHeightmapToWorkspace(request).IsSuccess());
        QDirIterator files(temporary.path(), {"terrain.tgheightmap.json", "source-observation.json"}, QDir::Files, QDirIterator::Subdirectories);
        EXPECT_FALSE(files.hasNext());
        request.m_control = nullptr;
        EXPECT_TRUE(TerrainHeightmap::ImportImageHeightmapToWorkspace(request).IsSuccess());
    }


    TEST(TerrainHeightmapDocumentTests, HostImportsRefreshesAndReopensWithoutExposingWorkspacePaths)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("host.raw");
        const QString sidecarPath = rawPath + ".json";
        ASSERT_TRUE(WriteFile(rawPath, LittleEndianSamples({0, 32768, 65535, 16384})));
        ASSERT_TRUE(WriteFile(sidecarPath, SidecarJson(2, 2, "little-endian")));
        auto profile = MakeImportRequest(temporary, rawPath, sidecarPath).m_profileBinding;
        TerrainImportHost host;
        auto send = [&](const QJsonObject& command)
        {
            auto response = host.Dispatch(command, temporary.path(), {}, profile);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (response.value("busy").toBool() && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                response = host.Dispatch({{"action", "poll"}}, temporary.path(), {}, profile);
            }
            EXPECT_FALSE(response.value("busy").toBool());
            EXPECT_FALSE(QJsonDocument(response).toJson().contains(temporary.path().toUtf8()));
            EXPECT_FALSE(response.contains("locator"));
            return response;
        };
        auto imported = send({{"action", "import"}, {"source", rawPath}});
        ASSERT_EQ(imported.value("status").toString(), "complete") << imported.value("message").toString().toUtf8().constData();
        const auto preview = imported.value("preview");
        const auto revision = imported.value("revision");
        ASSERT_FALSE(preview.toString().isEmpty());
        const auto inventory = send({{"action", "refresh"}});
        ASSERT_EQ(inventory.value("rows").toArray().size(), 1);
        EXPECT_EQ(inventory.value("rows").toArray().first().toObject().value("revision"), revision);
        const auto reopened = send({{"action", "open"}, {"revision", revision}});
        EXPECT_EQ(reopened.value("preview"), preview);
        profile.m_gameVersion = "changed";
        auto stale = send({{"action", "open"}, {"revision", revision}});
        EXPECT_EQ(stale.value("status").toString(), "failed");
        EXPECT_TRUE(send({{"action", "refresh"}}).value("rows").toArray().isEmpty());
    }

    TEST(TerrainHeightmapDocumentTests, HostRejectsSourcesInsideRegisteredGameBeforeImport)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("protected.raw");
        ASSERT_TRUE(WriteFile(rawPath, LittleEndianSamples({0, 32768})));
        const auto profile = MakeImportRequest(temporary, rawPath, rawPath + ".json").m_profileBinding;
        TerrainImportHost host;
        auto response = host.Dispatch({{"action", "import"}, {"source", rawPath}}, temporary.path(), temporary.path(), profile);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (response.value("busy").toBool() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            response = host.Dispatch({{"action", "poll"}}, temporary.path(), temporary.path(), profile);
        }
        ASSERT_FALSE(response.value("busy").toBool());
        EXPECT_EQ(response.value("status").toString(), "failed");
        EXPECT_TRUE(response.value("message").toString().contains("outside the game"));
        EXPECT_FALSE(QDir(QDir(temporary.path()).filePath("Derived")).exists());
    }

    TEST(TerrainHeightmapDocumentTests, CampaignExportProvenanceSurvivesRawImportAndStrictReopen)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString rawPath = QDir(temporary.path()).filePath("campaign.raw");
        ASSERT_TRUE(WriteFile(rawPath, LittleEndianSamples({0, 32768})));
        auto metadata = QJsonDocument::fromJson(SidecarJson(2, 1, "little-endian")).object();
        metadata["source_extraction"] = QJsonObject{{"schema", "foa.campaign-heightmap-export.receipt"},
            {"source_sha256", QString::fromUtf8(Sha('a').c_str())}};
        ASSERT_TRUE(WriteFile(rawPath + ".json", QJsonDocument(metadata).toJson()));
        auto request = MakeImportRequest(temporary, rawPath, rawPath + ".json");
        request.m_importerId = "importer.campaign-ground-raw";
        request.m_provenanceLimitations = "Synthetic ground coverage 60%; estimated cells remain marked in the source export.";
        auto imported = TerrainHeightmap::ImportRawHeightmapToWorkspace(request);
        ASSERT_TRUE(imported.IsSuccess()) << imported.GetError().c_str();
        EXPECT_EQ(imported.GetValue().m_document.m_provenance.m_limitations, request.m_provenanceLimitations);
        EXPECT_EQ(imported.GetValue().m_document.m_sourceBinding.m_configurationFingerprint, imported.GetValue().m_sidecarFingerprint);
        EXPECT_FALSE(imported.GetValue().m_document.m_authority.m_publicationAllowed);
        const auto relative = ToAzString(QDir(temporary.path()).relativeFilePath(QString::fromUtf8(imported.GetValue().m_publishedManifestPath.c_str())));
        auto reopened = TerrainHeightmap::LoadWorkspaceTerrainPreview(request.m_workspaceRoot, relative, request.m_profileBinding);
        ASSERT_TRUE(reopened.IsSuccess()) << reopened.GetError().c_str();
        EXPECT_EQ(reopened.GetValue().m_document.m_sourceBinding.m_exporterId, request.m_importerId);
    }

    TEST(TerrainHeightmapDocumentTests, HostRejectsCampaignImportBeforeStartingWorker)
    {
        QTemporaryDir temporary;
        const auto profile = MakeImportRequest(temporary, {}, {}).m_profileBinding;
        TerrainImportHost host;
        const auto result = host.Dispatch({{"action", "import-campaign"}, {"campaign", "hos"}}, temporary.path(), {}, profile);
        EXPECT_EQ(result.value("status").toString(), "failed");
        EXPECT_FALSE(result.value("busy").toBool());
        EXPECT_TRUE(result.value("message").toString().contains("game round-trip"));
        EXPECT_FALSE(QDir(temporary.path()).exists("Staging"));
        EXPECT_FALSE(QDir(temporary.path()).exists("Derived"));
    }

    TEST(TerrainHeightmapDocumentTests, LegacyCampaignCannotOpenForEditingOrReimportAsLocalHeightmap)
    {
        QTemporaryDir temporary;
        const auto source = temporary.filePath("legacy-campaign.raw");
        const QByteArray samples = QByteArray::fromHex("000001013412ffff");
        ASSERT_TRUE(WriteFile(source, samples));
        auto metadata = QJsonDocument::fromJson(SidecarJson(2, 2, "little-endian")).object();
        metadata["source_extraction"] = QJsonObject{{"schema", "foa.campaign-heightmap-export.receipt"}};
        ASSERT_TRUE(WriteFile(source + ".json", QJsonDocument(metadata).toJson()));
        auto request = MakeImportRequest(temporary, source, source + ".json");
        request.m_importerId = "importer.campaign-ground-raw";
        const auto imported = TerrainHeightmap::ImportRawHeightmapToWorkspace(request);
        ASSERT_TRUE(imported.IsSuccess());
        const auto locator = QDir(temporary.path()).relativeFilePath(QString::fromUtf8(imported.GetValue().m_publishedManifestPath.c_str()));
        const auto rejected = PrepareNativeTerrain(temporary.path(), locator, request.m_profileBinding, nullptr);
        EXPECT_EQ(rejected.value("status").toString(), "failed");
        EXPECT_TRUE(rejected.value("message").toString().contains("does not preserve the original map"));
        EXPECT_FALSE(QDir(temporary.path()).exists("EditorAssets"));
        EXPECT_FALSE(QDir(temporary.path()).exists("Staging/TerrainNative"));
        EXPECT_EQ(ReadAll(ToAzString(source)), samples);
        EXPECT_TRUE(TerrainHeightmap::LoadWorkspaceTerrainPreview(request.m_workspaceRoot, ToAzString(locator), request.m_profileBinding).IsSuccess());
        TerrainImportHost host;
        auto response = host.Dispatch({{"action", "import"}, {"source", source}}, temporary.path(), {}, request.m_profileBinding);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (response.value("busy").toBool() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            response = host.Dispatch({{"action", "poll"}}, temporary.path(), {}, request.m_profileBinding);
        }
        EXPECT_FALSE(response.value("busy").toBool());
        EXPECT_EQ(response.value("status").toString(), "failed");
        EXPECT_TRUE(response.value("message").toString().contains("unsupported campaign reconstruction"));
    }

    TEST(TerrainHeightmapDocumentTests, CampaignWorkerRejectsUnknownCampaignWithoutWrites)
    {
        QTemporaryDir temporary;
        TerrainCampaignProvider provider;
        TerrainHeightmap::ProfileBinding profile;
        auto result = ExportTerrainCampaign(provider, temporary.path(), temporary.path(), "6000.0.64f1",
            profile, "unknown", "terrain-import.invalid", "2026-09-08T00:00:00Z", nullptr);
        EXPECT_EQ(result.value("status").toString(), "failed");
        EXPECT_FALSE(QDir(temporary.path()).exists("Staging"));
    }

    TEST(TerrainHeightmapDocumentTests, CampaignWorkerFailureTimeoutCancellationAndMalformedResultsFailClosed)
    {
        const QString python = qEnvironmentVariable("FOA_HEIGHTMAP_TEST_PYTHON");
        if (python.isEmpty()) { GTEST_SKIP() << "Set FOA_HEIGHTMAP_TEST_PYTHON to the qualified local worker runtime."; }
        ASSERT_TRUE(QFileInfo(python).isFile());
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString workspace = QDir(temporary.path()).filePath("Workspace");
        const QString game = QDir(temporary.path()).filePath("SyntheticGame");
        const QString aa = QDir(game).filePath("Fall of Avalon_Data/StreamingAssets/aa");
        ASSERT_TRUE(QDir().mkpath(workspace));
        ASSERT_TRUE(QDir().mkpath(aa + "/StandaloneWindows64"));
        ASSERT_TRUE(WriteFile(aa + "/catalog.json", "{}"));
        for (const auto& suffix : {QString(".bundle"), QString("_static.bundle")})
        {
            ASSERT_TRUE(WriteFile(aa + "/StandaloneWindows64/scenes_scenes_campaignmap_hos" + suffix, "synthetic"));
        }
        const QString script = QDir(workspace).filePath("synthetic-worker.py");
        const TerrainCampaignProvider provider{python, script};
        TerrainHeightmap::ProfileBinding profile;
        profile.m_profileId = "profile.synthetic";
        profile.m_profileFingerprint = Sha('a');
        for (int mode = 0; mode < 4; ++mode)
        {
            const QByteArray body = mode == 0 ? QByteArray("import sys; sys.exit(9)\n")
                : mode == 1 || mode == 2 ? QByteArray("import time; time.sleep(60)\n")
                : QByteArray("from pathlib import Path\nPath('result.json').write_text('{}')\n");
            ASSERT_TRUE(WriteFile(script, body));
            TerrainHeightmap::ImportControl control;
            control.m_cancelled = [mode]() { return mode == 2; };
            const auto result = ExportTerrainCampaign(provider, workspace, game, "6000.0.64f1", profile,
                "hos", "terrain-import.process-" + QString::number(mode), "2026-09-08T00:00:00Z",
                &control, mode == 1 ? 30 : 10000);
            EXPECT_EQ(result.value("status").toString(), "failed") << mode;
            if (mode == 1) { EXPECT_TRUE(result.value("message").toString().contains("limit")); }
            if (mode == 2) { EXPECT_TRUE(result.value("message").toString().contains("cancelled")); }
            if (mode == 3) { EXPECT_TRUE(result.value("message").toString().contains("invalid")); }
            EXPECT_FALSE(QDir(workspace).exists("Derived"));
            EXPECT_FALSE(QDir(workspace).exists("SourceExports"));
        }
    }

    TEST(TerrainHeightmapTests, NativeProjectionPreservesU16SamplesAndDoesNotOverwritePaintedImage)
    {
        QTemporaryDir temporary;
        const auto raw = temporary.filePath("source.raw");
        const QByteArray samples = QByteArray::fromHex("000001013412ffff");
        ASSERT_TRUE(WriteFile(raw, samples));
        ASSERT_TRUE(WriteFile(raw + ".json", SidecarJson(2, 2, "little-endian")));
        const auto request = MakeImportRequest(temporary, raw, raw + ".json");
        const auto imported = TerrainHeightmap::ImportRawHeightmapToWorkspace(request);
        ASSERT_TRUE(imported.IsSuccess());
        const auto locator = QDir(temporary.path()).relativeFilePath(QString::fromUtf8(imported.GetValue().m_publishedManifestPath.c_str()));
        const auto prepared = PrepareNativeTerrain(temporary.path(), locator, request.m_profileBinding, nullptr);
        ASSERT_EQ(prepared.value("status").toString(), "running");
        QFile handoff(prepared.value("native_request").toString());
        ASSERT_TRUE(handoff.open(QIODevice::ReadOnly));
        const auto metadata = QJsonDocument::fromJson(handoff.readAll()).object();
        const auto imagePath = metadata.value("image").toString();
        const auto roundtrip = TerrainHeightmap::ImportImageHeightmapToWorkspace(MakeImageImportRequest(temporary, imagePath));
        ASSERT_TRUE(roundtrip.IsSuccess());
        ASSERT_EQ(roundtrip.GetValue().m_publishedTilePaths.size(), 1);
        EXPECT_EQ(ReadAll(roundtrip.GetValue().m_publishedTilePaths.front()), samples);
        EXPECT_TRUE(imagePath.endsWith("height_gsi.tif"));
        ASSERT_TRUE(WriteFile(imagePath, "painted-image-placeholder"));
        EXPECT_EQ(PrepareNativeTerrain(temporary.path(), locator, request.m_profileBinding, nullptr).value("status").toString(), "running");
        QFile painted(imagePath); ASSERT_TRUE(painted.open(QIODevice::ReadOnly));
        EXPECT_EQ(painted.readAll(), "painted-image-placeholder");
        QFile source(raw); ASSERT_TRUE(source.open(QIODevice::ReadOnly)); EXPECT_EQ(source.readAll(), samples);
        for (const char* path : {"EditorAssets/foa_maps/example/Map.prefab", "EditorAssets/foa_maps/example/height_gsi.tif", "Staging/TerrainNative/request.json"})
        { EXPECT_FALSE(TerrainHeightmap::ValidateTerrainPackagePath(path).m_allowed); }
    }

    TEST(TerrainHeightmapTests, NativeProjectionRejectsStaleProfilesCancellationAndSourceCheckouts)
    {
        QTemporaryDir temporary;
        const auto raw = temporary.filePath("source.raw");
        ASSERT_TRUE(WriteFile(raw, QByteArray::fromHex("000001013412ffff")));
        ASSERT_TRUE(WriteFile(raw + ".json", SidecarJson(2, 2, "little-endian")));
        const auto request = MakeImportRequest(temporary, raw, raw + ".json");
        const auto imported = TerrainHeightmap::ImportRawHeightmapToWorkspace(request);
        ASSERT_TRUE(imported.IsSuccess());
        const auto locator = QDir(temporary.path()).relativeFilePath(QString::fromUtf8(imported.GetValue().m_publishedManifestPath.c_str()));
        auto stale = request.m_profileBinding; stale.m_gameVersion = "stale";
        EXPECT_EQ(PrepareNativeTerrain(temporary.path(), locator, stale, nullptr).value("status").toString(), "failed");
        TerrainHeightmap::ImportControl cancelled; cancelled.m_cancelled = []() { return true; };
        EXPECT_EQ(PrepareNativeTerrain(temporary.path(), locator, request.m_profileBinding, &cancelled).value("status").toString(), "failed");
        EXPECT_FALSE(QDir(temporary.path()).exists("EditorAssets"));
        ASSERT_TRUE(WriteFile(temporary.filePath(".git"), "gitdir: synthetic"));
        EXPECT_EQ(PrepareNativeTerrain(temporary.path(), locator, request.m_profileBinding, nullptr).value("status").toString(), "failed");
        EXPECT_FALSE(QDir(temporary.path()).exists("EditorAssets"));
    }

} // namespace TaintedGrailModdingSDK
