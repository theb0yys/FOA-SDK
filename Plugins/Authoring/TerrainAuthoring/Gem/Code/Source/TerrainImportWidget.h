/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include <FoundationNotificationBus.h>
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>
class QLabel;
class QListWidget;
class QPushButton;
class QProgressBar;
class QTimer;
namespace TerrainAuthoring
{
    class TerrainImportWidget final : public QWidget,
        private TaintedGrailModdingSDK::FoundationNotificationBus::Handler
    {
    public:
        explicit TerrainImportWidget(QWidget* parent = nullptr);
        ~TerrainImportWidget() override;
    protected:
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dropEvent(QDropEvent* event) override;
    private:
        void OnFoundationChanged() override;
        void Send(const QJsonObject& command);
        void ImportFile(const QString& source);
        QLabel* m_context = nullptr;
        QLabel* m_status = nullptr;
        QLabel* m_preview = nullptr;
        QLabel* m_details = nullptr;
        QListWidget* m_recent = nullptr;
        QPushButton* m_import = nullptr;
        QPushButton* m_vanilla = nullptr;
        QJsonArray m_campaigns;
        QPushButton* m_refresh = nullptr;
        QPushButton* m_open = nullptr;
        QPushButton* m_cancel = nullptr;
        QProgressBar* m_progress = nullptr;
        QTimer* m_timer = nullptr;
        bool m_busy = false;
    };
}
