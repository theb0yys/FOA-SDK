/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <QWidget>

class QLabel;
class QHideEvent;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class GameConnectionService;
    class GameConnectionWidget final : public QWidget
    {
    public:
        explicit GameConnectionWidget(QWidget* parent = nullptr);
        ~GameConnectionWidget() override;

    protected:
        void hideEvent(QHideEvent* event) override;

    private:
        void RefreshView();
        GameConnectionService* m_service = nullptr;
        QSpinBox* m_port = nullptr;
        QLineEdit* m_key = nullptr;
        QLineEdit* m_version = nullptr;
        QPushButton* m_connect = nullptr;
        QPushButton* m_refresh = nullptr;
        QPushButton* m_disconnect = nullptr;
        QPushButton* m_getPosition = nullptr;
        QLabel* m_position = nullptr;
        QPushButton* m_getVitals = nullptr;
        QLabel* m_vitals = nullptr;
        QLineEdit* m_composition = nullptr;
        QPushButton* m_browseComposition = nullptr;
        QPushButton* m_previewEncounter = nullptr;
        QLabel* m_encounter = nullptr;
        QLabel* m_status = nullptr;
        QLabel* m_details = nullptr;
        QTableWidget* m_services = nullptr;
    };
}
