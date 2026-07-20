/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentExecutionEvidenceWidget.h"

#include <AzCore/std/algorithm.h>

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

        QString JoinValues(const AZStd::vector<AZStd::string>& values)
        {
            QStringList result;
            for (const AZStd::string& value : values)
            {
                result.push_back(ToQString(value));
            }
            return result.join(QStringLiteral(", "));
        }

        void SetCell(
            QTableWidget* table,
            int row,
            int column,
            const QString& value)
        {
            table->setItem(
                row,
                column,
                new QTableWidgetItem(value));
        }

        const AdapterDeploymentWorkOrder* FindWorkOrder(
            const AZStd::vector<AdapterDeploymentWorkOrder>& workOrders,
            const AZStd::string& workOrderId)
        {
            for (const AdapterDeploymentWorkOrder& workOrder : workOrders)
            {
                if (workOrder.m_workOrderId == workOrderId)
                {
                    return &workOrder;
                }
            }
            return nullptr;
        }

        const AdapterDeploymentBackupResult* FindBackup(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            for (const AdapterDeploymentBackupResult& backup :
                envelope.m_backupResults)
            {
                if (backup.m_stepId == stepId)
                {
                    return &backup;
                }
            }
            return nullptr;
        }

        const AdapterDeploymentTargetVerification* FindVerification(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            for (const AdapterDeploymentTargetVerification& verification :
                envelope.m_targetVerifications)
            {
                if (verification.m_stepId == stepId)
                {
                    return &verification;
                }
            }
            return nullptr;
        }

        const AdapterDeploymentRollbackResult* FindRollback(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            for (const AdapterDeploymentRollbackResult& rollback :
                envelope.m_rollbackResults)
            {
                if (rollback.m_sourceStepId == stepId)
                {
                    return &rollback;
                }
            }
            return nullptr;
        }

        QString IssueDetails(
            const AdapterDeploymentExecutionEvidenceReturn& result)
        {
            QStringList values;
            for (const AdapterDeploymentExecutionResultIssue& issue :
                result.m_issues)
            {
                QString value = ToQString(issue.m_code)
                    + QStringLiteral(": ")
                    + ToQString(issue.m_message);
                if (!issue.m_stepId.empty())
                {
                    value += QStringLiteral(" [step=")
                        + ToQString(issue.m_stepId)
                        + QStringLiteral("]");
                }
                if (!issue.m_rollbackResultId.empty())
                {
                    value += QStringLiteral(" [rollback=")
                        + ToQString(issue.m_rollbackResultId)
                        + QStringLiteral("]");
                }
                values.push_back(value);
            }
            return values.join(QStringLiteral(" | "));
        }

        QString FailureAndLogDetails(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AdapterDeploymentExecutionStepResult& step)
        {
            QStringList values;
            for (const AZStd::string& failureId : step.m_failureIds)
            {
                for (const AdapterDeploymentExecutionFailure& failure :
                    envelope.m_failures)
                {
                    if (failure.m_failureId == failureId)
                    {
                        values.push_back(
                            QObject::tr("%1 [%2/%3]: %4")
                                .arg(
                                    ToQString(failure.m_failureId),
                                    ToQString(ToString(failure.m_kind)),
                                    ToQString(failure.m_code),
                                    ToQString(failure.m_message)));
                    }
                }
            }
            for (const AZStd::string& logId : step.m_logReferenceIds)
            {
                for (const AdapterDeploymentExecutionLogReference& log :
                    envelope.m_logReferences)
                {
                    if (log.m_logId == logId)
                    {
                        values.push_back(
                            QObject::tr("%1 [%2] %3 | %4")
                                .arg(
                                    ToQString(log.m_logId),
                                    ToQString(ToString(log.m_kind)),
                                    ToQString(log.m_reference),
                                    ToQString(log.m_fingerprint)));
                    }
                }
            }
            return values.join(QStringLiteral(" | "));
        }

        QString BackupDetails(
            const AdapterDeploymentBackupResult* backup)
        {
            if (!backup)
            {
                return {};
            }
            return QObject::tr("%1 | %2 | source=%3 | backup=%4")
                .arg(
                    ToQString(ToString(backup->m_outcome)),
                    ToQString(backup->m_backupPath),
                    ToQString(backup->m_sourceFingerprint),
                    ToQString(backup->m_backupFingerprint));
        }

        QString VerificationDetails(
            const AdapterDeploymentTargetVerification* verification)
        {
            if (!verification)
            {
                return {};
            }
            return QObject::tr("%1 | expected=%2/%3 | observed=%4/%5")
                .arg(
                    ToQString(ToString(verification->m_status)),
                    verification->m_expectedPresent
                        ? QObject::tr("present")
                        : QObject::tr("absent"),
                    ToQString(verification->m_expectedFingerprint),
                    verification->m_observedPresent
                        ? QObject::tr("present")
                        : QObject::tr("absent"),
                    ToQString(verification->m_observedFingerprint));
        }

        QString RollbackDetails(
            const AdapterDeploymentRollbackResult* rollback)
        {
            if (!rollback)
            {
                return {};
            }
            return QObject::tr("%1 | %2 | restore=%3 | final=%4")
                .arg(
                    ToQString(ToString(rollback->m_action)),
                    ToQString(ToString(rollback->m_outcome)),
                    ToQString(rollback->m_restoreFingerprint),
                    ToQString(rollback->m_finalFingerprint));
        }
    } // namespace

    AdapterDeploymentExecutionEvidenceWidget::
        AdapterDeploymentExecutionEvidenceWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Deployment Execution Result Evidence"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 contract verification for deployment execution-result "
                "metadata supplied by a separately reviewed executor. Exact work-order "
                "and attempted-step identities, backup and restore outcomes, target "
                "fingerprints, rollback results, failures, and safe log references are "
                "returned as candidate source/evidence documents. Automatic evidence promotion: prohibited. "
                "Nothing is executed, imported, promoted, validated, permitted, deployed, launched, or written."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 12, this);
        m_table->setHorizontalHeaderLabels({
            tr("Result"),
            tr("Work order"),
            tr("Contract status"),
            tr("Reviewed executor"),
            tr("Attempted step"),
            tr("Outcome / observed fingerprint"),
            tr("Backup outcome"),
            tr("Target verification"),
            tr("Rollback / restore outcome"),
            tr("Failures and logs"),
            tr("Evidence candidates"),
            tr("Issues"),
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

    AdapterDeploymentExecutionEvidenceWidget::
        ~AdapterDeploymentExecutionEvidenceWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterDeploymentExecutionEvidenceWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterDeploymentExecutionEvidenceWidget::Refresh()
    {
        AZStd::vector<AdapterDeploymentWorkOrder> workOrders;
        for (const AdapterDeploymentWorkOrderRequest& request :
            AdapterDeploymentWorkOrderRegistry::Get().GetRequests())
        {
            workOrders.push_back(
                m_workOrderService.BuildWorkOrder(request));
        }

        const AZStd::vector<AdapterDeploymentExecutionResultEnvelope>&
            envelopes =
                AdapterDeploymentExecutionResultRegistry::Get().
                    GetEnvelopes();

        AZ::u64 acceptedCount = 0;
        AZ::u64 rejectedCount = 0;
        AZ::u64 sourceCandidateCount = 0;
        AZ::u64 evidenceCandidateCount = 0;
        int rowCount = 0;
        for (const AdapterDeploymentExecutionResultEnvelope& envelope :
            envelopes)
        {
            const AdapterDeploymentWorkOrder* workOrder =
                FindWorkOrder(workOrders, envelope.m_workOrderId);
            if (!workOrder)
            {
                ++rejectedCount;
                ++rowCount;
                continue;
            }
            const AdapterDeploymentExecutionEvidenceReturn result =
                m_resultService.BuildEvidenceReturn(
                    *workOrder,
                    envelope);
            acceptedCount += result.m_accepted ? 1 : 0;
            rejectedCount += result.m_accepted ? 0 : 1;
            sourceCandidateCount += result.m_sourceDocumentCount;
            evidenceCandidateCount += result.m_evidenceRecordCount;
            rowCount += result.m_accepted
                ? static_cast<int>(envelope.m_stepResults.size())
                : 1;
        }

        m_summary->setText(
            tr(
                "Registered execution-result envelopes: %1 | accepted contracts: %2 | "
                "rejected contracts: %3 | candidate source documents: %4 | "
                "candidate evidence records: %5 | executor invocation and automatic "
                "evidence promotion: prohibited")
                .arg(
                    QString::number(
                        static_cast<qulonglong>(envelopes.size())),
                    QString::number(
                        static_cast<qulonglong>(acceptedCount)),
                    QString::number(
                        static_cast<qulonglong>(rejectedCount)),
                    QString::number(
                        static_cast<qulonglong>(sourceCandidateCount)),
                    QString::number(
                        static_cast<qulonglong>(evidenceCandidateCount))));

        m_table->setRowCount(rowCount);
        int rowIndex = 0;
        for (const AdapterDeploymentExecutionResultEnvelope& envelope :
            envelopes)
        {
            const AdapterDeploymentWorkOrder* workOrder =
                FindWorkOrder(workOrders, envelope.m_workOrderId);
            if (!workOrder)
            {
                SetCell(
                    m_table,
                    rowIndex,
                    0,
                    ToQString(envelope.m_resultId));
                SetCell(
                    m_table,
                    rowIndex,
                    1,
                    ToQString(envelope.m_workOrderId));
                SetCell(
                    m_table,
                    rowIndex,
                    2,
                    tr("rejected; no current reviewed work order"));
                SetCell(
                    m_table,
                    rowIndex,
                    3,
                    ToQString(
                        envelope.m_executorReview.m_executorId));
                SetCell(
                    m_table,
                    rowIndex,
                    11,
                    tr(
                        "No current canonical deployment work order matches this "
                        "exact result envelope."));
                ++rowIndex;
                continue;
            }

            const AdapterDeploymentExecutionEvidenceReturn result =
                m_resultService.BuildEvidenceReturn(
                    *workOrder,
                    envelope);
            if (!result.m_accepted)
            {
                SetCell(
                    m_table,
                    rowIndex,
                    0,
                    ToQString(envelope.m_resultId));
                SetCell(
                    m_table,
                    rowIndex,
                    1,
                    ToQString(envelope.m_workOrderId));
                SetCell(
                    m_table,
                    rowIndex,
                    2,
                    ToQString(ToString(result.m_status)));
                SetCell(
                    m_table,
                    rowIndex,
                    3,
                    tr("%1 %2 | review=%3 | reviewer=%4")
                        .arg(
                            ToQString(
                                envelope.m_executorReview.m_executorId),
                            ToQString(
                                envelope.m_executorReview.m_executorVersion),
                            ToQString(
                                envelope.m_executorReview.m_reviewId),
                            ToQString(
                                envelope.m_executorReview.m_reviewer)));
                SetCell(
                    m_table,
                    rowIndex,
                    10,
                    tr("0 source documents | 0 evidence records"));
                SetCell(
                    m_table,
                    rowIndex,
                    11,
                    IssueDetails(result));
                ++rowIndex;
                continue;
            }

            AZStd::vector<const AdapterDeploymentExecutionStepResult*> steps;
            for (const AdapterDeploymentExecutionStepResult& step :
                envelope.m_stepResults)
            {
                steps.push_back(&step);
            }
            AZStd::sort(
                steps.begin(),
                steps.end(),
                [](const AdapterDeploymentExecutionStepResult* left,
                    const AdapterDeploymentExecutionStepResult* right)
                {
                    if (left->m_sequence != right->m_sequence)
                    {
                        return left->m_sequence < right->m_sequence;
                    }
                    return left->m_stepId < right->m_stepId;
                });

            for (const AdapterDeploymentExecutionStepResult* step : steps)
            {
                SetCell(
                    m_table,
                    rowIndex,
                    0,
                    ToQString(envelope.m_resultId));
                SetCell(
                    m_table,
                    rowIndex,
                    1,
                    ToQString(envelope.m_workOrderId));
                SetCell(
                    m_table,
                    rowIndex,
                    2,
                    tr("%1; accepted metadata, not execution authority")
                        .arg(ToQString(ToString(result.m_status))));
                SetCell(
                    m_table,
                    rowIndex,
                    3,
                    tr("%1 %2 | %3 | reviewer=%4")
                        .arg(
                            ToQString(
                                envelope.m_executorReview.m_executorId),
                            ToQString(
                                envelope.m_executorReview.m_executorVersion),
                            ToQString(
                                envelope.m_executorReview.m_executorFingerprint),
                            ToQString(
                                envelope.m_executorReview.m_reviewer)));
                SetCell(
                    m_table,
                    rowIndex,
                    4,
                    tr("%1. %2 | %3 | attempted=%4")
                        .arg(
                            QString::number(
                                static_cast<qulonglong>(
                                    step->m_sequence)),
                            ToQString(ToString(step->m_kind)),
                            ToQString(step->m_targetPath),
                            step->m_attempted
                                ? tr("true")
                                : tr("false")));
                SetCell(
                    m_table,
                    rowIndex,
                    5,
                    tr("%1 | present=%2 | observed=%3")
                        .arg(
                            ToQString(ToString(step->m_outcome)),
                            step->m_targetPresent
                                ? tr("true")
                                : tr("false"),
                            ToQString(step->m_observedFingerprint)));
                SetCell(
                    m_table,
                    rowIndex,
                    6,
                    BackupDetails(
                        FindBackup(envelope, step->m_stepId)));
                SetCell(
                    m_table,
                    rowIndex,
                    7,
                    VerificationDetails(
                        FindVerification(
                            envelope,
                            step->m_stepId)));
                SetCell(
                    m_table,
                    rowIndex,
                    8,
                    RollbackDetails(
                        FindRollback(envelope, step->m_stepId)));
                SetCell(
                    m_table,
                    rowIndex,
                    9,
                    FailureAndLogDetails(envelope, *step));
                SetCell(
                    m_table,
                    rowIndex,
                    10,
                    tr("%1 source documents | %2 evidence records")
                        .arg(
                            QString::number(
                                static_cast<qulonglong>(
                                    result.m_sourceDocumentCount)),
                            QString::number(
                                static_cast<qulonglong>(
                                    result.m_evidenceRecordCount))));
                SetCell(
                    m_table,
                    rowIndex,
                    11,
                    IssueDetails(result));
                ++rowIndex;
            }
        }
    }
} // namespace TaintedGrailModdingSDK
