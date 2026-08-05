/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationNotificationBus.h"

#include <AzCore/std/containers/vector.h>

#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace AzToolsFramework::AssetBrowser
{
    class PreviewerFrame;
}

namespace TaintedGrailModdingSDK
{
    //! Explicit, editor-only visual selection step for the Item and Recipe Editor.
    class ItemVisualSelectorWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit ItemVisualSelectorWidget(QWidget* parent = nullptr);
        ~ItemVisualSelectorWidget() override;

    private:
        struct PreviewEntry
        {
            QString m_paneEntryId;
            QString m_displayName;
            QString m_entryKind;
            QString m_previewAvailability;
            QString m_productKind;
            QString m_productAssetId;
            QString m_productCachePath;
            QString m_primarySourceAssetRecordId;
            QString m_productEvidenceId;
            QString m_issueText;
            bool m_previewRenderVerified = false;
        };

        void OnFoundationChanged() override;

        void RefreshTargetChoices();
        void RefreshRecipeItemChoices();
        void RefreshBindingSummary();
        void ChoosePreviewModel();
        void ReloadPreviewModel();
        bool LoadPreviewModel(const QString& path);
        void ClearLoadedModel(const QString& reason = {});
        bool LoadedModelMatchesActiveProfile() const;
        bool LoadedModelFileMatches() const;
        void ApplySearchFilter();
        void RefreshSelection();
        void ApplySelectionAsIcon();
        void ApplySelectionAsAsset();
        void ApplySelection(bool iconBinding);
        QString ResolveExtractedDataRoot() const;
        QString ResolveBindingItemRecordId() const;
        const PreviewEntry* GetSelectedEntry() const;
        void SetStatus(const QString& message, bool error = false);

        QComboBox* m_targetRecord = nullptr;
        QLabel* m_recipeItemLabel = nullptr;
        QComboBox* m_recipeItemRecord = nullptr;
        QLineEdit* m_modelPath = nullptr;
        QPushButton* m_reloadModel = nullptr;
        QLineEdit* m_search = nullptr;
        QTableWidget* m_entryTable = nullptr;
        AzToolsFramework::AssetBrowser::PreviewerFrame* m_previewer = nullptr;
        QLabel* m_modelInfo = nullptr;
        QLabel* m_selectionInfo = nullptr;
        QLabel* m_bindingInfo = nullptr;
        QPushButton* m_applyIcon = nullptr;
        QPushButton* m_applyAsset = nullptr;
        QLabel* m_status = nullptr;

        AZStd::vector<PreviewEntry> m_entries;
        QString m_loadedProfileId;
        QString m_loadedGameVersion;
        QString m_loadedBranch;
        QString m_loadedRuntimeTarget;
        QString m_loadedModelSha256;
        bool m_refreshing = false;
    };
} // namespace TaintedGrailModdingSDK
