/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPostDeploymentVerificationWidget.h"

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
            table->setItem(row, column, new QTableWidgetItem(value));
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

        QString BlockerDetails(
            const AdapterPostDeploymentVerificationReport& report,
            bool compatibilityOnly)
        {
            QStringList values;
            for (const AdapterPostDeploymentBlocker& blocker : report.m_blockers)
            {
                if (compatibilityOnly && !blocker.m_blocksCompatibility)
                {
                    continue;
                }
                if (!compatibilityOnly && !blocker.m_blocksRelease)
                {
                    continue;
                }

                QString value = ToQString(blocker.m_code)
                    + QStringLiteral(": ")
                    + ToQString(blocker.m_message);
                if (!blocker.m_stepId.empty())
                {
                    value += QStringLiteral(" [step=")
                        + ToQString(blocker.m_stepId)
                        + QStringLiteral("]");
                }
                if (!blocker.m_rollbackResultId.empty())
                {
                    value += QStringLiteral(" [rollback=")
                        + ToQString(blocker.m_rollbackResultId)
                        + QStringLiteral("]");
                }
                if (!blocker.m_evidenceIds.empty())
                {
                    value += QStringLiteral(" [evidence=")
                        + JoinValues(blocker.m_evidenceIds)
                        + QStringLiteral("]");
                }
                if (!blocker.m_logReferenceIds.empty())
                {
                    value += QStringLiteral(" [logs=")
                        + JoinValues(blocker.m_logReferenceIds)
                        + QStringLiteral("]");
                }
                values.push_back(value);
            }
            return values.join(QStringLiteral(" | "));
        }
    } // namespace

    AdapterPostDeploymentVerificationWidget::
        AdapterPostDeploymentVerificationWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Post-Deployment Verification and Release Blockers"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 aggregation of one accepted deployment execution-result "
                "envelope, its candidate evidence, target-verification states, rollback "
                "completeness, failures, and referenced diagnostics. The report produces "
                "explicit compatibility and release blockers without executing a verifier, "
                "launching FoA, promoting evidence, publishing a release, or calling an "
                "adapter."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 10, this);
        m_table->setHorizontalHeaderLabels({
            tr("Result / report"),
            tr("Report status"),
            tr("Exact context"),
            tr("Candidate evidence"),
            tr("Step outcomes"),
            tr("Target verification"),
            tr("Rollback completeness"),
            tr("Failures / diagnostics"),
            tr("Compatibility blockers"),
            tr("Release blockers"),
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

    AdapterPostDeploymentVerificationWidget::
        ~AdapterPostDeploymentVerificationWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterPostDeploymentVerificationWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterPostDeploymentVerificationWidget::Refresh()
    {
        AZStd::vector<AdapterDeploymentWorkOrder> workOrders;
        for (const AdapterDeploymentWorkOrderRequest& request :
            AdapterDeploymentWorkOrderRegistry::Get().GetRequests())
        {
            workOrders.push_back(m_workOrderService.BuildWorkOrder(request));
        }

        const AZStd::vector<AdapterDeploymentExecutionResultEnvelope>& envelopes =
            AdapterDeploymentExecutionResultRegistry::Get().GetEnvelopes();
        m_table->setRowCount(static_cast<int>(envelopes.size()));

        AZ::u64 reviewReadyCount = 0;
        AZ::u64 compatibilityBlockedCount = 0;
        AZ::u64 releaseBlockedCount = 0;
        AZ::u64 candidateSourceCount = 0;
        AZ::u64 candidateEvidenceCount = 0;

        int rowIndex = 0;
        for (const AdapterDeploymentExecutionResultEnvelope& envelope : envelopes)
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
                    tr("evidence_rejected"));
                SetCell(
                    m_table,
                    rowIndex,
                    2,
                    tr("No current canonical deployment work order matches %1.")
                        .arg(ToQString(envelope.m_workOrderId)));
                SetCell(
                    m_table,
                    rowIndex,
                    9,
                    tr("Missing exact current work-order binding blocks release review."));
                ++releaseBlockedCount;
                ++rowIndex;
                continue;
            }

            const AdapterDeploymentExecutionEvidenceReturn evidenceReturn =
                m_evidenceService.BuildEvidenceReturn(*workOrder, envelope);
            const AdapterPostDeploymentVerificationReport report =
                m_reportService.BuildReport(
                    *workOrder,
                    envelope,
                    evidenceReturn);

            reviewReadyCount += report.m_status
                    == AdapterPostDeploymentReportStatus::ReviewReady
                ? 1
                : 0;
            compatibilityBlockedCount +=
                report.m_compatibilityBlockerCount != 0 ? 1 : 0;
            releaseBlockedCount += report.m_releaseBlockerCount != 0 ? 1 : 0;
            candidateSourceCount += report.m_candidateSourceDocumentCount;
            candidateEvidenceCount += report.m_candidateEvidenceRecordCount;

            SetCell(
                m_table,
                rowIndex,
                0,
                tr("%1 | %2")
                    .arg(
                        ToQString(report.m_resultId),
                        ToQString(report.m_reportId)));
            SetCell(
                m_table,
                rowIndex,
                1,
                tr("%1 | human review required")
                    .arg(ToQString(ToString(report.m_status))));
            SetCell(
                m_table,
                rowIndex,
                2,
                tr("work order=%1 | profile=%2 | game=%3 | branch=%4 | runtime=%5")
                    .arg(
                        ToQString(report.m_workOrderId),
                        ToQString(report.m_profileId),
                        ToQString(report.m_gameVersion),
                        ToQString(report.m_branch),
                        ToQString(report.m_runtimeTarget)));
            SetCell(
                m_table,
                rowIndex,
                3,
                tr("sources=%1 [%2] | evidence=%3 [%4] | promoted=no")
                    .arg(
                        QString::number(static_cast<qulonglong>(
                            report.m_candidateSourceDocumentCount)),
                        JoinValues(report.m_candidateSourceIds),
                        QString::number(static_cast<qulonglong>(
                            report.m_candidateEvidenceRecordCount)),
                        JoinValues(report.m_candidateEvidenceIds)));
            SetCell(
                m_table,
                rowIndex,
                4,
                tr("total=%1 | succeeded=%2 | failed=%3 | incomplete=%4")
                    .arg(
                        QString::number(static_cast<qulonglong>(
                            report.m_stepCount)),
                        QString::number(static_cast<qulonglong>(
                            report.m_completedStepCount)),
                        QString::number(static_cast<qulonglong>(
                            report.m_failedStepCount)),
                        QString::number(static_cast<qulonglong>(
                            report.m_incompleteStepCount))));
            SetCell(
                m_table,
                rowIndex,
                5,
                tr("matched=%1 | mismatched=%2 | not checked=%3 | verifier run=no")
                    .arg(
                        QString::number(static_cast<qulonglong>(
                            report.m_matchedVerificationCount)),
                        QString::number(static_cast<qulonglong>(
                            report.m_mismatchedVerificationCount)),
                        QString::number(static_cast<qulonglong>(
                            report.m_uncheckedVerificationCount))));
            SetCell(
                m_table,
                rowIndex,
                6,
                tr("required=%1 | succeeded=%2 | incomplete=%3 | backups incomplete=%4")
                    .arg(
                        QString::number(static_cast<qulonglong>(
                            report.m_rollbackRequiredCount)),
                        QString::number(static_cast<qulonglong>(
                            report.m_rollbackSucceededCount)),
                        QString::number(static_cast<qulonglong>(
                            report.m_rollbackIncompleteCount)),
                        QString::number(static_cast<qulonglong>(
                            report.m_incompleteBackupCount))));
            SetCell(
                m_table,
                rowIndex,
                7,
                tr("failures=%1 | diagnostics=%2 [%3] | content inspected=no")
                    .arg(
                        QString::number(static_cast<qulonglong>(
                            report.m_failureCount)),
                        QString::number(static_cast<qulonglong>(
                            report.m_diagnosticReferenceCount)),
                        JoinValues(report.m_diagnosticReferenceIds)));
            SetCell(
                m_table,
                rowIndex,
                8,
                BlockerDetails(report, true));
            SetCell(
                m_table,
                rowIndex,
                9,
                BlockerDetails(report, false));
            ++rowIndex;
        }

        m_summary->setText(
            tr(
                "Registered execution-result envelopes: %1 | review-ready reports: %2 | "
                "compatibility-blocked reports: %3 | release-blocked reports: %4 | "
                "candidate sources: %5 | candidate evidence: %6 | verifier execution, "
                "FoA launch, evidence promotion, release publication, and adapter calls: "
                "prohibited")
                .arg(
                    QString::number(static_cast<qulonglong>(envelopes.size())),
                    QString::number(static_cast<qulonglong>(reviewReadyCount)),
                    QString::number(static_cast<qulonglong>(
                        compatibilityBlockedCount)),
                    QString::number(static_cast<qulonglong>(releaseBlockedCount)),
                    QString::number(static_cast<qulonglong>(candidateSourceCount)),
                    QString::number(static_cast<qulonglong>(candidateEvidenceCount))));
    }
} // namespace TaintedGrailModdingSDK
