/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "ModPackageService.h"
#include "FoundationNotificationBus.h"
#include <QWidget>
#include <thread>
class QLabel; class QPushButton; class QTableWidget; class QLineEdit; class QProgressBar; class QCloseEvent;
namespace TaintedGrailModdingSDK
{
    class ModPackageWidget final : public QWidget, private FoundationNotificationBus::Handler
    {
    public:
        explicit ModPackageWidget(QWidget* parent=nullptr);
        ~ModPackageWidget() override;
    private:
        void OnFoundationChanged() override;
        void Start(int operation,const QString& output = {});
        void UpdateActions();
        void ShowPreview(const ModPackagePreview&);
        ModPackageContext Context() const;
        QLabel* m_status=nullptr;
        QTableWidget* m_inventory=nullptr;
        QLineEdit *m_archive=nullptr,*m_destination=nullptr;
        QPushButton *m_previewButton=nullptr,*m_exportButton=nullptr,*m_inspectButton=nullptr,*m_importButton=nullptr,*m_openButton=nullptr,*m_cancelButton=nullptr;
        QProgressBar* m_progress=nullptr;
        ModPackagePreview m_preview,m_inspected;
        QString m_inspectedPath,m_createdWorkspace,m_createdPack;
        bool m_busy=false,m_contextChanged=false;
        std::shared_ptr<std::atomic_bool> m_cancel;
        std::thread m_worker;
    };
}
