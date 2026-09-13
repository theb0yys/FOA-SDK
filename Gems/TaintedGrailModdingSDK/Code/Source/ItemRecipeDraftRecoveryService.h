/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <QHash>
#include <QString>
#include <QVariant>
#include <memory>

class QLockFile;

namespace TaintedGrailModdingSDK
{
    using ItemRecipeFormValues = QHash<QString, QVariant>;
    struct ItemRecipeDraft
    {
        ItemRecipeFormValues m_values;
        ItemRecipeFormValues m_baseline;
        bool operator==(const ItemRecipeDraft& other) const
        {
            return m_values == other.m_values && m_baseline == other.m_baseline;
        }
    };
    struct ItemRecipeDraftRecovery
    {
        QHash<QString, ItemRecipeDraft> m_drafts;
        QString m_item;
        QString m_recipe;
        int m_tab = 0;
        bool m_recipeExpanded = false;
        bool operator==(const ItemRecipeDraftRecovery& other) const
        {
            return m_drafts == other.m_drafts && m_item == other.m_item && m_recipe == other.m_recipe
                && m_tab == other.m_tab && m_recipeExpanded == other.m_recipeExpanded;
        }
    };
    struct ItemRecipeDraftRecoveryRead
    {
        bool m_exists = false;
        bool m_valid = false;
        QString m_error;
        ItemRecipeDraftRecovery m_draft;
    };

    // Serial raw form storage, never a catalog or authoring command. A drained
    // worker may transfer ownership to the synchronous Editor close dispatch.
    class ItemRecipeDraftRecoveryService
    {
    public:
        static constexpr qint64 MaximumDocumentBytes = 2 * 1024 * 1024;
        static constexpr qsizetype MaximumFieldCharacters = 16 * 1024;
        static constexpr qsizetype MaximumDrafts = 256;
        explicit ItemRecipeDraftRecoveryService(const QString& recoveryRoot = DefaultRecoveryRoot());
        ~ItemRecipeDraftRecoveryService();
        static QString DefaultRecoveryRoot();
        bool Bind(const QString& workspaceId, const QString& workspaceRoot,
            const QString& workspaceDocument, QString& error);
        void Release();
        ItemRecipeDraftRecoveryRead Read();
        bool Write(const ItemRecipeDraftRecovery& draft, QString& error);
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
