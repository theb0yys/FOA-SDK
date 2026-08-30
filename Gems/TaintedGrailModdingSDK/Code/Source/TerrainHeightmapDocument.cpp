/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "TerrainHeightmapDocument.h"

#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"
#include "ResearchContractValidation.h"

#include <AzCore/PlatformDef.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QImage>
#include <QImageReader>
#include <QStringList>

#include <cmath>
#include <cstring>
#include <limits>

namespace TaintedGrailModdingSDK::TerrainHeightmap
{
    namespace
    {
        constexpr size_t MaximumShortTextLength = 256;
        constexpr size_t MaximumLongTextLength = 4096;
        constexpr size_t MaximumAliases = 16;
        constexpr size_t MaximumRelativePathLength = 260;
        constexpr qint64 MaximumRawSidecarBytes = 1024 * 1024;
        constexpr AZ::u64 MaximumImageContainerOverheadBytes = 16ull * 1024ull * 1024ull;

        struct FileSnapshot
        {
            QString m_canonicalPath;
            qint64 m_size = 0;
            qint64 m_lastModifiedMs = 0;
        };

        struct RawSidecarMetadata
        {
            Grid m_grid;
            AZStd::string m_byteOrder;
            VerticalMapping m_verticalMapping;
            CoordinateSpace m_coordinateSpace;
        };

        struct ImageHeaderMetadata
        {
            QByteArray m_format;
            AZ::u32 m_width = 0;
            AZ::u32 m_height = 0;
            bool m_tiffLittleEndian = true;
            AZ::u32 m_tiffRowsPerStrip = 0;
            AZStd::vector<AZ::u32> m_tiffStripOffsets;
            AZStd::vector<AZ::u32> m_tiffStripByteCounts;
        };

        bool CalculateTotalSamples(const Grid& grid, AZ::u64& totalSamples);

        bool StartsWith(const AZStd::string& value, const char* prefix)
        {
            const AZStd::string prefixText(prefix);
            return value.size() >= prefixText.size()
                && value.compare(0, prefixText.size(), prefixText) == 0;
        }

        bool EndsWith(const AZStd::string& value, const char* suffix)
        {
            const AZStd::string suffixText(suffix);
            return value.size() >= suffixText.size()
                && value.compare(value.size() - suffixText.size(), suffixText.size(), suffixText) == 0;
        }

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        AZStd::string ToSha256Fingerprint(const QByteArray& digest)
        {
            const QByteArray hex = digest.toHex();
            return "sha256:" + AZStd::string(hex.constData(), static_cast<size_t>(hex.size()));
        }

        AZStd::string FoldAscii(AZStd::string value)
        {
            for (char& character : value)
            {
                if (character >= 'A' && character <= 'Z')
                {
                    character = static_cast<char>(character - 'A' + 'a');
                }
            }
            return value;
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

        void AddIssue(
            ValidationResult& result,
            AZStd::string locator,
            AZStd::string code,
            AZStd::string message)
        {
            result.m_issues.push_back(
                ValidationIssue{
                    AZStd::move(locator),
                    AZStd::move(code),
                    AZStd::move(message) });
        }

        void AddPackageIssue(
            PackageGuardResult& result,
            AZStd::string locator,
            AZStd::string code,
            AZStd::string message)
        {
            result.m_issues.push_back(
                ValidationIssue{
                    AZStd::move(locator),
                    AZStd::move(code),
                    AZStd::move(message) });
        }

        bool IsHexFingerprintOrEmpty(const AZStd::string& value)
        {
            return value.empty() || IsSha256Fingerprint(value);
        }

        bool IsAllowedSourceKind(const AZStd::string& value)
        {
            return value == "user-exported-png16"
                || value == "user-exported-tiff16"
                || value == "user-exported-raw-u16-le"
                || value == "user-exported-raw-u16-be";
        }

        bool IsAllowedCoordinateValue(
            const AZStd::string& value,
            const AZStd::vector<AZStd::string>& allowedValues)
        {
            return AZStd::find(allowedValues.begin(), allowedValues.end(), value)
                != allowedValues.end();
        }

        bool IsReservedWindowsDeviceSegment(AZStd::string segment)
        {
            const size_t dotPosition = segment.find('.');
            if (dotPosition != AZStd::string::npos)
            {
                segment = segment.substr(0, dotPosition);
            }
            segment = FoldAscii(AZStd::move(segment));
            return segment == "con"
                || segment == "prn"
                || segment == "aux"
                || segment == "nul"
                || segment == "com1"
                || segment == "com2"
                || segment == "com3"
                || segment == "com4"
                || segment == "com5"
                || segment == "com6"
                || segment == "com7"
                || segment == "com8"
                || segment == "com9"
                || segment == "lpt1"
                || segment == "lpt2"
                || segment == "lpt3"
                || segment == "lpt4"
                || segment == "lpt5"
                || segment == "lpt6"
                || segment == "lpt7"
                || segment == "lpt8"
                || segment == "lpt9";
        }

        bool IsAllowedPathCharacter(char character)
        {
            return (character >= 'a' && character <= 'z')
                || (character >= 'A' && character <= 'Z')
                || (character >= '0' && character <= '9')
                || character == '/'
                || character == '-'
                || character == '_'
                || character == '.';
        }

        bool PathsIdentifySameLocation(const QString& left, const QString& right)
        {
#if AZ_TRAIT_USE_WINDOWS_FILE_API
            return left.compare(right, Qt::CaseInsensitive) == 0;
#else
            return left == right;
#endif
        }

        bool IsContainedPath(const QString& rootPath, const QString& candidatePath)
        {
            const QString cleanRoot = QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath());
            const QString cleanCandidate =
                QDir::cleanPath(QFileInfo(candidatePath).absoluteFilePath());
            const QString relative = QDir(cleanRoot).relativeFilePath(cleanCandidate);
            return !QDir::isAbsolutePath(relative)
                && relative != QStringLiteral("..")
                && !relative.startsWith(QStringLiteral("../"))
                && !relative.startsWith(QStringLiteral("..\\"));
        }

        QString ResolveDirectCanonicalDirectory(const QString& path)
        {
            const QFileInfo info(path);
            if (!info.exists() || !info.isDir() || info.isSymLink())
            {
                return {};
            }
            const QString declared = QDir::cleanPath(info.absoluteFilePath());
            const QString canonical = QDir::cleanPath(info.canonicalFilePath());
            return !canonical.isEmpty()
                    && PathsIdentifySameLocation(declared, canonical)
                ? canonical
                : QString();
        }

        AZ::Outcome<FileSnapshot, AZStd::string> ResolveDirectCanonicalFile(
            const AZStd::string& path,
            const char* description)
        {
            if (path.empty())
            {
                return AZ::Failure(AZStd::string(description) + " path is required.");
            }

            QFileInfo info(ToQString(path));
            info.refresh();
            if (!info.exists() || !info.isFile() || info.isSymLink())
            {
                return AZ::Failure(
                    AZStd::string(description)
                    + " must be an existing direct regular local file.");
            }
            const QString declared = QDir::cleanPath(info.absoluteFilePath());
            const QString canonical = QDir::cleanPath(info.canonicalFilePath());
            if (canonical.isEmpty()
                || canonical.startsWith(QStringLiteral("//"))
                || !PathsIdentifySameLocation(declared, canonical))
            {
                return AZ::Failure(
                    AZStd::string(description)
                    + " must resolve to one direct canonical local filesystem identity.");
            }

            info = QFileInfo(canonical);
            info.refresh();
            FileSnapshot snapshot;
            snapshot.m_canonicalPath = canonical;
            snapshot.m_size = info.size();
            snapshot.m_lastModifiedMs = info.lastModified().toMSecsSinceEpoch();
            return AZ::Success(AZStd::move(snapshot));
        }

        bool FileSnapshotIsUnchanged(const FileSnapshot& snapshot)
        {
            QFileInfo info(snapshot.m_canonicalPath);
            info.refresh();
            return info.exists()
                && info.isFile()
                && !info.isSymLink()
                && PathsIdentifySameLocation(
                    QDir::cleanPath(info.canonicalFilePath()),
                    snapshot.m_canonicalPath)
                && info.size() == snapshot.m_size
                && info.lastModified().toMSecsSinceEpoch()
                    == snapshot.m_lastModifiedMs;
        }

        bool RemoveContainedDirectory(const QString& rootPath, const QString& directory)
        {
            const QString canonicalDirectory = ResolveDirectCanonicalDirectory(directory);
            return !canonicalDirectory.isEmpty()
                && IsContainedPath(rootPath, canonicalDirectory)
                && QDir(canonicalDirectory).removeRecursively()
                && !QFileInfo::exists(canonicalDirectory);
        }

        AZ::Outcome<QString, AZStd::string> ResolveWorkspaceRoot(
            const AZStd::string& workspaceRoot)
        {
            if (workspaceRoot.empty())
            {
                return AZ::Failure(AZStd::string(
                    "A workspace root is required before terrain import."));
            }
            const QString declared =
                QDir::cleanPath(QFileInfo(ToQString(workspaceRoot)).absoluteFilePath());
            const QString canonical = ResolveDirectCanonicalDirectory(declared);
            if (canonical.isEmpty())
            {
                return AZ::Failure(AZStd::string(
                    "The workspace root must be an existing canonical directory without storage indirection."));
            }
            return AZ::Success(canonical);
        }

        AZ::Outcome<QString, AZStd::string> EnsureContainedDirectory(
            const QString& canonicalWorkspaceRoot,
            const AZStd::string& relativePath)
        {
            if (!IsSafeWorkspaceRelativePath(relativePath))
            {
                return AZ::Failure(AZStd::string(
                    "Terrain workspace directories must be safe relative paths."));
            }

            const QString path =
                QDir(canonicalWorkspaceRoot).filePath(ToQString(relativePath));
            if (!IsContainedPath(canonicalWorkspaceRoot, path)
                || !QDir().mkpath(path))
            {
                return AZ::Failure(AZStd::string(
                    "Unable to create a contained terrain workspace directory."));
            }
            const QString canonical = ResolveDirectCanonicalDirectory(path);
            if (canonical.isEmpty()
                || !IsContainedPath(canonicalWorkspaceRoot, canonical))
            {
                return AZ::Failure(AZStd::string(
                    "Terrain workspace directory crossed a symbolic link, junction, reparse, or containment boundary."));
            }
            return AZ::Success(canonical);
        }

        AZ::Outcome<AZStd::string, AZStd::string> HashFile(
            const FileSnapshot& snapshot,
            AZ::u64& bytesRead)
        {
            bytesRead = 0;
            QFile file(snapshot.m_canonicalPath);
            if (!file.open(QIODevice::ReadOnly))
            {
                return AZ::Failure(AZStd::string("Unable to open terrain source for hashing."));
            }

            QCryptographicHash hash(QCryptographicHash::Sha256);
            while (!file.atEnd())
            {
                const QByteArray chunk = file.read(1024 * 1024);
                if (chunk.isEmpty() && file.error() != QFileDevice::NoError)
                {
                    return AZ::Failure(AZStd::string("Unable to read terrain source for hashing."));
                }
                bytesRead += static_cast<AZ::u64>(chunk.size());
                hash.addData(chunk);
            }
            if (!FileSnapshotIsUnchanged(snapshot))
            {
                return AZ::Failure(AZStd::string(
                    "The terrain source changed during import; no document was published."));
            }
            return AZ::Success(ToSha256Fingerprint(hash.result()));
        }

        AZ::Outcome<QByteArray, AZStd::string> ReadBoundedSidecar(
            const FileSnapshot& snapshot,
            AZStd::string& fingerprint)
        {
            if (snapshot.m_size <= 0 || snapshot.m_size > MaximumRawSidecarBytes)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain RAW sidecar must be non-empty and no larger than 1 MiB."));
            }

            QFile file(snapshot.m_canonicalPath);
            if (!file.open(QIODevice::ReadOnly))
            {
                return AZ::Failure(AZStd::string("Unable to open terrain RAW sidecar."));
            }
            const QByteArray data = file.readAll();
            if (file.error() != QFileDevice::NoError
                || data.size() != snapshot.m_size
                || !FileSnapshotIsUnchanged(snapshot))
            {
                return AZ::Failure(AZStd::string(
                    "The terrain RAW sidecar could not be read as one immutable snapshot."));
            }
            fingerprint = ToSha256Fingerprint(QCryptographicHash::hash(
                data,
                QCryptographicHash::Sha256));
            return AZ::Success(data);
        }

        bool IsSafeObjectIdentifier(const AZStd::string& value)
        {
            return IsBoundedText(value, MaximumShortTextLength)
                && value.find(':') == AZStd::string::npos
                && value.find('/') == AZStd::string::npos
                && value.find('\\') == AZStd::string::npos
                && value.find("..") == AZStd::string::npos;
        }

        AZ::Outcome<AZStd::string, AZStd::string> ReadStringField(
            const QJsonObject& object,
            const char* key)
        {
            const QJsonValue value = object.value(QString::fromUtf8(key));
            if (!value.isString())
            {
                return AZ::Failure(
                    AZStd::string("Terrain RAW sidecar field must be a string: ") + key);
            }
            return AZ::Success(ToAzString(value.toString()));
        }

        AZ::Outcome<double, AZStd::string> ReadFiniteDoubleField(
            const QJsonObject& object,
            const char* key)
        {
            const QJsonValue value = object.value(QString::fromUtf8(key));
            if (!value.isDouble() || !std::isfinite(value.toDouble()))
            {
                return AZ::Failure(
                    AZStd::string("Terrain RAW sidecar field must be a finite number: ") + key);
            }
            return AZ::Success(value.toDouble());
        }

        AZ::Outcome<AZ::u32, AZStd::string> ReadU32Field(
            const QJsonObject& object,
            const char* key)
        {
            const QJsonValue value = object.value(QString::fromUtf8(key));
            const double number = value.toDouble(-1.0);
            if (!value.isDouble()
                || number <= 0.0
                || number > static_cast<double>(std::numeric_limits<AZ::u32>::max())
                || std::floor(number) != number)
            {
                return AZ::Failure(
                    AZStd::string("Terrain RAW sidecar field must be a positive integer: ") + key);
            }
            return AZ::Success(static_cast<AZ::u32>(number));
        }

