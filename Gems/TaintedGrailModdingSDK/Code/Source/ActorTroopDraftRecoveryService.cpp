/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorTroopDraftRecoveryService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <cmath>
#include <limits>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool ActorRecoveryPathHasNoLinks(const QString& path)
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

        QString ActorRecoveryIdentity(const QString& path)
        {
            QString canonical = QFileInfo(path).canonicalFilePath();
#ifdef Q_OS_WIN
            canonical = canonical.toCaseFolded();
#endif
            return canonical;
        }

        bool ActorRecoveryString(const QJsonValue& value, qsizetype maximum = ActorTroopDraftRecoveryService::MaximumFieldCharacters)
        {
            return value.isString() && value.toString().size() <= maximum;
        }


        bool ActorRecoveryStrings(const QJsonValue& value, QStringList& result, qsizetype limit = ActorTroopDraftRecoveryService::MaximumFieldCharacters)
        {
            if (!value.isArray() || value.toArray().size() > ActorTroopDraftRecoveryService::MaximumEntries) { return false; }
            for (const auto& item : value.toArray())
            {
                if (!ActorRecoveryString(item, limit)) { return false; }
                result.push_back(item.toString());
            }
            return true;
        }

        QJsonArray ActorRecoveryValueToJson(const QVariant& value)
        {
            switch (value.metaType().id())
            {
            case QMetaType::QString: return {"string", value.toString()};
            case QMetaType::Int: return {"integer", value.toInt()};
            case QMetaType::Bool: return {"boolean", value.toBool()};
            case QMetaType::QStringList: return {"strings", QJsonArray::fromStringList(value.toStringList())};
            case QMetaType::QVariantList:
            {
                const auto selection = value.toList();
                if (selection.size() != 3 || selection[1].metaType().id() != QMetaType::QString
                    || (selection[0].isValid() && selection[0].metaType().id() != QMetaType::QString)
                    || selection[2].metaType().id() != QMetaType::QVariantList
                    || selection[2].toList().size() > ActorTroopDraftRecoveryService::MaximumEntries) { return {}; }
                QJsonArray items;
                for (const auto& item : selection[2].toList())
                {
                    const auto pair = item.toList();
                    if (pair.size() != 2 || pair[0].metaType().id() != QMetaType::QString
                        || pair[1].metaType().id() != QMetaType::QString) { return {}; }
                    items.push_back(QJsonArray{pair[0].toString(), pair[1].toString()});
                }
                return {"selection", selection[0].isValid() ? QJsonValue(selection[0].toString()) : QJsonValue(),
                    selection[1].toString(), items};
            }
            default: return {};
            }
        }

        bool ActorRecoveryValueFromJson(const QJsonValue& encoded, QVariant& value)
        {
            if (!encoded.isArray()) { return false; }
            const auto array = encoded.toArray();
            if (array.size() < 2 || !array[0].isString()) { return false; }
            const auto type = array[0].toString();
            const auto data = array[1];
            if (type == "selection")
            {
                if (array.size() != 4 || !(data.isNull() || ActorRecoveryString(data, 512))
                    || !ActorRecoveryString(array[2]) || !array[3].isArray()
                    || array[3].toArray().size() > ActorTroopDraftRecoveryService::MaximumEntries) { return false; }
                QVariantList items;
                QSet<QString> ids;
                for (const auto& item : array[3].toArray())
                {
                    if (!item.isArray()) { return false; }
                    const auto pair = item.toArray();
                    if (pair.size() != 2 || !ActorRecoveryString(pair[0], 512) || !ActorRecoveryString(pair[1])
                        || ids.contains(pair[0].toString())) { return false; }
                    ids.insert(pair[0].toString());
                    items.push_back(QVariantList{pair[0].toString(), pair[1].toString()});
                }
                value = QVariantList{data.isNull() ? QVariant() : QVariant(data.toString()), array[2].toString(), items};
                return true;
            }
            if (array.size() != 2) { return false; }
            if (type == "string" && ActorRecoveryString(data)) { value = data.toString(); }
            else if (type == "boolean" && data.isBool()) { value = data.toBool(); }
            else if (type == "integer" && data.isDouble() && data.toDouble() >= std::numeric_limits<int>::min()
                && data.toDouble() <= std::numeric_limits<int>::max() && std::floor(data.toDouble()) == data.toDouble()) { value = data.toInt(); }
            else if (type == "strings")
            {
                QStringList strings;
                if (!ActorRecoveryStrings(data, strings, 512)) { return false; }
                value = strings;
            }
            else { return false; }
            return true;
        }

        bool ActorRecoveryFieldsFromJson(const QJsonValue& encoded, QHash<QString, QVariant>& fields)
        {
            if (!encoded.isObject() || encoded.toObject().isEmpty() || encoded.toObject().size() > 64) { return false; }
            const auto object = encoded.toObject();
            for (auto it = object.begin(); it != object.end(); ++it)
            {
                QVariant value;
                if (it.key().size() > 64 || !(it.key().startsWith("actor") || it.key().startsWith("troop") || it.key().startsWith("member"))
                    || !ActorRecoveryValueFromJson(it.value(), value)) { return false; }
                fields.insert(it.key(), value);
            }
            return true;
        }

        bool ActorRecoveryMembersValid(const QJsonArray& members, const QString& troop, const QString& selected, const QStringList& removed)
        {
            if (members.size() > ActorTroopDraftRecoveryService::MaximumEntries) { return false; }
            QSet<QString> ids;
            for (const auto& value : members)
            {
                if (!value.isObject()) { return false; }
                const auto member = value.toObject();
                if (member.size() != 11) { return false; }
                for (const auto* key : {"LinkId", "TroopRecordId", "ActorRecordId", "ActorSubjectRef", "Role"})
                {
                    if (!ActorRecoveryString(member[key], 512)) { return false; }
                }
                const auto id = member["LinkId"].toString();
                if (id.isEmpty() || troop.isEmpty() || member["TroopRecordId"] != troop || ids.contains(id) || removed.contains(id)) { return false; }
                ids.insert(id);
                for (const auto* key : {"MinimumCount", "MaximumCount"})
                {
                    const auto count = member[key];
                    if (!count.isDouble() || count.toDouble() < 1 || count.toDouble() > std::numeric_limits<unsigned int>::max()
                        || std::floor(count.toDouble()) != count.toDouble()) { return false; }
                }
                if (!member["Weight"].isDouble() || !std::isfinite(member["Weight"].toDouble())
                    || !member["Required"].isBool()) { return false; }
                QStringList conditions, evidence;
                if (!ActorRecoveryStrings(member["Conditions"], conditions) || !ActorRecoveryStrings(member["EvidenceIds"], evidence, 512)) { return false; }
            }
            QSet<QString> deleted;
            for (const auto& id : removed)
            {
                if (id.isEmpty() || deleted.contains(id)) { return false; }
                deleted.insert(id);
            }
            return selected.isEmpty() || ids.contains(selected);
        }

        bool ActorRecoveryPayloadFromJson(const QJsonObject& object, ActorTroopDraftRecovery& draft)
        {
            if (object.size() != 16 || !ActorRecoveryString(object["Actor"], 512) || !ActorRecoveryString(object["Troop"], 512)
                || !ActorRecoveryString(object["Member"], 512) || !object["Tab"].isDouble()
                || (object["Tab"].toDouble() != 0 && object["Tab"].toDouble() != 1)
                || !object["ActorDirty"].isBool() || !object["TroopDirty"].isBool()
                || !object["MemberDirty"].isBool() || !object["RefreshPending"].isBool()
                || !ActorRecoveryFieldsFromJson(object["Values"], draft.m_values)
                || !object["Members"].isArray() || !ActorRecoveryStrings(object["RemovedMembers"], draft.m_removed, 512)) { return false; }
            draft.m_actor = object["Actor"].toString();
            draft.m_troop = object["Troop"].toString();
            draft.m_member = object["Member"].toString();
            draft.m_tab = object["Tab"].toInt();
            draft.m_actorDirty = object["ActorDirty"].toBool();
            draft.m_troopDirty = object["TroopDirty"].toBool();
            draft.m_memberDirty = object["MemberDirty"].toBool();
            draft.m_refreshPending = object["RefreshPending"].toBool();
            draft.m_members = object["Members"].toArray();
            return draft.IsDirty() && (!draft.m_actorDirty || !draft.m_actor.isEmpty())
                && (!(draft.m_troopDirty || draft.m_memberDirty) || !draft.m_troop.isEmpty())
                && (draft.m_removed.isEmpty() || !draft.m_troop.isEmpty())
                && ActorRecoveryMembersValid(draft.m_members, draft.m_troop, draft.m_member, draft.m_removed);
        }
    } // namespace

    ActorTroopDraftRecoveryService::ActorTroopDraftRecoveryService(const QString& recoveryRoot)
        : m_root(recoveryRoot.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(recoveryRoot).absoluteFilePath()))
    {
    }

    ActorTroopDraftRecoveryService::~ActorTroopDraftRecoveryService() = default;

    QString ActorTroopDraftRecoveryService::DefaultRecoveryRoot()
    {
#ifdef Q_OS_WIN
        const QString base = qEnvironmentVariable("LOCALAPPDATA");
#else
        const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#endif
        return base.isEmpty() || !QDir::isAbsolutePath(base) ? QString() : QDir(base).filePath(QStringLiteral("FOA-SDK/Recovery/ActorTroopDrafts"));
    }

    bool ActorTroopDraftRecoveryService::Bind(const QString& workspaceId, const QString& workspaceRoot,
        const QString& workspaceDocument, QString& error)
    {
        m_lock.reset();
        m_path.clear();
        m_canWrite = false;
        m_workspaceId = workspaceId;
        m_workspaceRoot = ActorRecoveryIdentity(workspaceRoot);
        m_workspaceDocument = workspaceDocument.isEmpty() ? QString() : ActorRecoveryIdentity(workspaceDocument);
        if (m_root.isEmpty() || !QDir::isAbsolutePath(m_root) || workspaceId.isEmpty()
            || m_workspaceRoot.isEmpty() || !QFileInfo(workspaceRoot).isDir()
            || (!workspaceDocument.isEmpty() && (m_workspaceDocument.isEmpty() || !QFileInfo(workspaceDocument).isFile())))
        {
            error = QStringLiteral("Recovery needs an available workspace and local application-data folder.");
            return false;
        }
        if (!ActorRecoveryPathHasNoLinks(m_root) || !QDir().mkpath(m_root) || !ActorRecoveryPathHasNoLinks(m_root))
        {
            error = QStringLiteral("The recovery folder is unavailable or uses a linked path.");
            return false;
        }
        const QJsonObject identity{ { "id", m_workspaceId }, { "root", m_workspaceRoot }, { "document", m_workspaceDocument } };
        const QByteArray key = QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),
            QCryptographicHash::Sha256).toHex();
        m_path = QDir(m_root).filePath(QString::fromLatin1(key) + QStringLiteral(".actortroopdrafts.json"));
        if (!ActorRecoveryPathHasNoLinks(m_path) || !ActorRecoveryPathHasNoLinks(m_path + QStringLiteral(".lock")))
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

    bool ActorTroopDraftRecoveryService::CheckPath(QString& error) const
    {
        if (m_path.isEmpty() || !m_lock || !m_lock->isLocked() || !ActorRecoveryPathHasNoLinks(m_path))
        {
            error = QStringLiteral("Recovery storage is not available for this workspace.");
            return false;
        }
        return true;
    }

    bool ActorTroopDraftRecoveryService::Clear(QString& error)
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
    void ActorTroopDraftRecoveryService::Release()
    {
        m_lock.reset();
        m_path.clear();
        m_canWrite = false;
    }

    ActorTroopDraftRecoveryRead ActorTroopDraftRecoveryService::Read()
    {
        ActorTroopDraftRecoveryRead result;
        m_canWrite = false;
        if (!CheckPath(result.m_error)) { return result; }
        result.m_exists = QFileInfo::exists(m_path);
        if (!result.m_exists) { m_canWrite = true; return result; }
        QFile file(m_path);
        if (file.size() <= 0 || file.size() > MaximumDocumentBytes || !file.open(QIODevice::ReadOnly))
        {
            result.m_error = QStringLiteral("The recovery copy is unreadable or exceeds the 2 MiB limit. It has been kept.");
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
        if (object["Format"] != QStringLiteral("FOA-SDK.ActorTroopDraftRecovery")
            || !object["SchemaVersion"].isDouble() || object["SchemaVersion"].toDouble() != 1.0)
        {
            result.m_error = QStringLiteral("This Editor cannot read the recovery format. The copy has been kept.");
            return result;
        }
        if (object["WorkspaceId"] != m_workspaceId || object["WorkspaceRoot"] != m_workspaceRoot
            || object["WorkspaceDocument"] != m_workspaceDocument)
        {
            result.m_error = QStringLiteral("The recovery copy belongs to another workspace. It has been kept.");
            return result;
        }
        if (!ActorRecoveryPayloadFromJson(object, result.m_draft))
        {
            result.m_error = QStringLiteral("The recovery copy has unsupported or incomplete form values. It has been kept.");
            return result;
        }
        result.m_valid = true;
        m_canWrite = true;
        return result;
    }

    bool ActorTroopDraftRecoveryService::Write(const ActorTroopDraftRecovery& draft, QString& error)
    {
        if (!CheckPath(error)) { return false; }
        if (!m_canWrite || !draft.IsDirty() || draft.m_values.isEmpty() || draft.m_values.size() > 64
            || draft.m_members.size() > MaximumEntries || draft.m_removed.size() > MaximumEntries)
        {
            error = QStringLiteral("Recovery could not record these form values. The previous copy has been kept.");
            return false;
        }
        QJsonObject fields;
        qint64 payloadBytes = 0;
        for (auto it = draft.m_values.cbegin(); it != draft.m_values.cend(); ++it)
        {
            if ((it->metaType().id() == QMetaType::QString && it->toString().size() > MaximumFieldCharacters)
                || (it->metaType().id() == QMetaType::QStringList && it->toStringList().size() > MaximumEntries))
            {
                error = QStringLiteral("Recovery has an oversized field. The previous copy has been kept.");
                return false;
            }
            const auto encoded = ActorRecoveryValueToJson(it.value());
            QVariant checked;
            if (!ActorRecoveryValueFromJson(encoded, checked))
            {
                error = QStringLiteral("Recovery has an unsupported field. The previous copy has been kept.");
                return false;
            }
            payloadBytes += QJsonDocument(encoded).toJson(QJsonDocument::Compact).size();
            if (payloadBytes > MaximumDocumentBytes)
            {
                error = QStringLiteral("Recovery exceeds the 2 MiB limit. The previous copy has been kept.");
                return false;
            }
            fields.insert(it.key(), encoded);
        }
        const QJsonObject object{{"Format", "FOA-SDK.ActorTroopDraftRecovery"}, {"SchemaVersion", 1},
            {"WorkspaceId", m_workspaceId}, {"WorkspaceRoot", m_workspaceRoot}, {"WorkspaceDocument", m_workspaceDocument},
            {"Values", fields}, {"Members", draft.m_members}, {"RemovedMembers", QJsonArray::fromStringList(draft.m_removed)},
            {"Actor", draft.m_actor}, {"Troop", draft.m_troop}, {"Member", draft.m_member}, {"Tab", draft.m_tab},
            {"ActorDirty", draft.m_actorDirty}, {"TroopDirty", draft.m_troopDirty},
            {"MemberDirty", draft.m_memberDirty}, {"RefreshPending", draft.m_refreshPending}};
        ActorTroopDraftRecovery checked;
        if (!ActorRecoveryPayloadFromJson(object, checked))
        {
            error = QStringLiteral("Recovery has invalid member or form state. The previous copy has been kept.");
            return false;
        }
        const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
        if (bytes.size() > MaximumDocumentBytes)
        {
            error = QStringLiteral("Recovery exceeds the 2 MiB limit. Save your drafts to preserve all changes.");
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
} // namespace TaintedGrailModdingSDK
