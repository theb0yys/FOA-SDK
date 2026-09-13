/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterWorkOrderPlanWidget.h"

#include "FoundationService.h"

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

        QString StepSubject(const AdapterWorkOrderStep& step)
        {
            QStringList values;
            values.push_back(
                QObject::tr("%1:%2").arg(ToQString(step.m_subjectKind), ToQString(step.m_subjectId)));
            if (!step.m_subjectRef.empty())
            {
                values.push_back(QObject::tr("subject: %1").arg(ToQString(step.m_subjectRef)));
            }
            if (!step.m_sourceRecordId.empty())
            {
                values.push_back(QObject::tr("source record: %1").arg(ToQString(step.m_sourceRecordId)));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString StepTarget(const AdapterWorkOrderStep& step)
        {
            QStringList values;
            if (!step.m_relationshipId.empty())
            {
                values.push_back(QObject::tr("relationship: %1").arg(ToQString(step.m_relationshipId)));
            }
            if (!step.m_targetRecordId.empty())
            {
                values.push_back(QObject::tr("target: %1").arg(ToQString(step.m_targetRecordId)));
            }
            if (!step.m_targetSubjectRef.empty())
            {
                values.push_back(
                    QObject::tr("target subject: %1").arg(ToQString(step.m_targetSubjectRef)));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString StepProof(const AdapterWorkOrderStep& step)
        {
            QStringList values;
            if (!step.m_inputEvidenceIds.empty())
            {
                values.push_back(
                    QObject::tr("input evidence: %1").arg(JoinValues(step.m_inputEvidenceIds)));
            }
            if (!step.m_declarationEvidenceIds.empty())
            {
                values.push_back(
                    QObject::tr("adapter evidence: %1")
                        .arg(JoinValues(step.m_declarationEvidenceIds)));
            }
            if (!step.m_permissionEventIds.empty())
            {
                values.push_back(
                    QObject::tr("permission events: %1")
                        .arg(JoinValues(step.m_permissionEventIds)));
            }
            if (!step.m_permissionEvidenceIds.empty())
            {
                values.push_back(
                    QObject::tr("permission evidence: %1")
                        .arg(JoinValues(step.m_permissionEvidenceIds)));
            }
            if (!step.m_validationProofIds.empty())
            {
                values.push_back(
                    QObject::tr("validation proof: %1")
                        .arg(JoinValues(step.m_validationProofIds)));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString RefusalDetails(const AdapterWorkOrderRefusal& refusal)
        {
            QStringList values;
            if (!refusal.m_failedCapabilities.empty())
            {
                values.push_back(
                    QObject::tr("failed capabilities: %1")
                        .arg(JoinValues(refusal.m_failedCapabilities)));
            }
            if (!refusal.m_compatibilityStatuses.empty())
            {
                values.push_back(
                    QObject::tr("compatibility: %1")
                        .arg(JoinValues(refusal.m_compatibilityStatuses)));
            }
            if (!refusal.m_subjectIds.empty())
            {
                values.push_back(QObject::tr("subjects: %1").arg(JoinValues(refusal.m_subjectIds)));
            }
            if (!refusal.m_reasons.empty())
            {
                values.push_back(QObject::tr("reasons: %1").arg(JoinValues(refusal.m_reasons)));
            }
            return values.join(QStringLiteral(" | "));
        }
    } // namespace

    AdapterWorkOrderPlanWidget::AdapterWorkOrderPlanWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Adapter Work-Order Plans"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only canonical plan preview. A plan is generated only when all eleven typed adapter "
                "capabilities are supported and every exact catalog payload passes an independent evidence, "
                "permission, validation, relationship, target, and blocker check. Execution is always prohibited. "
                "This pane cannot save, export, dispatch, deploy, launch, or execute a plan."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 11, this);
        m_table->setHorizontalHeaderLabels({
            tr("Plan"),
            tr("Pack"),
            tr("Adapter"),
            tr("Status"),
            tr("Sequence"),
            tr("Capability"),
            tr("Exact subject"),
            tr("Relationship / target"),
            tr("Typed arguments"),
            tr("Evidence and validation proof"),
            tr("Canonical JSON / refusal reasons") });
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table, 1);

        FoundationNotificationBus::Handler::BusConnect();
        Refresh();
    }

    AdapterWorkOrderPlanWidget::~AdapterWorkOrderPlanWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterWorkOrderPlanWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterWorkOrderPlanWidget::Refresh()
    {
        const FoundationService& foundation = FoundationService::Get();
        const AdapterWorkOrderPlanSet planSet = FoundationService::Get().GetFrameworkPlanners().BuildPlans(
            foundation.GetWorkspace(),
            foundation.GetPacks(),
            AdapterContractRegistry::Get(),
            foundation.GetSourceRegistry(),
            foundation.GetCatalog(),
            foundation.GetSnapshot().m_blockers);

        m_summary->setText(
            tr(
                "Candidates: %1 | generated: %2 | refused: %3 | steps: %4 | execution: %5")
                .arg(QString::number(static_cast<qulonglong>(planSet.m_candidatePlanCount)))
                .arg(QString::number(static_cast<qulonglong>(planSet.m_generatedPlanCount)))
                .arg(QString::number(static_cast<qulonglong>(planSet.m_refusedPlanCount)))
                .arg(QString::number(static_cast<qulonglong>(planSet.m_stepCount)))
                .arg(tr("prohibited")));

        int rowCount = static_cast<int>(planSet.m_refusals.size());
        for (const AdapterWorkOrderPlan& plan : planSet.m_plans)
        {
            rowCount += static_cast<int>(plan.m_steps.size());
        }
        m_table->setRowCount(rowCount);

        int rowIndex = 0;
        for (const AdapterWorkOrderPlan& plan : planSet.m_plans)
        {
            for (const AdapterWorkOrderStep& step : plan.m_steps)
            {
                QStringList arguments;
                for (const AdapterWorkOrderArgument& argument : step.m_arguments)
                {
                    arguments.push_back(
                        QObject::tr("%1=%2")
                            .arg(ToQString(argument.m_key), ToQString(argument.m_value)));
                }
                SetCell(m_table, rowIndex, 0, ToQString(plan.m_planId));
                SetCell(m_table, rowIndex, 1, ToQString(plan.m_packId));
                SetCell(
                    m_table,
                    rowIndex,
                    2,
                    tr("%1 @ %2").arg(ToQString(plan.m_adapterId), ToQString(plan.m_adapterVersion)));
                SetCell(m_table, rowIndex, 3, tr("generated; execution prohibited"));
                SetCell(
                    m_table,
                    rowIndex,
                    4,
                    QString::number(static_cast<qulonglong>(step.m_sequence)));
                SetCell(m_table, rowIndex, 5, ToQString(step.m_capability));
                SetCell(m_table, rowIndex, 6, StepSubject(step));
                SetCell(m_table, rowIndex, 7, StepTarget(step));
                SetCell(m_table, rowIndex, 8, arguments.join(QStringLiteral(", ")));
                SetCell(m_table, rowIndex, 9, StepProof(step));
                SetCell(m_table, rowIndex, 10, ToQString(plan.m_canonicalJson));
                ++rowIndex;
            }
        }

        for (const AdapterWorkOrderRefusal& refusal : planSet.m_refusals)
        {
            SetCell(m_table, rowIndex, 0, ToQString(refusal.m_planId));
            SetCell(m_table, rowIndex, 1, ToQString(refusal.m_packId));
            SetCell(
                m_table,
                rowIndex,
                2,
                tr("%1 @ %2")
                    .arg(ToQString(refusal.m_adapterId), ToQString(refusal.m_adapterVersion)));
            SetCell(m_table, rowIndex, 3, tr("refused; execution prohibited"));
            SetCell(m_table, rowIndex, 5, JoinValues(refusal.m_failedCapabilities));
            SetCell(m_table, rowIndex, 6, JoinValues(refusal.m_subjectIds));
            SetCell(m_table, rowIndex, 10, RefusalDetails(refusal));
            ++rowIndex;
        }
    }
} // namespace TaintedGrailModdingSDK
