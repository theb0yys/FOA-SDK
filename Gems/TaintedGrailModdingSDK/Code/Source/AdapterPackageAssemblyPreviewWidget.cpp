/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPackageAssemblyPreviewWidget.h"

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

        QString LayoutSummary(
            const AZStd::vector<AdapterPackageLayoutEntry>& layout)
        {
            QStringList values;
            for (const AdapterPackageLayoutEntry& entry : layout)
            {
                values.push_back(
                    QObject::tr("%1=%2 [%3; %4 bytes]")
                        .arg(
                            ToQString(entry.m_role),
                            ToQString(entry.m_packagePath),
                            ToQString(entry.m_outputDigest),
                            QString::number(
                                static_cast<qulonglong>(entry.m_byteSize))));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString OmissionSummary(
            const AZStd::vector<AdapterPackageAssemblyOmission>& omissions)
        {
            QStringList values;
            for (const AdapterPackageAssemblyOmission& omission : omissions)
            {
                values.push_back(
                    QObject::tr("%1=%2 (%3)")
                        .arg(
                            ToQString(omission.m_role),
                            ToQString(omission.m_expectedPath),
                            ToQString(omission.m_reason)));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString CollisionSummary(
            const AZStd::vector<AdapterPackageAssemblyCollision>& collisions)
        {
            QStringList values;
            for (const AdapterPackageAssemblyCollision& collision : collisions)
            {
                QStringList ids;
                for (const AZStd::string& entryId : collision.m_inventoryEntryIds)
                {
                    ids.push_back(ToQString(entryId));
                }
                values.push_back(
                    QObject::tr("%1 <= %2")
                        .arg(
                            ToQString(collision.m_packagePath),
                            ids.join(QStringLiteral(", "))));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString BlockerSummary(
            const AZStd::vector<AdapterPackageAssemblyBlocker>& blockers)
        {
            QStringList values;
            for (const AdapterPackageAssemblyBlocker& blocker : blockers)
            {
                values.push_back(
                    QObject::tr("%1 [%2]: %3")
                        .arg(
                            ToQString(blocker.m_code),
                            ToQString(blocker.m_packagePath),
                            ToQString(blocker.m_reason)));
            }
            return values.join(QStringLiteral(" | "));
        }
    } // namespace

    AdapterPackageAssemblyPreviewWidget::AdapterPackageAssemblyPreviewWidget(
        QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Package Assembly Preview"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 package-layout research. Each preview compares one accepted "
                "build-manifest review with one project-owned staging inventory, then derives "
                "package paths, output digests, omissions, collisions, and redistribution blockers. "
                "AssemblyAllowed, ArchiveAllowed, and DeploymentAllowed are always false. "
                "Nothing is copied, archived, deployed, launched, or executed by this pane."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 11, this);
        m_table->setHorizontalHeaderLabels({
            tr("Preview"),
            tr("Reviewed manifest"),
            tr("Staging inventory"),
            tr("Pack / adapter"),
            tr("Status"),
            tr("Package root"),
            tr("Derived layout and output digests"),
            tr("Omissions"),
            tr("Collisions"),
            tr("Redistribution and trust blockers"),
            tr("Canonical JSON") });
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table, 1);

        FoundationNotificationBus::Handler::BusConnect();
        Refresh();
    }

    AdapterPackageAssemblyPreviewWidget::~AdapterPackageAssemblyPreviewWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterPackageAssemblyPreviewWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterPackageAssemblyPreviewWidget::Refresh()
    {
        const AZStd::vector<AdapterPackageAssemblyPreviewRequest>& requests =
            AdapterPackageAssemblyPreviewRegistry::Get().GetRequests();
        AZStd::vector<AdapterPackageAssemblyPreview> previews;
        previews.reserve(requests.size());
        AZ::u64 readyCount = 0;
        AZ::u64 layoutCount = 0;
        AZ::u64 omissionCount = 0;
        AZ::u64 collisionCount = 0;
        for (const AdapterPackageAssemblyPreviewRequest& request : requests)
        {
            previews.push_back(m_previewService.BuildPreview(request));
            const AdapterPackageAssemblyPreview& preview = previews.back();
            if (preview.m_status == AdapterPackageAssemblyPreviewStatus::Ready)
            {
                ++readyCount;
            }
            layoutCount += static_cast<AZ::u64>(preview.m_layout.size());
            omissionCount += static_cast<AZ::u64>(preview.m_omissions.size());
            collisionCount += static_cast<AZ::u64>(preview.m_collisions.size());
        }

        m_summary->setText(
            tr(
                "Registered preview inputs: %1 | ready layouts: %2 | files: %3 | omissions: %4 | "
                "collisions: %5 | assembly/archive/deployment: prohibited")
                .arg(QString::number(static_cast<qulonglong>(requests.size())))
                .arg(QString::number(static_cast<qulonglong>(readyCount)))
                .arg(QString::number(static_cast<qulonglong>(layoutCount)))
                .arg(QString::number(static_cast<qulonglong>(omissionCount)))
                .arg(QString::number(static_cast<qulonglong>(collisionCount))));

        m_table->setRowCount(static_cast<int>(previews.size()));
        for (int row = 0; row < static_cast<int>(previews.size()); ++row)
        {
            const AdapterPackageAssemblyPreview& preview = previews[row];
            SetCell(m_table, row, 0, ToQString(preview.m_previewId));
            SetCell(
                m_table,
                row,
                1,
                tr("%1 | %2")
                    .arg(
                        ToQString(preview.m_manifestId),
                        ToQString(preview.m_manifestFingerprint)));
            SetCell(m_table, row, 2, ToQString(preview.m_inventoryId));
            SetCell(
                m_table,
                row,
                3,
                tr("%1 @ %2 | %3 @ %4")
                    .arg(
                        ToQString(preview.m_packId),
                        ToQString(preview.m_packVersion),
                        ToQString(preview.m_adapterId),
                        ToQString(preview.m_adapterVersion)));
            SetCell(
                m_table,
                row,
                4,
                tr("%1; assembly prohibited")
                    .arg(ToQString(ToString(preview.m_status))));
            SetCell(m_table, row, 5, ToQString(preview.m_packageRoot));
            SetCell(m_table, row, 6, LayoutSummary(preview.m_layout));
            SetCell(m_table, row, 7, OmissionSummary(preview.m_omissions));
            SetCell(m_table, row, 8, CollisionSummary(preview.m_collisions));
            SetCell(m_table, row, 9, BlockerSummary(preview.m_blockers));
            SetCell(m_table, row, 10, ToQString(preview.m_canonicalJson));
        }
    }
} // namespace TaintedGrailModdingSDK
