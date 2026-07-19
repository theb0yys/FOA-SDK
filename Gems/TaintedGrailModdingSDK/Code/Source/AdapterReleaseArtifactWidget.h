/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterReleaseArtifactContracts.h"
#include "FoundationNotificationBus.h"

#include <QWidget>

class QLabel;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class AdapterReleaseArtifactWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit AdapterReleaseArtifactWidget(QWidget* parent = nullptr);
        ~AdapterReleaseArtifactWidget() override;

    private:
        void OnFoundationChanged() override;
        void Refresh();

        QLabel* m_summary = nullptr;
        QTableWidget* m_table = nullptr;
    };
} // namespace TaintedGrailModdingSDK
