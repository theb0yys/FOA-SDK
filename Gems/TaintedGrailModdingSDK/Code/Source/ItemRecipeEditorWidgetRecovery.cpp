/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ItemRecipeEditorWidget.h"
#include "FoundationService.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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
    void ItemRecipeEditorWidget::InitializeRecovery()
    {
        m_recoveryStatus = new QLabel(this);
        m_recoveryStatus->setObjectName("economyRecoveryStatus");
        m_recoveryStatus->setWordWrap(true);
        m_recoveryPanel = new QWidget(this);
        m_recoveryPanel->setObjectName("economyRecoveryPrompt");
        auto* buttons = new QHBoxLayout(m_recoveryPanel);
        buttons->setContentsMargins(0, 0, 0, 0);
        m_restoreRecovery = new QPushButton(tr("Restore drafts"), m_recoveryPanel);
        m_restoreRecovery->setObjectName("economyRestoreDrafts");
        m_discardRecovery = new QPushButton(tr("Discard recovery copy"), m_recoveryPanel);
        m_discardRecovery->setObjectName("economyDiscardRecovery");
        m_retryRecovery = new QPushButton(tr("Retry recovery"), m_recoveryPanel);
        m_retryRecovery->setObjectName("economyRetryRecovery");
        buttons->addWidget(m_restoreRecovery);
        buttons->addWidget(m_discardRecovery);
        buttons->addWidget(m_retryRecovery);
        buttons->addStretch();
        auto* outer = qobject_cast<QVBoxLayout*>(layout());
        outer->insertWidget(0, m_recoveryStatus);
        outer->insertWidget(1, m_recoveryPanel);
        connect(m_restoreRecovery, &QPushButton::clicked, this, &ItemRecipeEditorWidget::RestoreRecovery);
        connect(m_discardRecovery, &QPushButton::clicked, this, [this]()
        {
            if (ClearRecovery())
            {
                m_recoveryRead = {};
                m_recoveryResumeAfterExit = false;
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
        connect(m_recoveryTimer, &QTimer::timeout, this, &ItemRecipeEditorWidget::CheckpointRecovery);
        // Field signals only arm a timer; capture and disk IO are not hot-path work.
        for (QWidget* form : {m_itemForm, m_recipeForm, m_ingredientForm, m_outputForm, m_relationshipForm})
        {
            const FormValues fields = ReadForm(form);
            for (QWidget* field : form->findChildren<QWidget*>())
            {
                if (!fields.contains(field->objectName())) { continue; }
                if (auto* edit = qobject_cast<QLineEdit*>(field)) { connect(edit, &QLineEdit::textChanged, this, &ItemRecipeEditorWidget::ScheduleRecovery); }
                else if (auto* spin = qobject_cast<QSpinBox*>(field)) { connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, &ItemRecipeEditorWidget::ScheduleRecovery); }
                else if (auto* decimal = qobject_cast<QDoubleSpinBox*>(field)) { connect(decimal, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &ItemRecipeEditorWidget::ScheduleRecovery); }
                else if (auto* check = qobject_cast<QCheckBox*>(field)) { connect(check, &QCheckBox::toggled, this, &ItemRecipeEditorWidget::ScheduleRecovery); }
                else if (auto* combo = qobject_cast<QComboBox*>(field)) { connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, &ItemRecipeEditorWidget::ScheduleRecovery); }
            }
        }
        connect(m_itemRecord, qOverload<int>(&QComboBox::currentIndexChanged), this, &ItemRecipeEditorWidget::ScheduleRecovery);
        connect(m_recipeRecord, qOverload<int>(&QComboBox::currentIndexChanged), this, &ItemRecipeEditorWidget::ScheduleRecovery);
        connect(m_tabs, &QTabWidget::currentChanged, this, &ItemRecipeEditorWidget::ScheduleRecovery);
        connect(qobject_cast<QGroupBox*>(m_recipeForm), &QGroupBox::toggled, this, &ItemRecipeEditorWidget::ScheduleRecovery);
        m_recoveryStore = std::make_shared<ItemRecipeDraftRecoveryService>();
        m_recoveryThread = new QThread(this);
        m_recoveryWorker = new QObject();
        m_recoveryWorker->moveToThread(m_recoveryThread);
        connect(m_recoveryThread, &QThread::finished, m_recoveryWorker, &QObject::deleteLater);
        m_recoveryThread->start();
        StartRecoveryForWorkspace();
    }

    void ItemRecipeEditorWidget::SetRecoveryPending(bool pending)
    {
        if (pending == m_recoveryPending) { return; }
        if (pending)
        {
            for (QWidget* control : findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            {
                if (control == m_tabs || qobject_cast<QPushButton*>(control))
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

    void ItemRecipeEditorWidget::StartRecoveryForWorkspace()
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
            ItemRecipeDraftRecoveryRead result;
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
                        ? tr("Unsaved item and recipe drafts are available for this workspace. Restore them or discard the recovery copy.") : m_recoveryRead.m_error);
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

    ItemRecipeDraftRecovery ItemRecipeEditorWidget::CaptureRecovery()
    {
        StoreCurrentDrafts();
        ItemRecipeDraftRecovery result;
        for (const QString& key : UnsavedDraftKeys()) { result.m_drafts.insert(key, m_drafts.value(key)); }
        result.m_item = m_itemRecord->currentData().toString();
        result.m_recipe = m_recipeRecord->currentData().toString();
        result.m_tab = m_tabs->currentIndex();
        result.m_recipeExpanded = qobject_cast<QGroupBox*>(m_recipeForm)->isChecked();
        return result;
    }

    void ItemRecipeEditorWidget::ScheduleRecovery()
    {
        if (m_recoveryTimer && !m_recoveryPending && !m_recoveryClosing && !m_refreshing && !m_saving
            && !m_recoveryTimer->isActive()) { m_recoveryTimer->start(); }
    }

    void ItemRecipeEditorWidget::CheckpointRecovery()
    {
        if (m_recoveryPending || !m_recoveryStoreReady || m_recoveryClosing) { return; }
        if (m_recoveryWriteInFlight) { m_recoveryTimer->start(); return; }
        const quint64 generation = m_recoveryGeneration;
        const ItemRecipeDraftRecovery draft = CaptureRecovery();
        m_recoveryWriteInFlight = true;
        QMetaObject::invokeMethod(m_recoveryWorker, [this, generation, draft]()
        {
            QString error;
            const bool success = draft.m_drafts.isEmpty() ? m_recoveryStore->Clear(error) : m_recoveryStore->Write(draft, error);
            QMetaObject::invokeMethod(this, [this, generation, success, error, clean = draft.m_drafts.isEmpty()]()
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

    bool ItemRecipeEditorWidget::FlushRecovery()
    {
        if (!m_recoveryTimer || m_recoveryPending || m_recoveryClosing) { return false; }
        ++m_recoveryGeneration;
        m_recoveryTimer->stop();
        m_recoveryWriteInFlight = false;
        const ItemRecipeDraftRecovery draft = CaptureRecovery();
        QString error;
        bool success = false;
        QMetaObject::invokeMethod(m_recoveryWorker, [this, draft, &success, &error]()
        {
            success = draft.m_drafts.isEmpty() ? m_recoveryStore->Clear(error) : m_recoveryStore->Write(draft, error);
        }, Qt::BlockingQueuedConnection);
        m_recoveryStatus->setText(success ? (draft.m_drafts.isEmpty() ? QString() : tr("Recovery copy updated.")) : error);
        m_recoveryPanel->setVisible(!success);
        m_restoreRecovery->hide();
        m_discardRecovery->hide();
        m_retryRecovery->show();
        return success;
    }

    bool ItemRecipeEditorWidget::ClearRecovery()
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

    bool ItemRecipeEditorWidget::CanRestoreRecovery(const ItemRecipeDraftRecovery& recovery) const
    {
        if (recovery.m_tab >= m_tabs->count() || m_itemRecord->findData(recovery.m_item) < 0
            || m_recipeRecord->findData(recovery.m_recipe) < 0) { return false; }
        for (auto it = recovery.m_drafts.cbegin(); it != recovery.m_drafts.cend(); ++it)
        {
            const QString kind = it.key().section(':', 0, 0);
            const QString id = it.key().mid(it.key().indexOf(':') + 1);
            if (kind != "acquisition" && (kind == "item" ? m_itemRecord : m_recipeRecord)->findData(id) < 0) { return false; }
            QWidget* form = kind == "item" ? m_itemForm : kind == "recipe" ? m_recipeForm
                : kind == "ingredient" ? m_ingredientForm : kind == "output" ? m_outputForm : m_relationshipForm;
            const FormValues expected = ReadForm(form);
            for (const FormValues* fields : { &it->m_values, &it->m_baseline })
            {
                if (fields->size() != expected.size()) { return false; }
                for (auto field = expected.cbegin(); field != expected.cend(); ++field)
                {
                    if (!fields->contains(field.key()) || fields->value(field.key()).metaType() != field.value().metaType()) { return false; }
                    const QVariant value = fields->value(field.key());
                    QWidget* widget = form->findChild<QWidget*>(field.key());
                    if (auto* spin = qobject_cast<QSpinBox*>(widget))
                    {
                        if (value.toInt() < spin->minimum() || value.toInt() > spin->maximum()) { return false; }
                    }
                    else if (auto* decimal = qobject_cast<QDoubleSpinBox*>(widget))
                    {
                        const double number = value.toDouble();
                        const double scale = std::pow(10.0, decimal->decimals());
                        if (number < decimal->minimum() || number > decimal->maximum()
                            || std::abs(std::round(number * scale) / scale - number) > 1e-9) { return false; }
                    }
                    else if (auto* combo = qobject_cast<QComboBox*>(widget))
                    {
                        const QVariantMap selection = value.toMap();
                        if ((selection["data"].isValid() ? combo->findData(selection["data"]) : combo->findText(selection["text"].toString())) < 0) { return false; }
                    }
                }
            }
        }
        return true;
    }

    void ItemRecipeEditorWidget::RestoreRecovery()
    {
        if (!m_recoveryPending || !m_recoveryRead.m_valid || !CanRestoreRecovery(m_recoveryRead.m_draft)) { return; }
        const ItemRecipeDraftRecovery recovery = m_recoveryRead.m_draft;
        const QSignalBlocker itemBlocker(m_itemRecord);
        const QSignalBlocker recipeBlocker(m_recipeRecord);
        m_itemRecord->setCurrentIndex(m_itemRecord->findData(recovery.m_item));
        m_recipeRecord->setCurrentIndex(m_recipeRecord->findData(recovery.m_recipe));
        m_loadedItem.clear();
        m_loadedRecipe.clear();
        m_baselines.clear();
        m_drafts = recovery.m_drafts;
        RefreshAll();
        m_tabs->setCurrentIndex(recovery.m_tab);
        qobject_cast<QGroupBox*>(m_recipeForm)->setChecked(recovery.m_recipeExpanded);
        m_recoveryPanel->hide();
        SetRecoveryPending(false);
        m_recoveryStatus->setText(tr("Drafts restored. Review them before saving, especially if the saved catalog has changed."));
        ScheduleRecovery();
    }

    bool ItemRecipeEditorWidget::TryResumeRecoveryAfterExit()
    {
        if (!m_recoveryResumeAfterExit || !m_recoveryStoreReady) { return false; }
        if (!m_recoveryRead.m_error.isEmpty()) { return false; }
        if (m_recoveryRead.m_exists && (!m_recoveryRead.m_valid || !(m_recoveryRead.m_draft == CaptureRecovery())))
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
        m_recoveryRead = {};
        m_recoveryPanel->hide();
        SetRecoveryPending(false);
        ScheduleRecovery();
        return true;
    }

    void ItemRecipeEditorWidget::ReleaseRecovery(bool holdForEditorExit)
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

    void ItemRecipeEditorWidget::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        if (m_recoveryClosing) { StartRecoveryForWorkspace(); }
    }

    void ItemRecipeEditorWidget::StopRecovery()
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
