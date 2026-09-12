/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "WorldModels.h"
#include "FoundationNotificationBus.h"
#include <QWidget>
class QComboBox; class QLabel; class QLineEdit; class QPushButton; class QCheckBox;
class QDoubleSpinBox; class QTableWidget; class QTabWidget; class QGraphicsView; class QGraphicsScene;
class QCloseEvent; class QResizeEvent;
namespace TaintedGrailModdingSDK
{
    class WorldRouteEditorWidget final : public QWidget, private FoundationNotificationBus::Handler
    {
    public:
        explicit WorldRouteEditorWidget(QWidget* parent = nullptr);
        ~WorldRouteEditorWidget() override;
    private:
        void closeEvent(QCloseEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void OnFoundationChanged() override;
        void RefreshChoices();
        void Load(const AZStd::string& id);
        void ReadFields();
        void MarkDirty();
        void Preview();
        void RefreshGraphRows();
        void SelectGraphRow(bool edge);
        void ChangeGraphRow(bool edge, int operation);
        void Create();
        void Save();
        void Status(const QString& text);
        bool SameWorkspace() const;
        QComboBox* m_records = nullptr;
        QComboBox* m_parent = nullptr;
        QComboBox* m_road = nullptr;
        QComboBox* m_location = nullptr;
        QComboBox* m_from = nullptr;
        QComboBox* m_to = nullptr;
        QComboBox* m_mode = nullptr;
        QLineEdit* m_name = nullptr;
        QLineEdit* m_description = nullptr;
        QLineEdit* m_constraints = nullptr;
        QLineEdit* m_nodeNotes = nullptr;
        QLineEdit* m_edgeNotes = nullptr;
        QCheckBox* m_position = nullptr;
        QCheckBox* m_twoWay = nullptr;
        QDoubleSpinBox* m_x = nullptr;
        QDoubleSpinBox* m_z = nullptr;
        QDoubleSpinBox* m_cost = nullptr;
        QTableWidget* m_nodes = nullptr;
        QTableWidget* m_edges = nullptr;
        QTabWidget* m_tabs = nullptr;
        QGraphicsView* m_graph = nullptr;
        QGraphicsScene* m_scene = nullptr;
        QLabel* m_summary = nullptr;
        QLabel* m_preview = nullptr;
        QLabel* m_status = nullptr;
        QPushButton* m_save = nullptr;
        QWidget* m_form = nullptr;
        QWidget* m_graphControls = nullptr;
        WorldPlaceProfile m_place;
        WorldPathDefinition m_path;
        AZStd::string m_id, m_kind;
        AZStd::string m_workspaceId, m_profileId, m_workspacePath, m_gameVersion, m_branch, m_runtimeTarget;
        bool m_isPlace = true;
        bool m_edgeCostEdited = false;
        bool m_loading = false;
        bool m_dirty = false;
        bool m_saving = false;
    };
}