        AZ::Outcome<RawSidecarMetadata, AZStd::string> ParseRawSidecar(
            const QByteArray& data)
        {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                return AZ::Failure(
                    AZStd::string("Terrain RAW sidecar must be a JSON object: ")
                    + ToAzString(parseError.errorString()));
            }
            const QJsonObject object = document.object();
            auto schema = ReadStringField(object, "schema");
            if (!schema.IsSuccess()
                || schema.GetValue() != "foa.raw-u16-heightmap-sidecar")
            {
                return AZ::Failure(AZStd::string(
                    "Terrain RAW sidecar schema must be foa.raw-u16-heightmap-sidecar."));
            }
            auto version = ReadU32Field(object, "schema_version");
            if (!version.IsSuccess() || version.GetValue() != 1)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain RAW sidecar schema_version must be 1."));
            }

            RawSidecarMetadata metadata;
#define TG_READ_U32_FIELD(jsonName, member) \
            if (auto result = ReadU32Field(object, jsonName); result.IsSuccess()) \
            { \
                metadata.member = result.GetValue(); \
            } \
            else \
            { \
                return AZ::Failure(AZStd::string(result.GetError())); \
            }
#define TG_READ_DOUBLE_FIELD(jsonName, member) \
            if (auto result = ReadFiniteDoubleField(object, jsonName); result.IsSuccess()) \
            { \
                metadata.member = result.GetValue(); \
            } \
            else \
            { \
                return AZ::Failure(AZStd::string(result.GetError())); \
            }
#define TG_READ_STRING_FIELD(jsonName, member) \
            if (auto result = ReadStringField(object, jsonName); result.IsSuccess()) \
            { \
                metadata.member = result.GetValue(); \
            } \
            else \
            { \
                return AZ::Failure(AZStd::string(result.GetError())); \
            }
            TG_READ_U32_FIELD("width", m_grid.m_width)
            TG_READ_U32_FIELD("height", m_grid.m_height)
            TG_READ_STRING_FIELD("byte_order", m_byteOrder)
            TG_READ_DOUBLE_FIELD("sample_spacing_x_metres", m_grid.m_sampleSpacingXMetres)
            TG_READ_DOUBLE_FIELD("sample_spacing_y_metres", m_grid.m_sampleSpacingYMetres)
            TG_READ_DOUBLE_FIELD("min_height_metres", m_verticalMapping.m_minHeightMetres)
            TG_READ_DOUBLE_FIELD("max_height_metres", m_verticalMapping.m_maxHeightMetres)
            TG_READ_STRING_FIELD("handedness", m_coordinateSpace.m_handedness)
            TG_READ_STRING_FIELD("up_axis", m_coordinateSpace.m_upAxis)
            TG_READ_STRING_FIELD("forward_axis", m_coordinateSpace.m_forwardAxis)
            TG_READ_STRING_FIELD("row_zero_orientation", m_coordinateSpace.m_rowZeroOrientation)
            TG_READ_STRING_FIELD("sample_position", m_coordinateSpace.m_samplePosition)
