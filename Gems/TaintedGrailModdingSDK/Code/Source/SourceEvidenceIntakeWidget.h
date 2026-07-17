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
class QPlainTextEdit;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class SourceEvidenceIntakeWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit SourceEvidenceIntakeWidget(QWidget* parent = nullptr);
        ~SourceEvidenceIntakeWidget() override;

    private:
        void OnFoundationChanged() override;
        void Refresh();
        void BrowseForSource();
        void ImportSource();
        void ReloadWorkspaceSources();
        void PopulateImporterContracts();
        void SetStatus(const QString& message, bool error = false);

        QLabel* m_profileValue = nullptr;
        QLabel* m_buildValue = nullptr;
        QLabel* m_runtimeValue = nullptr;
        QLabel* m_statusLabel = nullptr;
        QLineEdit* m_inputPathEdit = nullptr;
        QComboBox* m_sourceKindCombo = nullptr;
        QComboBox* m_importerCombo = nullptr;
        QLineEdit* m_titleEdit = nullptr;
        QLineEdit* m_toolNameEdit = nullptr;
        QLineEdit* m_toolVersionEdit = nullptr;
        QLineEdit* m_capturedAtEdit = nullptr;
        QPlainTextEdit* m_limitationsEdit = nullptr;
        QTableWidget* m_sourcesTable = nullptr;
        QTableWidget* m_evidenceTable = nullptr;
        QTableWidget* m_issuesTable = nullptr;
    };
} // namespace TaintedGrailModdingSDK
