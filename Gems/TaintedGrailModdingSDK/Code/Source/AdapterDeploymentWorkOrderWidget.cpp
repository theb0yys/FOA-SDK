/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentWorkOrderWidget.h"

#include <QAbstractItemView>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QObject>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        void SetCell(
            QTableWidget* table,
            int row,
            int column,
            const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }

        QString StepSummary(
            const AZStd::vector<AdapterDeploymentWorkOrderStep>& steps)
        {
            QStringList values;
            for (const AdapterDeploymentWorkOrderStep& step : steps)
            {
                values.push_back(
                    QObject::tr("%1. %2 | %3 | previous=%4 | desired=%5 | execution=false")
                        .arg(
                            QString::number(
                                static_cast<qulonglong>(step.m_sequence)),
                            ToQString(ToString(step.m_kind)),
                            ToQString(step.m_targetPath),
                            ToQString(step.m_previousFingerprint),
                            ToQString(step.m_desiredFingerprint)));
            }
            return values.join(QStringLiteral(" || "));
        }

        QString ChecklistSummary(
            const AZStd::vector<AdapterDeploymentOperatorChecklistItem>& items)
        {
            QStringList values;
            for (const AdapterDeploymentOperatorChecklistItem& item : items)
            {
                values.push_back(
                    QObject::tr("%1. %2 [%3] acknowledgement=false")
                        .arg(
                            QString::number(
                                static_cast<qulonglong>(item.m_sequence)),
                            ToQString(item.m_label),
                            ToQString(ToString(item.m_state))));
            }
            return values.join(QStringLiteral(" || "));
        }

        QString BlockerSummary(
            const AZStd::vector<AdapterDeploymentWorkOrderBlocker>& blockers)
        {
            QStringList values;
            for (const AdapterDeploymentWorkOrderBlocker& blocker : blockers)
            {
                values.push_back(
                    QObject::tr("%1 | %2 | %3")
                        .arg(
                            ToQString(blocker.m_code),
                            ToQString(blocker.m_subject),
                            ToQString(blocker.m_reason)));
            }
            return values.join(QStringLiteral(" || "));
        }

        QString ConfirmationSummary(
            const AdapterDeploymentWorkOrder& workOrder)
        {
            return QObject::tr("%1 | %2 | scope=%3 | expires=%4")
                .arg(
                    ToQString(workOrder.m_confirmationId),
                    ToQString(workOrder.m_reviewer),
                    ToQString(ToString(workOrder.m_confirmationScope)),
                    ToQString(workOrder.m_confirmationExpiresAtUtc));
        }

        QString WindowSummary(
            const AdapterDeploymentWorkOrder& workOrder)
        {
            return QObject::tr("%1 | %2 → %3 | operators=%4 | evaluated=%5")
                .arg(
                    ToQString(workOrder.m_maintenanceWindowId),
                    ToQString(workOrder.m_maintenanceStartAtUtc),
                    ToQString(workOrder.m_maintenanceEndAtUtc),
                    ToQString(workOrder.m_operatorGroup),
                    ToQString(workOrder.m_evaluatedAtUtc));
        }
    } // namespace

    AdapterDeploymentWorkOrderWidget::AdapterDeploymentWorkOrderWidget(
        QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Deployment Confirmation and Work Orders"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 explicit-confirmation and work-order contract. "
                "Each row binds one exact ready staging/deployment preview to a named reviewer, "
                "typed scope, expiry, UTC maintenance window, preflight evidence, deterministic "
                "work-order steps, and an operator checklist. ReviewReady never authorizes execution. "
                "Nothing is copied, deleted, backed up, restored, deployed, launched, or executed."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 11, this);
        m_table->setHorizontalHeaderLabels({
            tr("Work order"),
            tr("Preview / pack"),
            tr("Confirmation"),
            tr("Maintenance window"),
            tr("Status"),
            tr("Typed work-order steps"),
            tr("Operator checklist"),
            tr("Blockers"),
            tr("Execution permissions"),
            tr("Canonical JSON"),
            tr("Review boundary"),
        });
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table, 1);

        FoundationNotificationBus::Handler::BusConnect();
        Refresh();
    }

    AdapterDeploymentWorkOrderWidget::~AdapterDeploymentWorkOrderWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterDeploymentWorkOrderWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterDeploymentWorkOrderWidget::Refresh()
    {
        const AZStd::vector<AdapterDeploymentWorkOrderRequest>& requests =
            AdapterDeploymentWorkOrderRegistry::Get().GetRequests();
        m_table->setRowCount(static_cast<int>(requests.size()));

        AZ::u64 reviewReadyCount = 0;
        for (size_t index = 0; index < requests.size(); ++index)
        {
            const AdapterDeploymentWorkOrder workOrder =
                m_service.BuildWorkOrder(requests[index]);
            if (workOrder.m_status
                == AdapterDeploymentWorkOrderStatus::ReviewReady)
            {
                ++reviewReadyCount;
            }

            const int row = static_cast<int>(index);
            SetCell(m_table, row, 0, ToQString(workOrder.m_workOrderId));
            SetCell(
                m_table,
                row,
                1,
                tr("%1 | pack=%2 | target inventory=%3")
                    .arg(
                        ToQString(workOrder.m_previewId),
                        ToQString(workOrder.m_packId),
                        ToQString(workOrder.m_targetInventoryId)));
            SetCell(m_table, row, 2, ConfirmationSummary(workOrder));
            SetCell(m_table, row, 3, WindowSummary(workOrder));
            SetCell(
                m_table,
                row,
                4,
                tr("%1; execution=false")
                    .arg(ToQString(ToString(workOrder.m_status))));
            SetCell(m_table, row, 5, StepSummary(workOrder.m_steps));
            SetCell(m_table, row, 6, ChecklistSummary(workOrder.m_checklist));
            SetCell(m_table, row, 7, BlockerSummary(workOrder.m_blockers));
            SetCell(
                m_table,
                row,
                8,
                tr(
                    "ExecutionAllowed=false | CopyAllowed=false | DeleteAllowed=false | "
                    "BackupAllowed=false | RestoreAllowed=false | DeploymentAllowed=false | "
                    "LaunchAllowed=false"));
            SetCell(m_table, row, 9, ToQString(workOrder.m_canonicalJson));
            SetCell(
                m_table,
                row,
                10,
                tr(
                    "ReviewReady is an operator-review state only. No acknowledgement is recorded "
                    "and no execution path exists."));
        }

        m_summary->setText(
            tr(
                "Registered confirmation/work-order inputs: %1 | review-ready contracts: %2 | "
                "execution, copy, delete, backup, restore, deployment, and launch: prohibited")
                .arg(
                    QString::number(
                        static_cast<qulonglong>(requests.size())),
                    QString::number(
                        static_cast<qulonglong>(reviewReadyCount))));
    }
} // namespace TaintedGrailModdingSDK
