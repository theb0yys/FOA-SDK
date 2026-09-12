/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include "QuestAuthoringModels.h"
#include "FoundationNotificationBus.h"
#include <QWidget>
class QLabel; class QLineEdit; class QComboBox; class QTableWidget; class QTabWidget;
class QPushButton; class QGraphicsScene; class QGraphicsView; class QCloseEvent; class QResizeEvent;
namespace TaintedGrailModdingSDK
{
    class QuestStateInspectorWidget final : public QWidget, private FoundationNotificationBus::Handler
    {
    public:
        explicit QuestStateInspectorWidget(QWidget* parent = nullptr);
        ~QuestStateInspectorWidget() override;
    private:
        enum RowKind { Phase, Objective, Transition, Condition, Action, Outcome, Role, Requirement, State, Binding };
        void closeEvent(QCloseEvent*) override;
        void resizeEvent(QResizeEvent*) override;
        void OnFoundationChanged() override;
        void RefreshChoices();
        void Load(const AZStd::string&);
        void Populate();
        void MarkDirty();
        void Inspect();
        void Browse();
        void Create();
        void Save();
        void Revert();
        void RefreshRows();
        void EditRow(bool creating);
        void RemoveRow();
        void DrawGraph();
        AZStd::string RowId(int row) const;
        QString Context() const;
        void Status(const QString&);
        QuestAuthoringDraft m_draft, m_imported;
        AZStd::string m_id, m_revision, m_importError;
        QString m_context;
        bool m_external = false, m_dirty = false, m_loading = false, m_saving = false;
        QComboBox *m_records = nullptr, *m_kind = nullptr, *m_lifecycle = nullptr;
        QLineEdit *m_search = nullptr, *m_name = nullptr, *m_description = nullptr, *m_version = nullptr, *m_rowSearch = nullptr;
        QLabel *m_summary = nullptr, *m_status = nullptr, *m_graphSummary = nullptr, *m_details = nullptr;
        QTableWidget *m_rows = nullptr, *m_issues = nullptr;
        QTabWidget* m_tabs = nullptr;
        QPushButton* m_save = nullptr;
        QGraphicsScene* m_scene = nullptr;
        QGraphicsView* m_graph = nullptr;
    };
}
