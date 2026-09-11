/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PackDraftRecoveryService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool RecoveryPathHasNoLinks(const QString& path)
        {
            QString current = QFileInfo(path).absoluteFilePath();
            for (int depth = 0; depth < 64; ++depth)
            {
                const QFileInfo info(current);
                if (info.isSymLink() || info.isJunction())
                {
                    return false;
                }
                const QString parent = info.absolutePath();
                if (parent == current)
                {
                    return true;
                }
                current = parent;
            }
            return false;
        }

        QString CanonicalRecoveryIdentity(const QString& path)
        {
            QString canonical = QFileInfo(path).canonicalFilePath();
#ifdef Q_OS_WIN
            canonical = canonical.toCaseFolded();
#endif
            return canonical;
        }

        bool ValidRecoveryFields(const QMap<QString, QString>& values)
        {
            if (values.keys() != PackDraftRecoveryService::FieldNames())
            {
                return false;
            }
            for (const QString& value : values)
            {
                if (value.size() > PackDraftRecoveryService::MaximumFieldCharacters)
                {
                    return false;
                }
            }
            return QStringList({ "unknown", "none", "compatible", "migration", "destructive" })
                    .contains(values.value(QStringLiteral("saveImpact")))
                && QStringList({ "development", "alpha", "beta", "release" })
                    .contains(values.value(QStringLiteral("releaseChannel")));
        }

        QJsonObject RecoveryFieldsToJson(const QMap<QString, QString>& values)
        {
            QJsonObject result;
            for (auto it = values.cbegin(); it != values.cend(); ++it)
            {
                result.insert(it.key(), it.value());
            }
            return result;
        }

        bool RecoveryFieldsFromJson(const QJsonValue& value, QMap<QString, QString>& fields)
        {
            if (!value.isObject())
            {
                return false;
            }
            const QJsonObject object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it)
            {
                if (!it.value().isString())
                {
                    return false;
                }
                fields.insert(it.key(), it.value().toString());
            }
            return ValidRecoveryFields(fields);
        }
    } // namespace

    PackDraftRecoveryService::PackDraftRecoveryService(const QString& recoveryRoot)
        : m_root(recoveryRoot.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(recoveryRoot).absoluteFilePath()))
    {
    }

    PackDraftRecoveryService::~PackDraftRecoveryService() = default;

    QString PackDraftRecoveryService::DefaultRecoveryRoot()
    {
#ifdef Q_OS_WIN
        const QString base = qEnvironmentVariable("LOCALAPPDATA");
#else
        const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#endif
        return base.isEmpty() ? QString() : QDir(base).filePath(QStringLiteral("FOA-SDK/Recovery/PackDrafts"));
    }

    QStringList PackDraftRecoveryService::FieldNames()
    {
        QStringList names{ "displayName", "owner", "version", "targetGameVersion", "targetBranch",
            "coreVersion", "adapterVersion", "buildConfiguration", "compatibleGameVersions", "dlcScopes",
            "dependencies", "requiredMods", "incompatibilities", "contentDefinitions", "assetPaths",
            "localisationPaths", "saveImpact", "releaseChannel" };
        names.sort();
        return names;
    }

    bool PackDraftRecoveryService::Bind(const QString& workspaceId, const QString& workspaceRoot,
        const QString& workspaceDocument, QString& error)
    {
        m_lock.reset();
        m_path.clear();
        m_canWrite = false;
        m_workspaceId = workspaceId;
        m_workspaceRoot = CanonicalRecoveryIdentity(workspaceRoot);
        m_workspaceDocument = workspaceDocument.isEmpty() ? QString() : CanonicalRecoveryIdentity(workspaceDocument);
        if (m_root.isEmpty() || !QDir::isAbsolutePath(m_root) || workspaceId.isEmpty()
            || m_workspaceRoot.isEmpty() || !QFileInfo(workspaceRoot).isDir()
            || (!workspaceDocument.isEmpty() && (m_workspaceDocument.isEmpty() || !QFileInfo(workspaceDocument).isFile())))
        {
            error = QStringLiteral("Recovery needs an available workspace and local application-data folder.");
            return false;
        }
        if (!RecoveryPathHasNoLinks(m_root) || !QDir().mkpath(m_root) || !RecoveryPathHasNoLinks(m_root))
        {
            error = QStringLiteral("The recovery folder is unavailable or uses a linked path.");
            return false;
        }
        const QJsonObject identity{ { "id", m_workspaceId }, { "root", m_workspaceRoot }, { "document", m_workspaceDocument } };
        const QByteArray key = QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),
            QCryptographicHash::Sha256).toHex();
        m_path = QDir(m_root).filePath(QString::fromLatin1(key) + QStringLiteral(".packdraft.json"));
        if (!RecoveryPathHasNoLinks(m_path) || !RecoveryPathHasNoLinks(m_path + QStringLiteral(".lock")))
        {
            error = QStringLiteral("Recovery cannot use a linked file.");
            return false;
        }
        m_lock = std::make_unique<QLockFile>(m_path + QStringLiteral(".lock"));
        m_lock->setStaleLockTime(0); // A living Editor retains ownership regardless of session duration.
        if (!m_lock->tryLock(0))
        {
            error = QStringLiteral("Recovery is in use by another Editor, or its folder is not writable. Close the other Editor or fix folder access, then retry.");
            return false;
        }
        error.clear();
        return true;
    }

    bool PackDraftRecoveryService::CheckPath(QString& error) const
    {
        if (m_path.isEmpty() || !m_lock || !m_lock->isLocked() || !RecoveryPathHasNoLinks(m_path))
        {
            error = QStringLiteral("Recovery storage is not available for this workspace.");
            return false;
        }
        return true;
    }

    PackDraftRecoveryRead PackDraftRecoveryService::Read()
    {
        PackDraftRecoveryRead result;
        m_canWrite = false;
        if (!CheckPath(result.m_error))
        {
            return result;
        }
        result.m_exists = QFileInfo::exists(m_path);
        if (!result.m_exists)
        {
            m_canWrite = true;
            return result;
        }
        QFile file(m_path);
        if (file.size() <= 0 || file.size() > MaximumDocumentBytes || !file.open(QIODevice::ReadOnly))
        {
            result.m_error = QStringLiteral("The recovery copy is unreadable or exceeds the 256 KiB limit. It has been kept.");
            return result;
        }
        const QByteArray bytes = file.read(MaximumDocumentBytes + 1);
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
        if (bytes.size() > MaximumDocumentBytes || parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            result.m_error = QStringLiteral("The recovery copy is damaged. It has been kept.");
            return result;
        }
        const QJsonObject object = document.object();
        if (object.value("Format") != QStringLiteral("FOA-SDK.PackDraftRecovery")
            || !object.value("SchemaVersion").isDouble() || object.value("SchemaVersion").toDouble() != 1.0)
        {
            result.m_error = QStringLiteral("This Editor cannot read the recovery format. The copy has been kept.");
            return result;
        }
        if (object.value("WorkspaceId") != m_workspaceId || object.value("WorkspaceRoot") != m_workspaceRoot
            || object.value("WorkspaceDocument") != m_workspaceDocument)
        {
            result.m_error = QStringLiteral("The recovery copy belongs to another workspace. It has been kept.");
            return result;
        }
        auto& draft = result.m_draft;
        if (object.size() != 10 || !object.value("PackId").isString() || object.value("PackId").toString().size() > 256
            || !object.value("IsNew").isBool() || !object.value("AdvancedExpanded").isBool()
            || !RecoveryFieldsFromJson(object.value("Fields"), draft.m_fields)
            || !RecoveryFieldsFromJson(object.value("Baseline"), draft.m_baseline))
        {
            result.m_error = QStringLiteral("The recovery copy has unsupported or incomplete form values. It has been kept.");
            return result;
        }
        draft.m_packId = object.value("PackId").toString();
        draft.m_isNew = object.value("IsNew").toBool();
        draft.m_advancedExpanded = object.value("AdvancedExpanded").toBool();
        result.m_valid = true;
        m_canWrite = true;
        return result;
    }

    bool PackDraftRecoveryService::Write(const PackDraftRecovery& draft, QString& error)
    {
        if (!CheckPath(error))
        {
            return false;
        }
        if (!m_canWrite || !ValidRecoveryFields(draft.m_fields) || !ValidRecoveryFields(draft.m_baseline)
            || draft.m_packId.size() > 256)
        {
            error = QStringLiteral("Recovery could not record these form values. The previous copy has been kept.");
            return false;
        }
        const QJsonObject object{ { "Format", "FOA-SDK.PackDraftRecovery" }, { "SchemaVersion", 1 },
            { "WorkspaceId", m_workspaceId }, { "WorkspaceRoot", m_workspaceRoot }, { "WorkspaceDocument", m_workspaceDocument },
            { "PackId", draft.m_packId }, { "IsNew", draft.m_isNew }, { "AdvancedExpanded", draft.m_advancedExpanded },
            { "Fields", RecoveryFieldsToJson(draft.m_fields) }, { "Baseline", RecoveryFieldsToJson(draft.m_baseline) } };
        const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
        if (bytes.size() > MaximumDocumentBytes)
        {
            error = QStringLiteral("Recovery exceeds the 256 KiB limit. Save the mod to preserve all changes.");
            return false;
        }
        QSaveFile file(m_path);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        {
            error = QStringLiteral("Recovery could not be updated. The previous copy has been kept. ") + file.errorString();
            return false;
        }
        error.clear();
        return true;
    }

    bool PackDraftRecoveryService::Clear(QString& error)
    {
        if (!CheckPath(error))
        {
            return false;
        }
        if (QFileInfo::exists(m_path) && !QFile::remove(m_path))
        {
            error = QStringLiteral("The recovery copy could not be cleared. Check folder access and retry.");
            return false;
        }
        m_canWrite = true;
        error.clear();
        return true;
    }
} // namespace TaintedGrailModdingSDK
