/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterBuildManifestService.h"
#include "AdapterWorkOrderPlanningService.h"
#include "FoundationNotificationBus.h"

#include <QWidget>

class QLabel;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class AdapterBuildManifestWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit AdapterBuildManifestWidget(QWidget* parent = nullptr);
        ~AdapterBuildManifestWidget() override;

    private:
        void OnFoundationChanged() override;
        void Refresh();

        QLabel* m_summary = nullptr;
        QTableWidget* m_table = nullptr;
    };
} // namespace TaintedGrailModdingSDK
