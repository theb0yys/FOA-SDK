/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AssetBrowserPreviewService.h"
#include "FoundationNotificationBus.h"

#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QThreadPool>
#include <QWidget>
#include <atomic>
#include <memory>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace TaintedGrailModdingSDK
{
    class NativeItemPreviewService;
    class AssetBrowserPreviewWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit AssetBrowserPreviewWidget(QWidget* parent = nullptr, bool itemPreviewOnly = false);
        ~AssetBrowserPreviewWidget() override;
        void SetItemTarget(const QString& recordId, const QString& nativeRefExact);

    private:
        void OnFoundationChanged() override;
        void resizeEvent(QResizeEvent* event) override;
        void ShowItemThumbnail();
        void ScaleItemThumbnail();
        void RefreshProfileContext();
        void AutoFindEvidence();
        void RefreshAssets();
        void LoadPreviewEvidence();
        void ApplyPreviewResult(AZ::Outcome<AssetBrowserPreviewSnapshot, AZStd::string> result);
        void RebuildCategoryFilters();
        void RefreshSubcategoryFilter(bool preserveSelection = false);
        void PopulateTree();
        void ShowSelectedEntry(QTreeWidgetItem* current);
        void RouteSelectedEntry();
        void SetStatus(const QString& message, bool error = false);
        QString ResolveCustomAssetsRoot() const;
        QString FindEvidenceDocument(const QString& documentKind) const;
        static QString FindEvidenceDocument(const AssetBrowserPreviewLoadRequest& request, const QString& documentKind);
        AssetBrowserPreviewLoadRequest BuildRequest() const;

        AssetBrowserPreviewService m_service;
        AssetBrowserPreviewSnapshot m_snapshot;
        AZStd::string m_selectedEntryId;
        bool m_itemPreviewOnly = false;
        QString m_itemRecordId;
        QString m_itemNativeRef;
        QStringList m_profileContext;
        QPixmap m_itemPixmap;
        QLabel* m_itemCaption = nullptr;

        QLabel* m_profileValue = nullptr;
        QLabel* m_statusLabel = nullptr;
        QLineEdit* m_gameInstallEdit = nullptr;
        QLineEdit* m_customAssetsEdit = nullptr;
        QLineEdit* m_searchEdit = nullptr;
        QPushButton* m_refreshButton = nullptr;
        QPushButton* m_loadButton = nullptr;
        NativeItemPreviewService* m_nativePreviewService = nullptr;
        QThreadPool m_previewLoadPool;
        std::shared_ptr<std::atomic_bool> m_loadCancelled;
        unsigned int m_loadGeneration = 0;
        bool m_loading = false;
        bool m_autoRefreshIfEmpty = true;
        QString m_extractedRootPath;
        QString m_paneModelPath;
        QString m_thumbnailEvidencePath;
        QString m_viewportEvidencePath;
        QComboBox* m_categoryFilter = nullptr;
        QComboBox* m_subcategoryFilter = nullptr;
        QTreeWidget* m_assetTree = nullptr;
        QLabel* m_thumbnailLabel = nullptr;
        QLabel* m_identityValue = nullptr;
        QLabel* m_categoryValue = nullptr;
        QLabel* m_fidelityValue = nullptr;
        QLabel* m_routeValue = nullptr;
        QLabel* m_productValue = nullptr;
        QLabel* m_evidenceValue = nullptr;
        QLabel* m_blockerValue = nullptr;
        QPushButton* m_routeButton = nullptr;
        QLabel* m_routeStatus = nullptr;
    };
} // namespace TaintedGrailModdingSDK
