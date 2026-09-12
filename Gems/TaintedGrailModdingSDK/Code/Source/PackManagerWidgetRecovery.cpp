/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PackManagerWidget.h"

#include "FoundationService.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

namespace TaintedGrailModdingSDK
{
    void PackManagerWidget::InitializeRecovery()
    {
        m_recoveryFields = {
            { "displayName", m_displayNameEdit }, { "owner", m_ownerIdEdit }, { "version", m_versionEdit },
            { "targetGameVersion", m_targetGameVersionEdit }, { "targetBranch", m_targetBranchEdit },
            { "coreVersion", m_coreVersionEdit }, { "adapterVersion", m_adapterVersionEdit },
            { "buildConfiguration", m_buildConfigurationEdit }, { "compatibleGameVersions", m_compatibleGameVersionsEdit },
            { "dlcScopes", m_dlcScopesEdit }, { "dependencies", m_dependenciesEdit }, { "requiredMods", m_requiredModsEdit },
            { "incompatibilities", m_incompatibilitiesEdit }, { "contentDefinitions", m_contentDefinitionsEdit },
            { "assetPaths", m_assetPathsEdit }, { "localisationPaths", m_localisationPathsEdit },
            { "saveImpact", m_saveImpactCombo }, { "releaseChannel", m_releaseChannelCombo }
        };
        for (auto it = m_recoveryFields.cbegin(); it != m_recoveryFields.cend(); ++it)
        {
            it.value()->setProperty("recoveryField", it.key());
        }
        m_recoveryStatus = new QLabel(this);
        m_recoveryStatus->setObjectName(QStringLiteral("packRecoveryStatus"));
        m_recoveryStatus->setWordWrap(true);
        m_recoveryPanel = new QWidget(this);
        m_recoveryPanel->setObjectName(QStringLiteral("packRecoveryPrompt"));
        auto* buttons = new QHBoxLayout(m_recoveryPanel);
        buttons->setContentsMargins(0, 0, 0, 0);
        m_restoreRecoveryButton = new QPushButton(tr("Restore draft"), m_recoveryPanel);
        m_restoreRecoveryButton->setObjectName(QStringLiteral("packRestoreDraft"));
        m_discardRecoveryButton = new QPushButton(tr("Discard recovery copy"), m_recoveryPanel);
        m_discardRecoveryButton->setObjectName(QStringLiteral("packDiscardRecovery"));
        m_retryRecoveryButton = new QPushButton(tr("Retry recovery"), m_recoveryPanel);
        m_retryRecoveryButton->setObjectName(QStringLiteral("packRetryRecovery"));
        buttons->addWidget(m_restoreRecoveryButton);
        buttons->addWidget(m_discardRecoveryButton);
        buttons->addWidget(m_retryRecoveryButton);
        buttons->addStretch(1);
        auto* root = qobject_cast<QVBoxLayout*>(layout());
        const int index = root->indexOf(m_draftStatusLabel) + 1;
        root->insertWidget(index, m_recoveryStatus);
        root->insertWidget(index + 1, m_recoveryPanel);
        connect(m_restoreRecoveryButton, &QPushButton::clicked, this, &PackManagerWidget::RestoreRecovery);
        connect(m_discardRecoveryButton, &QPushButton::clicked, this, [this]()
        {
            if (RetireRecovery())
            {
                m_recoveryPanel->hide();
                SetRecoveryPending(false);
            }
        });
        connect(m_retryRecoveryButton, &QPushButton::clicked, this, &PackManagerWidget::StartRecoveryForWorkspace);
        m_recoveryTimer = new QTimer(this);
        m_recoveryTimer->setSingleShot(true);
        m_recoveryTimer->setInterval(750);
        connect(m_recoveryTimer, &QTimer::timeout, this, &PackManagerWidget::CheckpointRecovery);
        m_recoveryStore = std::make_shared<PackDraftRecoveryService>();
        m_recoveryThread = new QThread(this);
        m_recoveryWorker = new QObject();
        m_recoveryWorker->moveToThread(m_recoveryThread);
        connect(m_recoveryThread, &QThread::finished, m_recoveryWorker, &QObject::deleteLater);
        m_recoveryThread->start();
        StartRecoveryForWorkspace();
    }

