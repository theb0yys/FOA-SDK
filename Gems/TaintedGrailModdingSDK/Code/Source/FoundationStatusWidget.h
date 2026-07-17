/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <QWidget>

class QLabel;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class FoundationStatusWidget final
        : public QWidget
    {
    public:
        explicit FoundationStatusWidget(QWidget* parent = nullptr);

        void Refresh();

    private:
        QLabel* m_workspaceValue = nullptr;
        QLabel* m_profileValue = nullptr;
        QLabel* m_versionValue = nullptr;
        QLabel* m_branchValue = nullptr;
        QLabel* m_boundaryValue = nullptr;
        QTableWidget* m_countsTable = nullptr;
        QTableWidget* m_domainTable = nullptr;
        QTableWidget* m_blockerTable = nullptr;
    };
} // namespace TaintedGrailModdingSDK
