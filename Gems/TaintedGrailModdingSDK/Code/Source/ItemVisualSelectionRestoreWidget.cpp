/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "ItemVisualSelectionRestoreBridge.h"

#include <QAbstractItemModel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr int EntryRowRole = Qt::UserRole;
    }

    ItemVisualSelectionRestoreBridge::ItemVisualSelectionRestoreBridge(QWidget* selector)
        : QObject(selector)
    {
        if (!selector)
        {
            return;
        }
        m_grid = selector->findChild<QListWidget*>(QStringLiteral("FOAItemViewerThumbnailGrid"));
        for (QTableWidget* table : selector->findChildren<QTableWidget*>())
        {
            if (table->accessibleName() == tr("Selected asset details")
                || table->accessibleName() == tr("Evidence-backed preview products"))
            {
                m_table = table;
                break;
            }
        }
        if (!m_grid || !m_table)
        {
            return;
        }
        connect(m_table->model(), &QAbstractItemModel::rowsInserted, this, [this]() { ScheduleRestore(); });
        connect(m_table->model(), &QAbstractItemModel::modelReset, this, [this]() { ScheduleRestore(); });
    }

    void ItemVisualSelectionRestoreBridge::ScheduleRestore()
    {
        if (m_pending)
        {
            return;
        }
        m_pending = true;
        QTimer::singleShot(0, this, [this]()
        {
            m_pending = false;
            RestoreSelection();
        });
    }

    void ItemVisualSelectionRestoreBridge::RestoreSelection()
    {
        if (!m_grid || !m_table || !m_grid->currentItem())
        {
            return;
        }
        const int row = m_grid->currentItem()->data(EntryRowRole).toInt();
        if (row >= 0 && row < m_table->rowCount() && m_table->currentRow() != row)
        {
            m_table->selectRow(row);
        }
    }
}
