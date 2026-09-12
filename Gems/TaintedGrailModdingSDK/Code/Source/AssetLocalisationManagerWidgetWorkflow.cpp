/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetLocalisationManagerWidget.h"
#include "AssetLocalisationService.h"
#include "FoundationService.h"
#include "ProjectImageService.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString Q(const AZStd::string& s) { return QString::fromUtf8(s.data(), static_cast<int>(s.size())); }
        AZStd::string A(const QString& s) { const auto b = s.toUtf8(); return {b.constData(), static_cast<size_t>(b.size())}; }
    }
    void AssetLocalisationManagerWidget::ChooseImage()
    {
        const auto context = Context();
        const auto file = QFileDialog::getOpenFileName(this, tr("Choose your image"), {}, tr("Images (*.png *.jpg *.jpeg);;All files (*)"));
        if (file.isEmpty()) { return; }
        if (context != Context() || m_context != Context()) { Status(tr("The workspace or mod changed. Return to the original context.")); return; }
        const auto& s = FoundationService::Get(); const auto result = ProjectImageService::ReadSource(A(file), s.GetWorkspace(), s.GetWorkspaceRootPath());
        if (!result.IsSuccess()) { Status(Q(result.GetError())); return; }
        m_source = file; m_image = result.GetValue().m_image;
        const auto& p = result.GetValue().m_metadata;
        m_imageDetails->setText(tr("%1 x %2 pixels | %3 bytes | Ready to save").arg(p.m_width).arg(p.m_height).arg(p.m_byteSize));
        MarkDirty(); DrawImage();
    }
    void AssetLocalisationManagerWidget::EditVariant(bool create)
    {
        const int row = m_variants->currentRow();
        if ((!create && (row < 0 || row >= static_cast<int>(m_entry.m_variants.size()))) || (create && m_entry.m_variants.size() >= 32))
        { Status(tr("Select a translation to edit. An entry supports up to 32 languages.")); return; }
        QDialog dialog(this); dialog.setObjectName("assetVariantDialog"); dialog.setWindowTitle(create ? tr("Add translation") : tr("Edit translation"));
        auto* layout = new QVBoxLayout(&dialog); auto* form = new QFormLayout();
        auto* language = new QLineEdit(&dialog); language->setObjectName("assetVariantLanguage"); language->setMaxLength(16);
        auto* text = new QPlainTextEdit(&dialog); text->setObjectName("assetVariantText");
        if (!create) { language->setText(Q(m_entry.m_variants[row].m_language)); text->setPlainText(Q(m_entry.m_variants[row].m_text)); }
        form->addRow(tr("Language (en, fr, en-gb)"), language); layout->addLayout(form); layout->addWidget(text);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog); layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.resize(520, 360); const auto context = Context();
        if (dialog.exec() != QDialog::Accepted) { return; }
        if (context != Context() || m_context != Context()) { Status(tr("The workspace or mod changed. This translation was not applied.")); return; }
        LocalisationVariant variant; variant.m_language = A(language->text()); variant.m_text = A(text->toPlainText());
        if (!AssetLocalisationService::IsLanguage(variant.m_language) || !AssetLocalisationService::IsText(variant.m_text, 8192, true))
        { Status(tr("Use a lowercase language such as en or fr, and nonempty text up to 8192 UTF-8 bytes.")); return; }
        for (size_t i = 0; i < m_entry.m_variants.size(); ++i)
        {
            if ((create || static_cast<int>(i) != row) && m_entry.m_variants[i].m_language == variant.m_language)
            { Status(tr("That language already exists. Edit its translation instead.")); return; }
        }
        const int destination = create ? m_variants->rowCount() : row;
        if (create) { m_entry.m_variants.push_back(variant); m_variants->insertRow(destination); }
        else { m_entry.m_variants[row] = variant; }
        m_variants->setItem(destination, 0, new QTableWidgetItem(Q(variant.m_language))); m_variants->setItem(destination, 1, new QTableWidgetItem(Q(variant.m_text)));
        MarkDirty();
    }
    void AssetLocalisationManagerWidget::RemoveVariant()
    {
        const int row = m_variants->currentRow();
        if (row < 0 || row >= static_cast<int>(m_entry.m_variants.size())) { Status(tr("Select a translation to remove.")); return; }
        m_entry.m_variants.erase(m_entry.m_variants.begin() + row); m_variants->removeRow(row); MarkDirty();
    }
    void AssetLocalisationManagerWidget::Preview()
    {
        auto entry = m_entry;
        // A draft has no persisted identity yet; preview does not create one.
        if (entry.m_recordId.empty()) { entry.m_recordId = "text.preview.draft"; }
        const auto result = AssetLocalisationService::Resolve(entry, A(m_language->text()));
        m_textPreview->setPlainText(Q(result.m_text));
        m_fallback->setText(!result.IsSuccess() ? Q(result.m_error) : result.m_usedFallback
            ? tr("No %1 translation. Showing the default language: %2.").arg(m_language->text(), Q(result.m_language))
            : tr("Showing %1.").arg(Q(result.m_language)));
    }
    void AssetLocalisationManagerWidget::DrawImage()
    {
        m_imageLabel->clear();
        if (m_image.isNull()) { m_imageLabel->setText(tr("No image preview. Choose an image or repair its saved file.")); }
        else { m_imageLabel->setPixmap(QPixmap::fromImage(m_image).scaled(m_imageLabel->size().boundedTo(QSize(600, 340)), Qt::KeepAspectRatio, Qt::SmoothTransformation)); }
        if (!m_assignmentImage.isNull()) { m_assignmentPreview->setPixmap(QPixmap::fromImage(m_assignmentImage).scaled(m_assignmentPreview->size().boundedTo(QSize(800, 500)), Qt::KeepAspectRatio, Qt::SmoothTransformation)); }
    }
    void AssetLocalisationManagerWidget::RefreshTargets()
    {
        const auto previous = m_target->currentData(); const QSignalBlocker b(m_target);
        m_target->clear(); m_target->addItem(tr("Choose an item, actor or quest"), "");
        const auto& s = FoundationService::Get(); const auto* pack = s.GetActivePack();
        for (const auto& r : s.GetCatalog().GetRecords())
        {
            if (!pack || (r.IsSynthetic() ? r.m_ownerPackId != pack->m_packId : r.m_identityKind != "native")) { continue; }
            if (!((r.m_domain == "economy" && r.m_recordKind == "item") || (r.m_domain == "population" && r.m_recordKind == "actor") || (r.m_domain == "narrative" && r.m_recordKind == "quest"))) { continue; }
            const auto title = Q(r.m_displayName) + " (" + Q(r.m_recordKind) + ") [" + Q(r.m_recordId).right(8) + "]";
            if (Q(r.m_recordId) == previous.toString() || title.contains(m_targetSearch->text(), Qt::CaseInsensitive)) { m_target->addItem(title, Q(r.m_recordId)); }
        }
        m_target->setCurrentIndex(qMax(0, m_target->findData(previous))); SelectTarget();
    }
    void AssetLocalisationManagerWidget::SelectTarget()
    {
        const auto previous = m_slot->currentData(); const QSignalBlocker b(m_slot); m_slot->clear();
        const auto* r = FoundationService::Get().GetCatalog().FindByRecordId(A(m_target->currentData().toString()));
        if (r)
        {
            if (r->m_recordKind == "item") { m_slot->addItem(tr("Icon"), "icon"); }
            if (r->m_recordKind == "actor") { m_slot->addItem(tr("Portrait"), "portrait"); }
            m_slot->addItem(tr("Name"), "name"); m_slot->addItem(tr("Description"), "description");
        }
        m_slot->setCurrentIndex(qMax(0, m_slot->findData(previous))); SelectSlot();
    }
    void AssetLocalisationManagerWidget::SelectSlot()
    {
        const QSignalBlocker b(m_value); m_value->clear(); m_value->addItem(tr("Choose an entry"), ""); m_bindingRevision.clear();
        const auto& s = FoundationService::Get(); const auto* pack = s.GetActivePack(); const auto& catalog = s.GetCatalog();
        const auto slot = A(m_slot->currentData().toString()); const bool image = slot == "icon" || slot == "portrait";
        for (const auto& r : catalog.GetRecords())
        {
            if (pack && r.m_ownerPackId == pack->m_packId && (image ? catalog.FindProjectAsset(r.m_recordId) != nullptr : catalog.FindLocalisationEntry(r.m_recordId) != nullptr))
            { m_value->addItem(Q(r.m_displayName), Q(r.m_recordId)); }
        }
        const auto* binding = pack ? catalog.FindPresentationBinding(pack->m_packId, A(m_target->currentData().toString()), slot) : nullptr;
        if (binding)
        {
            m_bindingRevision = AssetLocalisationService::Revision(*binding);
            m_value->setCurrentIndex(qMax(0, m_value->findData(Q(binding->m_valueRecordId))));
        }
        m_bindingStatus->setText(binding && !binding->m_valueRecordId.empty() ? tr("Saved assignment loaded. Choose another entry to replace it.") : tr("No saved assignment for this slot."));
        PreviewAssignment();
    }
    void AssetLocalisationManagerWidget::PreviewAssignment()
    {
        m_assignmentImage = {}; m_assignmentPreview->clear();
        const auto& s = FoundationService::Get(); const auto id = A(m_value->currentData().toString());
        if (id.empty()) { m_assignmentPreview->setText(tr("Choose an image or translation to preview.")); return; }
        if (s.GetCatalog().FindProjectAsset(id))
        {
            AZStd::string error;
            if (!s.ReadProjectAssetImage(id, m_assignmentImage, &error)) { m_assignmentPreview->setText(Q(error)); return; }
            DrawImage(); return;
        }
        if (const auto* entry = s.GetCatalog().FindLocalisationEntry(id))
        {
            const auto result = AssetLocalisationService::Resolve(*entry, A(m_language->text()));
            m_assignmentPreview->setText(result.IsSuccess() ? Q(result.m_text) : Q(result.m_error));
            m_bindingStatus->setText(!result.IsSuccess() ? Q(result.m_error) : result.m_usedFallback
                ? tr("No %1 translation. Preview uses default language %2.").arg(m_language->text(), Q(result.m_language))
                : tr("Preview in %1. Save assignment to keep the selected entry.").arg(Q(result.m_language)));
        }
    }
    void AssetLocalisationManagerWidget::Assign(bool clear)
    {
        if (m_dirty) { Status(tr("Save or revert the entry draft before changing an assignment.")); return; }
        if (m_context != Context()) { Status(tr("The workspace or mod changed. Select the target again.")); return; }
        if (!clear && m_value->currentData().toString().isEmpty()) { Status(tr("Choose an image or translation to assign.")); return; }
        AZStd::string error; m_saving = true;
        const bool saved = FoundationService::Get().SavePresentationBinding(A(m_target->currentData().toString()), A(m_slot->currentData().toString()),
            clear ? AZStd::string{} : A(m_value->currentData().toString()), m_bindingRevision, &error);
        m_saving = false; if (!saved) { Status(Q(error)); return; }
        SelectSlot(); Status(clear ? tr("Assignment cleared and saved.") : tr("Assignment saved for the active mod."));
    }
}