    void PackManagerWidget::SetRecoveryPending(bool pending)
    {
        m_recoveryPending = pending;
        for (QWidget* field : m_recoveryFields)
        {
            field->setEnabled(!pending);
        }
        m_newButton->setEnabled(!pending);
        m_saveButton->setEnabled(!pending);
        m_advancedToggleButton->setEnabled(!pending);
        RefreshWorkspaceMods();
        if (pending)
        {
            m_workspaceModsCombo->setEnabled(false);
            m_openSelectedButton->setEnabled(false);
        }
    }

    void PackManagerWidget::StartRecoveryForWorkspace()
    {
        const quint64 generation = ++m_recoveryGeneration;
        m_recoveryTimer->stop();
        m_recoveryStoreReady = false;
        m_recoveryWriteInFlight = false;
        m_recoveryRead = {};
        SetRecoveryPending(true);
        m_recoveryPanel->hide();
        m_recoveryStatus->setText(tr("Checking draft recovery..."));
        const FoundationService& service = FoundationService::Get();
        const QString id = QString::fromUtf8(service.GetWorkspace().m_workspaceId.c_str());
        const QString root = QString::fromUtf8(service.GetWorkspaceRootPath().c_str());
        const QString document = QString::fromUtf8(service.GetWorkspaceFilePath().c_str());
        QMetaObject::invokeMethod(m_recoveryWorker, [this, generation, id, root, document]()
        {
            QString error;
            const bool bound = m_recoveryStore->Bind(id, root, document, error);
            PackDraftRecoveryRead result;
            if (bound)
            {
                result = m_recoveryStore->Read();
            }
            else
            {
                result.m_error = error;
            }
            QMetaObject::invokeMethod(this, [this, generation, bound, result]()
            {
                if (generation != m_recoveryGeneration)
                {
                    return;
                }
                m_recoveryStoreReady = bound;
                m_recoveryRead = result;
                if (!result.m_error.isEmpty() || result.m_exists)
                {
                    m_recoveryStatus->setText(result.m_error.isEmpty()
                        ? tr("An unsaved draft is available for this workspace. Restore it or discard the recovery copy.")
                        : result.m_error);
                    m_restoreRecoveryButton->setEnabled(result.m_valid);
                    m_discardRecoveryButton->setEnabled(bound);
                    m_retryRecoveryButton->setVisible(!result.m_error.isEmpty());
                    m_recoveryPanel->show();
                }
                else
                {
                    m_recoveryStatus->clear();
                    SetRecoveryPending(false);
                }
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    }

    PackDraftRecovery PackManagerWidget::CaptureRecovery() const
    {
        PackDraftRecovery draft;
        for (auto it = m_recoveryFields.cbegin(); it != m_recoveryFields.cend(); ++it)
        {
            draft.m_fields.insert(it.key(), m_formValues.value(it.value()));
            draft.m_baseline.insert(it.key(), m_savedFormValues.value(it.value()));
        }
        draft.m_packId = m_packIdEdit->text();
        draft.m_isNew = m_isNewPack;
        draft.m_advancedExpanded = !m_advancedGroup->isHidden();
        return draft;
    }

    void PackManagerWidget::ScheduleRecovery()
    {
        // One timer and one outstanding worker request; no disk IO on field signals.
        if (m_recoveryTimer && !m_recoverySuppressed && !m_recoveryPending && !m_recoveryClosing
            && !m_recoveryTimer->isActive())
        {
            m_recoveryTimer->start();
        }
    }

    void PackManagerWidget::CheckpointRecovery()
    {
        if (m_recoveryPending || !m_recoveryStoreReady || m_recoveryClosing)
        {
            return;
        }
        if (m_recoveryWriteInFlight)
        {
            m_recoveryTimer->start();
            return;
        }
        const quint64 generation = m_recoveryGeneration;
        const PackDraftRecovery draft = CaptureRecovery();
        m_recoveryWriteInFlight = true;
        QMetaObject::invokeMethod(m_recoveryWorker, [this, generation, draft]()
        {
            QString error;
            const bool clean = draft.m_fields == draft.m_baseline;
            const bool success = clean ? m_recoveryStore->Clear(error) : m_recoveryStore->Write(draft, error);
            QMetaObject::invokeMethod(this, [this, generation, clean, success, error]()
            {
                if (generation == m_recoveryGeneration)
                {
                    m_recoveryWriteInFlight = false;
                    m_recoveryStatus->setText(success
                        ? (clean ? QString() : tr("Recovery copy updated.")) : error);
                }
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    }

    bool PackManagerWidget::RetireRecovery()
    {
        if (!m_recoveryStoreReady)
        {
            return false;
        }
        ++m_recoveryGeneration; // Ignore acknowledgements queued before this explicit action.
        m_recoveryTimer->stop();
        m_recoveryWriteInFlight = false;
        QString error;
        bool success = false;
        // Explicit Save/Discard waits for earlier checkpoints before removing the copy.
        QMetaObject::invokeMethod(m_recoveryWorker, [this, &error, &success]()
        {
            success = m_recoveryStore->Clear(error);
        }, Qt::BlockingQueuedConnection);
        m_recoveryStatus->setText(error);
        if (success)
        {
            m_recoveryRead = {};
        }
        return success;
    }

    void PackManagerWidget::RestoreRecovery()
    {
        if (!m_recoveryPending || !m_recoveryRead.m_valid)
        {
            return;
        }
        const QScopedValueRollback<bool> suppress(m_recoverySuppressed, true);
        const PackDraftRecovery draft = m_recoveryRead.m_draft;
        m_isNewPack = false; // Preserve the captured identity while applying the raw fields.
        for (auto it = m_recoveryFields.cbegin(); it != m_recoveryFields.cend(); ++it)
        {
            const QString value = draft.m_fields.value(it.key());
            if (auto* edit = qobject_cast<QLineEdit*>(it.value()))
            {
                edit->setText(value);
            }
            else if (auto* text = qobject_cast<QPlainTextEdit*>(it.value()))
            {
                text->setPlainText(value);
            }
            else if (auto* combo = qobject_cast<QComboBox*>(it.value()))
            {
                combo->setCurrentText(value);
            }
            m_savedFormValues.insert(it.value(), draft.m_baseline.value(it.key()));
        }
        m_packIdEdit->setText(draft.m_packId);
        m_isNewPack = draft.m_isNew;
        m_advancedGroup->setVisible(draft.m_advancedExpanded);
        m_advancedToggleButton->setText(draft.m_advancedExpanded ? tr("Hide advanced manifest") : tr("Show advanced manifest"));
        m_recoveryPanel->hide();
        SetRecoveryPending(false);
        UpdateGeneratedIdentity();
        UpdateDraftStatus();
        UpdateSummary();
        m_recoveryStatus->setText(tr("Draft restored. Save mod to keep your changes."));
    }

    void PackManagerWidget::StopRecovery()
    {
        if (!m_recoveryThread)
        {
            return;
        }
        m_recoveryTimer->stop();
        const bool preserve = !m_recoveryClosing && !m_recoveryPending && m_recoveryStoreReady;
        const PackDraftRecovery draft = CaptureRecovery();
        // Drain queued operations before destroying their store or callback context.
        QMetaObject::invokeMethod(m_recoveryWorker, [this, preserve, draft]()
        {
            if (preserve)
            {
                QString error;
                if (draft.m_fields == draft.m_baseline)
                {
                    m_recoveryStore->Clear(error);
                }
                else
                {
                    m_recoveryStore->Write(draft, error);
                }
            }
        }, Qt::BlockingQueuedConnection);
        m_recoveryThread->quit();
        m_recoveryThread->wait();
        m_recoveryStore.reset();
    }
} // namespace TaintedGrailModdingSDK
