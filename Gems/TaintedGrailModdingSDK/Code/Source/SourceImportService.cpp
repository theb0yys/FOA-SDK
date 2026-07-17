/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "SourceImportService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QStringList>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr qint64 MaxStructuredImportBytes = 64 * 1024 * 1024;
        constexpr const char* GenericImporterId = "tg.generic-artifact";
        constexpr const char* JsonImporterId = "tg.structured-json";
        constexpr const char* CsvImporterId = "tg.structured-csv";
        constexpr const char* ImporterVersion = "1.0.0";

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        QStringList ParseCsvRow(const QString& line)
        {
            QStringList values;
            QString current;
            bool quoted = false;

            for (qsizetype index = 0; index < line.size(); ++index)
            {
                const QChar character = line.at(index);
                if (character == '"')
                {
                    if (quoted && index + 1 < line.size() && line.at(index + 1) == '"')
                    {
                        current.append('"');
                        ++index;
                    }
                    else
                    {
                        quoted = !quoted;
                    }
                }
                else if (character == ',' && !quoted)
                {
                    values.push_back(current.trimmed());
                    current.clear();
                }
                else
                {
                    current.append(character);
                }
            }

            values.push_back(current.trimmed());
            return values;
        }

        int ColumnIndex(const QHash<QString, int>& columns, const QStringList& names)
        {
            for (const QString& name : names)
            {
                const auto iterator = columns.constFind(name);
                if (iterator != columns.constEnd())
                {
                    return iterator.value();
                }
            }
            return -1;
        }

        QString CsvValue(const QStringList& values, int index)
        {
            return index >= 0 && index < values.size() ? values.at(index).trimmed() : QString();
        }

        bool HasSeverity(const AZStd::vector<ImportIssue>& issues, const char* severity)
        {
            for (const ImportIssue& issue : issues)
            {
                if (issue.m_severity == severity)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    AZStd::vector<SourceImporterContract> SourceImportService::GetContracts() const
    {
        SourceImporterContract json;
        json.m_importerId = JsonImporterId;
        json.m_displayName = "Structured JSON evidence";
        json.m_supportedSourceKinds = { "template-diagnostics", "item-recipe-dump", "scene-world-observation", "json-register", "avalon-core-source-set" };
        json.m_supportedExtensions = { ".json" };
        json.m_extractsEvidence = true;

        SourceImporterContract csv;
        csv.m_importerId = CsvImporterId;
        csv.m_displayName = "Structured CSV evidence";
        csv.m_supportedSourceKinds = { "item-recipe-dump", "scene-world-observation", "csv-register" };
        csv.m_supportedExtensions = { ".csv" };
        csv.m_extractsEvidence = true;

        SourceImporterContract generic;
        generic.m_importerId = GenericImporterId;
        generic.m_displayName = "Generic fingerprinted artifact";
        generic.m_supportedSourceKinds = {
            "template-diagnostics",
            "item-recipe-dump",
            "scene-world-observation",
            "decompilation-note",
            "runtime-log",
            "screenshot",
            "csv-register",
            "json-register",
            "avalon-core-source-set"
        };
        generic.m_supportedExtensions = { "*" };
        generic.m_extractsEvidence = false;

        return { json, csv, generic };
    }

    AZ::Outcome<SourceImportResult, AZStd::string> SourceImportService::Import(
        const SourceImportRequest& request,
        const GameProfile& profile) const
    {
        if (!profile.IsConfigured())
        {
            return AZ::Failure(AZStd::string("An exact configured FoA game profile is required before source intake."));
        }
        if (request.m_inputPath.empty())
        {
            return AZ::Failure(AZStd::string("An input file is required."));
        }
        if (request.m_sourceKind.empty())
        {
            return AZ::Failure(AZStd::string("A source kind is required."));
        }

        const QString inputPath = ToQString(request.m_inputPath);
        QFileInfo fileInfo(inputPath);
        if (!fileInfo.exists() || !fileInfo.isFile())
        {
            return AZ::Failure(AZStd::string("The selected source file does not exist or is not a regular file."));
        }

        const AZStd::string importerId = SelectImporterId(request);
        if (importerId != GenericImporterId && importerId != JsonImporterId && importerId != CsvImporterId)
        {
            return AZ::Failure(AZStd::string("The requested importer contract is not supported."));
        }

        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
        {
            return AZ::Failure(AZStd::string("The selected source file could not be opened for reading."));
        }

        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!file.atEnd())
        {
            const QByteArray chunk = file.read(1024 * 1024);
            if (chunk.isEmpty() && file.error() != QFileDevice::NoError)
            {
                return AZ::Failure(AZStd::string("The selected source file could not be read completely."));
            }
            hash.addData(chunk);
        }

        const QByteArray digest = hash.result().toHex();
        const QString digestText = QString::fromLatin1(digest);
        const QString importedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        const QString profileToken = ToQString(SanitizeIdentifier(profile.m_profileId));
        const QString sourceId = QStringLiteral("source.%1.%2").arg(profileToken, digestText.left(24));

        SourceImportResult result;
        SourceRecord& source = result.m_sourceDocument.m_source;
        source.m_sourceId = ToAzString(sourceId);
        source.m_title = request.m_title.empty() ? ToAzString(fileInfo.fileName()) : request.m_title;
        source.m_sourceKind = request.m_sourceKind;
        source.m_locator = ToAzString(fileInfo.absoluteFilePath());
        source.m_fingerprint = ToAzString(QStringLiteral("sha256:%1").arg(digestText));
        source.m_profileId = profile.m_profileId;
        source.m_gameVersion = profile.m_gameVersion;
        source.m_branch = profile.m_branch;
        source.m_runtimeTarget = profile.m_runtimeTarget;
        source.m_toolName = request.m_toolName;
        source.m_toolVersion = request.m_toolVersion;
        source.m_importerId = importerId;
        source.m_importerVersion = ImporterVersion;
        source.m_capturedAt = request.m_capturedAt.empty() ? ToAzString(importedAt) : request.m_capturedAt;
        source.m_importedAt = ToAzString(importedAt);
        source.m_limitations = request.m_limitations;
        source.m_mediaType = MediaTypeForPath(source.m_locator);
        source.m_byteSize = static_cast<AZ::u64>(fileInfo.size());

        EvidenceDocument& evidenceDocument = result.m_evidenceDocument;
        evidenceDocument.m_sourceId = source.m_sourceId;
        evidenceDocument.m_sourceFingerprint = source.m_fingerprint;
        evidenceDocument.m_profileId = profile.m_profileId;
        evidenceDocument.m_gameVersion = profile.m_gameVersion;
        evidenceDocument.m_branch = profile.m_branch;

        if (importerId == GenericImporterId)
        {
            AddIssue(
                evidenceDocument.m_issues,
                source.m_sourceId,
                "warning",
                "evidence.manual-extraction-required",
                "The artifact was fingerprinted and registered, but this importer contract does not infer evidence.",
                source.m_locator);
        }
        else if (fileInfo.size() > MaxStructuredImportBytes)
        {
            AddIssue(
                evidenceDocument.m_issues,
                source.m_sourceId,
                "error",
                "schema.structured-input-too-large",
                "Structured evidence extraction is limited to 64 MiB; the source remains registered for manual review.",
                source.m_locator);
        }
        else
        {
            if (!file.seek(0))
            {
                return AZ::Failure(AZStd::string("The selected source file could not be rewound for evidence extraction."));
            }
            const QByteArray data = file.readAll();
            if (importerId == JsonImporterId)
            {
                ExtractJsonEvidence(data, source, evidenceDocument);
            }
            else
            {
                ExtractCsvEvidence(data, source, evidenceDocument);
            }
        }

        const bool hasErrors = HasSeverity(result.m_sourceDocument.m_issues, "error")
            || HasSeverity(evidenceDocument.m_issues, "error");
        const bool hasWarnings = HasSeverity(result.m_sourceDocument.m_issues, "warning")
            || HasSeverity(evidenceDocument.m_issues, "warning");
        source.m_importStatus = hasErrors ? "error" : (hasWarnings ? "warning" : "imported");

        return AZ::Success(AZStd::move(result));
    }

    AZStd::string SourceImportService::SelectImporterId(const SourceImportRequest& request)
    {
        if (!request.m_preferredImporterId.empty())
        {
            return request.m_preferredImporterId;
        }

        const QString suffix = QFileInfo(ToQString(request.m_inputPath)).suffix().toLower();
        if (suffix == QStringLiteral("json"))
        {
            return JsonImporterId;
        }
        if (suffix == QStringLiteral("csv"))
        {
            return CsvImporterId;
        }
        return GenericImporterId;
    }

    AZStd::string SourceImportService::SanitizeIdentifier(const AZStd::string& value)
    {
        QString output = ToQString(value).toLower();
        for (qsizetype index = 0; index < output.size(); ++index)
        {
            const QChar character = output.at(index);
            if (!character.isLetterOrNumber() && character != '.' && character != '-' && character != '_')
            {
                output[index] = '-';
            }
        }
        return ToAzString(output);
    }

    AZStd::string SourceImportService::MediaTypeForPath(const AZStd::string& path)
    {
        const QString suffix = QFileInfo(ToQString(path)).suffix().toLower();
        if (suffix == "json")
        {
            return "application/json";
        }
        if (suffix == "csv")
        {
            return "text/csv";
        }
        if (suffix == "png")
        {
            return "image/png";
        }
        if (suffix == "jpg" || suffix == "jpeg")
        {
            return "image/jpeg";
        }
        if (suffix == "log" || suffix == "txt" || suffix == "md")
        {
            return "text/plain";
        }
        return "application/octet-stream";
    }

    void SourceImportService::AddIssue(
        AZStd::vector<ImportIssue>& issues,
        const AZStd::string& sourceId,
        AZStd::string severity,
        AZStd::string code,
        AZStd::string message,
        AZStd::string locator,
        AZStd::string recordPath,
        AZ::s64 line)
    {
        ImportIssue issue;
        issue.m_issueId = "issue.";
        issue.m_issueId += sourceId;
        issue.m_issueId += ".";
        issue.m_issueId += AZStd::to_string(issues.size() + 1);
        issue.m_severity = AZStd::move(severity);
        issue.m_code = AZStd::move(code);
        issue.m_message = AZStd::move(message);
        issue.m_locator = AZStd::move(locator);
        issue.m_recordPath = AZStd::move(recordPath);
        issue.m_line = line;
        issues.push_back(AZStd::move(issue));
    }

    void SourceImportService::ExtractJsonEvidence(
        const QByteArray& data,
        const SourceRecord& source,
        EvidenceDocument& evidenceDocument)
    {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError)
        {
            AddIssue(
                evidenceDocument.m_issues,
                source.m_sourceId,
                "error",
                "schema.invalid-json",
                ToAzString(QStringLiteral("JSON parse error at byte %1: %2").arg(parseError.offset).arg(parseError.errorString())),
                source.m_locator);
            return;
        }

        QJsonArray records;
        QString rootPath;
        if (document.isArray())
        {
            records = document.array();
            rootPath = QStringLiteral("$");
        }
        else if (document.isObject())
        {
            const QJsonObject rootObject = document.object();
            if (rootObject.value(QStringLiteral("evidence")).isArray())
            {
                records = rootObject.value(QStringLiteral("evidence")).toArray();
                rootPath = QStringLiteral("$.evidence");
            }
            else if (rootObject.contains(QStringLiteral("subject_ref")) && rootObject.contains(QStringLiteral("claim")))
            {
                records.append(rootObject);
                rootPath = QStringLiteral("$");
            }
        }

        if (records.isEmpty())
        {
            AddIssue(
                evidenceDocument.m_issues,
                source.m_sourceId,
                "warning",
                "schema.no-evidence-records",
                "Valid JSON was imported, but no evidence array or evidence-shaped root object was found.",
                source.m_locator,
                "$");
            return;
        }

        QSet<QString> evidenceIds;
        const QString fingerprintToken = ToQString(source.m_fingerprint).section(':', 1).left(16);
        for (qsizetype index = 0; index < records.size(); ++index)
        {
            const QString recordPath = records.size() == 1 && rootPath == "$"
                ? rootPath
                : QStringLiteral("%1[%2]").arg(rootPath).arg(index);
            if (!records.at(index).isObject())
            {
                AddIssue(
                    evidenceDocument.m_issues,
                    source.m_sourceId,
                    "error",
                    "schema.evidence-not-object",
                    "Evidence entries must be JSON objects.",
                    source.m_locator,
                    ToAzString(recordPath));
                continue;
            }

            const QJsonObject object = records.at(index).toObject();
            const QString subjectRef = object.value(QStringLiteral("subject_ref")).toString().trimmed();
            const QString claim = object.value(QStringLiteral("claim")).toString().trimmed();
            if (subjectRef.isEmpty() || claim.isEmpty())
            {
                AddIssue(
                    evidenceDocument.m_issues,
                    source.m_sourceId,
                    "error",
                    "schema.evidence-required-fields",
                    "Evidence entries require non-empty subject_ref and claim values.",
                    source.m_locator,
                    ToAzString(recordPath));
                continue;
            }

            QString evidenceId = object.value(QStringLiteral("evidence_id")).toString().trimmed();
            if (evidenceId.isEmpty())
            {
                evidenceId = QStringLiteral("evidence.%1.%2").arg(fingerprintToken).arg(index + 1);
            }
            if (evidenceIds.contains(evidenceId))
            {
                AddIssue(
                    evidenceDocument.m_issues,
                    source.m_sourceId,
                    "error",
                    "schema.duplicate-evidence-id",
                    ToAzString(QStringLiteral("Duplicate evidence ID: %1").arg(evidenceId)),
                    source.m_locator,
                    ToAzString(recordPath));
                continue;
            }
            evidenceIds.insert(evidenceId);

            EvidenceRecord evidence;
            evidence.m_evidenceId = ToAzString(evidenceId);
            evidence.m_sourceId = source.m_sourceId;
            evidence.m_sourceFingerprint = source.m_fingerprint;
            evidence.m_profileId = source.m_profileId;
            evidence.m_gameVersion = source.m_gameVersion;
            evidence.m_branch = source.m_branch;
            evidence.m_subjectRef = ToAzString(subjectRef);
            evidence.m_claim = ToAzString(claim);
            evidence.m_evidenceKind = ToAzString(object.value(QStringLiteral("kind")).toString(source.m_sourceKind.c_str()));
            evidence.m_confidence = ToAzString(object.value(QStringLiteral("confidence")).toString(QStringLiteral("unrated")));
            evidence.m_locator = object.value(QStringLiteral("locator")).toString().isEmpty()
                ? source.m_locator
                : ToAzString(object.value(QStringLiteral("locator")).toString());
            evidence.m_recordPath = ToAzString(recordPath);
            evidence.m_extractedAt = source.m_importedAt;
            evidenceDocument.m_evidence.push_back(AZStd::move(evidence));
        }
    }

    void SourceImportService::ExtractCsvEvidence(
        const QByteArray& data,
        const SourceRecord& source,
        EvidenceDocument& evidenceDocument)
    {
        QString text = QString::fromUtf8(data);
        QStringList lines = text.split('\n');
        if (lines.isEmpty())
        {
            AddIssue(
                evidenceDocument.m_issues,
                source.m_sourceId,
                "error",
                "schema.empty-csv",
                "The CSV source is empty.",
                source.m_locator);
            return;
        }

        const QStringList header = ParseCsvRow(lines.takeFirst().remove('\r'));
        QHash<QString, int> columns;
        for (int index = 0; index < header.size(); ++index)
        {
            columns.insert(header.at(index).trimmed().toLower(), index);
        }

        const int subjectColumn = ColumnIndex(columns, { "subject_ref", "subject" });
        const int claimColumn = ColumnIndex(columns, { "claim" });
        const int idColumn = ColumnIndex(columns, { "evidence_id", "id" });
        const int kindColumn = ColumnIndex(columns, { "kind", "evidence_kind" });
        const int confidenceColumn = ColumnIndex(columns, { "confidence" });
        const int locatorColumn = ColumnIndex(columns, { "locator" });
        if (subjectColumn < 0 || claimColumn < 0)
        {
            AddIssue(
                evidenceDocument.m_issues,
                source.m_sourceId,
                "error",
                "schema.csv-required-columns",
                "CSV evidence requires subject_ref and claim columns.",
                source.m_locator,
                "header",
                1);
            return;
        }

        QSet<QString> evidenceIds;
        const QString fingerprintToken = ToQString(source.m_fingerprint).section(':', 1).left(16);
        for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
        {
            QString line = lines.at(lineIndex);
            line.remove('\r');
            if (line.trimmed().isEmpty())
            {
                continue;
            }

            const QStringList values = ParseCsvRow(line);
            const QString subjectRef = CsvValue(values, subjectColumn);
            const QString claim = CsvValue(values, claimColumn);
            const AZ::s64 sourceLine = static_cast<AZ::s64>(lineIndex + 2);
            const QString recordPath = QStringLiteral("row[%1]").arg(sourceLine);
            if (subjectRef.isEmpty() || claim.isEmpty())
            {
                AddIssue(
                    evidenceDocument.m_issues,
                    source.m_sourceId,
                    "error",
                    "schema.csv-required-values",
                    "CSV evidence rows require non-empty subject_ref and claim values.",
                    source.m_locator,
                    ToAzString(recordPath),
                    sourceLine);
                continue;
            }

            QString evidenceId = CsvValue(values, idColumn);
            if (evidenceId.isEmpty())
            {
                evidenceId = QStringLiteral("evidence.%1.%2").arg(fingerprintToken).arg(lineIndex + 1);
            }
            if (evidenceIds.contains(evidenceId))
            {
                AddIssue(
                    evidenceDocument.m_issues,
                    source.m_sourceId,
                    "error",
                    "schema.duplicate-evidence-id",
                    ToAzString(QStringLiteral("Duplicate evidence ID: %1").arg(evidenceId)),
                    source.m_locator,
                    ToAzString(recordPath),
                    sourceLine);
                continue;
            }
            evidenceIds.insert(evidenceId);

            EvidenceRecord evidence;
            evidence.m_evidenceId = ToAzString(evidenceId);
            evidence.m_sourceId = source.m_sourceId;
            evidence.m_sourceFingerprint = source.m_fingerprint;
            evidence.m_profileId = source.m_profileId;
            evidence.m_gameVersion = source.m_gameVersion;
            evidence.m_branch = source.m_branch;
            evidence.m_subjectRef = ToAzString(subjectRef);
            evidence.m_claim = ToAzString(claim);
            const QString kind = CsvValue(values, kindColumn);
            evidence.m_evidenceKind = kind.isEmpty() ? source.m_sourceKind : ToAzString(kind);
            const QString confidence = CsvValue(values, confidenceColumn);
            evidence.m_confidence = confidence.isEmpty() ? "unrated" : ToAzString(confidence);
            const QString locator = CsvValue(values, locatorColumn);
            evidence.m_locator = locator.isEmpty() ? source.m_locator : ToAzString(locator);
            evidence.m_recordPath = ToAzString(recordPath);
            evidence.m_extractedAt = source.m_importedAt;
            evidenceDocument.m_evidence.push_back(AZStd::move(evidence));
        }

        if (evidenceDocument.m_evidence.empty() && evidenceDocument.m_issues.empty())
        {
            AddIssue(
                evidenceDocument.m_issues,
                source.m_sourceId,
                "warning",
                "schema.no-evidence-records",
                "The CSV contained headers but no evidence rows.",
                source.m_locator);
        }
    }
} // namespace TaintedGrailModdingSDK
