/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationNotificationBus.h"

#include <QWidget>

class QGroupBox;
class QLabel;
class QPushButton;

namespace TaintedGrailModdingSDK
{
    class DevelopmentHubWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit DevelopmentHubWidget(QWidget* parent = nullptr);
        ~DevelopmentHubWidget() override;

        void Refresh();

    private:
        void OnFoundationChanged() override;

        QLabel* m_statusHeadline = nullptr;
        QLabel* m_setupValue = nullptr;
        QLabel* m_gameValue = nullptr;
        QLabel* m_packValue = nullptr;
        QLabel* m_blockersValue = nullptr;
        QLabel* m_primaryHint = nullptr;
        QPushButton* m_setupButton = nullptr;
        QPushButton* m_packButton = nullptr;
        QPushButton* m_advancedToggleButton = nullptr;
        QGroupBox* m_authoringGroup = nullptr;
        QGroupBox* m_advancedGroup = nullptr;
    };
} // namespace TaintedGrailModdingSDK
