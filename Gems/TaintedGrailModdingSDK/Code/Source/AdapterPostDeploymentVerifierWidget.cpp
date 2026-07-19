/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPostDeploymentVerifierWidget.h"

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
        QString ToVerifierQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        QString JoinVerifierValues(const AZStd::vector<AZStd::string>& values)
        {
            QStringList result;
            for (const AZStd::string& value : values)
            {
                result.push_back(ToVerifierQString(value));
            }
            return result.join(QStringLiteral(", "));
        }

        void SetVerifierCell(
            QTableWidget* table,
            int row,
            int column,
            const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }

        const AdapterDeploymentWorkOrder* FindVerifierWorkOrderForPane(
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

        const AdapterDeploymentExecutionResultEnvelope*
        FindExecutionEnvelopeForPane(
            const AZStd::vector<AdapterDeploymentExecutionResultEnvelope>& envelopes,
            const AZStd::string& resultId)
        {
            for (const AdapterDeploymentExecutionResultEnvelope& envelope : envelopes)
            {
                if (envelope.m_resultId == resultId)
                {
                    return &envelope;
                }
            }
            return nullptr;
        }

        QString VerifierIssueDetails(
            const AdapterPostDeploymentVerifierEvidenceReturn& result)
        {
            QStringList values;
            for (const AdapterPostDeploymentVerifierIssue& issue : result.m_issues)
            {
                QString value = ToVerifierQString(issue.m_code)
                    + QStringLiteral(": ")
                    + ToVerifierQString(issue.m_message);
                if (!issue.m_stepId.empty())
                {
                    value += QStringLiteral(" [step=")
                        + ToVerifierQString(issue.m_stepId)
                        + QStringLiteral("]");
                }
                if (!issue.m_checkId.empty())
                {
                    value += QStringLiteral(" [check=")
                        + ToVerifierQString(issue.m_checkId)
                        + QStringLiteral("]");
                }
                if (!issue.m_failureId.empty())
                {
                    value += QStringLiteral(" [failure=")
                        + ToVerifierQString(issue.m_failureId)
                        + QStringLiteral("]");
                }
                if (!issue.m_diagnosticId.empty())
                {
                    value += QStringLiteral(" [diagnostic=")
                        + ToVerifierQString(issue.m_diagnosticId)
                        + QStringLiteral("]");
                }
                values.push_back(value);
            }
            return values.join(QStringLiteral(" | "));
        }

        QString VerifierReviewDetails(
            const AdapterPostDeploymentVerifierReview& review)
        {
            QStringList capabilities;
            for (AdapterPostDeploymentVerifierCapability capability :
                review.m_capabilities)
            {
                capabilities.push_back(ToVerifierQString(ToString(capability)));
            }
            return QObject::tr("%1 %2 | review=%3/%4 | capabilities=%5")
                .arg(
                    ToVerifierQString(review.m_verifierId),
                    ToVerifierQString(review.m_verifierVersion),
                    ToVerifierQString(review.m_reviewId),
                    ToVerifierQString(ToString(review.m_decision)),
                    capabilities.join(QStringLiteral(", ")));
        }

        QString VerifierFailureDiagnosticDetails(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            const AdapterPostDeploymentVerifierCheckResult& check)
        {
            QStringList values;
            for (const AZStd::string& failureId : check.m_failureIds)
            {
                for (const AdapterPostDeploymentVerifierFailure& failure :
                    envelope.m_failures)
                {
                    if (failure.m_failureId == failureId)
                    {
                        values.push_back(
                            QObject::tr("%1 [%2/%3]: %4")
                                .arg(
                                    ToVerifierQString(failure.m_failureId),
                                    ToVerifierQString(ToString(failure.m_kind)),
                                    ToVerifierQString(failure.m_code),
                                    ToVerifierQString(failure.m_message)));
                    }
                }
            }
            for (const AZStd::string& diagnosticId :
                check.m_diagnosticReferenceIds)
            {
                for (const AdapterPostDeploymentVerifierDiagnosticReference& diagnostic :
                    envelope.m_diagnosticReferences)
                {
                    if (diagnostic.m_diagnosticId == diagnosticId)
                    {
                        values.push_back(
                            QObject::tr("%1 [%2] %3 | %4")
                                .arg(
                                    ToVerifierQString(diagnostic.m_diagnosticId),
                                    ToVerifierQString(ToString(diagnostic.m_kind)),
                                    ToVerifierQString(diagnostic.m_reference),
                                    ToVerifierQString(diagnostic.m_fingerprint)));
                    }
                }
            }
            return values.join(QStringLiteral(" | "));
        }
    } // namespace

    AdapterPostDeploymentVerifierWidget::AdapterPostDeploymentVerifierWidget(
        QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Independent Post-Deployment Verifier Results"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 validation of metadata supplied by one separately "
                "reviewed independent verifier. Exact report, work-order, execution-result, "
                "target, expected-state, observation, failure, and safe diagnostic bindings "
                "are checked and returned as candidate evidence. The Editor does not run a "
                "verifier, read or mutate target files, launch FoA, call an adapter, promote "
                "evidence, sign an archive, or publish a release."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 10, this);
        m_table->setHorizontalHeaderLabels({
            tr("Verifier result / report"),
            tr("Contract status"),
            tr("Reviewed verifier"),
            tr("Expected check"),
            tr("Independent observation"),
            tr("Failures / diagnostics"),
            tr("Candidate evidence"),
            tr("Exact context"),
            tr("Safety boundary"),
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

    AdapterPostDeploymentVerifierWidget::~AdapterPostDeploymentVerifierWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterPostDeploymentVerifierWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterPostDeploymentVerifierWidget::Refresh()
    {
        AZStd::vector<AdapterDeploymentWorkOrder> workOrders;
        for (const AdapterDeploymentWorkOrderRequest& request :
            AdapterDeploymentWorkOrderRegistry::Get().GetRequests())
        {
            workOrders.push_back(m_workOrderService.BuildWorkOrder(request));
        }

        const AZStd::vector<AdapterDeploymentExecutionResultEnvelope>&
            executionEnvelopes =
                AdapterDeploymentExecutionResultRegistry::Get().GetEnvelopes();
        const AZStd::vector<AdapterPostDeploymentVerifierResultEnvelope>&
            verifierEnvelopes =
                AdapterPostDeploymentVerifierResultRegistry::Get().GetEnvelopes();

        int rowCount = 0;
        for (const AdapterPostDeploymentVerifierResultEnvelope& envelope :
            verifierEnvelopes)
        {
            rowCount += envelope.m_checkResults.empty()
                ? 1
                : static_cast<int>(envelope.m_checkResults.size());
        }
        m_table->setRowCount(rowCount);

        AZ::u64 acceptedCount = 0;
        AZ::u64 observationMismatchCount = 0;
        AZ::u64 rejectedCount = 0;
        AZ::u64 candidateSourceCount = 0;
        AZ::u64 candidateEvidenceCount = 0;
        int rowIndex = 0;

        for (const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope :
            verifierEnvelopes)
        {
            const AdapterDeploymentWorkOrder* workOrder =
                FindVerifierWorkOrderForPane(
                    workOrders,
                    verifierEnvelope.m_workOrderId);
            const AdapterDeploymentExecutionResultEnvelope* executionEnvelope =
                FindExecutionEnvelopeForPane(
                    executionEnvelopes,
                    verifierEnvelope.m_resultId);
            if (!workOrder || !executionEnvelope)
            {
                const int missingRows = verifierEnvelope.m_checkResults.empty()
                    ? 1
                    : static_cast<int>(verifierEnvelope.m_checkResults.size());
                for (int missingIndex = 0; missingIndex < missingRows; ++missingIndex)
                {
                    SetVerifierCell(
                        m_table,
                        rowIndex,
                        0,
                        ToVerifierQString(verifierEnvelope.m_verifierResultId));
                    SetVerifierCell(
                        m_table,
                        rowIndex,
                        1,
                        tr("report_binding_mismatch"));
                    SetVerifierCell(
                        m_table,
                        rowIndex,
                        2,
                        VerifierReviewDetails(verifierEnvelope.m_verifierReview));
                    SetVerifierCell(
                        m_table,
                        rowIndex,
                        9,
                        tr("No exact current work order or execution-result envelope matches "
                           "this independently supplied verifier result."));
                    ++rowIndex;
                }
                ++rejectedCount;
                continue;
            }

            const AdapterDeploymentExecutionEvidenceReturn executionEvidence =
                m_executionEvidenceService.BuildEvidenceReturn(
                    *workOrder,
                    *executionEnvelope);
            const AdapterPostDeploymentVerificationReport report =
                m_reportService.BuildReport(
                    *workOrder,
                    *executionEnvelope,
                    executionEvidence);
            const AdapterPostDeploymentVerifierEvidenceReturn result =
                m_verifierService.BuildEvidenceReturn(
                    *workOrder,
                    *executionEnvelope,
                    report,
                    verifierEnvelope);

            acceptedCount += result.m_accepted ? 1 : 0;
            observationMismatchCount += result.m_status
                    == AdapterPostDeploymentVerifierEnvelopeStatus::ObservationMismatch
                ? 1
                : 0;
            rejectedCount += result.m_contractValid ? 0 : 1;
            candidateSourceCount += result.m_sourceDocumentCount;
            candidateEvidenceCount += result.m_evidenceRecordCount;

            if (verifierEnvelope.m_checkResults.empty())
            {
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    0,
                    tr("%1 | %2")
                        .arg(
                            ToVerifierQString(result.m_verifierResultId),
                            ToVerifierQString(result.m_reportId)));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    1,
                    ToVerifierQString(ToString(result.m_status)));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    2,
                    VerifierReviewDetails(verifierEnvelope.m_verifierReview));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    6,
                    tr("sources=%1 | evidence=%2 | promoted=no")
                        .arg(
                            QString::number(static_cast<qulonglong>(
                                result.m_sourceDocumentCount)),
                            QString::number(static_cast<qulonglong>(
                                result.m_evidenceRecordCount))));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    9,
                    VerifierIssueDetails(result));
                ++rowIndex;
                continue;
            }

            for (const AdapterPostDeploymentVerifierCheckResult& check :
                verifierEnvelope.m_checkResults)
            {
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    0,
                    tr("%1 | %2")
                        .arg(
                            ToVerifierQString(result.m_verifierResultId),
                            ToVerifierQString(result.m_reportId)));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    1,
                    tr("%1 | contract valid=%2 | all matched=%3")
                        .arg(
                            ToVerifierQString(ToString(result.m_status)),
                            result.m_contractValid ? tr("yes") : tr("no"),
                            result.m_accepted ? tr("yes") : tr("no")));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    2,
                    VerifierReviewDetails(verifierEnvelope.m_verifierReview));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    3,
                    tr("%1 | step=%2 | sequence=%3 | expected=%4/%5")
                        .arg(
                            ToVerifierQString(check.m_checkId),
                            ToVerifierQString(check.m_stepId),
                            QString::number(static_cast<qulonglong>(check.m_sequence)),
                            check.m_expectedPresent ? tr("present") : tr("absent"),
                            ToVerifierQString(check.m_expectedFingerprint)));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    4,
                    tr("%1 | attempted=%2 | recorded=%3 | observed=%4/%5 | at=%6")
                        .arg(
                            ToVerifierQString(ToString(check.m_outcome)),
                            check.m_attempted ? tr("yes") : tr("no"),
                            check.m_observationRecorded ? tr("yes") : tr("no"),
                            check.m_observedPresent ? tr("present") : tr("absent"),
                            ToVerifierQString(check.m_observedFingerprint),
                            ToVerifierQString(check.m_checkedAtUtc)));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    5,
                    VerifierFailureDiagnosticDetails(verifierEnvelope, check));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    6,
                    tr("sources=%1 | evidence=%2 | confidence=unrated | promoted=no")
                        .arg(
                            QString::number(static_cast<qulonglong>(
                                result.m_sourceDocumentCount)),
                            QString::number(static_cast<qulonglong>(
                                result.m_evidenceRecordCount))));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    7,
                    tr("work order=%1 | execution result=%2 | profile=%3 | game=%4 | "
                       "branch=%5 | runtime=%6")
                        .arg(
                            ToVerifierQString(verifierEnvelope.m_workOrderId),
                            ToVerifierQString(verifierEnvelope.m_resultId),
                            ToVerifierQString(verifierEnvelope.m_profileId),
                            ToVerifierQString(verifierEnvelope.m_gameVersion),
                            ToVerifierQString(verifierEnvelope.m_branch),
                            ToVerifierQString(verifierEnvelope.m_runtimeTarget)));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    8,
                    tr("verifier run by Editor=no | target read/mutation=no | FoA launch=no "
                       "| adapter call=no | evidence promotion=no | signing/publication=no"));
                SetVerifierCell(
                    m_table,
                    rowIndex,
                    9,
                    VerifierIssueDetails(result));
                ++rowIndex;
            }
        }

        m_summary->setText(
            tr(
                "Registered independent-verifier envelopes: %1 | accepted all-matched "
                "results: %2 | structurally valid observation mismatches: %3 | rejected "
                "contracts: %4 | candidate sources: %5 | candidate evidence: %6 | verifier "
                "execution, target filesystem access, FoA launch, adapter calls, evidence "
                "promotion, archive signing, and release publication: prohibited")
                .arg(
                    QString::number(static_cast<qulonglong>(
                        verifierEnvelopes.size())),
                    QString::number(static_cast<qulonglong>(acceptedCount)),
                    QString::number(static_cast<qulonglong>(
                        observationMismatchCount)),
                    QString::number(static_cast<qulonglong>(rejectedCount)),
                    QString::number(static_cast<qulonglong>(candidateSourceCount)),
                    QString::number(static_cast<qulonglong>(candidateEvidenceCount))));
    }
} // namespace TaintedGrailModdingSDK
