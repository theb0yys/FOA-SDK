/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterStagingDeploymentPreviewWidget.h"

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

        QString ChangeSummary(
            const AZStd::vector<AdapterDeploymentChange>& changes)
        {
            QStringList values;
            for (const AdapterDeploymentChange& change : changes)
            {
                values.push_back(
                    QObject::tr("%1 [%2 → %3]")
                        .arg(
                            ToQString(change.m_targetPath),
                            ToQString(change.m_previousFingerprint),
                            ToQString(change.m_desiredFingerprint)));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString ConflictSummary(
            const AZStd::vector<AdapterDeploymentConflict>& conflicts)
        {
            QStringList values;
            for (const AdapterDeploymentConflict& conflict : conflicts)
            {
                QStringList ids;
                for (const AZStd::string& id : conflict.m_targetEntryIds)
                {
                    ids.push_back(ToQString(id));
                }
                values.push_back(
                    QObject::tr("%1 [%2]: %3")
                        .arg(
                            ToQString(conflict.m_targetPath),
                            ids.join(QStringLiteral(", ")),
                            ToQString(conflict.m_reason)));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString BackupSummary(
            const AZStd::vector<AdapterDeploymentBackupRequirement>& backups)
        {
            QStringList values;
            for (const AdapterDeploymentBackupRequirement& backup : backups)
            {
                values.push_back(
                    QObject::tr("%1 → %2 [%3]")
                        .arg(
                            ToQString(backup.m_targetPath),
                            ToQString(backup.m_backupPath),
                            ToQString(backup.m_fingerprint)));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString RollbackSummary(
            const AZStd::vector<AdapterDeploymentRollbackStep>& steps)
        {
            QStringList values;
            for (const AdapterDeploymentRollbackStep& step : steps)
            {
                values.push_back(
                    QObject::tr("%1. %2 %3")
                        .arg(QString::number(static_cast<qulonglong>(step.m_sequence)))
                        .arg(ToQString(ToString(step.m_action)))
                        .arg(ToQString(step.m_targetPath)));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString BlockerSummary(
            const AZStd::vector<AdapterDeploymentPreviewBlocker>& blockers)
        {
            QStringList values;
            for (const AdapterDeploymentPreviewBlocker& blocker : blockers)
            {
                values.push_back(
                    QObject::tr("%1 [%2]: %3")
                        .arg(
                            ToQString(blocker.m_code),
                            ToQString(blocker.m_targetPath),
                            ToQString(blocker.m_reason)));
            }
            return values.join(QStringLiteral(" | "));
        }
    } // namespace

    AdapterStagingDeploymentPreviewWidget::AdapterStagingDeploymentPreviewWidget(
        QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Staging and Deployment Preview"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 comparison of one ready package layout with one accepted target inventory. "
                "It derives additions, replacements, removals, unchanged paths, exact conflicts, backup requirements, "
                "and deterministic inverse rollback steps. StagingMutationAllowed, DeploymentMutationAllowed, "
                "RollbackExecutionAllowed, and LaunchAllowed are always false. Nothing is copied, replaced, removed, "
                "backed up, restored, deployed, launched, or executed by this pane."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 13, this);
        m_table->setHorizontalHeaderLabels({
            tr("Preview"),
            tr("Package preview"),
            tr("Target inventory"),
            tr("Pack"),
            tr("Status"),
            tr("Target / backup roots"),
            tr("Additions"),
            tr("Replacements"),
            tr("Removals / unchanged"),
            tr("Conflicts"),
            tr("Backup requirements"),
            tr("Rollback steps"),
            tr("Blockers / canonical JSON") });
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table, 1);

        FoundationNotificationBus::Handler::BusConnect();
        Refresh();
    }

    AdapterStagingDeploymentPreviewWidget::~AdapterStagingDeploymentPreviewWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterStagingDeploymentPreviewWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterStagingDeploymentPreviewWidget::Refresh()
    {
        const AZStd::vector<AdapterStagingDeploymentPreviewRequest>& requests =
            AdapterStagingDeploymentPreviewRegistry::Get().GetRequests();
        AZStd::vector<AdapterStagingDeploymentPreview> previews;
        previews.reserve(requests.size());
        AZ::u64 readyCount = 0;
        AZ::u64 changeCount = 0;
        AZ::u64 conflictCount = 0;
        AZ::u64 rollbackCount = 0;
        for (const AdapterStagingDeploymentPreviewRequest& request : requests)
        {
            previews.push_back(m_previewService.BuildPreview(request));
            const AdapterStagingDeploymentPreview& preview = previews.back();
            if (preview.m_status == AdapterStagingDeploymentPreviewStatus::Ready)
            {
                ++readyCount;
            }
            changeCount += preview.m_additions.size();
            changeCount += preview.m_replacements.size();
            changeCount += preview.m_removals.size();
            conflictCount += preview.m_conflicts.size();
            rollbackCount += preview.m_rollbackSteps.size();
        }

        m_summary->setText(
            tr(
                "Registered inputs: %1 | ready previews: %2 | derived changes: %3 | conflicts: %4 | rollback steps: %5 | filesystem mutation and launch: prohibited")
                .arg(QString::number(static_cast<qulonglong>(requests.size())))
                .arg(QString::number(static_cast<qulonglong>(readyCount)))
                .arg(QString::number(static_cast<qulonglong>(changeCount)))
                .arg(QString::number(static_cast<qulonglong>(conflictCount)))
                .arg(QString::number(static_cast<qulonglong>(rollbackCount))));

        m_table->setRowCount(static_cast<int>(previews.size()));
        int row = 0;
        for (const AdapterStagingDeploymentPreview& preview : previews)
        {
            SetCell(m_table, row, 0, ToQString(preview.m_previewId));
            SetCell(
                m_table,
                row,
                1,
                tr("%1 | %2")
                    .arg(
                        ToQString(preview.m_packagePreviewId),
                        ToQString(preview.m_packagePreviewFingerprint)));
            SetCell(
                m_table,
                row,
                2,
                tr("%1 | %2")
                    .arg(
                        ToQString(preview.m_targetInventoryId),
                        ToQString(preview.m_targetInventoryFingerprint)));
            SetCell(m_table, row, 3, ToQString(preview.m_packId));
            SetCell(
                m_table,
                row,
                4,
                tr("%1; all mutation permissions=false")
                    .arg(ToQString(ToString(preview.m_status))));
            SetCell(
                m_table,
                row,
                5,
                tr("target %1 | backup %2")
                    .arg(
                        ToQString(preview.m_targetRoot),
                        ToQString(preview.m_backupRoot)));
            SetCell(m_table, row, 6, ChangeSummary(preview.m_additions));
            SetCell(m_table, row, 7, ChangeSummary(preview.m_replacements));
            SetCell(
                m_table,
                row,
                8,
                tr("remove: %1 | unchanged: %2")
                    .arg(
                        ChangeSummary(preview.m_removals),
                        ChangeSummary(preview.m_unchanged)));
            SetCell(m_table, row, 9, ConflictSummary(preview.m_conflicts));
            SetCell(m_table, row, 10, BackupSummary(preview.m_backups));
            SetCell(m_table, row, 11, RollbackSummary(preview.m_rollbackSteps));
            SetCell(
                m_table,
                row,
                12,
                tr("%1 | %2")
                    .arg(
                        BlockerSummary(preview.m_blockers),
                        ToQString(preview.m_canonicalJson)));
            ++row;
        }
    }
} // namespace TaintedGrailModdingSDK
