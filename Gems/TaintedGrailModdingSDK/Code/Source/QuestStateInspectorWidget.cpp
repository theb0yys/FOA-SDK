/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "QuestStateInspectorWidget.h"
#include "FoundationService.h"
#include "QuestAuthoringService.h"
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString IQ(const AZStd::string& v) { return QString::fromUtf8(v.c_str()); }
        AZStd::string IA(const QString& v) { const auto b = v.toUtf8(); return {b.constData(), static_cast<size_t>(b.size())}; }
        QLabel* QuestText(QWidget* parent)
        { auto* v = new QLabel(parent); v->setWordWrap(true); v->setTextFormat(Qt::PlainText); v->setTextInteractionFlags(Qt::TextSelectableByMouse); return v; }
    }
    QuestStateInspectorWidget::QuestStateInspectorWidget(QWidget* parent) : QWidget(parent)
    {
        setObjectName("questStateEditor");
        auto* outer = new QVBoxLayout(this);
        auto* heading = QuestText(this); heading->setText(tr("Quest and State Inspector - local quest authoring")); outer->addWidget(heading);
        auto* intro = QuestText(this); intro->setText(tr("Inspect quest documents or create quests for your mod. State keys describe authored defaults; the graph shows planned progression.")); outer->addWidget(intro);
        auto* actions = new QHBoxLayout();
        const auto button = [this, actions](const QString& text, const char* name)
        { auto* b = new QPushButton(text, this); b->setObjectName(name); actions->addWidget(b); return b; };
        connect(button(tr("Load quest document"), "questLoad"), &QPushButton::clicked, this, [this]() { Browse(); });
        connect(button(tr("New quest"), "questNew"), &QPushButton::clicked, this, [this]() { Create(); });
        connect(button(tr("Choose or create mod"), "questChooseMod"), &QPushButton::clicked, this, []()
        { AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::OpenViewPane, "Tainted Grail Pack Manager"); });
        outer->addLayout(actions);
        m_summary = QuestText(this); m_summary->setObjectName("questSummary"); outer->addWidget(m_summary);
        m_search = new QLineEdit(this); m_search->setObjectName("questSearch"); m_search->setPlaceholderText(tr("Search quests by name or identity")); outer->addWidget(m_search);
        m_records = new QComboBox(this); m_records->setObjectName("questRecords"); m_records->setMinimumContentsLength(20);
        m_records->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); m_records->setMaxVisibleItems(20); outer->addWidget(m_records);
        connect(m_search, &QLineEdit::textChanged, this, [this]() { RefreshChoices(); });
        connect(m_records, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]()
        {
            if (m_loading) { return; }
            if (m_dirty) { const QSignalBlocker block(m_records); m_records->setCurrentIndex(m_records->findData(m_external ? "@import" : IQ(m_id))); Status(tr("Save or revert your draft before selecting another quest.")); return; }
            Load(IA(m_records->currentData().toString()));
        });
        m_tabs = new QTabWidget(this); m_tabs->setObjectName("questTabs"); outer->addWidget(m_tabs, 1);
        auto* scroll = new QScrollArea(m_tabs); scroll->setWidgetResizable(true); auto* page = new QWidget(scroll);
        auto* form = new QFormLayout(page); form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        const auto line = [page, form](const QString& label, const char* name, int length)
        { auto* v = new QLineEdit(page); v->setObjectName(name); v->setMaxLength(length); form->addRow(label, v); return v; };
        m_name = line(tr("Name"), "questName", 512); m_description = line(tr("Summary"), "questDescription", 2048); m_version = line(tr("Content version"), "questVersion", 40);
        m_lifecycle = new QComboBox(page); m_lifecycle->setObjectName("questLifecycle");
        for (const auto* v : {"registered","available","offered","accepted","active","suspended","resolved","archived"}) { m_lifecycle->addItem(v); }
        form->addRow(tr("Declared lifecycle"), m_lifecycle);
        m_details = QuestText(page); m_details->setObjectName("questInspectionDetails"); form->addRow(tr("Inspection"), m_details);
        auto* note = QuestText(page); note->setText(tr("Save stores a local definition. Conditions and actions remain declarations; this editor does not advance quests or write game state.")); form->addRow(note);
        scroll->setWidget(page); m_tabs->addTab(scroll, tr("Definition"));
        auto* elements = new QWidget(m_tabs); auto* layout = new QVBoxLayout(elements);
        m_kind = new QComboBox(elements); m_kind->setObjectName("questElementKind");
        m_kind->addItems({tr("Phases"),tr("Objectives"),tr("Transitions"),tr("Conditions"),tr("Actions"),tr("Outcomes"),tr("Roles"),tr("Binding requirements"),tr("State keys"),tr("Catalog links")}); layout->addWidget(m_kind);
        m_rowSearch = new QLineEdit(elements); m_rowSearch->setObjectName("questElementSearch"); m_rowSearch->setPlaceholderText(tr("Filter elements by name, type or reference")); layout->addWidget(m_rowSearch);
        m_rows = new QTableWidget(elements); m_rows->setObjectName("questElements"); m_rows->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_rows->setSelectionBehavior(QAbstractItemView::SelectRows); m_rows->setSelectionMode(QAbstractItemView::SingleSelection);
        m_rows->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); m_rows->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_rows, 1);
        auto* rowActions = new QHBoxLayout();
        for (int op = 0; op < 3; ++op)
        {
            auto* b = new QPushButton(op == 0 ? tr("Add") : op == 1 ? tr("Edit selected") : tr("Remove selected"), elements);
            b->setObjectName(op == 0 ? "questElementAdd" : op == 1 ? "questElementEdit" : "questElementRemove"); rowActions->addWidget(b);
            connect(b, &QPushButton::clicked, this, [this, op]() { if (op == 2) { RemoveRow(); } else { EditRow(op == 0); } });
        }
        layout->addLayout(rowActions); m_tabs->addTab(elements, tr("Quest elements"));
        connect(m_kind, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { RefreshRows(); });
        connect(m_rowSearch, &QLineEdit::textChanged, this, [this]() { RefreshRows(); });
        connect(m_rows, &QTableWidget::cellDoubleClicked, this, [this]() { EditRow(false); });
        auto* graphPage = new QWidget(m_tabs); auto* graphLayout = new QVBoxLayout(graphPage);
        m_graphSummary = QuestText(graphPage); m_graphSummary->setObjectName("questGraphSummary"); graphLayout->addWidget(m_graphSummary);
        m_scene = new QGraphicsScene(this); m_graph = new QGraphicsView(m_scene, graphPage); m_graph->setObjectName("questGraph");
        m_graph->setRenderHint(QPainter::Antialiasing); m_graph->setDragMode(QGraphicsView::ScrollHandDrag); m_graph->setBackgroundBrush(QColor("#202830")); graphLayout->addWidget(m_graph, 1);
        auto* fit = new QPushButton(tr("Fit graph"), graphPage); fit->setObjectName("questFitGraph"); graphLayout->addWidget(fit);
        connect(fit, &QPushButton::clicked, this, [this]() { m_graph->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio); });
        m_tabs->addTab(graphPage, tr("Progression graph"));
        m_issues = new QTableWidget(0, 2, m_tabs); m_issues->setObjectName("questIssues"); m_issues->setHorizontalHeaderLabels({tr("Level"),tr("Details")});
        m_issues->setEditTriggers(QAbstractItemView::NoEditTriggers); m_issues->horizontalHeader()->setStretchLastSection(true); m_tabs->addTab(m_issues, tr("Validation"));
        connect(m_tabs, &QTabWidget::currentChanged, this, [this]() { if (m_tabs->currentIndex() == 2) { DrawGraph(); } });
        auto* saves = new QHBoxLayout(); m_save = new QPushButton(tr("Save quest"), this); m_save->setObjectName("questSave");
        auto* revert = new QPushButton(tr("Revert changes"), this); revert->setObjectName("questRevert"); saves->addWidget(m_save); saves->addWidget(revert); outer->addLayout(saves);
        m_status = QuestText(this); m_status->setObjectName("questStatus"); outer->addWidget(m_status);
        connect(m_save, &QPushButton::clicked, this, [this]() { Save(); }); connect(revert, &QPushButton::clicked, this, [this]() { Revert(); });
        for (auto* v : {m_name,m_description,m_version}) { connect(v, &QLineEdit::textEdited, this, [this]() { MarkDirty(); }); }
        connect(m_lifecycle, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { MarkDirty(); });
        if (FoundationService::Get().GetWorkspaceFilePath().empty()) { FoundationService::Get().RefreshLocalSetup(); }
        FoundationNotificationBus::Handler::BusConnect(); RefreshChoices(); Load({});
    }
    QuestStateInspectorWidget::~QuestStateInspectorWidget() { FoundationNotificationBus::Handler::BusDisconnect(); }
    QString QuestStateInspectorWidget::Context() const
    {
        const auto& s = FoundationService::Get(); const auto& w = s.GetWorkspace(); const auto* p = w.FindActiveGameProfile(); const auto* pack = s.GetActivePack();
        return IQ(s.GetWorkspaceFilePath()) + "|" + IQ(w.m_workspaceId) + "|" + IQ(w.m_activeGameProfileId)
            + "|" + (p ? IQ(p->m_gameVersion + "|" + p->m_branch + "|" + p->m_runtimeTarget) : QString{})
            + "|" + (pack ? IQ(pack->m_packId) : QString{}) + "|" + IQ(s.GetActivePackFilePath());
    }
    void QuestStateInspectorWidget::Status(const QString& text) { m_status->setText(text); }
    void QuestStateInspectorWidget::RefreshChoices()
    {
        const QSignalBlocker block(m_records); m_records->clear(); m_records->addItem(tr("Choose a quest"), "");
        const auto& service = FoundationService::Get(); const auto& catalog = service.GetCatalog(); const auto filter = m_search->text().trimmed();
        for (const auto& q : catalog.GetQuestProfiles())
        {
            const auto* record = catalog.FindByRecordId(q.m_recordId); const auto text = record ? IQ(record->m_displayName) : IQ(q.m_recordId);
            if (q.m_recordId == m_id || filter.isEmpty() || text.contains(filter, Qt::CaseInsensitive) || IQ(q.m_recordId).contains(filter, Qt::CaseInsensitive))
            { m_records->addItem(text + " [" + IQ(q.m_recordId).right(8) + "]", IQ(q.m_recordId)); }
        }
        if (!m_imported.m_definition.m_questId.empty() || !m_importError.empty()) { m_records->addItem(tr("Loaded document: %1").arg(IQ(m_imported.m_definition.m_display.m_fallbackName)), "@import"); }
        m_records->setCurrentIndex(qMax(0, m_records->findData(m_external ? "@import" : IQ(m_id))));
        const auto* pack = service.GetActivePack();
        m_summary->setText(tr("%1 saved quests | Active mod: %2").arg(static_cast<qulonglong>(catalog.GetQuestProfiles().size())).arg(pack ? IQ(pack->m_displayName) : tr("none selected")));
    }
    void QuestStateInspectorWidget::Load(const AZStd::string& id)
    {
        m_external = id == "@import"; m_id = m_external ? AZStd::string{} : id; m_revision.clear(); m_draft = {};
        if (m_external) { m_draft = m_imported; }
        else if (const auto* p = FoundationService::Get().GetCatalog().FindQuestProfile(id))
        { m_draft = QuestAuthoringService::Read(*p); m_revision = QuestAuthoringService::Revision(*p); }
        else { m_id.clear(); }
        m_context = Context(); m_dirty = false; Populate(); RefreshChoices(); Inspect();
    }
    void QuestStateInspectorWidget::Populate()
    {
        m_loading = true; const auto& d = m_draft.m_definition;
        m_name->setText(IQ(d.m_display.m_fallbackName)); m_description->setText(IQ(d.m_display.m_fallbackSummary)); m_version->setText(IQ(d.m_contentVersion));
        m_lifecycle->setCurrentIndex(qMax(0, m_lifecycle->findText(IQ(d.m_lifecycle)))); m_loading = false;
        m_save->setText(m_external ? tr("Save editable copy to mod") : tr("Save quest")); RefreshRows();
    }
    void QuestStateInspectorWidget::MarkDirty()
    {
        if (m_loading || m_draft.m_definition.m_questId.empty()) { return; }
        auto& d = m_draft.m_definition; d.m_display.m_fallbackName = IA(m_name->text()); d.m_display.m_fallbackSummary = IA(m_description->text());
        d.m_contentVersion = IA(m_version->text()); d.m_lifecycle = IA(m_lifecycle->currentText()); d.m_questFingerprint.clear();
        m_dirty = true; Inspect(); Status(tr("Unsaved local quest changes."));
    }
    void QuestStateInspectorWidget::Inspect()
    {
        m_issues->setRowCount(0); const auto result = QuestAuthoringService::Inspect(m_draft, FoundationService::Get().GetCatalog());
        const auto row = [this](const QString& level, const QString& text)
        { const int i = m_issues->rowCount(); m_issues->insertRow(i); m_issues->setItem(i, 0, new QTableWidgetItem(level)); m_issues->setItem(i, 1, new QTableWidgetItem(text)); };
        if (m_external && !m_importError.empty()) { row(tr("Error"), IQ(m_importError)); }
        if (!m_draft.m_definition.m_questId.empty())
        {
            for (const auto& e : result.m_errors) { row(tr("Error"), IQ(e)); }
            for (const auto& e : result.m_warnings) { row(tr("Draft note"), IQ(e)); }
        }
        const auto& d = m_draft.m_definition;
        m_details->setText(d.m_questId.empty() ? tr("No quest loaded.") : tr("Quest: %1\nOwner mod: %2 | Module: %3\n%4 phases | %5 objectives | %6 state keys\nDefinition fingerprint: %7")
            .arg(IQ(d.m_questId)).arg(IQ(d.m_ownerPackId)).arg(IQ(d.m_ownerModuleId))
            .arg(static_cast<qulonglong>(d.m_phases.size())).arg(static_cast<qulonglong>(d.m_objectives.size()))
            .arg(static_cast<qulonglong>(m_draft.m_stateKeys.size())).arg(IQ(CalculateQuestDefinitionFingerprintV1(d))));
        m_tabs->setTabText(3, tr("Validation (%1)").arg(m_issues->rowCount()));
        if (m_tabs->currentIndex() == 2) { DrawGraph(); }
        m_save->setEnabled(!m_draft.m_definition.m_questId.empty());
    }
    void QuestStateInspectorWidget::Browse()
    {
        if (m_dirty) { Status(tr("Save or revert the current draft before loading another document.")); return; }
        const auto path = QFileDialog::getOpenFileName(this, tr("Inspect quest document"), {}, tr("QuestDefinition (*.tgquest.json);;JSON (*.json)"));
        if (path.isEmpty()) { return; }
        FoundationService::Get().ReadQuestDocument(IA(path), m_imported, &m_importError); Load("@import");
        Status(m_importError.empty() ? tr("Loaded for inspection. Saving an editable copy creates a quest in your active mod.") : IQ(m_importError));
    }
    void QuestStateInspectorWidget::Create()
    {
        if (m_dirty) { Status(tr("Save or revert your draft before creating a quest.")); return; }
        const auto context = Context(); bool ok = false;
        const auto name = QInputDialog::getText(this, tr("New quest"), tr("Quest name"), QLineEdit::Normal, {}, &ok);
        if (!ok) { return; } if (context != Context()) { Status(tr("The workspace or mod changed; reopen New quest.")); return; }
        AZStd::string id, error; m_saving = true;
        const bool created = FoundationService::Get().CreateQuestDefinition(IA(name), id, &error); m_saving = false;
        if (!created) { Status(IQ(error)); return; } Load(id); Status(tr("Created a local quest with Start and Complete phases."));
    }
    void QuestStateInspectorWidget::Save()
    {
        if (m_draft.m_definition.m_questId.empty()) { return; }
        if (m_external && !m_importError.empty()) { Status(IQ(m_importError)); return; }
        if (!m_external && m_context != Context()) { Status(tr("Return to the original workspace and mod, or revert this draft.")); return; }
        AZStd::string id = m_id, error; m_saving = true;
        const bool saved = m_external ? FoundationService::Get().AdoptQuestDefinition(m_draft, id, &error)
            : FoundationService::Get().SaveQuestDefinition(m_draft, m_revision, &error);
        m_saving = false;
        if (!saved) { Inspect(); Status(IQ(error)); return; }
        Load(id); Status(tr("Quest saved. Reopening the workspace restores this definition."));
    }
    void QuestStateInspectorWidget::Revert()
    {
        if (m_dirty && QMessageBox::question(this, tr("Revert quest"), tr("Discard the unsaved quest changes?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { return; }
        Load(m_external ? AZStd::string("@import") : m_id); Status(tr("Reloaded the saved definition."));
    }
    void QuestStateInspectorWidget::OnFoundationChanged()
    {
        if (m_saving) { return; }
        if (m_dirty) { Inspect(); Status(tr("The catalog, workspace or mod changed. Your draft is preserved; save in its original context or revert.")); return; }
        Load(m_external ? AZStd::string("@import") : m_id);
    }
    void QuestStateInspectorWidget::closeEvent(QCloseEvent* event)
    {
        if (m_dirty && QMessageBox::warning(this, tr("Unsaved quest"), tr("Discard quest changes and close?"), QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Discard)
        { event->ignore(); return; } QWidget::closeEvent(event);
    }
    void QuestStateInspectorWidget::resizeEvent(QResizeEvent* event)
    { QWidget::resizeEvent(event); if (m_graph && m_scene && !m_scene->items().isEmpty()) { m_graph->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio); } }
}
