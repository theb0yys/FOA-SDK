/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterVerifierEvidenceReconciliationWidget.h"

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

        const AdapterDeploymentExecutionResultEnvelope* FindExecutionEnvelope(
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

        const AdapterPostDeploymentVerifierResultEnvelope* FindVerifierEnvelope(
            const AZStd::vector<AdapterPostDeploymentVerifierResultEnvelope>& envelopes,
            const AZStd::string& verifierResultId)
        {
            for (const AdapterPostDeploymentVerifierResultEnvelope& envelope : envelopes)
            {
                if (envelope.m_verifierResultId == verifierResultId)
                {
                    return &envelope;
                }
            }
            return nullptr;
        }

        QString IssueDetails(
            const AdapterVerifierEvidenceReconciliationResult& result)
        {
            QStringList values;
            for (const AdapterVerifierEvidenceReconciliationIssue& issue :
                 result.m_issues)
            {
                QString value = ToQString(issue.m_code)
                    + QStringLiteral(": ")
                    + ToQString(issue.m_message);
                if (!issue.m_findingId.empty())
                {
                    value += QStringLiteral(" [finding=")
                        + ToQString(issue.m_findingId)
                        + QStringLiteral("]");
                }
                if (!issue.m_reportBlockerId.empty())
                {
                    value += QStringLiteral(" [blocker=")
                        + ToQString(issue.m_reportBlockerId)
                        + QStringLiteral("]");
                }
                if (!issue.m_checkId.empty())
                {
                    value += QStringLiteral(" [check=")
                        + ToQString(issue.m_checkId)
                        + QStringLiteral("]");
                }
                if (!issue.m_dispositionFindingId.empty())
                {
                    value += QStringLiteral(" [disposition=")
                        + ToQString(issue.m_dispositionFindingId)
                        + QStringLiteral("]");
                }
                values.push_back(value);
            }
            return values.join(QStringLiteral(" | "));
        }

        QString FindingDetails(
            const AdapterVerifierReconciliationFinding& finding)
        {
            return QObject::tr(
                       "%1 | %2 | relationship=%3 | subject=%4 | step=%5 | check=%6 | "
                       "compatibility blocker=%7 | release blocker=%8 | disposition=%9")
                .arg(
                    ToQString(finding.m_findingId),
                    ToQString(ToString(finding.m_kind)),
                    ToQString(ToString(finding.m_relationship)),
                    ToQString(finding.m_subjectRef),
                    ToQString(finding.m_stepId),
                    ToQString(finding.m_checkId),
                    finding.m_blocksCompatibility ? QObject::tr("yes")
                                                  : QObject::tr("no"),
                    finding.m_blocksRelease ? QObject::tr("yes")
                                            : QObject::tr("no"),
                    finding.m_requiresHumanDisposition ? QObject::tr("required")
                                                       : QObject::tr("not required"));
        }

        QString ReviewDetails(const AdapterVerifierReleaseReview& review)
        {
            return QObject::tr("%1 | decision=%2 | reviewer=%3 | reviewed=%4")
                .arg(
                    ToQString(review.m_reviewId),
                    ToQString(ToString(review.m_decision)),
                    ToQString(review.m_reviewer),
                    ToQString(review.m_reviewedAtUtc));
        }
    } // namespace

    AdapterVerifierEvidenceReconciliationWidget::
        AdapterVerifierEvidenceReconciliationWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Verifier Evidence Reconciliation and Release Decision"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 reconciliation of one exact post-deployment report, "
                "one structurally valid independent-verifier evidence return, and one "
                "explicit named human release review. Existing blockers and adverse "
                "observations are preserved. Matching metadata never grants approval. "
                "Nothing is executed, accessed, mutated, promoted, assembled, signed, "
                "launched, or published."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 11, this);
        m_table->setHorizontalHeaderLabels({
            tr("Reconciliation / report / verifier"),
            tr("Contract status"),
            tr("Compatibility assessment"),
            tr("Release decision"),
            tr("Human review"),
            tr("Finding"),
            tr("Blocker counts"),
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

    AdapterVerifierEvidenceReconciliationWidget::
        ~AdapterVerifierEvidenceReconciliationWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterVerifierEvidenceReconciliationWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterVerifierEvidenceReconciliationWidget::Refresh()
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
        const AZStd::vector<AdapterVerifierEvidenceReconciliationRequest>& requests =
            AdapterVerifierEvidenceReconciliationRegistry::Get().GetRequests();

        int rowCount = 0;
        for (const AdapterVerifierEvidenceReconciliationRequest& request : requests)
        {
            const AdapterPostDeploymentVerifierResultEnvelope* verifierEnvelope =
                FindVerifierEnvelope(
                    verifierEnvelopes,
                    request.m_releaseReview.m_verifierResultId);
            rowCount += verifierEnvelope && !verifierEnvelope->m_checkResults.empty()
                ? static_cast<int>(verifierEnvelope->m_checkResults.size())
                : 1;
        }
        m_table->setRowCount(rowCount);

        AZ::u64 acceptedCount = 0;
        AZ::u64 approvedCount = 0;
        AZ::u64 holdCount = 0;
        AZ::u64 rejectedCount = 0;
        int rowIndex = 0;

        for (const AdapterVerifierEvidenceReconciliationRequest& request : requests)
        {
            const AdapterPostDeploymentVerifierResultEnvelope* verifierEnvelope =
                FindVerifierEnvelope(
                    verifierEnvelopes,
                    request.m_releaseReview.m_verifierResultId);
            if (!verifierEnvelope)
            {
                SetCell(
                    m_table,
                    rowIndex,
                    0,
                    ToQString(request.m_reconciliationId));
                SetCell(
                    m_table,
                    rowIndex,
                    1,
                    tr("binding_mismatch"));
                SetCell(
                    m_table,
                    rowIndex,
                    4,
                    ReviewDetails(request.m_releaseReview));
                SetCell(
                    m_table,
                    rowIndex,
                    10,
                    tr("No exact current independent-verifier envelope matches the "
                       "release review binding."));
                ++rowIndex;
                ++rejectedCount;
                continue;
            }

            const AdapterDeploymentExecutionResultEnvelope* executionEnvelope =
                FindExecutionEnvelope(
                    executionEnvelopes,
                    verifierEnvelope->m_resultId);
            const AdapterDeploymentWorkOrder* workOrder = executionEnvelope
                ? FindWorkOrder(workOrders, executionEnvelope->m_workOrderId)
                : nullptr;
            if (!executionEnvelope || !workOrder)
            {
                SetCell(
                    m_table,
                    rowIndex,
                    0,
                    ToQString(request.m_reconciliationId));
                SetCell(
                    m_table,
                    rowIndex,
                    1,
                    tr("binding_mismatch"));
                SetCell(
                    m_table,
                    rowIndex,
                    4,
                    ReviewDetails(request.m_releaseReview));
                SetCell(
                    m_table,
                    rowIndex,
                    10,
                    tr("No exact current work order or execution-result envelope matches "
                       "the verifier evidence."));
                ++rowIndex;
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
            const AdapterPostDeploymentVerifierEvidenceReturn verifierEvidence =
                m_verifierService.BuildEvidenceReturn(
                    *workOrder,
                    *executionEnvelope,
                    report,
                    *verifierEnvelope);
            const AdapterVerifierEvidenceReconciliationResult result =
                m_reconciliationService.BuildReconciliation(
                    *workOrder,
                    *executionEnvelope,
                    report,
                    *verifierEnvelope,
                    verifierEvidence,
                    request);

            acceptedCount += result.m_accepted ? 1 : 0;
            approvedCount += result.m_envelope.m_releaseDecision
                    == AdapterVerifierReleaseDecision::Approved
                ? 1
                : 0;
            holdCount += result.m_envelope.m_releaseDecision
                    == AdapterVerifierReleaseDecision::Hold
                ? 1
                : 0;
            rejectedCount += result.m_accepted ? 0 : 1;

            const int findingRows = result.m_envelope.m_findings.empty()
                ? 1
                : static_cast<int>(result.m_envelope.m_findings.size());
            for (int findingIndex = 0; findingIndex < findingRows; ++findingIndex)
            {
                SetCell(
                    m_table,
                    rowIndex,
                    0,
                    tr("%1 | report=%2 | verifier=%3")
                        .arg(
                            ToQString(result.m_envelope.m_reconciliationId),
                            ToQString(result.m_envelope.m_reportId),
                            ToQString(result.m_envelope.m_verifierResultId)));
                SetCell(
                    m_table,
                    rowIndex,
                    1,
                    ToQString(ToString(result.m_envelope.m_status)));
                SetCell(
                    m_table,
                    rowIndex,
                    2,
                    ToQString(ToString(
                        result.m_envelope.m_compatibilityAssessment)));
                SetCell(
                    m_table,
                    rowIndex,
                    3,
                    ToQString(ToString(result.m_envelope.m_releaseDecision)));
                SetCell(
                    m_table,
                    rowIndex,
                    4,
                    tr("%1 | state=%2 | dispositions=%3/%4")
                        .arg(
                            ReviewDetails(result.m_envelope.m_releaseReview),
                            ToQString(ToString(
                                result.m_envelope.m_humanReviewState)),
                            QString::number(static_cast<qulonglong>(
                                result.m_envelope.m_completedDispositionCount)),
                            QString::number(static_cast<qulonglong>(
                                result.m_envelope.m_requiredDispositionCount))));
                if (!result.m_envelope.m_findings.empty())
                {
                    SetCell(
                        m_table,
                        rowIndex,
                        5,
                        FindingDetails(
                            result.m_envelope.m_findings[findingIndex]));
                }
                SetCell(
                    m_table,
                    rowIndex,
                    6,
                    tr("report=%1 | compatibility=%2 | release=%3")
                        .arg(
                            QString::number(static_cast<qulonglong>(
                                result.m_envelope.m_reportBlockerCount)),
                            QString::number(static_cast<qulonglong>(
                                result.m_envelope.m_compatibilityBlockerCount)),
                            QString::number(static_cast<qulonglong>(
                                result.m_envelope.m_releaseBlockerCount))));
                SetCell(
                    m_table,
                    rowIndex,
                    7,
                    tr("input sources=%1 | input evidence=%2 | returned sources=%3 | "
                       "returned evidence=%4 | promoted=no")
                        .arg(
                            QString::number(static_cast<qulonglong>(
                                result.m_envelope.m_inputCandidateSourceIds.size())),
                            QString::number(static_cast<qulonglong>(
                                result.m_envelope.m_inputCandidateEvidenceIds.size())),
                            QString::number(static_cast<qulonglong>(
                                result.m_sourceDocumentCount)),
                            QString::number(static_cast<qulonglong>(
                                result.m_evidenceRecordCount))));
                SetCell(
                    m_table,
                    rowIndex,
                    8,
                    tr("work order=%1 | execution=%2 | pack=%3 | preview=%4 | target=%5 | "
                       "profile=%6 | game=%7 | branch=%8 | runtime=%9")
                        .arg(
                            ToQString(result.m_envelope.m_workOrderId),
                            ToQString(result.m_envelope.m_executionResultId),
                            ToQString(result.m_envelope.m_packId),
                            ToQString(result.m_envelope.m_previewId),
                            ToQString(result.m_envelope.m_targetInventoryId),
                            ToQString(result.m_envelope.m_profileId),
                            ToQString(result.m_envelope.m_gameVersion),
                            ToQString(result.m_envelope.m_branch),
                            ToQString(result.m_envelope.m_runtimeTarget)));
                SetCell(
                    m_table,
                    rowIndex,
                    9,
                    tr("verifier execution=no | target access=no | file mutation=no | "
                       "evidence promotion=no | archive assembly/signing=no | "
                       "release publication=no | FoA launch=no | adapter call=no"));
                SetCell(
                    m_table,
                    rowIndex,
                    10,
                    IssueDetails(result));
                ++rowIndex;
            }
        }

        m_summary->setText(
            tr(
                "Registered reconciliation requests: %1 | accepted contracts: %2 | "
                "human approvals: %3 | holds: %4 | rejected or incomplete contracts: %5 | "
                "automatic approval, verifier execution, target access, mutation, evidence "
                "promotion, archive assembly/signing, launch, adapter call, and release "
                "publication: prohibited")
                .arg(
                    QString::number(static_cast<qulonglong>(requests.size())),
                    QString::number(static_cast<qulonglong>(acceptedCount)),
                    QString::number(static_cast<qulonglong>(approvedCount)),
                    QString::number(static_cast<qulonglong>(holdCount)),
                    QString::number(static_cast<qulonglong>(rejectedCount))));
    }
} // namespace TaintedGrailModdingSDK
