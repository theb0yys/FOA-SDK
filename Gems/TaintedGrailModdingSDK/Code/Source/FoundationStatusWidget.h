/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationNotificationBus.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
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
        void LoadFormFromWorkspace();
        void ApplyConfiguration();
        void OpenWorkspace();
        void SaveWorkspace();
        void SaveWorkspaceAs();

        QLabel* m_workspaceValue = nullptr;
        QLabel* m_workspaceFileValue = nullptr;
        QLabel* m_profileValue = nullptr;
        QLabel* m_versionValue = nullptr;
        QLabel* m_branchValue = nullptr;
        QLabel* m_runtimeTargetValue = nullptr;
        QLabel* m_unityVersionValue = nullptr;
        QLabel* m_bepInExVersionValue = nullptr;
        QLabel* m_boundaryValue = nullptr;
        QLabel* m_persistenceStatus = nullptr;

        QLineEdit* m_workspaceIdEdit = nullptr;
        QLineEdit* m_workspaceNameEdit = nullptr;
        QLineEdit* m_workspaceRootEdit = nullptr;
        QLineEdit* m_outputPathEdit = nullptr;
        QLineEdit* m_stagingPathEdit = nullptr;
        QLineEdit* m_deploymentPathEdit = nullptr;

        QLineEdit* m_profileIdEdit = nullptr;
        QLineEdit* m_profileNameEdit = nullptr;
        QLineEdit* m_installPathEdit = nullptr;
        QLineEdit* m_gameVersionEdit = nullptr;
        QLineEdit* m_branchEdit = nullptr;
        QComboBox* m_runtimeTargetEdit = nullptr;
        QLineEdit* m_unityVersionEdit = nullptr;
        QLineEdit* m_bepInExVersionEdit = nullptr;
        QLineEdit* m_managedAssembliesPathEdit = nullptr;
        QLineEdit* m_pluginPathEdit = nullptr;
        QLineEdit* m_diagnosticsPathEdit = nullptr;
        QLineEdit* m_extractedDataPathEdit = nullptr;
        QLineEdit* m_dlcScopesEdit = nullptr;

        QTableWidget* m_countsTable = nullptr;
        QTableWidget* m_domainTable = nullptr;
        QTableWidget* m_blockerTable = nullptr;
    };
} // namespace TaintedGrailModdingSDK
