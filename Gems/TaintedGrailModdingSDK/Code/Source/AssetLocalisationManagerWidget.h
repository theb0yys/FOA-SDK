/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "AssetLocalisationModels.h"
#include "FoundationNotificationBus.h"
#include <QWidget>
#include <QImage>
class QLabel; class QLineEdit; class QComboBox; class QTableWidget; class QStackedWidget;
class QPlainTextEdit; class QCloseEvent; class QResizeEvent;
namespace TaintedGrailModdingSDK
{
    class AssetLocalisationManagerWidget final : public QWidget, private FoundationNotificationBus::Handler
    {
    public:
        explicit AssetLocalisationManagerWidget(QWidget* parent = nullptr);
        ~AssetLocalisationManagerWidget() override;
    private:
        void OnFoundationChanged() override;
        void closeEvent(QCloseEvent*) override;
        void resizeEvent(QResizeEvent*) override;
        QString Context() const;
        void Status(const QString&);
        void Refresh();
        void Load(AZStd::string);
        void Create(int);
        void Populate();
        void MarkDirty();
        bool Save();
        void Revert();
        void ChooseImage();
        void EditVariant(bool);
        void RemoveVariant();
        void Preview();
        void DrawImage();
        void RefreshTargets();
        void SelectTarget();
        void SelectSlot();
        void PreviewAssignment();
        void Assign(bool clear);
        int m_kind = 0;
        bool m_loading = false, m_dirty = false, m_saving = false;
        QString m_context, m_source;
        AZStd::string m_id, m_revision, m_bindingRevision;
        ProjectAssetProfile m_asset;
        LocalisationEntry m_entry;
        QImage m_image, m_assignmentImage;
        QComboBox *m_records = nullptr, *m_rights = nullptr, *m_distribution = nullptr;
        QComboBox *m_target = nullptr, *m_slot = nullptr, *m_value = nullptr;
        QLineEdit *m_search = nullptr, *m_name = nullptr, *m_provenance = nullptr, *m_licence = nullptr;
        QLineEdit *m_key = nullptr, *m_defaultLanguage = nullptr, *m_language = nullptr, *m_targetSearch = nullptr;
        QLabel *m_summary = nullptr, *m_status = nullptr, *m_imageLabel = nullptr, *m_imageDetails = nullptr;
        QLabel *m_fallback = nullptr, *m_assignmentPreview = nullptr, *m_bindingStatus = nullptr;
        QPlainTextEdit* m_textPreview = nullptr;
        QTableWidget* m_variants = nullptr;
        QStackedWidget* m_pages = nullptr;
    };
}
