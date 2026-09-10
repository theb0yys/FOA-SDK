/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "SocietyModels.h"
#include "FoundationNotificationBus.h"
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QCloseEvent;
namespace TaintedGrailModdingSDK
{
    class FactionAuthorityEditorWidget final : public QWidget, private FoundationNotificationBus::Handler
    {
    public:
        explicit FactionAuthorityEditorWidget(QWidget* parent = nullptr);
        ~FactionAuthorityEditorWidget() override;
    private:
        struct LinkWidgets
        {
            const char* kind = nullptr;
            QComboBox* target = nullptr;
            QComboBox* value = nullptr;
            QLineEdit* reference = nullptr;
            QLineEdit* notes = nullptr;
            QTableWidget* table = nullptr;
        };
        void closeEvent(QCloseEvent* event) override;
        void OnFoundationChanged() override;
        void RefreshChoices();
        void Load(const AZStd::string& id);
        void ReadFields();
        void MarkDirty();
        void Preview();
        void RefreshLinks();
        void SelectLink(int index);
        void ChangeLink(int index, bool update);
        void RemoveLink(int index);
        void Create(bool culture);
        void EditCulture();
        void Save();
        void Status(const QString& text, bool error = false);
        bool SameWorkspace() const;
        QComboBox* m_records = nullptr;
        QComboBox* m_culture = nullptr;
        QLineEdit* m_name = nullptr;
        QLineEdit* m_description = nullptr;
        QLineEdit* m_authority = nullptr;
        QLabel* m_summary = nullptr;
        QLabel* m_preview = nullptr;
        QLabel* m_status = nullptr;
        QPushButton* m_save = nullptr;
        QWidget* m_form = nullptr;
        LinkWidgets m_links[3];
        FactionDefinition m_draft;
        AZStd::string m_workspaceId;
        AZStd::string m_profileId;
        AZStd::string m_workspacePath;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        bool m_loading = false;
        bool m_dirty = false;
        bool m_saving = false;
    };
}
