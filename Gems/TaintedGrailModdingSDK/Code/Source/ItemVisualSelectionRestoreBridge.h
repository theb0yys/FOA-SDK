/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <QObject>
#include <QPointer>

class QListWidget;
class QTableWidget;
class QWidget;

namespace TaintedGrailModdingSDK
{
    //! Re-drives the validated table selection after a persisted thumbnail is rebuilt.
    class ItemVisualSelectionRestoreBridge final : public QObject
    {
    public:
        explicit ItemVisualSelectionRestoreBridge(QWidget* selector);

    private:
        void ScheduleRestore();
        void RestoreSelection();

        QPointer<QListWidget> m_grid;
        QPointer<QTableWidget> m_table;
        bool m_pending = false;
    };
}
