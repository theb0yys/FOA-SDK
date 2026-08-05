/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "ActorAppearancePreviewService.h"
#include "ActorTroopEditorWidget.h"
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
    class ActorAppearancePreviewWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit ActorAppearancePreviewWidget(QWidget* parent = nullptr);
        ~ActorAppearancePreviewWidget() override;

    private:
        struct PreviewEntry
        {
            QString m_entryId;
            QString m_displayName;
            QString m_entryKind;
            QString m_productKind;
            QString m_assetId;
            QString m_cachePath;
            QString m_sourceSubject;
            QString m_evidenceId;
            QString m_issues;
        };

        void OnFoundationChanged() override;
        void RefreshActors();
        void RefreshActorView();
        void ChooseModel();
        void ReloadModel();
        bool LoadModel(const QString& path);
        void ClearModel(const QString& reason = {});
        bool ModelStillValid() const;
        void FilterEntries();
        void RefreshSelection();
        void BindSelection(bool portrait);
        const PreviewEntry* SelectedEntry() const;
        const PreviewEntry* FindEntryByAssetId(const AZStd::string& assetId) const;
        QString ExtractedRoot() const;
        void SetStatus(const QString& text, bool error = false);

        QComboBox* m_actor = nullptr;
        QLabel* m_actorSummary = nullptr;
        QLabel* m_fidelity = nullptr;
        QLineEdit* m_modelPath = nullptr;
        QPushButton* m_reload = nullptr;
        QLineEdit* m_search = nullptr;
        QTableWidget* m_products = nullptr;
        QTableWidget* m_equipment = nullptr;
        AzToolsFramework::AssetBrowser::PreviewerFrame* m_previewer = nullptr;
        QPushButton* m_bindPortrait = nullptr;
        QPushButton* m_bindModel = nullptr;
        QLabel* m_status = nullptr;

        AZStd::vector<PreviewEntry> m_entries;
        QString m_profileId;
        QString m_gameVersion;
        QString m_branch;
        QString m_runtimeTarget;
        QString m_modelSha256;
        bool m_refreshing = false;
    };

    class ActorTroopEditorWithAppearanceWidget final : public ActorTroopEditorWidget
    {
    public:
        explicit ActorTroopEditorWithAppearanceWidget(QWidget* parent = nullptr);
    };
} // namespace TaintedGrailModdingSDK
