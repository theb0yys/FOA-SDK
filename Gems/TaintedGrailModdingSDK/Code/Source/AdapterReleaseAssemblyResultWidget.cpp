/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseAssemblyResultWidget.h"

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
        QString ToAssemblyQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        void SetAssemblyCell(
            QTableWidget* table,
            int row,
            int column,
            const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }

        QString JoinFailureIds(const AZStd::vector<AZStd::string>& failureIds)
        {
            QStringList values;
            for (const AZStd::string& failureId : failureIds)
            {
                values.push_back(ToAssemblyQString(failureId));
            }
            return values.join(QStringLiteral(", "));
        }

        QString DiagnosticDetails(
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            const AZStd::vector<AZStd::string>& diagnosticIds)
        {
            QStringList values;
            for (const AZStd::string& diagnosticId : diagnosticIds)
            {
                for (const AdapterReleaseAssemblyDiagnosticReference& diagnostic :
                     envelope.m_diagnosticReferences)
                {
                    if (diagnostic.m_diagnosticId == diagnosticId)
                    {
                        values.push_back(
                            ToAssemblyQString(diagnostic.m_diagnosticId)
                            + QStringLiteral(" [")
                            + ToAssemblyQString(ToString(diagnostic.m_kind))
                            + QStringLiteral("] ")
                            + ToAssemblyQString(diagnostic.m_reference));
                    }
                }
            }
            return values.join(QStringLiteral(" | "));
        }
    } // namespace

    AdapterReleaseAssemblyResultWidget::AdapterReleaseAssemblyResultWidget(
        QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Release Assembly and Checksum Results"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 evidence supplied by a separately reviewed external "
                "assembler/checksummer. Exact release-artifact binding, attempted archive "
                "assembly, per-content checksum observations, archive identity and "
                "fingerprint, failures, and safe diagnostic references are displayed. "
                "This pane does not read or hash files, assemble an archive, sign, upload, "
                "publish, launch FoA, call an adapter, or mutate deployment state."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 9, this);
        m_table->setHorizontalHeaderLabels({
            tr("Result / artifact binding"),
            tr("Reviewed assembler/checksummer"),
            tr("Archive attempt"),
            tr("Content / package path"),
            tr("Checksum observation"),
            tr("Failures"),
            tr("Safe diagnostics"),
            tr("Context"),
            tr("Safety"),
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

    AdapterReleaseAssemblyResultWidget::~AdapterReleaseAssemblyResultWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterReleaseAssemblyResultWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterReleaseAssemblyResultWidget::Refresh()
    {
        const AZStd::vector<AdapterReleaseAssemblyResultEnvelope>& envelopes =
            AdapterReleaseAssemblyResultRegistry::Get().GetEnvelopes();
        int rowCount = 0;
        for (const AdapterReleaseAssemblyResultEnvelope& envelope : envelopes)
        {
            rowCount += envelope.m_checksumObservations.empty()
                ? 1
                : static_cast<int>(envelope.m_checksumObservations.size());
        }
        m_table->setRowCount(rowCount);

        AZ::u64 observationCount = 0;
        AZ::u64 archiveAttemptCount = 0;
        AZ::u64 failureCount = 0;
        int rowIndex = 0;
        for (const AdapterReleaseAssemblyResultEnvelope& envelope : envelopes)
        {
            observationCount += static_cast<AZ::u64>(
                envelope.m_checksumObservations.size());
            archiveAttemptCount += envelope.m_archive.m_attempted ? 1 : 0;
            failureCount += static_cast<AZ::u64>(envelope.m_failures.size());
            const int rows = envelope.m_checksumObservations.empty()
                ? 1
                : static_cast<int>(envelope.m_checksumObservations.size());
            for (int localRow = 0; localRow < rows; ++localRow)
            {
                const AdapterReleaseChecksumObservation* observation =
                    envelope.m_checksumObservations.empty()
                    ? nullptr
                    : &envelope.m_checksumObservations[
                        static_cast<size_t>(localRow)];
                SetAssemblyCell(
                    m_table,
                    rowIndex,
                    0,
                    tr("%1 | artifact=%2 | artifact fingerprint=%3 | result fingerprint=%4")
                        .arg(
                            ToAssemblyQString(envelope.m_resultId),
                            ToAssemblyQString(envelope.m_artifactId),
                            ToAssemblyQString(envelope.m_artifactFingerprint),
                            ToAssemblyQString(envelope.m_resultFingerprint)));
                SetAssemblyCell(
                    m_table,
                    rowIndex,
                    1,
                    tr("%1 %2 | review=%3 | decision=%4 | reviewer=%5")
                        .arg(
                            ToAssemblyQString(
                                envelope.m_assemblerReview.m_assemblerId),
                            ToAssemblyQString(
                                envelope.m_assemblerReview.m_assemblerVersion),
                            ToAssemblyQString(envelope.m_assemblerReview.m_reviewId),
                            ToAssemblyQString(ToString(
                                envelope.m_assemblerReview.m_decision)),
                            ToAssemblyQString(
                                envelope.m_assemblerReview.m_reviewer)));
                SetAssemblyCell(
                    m_table,
                    rowIndex,
                    2,
                    tr("%1 | %2 | outcome=%3 | attempted=%4 | present=%5 | bytes=%6 | %7")
                        .arg(
                            ToAssemblyQString(envelope.m_archive.m_archiveId),
                            ToAssemblyQString(envelope.m_archive.m_archivePath),
                            ToAssemblyQString(ToString(envelope.m_archive.m_outcome)),
                            envelope.m_archive.m_attempted ? tr("yes") : tr("no"),
                            envelope.m_archive.m_archivePresent ? tr("yes") : tr("no"),
                            QString::number(static_cast<qulonglong>(
                                envelope.m_archive.m_byteSize)),
                            ToAssemblyQString(
                                envelope.m_archive.m_archiveFingerprint)));
                if (observation)
                {
                    SetAssemblyCell(
                        m_table,
                        rowIndex,
                        3,
                        tr("%1 | %2")
                            .arg(
                                ToAssemblyQString(observation->m_contentId),
                                ToAssemblyQString(observation->m_packagePath)));
                    SetAssemblyCell(
                        m_table,
                        rowIndex,
                        4,
                        tr("%1 | outcome=%2 | attempted=%3 | expected=%4 | observed=%5")
                            .arg(
                                ToAssemblyQString(observation->m_observationId),
                                ToAssemblyQString(ToString(observation->m_outcome)),
                                observation->m_attempted ? tr("yes") : tr("no"),
                                ToAssemblyQString(observation->m_expectedChecksum),
                                ToAssemblyQString(observation->m_observedChecksum)));
                    SetAssemblyCell(
                        m_table,
                        rowIndex,
                        5,
                        JoinFailureIds(observation->m_failureIds));
                    SetAssemblyCell(
                        m_table,
                        rowIndex,
                        6,
                        DiagnosticDetails(
                            envelope,
                            observation->m_diagnosticReferenceIds));
                }
                else
                {
                    SetAssemblyCell(
                        m_table,
                        rowIndex,
                        5,
                        JoinFailureIds(envelope.m_archive.m_failureIds));
                    SetAssemblyCell(
                        m_table,
                        rowIndex,
                        6,
                        DiagnosticDetails(
                            envelope,
                            envelope.m_archive.m_diagnosticReferenceIds));
                }
                SetAssemblyCell(
                    m_table,
                    rowIndex,
                    7,
                    tr("manifest=%1 | pack=%2 %3 | profile=%4 | game=%5 | branch=%6 | target=%7")
                        .arg(
                            ToAssemblyQString(envelope.m_manifestId),
                            ToAssemblyQString(envelope.m_packId),
                            ToAssemblyQString(envelope.m_packVersion),
                            ToAssemblyQString(envelope.m_profileId),
                            ToAssemblyQString(envelope.m_gameVersion),
                            ToAssemblyQString(envelope.m_branch),
                            ToAssemblyQString(envelope.m_runtimeTarget)));
                SetAssemblyCell(
                    m_table,
                    rowIndex,
                    8,
                    tr("SDK read=no | hash=no | archive=no | sign=no | upload=no | "
                       "publish=no | launch=no | adapter=no | deploy=no"));
                ++rowIndex;
            }
        }

        m_summary->setText(
            tr("Registered release assembly-result envelopes: %1 | supplied checksum "
               "observations: %2 | reported archive attempts: %3 | supplied failures: "
               "%4. These are external-result claims and candidate evidence only.")
                .arg(
                    QString::number(static_cast<qulonglong>(envelopes.size())),
                    QString::number(static_cast<qulonglong>(observationCount)),
                    QString::number(static_cast<qulonglong>(archiveAttemptCount)),
                    QString::number(static_cast<qulonglong>(failureCount))));
    }
} // namespace TaintedGrailModdingSDK
