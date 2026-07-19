/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterDeploymentWorkOrderService.h"
#include "FoundationNotificationBus.h"

#include <QWidget>

class QLabel;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class AdapterDeploymentWorkOrderWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit AdapterDeploymentWorkOrderWidget(QWidget* parent = nullptr);
        ~AdapterDeploymentWorkOrderWidget() override;

    private:
        void OnFoundationChanged() override;
        void Refresh();

        AdapterDeploymentWorkOrderService m_service;
        QLabel* m_summary = nullptr;
        QTableWidget* m_table = nullptr;
    };
} // namespace TaintedGrailModdingSDK
