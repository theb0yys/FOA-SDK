/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"
#include "FoundationNotificationBus.h"
#include "PackDraftRecoveryService.h"

#include <QHash>
#include <QWidget>

class QCloseEvent;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QThread;
class QTimer;

namespace TaintedGrailModdingSDK
{
    class PackManagerWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit PackManagerWidget(QWidget* parent = nullptr);
        ~PackManagerWidget() override;

    protected:
        void closeEvent(QCloseEvent* event) override;

    private:
        void OnFoundationChanged() override;
        bool CanChangeWorkspace(const FoundationService& service) override;
        void OnWorkspaceChanged(const FoundationService& service) override;

        void InitializeRecovery();
        void StartRecoveryForWorkspace();
        void SetRecoveryPending(bool pending);
        void ScheduleRecovery();
        void CheckpointRecovery();
        void RestoreRecovery();
        bool RetireRecovery();
        void StopRecovery();
        PackDraftRecovery CaptureRecovery() const;

        PackManifest BuildPackFromForm() const;
        void PopulateFromPack(const PackManifest& pack);
        void ClearFormForNewPack();
        void UpdateGeneratedIdentity();
        void UpdateSummary();
        void UpdateDraftField(QWidget* field, const QString& value);
        void ResetDraftBaseline();
        void UpdateDraftStatus();
        bool ConfirmDraftReplacement(const QString& action);
        void RefreshWorkspaceMods(const QString& selectedPath = {});
        void OpenSelectedPack();
        void SetStatus(const QString& message, bool error = false);
        bool SavePack();
        QString CanonicalPackFilePath(const PackManifest& pack) const;
        bool IsInsideWorkspace(const QString& filePath) const;

        QLineEdit* m_packIdEdit = nullptr;
        QLineEdit* m_displayNameEdit = nullptr;
        QLineEdit* m_ownerIdEdit = nullptr;
        QLineEdit* m_versionEdit = nullptr;
        QLineEdit* m_targetGameVersionEdit = nullptr;
        QLineEdit* m_targetBranchEdit = nullptr;
        QPlainTextEdit* m_compatibleGameVersionsEdit = nullptr;
        QLineEdit* m_coreVersionEdit = nullptr;
        QLineEdit* m_adapterVersionEdit = nullptr;
        QPlainTextEdit* m_dlcScopesEdit = nullptr;
        QPlainTextEdit* m_dependenciesEdit = nullptr;
        QPlainTextEdit* m_requiredModsEdit = nullptr;
        QPlainTextEdit* m_incompatibilitiesEdit = nullptr;
        QComboBox* m_saveImpactCombo = nullptr;
        QPlainTextEdit* m_contentDefinitionsEdit = nullptr;
        QPlainTextEdit* m_assetPathsEdit = nullptr;
        QPlainTextEdit* m_localisationPathsEdit = nullptr;
        QLineEdit* m_buildConfigurationEdit = nullptr;
        QComboBox* m_releaseChannelCombo = nullptr;
        QComboBox* m_workspaceModsCombo = nullptr;
        QLabel* m_activePackValue = nullptr;
        QLabel* m_manifestPathValue = nullptr;
        QLabel* m_workspaceModsHint = nullptr;
        QLabel* m_statusLabel = nullptr;
        QLabel* m_draftStatusLabel = nullptr;
        QGroupBox* m_advancedGroup = nullptr;
        QPushButton* m_advancedToggleButton = nullptr;
        QPushButton* m_openSelectedButton = nullptr;
        QHash<QWidget*, QString> m_formValues;
        QHash<QWidget*, QString> m_savedFormValues;
        bool m_isNewPack = true;
        QMap<QString, QWidget*> m_recoveryFields;
        std::shared_ptr<PackDraftRecoveryService> m_recoveryStore;
        PackDraftRecoveryRead m_recoveryRead;
        QThread* m_recoveryThread = nullptr;
        QObject* m_recoveryWorker = nullptr;
        QTimer* m_recoveryTimer = nullptr;
        QLabel* m_recoveryStatus = nullptr;
        QWidget* m_recoveryPanel = nullptr;
        QPushButton* m_restoreRecoveryButton = nullptr;
        QPushButton* m_discardRecoveryButton = nullptr;
        QPushButton* m_retryRecoveryButton = nullptr;
        QPushButton* m_newButton = nullptr;
        QPushButton* m_saveButton = nullptr;
        quint64 m_recoveryGeneration = 0;
        bool m_recoveryPending = false;
        bool m_recoverySuppressed = false;
        bool m_recoveryStoreReady = false;
        bool m_recoveryWriteInFlight = false;
        bool m_recoveryClosing = false;
    };
} // namespace TaintedGrailModdingSDK
