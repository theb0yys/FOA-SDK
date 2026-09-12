/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetLocalisationManagerWidget.h"
#include "AssetLocalisationService.h"
#include "FoundationService.h"
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <QCloseEvent>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString Q(const AZStd::string& s) { return QString::fromUtf8(s.data(), static_cast<int>(s.size())); }
        AZStd::string A(const QString& s) { const auto b = s.toUtf8(); return {b.constData(), static_cast<size_t>(b.size())}; }
        QLabel* Label(QWidget* parent, const char* name)
        { auto* p = new QLabel(parent); p->setObjectName(name); p->setWordWrap(true); p->setTextFormat(Qt::PlainText); return p; }
        QComboBox* Combo(QWidget* parent, const char* name)
        {
            auto* p = new QComboBox(parent); p->setObjectName(name); p->setMaxVisibleItems(20);
            p->setMinimumContentsLength(16); p->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); return p;
        }
        QLineEdit* Line(QFormLayout* form, QWidget* parent, const QString& title, const char* name, int length)
        { auto* p = new QLineEdit(parent); p->setObjectName(name); p->setMaxLength(length); form->addRow(title, p); return p; }
    }
    AssetLocalisationManagerWidget::AssetLocalisationManagerWidget(QWidget* parent) : QWidget(parent)
    {
        setObjectName("assetLocalisationManager");
        auto* outer = new QVBoxLayout(this);
        auto* heading = Label(this, "assetHeading"); heading->setText(tr("Asset and Localisation Manager")); outer->addWidget(heading);
        auto* intro = Label(this, "assetIntro"); intro->setText(tr("Add images and translated text to your mod, then assign them to items, actors or quests.")); outer->addWidget(intro);
        auto* actions = new QHBoxLayout();
        const auto button = [this, actions](const QString& title, const char* name)
        { auto* b = new QPushButton(title, this); b->setObjectName(name); actions->addWidget(b); return b; };
        connect(button(tr("New image"), "assetNewImage"), &QPushButton::clicked, this, [this]() { Create(1); });
        connect(button(tr("New translation"), "assetNewText"), &QPushButton::clicked, this, [this]() { Create(2); });
        connect(button(tr("Choose or create mod"), "assetChooseMod"), &QPushButton::clicked, this, []()
        { AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::OpenViewPane, "Tainted Grail Pack Manager"); });
        outer->addLayout(actions); m_summary = Label(this, "assetSummary"); outer->addWidget(m_summary);
        auto* tabs = new QTabWidget(this); tabs->setObjectName("assetTabs"); outer->addWidget(tabs, 1);
        auto* library = new QWidget(tabs); auto* layout = new QVBoxLayout(library);
        m_search = new QLineEdit(library); m_search->setObjectName("assetSearch"); m_search->setPlaceholderText(tr("Search saved images and text keys")); layout->addWidget(m_search);
        m_records = Combo(library, "assetRecords"); layout->addWidget(m_records);
        auto* scroll = new QScrollArea(library); scroll->setWidgetResizable(true); m_pages = new QStackedWidget(scroll);
        auto* empty = Label(m_pages, "assetEmpty"); empty->setText(tr("Select a saved entry, or create an image or translation.")); m_pages->addWidget(empty);
        auto* imagePage = new QWidget(m_pages); auto* imageForm = new QFormLayout(imagePage); imageForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        m_name = Line(imageForm, imagePage, tr("Image name"), "assetName", 512);
        auto* choose = new QPushButton(tr("Choose PNG or JPEG"), imagePage); choose->setObjectName("assetChooseImage"); imageForm->addRow(choose);
        m_imageDetails = Label(imagePage, "assetImageDetails"); imageForm->addRow(m_imageDetails);
        m_provenance = Line(imageForm, imagePage, tr("Image source / creator"), "assetProvenance", 2048);
        m_rights = Combo(imagePage, "assetRights"); m_rights->addItem(tr("My original work"), "original_work"); m_rights->addItem(tr("Licensed work"), "licensed"); imageForm->addRow(tr("Source rights"), m_rights);
        m_licence = Line(imageForm, imagePage, tr("Licence details"), "assetLicence", 2048);
        m_distribution = Combo(imagePage, "assetRedistribution"); m_distribution->addItem(tr("Not reviewed"), "not_reviewed");
        m_distribution->addItem(tr("Declared permitted"), "declared_permitted"); m_distribution->addItem(tr("Prohibited"), "prohibited"); imageForm->addRow(tr("Redistribution"), m_distribution);
        m_imageLabel = Label(imagePage, "assetImagePreview"); m_imageLabel->setMinimumSize(240, 180); m_imageLabel->setMaximumHeight(340); m_imageLabel->setAlignment(Qt::AlignCenter); imageForm->addRow(m_imageLabel);
        m_pages->addWidget(imagePage);
        auto* textPage = new QWidget(m_pages); auto* textLayout = new QVBoxLayout(textPage); auto* textForm = new QFormLayout();
        m_key = Line(textForm, textPage, tr("Text key"), "assetTextKey", 256);
        m_defaultLanguage = Line(textForm, textPage, tr("Default language"), "assetDefaultLanguage", 16); textLayout->addLayout(textForm);
        m_variants = new QTableWidget(0, 2, textPage); m_variants->setObjectName("assetVariants"); m_variants->setHorizontalHeaderLabels({tr("Language"), tr("Translation")});
        m_variants->setSelectionBehavior(QAbstractItemView::SelectRows); m_variants->setSelectionMode(QAbstractItemView::SingleSelection);
        m_variants->setEditTriggers(QAbstractItemView::NoEditTriggers); m_variants->horizontalHeader()->setStretchLastSection(true); textLayout->addWidget(m_variants);
        auto* variantsActions = new QHBoxLayout();
        for (int i = 0; i < 3; ++i)
        {
            auto* b = new QPushButton(i == 0 ? tr("Add language") : i == 1 ? tr("Edit translation") : tr("Remove language"), textPage);
            b->setObjectName(i == 0 ? "assetVariantAdd" : i == 1 ? "assetVariantEdit" : "assetVariantRemove"); variantsActions->addWidget(b);
            connect(b, &QPushButton::clicked, this, [this, i]() { if (i == 2) { RemoveVariant(); } else { EditVariant(i == 0); } });
        }
        textLayout->addLayout(variantsActions);
        connect(m_variants, &QTableWidget::cellDoubleClicked, this, [this]() { EditVariant(false); });
        auto* languageForm = new QFormLayout(); m_language = Line(languageForm, textPage, tr("Preview language"), "assetPreviewLanguage", 16); m_language->setText("en"); textLayout->addLayout(languageForm);
        m_fallback = Label(textPage, "assetFallback"); textLayout->addWidget(m_fallback);
        m_textPreview = new QPlainTextEdit(textPage); m_textPreview->setReadOnly(true); m_textPreview->setObjectName("assetTextPreview"); textLayout->addWidget(m_textPreview);
        m_pages->addWidget(textPage); scroll->setWidget(m_pages); layout->addWidget(scroll, 1); tabs->addTab(library, tr("Images and translations"));
        auto* assignments = new QWidget(tabs); auto* assignmentLayout = new QVBoxLayout(assignments);
        m_targetSearch = new QLineEdit(assignments); m_targetSearch->setObjectName("assetTargetSearch"); m_targetSearch->setPlaceholderText(tr("Search items, actors and quests")); assignmentLayout->addWidget(m_targetSearch);
        auto* assignmentForm = new QFormLayout(); assignmentForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        m_target = Combo(assignments, "assetTarget"); m_slot = Combo(assignments, "assetSlot"); m_value = Combo(assignments, "assetValue");
        assignmentForm->addRow(tr("Content"), m_target); assignmentForm->addRow(tr("Assign to"), m_slot); assignmentForm->addRow(tr("Image or text"), m_value); assignmentLayout->addLayout(assignmentForm);
        auto* assignmentActions = new QHBoxLayout();
        auto* assign = new QPushButton(tr("Save assignment"), assignments); assign->setObjectName("assetAssign");
        auto* clear = new QPushButton(tr("Clear assignment"), assignments); clear->setObjectName("assetClearAssignment"); assignmentActions->addWidget(assign); assignmentActions->addWidget(clear); assignmentLayout->addLayout(assignmentActions);
        m_bindingStatus = Label(assignments, "assetBindingStatus"); assignmentLayout->addWidget(m_bindingStatus);
        m_assignmentPreview = Label(assignments, "assetAssignmentPreview"); m_assignmentPreview->setMinimumSize(240, 240); m_assignmentPreview->setAlignment(Qt::AlignCenter); m_assignmentPreview->setTextInteractionFlags(Qt::TextSelectableByMouse); assignmentLayout->addWidget(m_assignmentPreview, 1);
        auto* note = Label(assignments, "assetAssignmentNote"); note->setText(tr("Assignments are saved for the active mod. Text previews use the preview language in Images and translations. Game deployment is a separate step.")); assignmentLayout->addWidget(note); tabs->addTab(assignments, tr("Content assignments"));
        auto* saveRow = new QHBoxLayout(); auto* save = new QPushButton(tr("Save entry"), this); save->setObjectName("assetSave");
        auto* revert = new QPushButton(tr("Revert changes"), this); revert->setObjectName("assetRevert"); saveRow->addWidget(save); saveRow->addWidget(revert); outer->addLayout(saveRow);
        m_status = Label(this, "assetStatus"); outer->addWidget(m_status);
        connect(save, &QPushButton::clicked, this, [this]() { Save(); }); connect(revert, &QPushButton::clicked, this, [this]() { Revert(); });
        connect(choose, &QPushButton::clicked, this, [this]() { ChooseImage(); });
        connect(m_search, &QLineEdit::textChanged, this, [this]() { Refresh(); });
        connect(m_records, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]()
        {
            if (m_loading) { return; }
            if (m_dirty) { const QSignalBlocker b(m_records); m_records->setCurrentIndex(qMax(0, m_records->findData(Q(m_id)))); Status(tr("Save or revert your draft before selecting another entry.")); return; }
            Load(A(m_records->currentData().toString()));
        });
        for (auto* p : {m_name, m_provenance, m_licence, m_key, m_defaultLanguage}) { connect(p, &QLineEdit::textEdited, this, [this]() { MarkDirty(); }); }
        for (auto* p : {m_rights, m_distribution}) { connect(p, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { MarkDirty(); }); }
        connect(m_language, &QLineEdit::textChanged, this, [this]() { Preview(); PreviewAssignment(); });
        connect(m_targetSearch, &QLineEdit::textChanged, this, [this]() { RefreshTargets(); });
        connect(m_target, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { SelectTarget(); });
        connect(m_slot, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { SelectSlot(); });
        connect(m_value, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { PreviewAssignment(); });
        connect(assign, &QPushButton::clicked, this, [this]() { Assign(false); }); connect(clear, &QPushButton::clicked, this, [this]() { Assign(true); });
        if (FoundationService::Get().GetWorkspaceFilePath().empty()) { FoundationService::Get().RefreshLocalSetup(); }
        FoundationNotificationBus::Handler::BusConnect(); Load({}); RefreshTargets();
    }
    AssetLocalisationManagerWidget::~AssetLocalisationManagerWidget() { FoundationNotificationBus::Handler::BusDisconnect(); }
    QString AssetLocalisationManagerWidget::Context() const
    {
        const auto& s = FoundationService::Get(); const auto& w = s.GetWorkspace(); const auto* p = w.FindActiveGameProfile(); const auto* pack = s.GetActivePack();
        return Q(s.GetWorkspaceFilePath()) + "|" + Q(w.m_workspaceId) + "|" + Q(w.m_activeGameProfileId)
            + "|" + (p ? Q(p->m_gameVersion + "|" + p->m_branch + "|" + p->m_runtimeTarget) : QString{})
            + "|" + (pack ? Q(pack->m_packId) : QString{}) + "|" + Q(s.GetActivePackFilePath());
    }
    void AssetLocalisationManagerWidget::Status(const QString& s) { m_status->setText(s); }
    void AssetLocalisationManagerWidget::Refresh()
    {
        const QSignalBlocker b(m_records); m_records->clear(); m_records->addItem(tr("Choose an entry"), "");
        const auto& s = FoundationService::Get(); const auto* pack = s.GetActivePack(); int images = 0, texts = 0;
        for (const auto& r : s.GetCatalog().GetRecords())
        {
            if (!pack || r.m_ownerPackId != pack->m_packId || (r.m_domain != "assets" && r.m_domain != "localisation")) { continue; }
            r.m_domain == "assets" ? ++images : ++texts;
            const auto title = (r.m_domain == "assets" ? tr("Image: ") : tr("Text: ")) + Q(r.m_displayName);
            if (r.m_recordId == m_id || title.contains(m_search->text(), Qt::CaseInsensitive)) { m_records->addItem(title, Q(r.m_recordId)); }
        }
        m_records->setCurrentIndex(qMax(0, m_records->findData(Q(m_id))));
        m_summary->setText(tr("%1 images | %2 translations | Active mod: %3").arg(images).arg(texts).arg(pack ? Q(pack->m_displayName) : tr("none selected")));
    }
    void AssetLocalisationManagerWidget::Load(AZStd::string id)
    {
        m_id.clear(); m_revision.clear(); m_source.clear(); m_asset = {}; m_entry = {}; m_kind = 0; m_image = {};
        const auto& s = FoundationService::Get(); const auto& catalog = s.GetCatalog(); const auto* r = catalog.FindByRecordId(id); const auto* pack = s.GetActivePack();
        if (r && pack && r->m_ownerPackId == pack->m_packId)
        {
            if (const auto* p = catalog.FindProjectAsset(id)) { m_kind = 1; m_asset = *p; m_id = id; m_revision = AssetLocalisationService::Revision(*p, r->m_displayName); }
            if (const auto* p = catalog.FindLocalisationEntry(id)) { m_kind = 2; m_entry = *p; m_id = id; m_revision = AssetLocalisationService::Revision(*p); }
        }
        m_dirty = false; m_context = Context(); Populate(); Refresh();
        if (m_kind == 1) { AZStd::string error; if (!s.ReadProjectAssetImage(m_id, m_image, &error)) { m_imageDetails->setText(Q(error)); } DrawImage(); }
    }
    void AssetLocalisationManagerWidget::Populate()
    {
        m_loading = true; const auto* record = FoundationService::Get().GetCatalog().FindByRecordId(m_id);
        m_name->setText(record ? Q(record->m_displayName) : QString{}); m_provenance->setText(Q(m_asset.m_provenance)); m_licence->setText(Q(m_asset.m_licence));
        m_rights->setCurrentIndex(qMax(0, m_rights->findData(Q(m_asset.m_sourceRights)))); m_distribution->setCurrentIndex(qMax(0, m_distribution->findData(Q(m_asset.m_redistribution))));
        m_imageDetails->setText(m_asset.m_sourcePath.empty() ? tr("Choose an image up to 8 MiB and 4,194,304 pixels.") : tr("%1 x %2 pixels | %3 bytes").arg(m_asset.m_width).arg(m_asset.m_height).arg(m_asset.m_byteSize));
        m_key->setText(Q(m_entry.m_key)); m_defaultLanguage->setText(Q(m_entry.m_defaultLanguage));
        m_variants->setRowCount(static_cast<int>(m_entry.m_variants.size()));
        for (int i = 0; i < m_variants->rowCount(); ++i) { const auto& v = m_entry.m_variants[i]; m_variants->setItem(i, 0, new QTableWidgetItem(Q(v.m_language))); m_variants->setItem(i, 1, new QTableWidgetItem(Q(v.m_text))); }
        m_pages->setCurrentIndex(m_kind); m_loading = false; Preview(); DrawImage();
    }
    void AssetLocalisationManagerWidget::Create(int kind)
    {
        if (m_dirty) { Status(tr("Save or revert your draft before creating another entry.")); return; }
        Load({}); m_kind = kind; m_dirty = true; Populate(); Status(tr("New local draft. Complete its fields and save it to your active mod."));
    }
    void AssetLocalisationManagerWidget::MarkDirty()
    {
        if (m_loading || !m_kind) { return; } m_dirty = true;
        m_asset.m_provenance = A(m_provenance->text()); m_asset.m_licence = A(m_licence->text());
        m_asset.m_sourceRights = A(m_rights->currentData().toString()); m_asset.m_redistribution = A(m_distribution->currentData().toString());
        m_entry.m_key = A(m_key->text()); m_entry.m_defaultLanguage = A(m_defaultLanguage->text()); Preview(); Status(tr("Unsaved entry changes."));
    }
    bool AssetLocalisationManagerWidget::Save()
    {
        if (!m_kind) { Status(tr("Select or create an entry first.")); return false; }
        if (m_context != Context()) { Status(tr("Return to the original workspace and mod, or revert the draft.")); return false; }
        // Read controls even when their initial default selection did not emit an edit signal.
        MarkDirty(); AZStd::string id, error; m_saving = true; auto& service = FoundationService::Get();
        const bool saved = m_kind == 1 ? service.SaveProjectAsset(m_asset, A(m_name->text()), A(m_source), m_revision, id, &error)
            : service.SaveLocalisationEntry(m_entry, m_revision, id, &error);
        m_saving = false; if (!saved) { Status(Q(error)); return false; }
        Load(id); RefreshTargets(); Status(tr("Entry saved. Reopening the workspace restores it.")); return true;
    }
    void AssetLocalisationManagerWidget::Revert()
    {
        if (m_dirty && QMessageBox::question(this, tr("Revert entry"), tr("Discard unsaved changes?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { return; }
        Load(m_id); RefreshTargets(); Status(tr("Reloaded the saved entry."));
    }
    void AssetLocalisationManagerWidget::OnFoundationChanged()
    {
        if (m_saving) { return; }
        if (m_dirty) { Status(tr("The workspace, mod or catalog changed. Your draft is preserved; return to its original context to save, or revert.")); return; }
        Load(m_id); RefreshTargets();
    }
    void AssetLocalisationManagerWidget::closeEvent(QCloseEvent* event)
    {
        if (m_dirty)
        {
            const auto answer = QMessageBox::warning(this, tr("Unsaved entry"), tr("Save this entry before closing?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer == QMessageBox::Cancel || (answer == QMessageBox::Save && !Save())) { event->ignore(); return; }
        }
        QWidget::closeEvent(event);
    }
    void AssetLocalisationManagerWidget::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); DrawImage(); }
}
