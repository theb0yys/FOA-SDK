/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationNotificationBus.h"

#include <AzCore/std/algorithm.h>

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class CatalogGovernanceWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit CatalogGovernanceWidget(QWidget* parent = nullptr);
        ~CatalogGovernanceWidget() override;

    private:
        void OnFoundationChanged() override;
        void RefreshSubjects();
        void RefreshCurrentSubject();
        void RefreshAxisValues();
        void ApplyGovernanceDecision();
        void ApplyValidationDecision();
        void SetStatus(const QString& message, bool error = false);

        QComboBox* m_subjectKind = nullptr;
        QComboBox* m_subjectId = nullptr;
        QLabel* m_identityValue = nullptr;
        QLabel* m_maturityValue = nullptr;
        QLabel* m_confidenceValue = nullptr;
        QLabel* m_riskValue = nullptr;
        QLabel* m_validationValue = nullptr;
        QLabel* m_stalenessValue = nullptr;
        QLabel* m_allowedValue = nullptr;
        QLabel* m_forbiddenValue = nullptr;
        QLabel* m_supersessionValue = nullptr;

        QComboBox* m_axis = nullptr;
        QComboBox* m_axisValue = nullptr;
        QLineEdit* m_usage = nullptr;
        QLineEdit* m_evidenceIds = nullptr;
        QLineEdit* m_validationIds = nullptr;
        QLineEdit* m_reviewer = nullptr;
        QPlainTextEdit* m_notes = nullptr;

        QComboBox* m_validationState = nullptr;
        QLineEdit* m_validationMethod = nullptr;
        QLineEdit* m_validationEvidenceIds = nullptr;
        QLineEdit* m_validator = nullptr;
        QPlainTextEdit* m_validationNotes = nullptr;

        QTableWidget* m_governanceHistory = nullptr;
        QTableWidget* m_validationHistory = nullptr;
        QLabel* m_status = nullptr;
        bool m_refreshing = false;
    };
} // namespace TaintedGrailModdingSDK
