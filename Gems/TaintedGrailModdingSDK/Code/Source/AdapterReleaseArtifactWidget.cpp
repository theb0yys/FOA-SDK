/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseArtifactWidget.h"

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
        QString ToReleaseQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        void SetReleaseCell(
            QTableWidget* table,
            int row,
            int column,
            const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }

        QString ReleaseBlockerDetails(
            const AdapterReleaseArtifactEnvelope& envelope)
        {
            QStringList values;
            for (const AdapterReleaseArtifactBlocker& blocker : envelope.m_blockers)
            {
                values.push_back(
                    ToReleaseQString(blocker.m_code) + QStringLiteral(": ")
                    + ToReleaseQString(blocker.m_message));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString ReleaseProvenanceDetails(
            const AdapterReleaseArtifactEnvelope& envelope,
            const AdapterReleaseArtifactContent& content)
        {
            QStringList values;
            for (const AZStd::string& provenanceId : content.m_provenanceIds)
            {
                for (const AdapterReleaseProvenanceRecord& record :
                     envelope.m_provenance)
                {
                    if (record.m_provenanceId == provenanceId)
                    {
                        values.push_back(
                            ToReleaseQString(record.m_provenanceId)
                            + QStringLiteral(" [")
                            + ToReleaseQString(record.m_sourceKind)
                            + QStringLiteral("] ")
                            + ToReleaseQString(record.m_sourceId));
                    }
                }
            }
            return values.join(QStringLiteral(" | "));
        }

        QString ReleaseLegalDetails(
            const AdapterReleaseArtifactEnvelope& envelope,
            const AdapterReleaseArtifactContent& content)
        {
            for (const AdapterReleaseLegalDispositionRecord& disposition :
                 envelope.m_legalDispositions)
            {
                if (disposition.m_dispositionId == content.m_legalDispositionId)
                {
                    return QObject::tr("%1 | %2 | reviewer=%3")
                        .arg(
                            ToReleaseQString(disposition.m_dispositionId),
                            ToReleaseQString(ToString(disposition.m_disposition)),
                            ToReleaseQString(disposition.m_reviewer));
                }
            }
            return QObject::tr("missing");
        }

        QString PublicationTargetDetails(
            const AdapterReleaseArtifactEnvelope& envelope)
        {
            QStringList values;
            for (const AdapterReleasePublicationTarget& target :
                 envelope.m_publicationTargets)
            {
                values.push_back(
                    QObject::tr("%1 [%2] %3/%4")
                        .arg(
                            ToReleaseQString(target.m_targetId),
                            ToReleaseQString(ToString(target.m_kind)),
                            ToReleaseQString(target.m_locator),
                            ToReleaseQString(target.m_channel)));
            }
            return values.join(QStringLiteral(" | "));
        }
    } // namespace

    AdapterReleaseArtifactWidget::AdapterReleaseArtifactWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Release Artifact Provenance and Signing Intent"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 release metadata. Exact approved reconciliation, "
                "reviewed package layout, declared SHA-256 values, provenance, legal "
                "dispositions, signing identity intent, and publication targets are "
                "displayed without opening or hashing files, copying package content, "
                "assembling an archive, signing, uploading, publishing, launching FoA, "
                "calling an adapter, or mutating deployment state."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 10, this);
        m_table->setHorizontalHeaderLabels({
            tr("Artifact / status"),
            tr("Approved reconciliation"),
            tr("Ready package"),
            tr("Declared content"),
            tr("Declared checksum"),
            tr("Provenance"),
            tr("Legal disposition"),
            tr("Signing intent"),
            tr("Publication targets"),
            tr("Safety / blockers"),
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

    AdapterReleaseArtifactWidget::~AdapterReleaseArtifactWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterReleaseArtifactWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterReleaseArtifactWidget::Refresh()
    {
        const AZStd::vector<AdapterReleaseArtifactEnvelope>& envelopes =
            AdapterReleaseArtifactRegistry::Get().GetEnvelopes();
        int rowCount = 0;
        for (const AdapterReleaseArtifactEnvelope& envelope : envelopes)
        {
            rowCount += envelope.m_contents.empty()
                ? 1
                : static_cast<int>(envelope.m_contents.size());
        }
        m_table->setRowCount(rowCount);

        AZ::u64 readyCount = 0;
        AZ::u64 contentCount = 0;
        AZ::u64 targetCount = 0;
        int rowIndex = 0;
        for (const AdapterReleaseArtifactEnvelope& envelope : envelopes)
        {
            readyCount += envelope.m_metadataReady ? 1 : 0;
            contentCount += envelope.m_contentCount;
            targetCount += envelope.m_publicationTargetCount;
            const int rows = envelope.m_contents.empty()
                ? 1
                : static_cast<int>(envelope.m_contents.size());
            for (int localRow = 0; localRow < rows; ++localRow)
            {
                const AdapterReleaseArtifactContent* content =
                    envelope.m_contents.empty()
                    ? nullptr
                    : &envelope.m_contents[static_cast<size_t>(localRow)];
                SetReleaseCell(
                    m_table,
                    rowIndex,
                    0,
                    tr("%1 | %2 | metadata ready=%3")
                        .arg(
                            ToReleaseQString(envelope.m_artifactId),
                            ToReleaseQString(ToString(envelope.m_status)),
                            envelope.m_metadataReady ? tr("yes") : tr("no")));
                SetReleaseCell(
                    m_table,
                    rowIndex,
                    1,
                    tr("%1 | report=%2 | verifier=%3")
                        .arg(
                            ToReleaseQString(envelope.m_reconciliationId),
                            ToReleaseQString(envelope.m_reportId),
                            ToReleaseQString(envelope.m_verifierResultId)));
                SetReleaseCell(
                    m_table,
                    rowIndex,
                    2,
                    tr("%1 | manifest=%2 | pack=%3 %4")
                        .arg(
                            ToReleaseQString(envelope.m_packagePreviewId),
                            ToReleaseQString(envelope.m_manifestId),
                            ToReleaseQString(envelope.m_packId),
                            ToReleaseQString(envelope.m_packVersion)));
                if (content)
                {
                    SetReleaseCell(
                        m_table,
                        rowIndex,
                        3,
                        tr("%1 | %2 | %3 | %4 bytes")
                            .arg(
                                ToReleaseQString(content->m_contentId),
                                ToReleaseQString(content->m_packagePath),
                                ToReleaseQString(content->m_role),
                                QString::number(static_cast<qulonglong>(
                                    content->m_byteSize))));
                    SetReleaseCell(
                        m_table,
                        rowIndex,
                        4,
                        tr("%1 | expected=%2 | generated=no")
                            .arg(
                                ToReleaseQString(ToString(
                                    content->m_checksumAlgorithm)),
                                ToReleaseQString(content->m_expectedChecksum)));
                    SetReleaseCell(
                        m_table,
                        rowIndex,
                        5,
                        ReleaseProvenanceDetails(envelope, *content));
                    SetReleaseCell(
                        m_table,
                        rowIndex,
                        6,
                        ReleaseLegalDetails(envelope, *content));
                }
                SetReleaseCell(
                    m_table,
                    rowIndex,
                    7,
                    tr("%1 | identity=%2 | performed=no")
                        .arg(
                            ToReleaseQString(ToString(
                                envelope.m_signingIntent.m_decision)),
                            ToReleaseQString(ToString(
                                envelope.m_signingIntent.m_identityKind))));
                SetReleaseCell(
                    m_table,
                    rowIndex,
                    8,
                    PublicationTargetDetails(envelope));
                SetReleaseCell(
                    m_table,
                    rowIndex,
                    9,
                    tr("read=no | hash=no | copy=no | archive=no | sign=no | upload=no | "
                       "publish=no | deploy=no | %1")
                        .arg(ReleaseBlockerDetails(envelope)));
                ++rowIndex;
            }
        }

        m_summary->setText(
            tr("Registered release-artifact envelopes: %1 | metadata ready: %2 | "
               "declared contents: %3 | publication targets: %4. Ready means contract "
               "metadata only; no release operation is authorised or performed.")
                .arg(
                    QString::number(static_cast<qulonglong>(envelopes.size())),
                    QString::number(static_cast<qulonglong>(readyCount)),
                    QString::number(static_cast<qulonglong>(contentCount)),
                    QString::number(static_cast<qulonglong>(targetCount))));
    }
} // namespace TaintedGrailModdingSDK
