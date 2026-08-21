/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "QuestDefinitionContract.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class QuestStateInspectorWidget final
        : public QWidget
    {
    public:
        explicit QuestStateInspectorWidget(QWidget* parent = nullptr);

    private:
        void BrowseForQuestDefinition();
        void LoadQuestDefinition();
        void ClearInspection();
        void PopulateSummary(
            const QuestDefinitionV1& definition,
            const QuestDefinitionValidationResultV1& result);
        void PopulateIssues(const QuestDefinitionValidationResultV1& result);
        void PopulateBindingRequirements(const QuestDefinitionV1& definition);
        void SetStatus(const QString& message, bool error = false);

        QLineEdit* m_questPathEdit = nullptr;
        QLabel* m_status = nullptr;
        QLabel* m_summary = nullptr;
        QTableWidget* m_issueTable = nullptr;
        QTableWidget* m_bindingTable = nullptr;
    };
} // namespace TaintedGrailModdingSDK
