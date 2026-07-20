/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationNotificationBus.h"

#include <QWidget>

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

        QLabel* m_workspaceValue = nullptr;
        QLabel* m_profileValue = nullptr;
        QLabel* m_packValue = nullptr;
        QLabel* m_validationValue = nullptr;
        QLabel* m_blockersValue = nullptr;
        QLabel* m_dirtyValue = nullptr;
        QLabel* m_continueWorkspaceHint = nullptr;
        QPushButton* m_continuePackButton = nullptr;
        QLabel* m_continuePackHint = nullptr;
    };
} // namespace TaintedGrailModdingSDK
