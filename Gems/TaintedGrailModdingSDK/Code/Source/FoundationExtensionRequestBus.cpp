/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

#include "ResearchContractValidation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr size_t MaximumExtensionDocumentBytes = 4 * 1024 * 1024;

        void SetError(AZStd::string* error, AZStd::string message)
        {
            if (error)
            {
                *error = AZStd::move(message);
            }
        }

        bool IsSafeRelativePath(const AZStd::string& path)
        {
            if (path.empty() || path.size() > 512 || path.front() == '/'
                || path.find('\\') != AZStd::string::npos
                || path.find("//") != AZStd::string::npos)
            {
                return false;
            }
            size_t start = 0;
            while (start < path.size())
            {
                const size_t end = path.find('/', start);
                const size_t length = (end == AZStd::string::npos ? path.size() : end) - start;
                if (length == 0)
                {
                    return false;
                }
                const AZStd::string segment = path.substr(start, length);
                if (segment == "." || segment == "..")
                {
                    return false;
                }
                for (char character : segment)
                {
                    if (!((character >= 'a' && character <= 'z')
                        || (character >= 'A' && character <= 'Z')
                        || (character >= '0' && character <= '9')
                        || character == '.' || character == '_' || character == '-'))
                    {
                        return false;
                    }
                }
                if (end == AZStd::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return true;
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        AZStd::string ToAzString(const QByteArray& value)
        {
            return AZStd::string(value.constData(), static_cast<size_t>(value.size()));
        }
    } // namespace

    bool FoundationService::RegisterExtension(
        const ExtensionAPI::ExtensionDeclaration& declaration,
        AZStd::string* error)
    {
        return m_extensionApi.RegisterExtension(declaration, error);
    }

    bool FoundationService::UnregisterExtension(
        const AZStd::string& extensionId,
        AZStd::string* error)
    {
        if (extensionId == "extension.tainted-framework")
        {
            SetError(error, "The canonical Tainted Framework consumer cannot be unregistered while Foundation is active.");
            return false;
        }
        return m_extensionApi.UnregisterExtension(extensionId, error);
    }

    bool FoundationService::IsExtensionRegistered(const AZStd::string& extensionId) const
    {
        return m_extensionApi.IsExtensionRegistered(extensionId);
    }

    bool FoundationService::GetActiveProfile(
        const AZStd::string& extensionId,
        ExtensionAPI::ProfileView& profile,
        AZStd::string* error) const
    {
        return m_extensionApi.GetActiveProfile(extensionId, profile, error);
    }

    bool FoundationService::QueryCatalog(
        const AZStd::string& extensionId,
        const CatalogQuery& query,
        AZStd::vector<CatalogRecord>& records,
        size_t maximumResults,
        AZStd::string* error) const
    {
        return m_extensionApi.QueryCatalog(
            extensionId, query, records, maximumResults, error);
    }

    bool FoundationService::SubmitCandidateEvidence(
        const AZStd::string& extensionId,
        const EvidenceRecord& evidence,
        AZStd::string* error)
    {
        return m_extensionApi.SubmitCandidateEvidence(extensionId, evidence, error);
    }

    bool FoundationService::SaveExtensionDocument(
        const AZStd::string& extensionId,
        const AZStd::string& relativePath,
        const AZStd::string& contents,
        AZStd::string* error)
    {
        if (!m_extensionApi.IsExtensionRegistered(extensionId))
        {
            SetError(error, "Extension document writes require prior deterministic registration.");
            return false;
        }
        if (!IsStableContractId(extensionId) || !IsSafeRelativePath(relativePath)
            || contents.size() > MaximumExtensionDocumentBytes)
        {
            SetError(error, "Extension document identity, path, or size is outside the host-owned boundary.");
            return false;
        }
        const QByteArray payload(contents.data(), static_cast<qsizetype>(contents.size()));
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || document.isNull())
        {
            SetError(error, "Extension documents must contain one valid JSON object or array.");
            return false;
        }
        if (m_workspaceRootPath.empty())
        {
            SetError(error, "Save or open a workspace before writing extension documents.");
            return false;
        }

        const auto resolved = m_pathPolicy.ResolveExtensionDocumentPath(
            m_workspaceRootPath, extensionId, relativePath, false);
        if (!resolved.IsSuccess())
        {
            SetError(error, AZStd::string(resolved.GetError()));
            return false;
        }
        const QString filePath = ToQString(resolved.GetValue());
        const QFileInfo info(filePath);
        if (!QDir().mkpath(info.absolutePath()))
        {
            SetError(error, "Unable to create the extension document parent directory.");
            return false;
        }
        QSaveFile file(filePath);
        if (!file.open(QIODevice::WriteOnly))
        {
            SetError(error, "Unable to open the extension document candidate for atomic writing.");
            return false;
        }
        if (file.write(payload) != payload.size() || !file.commit())
        {
            file.cancelWriting();
            SetError(error, "Extension document write failed; no candidate state was published.");
            return false;
        }
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool FoundationService::LoadExtensionDocument(
        const AZStd::string& extensionId,
        const AZStd::string& relativePath,
        AZStd::string& contents,
        AZStd::string* error) const
    {
        contents.clear();
        if (!m_extensionApi.IsExtensionRegistered(extensionId))
        {
            SetError(error, "Extension document reads require prior deterministic registration.");
            return false;
        }
        if (!IsStableContractId(extensionId) || !IsSafeRelativePath(relativePath)
            || m_workspaceRootPath.empty())
        {
            SetError(error, "Extension document read requires a safe identity, path, and open workspace.");
            return false;
        }
        const auto resolved = m_pathPolicy.ResolveExtensionDocumentPath(
            m_workspaceRootPath, extensionId, relativePath, true);
        if (!resolved.IsSuccess())
        {
            SetError(error, AZStd::string(resolved.GetError()));
            return false;
        }
        const QString filePath = ToQString(resolved.GetValue());
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly) || file.size() < 0
            || file.size() > static_cast<qint64>(MaximumExtensionDocumentBytes))
        {
            SetError(error, "Extension document is missing, unreadable, or exceeds the host-owned size bound.");
            return false;
        }
        contents = ToAzString(file.readAll());
        if (error)
        {
            error->clear();
        }
        return true;
    }
} // namespace TaintedGrailModdingSDK
