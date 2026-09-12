/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "SpawnEncounterEditorWidget.h"
#include "EncounterPlanningService.h"
#include "FoundationService.h"
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QUuid>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString Q(const AZStd::string& value) { return QString::fromUtf8(value.c_str()); }
        AZStd::string A(const QString& value)
        {
            const auto bytes = value.toUtf8();
            return {bytes.constData(), static_cast<size_t>(bytes.size())};
        }
        void Open(const char* pane)
        {
            AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::OpenViewPane, pane);
        }
        QTableWidgetItem* Cell(const QString& text)
        {
            auto* cell = new QTableWidgetItem(text);
            cell->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return cell;
        }
        void Searchable(QComboBox* combo)
        {
            combo->setEditable(true);
            combo->setInsertPolicy(QComboBox::NoInsert);
            combo->completer()->setFilterMode(Qt::MatchContains);
            combo->completer()->setCompletionMode(QCompleter::PopupCompletion);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            combo->setMinimumContentsLength(24);
        }
    }

    SpawnEncounterEditorWidget::SpawnEncounterEditorWidget(QWidget* parent) : QWidget(parent)
    {
        setObjectName("spawnEncounterEditor");
        auto* outer = new QVBoxLayout(this);
        auto* title = new QLabel(tr("Spawn and Encounter Editor"), this);
        outer->addWidget(title);
        auto* introduction = new QLabel(tr("Plan actors and troops, placement, activation and population limits. "
            "Save plans to your mod; spawning in the game requires a separate supported workflow."), this);
        introduction->setWordWrap(true); outer->addWidget(introduction);
        auto* actions = new QHBoxLayout();
        auto* create = new QPushButton(tr("New encounter"), this); create->setObjectName("encounterNew");
        auto* pack = new QPushButton(tr("Choose or create mod"), this);
        auto* actors = new QPushButton(tr("Actors and troops"), this);
        actions->addWidget(create); actions->addWidget(pack); actions->addWidget(actors); actions->addStretch();
        outer->addLayout(actions);
        connect(create, &QPushButton::clicked, this, [this]() { Create(); });
        connect(pack, &QPushButton::clicked, this, []() { Open("Tainted Grail Pack Manager"); });
        connect(actors, &QPushButton::clicked, this, []() { Open("Tainted Grail Actor and Troop Editor"); });
        m_summary = new QLabel(this); outer->addWidget(m_summary);
        m_records = new QComboBox(this); m_records->setObjectName("encounterRecords");
        outer->addWidget(m_records);
        connect(m_records, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]()
        {
            if (m_loading) { return; }
            if (m_dirty)
            {
                const QSignalBlocker blocker(m_records);
                m_records->setCurrentIndex(m_records->findData(Q(m_draft.m_recordId)));
                Status(tr("Save or revert your changes before selecting another encounter."), true);
                return;
            }
            Load(A(m_records->currentData().toString()));
        });
        auto* scroll = new QScrollArea(this); scroll->setWidgetResizable(true);
        m_form = new QWidget(scroll); auto* layout = new QVBoxLayout(m_form);
        auto* form = new QFormLayout();
        m_name = new QLineEdit(m_form); m_name->setObjectName("encounterName"); m_name->setMaxLength(512);
        form->addRow(tr("Name"), m_name);
        m_placement = new QComboBox(m_form); m_placement->setObjectName("encounterPlacement");
        form->addRow(tr("World placement"), m_placement);
        m_placementSubject = new QLineEdit(m_form); m_placementSubject->setObjectName("encounterPlacementSubject");
        m_placementSubject->setMaxLength(1024); m_placementSubject->setPlaceholderText(tr("Optional location reference for a plan without a saved world record"));
        form->addRow(tr("Placement reference"), m_placementSubject);
        m_activation = new QComboBox(m_form); m_activation->setObjectName("encounterActivation");
        m_activation->addItem(tr("Manual"), "manual"); m_activation->addItem(tr("All listed conditions"), "all_conditions");
        form->addRow(tr("Activation"), m_activation);
        m_conditions = new QPlainTextEdit(m_form); m_conditions->setObjectName("encounterConditions");
        m_conditions->setPlaceholderText(tr("One condition per line, for example: Player enters the north gate"));
        m_conditions->setMaximumHeight(90); form->addRow(tr("Conditions"), m_conditions);
        m_instances = new QSpinBox(m_form); m_instances->setObjectName("encounterInstances"); m_instances->setRange(1, 1000);
        form->addRow(tr("Maximum active instances"), m_instances);
        m_populationLimit = new QSpinBox(m_form); m_populationLimit->setObjectName("encounterPopulationLimit");
        m_populationLimit->setRange(1, 1000000); form->addRow(tr("Population limit"), m_populationLimit);
        m_unique = new QCheckBox(tr("Only one active instance of this encounter"), m_form); m_unique->setObjectName("encounterUnique");
        form->addRow(tr("Unique encounter"), m_unique);
        m_cleanup = new QLineEdit(m_form); m_cleanup->setObjectName("encounterCleanup"); m_cleanup->setMaxLength(1024);
        m_rollback = new QLineEdit(m_form); m_rollback->setObjectName("encounterRollback"); m_rollback->setMaxLength(1024);
        form->addRow(tr("Cleanup notes"), m_cleanup); form->addRow(tr("Rollback notes"), m_rollback);
        layout->addLayout(form);
        auto* compositionTitle = new QLabel(tr("Composition"), m_form); layout->addWidget(compositionTitle);
        m_filter = new QLineEdit(m_form); m_filter->setObjectName("encounterTargetFilter");
        m_filter->setPlaceholderText(tr("Filter saved actors and troops by name or ID")); layout->addWidget(m_filter);
        m_target = new QComboBox(m_form); m_target->setObjectName("encounterTarget"); Searchable(m_target);
        auto* entryActions = new QHBoxLayout(); entryActions->addWidget(m_target, 1);
        auto* add = new QPushButton(tr("Add to encounter"), m_form); add->setObjectName("encounterAddEntry");
        auto* remove = new QPushButton(tr("Remove selected"), m_form); remove->setObjectName("encounterRemoveEntry");
        entryActions->addWidget(add); entryActions->addWidget(remove); layout->addLayout(entryActions);
        m_entries = new QTableWidget(0, 5, m_form); m_entries->setObjectName("encounterEntries");
        m_entries->setHorizontalHeaderLabels({tr("Actor or troop"), tr("Kind"), tr("Minimum"), tr("Maximum"), tr("Actors per instance")});
        m_entries->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_entries->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        m_entries->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_entries->setSelectionMode(QAbstractItemView::SingleSelection); m_entries->setMinimumHeight(200);
        layout->addWidget(m_entries);
        m_preview = new QLabel(m_form); m_preview->setObjectName("encounterPreview");
        m_preview->setTextFormat(Qt::PlainText); m_preview->setWordWrap(true);
        m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(m_preview); layout->addStretch();
        scroll->setWidget(m_form); outer->addWidget(scroll, 1);
        auto* saveActions = new QHBoxLayout();
        m_save = new QPushButton(tr("Save encounter"), this); m_save->setObjectName("encounterSave");
        auto* revert = new QPushButton(tr("Revert changes"), this); revert->setObjectName("encounterRevert");
        saveActions->addWidget(m_save); saveActions->addWidget(revert); saveActions->addStretch(); outer->addLayout(saveActions);
        m_status = new QLabel(this); m_status->setObjectName("encounterStatus"); m_status->setWordWrap(true); outer->addWidget(m_status);
        connect(add, &QPushButton::clicked, this, [this]() { AddEntry(); });
        connect(remove, &QPushButton::clicked, this, [this]() { RemoveEntry(); });
        connect(m_save, &QPushButton::clicked, this, [this]() { Save(); });
        connect(revert, &QPushButton::clicked, this, [this]()
        {
            const auto id = m_draft.m_recordId; m_dirty = false; RefreshChoices(); Load(id); Status(tr("Reloaded the saved encounter."));
        });
        for (auto* line : {m_name, m_placementSubject, m_cleanup, m_rollback})
        {
            connect(line, &QLineEdit::textEdited, this, [this]() { MarkDirty(); });
        }
        connect(m_conditions, &QPlainTextEdit::textChanged, this, [this]() { MarkDirty(); });
        for (auto* spin : {m_instances, m_populationLimit})
        {
            connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() { MarkDirty(); });
        }
        connect(m_unique, &QCheckBox::toggled, this, [this]() { MarkDirty(); });
        connect(m_activation, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]()
        {
            m_conditions->setEnabled(m_activation->currentData() == "all_conditions"); MarkDirty();
        });
        connect(m_placement, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]()
        {
            if (m_loading) { return; }
            const auto* record = FoundationService::Get().GetCatalog().FindByRecordId(A(m_placement->currentData().toString()));
            m_placementSubject->setEnabled(!record);
            if (record) { m_placementSubject->setText(Q(record->m_subjectRef)); }
            MarkDirty();
        });
        connect(m_filter, &QLineEdit::textChanged, this, [this]() { RefreshChoices(); });
        if (FoundationService::Get().GetWorkspaceFilePath().empty()) { FoundationService::Get().RefreshLocalSetup(); }
        FoundationNotificationBus::Handler::BusConnect(); RefreshChoices();
        Load(A(m_records->currentData().toString()));
    }

    SpawnEncounterEditorWidget::~SpawnEncounterEditorWidget() { FoundationNotificationBus::Handler::BusDisconnect(); }
    void SpawnEncounterEditorWidget::closeEvent(QCloseEvent* event)
    {
        if (m_unsavedPromptOpen) { event->ignore(); return; }
        if (m_dirty)
        {
            const QScopedValueRollback<bool> prompting(m_unsavedPromptOpen, true);
            QMessageBox prompt(QMessageBox::Warning, tr("Unsaved encounter"),
                tr("Save your encounter changes before closing the pane?"),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
            prompt.setObjectName("encounterUnsavedChangesDialog");
            prompt.setDefaultButton(QMessageBox::Cancel);
            prompt.setEscapeButton(QMessageBox::Cancel);
            const int choice = prompt.exec();
            if (choice != QMessageBox::Discard && (choice != QMessageBox::Save || !Save()))
            {
                event->ignore(); return;
            }
        }
        QWidget::closeEvent(event);
    }
    void SpawnEncounterEditorWidget::Status(const QString& text, bool error)
    {
        m_status->setText(text); m_status->setProperty("error", error);
    }
    bool SpawnEncounterEditorWidget::SameWorkspace() const
    {
        const auto& workspace = FoundationService::Get().GetWorkspace();
        return workspace.m_workspaceId == m_workspaceId && workspace.m_activeGameProfileId == m_profileId;
    }
    void SpawnEncounterEditorWidget::OnFoundationChanged()
    {
        if (m_saving) { return; }
        if (m_dirty)
        {
            Preview();
            Status(tr("The workspace or catalog changed. Your draft is preserved; save it in its original mod or revert to reload."), true);
            return;
        }
        const auto id = m_draft.m_recordId; RefreshChoices(); Load(id);
    }
    void SpawnEncounterEditorWidget::RefreshChoices()
    {
        const QSignalBlocker recordsBlock(m_records), targetBlock(m_target), placementBlock(m_placement);
        const auto selectedRecord = m_records->currentData(), selectedTarget = m_target->currentData(), selectedPlacement = m_placement->currentData();
        m_records->clear(); m_records->addItem(tr("Choose an encounter"), "");
        m_target->clear(); m_placement->clear(); m_placement->addItem(tr("Unassigned or reference only"), "");
        const auto& foundation = FoundationService::Get(); const auto& catalog = foundation.GetCatalog();
        const auto filter = m_filter->text().trimmed();
        for (const auto& record : catalog.GetRecords())
        {
            if (record.m_domain == "population" && record.m_recordKind == "encounter" && catalog.FindEncounterDefinition(record.m_recordId))
            {
                m_records->addItem(Q(record.m_displayName), Q(record.m_recordId));
            }
            if (record.m_domain == "population" && (record.m_recordKind == "actor" || record.m_recordKind == "troop")
                && (catalog.FindPopulationActorProfile(record.m_recordId) || catalog.FindPopulationTroopProfile(record.m_recordId))
                && (filter.isEmpty() || Q(record.m_displayName).contains(filter, Qt::CaseInsensitive) || Q(record.m_recordId).contains(filter, Qt::CaseInsensitive)))
            {
                m_target->addItem(Q(record.m_displayName) + " [" + Q(record.m_recordKind) + "]", Q(record.m_recordId));
            }
            if (record.m_domain == "world" && (record.m_recordKind == "location" || record.m_recordKind == "scene" || record.m_recordKind == "region"))
            {
                m_placement->addItem(Q(record.m_displayName), Q(record.m_recordId));
            }
        }
        m_records->setCurrentIndex(qMax(0, m_records->findData(selectedRecord)));
        m_target->setCurrentIndex(qMax(0, m_target->findData(selectedTarget)));
        m_placement->setCurrentIndex(qMax(0, m_placement->findData(selectedPlacement)));
        const auto* pack = foundation.GetActivePack();
        m_summary->setText(tr("%1 encounters | %2 actors | %3 troops | Active mod: %4")
            .arg(catalog.GetEncounterDefinitions().size()).arg(catalog.GetPopulationActorProfiles().size())
            .arg(catalog.GetPopulationTroopProfiles().size()).arg(pack ? Q(pack->m_displayName) : tr("none selected")));
    }
    void SpawnEncounterEditorWidget::Load(const AZStd::string& recordId)
    {
        m_loading = true;
        const auto& foundation = FoundationService::Get();
        const auto* definition = foundation.GetCatalog().FindEncounterDefinition(recordId);
        const auto* record = foundation.GetCatalog().FindByRecordId(recordId);
        m_draft = definition ? *definition : EncounterDefinition{};
        m_workspaceId = foundation.GetWorkspace().m_workspaceId;
        m_profileId = foundation.GetWorkspace().m_activeGameProfileId;
        m_records->setCurrentIndex(qMax(0, m_records->findData(Q(m_draft.m_recordId))));
        m_name->setText(record ? Q(record->m_displayName) : QString{});
        m_placement->setCurrentIndex(qMax(0, m_placement->findData(Q(m_draft.m_placementRecordId))));
        m_placementSubject->setText(Q(m_draft.m_placementSubjectRef));
        m_placementSubject->setEnabled(m_draft.m_placementRecordId.empty());
        m_activation->setCurrentIndex(m_activation->findData(Q(m_draft.m_activationMode)));
        QStringList conditions; for (const auto& condition : m_draft.m_conditions) { conditions.append(Q(condition)); }
        m_conditions->setPlainText(conditions.join('\n')); m_conditions->setEnabled(m_draft.m_activationMode == "all_conditions");
        m_instances->setValue(m_draft.m_maximumActiveInstances); m_populationLimit->setValue(m_draft.m_populationLimit);
        m_unique->setChecked(m_draft.m_uniqueEncounter); m_cleanup->setText(Q(m_draft.m_cleanupNotes)); m_rollback->setText(Q(m_draft.m_rollbackNotes));
        m_form->setEnabled(definition != nullptr); m_dirty = false; RefreshEntries(); m_loading = false; Preview();
    }
    void SpawnEncounterEditorWidget::ReadFields()
    {
        m_draft.m_placementRecordId = A(m_placement->currentData().toString());
        m_draft.m_placementSubjectRef = A(m_placementSubject->text().trimmed());
        m_draft.m_activationMode = A(m_activation->currentData().toString());
        m_draft.m_conditions.clear();
        if (m_draft.m_activationMode == "all_conditions")
        {
            if (m_conditions->document()->characterCount() > 16449)
            {
                m_draft.m_conditions.push_back(AZStd::string(257, 'x')); // Core rejects the oversized draft; UI text remains intact.
            }
            else
            {
                for (const auto& line : m_conditions->toPlainText().split('\n', Qt::SkipEmptyParts))
                {
                    m_draft.m_conditions.push_back(A(line.trimmed()));
                    if (m_draft.m_conditions.size() > 64) { break; }
                }
            }
        }
        m_draft.m_maximumActiveInstances = m_instances->value(); m_draft.m_populationLimit = m_populationLimit->value();
        m_draft.m_uniqueEncounter = m_unique->isChecked(); m_draft.m_cleanupNotes = A(m_cleanup->text().trimmed());
        m_draft.m_rollbackNotes = A(m_rollback->text().trimmed());
    }
    void SpawnEncounterEditorWidget::MarkDirty()
    {
        if (m_loading || m_draft.m_recordId.empty()) { return; }
        ReadFields(); m_dirty = true; Preview(); Status(tr("Unsaved encounter changes."));
    }
    void SpawnEncounterEditorWidget::RefreshEntries()
    {
        m_entries->setRowCount(0);
        const auto& catalog = FoundationService::Get().GetCatalog();
        for (size_t index = 0; index < m_draft.m_entries.size(); ++index)
        {
            const auto& entry = m_draft.m_entries[index]; const auto* record = catalog.FindByRecordId(entry.m_targetRecordId);
            const int row = m_entries->rowCount(); m_entries->insertRow(row);
            auto* name = Cell(record ? Q(record->m_displayName) : tr("Missing: %1").arg(Q(entry.m_targetRecordId)));
            name->setData(Qt::UserRole, Q(entry.m_entryId)); name->setToolTip(Q(entry.m_targetRecordId)); m_entries->setItem(row, 0, name);
            m_entries->setItem(row, 1, Cell(record ? Q(record->m_recordKind) : tr("Missing")));
            m_entries->setItem(row, 4, Cell(""));
            for (int column : {2, 3})
            {
                auto* spin = new QSpinBox(m_entries); spin->setRange(1, 1000);
                spin->setObjectName(QString("encounterQuantity_%1_%2").arg(row).arg(column));
                spin->setValue(column == 2 ? entry.m_minimumCount : entry.m_maximumCount);
                m_entries->setCellWidget(row, column, spin);
                const auto id = entry.m_entryId;
                connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this, id, column](int value)
                {
                    for (auto& draft : m_draft.m_entries)
                    {
                        if (draft.m_entryId == id)
                        {
                            if (column == 2) { draft.m_minimumCount = value; } else { draft.m_maximumCount = value; }
                            break;
                        }
                    }
                    MarkDirty();
                });
            }
        }
    }
    void SpawnEncounterEditorWidget::Preview()
    {
        if (m_draft.m_recordId.empty()) { m_preview->setText(tr("Create an encounter to start planning.")); m_save->setEnabled(false); return; }
        const auto result = EncounterPlanningService().Preview(m_draft, FoundationService::Get().GetCatalog());
        QStringList lines;
        lines << tr("Composition preview: %1-%2 actors per instance; up to %3 across %4 active instance(s).")
            .arg(result.m_minimumActors).arg(result.m_maximumActors).arg(result.m_maximumConcurrentActors).arg(m_draft.m_maximumActiveInstances);
        for (const auto& error : result.m_errors) { lines << tr("Fix: %1").arg(Q(error)); }
        size_t warningCount = 0;
        for (const auto& warning : result.m_warnings)
        {
            if (++warningCount > 12) { lines << tr("%1 more review notes.").arg(result.m_warnings.size() - 12); break; }
            lines << Q(warning);
        }
        for (int row = 0; row < m_entries->rowCount(); ++row)
        {
            m_entries->item(row, 4)->setText(tr("Unresolved"));
            for (const auto& preview : result.m_rows)
            {
                if (Q(preview.m_entryId) == m_entries->item(row, 0)->data(Qt::UserRole).toString())
                {
                    m_entries->item(row, 4)->setText(QString("%1-%2").arg(preview.m_minimumActors).arg(preview.m_maximumActors)); break;
                }
            }
        }
        m_preview->setText(lines.join('\n'));
        // Save remains available for invalid drafts so the command can give a concrete error without losing edits.
        m_save->setEnabled(m_dirty && SameWorkspace());
    }
    void SpawnEncounterEditorWidget::AddEntry()
    {
        if (m_draft.m_recordId.empty()) { return; }
        if (m_target->currentText() != m_target->itemText(m_target->currentIndex()))
        {
            Status(tr("Select a matching saved actor or troop from the list."), true); return;
        }
        const auto target = A(m_target->currentData().toString());
        if (target.empty()) { Status(tr("Choose a saved actor or troop first."), true); return; }
        for (const auto& entry : m_draft.m_entries)
        {
            if (entry.m_targetRecordId == target) { Status(tr("This actor or troop is already included. Change its quantity in the table."), true); return; }
        }
        if (m_draft.m_entries.size() >= 128) { Status(tr("An encounter supports at most 128 distinct composition rows."), true); return; }
        EncounterEntry entry;
        entry.m_entryId = A("custom.encounter-entry." + QUuid::createUuid().toString(QUuid::WithoutBraces)); entry.m_targetRecordId = target;
        m_draft.m_entries.push_back(entry); RefreshEntries(); MarkDirty();
    }
    void SpawnEncounterEditorWidget::RemoveEntry()
    {
        const int row = m_entries->currentRow();
        if (row < 0) { Status(tr("Select a composition row to remove."), true); return; }
        const auto id = A(m_entries->item(row, 0)->data(Qt::UserRole).toString());
        for (auto it = m_draft.m_entries.begin(); it != m_draft.m_entries.end(); ++it)
        {
            if (it->m_entryId == id) { m_draft.m_entries.erase(it); break; }
        }
        RefreshEntries(); MarkDirty();
    }
    void SpawnEncounterEditorWidget::Create()
    {
        if (m_dirty) { Status(tr("Save or revert your changes before creating another encounter."), true); return; }
        RefreshChoices();
        if (!m_target->count()) { Status(tr("Load or create an actor in Actors and troops, then create an encounter."), true); return; }
        bool accepted = false;
        const auto name = QInputDialog::getText(this, tr("New encounter"), tr("Encounter name"), QLineEdit::Normal, "", &accepted);
        if (!accepted) { return; }
        QStringList choices; for (int index = 0; index < m_target->count(); ++index) { choices.append(QString("%1. %2").arg(index + 1).arg(m_target->itemText(index))); }
        const auto target = QInputDialog::getItem(this, tr("Initial composition"), tr("First actor or troop"), choices, 0, false, &accepted);
        if (!accepted) { return; }
        const int index = choices.indexOf(target); if (index < 0) { return; }
        AZStd::string id, error; m_saving = true;
        const bool success = FoundationService::Get().CreateEncounterDefinition(A(name), A(m_target->itemData(index).toString()), id, &error);
        m_saving = false;
        if (!success) { Status(Q(error), true); return; }
        RefreshChoices(); Load(id); Status(tr("Encounter created and saved. Add the remaining actors or troops below."));
    }
    bool SpawnEncounterEditorWidget::Save()
    {
        if (!SameWorkspace()) { Status(tr("Return to the original workspace and profile before saving this draft."), true); return false; }
        ReadFields(); AZStd::string error; m_saving = true;
        const bool success = FoundationService::Get().SaveEncounterDefinition(m_draft, A(m_name->text()), &error);
        m_saving = false;
        if (!success) { Status(Q(error), true); return false; }
        const auto id = m_draft.m_recordId; m_dirty = false; RefreshChoices(); Load(id); Status(tr("Encounter saved."));
        return true;
    }
} // namespace TaintedGrailModdingSDK