#undef TG_READ_STRING_FIELD
#undef TG_READ_DOUBLE_FIELD
#undef TG_READ_U32_FIELD

            const QJsonValue transformValue = object.value(QStringLiteral(
                "source_to_canonical_transform"));
            if (!transformValue.isArray())
            {
                return AZ::Failure(AZStd::string(
                    "Terrain RAW sidecar requires source_to_canonical_transform as a 16-number array."));
            }
            const QJsonArray transform = transformValue.toArray();
            if (transform.size() != 16)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain RAW sidecar transform must contain exactly 16 numbers."));
            }
            metadata.m_coordinateSpace.m_sourceToCanonicalTransform.reserve(16);
            for (const QJsonValue& entry : transform)
            {
                if (!entry.isDouble() || !std::isfinite(entry.toDouble()))
                {
                    return AZ::Failure(AZStd::string(
                        "Terrain RAW sidecar transform values must be finite numbers."));
                }
                metadata.m_coordinateSpace.m_sourceToCanonicalTransform.push_back(
                    entry.toDouble());
            }

            if (metadata.m_byteOrder != "little-endian"
                && metadata.m_byteOrder != "big-endian")
            {
                return AZ::Failure(AZStd::string(
                    "Terrain RAW sidecar byte_order must be little-endian or big-endian."));
            }

            return AZ::Success(AZStd::move(metadata));
        }

        bool RawInputSuffixIsAllowed(const QString& path)
        {
            const QString suffix = QFileInfo(path).suffix().toLower();
            return suffix == QStringLiteral("raw")
                || suffix == QStringLiteral("u16")
                || suffix == QStringLiteral("r16");
        }

        bool RawInputSuffixIsProtectedOrUnsupported(const QString& path)
        {
            const QString suffix = QFileInfo(path).suffix().toLower();
            return suffix == QStringLiteral("assets")
                || suffix == QStringLiteral("ress")
                || suffix == QStringLiteral("bundle")
                || suffix == QStringLiteral("assetbundle")
                || suffix == QStringLiteral("unity")
                || suffix == QStringLiteral("exe")
                || suffix == QStringLiteral("dll")
                || suffix == QStringLiteral("sav")
                || suffix == QStringLiteral("save");
        }

        bool SidecarSuffixIsAllowed(const QString& path)
        {
            return QFileInfo(path).suffix().toLower() == QStringLiteral("json");
        }

        bool ImageInputSuffixIsAllowed(const QString& path)
        {
            const QString suffix = QFileInfo(path).suffix().toLower();
            return suffix == QStringLiteral("png")
                || suffix == QStringLiteral("tif")
                || suffix == QStringLiteral("tiff");
        }

        bool ImageFormatIsAllowed(const QByteArray& format)
        {
            const QByteArray folded = format.toLower();
            return folded == "png"
                || folded == "tif"
                || folded == "tiff";
        }

        AZ::u16 ReadUint16(const QByteArray& bytes, qsizetype offset, bool littleEndian)
        {
            const auto first = static_cast<AZ::u16>(static_cast<unsigned char>(bytes.at(offset)));
            const auto second = static_cast<AZ::u16>(static_cast<unsigned char>(bytes.at(offset + 1)));
            return littleEndian
                ? static_cast<AZ::u16>(first | (second << 8u))
                : static_cast<AZ::u16>((first << 8u) | second);
        }

        AZ::u32 ReadUint32(const QByteArray& bytes, qsizetype offset, bool littleEndian)
        {
            const auto first = static_cast<AZ::u32>(static_cast<unsigned char>(bytes.at(offset)));
            const auto second = static_cast<AZ::u32>(static_cast<unsigned char>(bytes.at(offset + 1)));
            const auto third = static_cast<AZ::u32>(static_cast<unsigned char>(bytes.at(offset + 2)));
            const auto fourth = static_cast<AZ::u32>(static_cast<unsigned char>(bytes.at(offset + 3)));
            return littleEndian
                ? first | (second << 8u) | (third << 16u) | (fourth << 24u)
                : (first << 24u) | (second << 16u) | (third << 8u) | fourth;
        }

        AZ::Outcome<AZStd::vector<AZ::u32>, AZStd::string> ReadTiffUnsignedValues(
            QFile& file,
            const QByteArray& entry,
            bool littleEndian,
            qint64 sourceSize)
        {
            constexpr AZ::u16 ShortType = 3;
            constexpr AZ::u16 LongType = 4;
            constexpr AZ::u32 MaximumTiffValueCount = TerrainHeightmapMaximumHeight;
            const AZ::u16 fieldType = ReadUint16(entry, 2, littleEndian);
            const AZ::u32 fieldCount = ReadUint32(entry, 4, littleEndian);
            if (fieldCount == 0 || fieldCount > MaximumTiffValueCount)
            {
                return AZ::Failure(AZStd::string("TIFF terrain image tag has an unsupported value count."));
            }

            qint64 fieldTypeBytes = 0;
            if (fieldType == ShortType)
            {
                fieldTypeBytes = 2;
            }
            else if (fieldType == LongType)
            {
                fieldTypeBytes = 4;
            }
            else
            {
                return AZ::Failure(AZStd::string("TIFF terrain image tags must use SHORT or LONG values."));
            }

            const qint64 valueByteCount = fieldTypeBytes * static_cast<qint64>(fieldCount);
            QByteArray valueBytes;
            if (valueByteCount <= 4)
            {
                valueBytes = entry.mid(8, static_cast<int>(valueByteCount));
            }
            else
            {
                const AZ::u32 valueOffset = ReadUint32(entry, 8, littleEndian);
                if (valueOffset < 8
                    || static_cast<qint64>(valueOffset) + valueByteCount > sourceSize
                    || !file.seek(static_cast<qint64>(valueOffset)))
                {
                    return AZ::Failure(AZStd::string("TIFF terrain image tag data is outside the selected local file."));
                }
                valueBytes = file.read(valueByteCount);
                if (valueBytes.size() != valueByteCount)
                {
                    return AZ::Failure(AZStd::string("Unable to read TIFF terrain image tag data."));
                }
            }

            AZStd::vector<AZ::u32> values;
            values.reserve(static_cast<size_t>(fieldCount));
            for (AZ::u32 index = 0; index < fieldCount; ++index)
            {
                const qsizetype valueOffset = static_cast<qsizetype>(index) * fieldTypeBytes;
                values.push_back(fieldType == ShortType
                    ? static_cast<AZ::u32>(ReadUint16(valueBytes, valueOffset, littleEndian))
                    : ReadUint32(valueBytes, valueOffset, littleEndian));
            }
            return AZ::Success(AZStd::move(values));
        }

        AZ::Outcome<AZ::u32, AZStd::string> ReadTiffScalarValue(
            QFile& file,
            const QByteArray& entry,
            bool littleEndian,
            qint64 sourceSize)
        {
            auto values = ReadTiffUnsignedValues(file, entry, littleEndian, sourceSize);
            if (!values.IsSuccess())
            {
                return AZ::Failure(AZStd::string(values.GetError()));
            }
            if (values.GetValue().size() != 1)
            {
                return AZ::Failure(AZStd::string("TIFF terrain image tag must be a scalar value."));
            }
            return AZ::Success(values.GetValue().front());
        }

        AZ::Outcome<ImageHeaderMetadata, AZStd::string> ReadImageHeaderMetadata(const FileSnapshot& snapshot)
        {
            QFile file(snapshot.m_canonicalPath);
            if (!file.open(QIODevice::ReadOnly))
            {
                return AZ::Failure(AZStd::string("Unable to open terrain image source for content validation."));
            }

            const QByteArray signature = file.read(8);
            if (signature.size() < 4)
            {
                return AZ::Failure(AZStd::string("Terrain image source is too small to identify as PNG or TIFF."));
            }

            static constexpr unsigned char PngSignature[8] =
            {
                0x89,
                0x50,
                0x4E,
                0x47,
                0x0D,
                0x0A,
                0x1A,
                0x0A
            };
            if (signature.size() >= 8
                && std::memcmp(signature.constData(), reinterpret_cast<const char*>(PngSignature), sizeof(PngSignature)) == 0)
            {
                file.seek(0);
                const QByteArray header = file.read(24);
                if (header.size() < 24
                    || ReadUint32(header, 8, false) != 13
                    || header.mid(12, 4) != "IHDR")
                {
                    return AZ::Failure(AZStd::string(
                        "Terrain image dimensions must be available before decoding and within schema bounds."));
                }

                ImageHeaderMetadata metadata;
                metadata.m_format = "png";
                metadata.m_width = ReadUint32(header, 16, false);
                metadata.m_height = ReadUint32(header, 20, false);
                return AZ::Success(metadata);
            }

            const unsigned char first = static_cast<unsigned char>(signature.at(0));
            const unsigned char second = static_cast<unsigned char>(signature.at(1));
            const unsigned char third = static_cast<unsigned char>(signature.at(2));
            const unsigned char fourth = static_cast<unsigned char>(signature.at(3));
            const bool littleEndianTiff = first == 'I' && second == 'I' && third == 0x2A && fourth == 0x00;
            const bool bigEndianTiff = first == 'M' && second == 'M' && third == 0x00 && fourth == 0x2A;
            if (littleEndianTiff || bigEndianTiff)
            {
                constexpr AZ::u16 MaximumTiffHeaderEntries = 256;
                const bool littleEndian = littleEndianTiff;
                const AZ::u32 ifdOffset = ReadUint32(signature, 4, littleEndian);
                if (ifdOffset < 8
                    || static_cast<qint64>(ifdOffset) + 2 > snapshot.m_size
                    || !file.seek(static_cast<qint64>(ifdOffset)))
                {
                    return AZ::Failure(AZStd::string(
                        "Terrain image dimensions must be available before decoding and within schema bounds."));
                }

                const QByteArray countBytes = file.read(2);
                if (countBytes.size() != 2)
                {
                    return AZ::Failure(AZStd::string(
                        "Terrain image dimensions must be available before decoding and within schema bounds."));
                }

                const AZ::u16 entryCount = ReadUint16(countBytes, 0, littleEndian);
                const qint64 entryTableBytes = static_cast<qint64>(entryCount) * 12;
                if (entryCount == 0
                    || entryCount > MaximumTiffHeaderEntries
                    || static_cast<qint64>(ifdOffset) + 2 + entryTableBytes + 4 > snapshot.m_size)
                {
                    return AZ::Failure(AZStd::string(
                        "Terrain image dimensions must be available before decoding and within schema bounds."));
                }

                const QByteArray entries = file.read(entryTableBytes);
                if (entries.size() != entryTableBytes)
                {
                    return AZ::Failure(AZStd::string(
                        "Terrain image dimensions must be available before decoding and within schema bounds."));
                }

                AZ::u32 width = 0;
                AZ::u32 height = 0;
                AZ::u32 bitsPerSample = 0;
                AZ::u32 compression = 1;
                AZ::u32 photometricInterpretation = 1;
                AZ::u32 samplesPerPixel = 1;
                AZ::u32 rowsPerStrip = 0;
                AZ::u32 sampleFormat = 1;
                AZStd::vector<AZ::u32> stripOffsets;
                AZStd::vector<AZ::u32> stripByteCounts;
                for (AZ::u16 entryIndex = 0; entryIndex < entryCount; ++entryIndex)
                {
                    const QByteArray entry = entries.mid(entryIndex * 12, 12);

                    const AZ::u16 tag = ReadUint16(entry, 0, littleEndian);
                    switch (tag)
                    {
                    case 256:
                    {
                        auto value = ReadTiffScalarValue(file, entry, littleEndian, snapshot.m_size);
                        if (!value.IsSuccess())
                        {
                            return AZ::Failure(AZStd::string(value.GetError()));
                        }
                        width = value.GetValue();
                        break;
                    }
                    case 257:
                    {
                        auto value = ReadTiffScalarValue(file, entry, littleEndian, snapshot.m_size);
                        if (!value.IsSuccess())
                        {
                            return AZ::Failure(AZStd::string(value.GetError()));
                        }
                        height = value.GetValue();
                        break;
                    }
                    case 258:
                    {
                        auto value = ReadTiffScalarValue(file, entry, littleEndian, snapshot.m_size);
                        if (!value.IsSuccess())
                        {
                            return AZ::Failure(AZStd::string(value.GetError()));
                        }
                        bitsPerSample = value.GetValue();
                        break;
                    }
                    case 259:
                    {
                        auto value = ReadTiffScalarValue(file, entry, littleEndian, snapshot.m_size);
                        if (!value.IsSuccess())
                        {
                            return AZ::Failure(AZStd::string(value.GetError()));
                        }
                        compression = value.GetValue();
                        break;
                    }
                    case 262:
                    {
                        auto value = ReadTiffScalarValue(file, entry, littleEndian, snapshot.m_size);
                        if (!value.IsSuccess())
                        {
                            return AZ::Failure(AZStd::string(value.GetError()));
                        }
                        photometricInterpretation = value.GetValue();
                        break;
                    }
                    case 273:
                    {
                        auto values = ReadTiffUnsignedValues(file, entry, littleEndian, snapshot.m_size);
                        if (!values.IsSuccess())
                        {
                            return AZ::Failure(AZStd::string(values.GetError()));
                        }
                        stripOffsets = values.TakeValue();
                        break;
                    }
                    case 277:
                    {
                        auto value = ReadTiffScalarValue(file, entry, littleEndian, snapshot.m_size);
                        if (!value.IsSuccess())
                        {
                            return AZ::Failure(AZStd::string(value.GetError()));
                        }
                        samplesPerPixel = value.GetValue();
                        break;
                    }
                    case 278:
                    {
                        auto value = ReadTiffScalarValue(file, entry, littleEndian, snapshot.m_size);
                        if (!value.IsSuccess())
                        {
                            return AZ::Failure(AZStd::string(value.GetError()));
                        }
                        rowsPerStrip = value.GetValue();
                        break;
                    }
                    case 279:
                    {
                        auto values = ReadTiffUnsignedValues(file, entry, littleEndian, snapshot.m_size);
                        if (!values.IsSuccess())
                        {
                            return AZ::Failure(AZStd::string(values.GetError()));
                        }
                        stripByteCounts = values.TakeValue();
                        break;
                    }
                    case 339:
                    {
                        auto value = ReadTiffScalarValue(file, entry, littleEndian, snapshot.m_size);
                        if (!value.IsSuccess())
                        {
                            return AZ::Failure(AZStd::string(value.GetError()));
                        }
                        sampleFormat = value.GetValue();
                        break;
                    }
                    default:
                        break;
                    }
                }

                if (width == 0
                    || height == 0
                    || bitsPerSample != 16
                    || compression != 1
                    || photometricInterpretation != 1
                    || samplesPerPixel != 1
                    || rowsPerStrip == 0
                    || sampleFormat != 1
                    || stripOffsets.empty()
                    || stripOffsets.size() != stripByteCounts.size())
                {
                    return AZ::Failure(AZStd::string(
                        "TIFF terrain image source must be uncompressed unsigned single-channel 16-bit grayscale with bounded strips."));
                }

                for (size_t stripIndex = 0; stripIndex < stripOffsets.size(); ++stripIndex)
                {
                    if (stripOffsets[stripIndex] < 8
                        || stripByteCounts[stripIndex] == 0
                        || static_cast<qint64>(stripOffsets[stripIndex])
                            + static_cast<qint64>(stripByteCounts[stripIndex])
                            > snapshot.m_size)
                    {
                        return AZ::Failure(AZStd::string(
                            "TIFF terrain image strip data is outside the selected local file."));
                    }
                }

                ImageHeaderMetadata metadata;
                metadata.m_format = "tif";
                metadata.m_width = width;
                metadata.m_height = height;
                metadata.m_tiffLittleEndian = littleEndian;
                metadata.m_tiffRowsPerStrip = rowsPerStrip;
                metadata.m_tiffStripOffsets = AZStd::move(stripOffsets);
                metadata.m_tiffStripByteCounts = AZStd::move(stripByteCounts);
                return AZ::Success(metadata);
            }

            return AZ::Failure(AZStd::string(
                "Terrain image import accepts only PNG or TIFF image content that matches the selected file suffix."));
        }

        bool ImageSuffixMatchesFormat(
            const QString& path,
            const QByteArray& format)
        {
            const QString suffix = QFileInfo(path).suffix().toLower();
            const QByteArray folded = format.toLower();
            if (suffix == QStringLiteral("png"))
            {
                return folded == "png";
            }
            return (suffix == QStringLiteral("tif")
                    || suffix == QStringLiteral("tiff"))
                && (folded == "tif" || folded == "tiff");
        }

        AZStd::string ImageSourceKindForPath(const QString& path)
        {
            const QString suffix = QFileInfo(path).suffix().toLower();
            return suffix == QStringLiteral("png")
                ? "user-exported-png16"
                : "user-exported-tiff16";
        }

        AZ::Outcome<QImage, AZStd::string> ReadBoundedU16TiffHeightmapImage(
            const FileSnapshot& snapshot,
            const ImageHeaderMetadata& metadata)
        {
            QFile file(snapshot.m_canonicalPath);
            if (!file.open(QIODevice::ReadOnly))
            {
                return AZ::Failure(AZStd::string("Unable to open TIFF terrain image source for decoding."));
            }

            QImage image(
                static_cast<int>(metadata.m_width),
                static_cast<int>(metadata.m_height),
                QImage::Format_Grayscale16);
            if (image.isNull())
            {
                return AZ::Failure(AZStd::string("Unable to allocate bounded TIFF terrain image buffer."));
            }

            AZ::u32 currentRow = 0;
            for (size_t stripIndex = 0; stripIndex < metadata.m_tiffStripOffsets.size(); ++stripIndex)
            {
                if (currentRow >= metadata.m_height)
                {
                    return AZ::Failure(AZStd::string("TIFF terrain image contains more strip rows than declared."));
                }

                const AZ::u32 stripRows = AZStd::min(
                    metadata.m_tiffRowsPerStrip,
                    metadata.m_height - currentRow);
                const AZ::u64 expectedStripBytes =
                    static_cast<AZ::u64>(stripRows) * metadata.m_width * 2u;
                if (metadata.m_tiffStripByteCounts[stripIndex] != expectedStripBytes)
                {
                    return AZ::Failure(AZStd::string(
                        "TIFF terrain image strip byte counts must exactly match unsigned 16-bit grayscale rows."));
                }

                if (!file.seek(static_cast<qint64>(metadata.m_tiffStripOffsets[stripIndex])))
                {
                    return AZ::Failure(AZStd::string("Unable to seek TIFF terrain image strip data."));
                }
                const QByteArray stripBytes = file.read(static_cast<qint64>(metadata.m_tiffStripByteCounts[stripIndex]));
                if (stripBytes.size() != static_cast<qint64>(metadata.m_tiffStripByteCounts[stripIndex]))
                {
                    return AZ::Failure(AZStd::string("Unable to read TIFF terrain image strip data."));
                }

                for (AZ::u32 stripRow = 0; stripRow < stripRows; ++stripRow)
                {
                    quint16* outputRow = reinterpret_cast<quint16*>(image.scanLine(static_cast<int>(currentRow + stripRow)));
                    const char* inputRow = stripBytes.constData()
                        + (static_cast<size_t>(stripRow) * metadata.m_width * 2u);
                    for (AZ::u32 column = 0; column < metadata.m_width; ++column)
                    {
                        const auto lowOrHigh = static_cast<AZ::u16>(
                            static_cast<unsigned char>(inputRow[(static_cast<size_t>(column) * 2u)]));
                        const auto highOrLow = static_cast<AZ::u16>(
                            static_cast<unsigned char>(inputRow[(static_cast<size_t>(column) * 2u) + 1u]));
                        outputRow[column] = metadata.m_tiffLittleEndian
                            ? static_cast<quint16>(lowOrHigh | (highOrLow << 8u))
                            : static_cast<quint16>((lowOrHigh << 8u) | highOrLow);
                    }
                }
                currentRow += stripRows;
            }

            if (currentRow != metadata.m_height)
            {
                return AZ::Failure(AZStd::string("TIFF terrain image strips do not cover the declared height."));
            }

            return AZ::Success(AZStd::move(image));
        }

        AZ::Outcome<QImage, AZStd::string> ReadBoundedU16HeightmapImage(
            const FileSnapshot& snapshot)
        {
            if (snapshot.m_size <= 0)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image source must be a non-empty direct local file."));
            }

            auto headerMetadata = ReadImageHeaderMetadata(snapshot);
            if (!headerMetadata.IsSuccess())
            {
                return AZ::Failure(AZStd::string(headerMetadata.GetError()));
            }

            if (!ImageFormatIsAllowed(headerMetadata.GetValue().m_format)
                || !ImageSuffixMatchesFormat(snapshot.m_canonicalPath, headerMetadata.GetValue().m_format))
            {
                AZStd::string message =
                    "Terrain image import accepts only PNG or TIFF image content that matches the selected file suffix.";
                message += " Detected format: ";
                message += headerMetadata.GetValue().m_format.isEmpty() ? "<none>" : headerMetadata.GetValue().m_format.constData();
                message += ", suffix: ";
                message += ToAzString(QFileInfo(snapshot.m_canonicalPath).suffix().toLower());
                message += ".";
                return AZ::Failure(message);
            }

            if (headerMetadata.GetValue().m_width == 0
                || headerMetadata.GetValue().m_height == 0
                || headerMetadata.GetValue().m_width > TerrainHeightmapMaximumWidth
                || headerMetadata.GetValue().m_height > TerrainHeightmapMaximumHeight)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image dimensions must be available before decoding and within schema bounds."));
            }

            Grid imageGrid;
            imageGrid.m_width = headerMetadata.GetValue().m_width;
            imageGrid.m_height = headerMetadata.GetValue().m_height;
            AZ::u64 totalSamples = 0;
            if (!CalculateTotalSamples(imageGrid, totalSamples))
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image dimensions exceed the schema sample-count bound."));
            }
            const AZ::u64 canonicalPayloadBytes = totalSamples * 2u;
            const AZ::u64 maximumContainerBytes =
                canonicalPayloadBytes + MaximumImageContainerOverheadBytes;
            if (static_cast<AZ::u64>(snapshot.m_size) > maximumContainerBytes)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image metadata or container overhead exceeds the bounded importer limit."));
            }

            QImage image;
            if (headerMetadata.GetValue().m_format.toLower() == "tif")
            {
                auto tiffImage = ReadBoundedU16TiffHeightmapImage(snapshot, headerMetadata.GetValue());
                if (!tiffImage.IsSuccess())
                {
                    return AZ::Failure(AZStd::string(tiffImage.GetError()));
                }
                image = tiffImage.TakeValue();
            }
            else
            {
                QImageReader reader(snapshot.m_canonicalPath);
                reader.setAutoTransform(false);
                reader.setDecideFormatFromContent(false);
                reader.setFormat(headerMetadata.GetValue().m_format);
                image = reader.read();
                if (image.isNull())
                {
                    return AZ::Failure(
                        AZStd::string("Unable to decode terrain image: ")
                        + ToAzString(reader.errorString()));
                }
            }

            if (image.width() != static_cast<int>(headerMetadata.GetValue().m_width)
                || image.height() != static_cast<int>(headerMetadata.GetValue().m_height))
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image dimensions changed between metadata inspection and decode."));
            }
            if (image.format() != QImage::Format_Grayscale16
                || image.depth() != 16)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image source must decode as a single-channel 16-bit grayscale image."));
            }
            if (!FileSnapshotIsUnchanged(snapshot))
            {
                return AZ::Failure(AZStd::string(
                    "The terrain image source changed during import; no document was published."));
            }
            return AZ::Success(AZStd::move(image));
        }

        AZ::Outcome<RawSidecarMetadata, AZStd::string> BuildImageMetadata(
            const ImageHeightmapImportRequest& request,
            const QImage& image)
        {
            if (image.width() <= 0
                || image.height() <= 0
                || image.width() > static_cast<int>(TerrainHeightmapMaximumWidth)
                || image.height() > static_cast<int>(TerrainHeightmapMaximumHeight))
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image dimensions are outside schema bounds."));
            }

            RawSidecarMetadata metadata;
            metadata.m_grid = request.m_gridMetadata;
            if ((metadata.m_grid.m_width != 0
                    && metadata.m_grid.m_width != static_cast<AZ::u32>(image.width()))
                || (metadata.m_grid.m_height != 0
                    && metadata.m_grid.m_height != static_cast<AZ::u32>(image.height())))
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image metadata dimensions must be empty or match the selected image."));
            }
            metadata.m_grid.m_width = static_cast<AZ::u32>(image.width());
            metadata.m_grid.m_height = static_cast<AZ::u32>(image.height());
            metadata.m_byteOrder = "little-endian";
            metadata.m_verticalMapping = request.m_verticalMapping;
            metadata.m_coordinateSpace = request.m_coordinateSpace;

            AZ::u64 totalSamples = 0;
            if (!CalculateTotalSamples(metadata.m_grid, totalSamples)
                || !std::isfinite(metadata.m_grid.m_sampleSpacingXMetres)
                || !std::isfinite(metadata.m_grid.m_sampleSpacingYMetres)
                || metadata.m_grid.m_sampleSpacingXMetres <= 0.0
                || metadata.m_grid.m_sampleSpacingYMetres <= 0.0)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image metadata requires bounded dimensions and positive finite sample spacing."));
            }
            if (!std::isfinite(metadata.m_verticalMapping.m_minHeightMetres)
                || !std::isfinite(metadata.m_verticalMapping.m_maxHeightMetres)
                || metadata.m_verticalMapping.m_maxHeightMetres
                    <= metadata.m_verticalMapping.m_minHeightMetres)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image metadata requires finite vertical min and max heights with max greater than min."));
            }
            if (!IsAllowedCoordinateValue(
                    metadata.m_coordinateSpace.m_handedness,
                    { "left-handed", "right-handed" })
                || !IsAllowedCoordinateValue(
                    metadata.m_coordinateSpace.m_upAxis,
                    { "x", "y", "z" })
                || !IsAllowedCoordinateValue(
                    metadata.m_coordinateSpace.m_forwardAxis,
                    { "x", "y", "z" })
                || metadata.m_coordinateSpace.m_upAxis
                    == metadata.m_coordinateSpace.m_forwardAxis
                || !IsAllowedCoordinateValue(
                    metadata.m_coordinateSpace.m_rowZeroOrientation,
                    { "north", "south", "east", "west" })
                || !IsAllowedCoordinateValue(
                    metadata.m_coordinateSpace.m_samplePosition,
                    { "cell-center", "grid-vertex" })
                || metadata.m_coordinateSpace.m_sourceToCanonicalTransform.size() != 16
                || !AZStd::all_of(
                    metadata.m_coordinateSpace.m_sourceToCanonicalTransform.begin(),
                    metadata.m_coordinateSpace.m_sourceToCanonicalTransform.end(),
                    [](double value)
                    {
                        return std::isfinite(value);
                    }))
            {
                return AZ::Failure(AZStd::string(
                    "Terrain image metadata requires known coordinate axes, row orientation, sample semantics, and a finite 4x4 transform."));
            }
            return AZ::Success(AZStd::move(metadata));
        }

        AZ::Outcome<AZStd::string, AZStd::string> GetSafeParentRelativePath(
            const AZStd::string& relativePath)
        {
            if (!IsSafeWorkspaceRelativePath(relativePath))
            {
                return AZ::Failure(AZStd::string(
                    "Terrain workspace file path must be a safe relative path."));
            }
            const size_t slash = relativePath.rfind('/');
            if (slash == AZStd::string::npos || slash == 0)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain workspace file path must include a contained parent directory."));
            }
            return AZ::Success(relativePath.substr(0, slash));
        }

        AZ::Outcome<AZStd::string, AZStd::string> GetRevisionRootFromTileRoot(
            const AZStd::string& tileRootRelativePath)
        {
            constexpr const char* TileRootSuffix = "/Tiles";
            const AZStd::string suffix(TileRootSuffix);
            if (!IsSafeWorkspaceRelativePath(tileRootRelativePath)
                || !EndsWith(tileRootRelativePath, TileRootSuffix)
                || tileRootRelativePath.size() <= suffix.size())
            {
                return AZ::Failure(AZStd::string(
                    "Terrain tile root must be a contained revision-relative tile directory."));
            }
            return AZ::Success(
                tileRootRelativePath.substr(
                    0,
                    tileRootRelativePath.size() - suffix.size()));
        }

        AZ::Outcome<AZ::u64, AZStd::string> CalculateExpectedRawBytes(const Grid& grid)
        {
            AZ::u64 totalSamples = 0;
            if (!CalculateTotalSamples(grid, totalSamples)
                || totalSamples > std::numeric_limits<AZ::u64>::max() / 2u)
            {
                return AZ::Failure(AZStd::string(
                    "Terrain RAW sidecar dimensions are outside schema bounds."));
            }
            return AZ::Success(totalSamples * 2u);
        }

        AZ::Outcome<void, AZStd::string> WriteBytesAtomically(
            const QString& filePath,
            const QByteArray& bytes)
        {
            QSaveFile file(filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                return AZ::Failure(AZStd::string("Unable to open terrain output for writing."));
            }
            if (file.write(bytes) != bytes.size() || !file.commit())
            {
                return AZ::Failure(AZStd::string("Unable to commit terrain output file."));
            }
            return AZ::Success();
        }

        AZ::Outcome<AZStd::string, AZStd::string> WriteTilePayload(
            const FileSnapshot& source,
            const Grid& grid,
            const Tile& tile,
            bool sourceBigEndian,
            const QString& outputPath)
        {
            QFile input(source.m_canonicalPath);
            if (!input.open(QIODevice::ReadOnly))
            {
                return AZ::Failure(AZStd::string("Unable to open terrain RAW source."));
            }
            QSaveFile output(outputPath);
            if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                return AZ::Failure(AZStd::string("Unable to open terrain tile for writing."));
            }

            QCryptographicHash hash(QCryptographicHash::Sha256);
            const qint64 rowBytes = static_cast<qint64>(tile.m_width) * 2;
            for (AZ::u32 row = 0; row < tile.m_height; ++row)
            {
                const AZ::u64 sourceOffset =
                    ((static_cast<AZ::u64>(tile.m_originY) + row)
                        * static_cast<AZ::u64>(grid.m_width)
                        + tile.m_originX)
                    * 2u;
                if (!input.seek(static_cast<qint64>(sourceOffset)))
                {
                    return AZ::Failure(AZStd::string(
                        "Unable to seek the terrain RAW source at a declared row boundary."));
                }
                QByteArray rowData = input.read(rowBytes);
                if (rowData.size() != rowBytes)
                {
                    return AZ::Failure(AZStd::string(
                        "Terrain RAW source ended before the declared grid was complete."));
                }
                if (sourceBigEndian)
                {
                    for (qsizetype byteIndex = 0; byteIndex + 1 < rowData.size(); byteIndex += 2)
                    {
                        const char high = rowData[byteIndex];
                        rowData[byteIndex] = rowData[byteIndex + 1];
                        rowData[byteIndex + 1] = high;
                    }
                }
                hash.addData(rowData);
                if (output.write(rowData) != rowData.size())
                {
                    return AZ::Failure(AZStd::string("Unable to write one terrain tile row."));
                }
            }
            if (!output.commit())
            {
                return AZ::Failure(AZStd::string("Unable to commit terrain tile payload."));
            }
            return AZ::Success(ToSha256Fingerprint(hash.result()));
        }

        AZStd::string TileRelativePath(AZ::u32 originX, AZ::u32 originY)
        {
            return AZStd::string::format(
                "Tiles/%08u-%08u.terrain.u16le",
                originY,
                originX);
        }

        AZStd::string TileId(
            const AZStd::string& operationId,
            AZ::u32 originX,
            AZ::u32 originY)
        {
            return AZStd::string::format(
                "terrain-tile.%s.%u-%u",
                operationId.c_str(),
                originY,
                originX);
        }

        AZ::Outcome<void, AZStd::string> WriteCanonicalTiles(
            const FileSnapshot& source,
            const RawSidecarMetadata& metadata,
            const AZStd::string& operationId,
            const QString& pendingTileRoot,
            AZStd::vector<Tile>& tiles)
        {
            tiles.clear();
            const bool sourceBigEndian = metadata.m_byteOrder == "big-endian";
            for (AZ::u32 originY = 0; originY < metadata.m_grid.m_height;
                 originY += TerrainHeightmapNominalTileSize)
            {
                const AZ::u32 tileHeight = AZStd::min(
                    TerrainHeightmapNominalTileSize,
                    metadata.m_grid.m_height - originY);
                for (AZ::u32 originX = 0; originX < metadata.m_grid.m_width;
                     originX += TerrainHeightmapNominalTileSize)
                {
                    const AZ::u32 tileWidth = AZStd::min(
                        TerrainHeightmapNominalTileSize,
                        metadata.m_grid.m_width - originX);
                    Tile tile;
                    tile.m_tileId = TileId(operationId, originX, originY);
                    tile.m_originX = originX;
                    tile.m_originY = originY;
                    tile.m_width = tileWidth;
                    tile.m_height = tileHeight;
                    tile.m_relativePath = TileRelativePath(originX, originY);
                    tile.m_byteSize = static_cast<AZ::u64>(tileWidth)
                        * static_cast<AZ::u64>(tileHeight)
                        * 2u;
                    const QString outputPath = QDir(pendingTileRoot).filePath(
                        QFileInfo(ToQString(tile.m_relativePath)).fileName());
                    auto hash = WriteTilePayload(
                        source,
                        metadata.m_grid,
                        tile,
                        sourceBigEndian,
                        outputPath);
                    if (!hash.IsSuccess())
                    {
                        return AZ::Failure(AZStd::string(hash.GetError()));
                    }
                    tile.m_sha256 = hash.TakeValue();
                    tiles.push_back(AZStd::move(tile));
                }
            }
            return AZ::Success();
        }

        AZ::Outcome<AZStd::string, AZStd::string> WriteImageTilePayload(
            const QImage& image,
            const Tile& tile,
            const QString& outputPath)
        {
            QSaveFile output(outputPath);
            if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                return AZ::Failure(AZStd::string("Unable to open terrain image tile for writing."));
            }

            QCryptographicHash hash(QCryptographicHash::Sha256);
            const qint64 rowBytes = static_cast<qint64>(tile.m_width) * 2;
            for (AZ::u32 row = 0; row < tile.m_height; ++row)
            {
                const quint16* sourceRow = reinterpret_cast<const quint16*>(
                    image.constScanLine(tile.m_originY + row));
                QByteArray rowData;
                rowData.resize(static_cast<int>(rowBytes));
                char* rowOutput = rowData.data();
                for (AZ::u32 column = 0; column < tile.m_width; ++column)
                {
                    const quint16 sample = sourceRow[tile.m_originX + column];
                    rowOutput[column * 2u] = static_cast<char>(sample & 0xffu);
                    rowOutput[(column * 2u) + 1u] =
                        static_cast<char>((sample >> 8u) & 0xffu);
                }
                hash.addData(rowData);
                if (output.write(rowData) != rowData.size())
                {
                    return AZ::Failure(AZStd::string("Unable to write one terrain image tile row."));
                }
            }
            if (!output.commit())
            {
                return AZ::Failure(AZStd::string("Unable to commit terrain image tile payload."));
            }
            return AZ::Success(ToSha256Fingerprint(hash.result()));
        }

        AZ::Outcome<void, AZStd::string> WriteCanonicalImageTiles(
            const QImage& image,
            const RawSidecarMetadata& metadata,
            const AZStd::string& operationId,
            const QString& pendingTileRoot,
            AZStd::vector<Tile>& tiles)
        {
            tiles.clear();
            for (AZ::u32 originY = 0; originY < metadata.m_grid.m_height;
                 originY += TerrainHeightmapNominalTileSize)
            {
                const AZ::u32 tileHeight = AZStd::min(
                    TerrainHeightmapNominalTileSize,
                    metadata.m_grid.m_height - originY);
                for (AZ::u32 originX = 0; originX < metadata.m_grid.m_width;
                     originX += TerrainHeightmapNominalTileSize)
                {
                    const AZ::u32 tileWidth = AZStd::min(
                        TerrainHeightmapNominalTileSize,
                        metadata.m_grid.m_width - originX);
                    Tile tile;
                    tile.m_tileId = TileId(operationId, originX, originY);
                    tile.m_originX = originX;
                    tile.m_originY = originY;
                    tile.m_width = tileWidth;
                    tile.m_height = tileHeight;
                    tile.m_relativePath = TileRelativePath(originX, originY);
                    tile.m_byteSize = static_cast<AZ::u64>(tileWidth)
                        * static_cast<AZ::u64>(tileHeight)
                        * 2u;
                    const QString outputPath = QDir(pendingTileRoot).filePath(
                        QFileInfo(ToQString(tile.m_relativePath)).fileName());
                    auto hash = WriteImageTilePayload(
                        image,
                        tile,
                        outputPath);
                    if (!hash.IsSuccess())
                    {
                        return AZ::Failure(AZStd::string(hash.GetError()));
                    }
                    tile.m_sha256 = hash.TakeValue();
                    tiles.push_back(AZStd::move(tile));
                }
            }
            return AZ::Success();
        }

        AZStd::string CalculateImageMetadataFingerprint(
            const RawSidecarMetadata& metadata)
        {
            AZStd::string json = "{";
            using namespace DeterministicContractJson;
            AppendString(json, "schema", "foa.terrain-heightmap.image-import-metadata");
            AppendUnsigned(json, "schema_version", 1);
            AppendUnsigned(json, "width", metadata.m_grid.m_width);
            AppendUnsigned(json, "height", metadata.m_grid.m_height);
            AppendDouble(
                json,
                "sample_spacing_x_metres",
                metadata.m_grid.m_sampleSpacingXMetres);
            AppendDouble(
                json,
                "sample_spacing_y_metres",
                metadata.m_grid.m_sampleSpacingYMetres);
            AppendDouble(
                json,
                "min_height_metres",
                metadata.m_verticalMapping.m_minHeightMetres);
            AppendDouble(
                json,
                "max_height_metres",
                metadata.m_verticalMapping.m_maxHeightMetres);
            AppendString(
                json,
                "handedness",
                metadata.m_coordinateSpace.m_handedness);
            AppendString(
                json,
                "up_axis",
                metadata.m_coordinateSpace.m_upAxis);
            AppendString(
                json,
                "forward_axis",
                metadata.m_coordinateSpace.m_forwardAxis);
            AppendString(
                json,
                "row_zero_orientation",
                metadata.m_coordinateSpace.m_rowZeroOrientation);
            AppendString(
                json,
                "sample_position",
                metadata.m_coordinateSpace.m_samplePosition);
            AppendName(json, "source_to_canonical_transform");
            json.push_back('[');
            for (size_t index = 0;
                 index < metadata.m_coordinateSpace.m_sourceToCanonicalTransform.size();
                 ++index)
            {
                if (index != 0)
                {
                    json.push_back(',');
                }
                json += DoubleString(
                    metadata.m_coordinateSpace.m_sourceToCanonicalTransform[index]);
            }
            json.push_back(']');
            json.push_back('}');
            return CalculateCanonicalSha256(json);
        }

        TerrainHeightmapDocumentV1 BuildDocument(
            const RawHeightmapImportRequest& request,
            const RawSidecarMetadata& metadata,
            const FileSnapshot& rawSource,
            const AZStd::string& sourceFingerprint,
            const AZStd::string& sidecarFingerprint,
            AZStd::vector<Tile> tiles)
        {
            TerrainHeightmapDocumentV1 document;
            document.m_documentId =
                "terrain-document." + request.m_mapIdentity.m_mapId;
            document.m_mapIdentity = request.m_mapIdentity;
            document.m_profileBinding = request.m_profileBinding;
            document.m_sourceBinding.m_sourceKind = metadata.m_byteOrder == "little-endian"
                ? "user-exported-raw-u16-le"
                : "user-exported-raw-u16-be";
            document.m_sourceBinding.m_sourceContainerSha256 = sourceFingerprint;
            document.m_sourceBinding.m_sourceObjectIdentifier =
                ToAzString(QFileInfo(rawSource.m_canonicalPath).fileName());
            document.m_sourceBinding.m_sourceSubresourceSha256.clear();
            document.m_sourceBinding.m_exporterId = request.m_importerId;
            document.m_sourceBinding.m_exporterVersion = request.m_importerVersion;
            document.m_sourceBinding.m_configurationFingerprint = sidecarFingerprint;
            document.m_sourceBinding.m_redactedRootToken = "source-root.user-selected";
            document.m_sourceBinding.m_relativeLocator =
                "SourceObservations/Terrain/"
                + request.m_operationId
                + "/source-observation.json";
            document.m_grid = metadata.m_grid;
            document.m_sampleEncoding.m_format = "u16";
            document.m_sampleEncoding.m_byteOrder = "little-endian";
            document.m_sampleEncoding.m_storageOrder = "row-major";
            document.m_sampleEncoding.m_bitsPerSample = 16;
            document.m_sampleEncoding.m_unsignedInteger = true;
            document.m_verticalMapping = metadata.m_verticalMapping;
            document.m_coordinateSpace = metadata.m_coordinateSpace;
            document.m_tiles = AZStd::move(tiles);
            document.m_provenance.m_createdAtUtc = request.m_createdAtUtc;
            document.m_provenance.m_importerId = request.m_importerId;
            document.m_provenance.m_importerVersion = request.m_importerVersion;
            document.m_provenance.m_sourceEvidenceId =
                "evidence.terrain." + request.m_operationId;
            document.m_provenance.m_limitations =
                "User-selected RAW U16 with mandatory sidecar; no game-source conversion or native map identity claim.";
            document.m_legalState = "user-exported-local-only";
            document.m_revision.m_revisionId =
                "terrain-revision." + request.m_operationId;
            document.m_revision.m_operationFingerprint = CalculateCanonicalSha256(
                sourceFingerprint
                + "\n"
                + sidecarFingerprint
                + "\n"
                + request.m_operationId);
            document.m_revision.m_createdAtUtc = request.m_createdAtUtc;
            document.m_localPayloadState = "workspace-local-derived";
            return document;
        }

        TerrainHeightmapDocumentV1 BuildImageDocument(
            const ImageHeightmapImportRequest& request,
            const RawSidecarMetadata& metadata,
            const FileSnapshot& imageSource,
            const AZStd::string& sourceFingerprint,
            const AZStd::string& metadataFingerprint,
            AZStd::vector<Tile> tiles)
        {
            TerrainHeightmapDocumentV1 document;
            document.m_documentId =
                "terrain-document." + request.m_mapIdentity.m_mapId;
            document.m_mapIdentity = request.m_mapIdentity;
            document.m_profileBinding = request.m_profileBinding;
            document.m_sourceBinding.m_sourceKind =
                ImageSourceKindForPath(imageSource.m_canonicalPath);
            document.m_sourceBinding.m_sourceContainerSha256 = sourceFingerprint;
            document.m_sourceBinding.m_sourceObjectIdentifier =
                ToAzString(QFileInfo(imageSource.m_canonicalPath).fileName());
            document.m_sourceBinding.m_sourceSubresourceSha256.clear();
            document.m_sourceBinding.m_exporterId = request.m_importerId;
            document.m_sourceBinding.m_exporterVersion = request.m_importerVersion;
            document.m_sourceBinding.m_configurationFingerprint = metadataFingerprint;
            document.m_sourceBinding.m_redactedRootToken = "source-root.user-selected";
            document.m_sourceBinding.m_relativeLocator =
                "SourceObservations/Terrain/"
                + request.m_operationId
                + "/source-observation.json";
            document.m_grid = metadata.m_grid;
            document.m_sampleEncoding.m_format = "u16";
            document.m_sampleEncoding.m_byteOrder = "little-endian";
            document.m_sampleEncoding.m_storageOrder = "row-major";
            document.m_sampleEncoding.m_bitsPerSample = 16;
            document.m_sampleEncoding.m_unsignedInteger = true;
            document.m_verticalMapping = metadata.m_verticalMapping;
            document.m_coordinateSpace = metadata.m_coordinateSpace;
            document.m_tiles = AZStd::move(tiles);
            document.m_provenance.m_createdAtUtc = request.m_createdAtUtc;
            document.m_provenance.m_importerId = request.m_importerId;
            document.m_provenance.m_importerVersion = request.m_importerVersion;
            document.m_provenance.m_sourceEvidenceId =
                "evidence.terrain." + request.m_operationId;
            document.m_provenance.m_limitations =
                "User-selected 16-bit grayscale PNG/TIFF with caller-supplied terrain metadata; no game-source conversion or native map identity claim.";
            document.m_legalState = "user-exported-local-only";
            document.m_revision.m_revisionId =
                "terrain-revision." + request.m_operationId;
            document.m_revision.m_operationFingerprint = CalculateCanonicalSha256(
                sourceFingerprint
                + "\n"
                + metadataFingerprint
                + "\n"
                + request.m_operationId);
            document.m_revision.m_createdAtUtc = request.m_createdAtUtc;
            document.m_localPayloadState = "workspace-local-derived";
            return document;
        }

        AZ::Outcome<void, AZStd::string> WriteSourceObservation(
            const RawHeightmapImportResult& result,
            const QString& sourceObservationPath)
        {
            AZStd::string json = "{";
            using namespace DeterministicContractJson;
            AppendString(json, "schema", "foa.terrain-source-observation");
            AppendUnsigned(json, "schema_version", 1);
            AppendString(json, "source_kind", result.m_document.m_sourceBinding.m_sourceKind);
            AppendString(json, "source_sha256", result.m_sourceFingerprint);
            AppendString(json, "sidecar_sha256", result.m_sidecarFingerprint);
            AppendUnsigned(json, "source_byte_size", result.m_sourceByteSize);
            AppendString(
                json,
                "source_object_identifier",
                result.m_document.m_sourceBinding.m_sourceObjectIdentifier);
            AppendString(
                json,
                "captured_at_utc",
                result.m_document.m_provenance.m_createdAtUtc,
                false);
            json.push_back('}');

            return WriteBytesAtomically(
                sourceObservationPath,
                QByteArray(json.data(), static_cast<int>(json.size())));
        }

        AZ::Outcome<void, AZStd::string> WriteSourceObservation(
            const ImageHeightmapImportResult& result,
            const QString& sourceObservationPath)
        {
            AZStd::string json = "{";
            using namespace DeterministicContractJson;
            AppendString(json, "schema", "foa.terrain-source-observation");
            AppendUnsigned(json, "schema_version", 1);
            AppendString(json, "source_kind", result.m_document.m_sourceBinding.m_sourceKind);
            AppendString(json, "source_sha256", result.m_sourceFingerprint);
            AppendString(json, "metadata_sha256", result.m_metadataFingerprint);
            AppendUnsigned(json, "source_byte_size", result.m_sourceByteSize);
            AppendString(
                json,
                "source_object_identifier",
                result.m_document.m_sourceBinding.m_sourceObjectIdentifier);
            AppendString(
                json,
                "captured_at_utc",
                result.m_document.m_provenance.m_createdAtUtc,
                false);
            json.push_back('}');

            return WriteBytesAtomically(
                sourceObservationPath,
                QByteArray(json.data(), static_cast<int>(json.size())));
        }

        bool HasDuplicateValues(AZStd::vector<AZStd::string> values)
        {
            AZStd::sort(values.begin(), values.end());
            return AZStd::adjacent_find(values.begin(), values.end()) != values.end();
        }

        bool IsAllowedAlias(const AZStd::string& value)
        {
            return value == "Horns of the South"
                || value == "Cuanacht / Cuanacht Village"
                || value == "Forlorn Swords"
                || value == "Sanctuary of Sarras / Sarras";
        }

        bool AliasesAreBounded(const MapIdentity& identity)
        {
            if (identity.m_publicAliases.size() > MaximumAliases
                || HasDuplicateValues(identity.m_publicAliases))
            {
                return false;
            }
            return AZStd::all_of(
                identity.m_publicAliases.begin(),
                identity.m_publicAliases.end(),
                [](const AZStd::string& alias)
                {
                    return IsBoundedText(alias, MaximumShortTextLength)
                        && IsAllowedAlias(alias);
                });
        }

        bool CalculateTotalSamples(const Grid& grid, AZ::u64& totalSamples)
        {
            if (grid.m_width == 0 || grid.m_height == 0)
            {
                totalSamples = 0;
                return false;
            }
            if (grid.m_width > TerrainHeightmapMaximumWidth
                || grid.m_height > TerrainHeightmapMaximumHeight)
            {
                totalSamples = 0;
                return false;
            }
            totalSamples = static_cast<AZ::u64>(grid.m_width)
                * static_cast<AZ::u64>(grid.m_height);
            return totalSamples <= TerrainHeightmapMaximumTotalSamples;
        }

        bool TileWithinGrid(const Tile& tile, const Grid& grid)
        {
            if (tile.m_width == 0 || tile.m_height == 0)
            {
                return false;
            }
            if (tile.m_originX > grid.m_width || tile.m_originY > grid.m_height)
            {
                return false;
            }
            return tile.m_width <= grid.m_width - tile.m_originX
                && tile.m_height <= grid.m_height - tile.m_originY;
        }

        bool TryCalculateTileByteSize(const Tile& tile, AZ::u64& expectedBytes)
        {
            expectedBytes = 0;
            constexpr AZ::u64 BytesPerSample = 2;
            const AZ::u64 width = tile.m_width;
            const AZ::u64 height = tile.m_height;
            if (height != 0
                && width > (std::numeric_limits<AZ::u64>::max() / height))
            {
                return false;
            }
            const AZ::u64 samples = width * height;
            if (samples > (std::numeric_limits<AZ::u64>::max() / BytesPerSample))
            {
                return false;
            }
            expectedBytes = samples * BytesPerSample;
            return true;
        }

        bool TilesOverlap(const Tile& left, const Tile& right)
        {
            const AZ::u64 leftEndX = static_cast<AZ::u64>(left.m_originX) + left.m_width;
            const AZ::u64 leftEndY = static_cast<AZ::u64>(left.m_originY) + left.m_height;
            const AZ::u64 rightEndX = static_cast<AZ::u64>(right.m_originX) + right.m_width;
            const AZ::u64 rightEndY = static_cast<AZ::u64>(right.m_originY) + right.m_height;

            return static_cast<AZ::u64>(left.m_originX) < rightEndX
                && leftEndX > right.m_originX
                && static_cast<AZ::u64>(left.m_originY) < rightEndY
                && leftEndY > right.m_originY;
        }

        bool IsRowMajorOrder(
            const Tile& previous,
            const Tile& current)
        {
            return previous.m_originY < current.m_originY
                || (previous.m_originY == current.m_originY
                    && previous.m_originX < current.m_originX);
        }

        AZStd::string TileLocator(size_t tileIndex)
        {
            return "tiles[" + DeterministicContractJson::UnsignedString(tileIndex) + "]";
        }

        void AppendDoubleArray(
            AZStd::string& output,
            const char* name,
            const AZStd::vector<double>& values,
            bool comma = true)
        {
            using namespace DeterministicContractJson;

            AppendName(output, name);
            output.push_back('[');
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                output += DoubleString(values[index]);
            }
            output.push_back(']');
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendProfileBinding(
            AZStd::string& output,
            const ProfileBinding& binding)
        {
            using namespace DeterministicContractJson;

            AppendName(output, "profile_binding");
            output.push_back('{');
            AppendString(output, "profile_id", binding.m_profileId);
            AppendString(output, "game_version", binding.m_gameVersion);
            AppendString(output, "branch", binding.m_branch);
            AppendString(output, "runtime_target", binding.m_runtimeTarget);
            AppendString(output, "profile_fingerprint", binding.m_profileFingerprint, false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendMapIdentity(
            AZStd::string& output,
            const MapIdentity& identity)
        {
            using namespace DeterministicContractJson;

            AppendName(output, "map_identity");
            output.push_back('{');
            AppendString(output, "map_id", identity.m_mapId);
            AppendString(output, "display_name", identity.m_displayName);
            AppendSortedStringArray(output, "public_aliases", identity.m_publicAliases);
            AppendString(
                output,
                "native_identity_evidence_id",
                identity.m_nativeIdentityEvidenceId,
                false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendSourceBinding(
            AZStd::string& output,
            const SourceBinding& binding)
        {
            using namespace DeterministicContractJson;

            AppendName(output, "source_binding");
            output.push_back('{');
            AppendString(output, "source_kind", binding.m_sourceKind);
            AppendString(output, "source_container_sha256", binding.m_sourceContainerSha256);
            AppendString(output, "source_object_identifier", binding.m_sourceObjectIdentifier);
            AppendString(output, "source_subresource_sha256", binding.m_sourceSubresourceSha256);
            AppendString(output, "exporter_id", binding.m_exporterId);
            AppendString(output, "exporter_version", binding.m_exporterVersion);
            AppendString(output, "configuration_fingerprint", binding.m_configurationFingerprint);
            AppendString(output, "redacted_root_token", binding.m_redactedRootToken);
            AppendString(output, "relative_locator", binding.m_relativeLocator, false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendGrid(
            AZStd::string& output,
            const Grid& grid)
        {
            using namespace DeterministicContractJson;

            AppendName(output, "grid");
            output.push_back('{');
            AppendUnsigned(output, "width", grid.m_width);
            AppendUnsigned(output, "height", grid.m_height);
            AppendDouble(output, "sample_spacing_x_metres", grid.m_sampleSpacingXMetres);
            AppendDouble(output, "sample_spacing_y_metres", grid.m_sampleSpacingYMetres, false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendSampleEncoding(
            AZStd::string& output,
            const SampleEncoding& encoding)
        {
            using namespace DeterministicContractJson;

            AppendName(output, "sample_encoding");
            output.push_back('{');
            AppendString(output, "format", encoding.m_format);
            AppendString(output, "byte_order", encoding.m_byteOrder);
            AppendString(output, "storage_order", encoding.m_storageOrder);
            AppendUnsigned(output, "bits_per_sample", encoding.m_bitsPerSample);
            AppendBool(output, "unsigned_integer", encoding.m_unsignedInteger, false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendVerticalMapping(
            AZStd::string& output,
            const VerticalMapping& mapping)
        {
            using namespace DeterministicContractJson;

            AppendName(output, "vertical_mapping");
            output.push_back('{');
            AppendDouble(output, "min_height_metres", mapping.m_minHeightMetres);
            AppendDouble(output, "max_height_metres", mapping.m_maxHeightMetres, false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendCoordinateSpace(
            AZStd::string& output,
            const CoordinateSpace& space)
        {
            using namespace DeterministicContractJson;

            AppendName(output, "coordinate_space");
            output.push_back('{');
            AppendString(output, "handedness", space.m_handedness);
            AppendString(output, "up_axis", space.m_upAxis);
            AppendString(output, "forward_axis", space.m_forwardAxis);
            AppendString(output, "row_zero_orientation", space.m_rowZeroOrientation);
            AppendString(output, "sample_position", space.m_samplePosition);
            AppendDoubleArray(
                output,
                "source_to_canonical_transform",
                space.m_sourceToCanonicalTransform,
                false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendTiles(
            AZStd::string& output,
            const AZStd::vector<Tile>& tiles)
        {
            using namespace DeterministicContractJson;

            AZStd::vector<const Tile*> sortedTiles;
            for (const Tile& tile : tiles)
            {
                sortedTiles.push_back(&tile);
            }
            AZStd::sort(
                sortedTiles.begin(),
                sortedTiles.end(),
                [](const Tile* left, const Tile* right)
                {
                    if (left->m_originY != right->m_originY)
                    {
                        return left->m_originY < right->m_originY;
                    }
                    if (left->m_originX != right->m_originX)
                    {
                        return left->m_originX < right->m_originX;
                    }
                    return left->m_relativePath < right->m_relativePath;
                });

            AppendName(output, "tiles");
            output.push_back('[');
            for (size_t index = 0; index < sortedTiles.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const Tile& tile = *sortedTiles[index];
                output.push_back('{');
                AppendString(output, "tile_id", tile.m_tileId);
                AppendUnsigned(output, "origin_x", tile.m_originX);
                AppendUnsigned(output, "origin_y", tile.m_originY);
                AppendUnsigned(output, "width", tile.m_width);
                AppendUnsigned(output, "height", tile.m_height);
                AppendString(output, "relative_path", tile.m_relativePath);
                AppendUnsigned(output, "byte_size", tile.m_byteSize);
                AppendString(output, "sha256", tile.m_sha256, false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back(',');
        }

        void AppendProvenance(
            AZStd::string& output,
            const Provenance& provenance)
        {
            using namespace DeterministicContractJson;

            AppendName(output, "provenance");
            output.push_back('{');
            AppendString(output, "created_at_utc", provenance.m_createdAtUtc);
            AppendString(output, "importer_id", provenance.m_importerId);
            AppendString(output, "importer_version", provenance.m_importerVersion);
            AppendString(output, "source_evidence_id", provenance.m_sourceEvidenceId);
            AppendString(output, "limitations", provenance.m_limitations, false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendRevision(
            AZStd::string& output,
            const Revision& revision)
        {
            using namespace DeterministicContractJson;

            AppendName(output, "revision");
            output.push_back('{');
            AppendString(output, "revision_id", revision.m_revisionId);
            AppendString(output, "parent_document_fingerprint", revision.m_parentDocumentFingerprint);
            AppendString(output, "operation_fingerprint", revision.m_operationFingerprint);
            AppendString(output, "created_at_utc", revision.m_createdAtUtc, false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendAuthority(
            AZStd::string& output,
            const Authority& authority)
        {
            using namespace DeterministicContractJson;

            AppendName(output, "authority");
            output.push_back('{');
            AppendBool(output, "runtime_use_allowed", authority.m_runtimeUseAllowed);
            AppendBool(output, "deployment_allowed", authority.m_deploymentAllowed);
            AppendBool(output, "publication_allowed", authority.m_publicationAllowed);
            AppendBool(output, "packaging_allowed", authority.m_packagingAllowed);
            AppendBool(output, "game_write_allowed", authority.m_gameWriteAllowed);
            AppendBool(
                output,
                "evidence_promotion_allowed",
                authority.m_evidencePromotionAllowed,
                false);
            output.push_back('}');
        }
    } // namespace

    bool IsSafeWorkspaceRelativePath(const AZStd::string& relativePath)
    {
        if (relativePath.empty()
            || relativePath.size() > MaximumRelativePathLength
            || relativePath.front() == '/'
            || relativePath.front() == '\\'
            || relativePath.find('\\') != AZStd::string::npos
            || relativePath.find(':') != AZStd::string::npos
            || relativePath.find("//") != AZStd::string::npos
            || relativePath.find("..") != AZStd::string::npos)
        {
            return false;
        }

        size_t segmentStart = 0;
        while (segmentStart < relativePath.size())
        {
            const size_t slash = relativePath.find('/', segmentStart);
            const size_t segmentEnd = slash == AZStd::string::npos
                ? relativePath.size()
                : slash;
            if (segmentEnd == segmentStart)
            {
                return false;
            }

            const AZStd::string segment =
                relativePath.substr(segmentStart, segmentEnd - segmentStart);
            if (segment == "." || segment == ".."
                || segment.back() == '.'
                || segment.back() == ' '
                || IsReservedWindowsDeviceSegment(segment))
            {
                return false;
            }
            for (char character : segment)
            {
                if (!IsAllowedPathCharacter(character))
                {
                    return false;
                }
            }

            if (slash == AZStd::string::npos)
            {
                break;
            }
            segmentStart = slash + 1;
        }
        return true;
    }

    ValidationResult ValidateDocument(const TerrainHeightmapDocumentV1& document)
    {
        ValidationResult result;

        if (document.m_schema != TerrainHeightmapSchemaId
            || document.m_schemaVersion != TerrainHeightmapSchemaVersion)
        {
            AddIssue(
                result,
                "$.schema",
                "schema.unsupported-version",
                "Terrain heightmap documents must use foa.terrain-heightmap schema version 1.");
        }

        if (!IsStableContractId(document.m_documentId)
            || !IsSafePersistenceId(document.m_mapIdentity.m_mapId)
            || !IsBoundedText(document.m_mapIdentity.m_displayName, MaximumShortTextLength)
            || document.m_mapIdentity.m_mapId == document.m_mapIdentity.m_displayName
            || !document.m_mapIdentity.m_nativeIdentityEvidenceId.empty()
            || !AliasesAreBounded(document.m_mapIdentity))
        {
            AddIssue(
                result,
                "$.map_identity",
                "identity.invalid",
                "Terrain map identity must use stable local IDs, bounded public aliases, and no native identity claim.");
        }

        if (!IsStableContractId(document.m_profileBinding.m_profileId)
            || !IsBoundedText(document.m_profileBinding.m_gameVersion, MaximumShortTextLength)
            || !IsBoundedText(document.m_profileBinding.m_branch, MaximumShortTextLength)
            || !IsSupportedRuntimeTarget(document.m_profileBinding.m_runtimeTarget)
            || !IsSha256Fingerprint(document.m_profileBinding.m_profileFingerprint))
        {
            AddIssue(
                result,
                "$.profile_binding",
                "profile.invalid",
                "Terrain heightmap documents require an exact sanitized profile binding.");
        }

        if (!IsAllowedSourceKind(document.m_sourceBinding.m_sourceKind)
            || !IsSha256Fingerprint(document.m_sourceBinding.m_sourceContainerSha256)
            || !IsSafeObjectIdentifier(document.m_sourceBinding.m_sourceObjectIdentifier)
            || !IsHexFingerprintOrEmpty(document.m_sourceBinding.m_sourceSubresourceSha256)
            || !IsStableContractId(document.m_sourceBinding.m_exporterId)
            || !IsBoundedText(document.m_sourceBinding.m_exporterVersion, MaximumShortTextLength)
            || !IsSha256Fingerprint(document.m_sourceBinding.m_configurationFingerprint)
            || !IsStableContractId(document.m_sourceBinding.m_redactedRootToken)
            || !IsSafeWorkspaceRelativePath(document.m_sourceBinding.m_relativeLocator))
        {
            AddIssue(
                result,
                "$.source_binding",
                "source.invalid",
                "Terrain source binding must describe one user-exported local input with only redacted relative locators.");
        }

        if (!CalculateTotalSamples(document.m_grid, result.m_totalSamples)
            || !std::isfinite(document.m_grid.m_sampleSpacingXMetres)
            || !std::isfinite(document.m_grid.m_sampleSpacingYMetres)
            || document.m_grid.m_sampleSpacingXMetres <= 0.0
            || document.m_grid.m_sampleSpacingYMetres <= 0.0)
        {
            AddIssue(
                result,
                "$.grid",
                "grid.invalid",
                "Terrain grid dimensions and sample spacing must be finite, non-zero, and within schema bounds.");
        }

        if (document.m_sampleEncoding.m_format != "u16"
            || document.m_sampleEncoding.m_byteOrder != "little-endian"
            || document.m_sampleEncoding.m_storageOrder != "row-major"
            || document.m_sampleEncoding.m_bitsPerSample != 16
            || !document.m_sampleEncoding.m_unsignedInteger)
        {
            AddIssue(
                result,
                "$.sample_encoding",
                "encoding.invalid",
                "Terrain payloads must be canonical unsigned 16-bit little-endian row-major samples.");
        }

        if (!std::isfinite(document.m_verticalMapping.m_minHeightMetres)
            || !std::isfinite(document.m_verticalMapping.m_maxHeightMetres)
            || document.m_verticalMapping.m_maxHeightMetres
                <= document.m_verticalMapping.m_minHeightMetres)
        {
            AddIssue(
                result,
                "$.vertical_mapping",
                "vertical.invalid",
                "Terrain vertical mapping requires finite min and max heights with max greater than min.");
        }

        if (!IsAllowedCoordinateValue(
                document.m_coordinateSpace.m_handedness,
                { "left-handed", "right-handed" })
            || !IsAllowedCoordinateValue(
                document.m_coordinateSpace.m_upAxis,
                { "x", "y", "z" })
            || !IsAllowedCoordinateValue(
                document.m_coordinateSpace.m_forwardAxis,
                { "x", "y", "z" })
            || document.m_coordinateSpace.m_upAxis
                == document.m_coordinateSpace.m_forwardAxis
            || !IsAllowedCoordinateValue(
                document.m_coordinateSpace.m_rowZeroOrientation,
                { "north", "south", "east", "west" })
            || !IsAllowedCoordinateValue(
                document.m_coordinateSpace.m_samplePosition,
                { "cell-center", "grid-vertex" })
            || document.m_coordinateSpace.m_sourceToCanonicalTransform.size() != 16
            || !AZStd::all_of(
                document.m_coordinateSpace.m_sourceToCanonicalTransform.begin(),
                document.m_coordinateSpace.m_sourceToCanonicalTransform.end(),
                [](double value)
                {
                    return std::isfinite(value);
                }))
        {
            AddIssue(
                result,
                "$.coordinate_space",
                "coordinates.invalid",
                "Terrain coordinate space requires known axes, row orientation, sample semantics, and a finite 4x4 transform.");
        }

        if (document.m_tiles.empty()
            || document.m_tiles.size() > TerrainHeightmapMaximumTileCount)
        {
            AddIssue(
                result,
                "$.tiles",
                "tiles.count",
                "Terrain documents require at least one tile and must stay within the tile-count bound.");
        }

        AZ::u64 coveredSamples = 0;
        AZStd::vector<AZStd::string> tileIds;
        AZStd::vector<AZStd::string> caseFoldedPaths;
        bool overlapFound = false;
        bool unorderedTiles = false;
        bool coverageOverflow = false;
        constexpr size_t MaximumTileCountToInspect =
            static_cast<size_t>(TerrainHeightmapMaximumTileCount);
        const size_t inspectedTileCount =
            document.m_tiles.size() > MaximumTileCountToInspect
            ? MaximumTileCountToInspect
            : document.m_tiles.size();
        for (size_t tileIndex = 0; tileIndex < inspectedTileCount; ++tileIndex)
        {
            const Tile& tile = document.m_tiles[tileIndex];
            const AZStd::string locator = TileLocator(tileIndex);
            tileIds.push_back(tile.m_tileId);
            caseFoldedPaths.push_back(FoldAscii(tile.m_relativePath));

            if (tileIndex > 0
                && !IsRowMajorOrder(document.m_tiles[tileIndex - 1], tile))
            {
                unorderedTiles = true;
            }

            AZ::u64 expectedBytes = 0;
            const bool byteSizeCanBeCalculated =
                TryCalculateTileByteSize(tile, expectedBytes);
            if (!IsStableContractId(tile.m_tileId)
                || !TileWithinGrid(tile, document.m_grid)
                || tile.m_width > TerrainHeightmapNominalTileSize
                || tile.m_height > TerrainHeightmapNominalTileSize
                || !IsSafeWorkspaceRelativePath(tile.m_relativePath)
                || !EndsWith(FoldAscii(tile.m_relativePath), ".terrain.u16le")
                || !byteSizeCanBeCalculated
                || tile.m_byteSize != expectedBytes
                || !IsSha256Fingerprint(tile.m_sha256))
            {
                AddIssue(
                    result,
                    locator,
                    "tile.invalid",
                    "Terrain tile metadata must be bounded, row-major U16 payload metadata with safe relative paths and hashes.");
            }
            if (TileWithinGrid(tile, document.m_grid))
            {
                const AZ::u64 tileSamples = static_cast<AZ::u64>(tile.m_width)
                    * static_cast<AZ::u64>(tile.m_height);
                if (coveredSamples > std::numeric_limits<AZ::u64>::max() - tileSamples)
                {
                    coverageOverflow = true;
                }
                else
                {
                    coveredSamples += tileSamples;
                }
            }
        }

        if (HasDuplicateValues(tileIds))
        {
            AddIssue(
                result,
                "$.tiles",
                "tile.duplicate-id",
                "Terrain tile identities must be unique.");
        }
        if (HasDuplicateValues(caseFoldedPaths))
        {
            AddIssue(
                result,
                "$.tiles",
                "tile.path-case-collision",
                "Terrain tile payload paths must not collide after Windows case folding.");
        }
        for (size_t leftIndex = 0; leftIndex < inspectedTileCount; ++leftIndex)
        {
            for (size_t rightIndex = leftIndex + 1; rightIndex < inspectedTileCount; ++rightIndex)
            {
                if (TilesOverlap(document.m_tiles[leftIndex], document.m_tiles[rightIndex]))
                {
                    overlapFound = true;
                }
            }
        }
        if (overlapFound)
        {
            AddIssue(
                result,
                "$.tiles",
                "tile.overlap",
                "Terrain tiles must not overlap.");
        }
        if (unorderedTiles)
        {
            AddIssue(
                result,
                "$.tiles",
                "tile.order",
                "Terrain tile inventory must be deterministic row-major order.");
        }
        if (coverageOverflow
            || (result.m_totalSamples != 0
                && coveredSamples != result.m_totalSamples))
        {
            AddIssue(
                result,
                "$.tiles",
                "tile.coverage",
                "Terrain tiles must cover the complete grid exactly once without gaps.");
        }

        if (!IsStrictUtcTimestamp(document.m_provenance.m_createdAtUtc)
            || !IsStableContractId(document.m_provenance.m_importerId)
            || !IsBoundedText(document.m_provenance.m_importerVersion, MaximumShortTextLength)
            || !IsStableContractId(document.m_provenance.m_sourceEvidenceId)
            || !IsBoundedText(document.m_provenance.m_limitations, MaximumLongTextLength, true))
        {
            AddIssue(
                result,
                "$.provenance",
                "provenance.invalid",
                "Terrain provenance requires stable importer, source evidence, timestamp, and bounded notes.");
        }

        if (document.m_legalState != "user-exported-local-only"
            || document.m_localPayloadState != "workspace-local-derived")
        {
            AddIssue(
                result,
                "$.legal_state",
                "authority.local-only-required",
                "Terrain heightmap documents must remain user-exported local-only workspace payloads.");
        }

        if (!IsSafePersistenceId(document.m_revision.m_revisionId)
            || !IsHexFingerprintOrEmpty(document.m_revision.m_parentDocumentFingerprint)
            || !IsSha256Fingerprint(document.m_revision.m_operationFingerprint)
            || !IsStrictUtcTimestamp(document.m_revision.m_createdAtUtc))
        {
            AddIssue(
                result,
                "$.revision",
                "revision.invalid",
                "Terrain revisions require stable IDs, valid fingerprints, and strict UTC timestamps.");
        }

        result.m_runtimeUseAllowed = document.m_authority.m_runtimeUseAllowed;
        result.m_deploymentAllowed = document.m_authority.m_deploymentAllowed;
        result.m_publicationAllowed = document.m_authority.m_publicationAllowed;
        result.m_packagingAllowed = document.m_authority.m_packagingAllowed;
        result.m_gameWriteAllowed = document.m_authority.m_gameWriteAllowed;
        result.m_evidencePromotionAllowed = document.m_authority.m_evidencePromotionAllowed;
        if (document.m_authority.m_runtimeUseAllowed
            || document.m_authority.m_deploymentAllowed
            || document.m_authority.m_publicationAllowed
            || document.m_authority.m_packagingAllowed
            || document.m_authority.m_gameWriteAllowed
            || document.m_authority.m_evidencePromotionAllowed)
        {
            AddIssue(
                result,
                "$.authority",
                "authority.forbidden",
                "Terrain schema version 1 cannot grant runtime, deployment, publication, packaging, game-write, or evidence-promotion authority.");
        }

        result.m_tileCount = document.m_tiles.size();
        result.m_accepted = result.m_issues.empty();
        if (result.m_accepted)
        {
            result.m_canonicalFingerprint = CalculateDocumentFingerprint(document);
        }
        return result;
    }

    AZStd::string BuildCanonicalDocumentJson(const TerrainHeightmapDocumentV1& document)
    {
        using namespace DeterministicContractJson;

        AZStd::string output = "{";
        AppendString(output, "schema", document.m_schema);
        AppendUnsigned(output, "schema_version", document.m_schemaVersion);
        AppendString(output, "document_id", document.m_documentId);
        AppendMapIdentity(output, document.m_mapIdentity);
        AppendProfileBinding(output, document.m_profileBinding);
        AppendSourceBinding(output, document.m_sourceBinding);
        AppendGrid(output, document.m_grid);
        AppendSampleEncoding(output, document.m_sampleEncoding);
        AppendVerticalMapping(output, document.m_verticalMapping);
        AppendCoordinateSpace(output, document.m_coordinateSpace);
        AppendTiles(output, document.m_tiles);
        AppendProvenance(output, document.m_provenance);
        AppendString(output, "legal_state", document.m_legalState);
        AppendRevision(output, document.m_revision);
        AppendString(output, "local_payload_state", document.m_localPayloadState);
        AppendAuthority(output, document.m_authority);
        output.push_back('}');
        return output;
    }

    AZStd::string CalculateDocumentFingerprint(const TerrainHeightmapDocumentV1& document)
    {
        return CalculateCanonicalSha256(BuildCanonicalDocumentJson(document));
    }

    ValidationResult BuildWorkspaceStagingPlan(
        const TerrainHeightmapDocumentV1& document,
        const AZStd::string& operationId,
        WorkspaceStagingPlan& plan)
    {
        ValidationResult result = ValidateDocument(document);
        plan = {};
        if (!IsSafePersistenceId(operationId))
        {
            AddIssue(
                result,
                "$.operation_id",
                "staging.operation-id",
                "Terrain staging operations require a stable persistence-safe operation ID.");
        }
        if (!result.m_accepted || !result.m_issues.empty())
        {
            result.m_accepted = false;
            return result;
        }

        const AZStd::string documentFingerprint = result.m_canonicalFingerprint.substr(7, 16);
        const AZStd::string terrainRoot = "Derived/Terrain/"
            + document.m_mapIdentity.m_mapId
            + "/Revisions/"
            + document.m_revision.m_revisionId
            + "/"
            + documentFingerprint;
        const AZStd::string stagingRoot = "Staging/Terrain/"
            + operationId
            + "/"
            + documentFingerprint;

        plan.m_operationId = operationId;
        plan.m_stagingManifestRelativePath = stagingRoot + "/terrain.tgheightmap.json";
        plan.m_stagingTileRootRelativePath = stagingRoot + "/Tiles";
        plan.m_publishedManifestRelativePath = terrainRoot + "/terrain.tgheightmap.json";
        plan.m_publishedTileRootRelativePath = terrainRoot + "/Tiles";
        plan.m_sourceObservationRelativePath =
            "SourceObservations/Terrain/"
            + operationId
            + "/source-observation.json";

        if (!IsSafeWorkspaceRelativePath(plan.m_stagingManifestRelativePath)
            || !IsSafeWorkspaceRelativePath(plan.m_stagingTileRootRelativePath)
            || !IsSafeWorkspaceRelativePath(plan.m_publishedManifestRelativePath)
            || !IsSafeWorkspaceRelativePath(plan.m_publishedTileRootRelativePath)
            || !IsSafeWorkspaceRelativePath(plan.m_sourceObservationRelativePath))
        {
            AddIssue(
                result,
                "$.staging",
                "staging.path",
                "Terrain staging paths must remain contained workspace-relative paths.");
            result.m_accepted = false;
        }
        return result;
    }

    PackageGuardResult ValidateTerrainPackagePath(const AZStd::string& relativePath)
    {
        PackageGuardResult result;
        if (!IsSafeWorkspaceRelativePath(relativePath))
        {
            AddPackageIssue(
                result,
                relativePath,
                "package.path",
                "Package candidates must be safe workspace-relative paths.");
            return result;
        }

        const AZStd::string folded = FoldAscii(relativePath);
        if (StartsWith(folded, "derived/terrain/")
            || StartsWith(folded, "staging/terrain/")
            || StartsWith(folded, "sourceobservations/terrain/")
            || EndsWith(folded, ".tgheightmap.json")
            || EndsWith(folded, ".terrain.u16le")
            || folded.find("_gsi") != AZStd::string::npos)
        {
            AddPackageIssue(
                result,
                relativePath,
                "package.local-terrain-blocked",
                "Local terrain manifests, payloads, source observations, and preview projections are excluded from release packages in schema version 1.");
            return result;
        }

        result.m_allowed = true;
        return result;
    }

    AZ::Outcome<RawHeightmapImportResult, AZStd::string> ImportRawHeightmapToWorkspace(
        const RawHeightmapImportRequest& request)
    {
        if (!IsSafePersistenceId(request.m_operationId)
            || !IsStrictUtcTimestamp(request.m_createdAtUtc)
            || !IsStableContractId(request.m_importerId)
            || !IsBoundedText(request.m_importerVersion, MaximumShortTextLength))
        {
            return AZ::Failure(AZStd::string(
                "Terrain RAW import requires a safe operation ID, strict UTC timestamp, and stable importer identity."));
        }

        auto workspaceRoot = ResolveWorkspaceRoot(request.m_workspaceRoot);
        if (!workspaceRoot.IsSuccess())
        {
            return AZ::Failure(AZStd::string(workspaceRoot.GetError()));
        }

        auto rawSource = ResolveDirectCanonicalFile(
            request.m_rawInputPath,
            "Terrain RAW source");
        if (!rawSource.IsSuccess())
        {
            return AZ::Failure(AZStd::string(rawSource.GetError()));
        }
        auto sidecar = ResolveDirectCanonicalFile(
            request.m_sidecarPath,
            "Terrain RAW sidecar");
        if (!sidecar.IsSuccess())
        {
            return AZ::Failure(AZStd::string(sidecar.GetError()));
        }
        if (PathsIdentifySameLocation(
                rawSource.GetValue().m_canonicalPath,
                sidecar.GetValue().m_canonicalPath))
        {
            return AZ::Failure(AZStd::string(
                "Terrain RAW source and sidecar must be separate files."));
        }
        if (!SidecarSuffixIsAllowed(sidecar.GetValue().m_canonicalPath))
        {
            return AZ::Failure(AZStd::string(
                "Terrain RAW sidecar must be an explicit JSON sidecar file."));
        }
        if (RawInputSuffixIsProtectedOrUnsupported(rawSource.GetValue().m_canonicalPath)
            || !RawInputSuffixIsAllowed(rawSource.GetValue().m_canonicalPath))
        {
            return AZ::Failure(AZStd::string(
                "Terrain RAW import accepts only user-selected .raw, .u16, or .r16 files; protected game, Unity, executable, save, and bundle inputs are prohibited."));
        }

        AZStd::string sidecarFingerprint;
        auto sidecarData = ReadBoundedSidecar(sidecar.GetValue(), sidecarFingerprint);
        if (!sidecarData.IsSuccess())
        {
            return AZ::Failure(AZStd::string(sidecarData.GetError()));
        }
        auto metadata = ParseRawSidecar(sidecarData.GetValue());
        if (!metadata.IsSuccess())
        {
            return AZ::Failure(AZStd::string(metadata.GetError()));
        }

        auto expectedRawBytes = CalculateExpectedRawBytes(metadata.GetValue().m_grid);
        if (!expectedRawBytes.IsSuccess())
        {
            return AZ::Failure(AZStd::string(expectedRawBytes.GetError()));
        }
        if (static_cast<AZ::u64>(rawSource.GetValue().m_size) != expectedRawBytes.GetValue())
        {
            return AZ::Failure(AZStd::string(
                "Terrain RAW source byte size does not exactly match the sidecar dimensions and U16 sample size."));
        }

        AZ::u64 sourceByteSize = 0;
        auto sourceFingerprint = HashFile(rawSource.GetValue(), sourceByteSize);
        if (!sourceFingerprint.IsSuccess())
        {
            return AZ::Failure(AZStd::string(sourceFingerprint.GetError()));
        }

        const QString canonicalWorkspaceRoot = workspaceRoot.GetValue();
        const AZStd::string pendingRootRelativePath =
            "Staging/Terrain/" + request.m_operationId + ".pending";
        auto pendingRoot = EnsureContainedDirectory(
            canonicalWorkspaceRoot,
            pendingRootRelativePath);
        if (!pendingRoot.IsSuccess())
        {
            return AZ::Failure(AZStd::string(pendingRoot.GetError()));
        }
        if (!QDir(pendingRoot.GetValue()).removeRecursively()
            || !QDir().mkpath(pendingRoot.GetValue()))
        {
            return AZ::Failure(AZStd::string(
                "Unable to reset contained pending terrain staging state."));
        }
        const QString canonicalPendingRoot =
            ResolveDirectCanonicalDirectory(pendingRoot.GetValue());
        if (canonicalPendingRoot.isEmpty()
            || !IsContainedPath(canonicalWorkspaceRoot, canonicalPendingRoot))
        {
            return AZ::Failure(AZStd::string(
                "Pending terrain staging root did not retain contained canonical identity."));
        }

        const QString pendingTileRoot =
            QDir(canonicalPendingRoot).filePath(QStringLiteral("Tiles"));
        if (!QDir().mkpath(pendingTileRoot))
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(
                "Unable to create contained pending terrain tile directory."));
        }

        AZStd::vector<Tile> tiles;
        auto tileWrite = WriteCanonicalTiles(
            rawSource.GetValue(),
            metadata.GetValue(),
            request.m_operationId,
            pendingTileRoot,
            tiles);
        if (!tileWrite.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(tileWrite.GetError()));
        }
        if (!FileSnapshotIsUnchanged(rawSource.GetValue())
            || !FileSnapshotIsUnchanged(sidecar.GetValue()))
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(
                "The terrain RAW source or sidecar changed during import; no document was published."));
        }

        RawHeightmapImportResult result;
        result.m_sourceFingerprint = sourceFingerprint.TakeValue();
        result.m_sidecarFingerprint = sidecarFingerprint;
        result.m_sourceByteSize = sourceByteSize;
        result.m_document = BuildDocument(
            request,
            metadata.GetValue(),
            rawSource.GetValue(),
            result.m_sourceFingerprint,
            result.m_sidecarFingerprint,
            AZStd::move(tiles));

        auto validation = BuildWorkspaceStagingPlan(
            result.m_document,
            request.m_operationId,
            result.m_stagingPlan);
        if (!validation.m_accepted)
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            AZStd::string message = "Terrain RAW import produced an invalid document.";
            if (!validation.m_issues.empty())
            {
                message += " First issue: ";
                message += validation.m_issues.front().m_code;
            }
            return AZ::Failure(message);
        }

        auto stagingRootRelativePath = GetRevisionRootFromTileRoot(
            result.m_stagingPlan.m_stagingTileRootRelativePath);
        if (!stagingRootRelativePath.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(stagingRootRelativePath.GetError()));
        }
        auto stagingRoot = EnsureContainedDirectory(
            canonicalWorkspaceRoot,
            stagingRootRelativePath.GetValue());
        if (!stagingRoot.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(stagingRoot.GetError()));
        }
        const QString canonicalStagingRoot = stagingRoot.GetValue();
        if (QFileInfo::exists(canonicalStagingRoot)
            && !PathsIdentifySameLocation(canonicalStagingRoot, canonicalPendingRoot)
            && QDir(canonicalStagingRoot).entryInfoList(
                   QDir::NoDotAndDotDot | QDir::AllEntries).size() != 0)
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(
                "Terrain staging destination already contains state for this operation."));
        }
        if (!PathsIdentifySameLocation(canonicalStagingRoot, canonicalPendingRoot))
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            const QString stagingParent = QFileInfo(canonicalStagingRoot).absolutePath();
            if (!QDir().mkpath(stagingParent)
                || !QDir().rename(canonicalPendingRoot, canonicalStagingRoot))
            {
                RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
                return AZ::Failure(AZStd::string(
                    "Unable to promote pending terrain files into the contained staging root."));
            }
        }

        const QString stagingManifestPath = QDir(canonicalStagingRoot).filePath(
            QFileInfo(ToQString(result.m_stagingPlan.m_stagingManifestRelativePath)).fileName());
        const AZStd::string manifestJson = BuildCanonicalDocumentJson(result.m_document);
        auto manifestWrite = WriteBytesAtomically(
            stagingManifestPath,
            QByteArray(
                manifestJson.data(),
                static_cast<int>(manifestJson.size())));
        if (!manifestWrite.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(manifestWrite.GetError()));
        }

        auto publishedRootRelativePath = GetSafeParentRelativePath(
            result.m_stagingPlan.m_publishedManifestRelativePath);
        if (!publishedRootRelativePath.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(publishedRootRelativePath.GetError()));
        }
        auto publishedParentRelativePath = GetSafeParentRelativePath(
            publishedRootRelativePath.GetValue());
        if (!publishedParentRelativePath.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(publishedParentRelativePath.GetError()));
        }
        auto publishedParent = EnsureContainedDirectory(
            canonicalWorkspaceRoot,
            publishedParentRelativePath.GetValue());
        if (!publishedParent.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(publishedParent.GetError()));
        }
        const QString publishedRoot = QDir(publishedParent.GetValue()).filePath(
            QFileInfo(ToQString(publishedRootRelativePath.GetValue())).fileName());
        if (QFileInfo::exists(publishedRoot))
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(
                "Published terrain revision already exists; import will not overwrite it."));
        }

        auto sourceObservationRootRelativePath = GetSafeParentRelativePath(
            result.m_stagingPlan.m_sourceObservationRelativePath);
        if (!sourceObservationRootRelativePath.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(sourceObservationRootRelativePath.GetError()));
        }
        const QString sourceObservationRootDeclared = QDir(canonicalWorkspaceRoot).filePath(
            ToQString(sourceObservationRootRelativePath.GetValue()));
        const bool sourceObservationRootPreexisting =
            QFileInfo::exists(sourceObservationRootDeclared);
        auto sourceObservationRoot = EnsureContainedDirectory(
            canonicalWorkspaceRoot,
            sourceObservationRootRelativePath.GetValue());
        if (!sourceObservationRoot.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(sourceObservationRoot.GetError()));
        }
        const QString sourceObservationPath = QDir(sourceObservationRoot.GetValue()).filePath(
            QFileInfo(ToQString(result.m_stagingPlan.m_sourceObservationRelativePath)).fileName());
        auto observationWrite = WriteSourceObservation(result, sourceObservationPath);
        if (!observationWrite.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            if (!sourceObservationRootPreexisting)
            {
                RemoveContainedDirectory(canonicalWorkspaceRoot, sourceObservationRoot.GetValue());
            }
            return AZ::Failure(AZStd::string(observationWrite.GetError()));
        }

        if (!IsContainedPath(canonicalWorkspaceRoot, publishedRoot)
            || !QDir().rename(canonicalStagingRoot, publishedRoot))
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            if (!sourceObservationRootPreexisting)
            {
                RemoveContainedDirectory(canonicalWorkspaceRoot, sourceObservationRoot.GetValue());
            }
            return AZ::Failure(AZStd::string(
                "Unable to atomically publish the terrain revision from staging."));
        }
        const QString canonicalPublishedRoot =
            ResolveDirectCanonicalDirectory(publishedRoot);
        if (canonicalPublishedRoot.isEmpty()
            || !IsContainedPath(canonicalWorkspaceRoot, canonicalPublishedRoot))
        {
            return AZ::Failure(AZStd::string(
                "Published terrain revision does not retain contained canonical identity."));
        }

        result.m_publishedManifestPath = ToAzString(
            QDir(canonicalPublishedRoot).filePath(
                QFileInfo(ToQString(result.m_stagingPlan.m_publishedManifestRelativePath)).fileName()));
        result.m_sourceObservationPath = ToAzString(sourceObservationPath);
        result.m_tileCount = result.m_document.m_tiles.size();
        for (const Tile& tile : result.m_document.m_tiles)
        {
            result.m_publishedTilePaths.push_back(ToAzString(
                QDir(canonicalPublishedRoot).filePath(
                    ToQString(tile.m_relativePath))));
        }
        return AZ::Success(AZStd::move(result));
    }

    AZ::Outcome<ImageHeightmapImportResult, AZStd::string> ImportImageHeightmapToWorkspace(
        const ImageHeightmapImportRequest& request)
    {
        if (!IsSafePersistenceId(request.m_operationId)
            || !IsStrictUtcTimestamp(request.m_createdAtUtc)
            || !IsStableContractId(request.m_importerId)
            || !IsBoundedText(request.m_importerVersion, MaximumShortTextLength))
        {
            return AZ::Failure(AZStd::string(
                "Terrain image import requires a safe operation ID, strict UTC timestamp, and stable importer identity."));
        }

        auto workspaceRoot = ResolveWorkspaceRoot(request.m_workspaceRoot);
        if (!workspaceRoot.IsSuccess())
        {
            return AZ::Failure(AZStd::string(workspaceRoot.GetError()));
        }

        auto imageSource = ResolveDirectCanonicalFile(
            request.m_imageInputPath,
            "Terrain image source");
        if (!imageSource.IsSuccess())
        {
            return AZ::Failure(AZStd::string(imageSource.GetError()));
        }
        if (RawInputSuffixIsProtectedOrUnsupported(imageSource.GetValue().m_canonicalPath)
            || !ImageInputSuffixIsAllowed(imageSource.GetValue().m_canonicalPath))
        {
            return AZ::Failure(AZStd::string(
                "Terrain image import accepts only user-selected .png, .tif, or .tiff files; protected game, Unity, executable, save, and bundle inputs are prohibited."));
        }
        if (!IsSafeObjectIdentifier(ToAzString(
                QFileInfo(imageSource.GetValue().m_canonicalPath).fileName())))
        {
            return AZ::Failure(AZStd::string(
                "Terrain image source file name must be a bounded local object identifier."));
        }

        AZ::u64 sourceByteSize = 0;
        auto sourceFingerprint = HashFile(imageSource.GetValue(), sourceByteSize);
        if (!sourceFingerprint.IsSuccess())
        {
            return AZ::Failure(AZStd::string(sourceFingerprint.GetError()));
        }

        auto image = ReadBoundedU16HeightmapImage(imageSource.GetValue());
        if (!image.IsSuccess())
        {
            return AZ::Failure(AZStd::string(image.GetError()));
        }
        auto metadata = BuildImageMetadata(request, image.GetValue());
        if (!metadata.IsSuccess())
        {
            return AZ::Failure(AZStd::string(metadata.GetError()));
        }

        const QString canonicalWorkspaceRoot = workspaceRoot.GetValue();
        const AZStd::string pendingRootRelativePath =
            "Staging/Terrain/" + request.m_operationId + ".pending";
        auto pendingRoot = EnsureContainedDirectory(
            canonicalWorkspaceRoot,
            pendingRootRelativePath);
        if (!pendingRoot.IsSuccess())
        {
            return AZ::Failure(AZStd::string(pendingRoot.GetError()));
        }
        if (!QDir(pendingRoot.GetValue()).removeRecursively()
            || !QDir().mkpath(pendingRoot.GetValue()))
        {
            return AZ::Failure(AZStd::string(
                "Unable to reset contained pending terrain staging state."));
        }
        const QString canonicalPendingRoot =
            ResolveDirectCanonicalDirectory(pendingRoot.GetValue());
        if (canonicalPendingRoot.isEmpty()
            || !IsContainedPath(canonicalWorkspaceRoot, canonicalPendingRoot))
        {
            return AZ::Failure(AZStd::string(
                "Pending terrain staging root did not retain contained canonical identity."));
        }

        const QString pendingTileRoot =
            QDir(canonicalPendingRoot).filePath(QStringLiteral("Tiles"));
        if (!QDir().mkpath(pendingTileRoot))
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(
                "Unable to create contained pending terrain tile directory."));
        }

        AZStd::vector<Tile> tiles;
        auto tileWrite = WriteCanonicalImageTiles(
            image.GetValue(),
            metadata.GetValue(),
            request.m_operationId,
            pendingTileRoot,
            tiles);
        if (!tileWrite.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(tileWrite.GetError()));
        }
        if (!FileSnapshotIsUnchanged(imageSource.GetValue()))
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(
                "The terrain image source changed during import; no document was published."));
        }

        ImageHeightmapImportResult result;
        result.m_sourceFingerprint = sourceFingerprint.TakeValue();
        result.m_metadataFingerprint = CalculateImageMetadataFingerprint(metadata.GetValue());
        result.m_sourceByteSize = sourceByteSize;
        result.m_document = BuildImageDocument(
            request,
            metadata.GetValue(),
            imageSource.GetValue(),
            result.m_sourceFingerprint,
            result.m_metadataFingerprint,
            AZStd::move(tiles));

        auto validation = BuildWorkspaceStagingPlan(
            result.m_document,
            request.m_operationId,
            result.m_stagingPlan);
        if (!validation.m_accepted)
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            AZStd::string message = "Terrain image import produced an invalid document.";
            if (!validation.m_issues.empty())
            {
                message += " First issue: ";
                message += validation.m_issues.front().m_code;
            }
            return AZ::Failure(message);
        }

        auto stagingRootRelativePath = GetRevisionRootFromTileRoot(
            result.m_stagingPlan.m_stagingTileRootRelativePath);
        if (!stagingRootRelativePath.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(stagingRootRelativePath.GetError()));
        }
        auto stagingRoot = EnsureContainedDirectory(
            canonicalWorkspaceRoot,
            stagingRootRelativePath.GetValue());
        if (!stagingRoot.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(stagingRoot.GetError()));
        }
        const QString canonicalStagingRoot = stagingRoot.GetValue();
        if (QFileInfo::exists(canonicalStagingRoot)
            && !PathsIdentifySameLocation(canonicalStagingRoot, canonicalPendingRoot)
            && QDir(canonicalStagingRoot).entryInfoList(
                   QDir::NoDotAndDotDot | QDir::AllEntries).size() != 0)
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
            return AZ::Failure(AZStd::string(
                "Terrain staging destination already contains state for this operation."));
        }
        if (!PathsIdentifySameLocation(canonicalStagingRoot, canonicalPendingRoot))
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            const QString stagingParent = QFileInfo(canonicalStagingRoot).absolutePath();
            if (!QDir().mkpath(stagingParent)
                || !QDir().rename(canonicalPendingRoot, canonicalStagingRoot))
            {
                RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalPendingRoot);
                return AZ::Failure(AZStd::string(
                    "Unable to promote pending terrain files into the contained staging root."));
            }
        }

        const QString stagingManifestPath = QDir(canonicalStagingRoot).filePath(
            QFileInfo(ToQString(result.m_stagingPlan.m_stagingManifestRelativePath)).fileName());
        const AZStd::string manifestJson = BuildCanonicalDocumentJson(result.m_document);
        auto manifestWrite = WriteBytesAtomically(
            stagingManifestPath,
            QByteArray(
                manifestJson.data(),
                static_cast<int>(manifestJson.size())));
        if (!manifestWrite.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(manifestWrite.GetError()));
        }

        auto publishedRootRelativePath = GetSafeParentRelativePath(
            result.m_stagingPlan.m_publishedManifestRelativePath);
        if (!publishedRootRelativePath.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(publishedRootRelativePath.GetError()));
        }
        auto publishedParentRelativePath = GetSafeParentRelativePath(
            publishedRootRelativePath.GetValue());
        if (!publishedParentRelativePath.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(publishedParentRelativePath.GetError()));
        }
        auto publishedParent = EnsureContainedDirectory(
            canonicalWorkspaceRoot,
            publishedParentRelativePath.GetValue());
        if (!publishedParent.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(publishedParent.GetError()));
        }
        const QString publishedRoot = QDir(publishedParent.GetValue()).filePath(
            QFileInfo(ToQString(publishedRootRelativePath.GetValue())).fileName());
        if (QFileInfo::exists(publishedRoot))
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(
                "Published terrain revision already exists; import will not overwrite it."));
        }

        auto sourceObservationRootRelativePath = GetSafeParentRelativePath(
            result.m_stagingPlan.m_sourceObservationRelativePath);
        if (!sourceObservationRootRelativePath.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(sourceObservationRootRelativePath.GetError()));
        }
        const QString sourceObservationRootDeclared = QDir(canonicalWorkspaceRoot).filePath(
            ToQString(sourceObservationRootRelativePath.GetValue()));
        const bool sourceObservationRootPreexisting =
            QFileInfo::exists(sourceObservationRootDeclared);
        auto sourceObservationRoot = EnsureContainedDirectory(
            canonicalWorkspaceRoot,
            sourceObservationRootRelativePath.GetValue());
        if (!sourceObservationRoot.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            return AZ::Failure(AZStd::string(sourceObservationRoot.GetError()));
        }
        const QString sourceObservationPath = QDir(sourceObservationRoot.GetValue()).filePath(
            QFileInfo(ToQString(result.m_stagingPlan.m_sourceObservationRelativePath)).fileName());
        auto observationWrite = WriteSourceObservation(result, sourceObservationPath);
        if (!observationWrite.IsSuccess())
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            if (!sourceObservationRootPreexisting)
            {
                RemoveContainedDirectory(canonicalWorkspaceRoot, sourceObservationRoot.GetValue());
            }
            return AZ::Failure(AZStd::string(observationWrite.GetError()));
        }

        if (!IsContainedPath(canonicalWorkspaceRoot, publishedRoot)
            || !QDir().rename(canonicalStagingRoot, publishedRoot))
        {
            RemoveContainedDirectory(canonicalWorkspaceRoot, canonicalStagingRoot);
            if (!sourceObservationRootPreexisting)
            {
                RemoveContainedDirectory(canonicalWorkspaceRoot, sourceObservationRoot.GetValue());
            }
            return AZ::Failure(AZStd::string(
                "Unable to atomically publish the terrain revision from staging."));
        }
        const QString canonicalPublishedRoot =
            ResolveDirectCanonicalDirectory(publishedRoot);
        if (canonicalPublishedRoot.isEmpty()
            || !IsContainedPath(canonicalWorkspaceRoot, canonicalPublishedRoot))
        {
            return AZ::Failure(AZStd::string(
                "Published terrain revision does not retain contained canonical identity."));
        }

        result.m_publishedManifestPath = ToAzString(
            QDir(canonicalPublishedRoot).filePath(
                QFileInfo(ToQString(result.m_stagingPlan.m_publishedManifestRelativePath)).fileName()));
        result.m_sourceObservationPath = ToAzString(sourceObservationPath);
        result.m_tileCount = result.m_document.m_tiles.size();
        for (const Tile& tile : result.m_document.m_tiles)
        {
            result.m_publishedTilePaths.push_back(ToAzString(
                QDir(canonicalPublishedRoot).filePath(
                    ToQString(tile.m_relativePath))));
        }
        return AZ::Success(AZStd::move(result));
    }
} // namespace TaintedGrailModdingSDK::TerrainHeightmap
