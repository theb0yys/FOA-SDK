/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "EconomyCoverageDashboardWidget.h"

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

        const EconomyAcquisitionCoverageLane* FindLane(
            const EconomyAcquisitionCoverageRow& row,
            const AZStd::string& laneName)
        {
            for (const EconomyAcquisitionCoverageLane& lane : row.m_lanes)
            {
                if (lane.m_lane == laneName)
                {
                    return &lane;
                }
            }
            return nullptr;
        }

        QString LaneStatus(const EconomyAcquisitionCoverageRow& row, const AZStd::string& laneName)
        {
            if (const EconomyAcquisitionCoverageLane* lane = FindLane(row, laneName))
            {
                return QStringLiteral("%1 (%2/%3)")
                    .arg(ToQString(lane->m_status))
                    .arg(QString::number(static_cast<qulonglong>(lane->m_coveredRelationshipCount)))
                    .arg(QString::number(static_cast<qulonglong>(lane->m_relationshipCount)));
            }
            return QObject::tr("not applicable");
        }

        QString BuildDetails(const EconomyAcquisitionCoverageRow& row)
        {
            QStringList details;
            for (const EconomyAcquisitionCoverageLane& lane : row.m_lanes)
            {
                const QString laneName = ToQString(lane.m_lane);
                if (!lane.m_relationshipIds.empty())
                {
                    details.push_back(
                        QObject::tr("%1 relationships: %2").arg(laneName, JoinValues(lane.m_relationshipIds)));
                }
                if (!lane.m_evidenceIds.empty())
                {
                    details.push_back(
                        QObject::tr("%1 evidence: %2").arg(laneName, JoinValues(lane.m_evidenceIds)));
                }
                if (!lane.m_blockerIds.empty())
                {
                    details.push_back(
                        QObject::tr("%1 blockers: %2").arg(laneName, JoinValues(lane.m_blockerIds)));
                }
                if (!lane.m_reasons.empty())
                {
                    details.push_back(
                        QObject::tr("%1 notes: %2").arg(laneName, JoinValues(lane.m_reasons)));
                }
            }
            return details.join(QStringLiteral(" | "));
        }
    } // namespace

    EconomyCoverageDashboardWidget::EconomyCoverageDashboardWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Economy Acquisition Coverage"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only coverage for canonical economy items and recipes. Vendor, loot, reward, learnability, "
                "and crafting lanes are derived from catalog relationships, exact evidence binding, governance "
                "state, and open blockers. Coverage does not grant permission or prove runtime behavior."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 10, this);
        m_table->setHorizontalHeaderLabels({
            tr("Record"),
            tr("Kind"),
            tr("Owner"),
            tr("Vendor"),
            tr("Loot"),
            tr("Reward"),
            tr("Learnability"),
            tr("Crafting"),
            tr("Overall"),
            tr("Relationships, evidence, blockers, and notes") });
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table, 1);

        FoundationNotificationBus::Handler::BusConnect();
        Refresh();
    }

    EconomyCoverageDashboardWidget::~EconomyCoverageDashboardWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void EconomyCoverageDashboardWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void EconomyCoverageDashboardWidget::Refresh()
    {
        const FoundationService& foundation = FoundationService::Get();
        const EconomyAcquisitionCoverageSummary coverage = m_coverageService.BuildAcquisitionCoverage(
            foundation.GetSourceRegistry(),
            foundation.GetCatalog(),
            foundation.GetSnapshot().m_blockers);

        m_summary->setText(
            tr("Subjects: %1 | covered: %2 | partial: %3 | blocked: %4 | missing: %5")
                .arg(QString::number(static_cast<qulonglong>(coverage.m_subjectCount)))
                .arg(QString::number(static_cast<qulonglong>(coverage.m_coveredSubjectCount)))
                .arg(QString::number(static_cast<qulonglong>(coverage.m_partialSubjectCount)))
                .arg(QString::number(static_cast<qulonglong>(coverage.m_blockedSubjectCount)))
                .arg(QString::number(static_cast<qulonglong>(coverage.m_missingSubjectCount))));

        m_table->setRowCount(static_cast<int>(coverage.m_rows.size()));
        for (int rowIndex = 0; rowIndex < static_cast<int>(coverage.m_rows.size()); ++rowIndex)
        {
            const EconomyAcquisitionCoverageRow& row = coverage.m_rows[static_cast<size_t>(rowIndex)];
            QString record = ToQString(row.m_recordId);
            if (!row.m_displayName.empty())
            {
                record += QStringLiteral(" — ") + ToQString(row.m_displayName);
            }
            const QString owner = row.m_ownerPackId.empty()
                ? tr("native / no pack owner")
                : ToQString(row.m_ownerPackId);
            const QString overall = tr("%1 | validation: %2 | staleness: %3")
                .arg(ToQString(row.m_overallStatus))
                .arg(ToQString(row.m_validationState))
                .arg(ToQString(row.m_stalenessState));

            SetCell(m_table, rowIndex, 0, record);
            SetCell(m_table, rowIndex, 1, ToQString(row.m_recordKind));
            SetCell(m_table, rowIndex, 2, owner);
            SetCell(m_table, rowIndex, 3, LaneStatus(row, "vendor"));
            SetCell(m_table, rowIndex, 4, LaneStatus(row, "loot"));
            SetCell(m_table, rowIndex, 5, LaneStatus(row, "reward"));
            SetCell(m_table, rowIndex, 6, LaneStatus(row, "learnability"));
            SetCell(m_table, rowIndex, 7, LaneStatus(row, "crafting"));
            SetCell(m_table, rowIndex, 8, overall);
            SetCell(m_table, rowIndex, 9, BuildDetails(row));
        }
    }
} // namespace TaintedGrailModdingSDK
