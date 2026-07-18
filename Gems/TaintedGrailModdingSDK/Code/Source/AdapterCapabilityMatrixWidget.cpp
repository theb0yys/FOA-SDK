/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterCapabilityMatrixWidget.h"

#include "FoundationService.h"

#include <QAbstractItemView>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
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

        QString BuildProofDetails(const AdapterCapabilityMatrixRow& row)
        {
            QStringList details;
            if (!row.m_subjectIds.empty())
            {
                details.push_back(QObject::tr("subjects: %1").arg(JoinValues(row.m_subjectIds)));
            }
            if (!row.m_declarationEvidenceIds.empty())
            {
                details.push_back(
                    QObject::tr("declaration evidence: %1").arg(JoinValues(row.m_declarationEvidenceIds)));
            }
            if (!row.m_permissionEvidenceIds.empty())
            {
                details.push_back(
                    QObject::tr("permission evidence: %1").arg(JoinValues(row.m_permissionEvidenceIds)));
            }
            if (!row.m_validationProofIds.empty())
            {
                details.push_back(
                    QObject::tr("validation proof: %1").arg(JoinValues(row.m_validationProofIds)));
            }
            if (!row.m_reasons.empty())
            {
                details.push_back(QObject::tr("notes: %1").arg(JoinValues(row.m_reasons)));
            }
            return details.join(QStringLiteral(" | "));
        }
    } // namespace

    AdapterCapabilityMatrixWidget::AdapterCapabilityMatrixWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Adapter Capability Matrix"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 7 compatibility analysis over pack adapter-version requirements, typed adapter "
                "declarations, active runtime target, catalog permissions, validation proof, and exact adapter "
                "evidence. The matrix does not load or execute an adapter, generate a work order, deploy files, "
                "launch FoA, or mutate saves."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 9, this);
        m_table->setHorizontalHeaderLabels({
            tr("Pack"),
            tr("Required adapter"),
            tr("Declared adapter"),
            tr("Declared version"),
            tr("Runtime target"),
            tr("Capability"),
            tr("Status"),
            tr("Subjects"),
            tr("Evidence, validation proof, and reasons") });
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table, 1);

        FoundationNotificationBus::Handler::BusConnect();
        Refresh();
    }

    AdapterCapabilityMatrixWidget::~AdapterCapabilityMatrixWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterCapabilityMatrixWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterCapabilityMatrixWidget::Refresh()
    {
        const FoundationService& foundation = FoundationService::Get();
        const AdapterCapabilityMatrix matrix = m_compatibilityService.BuildCapabilityMatrix(
            foundation.GetWorkspace(),
            foundation.GetPacks(),
            AdapterContractRegistry::Get(),
            foundation.GetSourceRegistry(),
            foundation.GetCatalog(),
            foundation.GetSnapshot().m_blockers);

        m_summary->setText(
            tr(
                "Rows: %1 | supported: %2 | unsupported: %3 | version mismatch: %4 | "
                "permission missing: %5 | proof missing: %6")
                .arg(QString::number(static_cast<qulonglong>(matrix.m_rowCount)))
                .arg(QString::number(static_cast<qulonglong>(matrix.m_supportedCount)))
                .arg(QString::number(static_cast<qulonglong>(matrix.m_unsupportedCount)))
                .arg(QString::number(static_cast<qulonglong>(matrix.m_versionMismatchCount)))
                .arg(QString::number(static_cast<qulonglong>(matrix.m_permissionMissingCount)))
                .arg(QString::number(static_cast<qulonglong>(matrix.m_proofMissingCount))));

        m_table->setRowCount(static_cast<int>(matrix.m_rows.size()));
        for (int rowIndex = 0; rowIndex < static_cast<int>(matrix.m_rows.size()); ++rowIndex)
        {
            const AdapterCapabilityMatrixRow& row = matrix.m_rows[static_cast<size_t>(rowIndex)];
            SetCell(m_table, rowIndex, 0, ToQString(row.m_packId));
            SetCell(m_table, rowIndex, 1, ToQString(row.m_requiredAdapterVersion));
            SetCell(m_table, rowIndex, 2, ToQString(row.m_adapterId));
            SetCell(m_table, rowIndex, 3, ToQString(row.m_adapterVersion));
            SetCell(m_table, rowIndex, 4, ToQString(row.m_runtimeTarget));
            SetCell(m_table, rowIndex, 5, ToQString(row.m_capability));
            SetCell(m_table, rowIndex, 6, ToQString(row.m_status));
            SetCell(m_table, rowIndex, 7, JoinValues(row.m_subjectIds));
            SetCell(m_table, rowIndex, 8, BuildProofDetails(row));
        }
    }
} // namespace TaintedGrailModdingSDK
