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
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace TaintedGrailModdingSDK
{
    class AssetBrowserPreviewWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit AssetBrowserPreviewWidget(QWidget* parent = nullptr);
        ~AssetBrowserPreviewWidget() override;

    private:
        void OnFoundationChanged() override;
        void RefreshProfileContext();
        void AutoFindEvidence();
        void LoadPreviewEvidence();
        void PopulateTree();
        void ShowSelectedEntry(QTreeWidgetItem* current);
        void RouteSelectedEntry();
        void SetStatus(const QString& message, bool error = false);
        QString ResolveCustomAssetsRoot() const;
        QString FindEvidenceDocument(const QString& documentKind) const;
        AssetBrowserPreviewLoadRequest BuildRequest() const;

        AssetBrowserPreviewService m_service;
        AssetBrowserPreviewSnapshot m_snapshot;
        AZStd::string m_selectedEntryId;

        QLabel* m_profileValue = nullptr;
        QLabel* m_statusLabel = nullptr;
        QLineEdit* m_gameInstallEdit = nullptr;
        QLineEdit* m_customAssetsEdit = nullptr;
        QString m_extractedRootPath;
        QString m_paneModelPath;
        QString m_thumbnailEvidencePath;
        QString m_viewportEvidencePath;
        QComboBox* m_categoryFilter = nullptr;
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
