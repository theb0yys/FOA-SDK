/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterRuntimeResultEvidenceWidget.h"

#include "AdapterContractRegistry.h"
#include "FoundationService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

#include <QAbstractItemView>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QObject>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstddef>

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

        void SetCell(QTableWidget* table, int row, int column, const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }

        const AdapterWorkOrderPlan* FindPlan(
            const AdapterWorkOrderPlanSet& plans,
            const AZStd::string& planId)
        {
            for (const AdapterWorkOrderPlan& plan : plans.m_plans)
            {
                if (plan.m_planId == planId)
                {
                    return &plan;
                }
            }
            return nullptr;
        }

        QString IssueDetails(const AdapterRuntimeEvidenceReturn& evidenceReturn)
        {
            QStringList values;
            for (const AdapterRuntimeResultIssue& issue : evidenceReturn.m_issues)
            {
                QString value = ToQString(issue.m_code) + QStringLiteral(": ")
                    + ToQString(issue.m_message);
                if (!issue.m_stepId.empty())
                {
                    value += QStringLiteral(" [") + ToQString(issue.m_stepId) + QStringLiteral("]");
                }
                values.push_back(value);
            }
            return values.join(QStringLiteral(" | "));
        }

        QString FailureDetails(
            const AdapterRuntimeResultEnvelope& envelope,
            const AdapterRuntimeStepResult& step)
        {
            QStringList values;
            for (const AZStd::string& failureId : step.m_failureIds)
            {
                for (const AdapterRuntimeFailure& failure : envelope.m_failures)
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
            return values.join(QStringLiteral(" | "));
        }

        QString RecoveryDetails(
            const AdapterRuntimeResultEnvelope& envelope,
            const AdapterRuntimeStepResult& step)
        {
            const AdapterRuntimeRecoveryResult* recovery = nullptr;
            QString name;
            if (step.m_stepId == envelope.m_cleanupResult.m_stepId)
            {
                recovery = &envelope.m_cleanupResult;
                name = QObject::tr("cleanup");
            }
            else if (step.m_stepId == envelope.m_rollbackResult.m_stepId)
            {
                recovery = &envelope.m_rollbackResult;
                name = QObject::tr("rollback");
            }
            if (!recovery)
            {
                return {};
            }
            return QObject::tr("%1: %2 | failures: %3 | logs: %4")
                .arg(
                    name,
                    ToQString(ToString(recovery->m_outcome)),
                    JoinValues(recovery->m_failureIds),
                    JoinValues(recovery->m_logReferenceIds));
        }

        QString LogDetails(
            const AdapterRuntimeResultEnvelope& envelope,
            const AdapterRuntimeStepResult& step)
        {
            QStringList values;
            for (const AZStd::string& logId : step.m_logReferenceIds)
            {
                for (const AdapterRuntimeLogReference& log : envelope.m_logReferences)
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
    } // namespace

    AdapterRuntimeResultEvidenceWidget::AdapterRuntimeResultEvidenceWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Adapter Runtime Result Evidence"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 7 contract verification for externally supplied runtime-result envelopes. "
                "Exact attempted plan and step identities, typed outcomes, failures, cleanup and rollback, "
                "safe log references, and fingerprints are converted into candidate source/evidence documents. "
                "Nothing is imported, persisted, promoted, dispatched, executed, deployed, launched, or written "
                "to a game or save by this pane."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 10, this);
        m_table->setHorizontalHeaderLabels({
            tr("Result"),
            tr("Plan"),
            tr("Contract status"),
            tr("Attempted step"),
            tr("Outcome"),
            tr("Failures"),
            tr("Cleanup / rollback"),
            tr("Logs and fingerprints"),
            tr("Evidence candidates"),
            tr("Issues") });
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table, 1);

        FoundationNotificationBus::Handler::BusConnect();
        Refresh();
    }

    AdapterRuntimeResultEvidenceWidget::~AdapterRuntimeResultEvidenceWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterRuntimeResultEvidenceWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterRuntimeResultEvidenceWidget::Refresh()
    {
        const FoundationService& foundation = FoundationService::Get();
        const AdapterWorkOrderPlanSet plans = m_planningService.BuildPlans(
            foundation.GetWorkspace(),
            foundation.GetPacks(),
            AdapterContractRegistry::Get(),
            foundation.GetSourceRegistry(),
            foundation.GetCatalog(),
            foundation.GetSnapshot().m_blockers);
        const AZStd::vector<AdapterRuntimeResultEnvelope>& envelopes =
            AdapterRuntimeResultRegistry::Get().GetEnvelopes();

        AZ::u64 acceptedCount = 0;
        AZ::u64 rejectedCount = 0;
        AZ::u64 sourceCandidateCount = 0;
        AZ::u64 evidenceCandidateCount = 0;
        int rowCount = 0;
        for (const AdapterRuntimeResultEnvelope& envelope : envelopes)
        {
            const AdapterWorkOrderPlan* plan = FindPlan(plans, envelope.m_planId);
            if (!plan)
            {
                ++rejectedCount;
                ++rowCount;
                continue;
            }
            const AdapterRuntimeEvidenceReturn evidenceReturn =
                m_resultService.BuildEvidenceReturn(*plan, envelope);
            acceptedCount += evidenceReturn.m_accepted ? 1 : 0;
            rejectedCount += evidenceReturn.m_accepted ? 0 : 1;
            sourceCandidateCount += evidenceReturn.m_sourceDocumentCount;
            evidenceCandidateCount += evidenceReturn.m_evidenceRecordCount;
            rowCount += evidenceReturn.m_accepted
                ? static_cast<int>(envelope.m_stepResults.size())
                : 1;
        }

        m_summary->setText(
            tr(
                "Registered envelopes: %1 | accepted contracts: %2 | rejected contracts: %3 | "
                "candidate source documents: %4 | candidate evidence records: %5 | execution: prohibited")
                .arg(QString::number(static_cast<qulonglong>(envelopes.size())))
                .arg(QString::number(static_cast<qulonglong>(acceptedCount)))
                .arg(QString::number(static_cast<qulonglong>(rejectedCount)))
                .arg(QString::number(static_cast<qulonglong>(sourceCandidateCount)))
                .arg(QString::number(static_cast<qulonglong>(evidenceCandidateCount))));

        m_table->setRowCount(rowCount);
        int rowIndex = 0;
        for (const AdapterRuntimeResultEnvelope& envelope : envelopes)
        {
            const AdapterWorkOrderPlan* plan = FindPlan(plans, envelope.m_planId);
            if (!plan)
            {
                SetCell(m_table, rowIndex, 0, ToQString(envelope.m_resultId));
                SetCell(m_table, rowIndex, 1, ToQString(envelope.m_planId));
                SetCell(m_table, rowIndex, 2, tr("rejected; no current canonical plan"));
                SetCell(
                    m_table,
                    rowIndex,
                    7,
                    tr("plan %1 | result %2")
                        .arg(
                            ToQString(envelope.m_planFingerprint),
                            ToQString(envelope.m_resultFingerprint)));
                SetCell(
                    m_table,
                    rowIndex,
                    9,
                    tr("No generated canonical plan matches this exact result envelope."));
                ++rowIndex;
                continue;
            }

            const AdapterRuntimeEvidenceReturn evidenceReturn =
                m_resultService.BuildEvidenceReturn(*plan, envelope);
            if (!evidenceReturn.m_accepted)
            {
                SetCell(m_table, rowIndex, 0, ToQString(envelope.m_resultId));
                SetCell(m_table, rowIndex, 1, ToQString(envelope.m_planId));
                SetCell(m_table, rowIndex, 2, tr("rejected; contract mismatch"));
                SetCell(
                    m_table,
                    rowIndex,
                    7,
                    tr("plan %1 | result %2")
                        .arg(
                            ToQString(envelope.m_planFingerprint),
                            ToQString(envelope.m_resultFingerprint)));
                SetCell(m_table, rowIndex, 9, IssueDetails(evidenceReturn));
                ++rowIndex;
                continue;
            }

            AZStd::vector<const AdapterRuntimeStepResult*> steps;
            for (const AdapterRuntimeStepResult& step : envelope.m_stepResults)
            {
                steps.push_back(&step);
            }
            AZStd::sort(
                steps.begin(),
                steps.end(),
                [](const AdapterRuntimeStepResult* left, const AdapterRuntimeStepResult* right)
                {
                    return left->m_sequence < right->m_sequence;
                });
            for (const AdapterRuntimeStepResult* step : steps)
            {
                SetCell(m_table, rowIndex, 0, ToQString(envelope.m_resultId));
                SetCell(m_table, rowIndex, 1, ToQString(envelope.m_planId));
                SetCell(m_table, rowIndex, 2, tr("accepted; evidence candidates only"));
                SetCell(
                    m_table,
                    rowIndex,
                    3,
                    tr("%1 | %2 | %3:%4")
                        .arg(
                            ToQString(step->m_stepId),
                            ToQString(step->m_capability),
                            ToQString(step->m_subjectKind),
                            ToQString(step->m_subjectId)));
                SetCell(m_table, rowIndex, 4, ToQString(ToString(step->m_outcome)));
                SetCell(m_table, rowIndex, 5, FailureDetails(envelope, *step));
                SetCell(m_table, rowIndex, 6, RecoveryDetails(envelope, *step));
                SetCell(m_table, rowIndex, 7, LogDetails(envelope, *step));
                SetCell(
                    m_table,
                    rowIndex,
                    8,
                    tr("sources: %1 | evidence: %2 | output: %3")
                        .arg(
                            QString::number(
                                static_cast<qulonglong>(evidenceReturn.m_sourceDocumentCount)),
                            QString::number(
                                static_cast<qulonglong>(evidenceReturn.m_evidenceRecordCount)),
                            ToQString(step->m_outputFingerprint)));
                ++rowIndex;
            }
        }
    }
} // namespace TaintedGrailModdingSDK
