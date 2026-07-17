/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "SourceEvidencePersistenceService.h"

#include <AzCore/Serialization/Json/JsonUtils.h>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        ImportIssue MakeLoadIssue(
            const AZStd::string& code,
            const AZStd::string& message,
            const AZStd::string& locator)
        {
            ImportIssue issue;
            issue.m_issueId = "issue.workspace-load.";
            issue.m_issueId += AZStd::string::format("%llu", static_cast<unsigned long long>(qHash(ToQString(locator))));
            issue.m_severity = "error";
            issue.m_code = code;
            issue.m_message = message;
            issue.m_locator = locator;
            return issue;
        }
    } // namespace

    AZ::Outcome<SourceEvidenceDocumentPaths, AZStd::string> SourceEvidencePersistenceService::SaveDocuments(
        const SourceDocument& sourceDocument,
        const EvidenceDocument& evidenceDocument,
        const AZStd::string& workspaceRoot) const
    {
        if (workspaceRoot.empty())
        {
            return AZ::Failure(AZStd::string("A workspace root is required before source documents can be saved."));
        }
        if (!sourceDocument.UsesSupportedSchema() || !evidenceDocument.UsesSupportedSchema())
        {
            return AZ::Failure(AZStd::string("Source or evidence document uses an unsupported schema version."));
        }
        if (sourceDocument.m_source.m_sourceId.empty())
        {
            return AZ::Failure(AZStd::string("A source ID is required before source documents can be saved."));
        }
        if (evidenceDocument.m_sourceId != sourceDocument.m_source.m_sourceId
            || evidenceDocument.m_sourceFingerprint != sourceDocument.m_source.m_fingerprint)
        {
            return AZ::Failure(AZStd::string("The evidence document does not match the source identity and fingerprint."));
        }

        const QString sourceDirectory = QDir(ToQString(workspaceRoot)).filePath(
            QStringLiteral("Sources/%1").arg(ToQString(sourceDocument.m_source.m_sourceId)));
        if (!QDir().mkpath(sourceDirectory))
        {
            return AZ::Failure(AZStd::string("Unable to create the source document directory inside the workspace."));
        }

        SourceEvidenceDocumentPaths paths;
        paths.m_sourceDocumentPath = ToAzString(QDir(sourceDirectory).filePath(QStringLiteral("source.tgsource.json")));
        paths.m_evidenceDocumentPath = ToAzString(QDir(sourceDirectory).filePath(QStringLiteral("evidence.tgevidence.json")));

        const AZ::Outcome<void, AZStd::string> sourceSave = AZ::JsonSerializationUtils::SaveObjectToFile(
            &sourceDocument,
            paths.m_sourceDocumentPath);
        if (!sourceSave.IsSuccess())
        {
            return AZ::Failure(AZStd::string(sourceSave.GetError()));
        }

        const AZ::Outcome<void, AZStd::string> evidenceSave = AZ::JsonSerializationUtils::SaveObjectToFile(
            &evidenceDocument,
            paths.m_evidenceDocumentPath);
        if (!evidenceSave.IsSuccess())
        {
            return AZ::Failure(AZStd::string(evidenceSave.GetError()));
        }

        return AZ::Success(AZStd::move(paths));
    }

    AZ::Outcome<void, AZStd::string> SourceEvidencePersistenceService::LoadWorkspaceDocuments(
        const AZStd::string& workspaceRoot,
        AZStd::vector<SourceDocument>& sourceDocuments,
        AZStd::vector<EvidenceDocument>& evidenceDocuments,
        AZStd::vector<ImportIssue>& loadIssues) const
    {
        sourceDocuments.clear();
        evidenceDocuments.clear();
        loadIssues.clear();

        if (workspaceRoot.empty())
        {
            return AZ::Failure(AZStd::string("A workspace root is required before source documents can be loaded."));
        }

        const QString sourcesRoot = QDir(ToQString(workspaceRoot)).filePath(QStringLiteral("Sources"));
        if (!QFileInfo::exists(sourcesRoot))
        {
            return AZ::Success();
        }

        QDirIterator iterator(
            sourcesRoot,
            QStringList() << QStringLiteral("source.tgsource.json"),
            QDir::Files,
            QDirIterator::Subdirectories);
        while (iterator.hasNext())
        {
            const QString sourcePath = iterator.next();
            SourceDocument sourceDocument;
            const AZ::Outcome<void, AZStd::string> sourceLoad = AZ::JsonSerializationUtils::LoadObjectFromFile(
                sourceDocument,
                ToAzString(sourcePath));
            if (!sourceLoad.IsSuccess())
            {
                loadIssues.push_back(MakeLoadIssue(
                    "document.source-load-failed",
                    AZStd::string(sourceLoad.GetError()),
                    ToAzString(sourcePath)));
                continue;
            }
            if (!sourceDocument.UsesSupportedSchema())
            {
                loadIssues.push_back(MakeLoadIssue(
                    "document.source-schema-unsupported",
                    "The source document schema version is not supported by this editor build.",
                    ToAzString(sourcePath)));
                continue;
            }

            const QString evidencePath = QDir(QFileInfo(sourcePath).absolutePath()).filePath(
                QStringLiteral("evidence.tgevidence.json"));
            EvidenceDocument evidenceDocument;
            if (!QFileInfo::exists(evidencePath))
            {
                loadIssues.push_back(MakeLoadIssue(
                    "document.evidence-missing",
                    "The source document has no matching evidence document.",
                    ToAzString(evidencePath)));
            }
            else
            {
                const AZ::Outcome<void, AZStd::string> evidenceLoad = AZ::JsonSerializationUtils::LoadObjectFromFile(
                    evidenceDocument,
                    ToAzString(evidencePath));
                if (!evidenceLoad.IsSuccess())
                {
                    loadIssues.push_back(MakeLoadIssue(
                        "document.evidence-load-failed",
                        AZStd::string(evidenceLoad.GetError()),
                        ToAzString(evidencePath)));
                }
                else if (!evidenceDocument.UsesSupportedSchema())
                {
                    loadIssues.push_back(MakeLoadIssue(
                        "document.evidence-schema-unsupported",
                        "The evidence document schema version is not supported by this editor build.",
                        ToAzString(evidencePath)));
                }
                else if (evidenceDocument.m_sourceId != sourceDocument.m_source.m_sourceId
                    || evidenceDocument.m_sourceFingerprint != sourceDocument.m_source.m_fingerprint)
                {
                    loadIssues.push_back(MakeLoadIssue(
                        "document.source-evidence-mismatch",
                        "The evidence document does not match the source identity and fingerprint.",
                        ToAzString(evidencePath)));
                }
                else
                {
                    evidenceDocuments.push_back(AZStd::move(evidenceDocument));
                }
            }

            sourceDocuments.push_back(AZStd::move(sourceDocument));
        }

        return AZ::Success();
    }
} // namespace TaintedGrailModdingSDK
