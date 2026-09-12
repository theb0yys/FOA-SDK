/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FactionAuthorityEditorWidget.h"
#include "FoundationService.h"
#include "SocietyPlanningService.h"
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <QCloseEvent>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
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
#include <QVBoxLayout>
#include <QUuid>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString FQ(const AZStd::string& value) { return QString::fromUtf8(value.c_str()); }
        AZStd::string FA(const QString& value)
        {
            const auto bytes = value.toUtf8(); return {bytes.constData(), static_cast<size_t>(bytes.size())};
        }
        void FactionOpenPane(const char* name)
        {
            AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::OpenViewPane, name);
        }
        void FactionSearchable(QComboBox* combo)
        {
            combo->setEditable(true); combo->setInsertPolicy(QComboBox::NoInsert);
            combo->completer()->setFilterMode(Qt::MatchContains);
            combo->completer()->setCompletionMode(QCompleter::PopupCompletion);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            combo->setMinimumContentsLength(18);
        }
        QTableWidgetItem* FactionCell(const QString& text)
        {
            auto* cell = new QTableWidgetItem(text); cell->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); return cell;
        }
    }
    FactionAuthorityEditorWidget::FactionAuthorityEditorWidget(QWidget* parent) : QWidget(parent)
    {
        setObjectName("factionAuthorityEditor");
        auto* outer = new QVBoxLayout(this);
        outer->addWidget(new QLabel(tr("Faction and Authority Editor"), this));
        auto* introduction = new QLabel(tr("Create factions and cultures, assign actors and troops, and plan relationships and jurisdiction."), this);
        introduction->setWordWrap(true); outer->addWidget(introduction);
        auto* actions = new QHBoxLayout();
        const auto action = [this, actions](const QString& label, const char* object)
        {
            auto* button = new QPushButton(label, this); button->setObjectName(object); actions->addWidget(button); return button;
        };
        auto* create = action(tr("New faction"), "factionNew");
        auto* culture = action(tr("New culture"), "factionNewCulture");
        auto* editCulture = action(tr("Edit culture"), "factionEditCulture");
        auto* pack = action(tr("Choose or create mod"), "factionChooseMod");
        auto* actors = action(tr("Actors and troops"), "factionOpenActors");
        outer->addLayout(actions);
        connect(create, &QPushButton::clicked, this, [this]() { Create(false); });
        connect(culture, &QPushButton::clicked, this, [this]() { Create(true); });
        connect(editCulture, &QPushButton::clicked, this, [this]() { EditCulture(); });
        connect(pack, &QPushButton::clicked, this, []() { FactionOpenPane("Tainted Grail Pack Manager"); });
        connect(actors, &QPushButton::clicked, this, []() { FactionOpenPane("Tainted Grail Actor and Troop Editor"); });
        m_summary = new QLabel(this); m_summary->setWordWrap(true); outer->addWidget(m_summary);
        m_records = new QComboBox(this); m_records->setObjectName("factionRecords");
        m_records->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        outer->addWidget(m_records);
        connect(m_records, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]()
        {
            if (m_loading) { return; }
            if (m_dirty)
            {
                const QSignalBlocker block(m_records);
                m_records->setCurrentIndex(m_records->findData(FQ(m_draft.m_profile.m_recordId)));
                Status(tr("Save or revert your changes before selecting another faction."), true); return;
            }
            Load(FA(m_records->currentData().toString()));
        });
        auto* scroll = new QScrollArea(this); scroll->setObjectName("factionScroll"); scroll->setWidgetResizable(true);
        m_form = new QWidget(scroll); auto* layout = new QVBoxLayout(m_form); auto* form = new QFormLayout();
        const auto field = [this, form](const QString& label, const char* name, int length)
        {
            auto* line = new QLineEdit(m_form); line->setObjectName(name); line->setMaxLength(length); form->addRow(label, line); return line;
        };
        m_name = field(tr("Faction name"), "factionName", 512);
        m_culture = new QComboBox(m_form); m_culture->setObjectName("factionCulture"); form->addRow(tr("Culture"), m_culture);
        m_description = field(tr("Description"), "factionDescription", 2048);
        m_authority = field(tr("Authority and leadership"), "factionAuthority", 2048);
        layout->addLayout(form);
        auto* tabs = new QTabWidget(m_form); tabs->setObjectName("factionTabs");
        const char* kinds[] = {"member", "disposition", "jurisdiction"};
        const char* prefixes[] = {"factionMember", "factionDisposition", "factionJurisdiction"};
        const QString labels[] = {tr("Members and leadership"), tr("Faction relationships"), tr("Jurisdiction")};
        for (int index = 0; index < 3; ++index)
        {
            auto& controls = m_links[index]; controls.kind = kinds[index];
            auto* page = new QWidget(tabs); auto* pageLayout = new QVBoxLayout(page); auto* inputs = new QFormLayout();
            auto* explanation = new QLabel(index == 0 ? tr("Assign saved actors or troops. One actor may be the declared leader.")
                : index == 1 ? tr("These relationships describe this faction's view of the target. Reverse relationships are separate.")
                : tr("Describe controlled, claimed or protected territory. An unbound reference remains unverified."), page);
            explanation->setWordWrap(true); pageLayout->addWidget(explanation);
            controls.target = new QComboBox(page); controls.target->setObjectName(QString(prefixes[index]) + "Target");
            FactionSearchable(controls.target); inputs->addRow(index == 0 ? tr("Actor or troop") : index == 1 ? tr("Target faction") : tr("World location"), controls.target);
            controls.value = new QComboBox(page); controls.value->setObjectName(QString(prefixes[index]) + "Value");
            const QStringList values = index == 0 ? QStringList{"member", "officer", "leader"}
                : index == 1 ? QStringList{"friendly", "neutral", "hostile"} : QStringList{"controls", "claims", "protects"};
            for (const auto& value : values) { auto label = value; label[0] = label[0].toUpper(); controls.value->addItem(label, value); }
            inputs->addRow(index == 0 ? tr("Role") : index == 1 ? tr("Relationship") : tr("Authority"), controls.value);
            controls.reference = new QLineEdit(page); controls.reference->setObjectName(QString(prefixes[index]) + "Reference");
            controls.reference->setMaxLength(1024);
            if (index == 2) { inputs->addRow(tr("Unverified territory reference"), controls.reference); } else { controls.reference->hide(); }
            controls.notes = new QLineEdit(page); controls.notes->setObjectName(QString(prefixes[index]) + "Notes");
            controls.notes->setMaxLength(1024); inputs->addRow(tr("Notes"), controls.notes); pageLayout->addLayout(inputs);
            auto* buttons = new QHBoxLayout();
            for (int operation = 0; operation < 3; ++operation)
            {
                const QString suffix = operation == 0 ? "Add" : operation == 1 ? "Update" : "Remove";
                auto* button = new QPushButton(operation == 0 ? tr("Add") : operation == 1 ? tr("Update selected") : tr("Remove selected"), page);
                button->setObjectName(QString(prefixes[index]) + suffix); buttons->addWidget(button);
                connect(button, &QPushButton::clicked, this, [this, index, operation]()
                { if (operation == 2) { RemoveLink(index); } else { ChangeLink(index, operation == 1); } });
            }
            buttons->addStretch(); pageLayout->addLayout(buttons);
            controls.table = new QTableWidget(0, 3, page); controls.table->setObjectName(QString(prefixes[index]) + "Table");
            controls.table->setHorizontalHeaderLabels({tr("Target"),
                index == 0 ? tr("Role") : index == 1 ? tr("Relationship") : tr("Authority"), tr("Notes")});
            controls.table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            controls.table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
            controls.table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
            controls.table->setSelectionBehavior(QAbstractItemView::SelectRows);
            controls.table->setSelectionMode(QAbstractItemView::SingleSelection); controls.table->setMinimumHeight(180);
            pageLayout->addWidget(controls.table);
            connect(controls.table, &QTableWidget::itemSelectionChanged, this, [this, index]() { SelectLink(index); });
            tabs->addTab(page, labels[index]);
        }
        connect(m_links[2].target, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]()
        { m_links[2].reference->setEnabled(m_links[2].target->currentData().toString().isEmpty()); });
        layout->addWidget(tabs);
        m_preview = new QLabel(m_form); m_preview->setObjectName("factionPreview"); m_preview->setTextFormat(Qt::PlainText);
        m_preview->setWordWrap(true); m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(m_preview);
        scroll->setWidget(m_form); outer->addWidget(scroll, 1);
        auto* saveActions = new QHBoxLayout();
        m_save = new QPushButton(tr("Save faction"), this); m_save->setObjectName("factionSave");
        auto* revert = new QPushButton(tr("Revert changes"), this); revert->setObjectName("factionRevert");
        saveActions->addWidget(m_save); saveActions->addWidget(revert); saveActions->addStretch(); outer->addLayout(saveActions);
        m_status = new QLabel(this); m_status->setObjectName("factionStatus"); m_status->setWordWrap(true); outer->addWidget(m_status);
        connect(m_save, &QPushButton::clicked, this, [this]() { Save(); });
        connect(revert, &QPushButton::clicked, this, [this]()
        { const auto id = m_draft.m_profile.m_recordId; m_dirty = false; RefreshChoices(); Load(id); Status(tr("Reloaded the saved faction.")); });
        for (auto* line : {m_name, m_description, m_authority}) { connect(line, &QLineEdit::textEdited, this, [this]() { MarkDirty(); }); }
        connect(m_culture, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { MarkDirty(); });
        if (FoundationService::Get().GetWorkspaceFilePath().empty()) { FoundationService::Get().RefreshLocalSetup(); }
        FoundationNotificationBus::Handler::BusConnect(); RefreshChoices(); Load({});
    }
    FactionAuthorityEditorWidget::~FactionAuthorityEditorWidget() { FoundationNotificationBus::Handler::BusDisconnect(); }
    void FactionAuthorityEditorWidget::closeEvent(QCloseEvent* event)
    {
        if (m_dirty && QMessageBox::warning(this, tr("Unsaved faction"), tr("Discard your faction changes and close?"),
            QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Discard)
        { event->ignore(); return; }
        QWidget::closeEvent(event);
    }
    void FactionAuthorityEditorWidget::Status(const QString& text, bool error)
    { m_status->setText(text); m_status->setProperty("error", error); }
    bool FactionAuthorityEditorWidget::SameWorkspace() const
    {
        const auto& service = FoundationService::Get(); const auto& workspace = service.GetWorkspace();
        const auto* profile = workspace.FindActiveGameProfile();
        return profile && workspace.m_workspaceId == m_workspaceId && workspace.m_activeGameProfileId == m_profileId
            && service.GetWorkspaceFilePath() == m_workspacePath && profile->m_gameVersion == m_gameVersion
            && profile->m_branch == m_branch && profile->m_runtimeTarget == m_runtimeTarget;
    }
    void FactionAuthorityEditorWidget::OnFoundationChanged()
    {
        if (m_saving) { return; }
        if (m_dirty) { Preview(); Status(tr("The catalog or workspace changed. Your draft is preserved; save in its original mod or revert."), true); return; }
        const auto id = m_draft.m_profile.m_recordId; RefreshChoices(); Load(id);
    }
    void FactionAuthorityEditorWidget::RefreshChoices()
    {
        const QSignalBlocker rb(m_records), cb(m_culture);
        const auto selected = m_records->currentData(), culture = m_culture->currentData();
        m_records->clear(); m_records->addItem(tr("Choose a faction"), "");
        m_culture->clear(); m_culture->addItem(tr("No culture assigned"), "");
        const auto& service = FoundationService::Get(); const auto& catalog = service.GetCatalog();
        for (const auto& profile : catalog.GetFactionProfiles())
        {
            const auto* record = catalog.FindByRecordId(profile.m_recordId);
            if (record) { m_records->addItem(FQ(record->m_displayName), FQ(record->m_recordId)); }
        }
        for (const auto& profile : catalog.GetCultureProfiles())
        {
            const auto* record = catalog.FindByRecordId(profile.m_recordId);
            if (record) { m_culture->addItem(FQ(record->m_displayName), FQ(record->m_recordId)); }
        }
        for (int index = 0; index < 3; ++index)
        {
            auto* target = m_links[index].target; const QSignalBlocker block(target); const auto previous = target->currentData(); target->clear();
            if (index == 2) { target->addItem(tr("Unverified reference only"), ""); }
            for (const auto& record : catalog.GetRecords())
            {
                const bool included = index == 0 ? record.m_domain == "population"
                    && ((record.m_recordKind == "actor" && catalog.FindPopulationActorProfile(record.m_recordId))
                        || (record.m_recordKind == "troop" && catalog.FindPopulationTroopProfile(record.m_recordId)))
                    : index == 1 ? record.m_domain == "society" && record.m_recordKind == "faction" && catalog.FindFactionProfile(record.m_recordId)
                    : record.m_domain == "world" && (record.m_recordKind == "location" || record.m_recordKind == "scene" || record.m_recordKind == "region");
                if (included) { target->addItem(FQ(record.m_displayName) + " [" + FQ(record.m_recordKind) + "]", FQ(record.m_recordId)); }
            }
            target->setCurrentIndex(qMax(0, target->findData(previous)));
        }
        m_links[2].reference->setEnabled(m_links[2].target->currentData().toString().isEmpty());
        m_records->setCurrentIndex(qMax(0, m_records->findData(selected))); m_culture->setCurrentIndex(qMax(0, m_culture->findData(culture)));
        const auto* pack = service.GetActivePack();
        m_summary->setText(tr("%1 factions | %2 cultures | Active mod: %3").arg(catalog.GetFactionProfiles().size())
            .arg(catalog.GetCultureProfiles().size()).arg(pack ? FQ(pack->m_displayName) : tr("none selected")));
    }
    void FactionAuthorityEditorWidget::Load(const AZStd::string& id)
    {
        m_loading = true; const auto& service = FoundationService::Get(); const auto& catalog = service.GetCatalog();
        const auto* profile = catalog.FindFactionProfile(id); const auto* record = catalog.FindByRecordId(id);
        m_draft = profile ? FactionDefinition{*profile, catalog.FindFactionLinks(id)} : FactionDefinition{};
        m_workspaceId = service.GetWorkspace().m_workspaceId; m_profileId = service.GetWorkspace().m_activeGameProfileId;
        m_workspacePath = service.GetWorkspaceFilePath();
        const auto* activeProfile = service.GetWorkspace().FindActiveGameProfile();
        m_gameVersion = activeProfile ? activeProfile->m_gameVersion : AZStd::string{};
        m_branch = activeProfile ? activeProfile->m_branch : AZStd::string{};
        m_runtimeTarget = activeProfile ? activeProfile->m_runtimeTarget : AZStd::string{};
        m_records->setCurrentIndex(qMax(0, m_records->findData(FQ(m_draft.m_profile.m_recordId))));
        m_name->setText(record ? FQ(record->m_displayName) : QString{});
        m_culture->setCurrentIndex(qMax(0, m_culture->findData(FQ(m_draft.m_profile.m_cultureRecordId))));
        m_description->setText(FQ(m_draft.m_profile.m_description)); m_authority->setText(FQ(m_draft.m_profile.m_authorityNotes));
        m_form->setEnabled(profile != nullptr); m_dirty = false; RefreshLinks(); m_loading = false; Preview();
    }
    void FactionAuthorityEditorWidget::ReadFields()
    {
        m_draft.m_profile.m_cultureRecordId = FA(m_culture->currentData().toString());
        m_draft.m_profile.m_description = FA(m_description->text().trimmed()); m_draft.m_profile.m_authorityNotes = FA(m_authority->text().trimmed());
    }
    void FactionAuthorityEditorWidget::MarkDirty()
    {
        if (m_loading || m_draft.m_profile.m_recordId.empty()) { return; }
        ReadFields(); m_dirty = true; Preview(); Status(tr("Unsaved faction changes."));
    }
    void FactionAuthorityEditorWidget::Preview()
    {
        if (m_draft.m_profile.m_recordId.empty()) { m_preview->setText(tr("Create or select a faction to begin.")); m_save->setEnabled(false); return; }
        const auto result = SocietyPlanningService::Analyze(m_draft, FoundationService::Get().GetCatalog());
        QStringList lines{tr("%1 members | %2 directed relationships | %3 jurisdiction plans | Leader: %4")
            .arg(result.m_members).arg(result.m_dispositions).arg(result.m_jurisdictions)
            .arg(result.m_leaderName.empty() ? tr("unassigned") : FQ(result.m_leaderName))};
        size_t shown = 0;
        for (const auto& error : result.m_errors) { if (++shown > 12) { break; } lines << tr("Fix: %1").arg(FQ(error)); }
        for (const auto& warning : result.m_warnings) { if (++shown > 12) { break; } lines << FQ(warning); }
        const size_t total = result.m_errors.size() + result.m_warnings.size();
        if (total > 12) { lines << tr("%1 more review notes.").arg(total - 12); }
        m_preview->setText(lines.join('\n')); m_save->setEnabled(m_dirty && SameWorkspace());
    }
    void FactionAuthorityEditorWidget::RefreshLinks()
    {
        const auto& catalog = FoundationService::Get().GetCatalog();
        for (auto& controls : m_links)
        {
            const QSignalBlocker block(controls.table); controls.table->setRowCount(0);
            for (const auto& link : m_draft.m_links)
            {
                if (link.m_kind != controls.kind) { continue; }
                const auto* record = catalog.FindByRecordId(link.m_targetRecordId);
                auto* cell = FactionCell(record ? FQ(record->m_displayName) : tr("Unverified: %1").arg(FQ(link.m_targetSubjectRef)));
                cell->setData(Qt::UserRole, FQ(link.m_linkId)); cell->setToolTip(FQ(link.m_targetRecordId));
                const int row = controls.table->rowCount(); controls.table->insertRow(row); controls.table->setItem(row, 0, cell);
                controls.table->setItem(row, 1, FactionCell(FQ(link.m_value))); controls.table->setItem(row, 2, FactionCell(FQ(link.m_notes)));
            }
        }
    }
    void FactionAuthorityEditorWidget::SelectLink(int index)
    {
        auto& controls = m_links[index]; const int row = controls.table->currentRow(); if (row < 0) { return; }
        const auto id = FA(controls.table->item(row, 0)->data(Qt::UserRole).toString());
        for (const auto& link : m_draft.m_links)
        {
            if (link.m_linkId != id) { continue; }
            controls.target->setCurrentIndex(controls.target->findData(FQ(link.m_targetRecordId)));
            controls.reference->setText(FQ(link.m_targetSubjectRef));
            controls.value->setCurrentIndex(controls.value->findData(FQ(link.m_value))); controls.notes->setText(FQ(link.m_notes)); break;
        }
    }
    void FactionAuthorityEditorWidget::ChangeLink(int index, bool update)
    {
        auto& controls = m_links[index]; const int row = controls.table->currentRow();
        if (update && row < 0) { Status(tr("Select a link to update."), true); return; }
        if (controls.target->currentText() != controls.target->itemText(controls.target->currentIndex()))
        { Status(tr("Choose a matching saved target from the list."), true); return; }
        FactionLink link;
        link.m_linkId = update ? FA(controls.table->item(row, 0)->data(Qt::UserRole).toString())
            : FA("custom.faction-link." + QUuid::createUuid().toString(QUuid::WithoutBraces));
        link.m_factionRecordId = m_draft.m_profile.m_recordId; link.m_kind = controls.kind;
        link.m_targetRecordId = FA(controls.target->currentData().toString());
        const auto* target = FoundationService::Get().GetCatalog().FindByRecordId(link.m_targetRecordId);
        link.m_targetSubjectRef = target ? target->m_subjectRef : FA(controls.reference->text().trimmed());
        link.m_value = FA(controls.value->currentData().toString()); link.m_notes = FA(controls.notes->text().trimmed());
        FactionDefinition candidate = m_draft;
        if (update)
        {
            for (auto& current : candidate.m_links) { if (current.m_linkId == link.m_linkId) { current = link; break; } }
        }
        else { candidate.m_links.push_back(link); }
        const auto result = SocietyPlanningService::Analyze(candidate, FoundationService::Get().GetCatalog());
        if (!result.IsValid()) { Status(FQ(result.m_errors.front()), true); return; }
        m_draft = AZStd::move(candidate); RefreshLinks(); MarkDirty();
    }
    void FactionAuthorityEditorWidget::RemoveLink(int index)
    {
        auto* table = m_links[index].table; const int row = table->currentRow();
        if (row < 0) { Status(tr("Select a link to remove."), true); return; }
        const auto id = FA(table->item(row, 0)->data(Qt::UserRole).toString());
        for (auto it = m_draft.m_links.begin(); it != m_draft.m_links.end(); ++it)
        { if (it->m_linkId == id) { m_draft.m_links.erase(it); break; } }
        RefreshLinks(); MarkDirty();
    }
    void FactionAuthorityEditorWidget::Create(bool culture)
    {
        if (m_dirty) { Status(tr("Save or revert the faction draft before creating another definition."), true); return; }
        bool accepted = false;
        const auto name = QInputDialog::getText(this, culture ? tr("New culture") : tr("New faction"),
            tr("Name"), QLineEdit::Normal, "", &accepted);
        if (!accepted) { return; }
        AZStd::string id, error; m_saving = true; auto& service = FoundationService::Get();
        const bool success = culture ? service.CreateCultureProfile(FA(name), id, &error) : service.CreateFactionDefinition(FA(name), id, &error);
        m_saving = false;
        if (!success) { Status(FQ(error), true); return; }
        const auto selected = m_draft.m_profile.m_recordId; RefreshChoices(); Load(culture ? selected : id);
        Status(culture ? tr("Culture created and saved. Use Edit culture to add details.") : tr("Faction created and saved."));
    }
    void FactionAuthorityEditorWidget::EditCulture()
    {
        if (m_dirty) { Status(tr("Save or revert the faction draft before editing a culture."), true); return; }
        RefreshChoices(); if (m_culture->count() <= 1) { Status(tr("Create a culture first."), true); return; }
        QStringList choices; for (int i = 1; i < m_culture->count(); ++i) { choices << QString("%1. %2").arg(i).arg(m_culture->itemText(i)); }
        bool accepted = false;
        const auto choice = QInputDialog::getItem(this, tr("Edit culture"), tr("Saved culture"), choices, 0, false, &accepted);
        if (!accepted) { return; }
        const auto id = FA(m_culture->itemData(choices.indexOf(choice) + 1).toString());
        auto& service = FoundationService::Get(); const auto* existing = service.GetCatalog().FindCultureProfile(id);
        const auto* record = service.GetCatalog().FindByRecordId(id); if (!existing || !record) { return; }
        CultureProfile profile = *existing;
        QDialog dialog(this); dialog.setObjectName("cultureEditorDialog"); dialog.setWindowTitle(tr("Edit culture"));
        auto* layout = new QVBoxLayout(&dialog); auto* form = new QFormLayout();
        QLineEdit name(FQ(record->m_displayName), &dialog), description(FQ(profile.m_description), &dialog), language(FQ(profile.m_language), &dialog);
        name.setObjectName("cultureName"); description.setObjectName("cultureDescription"); language.setObjectName("cultureLanguage");
        name.setMaxLength(512); description.setMaxLength(2048); language.setMaxLength(128);
        form->addRow(tr("Name"), &name); form->addRow(tr("Description"), &description); form->addRow(tr("Language"), &language); layout->addLayout(form);
        QLabel errorLabel(&dialog); errorLabel.setObjectName("cultureStatus"); errorLabel.setWordWrap(true); layout->addWidget(&errorLabel);
        QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
        buttons.button(QDialogButtonBox::Save)->setObjectName("cultureSave"); layout->addWidget(&buttons);
        const auto workspaceId = service.GetWorkspace().m_workspaceId, workspacePath = service.GetWorkspaceFilePath();
        const auto profileId = service.GetWorkspace().m_activeGameProfileId;
        const auto* activeProfile = service.GetWorkspace().FindActiveGameProfile();
        const auto gameVersion = activeProfile ? activeProfile->m_gameVersion : AZStd::string{};
        const auto branch = activeProfile ? activeProfile->m_branch : AZStd::string{};
        const auto runtimeTarget = activeProfile ? activeProfile->m_runtimeTarget : AZStd::string{};
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&]()
        {
            const auto* currentProfile = service.GetWorkspace().FindActiveGameProfile();
            if (!currentProfile || service.GetWorkspace().m_workspaceId != workspaceId || service.GetWorkspaceFilePath() != workspacePath
                || service.GetWorkspace().m_activeGameProfileId != profileId || currentProfile->m_gameVersion != gameVersion
                || currentProfile->m_branch != branch || currentProfile->m_runtimeTarget != runtimeTarget)
            { errorLabel.setText(tr("The workspace changed. Close this dialog and reopen the culture.")); return; }
            profile.m_description = FA(description.text().trimmed()); profile.m_language = FA(language.text().trimmed());
            AZStd::string error; m_saving = true; const bool saved = service.SaveCultureProfile(profile, FA(name.text()), &error); m_saving = false;
            if (!saved) { errorLabel.setText(FQ(error)); return; }
            dialog.accept();
        });
        if (dialog.exec() == QDialog::Accepted)
        { const auto selected = m_draft.m_profile.m_recordId; RefreshChoices(); Load(selected); Status(tr("Culture saved.")); }
    }
    void FactionAuthorityEditorWidget::Save()
    {
        if (!SameWorkspace()) { Status(tr("Return to the original workspace and profile before saving this draft."), true); return; }
        ReadFields(); AZStd::string error; m_saving = true;
        const bool success = FoundationService::Get().SaveFactionDefinition(m_draft, FA(m_name->text()), &error); m_saving = false;
        if (!success) { Status(FQ(error), true); return; }
        const auto id = m_draft.m_profile.m_recordId; m_dirty = false; RefreshChoices(); Load(id); Status(tr("Faction saved."));
    }
}
