/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "QuestStateInspectorWidget.h"
#include "QuestAuthoringService.h"
#include "FoundationService.h"
#include <AzCore/std/algorithm.h>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QUuid>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString EQ(const AZStd::string& v) { return QString::fromUtf8(v.c_str()); }
        AZStd::string EA(const QString& v) { const auto b = v.toUtf8(); return {b.constData(), static_cast<size_t>(b.size())}; }
        AZStd::string ElementId(const char* prefix) { return EA(QString::fromUtf8(prefix) + "." + QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-')); }
        using Choices = AZStd::vector<AZStd::string>;
        class QuestRowDialog : public QDialog
        {
        public:
            QMap<QString, QLineEdit*> texts; QMap<QString, QComboBox*> choices;
            QMap<QString, QCheckBox*> flags; QMap<QString, QListWidget*> lists;
            QFormLayout* form;
            explicit QuestRowDialog(QWidget* parent) : QDialog(parent)
            {
                setObjectName("questRowDialog"); setWindowTitle(tr("Quest element")); resize(640, 650);
                auto* layout = new QVBoxLayout(this); auto* scroll = new QScrollArea(this); scroll->setWidgetResizable(true);
                auto* page = new QWidget(scroll); form = new QFormLayout(page); form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
                scroll->setWidget(page); layout->addWidget(scroll);
                auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this); buttons->setObjectName("questRowButtons"); layout->addWidget(buttons);
                connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
            }
            void Text(const char* key, const QString& title, const AZStd::string& value, int max = 512)
            { auto* v = new QLineEdit(EQ(value), this); v->setMaxLength(max); v->setObjectName(QString("questRow_") + key); texts[key] = v; form->addRow(title, v); }
            void Flag(const char* key, const QString& title, bool checked)
            { auto* v = new QCheckBox(title, this); v->setChecked(checked); v->setObjectName(QString("questRow_") + key); flags[key] = v; form->addRow(v); }
            void Choice(const char* key, const QString& title, const Choices& ids, const AZStd::string& current,
                const QuestAuthoringDraft* draft = nullptr, bool editable = false)
            {
                auto* v = new QComboBox(this); v->setObjectName(QString("questRow_") + key); v->setMinimumContentsLength(16);
                v->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); v->setMaxVisibleItems(15); v->setEditable(editable);
                for (const auto& id : ids) { v->addItem(draft ? EQ(QuestAuthoringService::Label(*draft, id)) + " [" + EQ(id).right(8) + "]" : EQ(id), EQ(id)); }
                if (!current.empty() && v->findData(EQ(current)) < 0) { v->addItem(EQ(current) + tr(" (unresolved)"), EQ(current)); }
                v->setCurrentIndex(v->findData(EQ(current))); choices[key] = v; form->addRow(title, v);
            }
            void Many(const char* key, const QString& title, const Choices& ids, const Choices& selected, const QuestAuthoringDraft& draft)
            {
                auto* v = new QListWidget(this); v->setObjectName(QString("questRow_") + key); v->setMinimumHeight(100); v->setMaximumHeight(150);
                auto all = ids; for (const auto& id : selected) { if (AZStd::find(all.begin(), all.end(), id) == all.end()) { all.push_back(id); } }
                for (const auto& id : all)
                {
                    auto* item = new QListWidgetItem(EQ(QuestAuthoringService::Label(draft, id)), v); item->setData(Qt::UserRole, EQ(id));
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setCheckState(AZStd::find(selected.begin(), selected.end(), id) == selected.end() ? Qt::Unchecked : Qt::Checked);
                }
                lists[key] = v; form->addRow(title, v);
            }
            AZStd::string Get(const char* key) const
            {
                if (texts.contains(key)) { return EA(texts[key]->text()); }
                auto* v = choices.value(key); if (!v) { return {}; }
                return EA(v->currentIndex() >= 0 && v->currentText() == v->itemText(v->currentIndex()) ? v->currentData().toString() : v->currentText());
            }
            bool Checked(const char* key) const { return flags.value(key)->isChecked(); }
            Choices Selected(const char* key) const
            { Choices result; auto* v = lists.value(key); for (int i = 0; i < v->count(); ++i) { if (v->item(i)->checkState() == Qt::Checked) { result.push_back(EA(v->item(i)->data(Qt::UserRole).toString())); } } return result; }
        };
    }
    AZStd::string QuestStateInspectorWidget::RowId(int row) const
    { return row < 0 || !m_rows->item(row, 0) ? AZStd::string{} : EA(m_rows->item(row, 0)->data(Qt::UserRole).toString()); }
    void QuestStateInspectorWidget::RefreshRows()
    {
        const auto selected = RowId(m_rows->currentRow()); m_rows->setRowCount(0); m_rows->setColumnCount(4);
        m_rows->setHorizontalHeaderLabels({tr("Name / key"),tr("Type / phase"),tr("References / default"),tr("Identity")});
        const auto& d = m_draft.m_definition; const auto& catalog = FoundationService::Get().GetCatalog();
        const auto label = [this](const AZStd::string& id) { return EQ(QuestAuthoringService::Label(m_draft, id)); };
        const auto add = [&](const AZStd::string& id, const QString& name, const QString& type, const QString& refs)
        {
            if (!(name + " " + type + " " + refs + " " + EQ(id)).contains(m_rowSearch->text(), Qt::CaseInsensitive)) { return; }
            const int r = m_rows->rowCount(); m_rows->insertRow(r); int c = 0;
            for (const auto& text : {name, type, refs, EQ(id)})
            { auto* v = new QTableWidgetItem(text); v->setData(Qt::UserRole, EQ(id)); v->setToolTip(text); m_rows->setItem(r, c++, v); }
            if (selected == id) { m_rows->selectRow(r); }
        };
        switch (m_kind->currentIndex())
        {
        case Phase: for (const auto& v : d.m_phases) { add(v.m_phaseId, label(v.m_phaseId), v.m_entryPhase ? tr("Entry") : v.m_terminalPhase ? tr("Terminal") : tr("Phase"), tr("%1 objectives").arg(static_cast<qulonglong>(v.m_objectiveIds.size()))); } break;
        case Objective: for (const auto& v : d.m_objectives) { add(v.m_objectiveId, label(v.m_objectiveId), label(v.m_phaseId), tr("%1 conditions").arg(static_cast<qulonglong>(v.m_conditionIds.size()))); } break;
        case Transition: for (const auto& v : d.m_transitions) { add(v.m_transitionId, label(v.m_transitionId), label(v.m_fromPhaseId) + " -> " + label(v.m_toPhaseId), EQ(v.m_triggerId) + (v.m_repeatAllowed ? tr(" | repeat allowed") : "")); } break;
        case Condition: for (const auto& v : d.m_conditions) { add(v.m_conditionId, label(v.m_conditionId), EQ(v.m_conditionTypeId), label(v.m_subjectId)); } break;
        case Action: for (const auto& v : d.m_actions) { add(v.m_actionId, label(v.m_actionId), EQ(v.m_actionTypeId), label(v.m_subjectId)); } break;
        case Outcome: for (const auto& v : d.m_outcomes) { add(v.m_outcomeId, label(v.m_outcomeId), label(v.m_phaseId), EQ(v.m_textKey)); } break;
        case Role: for (const auto& v : d.m_roles) { add(v.m_roleId, label(v.m_roleId), v.m_required ? tr("Required") : tr("Optional"), EQ(v.m_displayTextKey)); } break;
        case Requirement: for (const auto& v : d.m_bindingRequirements) { add(v.m_requirementId, label(v.m_requirementId), EQ(v.m_subjectKind), label(v.m_roleId) + " | " + EQ(v.m_usage)); } break;
        case State: for (const auto& v : m_draft.m_stateKeys) { add(v.m_keyId, EQ(v.m_keyId), EQ(v.m_type), EQ(v.m_defaultValue) + " | " + EQ(v.m_description)); } break;
        case Binding: for (const auto& v : m_draft.m_bindings)
            { const auto* r = catalog.FindByRecordId(v.m_recordId); add(v.m_subjectId, label(v.m_subjectId), r ? EQ(r->m_recordKind) : tr("Missing"), r ? EQ(r->m_displayName) : EQ(v.m_recordId)); } break;
        }
    }
    void QuestStateInspectorWidget::EditRow(bool creating)
    {
        if (m_draft.m_definition.m_questId.empty()) { Status(tr("Create or load a quest first.")); return; }
        const int kind = m_kind->currentIndex(); auto id = RowId(m_rows->currentRow());
        if (!creating && id.empty()) { Status(tr("Select an element to edit.")); return; }
        const char* prefixes[] = {"phase","objective","transition","condition","action","outcome","role","binding","state","subject"};
        if (creating) { id = ElementId(prefixes[kind]); }
        const auto context = Context(); const auto revision = m_revision; const auto questId = m_draft.m_definition.m_questId;
        QuestAuthoringDraft next = m_draft; auto& d = next.m_definition;
        Choices phases, objectives, conditions, actions, roles, subjects, catalogIds;
        for (const auto& v : d.m_phases) { phases.push_back(v.m_phaseId); }
        for (const auto& v : d.m_objectives) { objectives.push_back(v.m_objectiveId); }
        for (const auto& v : d.m_conditions) { conditions.push_back(v.m_conditionId); subjects.push_back(v.m_subjectId); }
        for (const auto& v : d.m_actions) { actions.push_back(v.m_actionId); subjects.push_back(v.m_subjectId); }
        for (const auto& v : d.m_roles) { roles.push_back(v.m_roleId); subjects.push_back(v.m_roleId); }
        for (const auto& v : next.m_stateKeys) { subjects.push_back(v.m_keyId); }
        for (const auto& v : phases) { subjects.push_back(v); } for (const auto& v : objectives) { subjects.push_back(v); }
        subjects.push_back(d.m_questId);
        const auto& catalog = FoundationService::Get().GetCatalog();
        for (const auto& r : catalog.GetRecords()) { if (r.m_recordKind == "actor" || r.m_recordKind == "item" || r.m_recordKind == "location" || r.m_recordKind == "quest") { catalogIds.push_back(r.m_recordId); } }
        QuestRowDialog dialog(this);
        if (kind < State) { dialog.Text("label", tr("Display name"), creating ? AZStd::string{} : QuestAuthoringService::Label(next, id)); }
        const auto find = [&id](const auto& values, auto get)
        { for (const auto& v : values) { if (get(v) == id) { return v; } } return typename AZStd::decay_t<decltype(values)>::value_type{}; };
        auto phase = find(d.m_phases, [](const auto& v) { return v.m_phaseId; });
        auto objective = find(d.m_objectives, [](const auto& v) { return v.m_objectiveId; });
        auto transition = find(d.m_transitions, [](const auto& v) { return v.m_transitionId; });
        auto condition = find(d.m_conditions, [](const auto& v) { return v.m_conditionId; });
        auto action = find(d.m_actions, [](const auto& v) { return v.m_actionId; });
        auto outcome = find(d.m_outcomes, [](const auto& v) { return v.m_outcomeId; });
        auto role = find(d.m_roles, [](const auto& v) { return v.m_roleId; });
        auto requirement = find(d.m_bindingRequirements, [](const auto& v) { return v.m_requirementId; });
        auto state = find(next.m_stateKeys, [](const auto& v) { return v.m_keyId; });
        auto binding = find(next.m_bindings, [](const auto& v) { return v.m_subjectId; });
        switch (kind)
        {
        case Phase:
            dialog.Flag("entry", tr("Entry phase"), phase.m_entryPhase); dialog.Flag("terminal", tr("Terminal phase"), phase.m_terminalPhase);
            dialog.Many("actions", tr("Entry actions"), actions, phase.m_entryActionIds, next); break;
        case Objective:
            dialog.Choice("phase", tr("Phase"), phases, objective.m_phaseId, &next);
            dialog.Many("conditions", tr("Required conditions"), conditions, objective.m_conditionIds, next);
            dialog.Many("actions", tr("Completion actions"), actions, objective.m_completionActionIds, next); break;
        case Transition:
            dialog.Choice("from", tr("From phase"), phases, transition.m_fromPhaseId, &next);
            dialog.Choice("to", tr("To phase"), phases, transition.m_toPhaseId, &next);
            dialog.Text("trigger", tr("Trigger key"), creating ? AZStd::string("trigger.completed") : transition.m_triggerId, 128);
            dialog.Text("priority", tr("Priority (unsigned integer)"), AZStd::string::format("%u", transition.m_priority), 10);
            dialog.Flag("repeat", tr("Allow repeat / loop"), transition.m_repeatAllowed);
            dialog.Many("conditions", tr("Conditions"), conditions, transition.m_conditionIds, next);
            dialog.Many("actions", tr("Actions"), actions, transition.m_actionIds, next); break;
        case Condition:
            dialog.Choice("type", tr("Condition type"), {"adapter.capability","counter.compare","decision.equals","fact.equals","location.presence","objective.status","phase.reached","quest.status","role.available"}, condition.m_conditionTypeId);
            dialog.Choice("subject", tr("Subject or state key"), subjects, condition.m_subjectId, nullptr, true); break;
        case Action:
            dialog.Choice("type", tr("Action declaration"), {"counter.increment","decision.set","fact.set","journal.update","marker.set","objective.activate","objective.complete","quest.archive","quest.resolve"}, action.m_actionTypeId);
            dialog.Choice("subject", tr("Subject or state key"), subjects, action.m_subjectId, nullptr, true); break;
        case Outcome: dialog.Choice("phase", tr("Terminal phase"), phases, outcome.m_phaseId, &next); break;
        case Role: dialog.Flag("required", tr("Required role"), creating || role.m_required); break;
        case Requirement:
            dialog.Choice("role", tr("Role"), roles, requirement.m_roleId, &next);
            dialog.Text("subjectKind", tr("Subject kind key"), creating ? AZStd::string("subject.actor") : requirement.m_subjectKind, 128);
            dialog.Text("usage", tr("Usage key"), creating ? AZStd::string("usage.quest-role") : requirement.m_usage, 128); break;
        case State:
            dialog.Text("key", tr("Stable state key"), creating ? id : state.m_keyId, 128); dialog.texts["key"]->setReadOnly(!creating);
            dialog.Choice("type", tr("Value type"), {"boolean","integer","text"}, creating ? AZStd::string("boolean") : state.m_type);
            dialog.Text("default", tr("Authored default"), creating ? AZStd::string("false") : state.m_defaultValue, 2048);
            dialog.Text("description", tr("Description"), state.m_description, 2048); break;
        case Binding:
            dialog.Choice("subject", tr("Logical subject"), subjects, binding.m_subjectId, nullptr, true);
            dialog.Choice("record", tr("Catalog target"), {}, binding.m_recordId);
            for (const auto& key : catalogIds)
            {
                auto* choice = dialog.choices["record"]; if (choice->findData(EQ(key)) >= 0) { continue; }
                const auto* r = catalog.FindByRecordId(key); choice->addItem(EQ(r->m_displayName) + " [" + EQ(r->m_recordKind) + " / " + EQ(key).right(8) + "]", EQ(key));
            }
            dialog.choices["record"]->setCurrentIndex(dialog.choices["record"]->findData(EQ(binding.m_recordId))); break;
        }
        if (dialog.exec() != QDialog::Accepted) { return; }
        if (context != Context() || revision != m_revision || questId != m_draft.m_definition.m_questId)
        { Status(tr("The quest or workspace changed while editing. Reopen the element form.")); return; }
        const auto put = [creating, &id](auto& values, auto value, auto get)
        {
            if (!creating) { for (auto& old : values) { if (get(old) == id) { old = AZStd::move(value); return; } } }
            values.push_back(AZStd::move(value));
        };
        switch (kind)
        {
        case Phase:
            phase.m_phaseId = id; if (creating) { phase.m_displayTextKey = "loc." + id; }
            phase.m_entryPhase = dialog.Checked("entry"); phase.m_terminalPhase = dialog.Checked("terminal"); phase.m_entryActionIds = dialog.Selected("actions");
            put(d.m_phases, phase, [](const auto& v) { return v.m_phaseId; }); break;
        case Objective:
            objective.m_objectiveId = id; if (creating) { objective.m_displayTextKey = "loc." + id; }
            objective.m_phaseId = dialog.Get("phase"); objective.m_conditionIds = dialog.Selected("conditions"); objective.m_completionActionIds = dialog.Selected("actions");
            put(d.m_objectives, objective, [](const auto& v) { return v.m_objectiveId; });
            for (auto& p : d.m_phases)
            { p.m_objectiveIds.erase(AZStd::remove(p.m_objectiveIds.begin(), p.m_objectiveIds.end(), id), p.m_objectiveIds.end()); if (p.m_phaseId == objective.m_phaseId) { p.m_objectiveIds.push_back(id); } } break;
        case Transition:
        {
            bool valid = false; const auto priority = EQ(dialog.Get("priority")).toUInt(&valid);
            if (!valid) { Status(tr("Transition priority must be an unsigned integer.")); return; }
            transition.m_transitionId = id; transition.m_fromPhaseId = dialog.Get("from"); transition.m_toPhaseId = dialog.Get("to");
            transition.m_triggerId = dialog.Get("trigger"); transition.m_priority = priority; transition.m_repeatAllowed = dialog.Checked("repeat");
            transition.m_conditionIds = dialog.Selected("conditions"); transition.m_actionIds = dialog.Selected("actions");
            put(d.m_transitions, transition, [](const auto& v) { return v.m_transitionId; }); break;
        }
        case Condition:
            condition = {id, dialog.Get("type"), dialog.Get("subject")}; put(d.m_conditions, condition, [](const auto& v) { return v.m_conditionId; }); break;
        case Action:
            action.m_actionId = id; action.m_actionTypeId = dialog.Get("type"); action.m_subjectId = dialog.Get("subject");
            if (creating) { action.m_idempotencyKey = "idempotency." + id; } put(d.m_actions, action, [](const auto& v) { return v.m_actionId; }); break;
        case Outcome:
            outcome.m_outcomeId = id; outcome.m_phaseId = dialog.Get("phase"); if (creating) { outcome.m_textKey = "loc." + id; }
            put(d.m_outcomes, outcome, [](const auto& v) { return v.m_outcomeId; }); break;
        case Role:
            role.m_roleId = id; role.m_required = dialog.Checked("required"); if (creating) { role.m_displayTextKey = "loc." + id; }
            put(d.m_roles, role, [](const auto& v) { return v.m_roleId; }); break;
        case Requirement:
            requirement = {id, dialog.Get("role"), dialog.Get("subjectKind"), dialog.Get("usage")};
            put(d.m_bindingRequirements, requirement, [](const auto& v) { return v.m_requirementId; }); break;
        case State:
            state = {dialog.Get("key"), dialog.Get("type"), dialog.Get("default"), dialog.Get("description")};
            put(next.m_stateKeys, state, [](const auto& v) { return v.m_keyId; }); break;
        case Binding:
            binding = {dialog.Get("subject"), dialog.Get("record")}; put(next.m_bindings, binding, [](const auto& v) { return v.m_subjectId; }); break;
        }
        if (kind < State)
        {
            next.m_labels.erase(AZStd::remove_if(next.m_labels.begin(), next.m_labels.end(), [&](const auto& l) { return l.m_id == id; }), next.m_labels.end());
            next.m_labels.push_back({id, dialog.Get("label")});
        }
        m_draft = AZStd::move(next); m_draft.m_definition.m_questFingerprint.clear(); m_dirty = true; RefreshRows(); Inspect();
        Status(tr("Element updated in the draft. Save quest to keep these changes."));
    }
    void QuestStateInspectorWidget::RemoveRow()
    {
        const auto id = RowId(m_rows->currentRow()); if (id.empty()) { Status(tr("Select an element to remove.")); return; }
        const auto erase = [&id](auto& values, auto get) { values.erase(AZStd::remove_if(values.begin(), values.end(), [&](const auto& v) { return get(v) == id; }), values.end()); };
        auto& d = m_draft.m_definition;
        switch (m_kind->currentIndex())
        {
        case Phase: erase(d.m_phases, [](const auto& v) { return v.m_phaseId; }); break;
        case Objective: erase(d.m_objectives, [](const auto& v) { return v.m_objectiveId; });
            for (auto& p : d.m_phases) { p.m_objectiveIds.erase(AZStd::remove(p.m_objectiveIds.begin(), p.m_objectiveIds.end(), id), p.m_objectiveIds.end()); } break;
        case Transition: erase(d.m_transitions, [](const auto& v) { return v.m_transitionId; }); break;
        case Condition: erase(d.m_conditions, [](const auto& v) { return v.m_conditionId; }); break;
        case Action: erase(d.m_actions, [](const auto& v) { return v.m_actionId; }); break;
        case Outcome: erase(d.m_outcomes, [](const auto& v) { return v.m_outcomeId; }); break;
        case Role: erase(d.m_roles, [](const auto& v) { return v.m_roleId; }); break;
        case Requirement: erase(d.m_bindingRequirements, [](const auto& v) { return v.m_requirementId; }); break;
        case State: erase(m_draft.m_stateKeys, [](const auto& v) { return v.m_keyId; }); break;
        case Binding: erase(m_draft.m_bindings, [](const auto& v) { return v.m_subjectId; }); break;
        }
        erase(m_draft.m_labels, [](const auto& v) { return v.m_id; }); d.m_questFingerprint.clear(); m_dirty = true; RefreshRows(); Inspect();
        Status(tr("Removed from the draft. Validation identifies any references that still need updating."));
    }
}
