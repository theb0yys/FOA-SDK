/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QStringList>
#include <QString>
#include <QVariant>
#include <memory>

class QLockFile;

namespace TaintedGrailModdingSDK
{
    struct ActorTroopDraftRecovery
    {
        QHash<QString, QVariant> m_values;
        QJsonArray m_members;
        QStringList m_removed;
        QString m_actor;
        QString m_troop;
        QString m_member;
        bool m_actorDirty = false;
        bool m_troopDirty = false;
        bool m_memberDirty = false;
        bool m_refreshPending = false;
        int m_tab = 0;
        bool IsDirty() const { return m_actorDirty || m_troopDirty || m_memberDirty; }
        bool operator==(const ActorTroopDraftRecovery& other) const
        {
            return m_values == other.m_values && m_members == other.m_members && m_removed == other.m_removed
                && m_actor == other.m_actor && m_troop == other.m_troop && m_member == other.m_member
                && m_actorDirty == other.m_actorDirty && m_troopDirty == other.m_troopDirty
                && m_memberDirty == other.m_memberDirty && m_refreshPending == other.m_refreshPending && m_tab == other.m_tab;
        }
    };
    struct ActorTroopDraftRecoveryRead
    {
        bool m_exists = false;
        bool m_valid = false;
        QString m_error;
        ActorTroopDraftRecovery m_draft;
    };

    // Serial raw form storage, never a catalog or authoring command. A drained
    // worker may transfer ownership to the synchronous Editor close dispatch.
    class ActorTroopDraftRecoveryService
    {
    public:
        static constexpr qint64 MaximumDocumentBytes = 2 * 1024 * 1024;
        static constexpr qsizetype MaximumFieldCharacters = 16 * 1024;
        static constexpr qsizetype MaximumEntries = 4096;
        explicit ActorTroopDraftRecoveryService(const QString& recoveryRoot = DefaultRecoveryRoot());
        ~ActorTroopDraftRecoveryService();
        static QString DefaultRecoveryRoot();
        bool Bind(const QString& workspaceId, const QString& workspaceRoot,
            const QString& workspaceDocument, QString& error);
        void Release();
        ActorTroopDraftRecoveryRead Read();
        bool Write(const ActorTroopDraftRecovery& draft, QString& error);
        bool Clear(QString& error);
        QString Path() const { return m_path; }

    private:
        bool CheckPath(QString& error) const;
        QString m_root;
        QString m_path;
        QString m_workspaceId;
        QString m_workspaceRoot;
        QString m_workspaceDocument;
        std::unique_ptr<QLockFile> m_lock;
        bool m_canWrite = false;
    };
} // namespace TaintedGrailModdingSDK
