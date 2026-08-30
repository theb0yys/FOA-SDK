/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"
#include "FoundationNotificationBus.h"

#include <AzCore/std/algorithm.h>

#include <QWidget>

class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class FoundationStatusWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit FoundationStatusWidget(QWidget* parent = nullptr);
        ~FoundationStatusWidget() override;

        void Refresh();

    private:
        void OnFoundationChanged() override;
        void DetectAndApply(const AZStd::string& explicitInstallPath = {});
        void LocateGame();
        void OpenWorkspace();
        bool EnsureWorkspaceDirectories(const WorkspaceModel& workspace);
        AZStd::string DefaultWorkspaceFilePath(const WorkspaceModel& workspace) const;
        bool PersistDetectedWorkspace(const WorkspaceModel& workspace);
        void UpdateAdvancedDetails();

        QLabel* m_overallStatus = nullptr;
        QLabel* m_sdkStatus = nullptr;
        QLabel* m_gameStatus = nullptr;
        QLabel* m_versionValue = nullptr;
        QLabel* m_runtimeTargetValue = nullptr;
        QLabel* m_workspaceValue = nullptr;
        QLabel* m_authoringStatus = nullptr;
        QLabel* m_persistenceStatus = nullptr;
        QLabel* m_boundaryValue = nullptr;
        QPlainTextEdit* m_advancedDetails = nullptr;
        QGroupBox* m_advancedGroup = nullptr;
        QPushButton* m_locateGameButton = nullptr;
        QPushButton* m_advancedToggleButton = nullptr;
        AZStd::string m_workspaceFilePath;
        AZStd::vector<AZStd::string> m_detectionNotes;

        QTableWidget* m_countsTable = nullptr;
        QTableWidget* m_domainTable = nullptr;
        QTableWidget* m_blockerTable = nullptr;
    };
} // namespace TaintedGrailModdingSDK
