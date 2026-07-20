/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "EconomyDuplicateReportWidget.h"

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

        QString CandidateDetails(const EconomyDuplicateGroup& group)
        {
            QStringList details;
            for (const EconomyDuplicateCandidate& candidate : group.m_candidates)
            {
                QString candidateText = QObject::tr("%1 -> %2 [%3]")
                    .arg(ToQString(candidate.m_ownerPackId))
                    .arg(ToQString(candidate.m_recordId))
                    .arg(ToQString(candidate.m_status));
                if (!candidate.m_evidenceIds.empty())
                {
                    candidateText += QObject::tr(" evidence: %1").arg(JoinValues(candidate.m_evidenceIds));
                }
                if (!candidate.m_blockerIds.empty())
                {
                    candidateText += QObject::tr(" blockers: %1").arg(JoinValues(candidate.m_blockerIds));
                }
                if (!candidate.m_reasons.empty())
                {
                    candidateText += QObject::tr(" notes: %1").arg(JoinValues(candidate.m_reasons));
                }
                details.push_back(candidateText);
            }
            return details.join(QStringLiteral(" | "));
        }
    } // namespace

    EconomyDuplicateReportWidget::EconomyDuplicateReportWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Economy Cross-Pack Duplicates"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only authoring-time candidates from exact economy subject references and exact recipe "
                "duplicate keys across distinct owner packs. "
                "Display names and fuzzy similarity are never identity signals. "
                "The report does not merge records, reject a pack, select a winner, grant permission, or "
                "prove runtime conflict."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 8, this);
        m_table->setHorizontalHeaderLabels({
            tr("Exact signal"),
            tr("Exact match key"),
            tr("Kind"),
            tr("Owner packs"),
            tr("Canonical records"),
            tr("Status"),
            tr("Evidence / blockers"),
            tr("Candidate details and reasons") });
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table, 1);

        FoundationNotificationBus::Handler::BusConnect();
        Refresh();
    }

    EconomyDuplicateReportWidget::~EconomyDuplicateReportWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void EconomyDuplicateReportWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void EconomyDuplicateReportWidget::Refresh()
    {
        const FoundationService& foundation = FoundationService::Get();
        const EconomyDuplicateReport report = m_duplicateDetection.BuildCrossPackDuplicateReport(
            foundation.GetSourceRegistry(),
            foundation.GetCatalog(),
            foundation.GetSnapshot().m_blockers);

        m_summary->setText(
            tr(
                "Pack-owned economy records scanned: %1 | duplicate groups: %2 | review required: %3 | "
                "partial: %4 | blocked: %5")
                .arg(QString::number(static_cast<qulonglong>(report.m_scannedPackOwnedRecordCount)))
                .arg(QString::number(static_cast<qulonglong>(report.m_duplicateGroupCount)))
                .arg(QString::number(static_cast<qulonglong>(report.m_reviewRequiredGroupCount)))
                .arg(QString::number(static_cast<qulonglong>(report.m_partialGroupCount)))
                .arg(QString::number(static_cast<qulonglong>(report.m_blockedGroupCount))));

        m_table->setRowCount(static_cast<int>(report.m_groups.size()));
        for (int rowIndex = 0; rowIndex < static_cast<int>(report.m_groups.size()); ++rowIndex)
        {
            const EconomyDuplicateGroup& group = report.m_groups[static_cast<size_t>(rowIndex)];
            QString evidenceAndBlockers = JoinValues(group.m_evidenceIds);
            if (!group.m_blockerIds.empty())
            {
                if (!evidenceAndBlockers.isEmpty())
                {
                    evidenceAndBlockers += QStringLiteral(" | ");
                }
                evidenceAndBlockers += tr("blockers: %1").arg(JoinValues(group.m_blockerIds));
            }

            QString details = CandidateDetails(group);
            if (!group.m_reasons.empty())
            {
                if (!details.isEmpty())
                {
                    details += QStringLiteral(" | ");
                }
                details += tr("group notes: %1").arg(JoinValues(group.m_reasons));
            }

            SetCell(m_table, rowIndex, 0, ToQString(group.m_signal));
            SetCell(m_table, rowIndex, 1, ToQString(group.m_matchKey));
            SetCell(m_table, rowIndex, 2, ToQString(group.m_recordKind));
            SetCell(m_table, rowIndex, 3, JoinValues(group.m_packIds));
            SetCell(m_table, rowIndex, 4, JoinValues(group.m_recordIds));
            SetCell(m_table, rowIndex, 5, ToQString(group.m_status));
            SetCell(m_table, rowIndex, 6, evidenceAndBlockers);
            SetCell(m_table, rowIndex, 7, details);
        }
    }
} // namespace TaintedGrailModdingSDK
