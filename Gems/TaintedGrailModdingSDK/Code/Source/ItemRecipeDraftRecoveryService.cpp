/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ItemRecipeDraftRecoveryService.h"

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
#include <QStandardPaths>
#include <cmath>
#include <limits>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool ItemRecoveryPathHasNoLinks(const QString& path)
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

        QString ItemRecoveryIdentity(const QString& path)
        {
            QString canonical = QFileInfo(path).canonicalFilePath();
#ifdef Q_OS_WIN
            canonical = canonical.toCaseFolded();
#endif
            return canonical;
        }

        bool ItemRecoveryString(const QJsonValue& value, qsizetype maximum = ItemRecipeDraftRecoveryService::MaximumFieldCharacters)
        {
            return value.isString() && value.toString().size() <= maximum;
        }

        bool ItemRecoveryKey(const QString& key)
        {
            const QString kind = key.section(':', 0, 0);
            return key == "acquisition:" || ((kind == "item" || kind == "recipe" || kind == "ingredient" || kind == "output")
                && key.contains(':') && key.mid(key.indexOf(':') + 1).size() <= 512);
        }

        QJsonArray ItemRecoveryValueToJson(const QVariant& value)
        {
            switch (value.metaType().id())
            {
            case QMetaType::QString: return { "string", value.toString() };
            case QMetaType::Int: return { "integer", value.toInt() };
            case QMetaType::Double: return { "number", value.toDouble() };
            case QMetaType::Bool: return { "boolean", value.toBool() };
            case QMetaType::QVariantMap:
            {
                const QVariantMap selection = value.toMap();
                if (selection.size() != 2 || !selection.contains("data") || !selection.contains("text")
                    || selection["text"].metaType().id() != QMetaType::QString
                    || (selection["data"].isValid() && selection["data"].metaType().id() != QMetaType::QString)) { return {}; }
                return { "selection", selection["data"].isValid() ? QJsonValue(selection["data"].toString()) : QJsonValue(),
                    selection["text"].toString() };
            }
            default: return {};
            }
        }

        bool ItemRecoveryValueFromJson(const QJsonValue& encoded, QVariant& value)
        {
            if (!encoded.isArray()) { return false; }
            const QJsonArray array = encoded.toArray();
            if (array.size() < 2 || !array[0].isString()) { return false; }
            const QString type = array[0].toString();
            const QJsonValue data = array[1];
            if (type == "selection")
            {
                if (array.size() != 3 || !(data.isNull() || ItemRecoveryString(data, 512)) || !ItemRecoveryString(array[2])) { return false; }
                value = QVariantMap{{ "data", data.isNull() ? QVariant() : QVariant(data.toString()) }, { "text", array[2].toString() }};
                return true;
            }
            if (array.size() != 2) { return false; }
            if (type == "string" && ItemRecoveryString(data)) { value = data.toString(); }
            else if (type == "boolean" && data.isBool()) { value = data.toBool(); }
            else if (type == "integer" && data.isDouble() && data.toDouble() >= std::numeric_limits<int>::min()
                && data.toDouble() <= std::numeric_limits<int>::max() && std::floor(data.toDouble()) == data.toDouble())
            {
                value = data.toInt();
            }
            else if (type == "number" && data.isDouble() && std::isfinite(data.toDouble())) { value = data.toDouble(); }
            else { return false; }
            return true;
        }

        bool ItemRecoveryFieldsFromJson(const QJsonValue& encoded, ItemRecipeFormValues& fields)
        {
            if (!encoded.isObject()) { return false; }
            const QJsonObject object = encoded.toObject();
            if (object.isEmpty() || object.size() > 64) { return false; }
            for (auto it = object.begin(); it != object.end(); ++it)
            {
                QVariant value;
                if (!it.key().startsWith("economy") || it.key().size() > 128 || !ItemRecoveryValueFromJson(it.value(), value)) { return false; }
                fields.insert(it.key(), value);
            }
            return true;
        }

        QJsonObject ItemRecoveryFieldsToJson(const ItemRecipeFormValues& fields)
        {
            QJsonObject object;
            for (auto it = fields.cbegin(); it != fields.cend(); ++it) { object.insert(it.key(), ItemRecoveryValueToJson(it.value())); }
            return object;
        }

        bool ItemRecoveryDraftsFromJson(const QJsonValue& encoded, QHash<QString, ItemRecipeDraft>& drafts)
        {
            if (!encoded.isObject()) { return false; }
            const QJsonObject object = encoded.toObject();
            if (object.isEmpty() || object.size() > ItemRecipeDraftRecoveryService::MaximumDrafts) { return false; }
            for (auto it = object.begin(); it != object.end(); ++it)
            {
                if (!ItemRecoveryKey(it.key()) || !it.value().isObject()) { return false; }
                const QJsonObject form = it.value().toObject();
                ItemRecipeDraft draft;
                if (form.size() != 2 || !ItemRecoveryFieldsFromJson(form["Values"], draft.m_values)
                    || !ItemRecoveryFieldsFromJson(form["Baseline"], draft.m_baseline) || draft.m_values.size() != draft.m_baseline.size()) { return false; }
                for (auto field = draft.m_values.cbegin(); field != draft.m_values.cend(); ++field)
                {
                    if (!draft.m_baseline.contains(field.key()) || field.value().metaType() != draft.m_baseline[field.key()].metaType()) { return false; }
                }
                drafts.insert(it.key(), draft);
            }
            return true;
        }
    } // namespace

    ItemRecipeDraftRecoveryService::ItemRecipeDraftRecoveryService(const QString& recoveryRoot)
        : m_root(recoveryRoot.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(recoveryRoot).absoluteFilePath()))
    {
    }

    ItemRecipeDraftRecoveryService::~ItemRecipeDraftRecoveryService() = default;

    QString ItemRecipeDraftRecoveryService::DefaultRecoveryRoot()
    {
#ifdef Q_OS_WIN
        const QString base = qEnvironmentVariable("LOCALAPPDATA");
#else
        const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#endif
        return base.isEmpty() || !QDir::isAbsolutePath(base) ? QString() : QDir(base).filePath(QStringLiteral("FOA-SDK/Recovery/ItemRecipeDrafts"));
    }

    bool ItemRecipeDraftRecoveryService::Bind(const QString& workspaceId, const QString& workspaceRoot,
        const QString& workspaceDocument, QString& error)
    {
        m_lock.reset();
        m_path.clear();
        m_canWrite = false;
        m_workspaceId = workspaceId;
        m_workspaceRoot = ItemRecoveryIdentity(workspaceRoot);
        m_workspaceDocument = workspaceDocument.isEmpty() ? QString() : ItemRecoveryIdentity(workspaceDocument);
        if (m_root.isEmpty() || !QDir::isAbsolutePath(m_root) || workspaceId.isEmpty()
            || m_workspaceRoot.isEmpty() || !QFileInfo(workspaceRoot).isDir()
            || (!workspaceDocument.isEmpty() && (m_workspaceDocument.isEmpty() || !QFileInfo(workspaceDocument).isFile())))
        {
            error = QStringLiteral("Recovery needs an available workspace and local application-data folder.");
            return false;
        }
        if (!ItemRecoveryPathHasNoLinks(m_root) || !QDir().mkpath(m_root) || !ItemRecoveryPathHasNoLinks(m_root))
        {
            error = QStringLiteral("The recovery folder is unavailable or uses a linked path.");
            return false;
        }
        const QJsonObject identity{ { "id", m_workspaceId }, { "root", m_workspaceRoot }, { "document", m_workspaceDocument } };
        const QByteArray key = QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),
            QCryptographicHash::Sha256).toHex();
        m_path = QDir(m_root).filePath(QString::fromLatin1(key) + QStringLiteral(".itemrecipedrafts.json"));
        if (!ItemRecoveryPathHasNoLinks(m_path) || !ItemRecoveryPathHasNoLinks(m_path + QStringLiteral(".lock")))
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

    bool ItemRecipeDraftRecoveryService::CheckPath(QString& error) const
    {
        if (m_path.isEmpty() || !m_lock || !m_lock->isLocked() || !ItemRecoveryPathHasNoLinks(m_path))
        {
            error = QStringLiteral("Recovery storage is not available for this workspace.");
            return false;
        }
        return true;
    }

    bool ItemRecipeDraftRecoveryService::Clear(QString& error)
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
    void ItemRecipeDraftRecoveryService::Release()
    {
        m_lock.reset();
        m_path.clear();
        m_canWrite = false;
    }

    ItemRecipeDraftRecoveryRead ItemRecipeDraftRecoveryService::Read()
    {
        ItemRecipeDraftRecoveryRead result;
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
        if (object["Format"] != QStringLiteral("FOA-SDK.ItemRecipeDraftRecovery")
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
        if (object.size() != 10 || !ItemRecoveryString(object["Item"], 512) || !ItemRecoveryString(object["Recipe"], 512)
            || !object["Tab"].isDouble() || object["Tab"].toDouble() < 0 || object["Tab"].toDouble() > 4
            || std::floor(object["Tab"].toDouble()) != object["Tab"].toDouble() || !object["RecipeExpanded"].isBool()
            || !ItemRecoveryDraftsFromJson(object["Drafts"], result.m_draft.m_drafts))
        {
            result.m_error = QStringLiteral("The recovery copy has unsupported or incomplete form values. It has been kept.");
            return result;
        }
        result.m_draft.m_item = object["Item"].toString();
        result.m_draft.m_recipe = object["Recipe"].toString();
        result.m_draft.m_tab = object["Tab"].toInt();
        result.m_draft.m_recipeExpanded = object["RecipeExpanded"].toBool();
        result.m_valid = true;
        m_canWrite = true;
        return result;
    }

    bool ItemRecipeDraftRecoveryService::Write(const ItemRecipeDraftRecovery& draft, QString& error)
    {
        if (!CheckPath(error)) { return false; }
        if (!m_canWrite || draft.m_drafts.isEmpty() || draft.m_drafts.size() > MaximumDrafts
            || draft.m_item.size() > 512 || draft.m_recipe.size() > 512 || draft.m_tab < 0 || draft.m_tab > 4)
        {
            error = QStringLiteral("Recovery could not record these form values. The previous copy has been kept.");
            return false;
        }
        QJsonObject drafts;
        qint64 payloadBytes = 0;
        for (auto it = draft.m_drafts.cbegin(); it != draft.m_drafts.cend(); ++it)
        {
            // Bound before allocating JSON for an oversized in-memory field set.
            if (it->m_values.size() > 64 || it->m_baseline.size() > 64)
            {
                error = QStringLiteral("Recovery has too many form fields. The previous copy has been kept.");
                return false;
            }
            for (const auto* fields : { &it->m_values, &it->m_baseline })
            {
                for (const QVariant& value : *fields)
                {
                    if ((value.metaType().id() == QMetaType::QString && value.toString().size() > MaximumFieldCharacters)
                        || (value.metaType().id() == QMetaType::Double && !std::isfinite(value.toDouble())))
                    {
                        error = QStringLiteral("Recovery has an oversized or invalid field. The previous copy has been kept.");
                        return false;
                    }
                }
            }
            const QJsonObject form{{ "Values", ItemRecoveryFieldsToJson(it->m_values) },
                { "Baseline", ItemRecoveryFieldsToJson(it->m_baseline) }};
            payloadBytes += QJsonDocument(form).toJson(QJsonDocument::Compact).size();
            if (payloadBytes > MaximumDocumentBytes)
            {
                error = QStringLiteral("Recovery exceeds the 2 MiB limit. The previous copy has been kept.");
                return false;
            }
            drafts.insert(it.key(), form);
        }
        QHash<QString, ItemRecipeDraft> checked;
        if (!ItemRecoveryDraftsFromJson(drafts, checked))
        {
            error = QStringLiteral("Recovery could not record these form values. The previous copy has been kept.");
            return false;
        }
        const QJsonObject object{{ "Format", "FOA-SDK.ItemRecipeDraftRecovery" }, { "SchemaVersion", 1 },
            { "WorkspaceId", m_workspaceId }, { "WorkspaceRoot", m_workspaceRoot }, { "WorkspaceDocument", m_workspaceDocument },
            { "Drafts", drafts }, { "Item", draft.m_item }, { "Recipe", draft.m_recipe },
            { "Tab", draft.m_tab }, { "RecipeExpanded", draft.m_recipeExpanded }};
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
