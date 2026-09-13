/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorTroopEditorWidget.h"
#include "FoundationService.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QJsonObject>
#include <QMetaObject>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

namespace TaintedGrailModdingSDK
{
    void ActorTroopEditorWidget::InitializeRecovery()
    {
        m_recoveryStatus = new QLabel(this);
        m_recoveryStatus->setObjectName("populationRecoveryStatus");
        m_recoveryStatus->setWordWrap(true);
        m_recoveryPanel = new QWidget(this);
        m_recoveryPanel->setObjectName("populationRecoveryPrompt");
        auto* buttons = new QHBoxLayout(m_recoveryPanel);
        buttons->setContentsMargins(0, 0, 0, 0);
        m_restoreRecovery = new QPushButton(tr("Restore drafts"), m_recoveryPanel);
        m_restoreRecovery->setObjectName("populationRestoreDrafts");
        m_discardRecovery = new QPushButton(tr("Discard recovery copy"), m_recoveryPanel);
        m_discardRecovery->setObjectName("populationDiscardRecovery");
        m_retryRecovery = new QPushButton(tr("Retry recovery"), m_recoveryPanel);
        m_retryRecovery->setObjectName("populationRetryRecovery");
        buttons->addWidget(m_restoreRecovery);
        buttons->addWidget(m_discardRecovery);
        buttons->addWidget(m_retryRecovery);
        buttons->addStretch();
        auto* outer = qobject_cast<QVBoxLayout*>(layout());
        outer->insertWidget(0, m_recoveryStatus);
        outer->insertWidget(1, m_recoveryPanel);
        connect(m_restoreRecovery, &QPushButton::clicked, this, &ActorTroopEditorWidget::RestoreRecovery);
        connect(m_discardRecovery, &QPushButton::clicked, this, [this]()
        {
            if (ClearRecovery())
            {
                m_recoveryRead = {};
                m_recoveryResumeAfterExit = false;
                m_recoveryExitDraft = {};
                m_recoveryPanel->hide();
                SetRecoveryPending(false);
                ScheduleRecovery();
            }
        });
        connect(m_retryRecovery, &QPushButton::clicked, this, [this]()
        {
            if (m_recoveryPending) { StartRecoveryForWorkspace(); }
            else { CheckpointRecovery(); }
        });
        m_recoveryTimer = new QTimer(this);
        m_recoveryTimer->setSingleShot(true);
        m_recoveryTimer->setInterval(750);
        connect(m_recoveryTimer, &QTimer::timeout, this, &ActorTroopEditorWidget::CheckpointRecovery);
        // Field signals only arm a timer; capture and disk IO are not hot-path work.
        for (QWidget* field : DraftControls())
        {
            if (auto* edit = qobject_cast<QLineEdit*>(field)) { connect(edit, &QLineEdit::textChanged, this, &ActorTroopEditorWidget::ScheduleRecovery); }
            else if (auto* spin = qobject_cast<QSpinBox*>(field)) { connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, &ActorTroopEditorWidget::ScheduleRecovery); }
            else if (auto* check = qobject_cast<QCheckBox*>(field)) { connect(check, &QCheckBox::toggled, this, &ActorTroopEditorWidget::ScheduleRecovery); }
            else if (auto* combo = qobject_cast<QComboBox*>(field)) { connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, &ActorTroopEditorWidget::ScheduleRecovery); }
            else if (auto* list = qobject_cast<QListWidget*>(field)) { connect(list, &QListWidget::itemSelectionChanged, this, &ActorTroopEditorWidget::ScheduleRecovery); }
        }
        connect(m_tabs, &QTabWidget::currentChanged, this, &ActorTroopEditorWidget::ScheduleRecovery);
        m_recoveryStore = std::make_shared<ActorTroopDraftRecoveryService>();
        m_recoveryThread = new QThread(this);
        m_recoveryWorker = new QObject();
        m_recoveryWorker->moveToThread(m_recoveryThread);
        connect(m_recoveryThread, &QThread::finished, m_recoveryWorker, &QObject::deleteLater);
        m_recoveryThread->start();
        StartRecoveryForWorkspace();
    }

    void ActorTroopEditorWidget::SetRecoveryPending(bool pending)
    {
        if (pending == m_recoveryPending) { return; }
        if (pending)
        {
            for (QWidget* control : findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            {
                if (control != m_recoveryStatus && control != m_recoveryPanel)
                {
                    m_recoveryControls.insert(control, control->isEnabled());
                    control->setEnabled(false);
                }
            }
        }
        else
        {
            for (auto it = m_recoveryControls.cbegin(); it != m_recoveryControls.cend(); ++it) { it.key()->setEnabled(it.value()); }
            m_recoveryControls.clear();
        }
        m_recoveryPending = pending;
    }

    void ActorTroopEditorWidget::StartRecoveryForWorkspace()
    {
        const quint64 generation = ++m_recoveryGeneration;
        m_recoveryTimer->stop();
        m_recoveryClosing = false;
        m_recoveryHeldForExit = false;
        m_recoveryStoreReady = false;
        m_recoveryWriteInFlight = false;
        m_recoveryRead = {};
        m_restoreRecovery->show();
        m_discardRecovery->show();
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
            ActorTroopDraftRecoveryRead result;
            if (bound) { result = m_recoveryStore->Read(); }
            else { result.m_error = error; }
            QMetaObject::invokeMethod(this, [this, generation, bound, result]()
            {
                if (generation != m_recoveryGeneration || m_recoveryClosing) { return; }
                m_recoveryStoreReady = bound;
                m_recoveryRead = result;
                if (TryResumeRecoveryAfterExit()) { return; }
                if (result.m_valid && !CanRestoreRecovery(result.m_draft))
                {
                    m_recoveryRead.m_valid = false;
                    m_recoveryRead.m_error = tr("The recovery copy has incompatible fields or unavailable definitions or choices. It has been kept. Reload the original catalog and retry, or discard the copy.");
                }
                if (!m_recoveryRead.m_error.isEmpty() || result.m_exists)
                {
                    m_recoveryStatus->setText(m_recoveryRead.m_error.isEmpty()
                        ? tr("Unsaved actor, troop, and member drafts are available for this workspace. Restore them or discard the recovery copy.") : m_recoveryRead.m_error);
                    m_restoreRecovery->setEnabled(m_recoveryRead.m_valid);
                    m_discardRecovery->setEnabled(bound);
                    m_retryRecovery->setVisible(!m_recoveryRead.m_error.isEmpty());
                    m_recoveryPanel->show();
                }
                else
                {
                    m_recoveryStatus->clear();
                    SetRecoveryPending(false);
                    ScheduleRecovery(); // Includes drafts restored by a cancelled Editor exit.
                }
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    }

    void ActorTroopEditorWidget::ScheduleRecovery()
    {
        if (m_recoveryTimer && !m_recoveryPending && !m_recoveryClosing && !m_refreshing
            && !m_recoveryTimer->isActive()) { m_recoveryTimer->start(); }
    }

    void ActorTroopEditorWidget::CheckpointRecovery()
    {
        if (m_recoveryPending || !m_recoveryStoreReady || m_recoveryClosing) { return; }
        if (m_recoveryWriteInFlight) { m_recoveryTimer->start(); return; }
        const quint64 generation = m_recoveryGeneration;
        const ActorTroopDraftRecovery draft = CaptureRecovery(true);
        m_recoveryWriteInFlight = true;
        QMetaObject::invokeMethod(m_recoveryWorker, [this, generation, draft]()
        {
            QString error;
            const bool success = !draft.IsDirty() ? m_recoveryStore->Clear(error) : m_recoveryStore->Write(draft, error);
            QMetaObject::invokeMethod(this, [this, generation, success, error, clean = !draft.IsDirty()]()
            {
                if (generation != m_recoveryGeneration || m_recoveryClosing) { return; }
                m_recoveryWriteInFlight = false;
                m_recoveryStatus->setText(success ? (clean ? QString() : tr("Recovery copy updated.")) : error);
                m_recoveryPanel->setVisible(!success);
                m_restoreRecovery->setVisible(false);
                m_discardRecovery->setVisible(false);
                m_retryRecovery->setVisible(true);
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    }

    bool ActorTroopEditorWidget::FlushRecovery()
    {
        if (!m_recoveryTimer || m_recoveryPending || m_recoveryClosing) { return false; }
        ++m_recoveryGeneration;
        m_recoveryTimer->stop();
        m_recoveryWriteInFlight = false;
        const ActorTroopDraftRecovery draft = CaptureRecovery(true);
        QString error;
        bool success = false;
        QMetaObject::invokeMethod(m_recoveryWorker, [this, draft, &success, &error]()
        {
            success = !draft.IsDirty() ? m_recoveryStore->Clear(error) : m_recoveryStore->Write(draft, error);
        }, Qt::BlockingQueuedConnection);
        m_recoveryStatus->setText(success ? (!draft.IsDirty() ? QString() : tr("Recovery copy updated.")) : error);
        m_recoveryPanel->setVisible(!success);
        m_restoreRecovery->hide();
        m_discardRecovery->hide();
        m_retryRecovery->show();
        return success;
    }

    bool ActorTroopEditorWidget::ClearRecovery()
    {
        if (!m_recoveryStoreReady) { return false; }
        ++m_recoveryGeneration;
        m_recoveryTimer->stop();
        m_recoveryWriteInFlight = false;
        QString error;
        bool success = false;
        QMetaObject::invokeMethod(m_recoveryWorker, [this, &error, &success]()
        {
            success = m_recoveryStore->Clear(error);
        }, Qt::BlockingQueuedConnection);
        m_recoveryStatus->setText(error);
        if (!m_recoveryPending)
        {
            m_recoveryPanel->setVisible(!success);
            m_restoreRecovery->hide();
            m_discardRecovery->hide();
            m_retryRecovery->show();
        }
        return success;
    }

    bool ActorTroopEditorWidget::CanRestoreRecovery(const ActorTroopDraftRecovery& recovery) const
    {
        if (recovery.m_tab >= m_tabs->count() || m_actorRecord->findData(recovery.m_actor) < 0
            || m_troopRecord->findData(recovery.m_troop) < 0) { return false; }
        const auto expected = CaptureRecovery().m_values;
        const auto controls = DraftControls();
        if (recovery.m_values.size() != expected.size()) { return false; }
        const auto& foundation = FoundationService::Get();
        for (auto it = expected.cbegin(); it != expected.cend(); ++it)
        {
            if (!recovery.m_values.contains(it.key()) || recovery.m_values[it.key()].metaType() != it->metaType()) { return false; }
            const auto value = recovery.m_values[it.key()];
            QWidget* field = controls.value(it.key());
            if (auto* spin = qobject_cast<QSpinBox*>(field))
            {
                if (value.toInt() < spin->minimum() || value.toInt() > spin->maximum()) { return false; }
            }
            else if (auto* combo = qobject_cast<QComboBox*>(field))
            {
                const auto selection = value.toList();
                if (selection.size() != 3
                    || (selection[0].isValid() ? combo->findData(selection[0]) : combo->findText(selection[1].toString())) < 0) { return false; }
                for (const auto& entry : selection[2].toList())
                {
                    const auto pair = entry.toList();
                    if (pair.size() != 2 || combo->findData(pair[0]) < 0) { return false; }
                }
                if (field == m_actorRecord && selection[0].toString() != recovery.m_actor) { return false; }
                if (field == m_troopRecord && selection[0].toString() != recovery.m_troop) { return false; }
            }
            else if (qobject_cast<QListWidget*>(field))
            {
                for (const auto& id : value.toStringList())
                {
                    if (!foundation.GetSourceRegistry().FindEvidence(AZStd::string(id.toUtf8().constData()))) { return false; }
                }
            }
        }
        if (recovery.m_values["memberLinkId"].toString() != recovery.m_member) { return false; }
        for (const auto& value : recovery.m_members)
        {
            const auto row = value.toObject();
            const auto actor = row["ActorRecordId"].toString();
            if (!actor.isEmpty() && m_memberActorRecord->findData(actor) < 0) { return false; }
            if (m_memberRole->findText(row["Role"].toString()) < 0) { return false; }
            for (const auto& id : row["EvidenceIds"].toArray())
            {
                if (!foundation.GetSourceRegistry().FindEvidence(AZStd::string(id.toString().toUtf8().constData()))) { return false; }
            }
        }
        return true;
    }

    void ActorTroopEditorWidget::RestoreRecovery()
    {
        if (!m_recoveryPending || !m_recoveryRead.m_valid || !CanRestoreRecovery(m_recoveryRead.m_draft)) { return; }
        RestoreDraftState(m_recoveryRead.m_draft);
        m_recoveryPanel->hide();
        SetRecoveryPending(false);
        m_recoveryStatus->setText(tr("Drafts restored. Review them before saving, especially if the saved catalog has changed."));
        SetStatus(tr("Actor, troop, and member drafts restored for review."));
        ScheduleRecovery();
    }

    bool ActorTroopEditorWidget::TryResumeRecoveryAfterExit()
    {
        if (!m_recoveryResumeAfterExit || !m_recoveryStoreReady) { return false; }
        if (!m_recoveryRead.m_error.isEmpty()) { return false; }
        // Compare with the exact checkpoint held by the close transaction. Host
        // layout/focus events can change presentation while the async read runs.
        if (m_recoveryRead.m_exists && (!m_recoveryRead.m_valid || !(m_recoveryRead.m_draft == m_recoveryExitDraft)))
        {
            m_recoveryRead.m_valid = false;
            m_recoveryRead.m_error = tr("A different recovery copy is present. It has been kept alongside the restored forms. Discard that copy to continue with these forms, or retry recovery.");
            m_restoreRecovery->setEnabled(false);
            m_recoveryStatus->setText(m_recoveryRead.m_error);
            m_recoveryPanel->show();
            SetRecoveryPending(true);
            return false;
        }
        m_recoveryResumeAfterExit = false;
        m_recoveryExitDraft = {};
        m_recoveryRead = {};
        m_recoveryPanel->hide();
        SetRecoveryPending(false);
        ScheduleRecovery();
        return true;
    }

    void ActorTroopEditorWidget::ReleaseRecovery(bool holdForEditorExit)
    {
        m_recoveryClosing = true;
        m_recoveryHeldForExit = holdForEditorExit;
        ++m_recoveryGeneration;
        m_recoveryTimer->stop();
        QMetaObject::invokeMethod(m_recoveryWorker, [this]()
        {
            if (!m_recoveryHeldForExit) { m_recoveryStore->Release(); }
        }, Qt::BlockingQueuedConnection);
    }

    void ActorTroopEditorWidget::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        if (m_recoveryClosing) { StartRecoveryForWorkspace(); }
    }

    void ActorTroopEditorWidget::StopRecovery()
    {
        if (!m_recoveryThread) { return; }
        if (!m_recoveryClosing && !m_recoveryPending) { FlushRecovery(); }
        m_recoveryTimer->stop();
        QMetaObject::invokeMethod(m_recoveryWorker, [this]()
        {
            if (!m_recoveryHeldForExit) { m_recoveryStore->Release(); }
        }, Qt::BlockingQueuedConnection);
        m_recoveryThread->quit();
        m_recoveryThread->wait();
        m_recoveryStore.reset();
    }
} // namespace TaintedGrailModdingSDK
