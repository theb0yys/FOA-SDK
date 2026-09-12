/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "EncounterModels.h"
#include "FoundationNotificationBus.h"
#include <QWidget>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class SpawnEncounterEditorWidget final : public QWidget, private FoundationNotificationBus::Handler
    {
    public:
        explicit SpawnEncounterEditorWidget(QWidget* parent = nullptr);
        ~SpawnEncounterEditorWidget() override;
    private:
        void closeEvent(QCloseEvent* event) override;
        void OnFoundationChanged() override;
        void RefreshChoices();
        void Load(const AZStd::string& recordId);
        void MarkDirty();
        void Preview();
        void RefreshEntries();
        void AddEntry();
        void RemoveEntry();
        void Create();
        void Save();
        void Status(const QString& text, bool error = false);
        void ReadFields();
        bool SameWorkspace() const;

        QComboBox* m_records = nullptr;
        QComboBox* m_target = nullptr;
        QComboBox* m_placement = nullptr;
        QComboBox* m_activation = nullptr;
        QLineEdit* m_filter = nullptr;
        QLineEdit* m_name = nullptr;
        QLineEdit* m_placementSubject = nullptr;
        QLineEdit* m_cleanup = nullptr;
        QLineEdit* m_rollback = nullptr;
        QPlainTextEdit* m_conditions = nullptr;
        QSpinBox* m_instances = nullptr;
        QSpinBox* m_populationLimit = nullptr;
        QCheckBox* m_unique = nullptr;
        QTableWidget* m_entries = nullptr;
        QLabel* m_summary = nullptr;
        QLabel* m_preview = nullptr;
        QLabel* m_status = nullptr;
        QWidget* m_form = nullptr;
        QPushButton* m_save = nullptr;
        EncounterDefinition m_draft;
        AZStd::string m_workspaceId;
        AZStd::string m_profileId;
        bool m_loading = false;
        bool m_dirty = false;
        bool m_saving = false;
    };
} // namespace TaintedGrailModdingSDK
