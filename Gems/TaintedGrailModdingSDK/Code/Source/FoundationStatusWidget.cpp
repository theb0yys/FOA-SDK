/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationStatusWidget.h"

#include "FoundationService.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
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

        void ConfigureReadOnlyTable(QTableWidget* table)
        {
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
        }

        void SetCell(QTableWidget* table, int row, int column, const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }
    } // namespace

    FoundationStatusWidget::FoundationStatusWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* rootLayout = new QVBoxLayout(this);

        auto* heading = new QLabel(tr("Tainted Grail Modding SDK — Foundation Status"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("Workspace, ownership, evidence, catalog coverage, and blockers. Displayed status never grants runtime permission."),
            this);
        description->setWordWrap(true);
        rootLayout->addWidget(description);

        auto* workspaceGroup = new QGroupBox(tr("Workspace and Game Profile"), this);
        auto* workspaceLayout = new QFormLayout(workspaceGroup);
        m_workspaceValue = new QLabel(workspaceGroup);
        m_profileValue = new QLabel(workspaceGroup);
        m_versionValue = new QLabel(workspaceGroup);
        m_branchValue = new QLabel(workspaceGroup);
        m_boundaryValue = new QLabel(tr("Editor authoring only — FoA runtime execution disabled"), workspaceGroup);
        m_boundaryValue->setWordWrap(true);
        workspaceLayout->addRow(tr("Workspace"), m_workspaceValue);
        workspaceLayout->addRow(tr("Active profile"), m_profileValue);
        workspaceLayout->addRow(tr("Game version"), m_versionValue);
        workspaceLayout->addRow(tr("Branch"), m_branchValue);
        workspaceLayout->addRow(tr("Runtime boundary"), m_boundaryValue);
        rootLayout->addWidget(workspaceGroup);

        auto* countsGroup = new QGroupBox(tr("Foundation Counts"), this);
        auto* countsLayout = new QVBoxLayout(countsGroup);
        m_countsTable = new QTableWidget(6, 2, countsGroup);
        m_countsTable->setHorizontalHeaderLabels({ tr("Area"), tr("Count") });
        ConfigureReadOnlyTable(m_countsTable);
        countsLayout->addWidget(m_countsTable);
        rootLayout->addWidget(countsGroup);

        auto* coverageGroup = new QGroupBox(tr("Catalog Coverage by Domain"), this);
        auto* coverageLayout = new QVBoxLayout(coverageGroup);
        m_domainTable = new QTableWidget(0, 3, coverageGroup);
        m_domainTable->setHorizontalHeaderLabels({ tr("Domain"), tr("Records"), tr("Blocked") });
        ConfigureReadOnlyTable(m_domainTable);
        coverageLayout->addWidget(m_domainTable);
        rootLayout->addWidget(coverageGroup);

        auto* blockersGroup = new QGroupBox(tr("Open Blockers"), this);
        auto* blockersLayout = new QVBoxLayout(blockersGroup);
        m_blockerTable = new QTableWidget(0, 3, blockersGroup);
        m_blockerTable->setHorizontalHeaderLabels({ tr("Severity"), tr("Area"), tr("Reason") });
        ConfigureReadOnlyTable(m_blockerTable);
        blockersLayout->addWidget(m_blockerTable);
        rootLayout->addWidget(blockersGroup, 1);

        auto* refreshButton = new QPushButton(tr("Refresh Foundation Status"), this);
        connect(refreshButton, &QPushButton::clicked, this, [this]()
        {
            FoundationService::Get().RefreshSnapshot();
            Refresh();
        });
        rootLayout->addWidget(refreshButton);

        Refresh();
    }

    void FoundationStatusWidget::Refresh()
    {
        const FoundationSnapshot& snapshot = FoundationService::Get().GetSnapshot();

        m_workspaceValue->setText(ToQString(snapshot.m_workspaceName));
        m_profileValue->setText(ToQString(snapshot.m_activeGameProfile));
        m_versionValue->setText(ToQString(snapshot.m_gameVersion));
        m_branchValue->setText(ToQString(snapshot.m_branch));

        const struct CountRow
        {
            const char* m_name;
            AZ::u64 m_count;
        } countRows[] = {
            { "Game profiles", snapshot.m_gameProfileCount },
            { "Pack manifests", snapshot.m_packCount },
            { "Registered sources", snapshot.m_sourceCount },
            { "Evidence records", snapshot.m_evidenceCount },
            { "Catalog records", snapshot.m_catalogRecordCount },
            { "Open blockers", snapshot.m_openBlockerCount },
        };

        for (int row = 0; row < 6; ++row)
        {
            SetCell(m_countsTable, row, 0, tr(countRows[row].m_name));
            SetCell(m_countsTable, row, 1, QString::number(static_cast<qulonglong>(countRows[row].m_count)));
        }
        m_countsTable->resizeRowsToContents();

        m_domainTable->setRowCount(static_cast<int>(snapshot.m_domainCoverage.size()));
        for (int row = 0; row < static_cast<int>(snapshot.m_domainCoverage.size()); ++row)
        {
            const DomainCoverage& coverage = snapshot.m_domainCoverage[static_cast<size_t>(row)];
            SetCell(m_domainTable, row, 0, ToQString(coverage.m_domain));
            SetCell(m_domainTable, row, 1, QString::number(static_cast<qulonglong>(coverage.m_recordCount)));
            SetCell(m_domainTable, row, 2, QString::number(static_cast<qulonglong>(coverage.m_blockedRecordCount)));
        }
        m_domainTable->resizeRowsToContents();

        m_blockerTable->setRowCount(static_cast<int>(snapshot.m_blockers.size()));
        for (int row = 0; row < static_cast<int>(snapshot.m_blockers.size()); ++row)
        {
            const BlockerRecord& blocker = snapshot.m_blockers[static_cast<size_t>(row)];
            SetCell(m_blockerTable, row, 0, ToQString(blocker.m_severity));
            SetCell(m_blockerTable, row, 1, ToQString(blocker.m_area));
            SetCell(m_blockerTable, row, 2, ToQString(blocker.m_reason));
        }
        m_blockerTable->resizeRowsToContents();
    }
} // namespace TaintedGrailModdingSDK
