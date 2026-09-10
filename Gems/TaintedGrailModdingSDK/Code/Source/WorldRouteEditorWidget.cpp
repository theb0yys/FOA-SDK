/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "WorldRouteEditorWidget.h"
#include "FoundationService.h"
#include "WorldPlanningService.h"
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzCore/std/algorithm.h>
#include <QMap>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsTextItem>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QUuid>
#include <cmath>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString RQ(const AZStd::string& value) { return QString::fromUtf8(value.c_str()); }
        AZStd::string RA(const QString& value)
        { const auto bytes = value.toUtf8(); return {bytes.constData(), static_cast<size_t>(bytes.size())}; }
        QString WorldLabel(const CatalogRecord& record)
        { return RQ(record.m_displayName) + " [" + RQ(record.m_recordKind) + " / " + RQ(record.m_recordId).right(8) + "]"; }
        void WorldChoice(QComboBox* combo)
        {
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            combo->setMinimumContentsLength(16); combo->setMaxVisibleItems(20);
        }
        void WorldOpen(const char* name)
        { AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::OpenViewPane, name); }
        QTableWidgetItem* WorldCell(const QString& text, const AZStd::string& id = {})
        {
            auto* cell = new QTableWidgetItem(text); cell->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            cell->setData(Qt::UserRole, RQ(id)); return cell;
        }
    }
    WorldRouteEditorWidget::WorldRouteEditorWidget(QWidget* parent) : QWidget(parent)
    {
        setObjectName("worldRouteEditor");
        auto* outer = new QVBoxLayout(this);
        outer->addWidget(new QLabel(tr("World and Route Editor"), this));
        auto* intro = new QLabel(tr("Create world places and plan roads or routes between them. Positions use scene-local plan units."), this);
        intro->setWordWrap(true); outer->addWidget(intro);
        auto* actions = new QHBoxLayout();
        const auto action = [this, actions](const QString& label, const char* name)
        { auto* b = new QPushButton(label, this); b->setObjectName(name); actions->addWidget(b); return b; };
        connect(action(tr("New world definition"), "worldNew"), &QPushButton::clicked, this, [this]() { Create(); });
        connect(action(tr("Choose or create mod"), "worldChooseMod"), &QPushButton::clicked, this, []() { WorldOpen("Tainted Grail Pack Manager"); });
        connect(action(tr("Road Atlas"), "worldOpenAtlas"), &QPushButton::clicked, this, []() { WorldOpen("Tainted Grail Map Editor (Road Atlas)"); });
        outer->addLayout(actions);
        m_summary = new QLabel(this); m_summary->setWordWrap(true); outer->addWidget(m_summary);
        m_records = new QComboBox(this); m_records->setObjectName("worldRecords"); WorldChoice(m_records); outer->addWidget(m_records);
        connect(m_records, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]()
        {
            if (m_loading) { return; }
            if (m_dirty)
            {
                const QSignalBlocker block(m_records); m_records->setCurrentIndex(m_records->findData(RQ(m_id)));
                Status(tr("Save or revert your changes before selecting another definition.")); return;
            }
            Load(RA(m_records->currentData().toString()));
        });
        m_tabs = new QTabWidget(this); m_tabs->setObjectName("worldTabs"); outer->addWidget(m_tabs, 1);
        auto* scroll = new QScrollArea(m_tabs); scroll->setObjectName("worldScroll"); scroll->setWidgetResizable(true);
        m_form = new QWidget(scroll); auto* form = new QFormLayout(m_form); form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        const auto line = [this, form](const QString& label, const char* object, int length)
        { auto* w = new QLineEdit(m_form); w->setObjectName(object); w->setMaxLength(length); form->addRow(label, w); return w; };
        m_name = line(tr("Name"), "worldName", 512);
        m_parent = new QComboBox(m_form); m_parent->setObjectName("worldParent"); WorldChoice(m_parent); form->addRow(tr("Parent / scene"), m_parent);
        m_description = line(tr("Description"), "worldDescription", 2048);
        m_road = new QComboBox(m_form); m_road->setObjectName("worldRoad"); WorldChoice(m_road); form->addRow(tr("Referenced road"), m_road);
        m_constraints = line(tr("Travel constraints"), "worldConstraints", 2048);
        m_position = new QCheckBox(tr("Use a location plan position"), m_form); m_position->setObjectName("worldHasPosition"); form->addRow(m_position);
        const auto coordinate = [this, form](const char* name, const QString& label)
        {
            auto* w = new QDoubleSpinBox(m_form); w->setObjectName(name); w->setRange(-1000000, 1000000);
            w->setDecimals(3); form->addRow(label, w); return w;
        };
        m_x = coordinate("worldX", tr("Plan X")); m_z = coordinate("worldZ", tr("Plan Z"));
        auto* basis = new QLabel(tr("Plan units describe your authored layout. Bind game coordinates only through separately reviewed map evidence."), m_form);
        basis->setWordWrap(true); form->addRow(basis);
        scroll->setWidget(m_form); m_tabs->addTab(scroll, tr("Definition"));
        auto* graphScroll = new QScrollArea(m_tabs); graphScroll->setWidgetResizable(true); graphScroll->setObjectName("worldGraphScroll");
        m_graphControls = new QWidget(graphScroll); auto* graphLayout = new QVBoxLayout(m_graphControls);
        const auto combo = [this](const char* object)
        { auto* w = new QComboBox(m_graphControls); w->setObjectName(object); WorldChoice(w); return w; };
        m_location = combo("worldNodeLocation"); m_from = combo("worldEdgeFrom"); m_to = combo("worldEdgeTo"); m_mode = combo("worldEdgeMode");
        for (const auto& mode : {"walk", "ride", "boat"}) { m_mode->addItem(QString::fromUtf8(mode), QString::fromUtf8(mode)); }
        m_nodeNotes = new QLineEdit(m_graphControls); m_nodeNotes->setObjectName("worldNodeNotes"); m_nodeNotes->setMaxLength(1024);
        auto* nodeForm = new QFormLayout(); nodeForm->addRow(tr("Location"), m_location); nodeForm->addRow(tr("Node notes"), m_nodeNotes); graphLayout->addLayout(nodeForm);
        const auto buttons = [this, graphLayout](bool edge)
        {
            auto* row = new QHBoxLayout();
            for (int op = 0; op < 3; ++op)
            {
                auto* b = new QPushButton(op == 0 ? tr("Add") : op == 1 ? tr("Update selected") : tr("Remove selected"), m_graphControls);
                b->setObjectName(QString(edge ? "worldEdge" : "worldNode") + (op == 0 ? "Add" : op == 1 ? "Update" : "Remove"));
                row->addWidget(b); connect(b, &QPushButton::clicked, this, [this, edge, op]() { ChangeGraphRow(edge, op); });
            }
            graphLayout->addLayout(row);
        };
        const auto table = [this, graphLayout](const char* name, const QStringList& headers)
        {
            auto* w = new QTableWidget(0, headers.size(), m_graphControls); w->setObjectName(name); w->setHorizontalHeaderLabels(headers);
            w->setSelectionBehavior(QAbstractItemView::SelectRows); w->setSelectionMode(QAbstractItemView::SingleSelection);
            w->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); w->horizontalHeader()->setStretchLastSection(true);
            w->setMinimumHeight(150); graphLayout->addWidget(w); return w;
        };
        buttons(false); m_nodes = table("worldNodeTable", {tr("Location"), tr("Notes")});
        auto* edgeForm = new QFormLayout();
        edgeForm->addRow(tr("From"), m_from); edgeForm->addRow(tr("To"), m_to); edgeForm->addRow(tr("Travel mode"), m_mode);
        m_twoWay = new QCheckBox(tr("Two-way connection"), m_graphControls); m_twoWay->setObjectName("worldEdgeTwoWay"); m_twoWay->setChecked(true); edgeForm->addRow(m_twoWay);
        m_cost = new QDoubleSpinBox(m_graphControls); m_cost->setObjectName("worldEdgeCost"); m_cost->setRange(0.001, 1000000); m_cost->setDecimals(3); m_cost->setValue(1);
        edgeForm->addRow(tr("Planning cost"), m_cost);
        m_edgeNotes = new QLineEdit(m_graphControls); m_edgeNotes->setObjectName("worldEdgeNotes"); m_edgeNotes->setMaxLength(1024); edgeForm->addRow(tr("Edge notes"), m_edgeNotes);
        graphLayout->addLayout(edgeForm); buttons(true);
        m_edges = table("worldEdgeTable", {tr("From"), tr("To"), tr("Mode"), tr("Direction"), tr("Cost")});
        graphScroll->setWidget(m_graphControls); m_tabs->addTab(graphScroll, tr("Nodes and edges"));
        auto* previewPage = new QWidget(m_tabs); auto* previewLayout = new QVBoxLayout(previewPage);
        m_preview = new QLabel(previewPage); m_preview->setObjectName("worldPreviewSummary"); m_preview->setTextFormat(Qt::PlainText); m_preview->setWordWrap(true); previewLayout->addWidget(m_preview);
        m_scene = new QGraphicsScene(this); m_graph = new QGraphicsView(m_scene, previewPage); m_graph->setObjectName("worldGraph");
        m_graph->setRenderHint(QPainter::Antialiasing); m_graph->setBackgroundBrush(QColor("#202830")); m_graph->setMinimumSize(200, 180);
        previewLayout->addWidget(m_graph, 1); m_tabs->addTab(previewPage, tr("Visual preview"));
        connect(m_tabs, &QTabWidget::currentChanged, this, [this]() { Preview(); });
        connect(m_nodes, &QTableWidget::itemSelectionChanged, this, [this]() { SelectGraphRow(false); });
        connect(m_edges, &QTableWidget::itemSelectionChanged, this, [this]() { SelectGraphRow(true); });
        auto* saves = new QHBoxLayout();
        m_save = new QPushButton(tr("Save definition"), this); m_save->setObjectName("worldSave"); saves->addWidget(m_save);
        auto* revert = new QPushButton(tr("Revert changes"), this); revert->setObjectName("worldRevert"); saves->addWidget(revert); saves->addStretch(); outer->addLayout(saves);
        m_status = new QLabel(this); m_status->setObjectName("worldStatus"); m_status->setTextFormat(Qt::PlainText); m_status->setWordWrap(true); outer->addWidget(m_status);
        connect(m_save, &QPushButton::clicked, this, [this]() { Save(); });
        connect(revert, &QPushButton::clicked, this, [this]() { const auto id = m_id; m_dirty = false; RefreshChoices(); Load(id); Status(tr("Reloaded the saved definition.")); });
        for (auto* field : {m_name, m_description, m_constraints}) { connect(field, &QLineEdit::textEdited, this, [this]() { MarkDirty(); }); }
        for (auto* field : {m_parent, m_road}) { connect(field, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { MarkDirty(); }); }
        connect(m_position, &QCheckBox::toggled, this, [this](bool checked)
        {
            m_x->setEnabled(checked); m_z->setEnabled(checked);
            if (!m_loading) { m_place.m_x = checked ? m_x->value() : 0; m_place.m_z = checked ? m_z->value() : 0; MarkDirty(); }
        });
        connect(m_x, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value)
        { if (!m_loading) { m_place.m_x = value; MarkDirty(); } });
        connect(m_z, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value)
        { if (!m_loading) { m_place.m_z = value; MarkDirty(); } });
        connect(m_cost, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this]() { m_edgeCostEdited = true; });
        if (FoundationService::Get().GetWorkspaceFilePath().empty()) { FoundationService::Get().RefreshLocalSetup(); }
        FoundationNotificationBus::Handler::BusConnect(); RefreshChoices(); Load({});
    }
    WorldRouteEditorWidget::~WorldRouteEditorWidget() { FoundationNotificationBus::Handler::BusDisconnect(); }
    void WorldRouteEditorWidget::closeEvent(QCloseEvent* event)
    {
        if (m_dirty && QMessageBox::warning(this, tr("Unsaved world definition"), tr("Discard your world changes and close?"),
            QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Discard) { event->ignore(); return; }
        QWidget::closeEvent(event);
    }
    void WorldRouteEditorWidget::resizeEvent(QResizeEvent* event)
    { QWidget::resizeEvent(event); if (m_graph && !m_scene->items().isEmpty()) { m_graph->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio); } }
    void WorldRouteEditorWidget::Status(const QString& text) { m_status->setText(text); }
    bool WorldRouteEditorWidget::SameWorkspace() const
    {
        const auto& service = FoundationService::Get(); const auto& workspace = service.GetWorkspace(); const auto* profile = workspace.FindActiveGameProfile();
        return profile && workspace.m_workspaceId == m_workspaceId && workspace.m_activeGameProfileId == m_profileId
            && service.GetWorkspaceFilePath() == m_workspacePath && profile->m_gameVersion == m_gameVersion
            && profile->m_branch == m_branch && profile->m_runtimeTarget == m_runtimeTarget;
    }
    void WorldRouteEditorWidget::OnFoundationChanged()
    {
        if (m_saving) { return; }
        if (m_dirty) { Preview(); Status(tr("The catalog or workspace changed. Save in the original mod, or revert your preserved draft.")); return; }
        const auto id = m_id; RefreshChoices(); Load(id);
    }
    void WorldRouteEditorWidget::RefreshChoices()
    {
        const QSignalBlocker block(m_records); const auto selected = m_records->currentData(); m_records->clear(); m_records->addItem(tr("Choose a world definition"), "");
        const auto& service = FoundationService::Get(); const auto& catalog = service.GetCatalog();
        for (const auto& record : catalog.GetRecords())
        {
            if (catalog.FindWorldPlace(record.m_recordId) || catalog.FindWorldPath(record.m_recordId))
            { m_records->addItem(WorldLabel(record), RQ(record.m_recordId)); }
        }
        m_records->setCurrentIndex(qMax(0, m_records->findData(selected)));
        const auto* pack = service.GetActivePack();
        m_summary->setText(tr("%1 places | %2 roads and routes | Active mod: %3").arg(catalog.GetWorldPlaces().size())
            .arg(catalog.GetWorldPaths().size()).arg(pack ? RQ(pack->m_displayName) : tr("none selected")));
    }
    void WorldRouteEditorWidget::Load(const AZStd::string& id)
    {
        m_loading = true; const auto& service = FoundationService::Get(); const auto& catalog = service.GetCatalog();
        const auto* record = catalog.FindByRecordId(id); const auto* place = catalog.FindWorldPlace(id); const auto* path = catalog.FindWorldPath(id);
        m_id = record && (place || path) ? id : AZStd::string{}; m_kind = m_id.empty() ? AZStd::string{} : record->m_recordKind;
        m_isPlace = path == nullptr; m_place = place ? *place : WorldPlaceProfile{}; m_path = path ? catalog.FindWorldPathDefinition(id) : WorldPathDefinition{};
        m_workspaceId = service.GetWorkspace().m_workspaceId; m_profileId = service.GetWorkspace().m_activeGameProfileId; m_workspacePath = service.GetWorkspaceFilePath();
        const auto* profile = service.GetWorkspace().FindActiveGameProfile();
        m_gameVersion = profile ? profile->m_gameVersion : AZStd::string{}; m_branch = profile ? profile->m_branch : AZStd::string{};
        m_runtimeTarget = profile ? profile->m_runtimeTarget : AZStd::string{};
        m_records->setCurrentIndex(qMax(0, m_records->findData(RQ(m_id))));
        m_name->setText(m_id.empty() ? QString{} : RQ(record->m_displayName)); m_description->setText(RQ(m_isPlace ? m_place.m_description : m_path.m_profile.m_description));
        m_parent->clear(); m_parent->addItem(tr("None"), ""); m_road->clear(); m_road->addItem(tr("None"), ""); m_location->clear();
        for (const auto& candidate : catalog.GetRecords())
        {
            if (candidate.m_domain != "world") { continue; }
            if (candidate.m_recordKind == (m_kind == "scene" ? "region" : "scene"))
            { m_parent->addItem(WorldLabel(candidate), RQ(candidate.m_recordId)); }
            if (candidate.m_recordKind == "road" && catalog.FindWorldPath(candidate.m_recordId))
            { m_road->addItem(WorldLabel(candidate), RQ(candidate.m_recordId)); }
            if (candidate.m_recordKind == "location" && catalog.FindWorldPlace(candidate.m_recordId))
            { m_location->addItem(WorldLabel(candidate), RQ(candidate.m_recordId)); }
        }
        m_parent->setCurrentIndex(qMax(0, m_parent->findData(RQ(m_isPlace ? m_place.m_parentRecordId : m_path.m_profile.m_sceneRecordId))));
        m_parent->setEnabled(m_kind != "region"); m_road->setEnabled(m_kind == "route"); m_constraints->setEnabled(!m_isPlace);
        m_road->setCurrentIndex(qMax(0, m_road->findData(RQ(m_path.m_profile.m_roadRecordId)))); m_constraints->setText(RQ(m_path.m_profile.m_travelConstraints));
        m_position->setEnabled(m_kind == "location"); m_position->setChecked(m_place.m_hasPosition); m_x->setValue(m_place.m_x); m_z->setValue(m_place.m_z);
        m_x->setEnabled(m_kind == "location" && m_place.m_hasPosition); m_z->setEnabled(m_kind == "location" && m_place.m_hasPosition);
        m_form->setEnabled(!m_id.empty()); m_graphControls->setEnabled(!m_isPlace && !m_id.empty());
        m_dirty = false; RefreshGraphRows(); m_loading = false; Preview();
    }
    void WorldRouteEditorWidget::ReadFields()
    {
        if (m_isPlace)
        {
            m_place.m_description = RA(m_description->text()); m_place.m_parentRecordId = RA(m_parent->currentData().toString());
            m_place.m_hasPosition = m_position->isChecked();
            if (!m_place.m_hasPosition) { m_place.m_x = 0; m_place.m_z = 0; }
        }
        else
        {
            m_path.m_profile.m_description = RA(m_description->text()); m_path.m_profile.m_sceneRecordId = RA(m_parent->currentData().toString());
            m_path.m_profile.m_roadRecordId = RA(m_road->currentData().toString()); m_path.m_profile.m_travelConstraints = RA(m_constraints->text());
        }
    }
    void WorldRouteEditorWidget::MarkDirty()
    { if (m_loading || m_id.empty()) { return; } ReadFields(); m_dirty = true; Preview(); Status(tr("Unsaved world changes.")); }
    void WorldRouteEditorWidget::RefreshGraphRows()
    {
        const QSignalBlocker nb(m_nodes), eb(m_edges), fb(m_from), tb(m_to);
        const auto from = m_from->currentData(), to = m_to->currentData();
        m_nodes->setRowCount(0); m_edges->setRowCount(0); m_from->clear(); m_to->clear();
        const auto& catalog = FoundationService::Get().GetCatalog(); QMap<QString, QString> names;
        for (const auto& node : m_path.m_nodes)
        {
            const auto* record = catalog.FindByRecordId(node.m_locationRecordId); const auto label = record ? WorldLabel(*record) : RQ(node.m_locationRecordId);
            const int row = m_nodes->rowCount(); m_nodes->insertRow(row); m_nodes->setItem(row, 0, WorldCell(label, node.m_nodeId));
            m_nodes->setItem(row, 1, WorldCell(RQ(node.m_notes))); m_from->addItem(label, RQ(node.m_nodeId)); m_to->addItem(label, RQ(node.m_nodeId)); names[RQ(node.m_nodeId)] = label;
        }
        for (const auto& edge : m_path.m_edges)
        {
            const int row = m_edges->rowCount(); m_edges->insertRow(row); m_edges->setItem(row, 0, WorldCell(names.value(RQ(edge.m_fromNodeId)), edge.m_edgeId));
            m_edges->setItem(row, 1, WorldCell(names.value(RQ(edge.m_toNodeId)))); m_edges->setItem(row, 2, WorldCell(RQ(edge.m_travelMode)));
            m_edges->setItem(row, 3, WorldCell(edge.m_bidirectional ? tr("Two-way") : tr("One-way"))); m_edges->setItem(row, 4, WorldCell(QString::number(edge.m_travelCost)));
        }
        m_from->setCurrentIndex(qMax(0, m_from->findData(from))); m_to->setCurrentIndex(qMax(0, m_to->findData(to)));
    }
    void WorldRouteEditorWidget::SelectGraphRow(bool edge)
    {
        auto* table = edge ? m_edges : m_nodes; const int row = table->currentRow(); if (row < 0) { return; }
        const auto id = RA(table->item(row, 0)->data(Qt::UserRole).toString());
        if (edge)
        {
            for (const auto& e : m_path.m_edges) { if (e.m_edgeId == id)
            {
                m_from->setCurrentIndex(m_from->findData(RQ(e.m_fromNodeId))); m_to->setCurrentIndex(m_to->findData(RQ(e.m_toNodeId)));
                m_mode->setCurrentIndex(m_mode->findData(RQ(e.m_travelMode))); m_twoWay->setChecked(e.m_bidirectional); m_cost->setValue(e.m_travelCost); m_edgeCostEdited = false; m_edgeNotes->setText(RQ(e.m_notes)); break;
            } }
        }
        else { for (const auto& n : m_path.m_nodes) { if (n.m_nodeId == id)
        { m_location->setCurrentIndex(m_location->findData(RQ(n.m_locationRecordId))); m_nodeNotes->setText(RQ(n.m_notes)); break; } } }
    }
    void WorldRouteEditorWidget::ChangeGraphRow(bool edge, int operation)
    {
        if (m_isPlace || m_id.empty()) { return; }
        auto* table = edge ? m_edges : m_nodes; const int row = table->currentRow();
        if (operation != 0 && row < 0) { Status(tr("Select a row first.")); return; }
        const auto id = operation == 0 ? RA(QString(edge ? "custom.world-edge." : "custom.world-node.") + QUuid::createUuid().toString(QUuid::WithoutBraces))
            : RA(table->item(row, 0)->data(Qt::UserRole).toString());
        ReadFields(); auto candidate = m_path;
        if (edge)
        {
            auto it = AZStd::find_if(candidate.m_edges.begin(), candidate.m_edges.end(), [&id](const auto& e) { return e.m_edgeId == id; });
            if (operation == 2) { if (it != candidate.m_edges.end()) { candidate.m_edges.erase(it); } }
            else
            {
                WorldPathEdge value; value.m_edgeId = id; value.m_pathRecordId = m_id; value.m_fromNodeId = RA(m_from->currentData().toString()); value.m_toNodeId = RA(m_to->currentData().toString());
                value.m_travelMode = RA(m_mode->currentData().toString()); value.m_bidirectional = m_twoWay->isChecked(); value.m_travelCost = operation == 1 && !m_edgeCostEdited && it != candidate.m_edges.end() ? it->m_travelCost : m_cost->value(); value.m_notes = RA(m_edgeNotes->text());
                if (it == candidate.m_edges.end()) { candidate.m_edges.push_back(value); } else { *it = value; }
            }
        }
        else
        {
            auto it = AZStd::find_if(candidate.m_nodes.begin(), candidate.m_nodes.end(), [&id](const auto& n) { return n.m_nodeId == id; });
            if (operation == 2)
            {
                for (const auto& e : candidate.m_edges) { if (e.m_fromNodeId == id || e.m_toNodeId == id) { Status(tr("Remove this node's connections before removing the node.")); return; } }
                if (it != candidate.m_nodes.end()) { candidate.m_nodes.erase(it); }
            }
            else
            {
                WorldPathNode value; value.m_nodeId = id; value.m_pathRecordId = m_id; value.m_locationRecordId = RA(m_location->currentData().toString()); value.m_notes = RA(m_nodeNotes->text());
                if (it == candidate.m_nodes.end()) { candidate.m_nodes.push_back(value); } else { *it = value; }
            }
        }
        const auto analysis = WorldPlanningService::Analyze(candidate, m_kind, FoundationService::Get().GetCatalog());
        if (!analysis.IsValid()) { Status(RQ(analysis.m_errors.front())); return; }
        m_path = AZStd::move(candidate); RefreshGraphRows(); MarkDirty();
    }
    void WorldRouteEditorWidget::Create()
    {
        if (m_dirty) { Status(tr("Save or revert your draft before creating another definition.")); return; }
        QDialog dialog(this); dialog.setWindowTitle(tr("New world definition")); dialog.setObjectName("worldCreateDialog");
        auto* layout = new QVBoxLayout(&dialog); auto* form = new QFormLayout();
        QComboBox kind(&dialog), parent(&dialog); QLineEdit name(&dialog); QLabel errorLabel(&dialog);
        kind.setObjectName("worldCreateKind"); parent.setObjectName("worldCreateParent"); name.setObjectName("worldCreateName"); errorLabel.setObjectName("worldCreateStatus");
        name.setMaxLength(512); errorLabel.setWordWrap(true); WorldChoice(&parent);
        for (const auto& value : {"region", "scene", "location", "road", "route"}) { kind.addItem(QString::fromUtf8(value), QString::fromUtf8(value)); }
        form->addRow(tr("Kind"), &kind); form->addRow(tr("Name"), &name); form->addRow(tr("Parent / scene"), &parent); layout->addLayout(form); layout->addWidget(&errorLabel);
        const auto refresh = [&]()
        {
            parent.clear(); parent.addItem(tr("None"), ""); parent.setEnabled(kind.currentData() != "region");
            const auto wanted = kind.currentData() == "scene" ? "region" : "scene";
            for (const auto& record : FoundationService::Get().GetCatalog().GetRecords())
            { if (record.m_domain == "world" && record.m_recordKind == wanted) { parent.addItem(WorldLabel(record), RQ(record.m_recordId)); } }
        };
        connect(&kind, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, refresh); refresh();
        QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
        buttons.button(QDialogButtonBox::Save)->setObjectName("worldCreateSave"); layout->addWidget(&buttons);
        AZStd::string created;
        const auto& serviceAtOpen = FoundationService::Get(); const auto workspaceAtOpen = serviceAtOpen.GetWorkspace();
        const auto originalWorkspace = serviceAtOpen.GetWorkspaceFilePath();
        const auto* profileAtOpen = workspaceAtOpen.FindActiveGameProfile();
        const auto gameVersionAtOpen = profileAtOpen ? profileAtOpen->m_gameVersion : AZStd::string{};
        const auto branchAtOpen = profileAtOpen ? profileAtOpen->m_branch : AZStd::string{};
        const auto runtimeAtOpen = profileAtOpen ? profileAtOpen->m_runtimeTarget : AZStd::string{};
        const auto packAtOpen = serviceAtOpen.GetActivePack() ? serviceAtOpen.GetActivePack()->m_packId : AZStd::string{};
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&]()
        {
            const auto& current = FoundationService::Get(); const auto& workspaceNow = current.GetWorkspace();
            const auto* profileNow = workspaceNow.FindActiveGameProfile();
            const auto packNow = current.GetActivePack() ? current.GetActivePack()->m_packId : AZStd::string{};
            if (!profileNow || current.GetWorkspaceFilePath() != originalWorkspace
                || workspaceNow.m_workspaceId != workspaceAtOpen.m_workspaceId
                || workspaceNow.m_activeGameProfileId != workspaceAtOpen.m_activeGameProfileId
                || profileNow->m_gameVersion != gameVersionAtOpen || profileNow->m_branch != branchAtOpen
                || profileNow->m_runtimeTarget != runtimeAtOpen || packNow != packAtOpen)
            { errorLabel.setText(tr("The workspace, profile or mod changed. Close and reopen this dialog.")); return; }
            const auto value = RA(kind.currentData().toString()); AZStd::string error; m_saving = true;
            auto& service = FoundationService::Get(); const bool isPlace = value != "road" && value != "route";
            const bool saved = isPlace ? service.CreateWorldPlace(value, RA(name.text()), RA(parent.currentData().toString()), created, &error)
                : service.CreateWorldPath(value, RA(name.text()), RA(parent.currentData().toString()), created, &error);
            m_saving = false; if (!saved) { errorLabel.setText(RQ(error)); return; } dialog.accept();
        });
        if (dialog.exec() == QDialog::Accepted) { RefreshChoices(); Load(created); m_tabs->setCurrentIndex(0); Status(tr("World definition created and saved.")); }
    }
    void WorldRouteEditorWidget::Save()
    {
        if (!SameWorkspace()) { Status(tr("Return to the original workspace and profile before saving this draft.")); return; }
        ReadFields(); AZStd::string error; m_saving = true; auto& service = FoundationService::Get();
        const bool saved = m_isPlace ? service.SaveWorldPlace(m_place, RA(m_name->text()), &error) : service.SaveWorldPath(m_path, RA(m_name->text()), &error);
        m_saving = false; if (!saved) { Status(RQ(error)); return; }
        const auto id = m_id; m_dirty = false; RefreshChoices(); Load(id); Status(tr("World definition saved."));
    }
}
