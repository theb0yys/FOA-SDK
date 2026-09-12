/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <memory>

class QLockFile;

namespace TaintedGrailModdingSDK
{
    // Raw authoring values, never a validated or active PackManifest.
    struct PackDraftRecovery
    {
        QMap<QString, QString> m_fields;
        QMap<QString, QString> m_baseline;
        QString m_packId;
        bool m_isNew = true;
        bool m_advancedExpanded = false;
    };

    struct PackDraftRecoveryRead
    {
        bool m_exists = false;
        bool m_valid = false;
        QString m_error;
        PackDraftRecovery m_draft;
    };

    // Confine each instance to one thread. Bind holds a per-workspace process lock.
    class PackDraftRecoveryService
    {
    public:
        static constexpr qint64 MaximumDocumentBytes = 256 * 1024;
        static constexpr qsizetype MaximumFieldCharacters = 16 * 1024;
        explicit PackDraftRecoveryService(const QString& recoveryRoot = DefaultRecoveryRoot());
        ~PackDraftRecoveryService();
        static QString DefaultRecoveryRoot();
        static QStringList FieldNames();
        bool Bind(const QString& workspaceId, const QString& workspaceRoot,
            const QString& workspaceDocument, QString& error);
        PackDraftRecoveryRead Read();
        bool Write(const PackDraftRecovery& draft, QString& error);
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
