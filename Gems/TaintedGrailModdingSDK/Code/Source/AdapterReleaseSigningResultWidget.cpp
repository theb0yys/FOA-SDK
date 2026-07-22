/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseSigningResultWidget.h"

#include "AdapterReleaseSigningHardening.h"

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
        constexpr qsizetype MaximumSigningCellCharacters = 4096;

        QString ToSigningQString(const AZStd::string& value)
        {
            return QString::fromUtf8(
                value.c_str(),
                static_cast<qsizetype>(value.size()));
        }

        QString BoundedSigningCell(QString value)
        {
            if (value.size() <= MaximumSigningCellCharacters)
            {
                return value;
            }
            value.truncate(MaximumSigningCellCharacters - 30);
            value += QStringLiteral(" ... [display truncated]");
            return value;
        }

        void SetSigningCell(
            QTableWidget* table,
            int row,
            int column,
            QString value)
        {
            table->setItem(
                row,
                column,
                new QTableWidgetItem(BoundedSigningCell(AZStd::move(value))));
        }

        QString JoinCapabilities(const AdapterReleaseSignerReview& review)
        {
            QStringList values;
            for (AdapterReleaseSignerCapability capability : review.m_capabilities)
            {
                values.push_back(ToSigningQString(ToString(capability)));
            }
            values.sort();
            return BoundedSigningCell(values.join(QStringLiteral(", ")));
        }

        QString JoinSigningIds(const AZStd::vector<AZStd::string>& ids)
        {
            AZStd::vector<AZStd::string> sorted = ids;
            AZStd::sort(sorted.begin(), sorted.end());
            QStringList values;
            for (const AZStd::string& id : sorted)
            {
                values.push_back(ToSigningQString(id));
            }
            return BoundedSigningCell(values.join(QStringLiteral(", ")));
        }

        QString FailureDetails(
            const AdapterReleaseSigningResultEnvelope& envelope,
            const AZStd::string& signatureArtifactId)
        {
            AZStd::vector<const AdapterReleaseSigningFailure*> failures;
            for (const AdapterReleaseSigningFailure& failure : envelope.m_failures)
            {
                if (signatureArtifactId.empty()
                    || failure.m_signatureArtifactId.empty()
                    || failure.m_signatureArtifactId == signatureArtifactId)
                {
                    failures.push_back(&failure);
                }
            }
            AZStd::sort(
                failures.begin(),
                failures.end(),
                [](const auto* left, const auto* right)
                {
                    return left->m_failureId < right->m_failureId;
                });

            QStringList values;
            for (const AdapterReleaseSigningFailure* failure : failures)
            {
                QString value = ToSigningQString(failure->m_failureId)
                    + QStringLiteral(" [")
                    + ToSigningQString(ToString(failure->m_kind))
                    + QStringLiteral("] ")
                    + ToSigningQString(failure->m_code)
                    + QStringLiteral(": ")
                    + ToSigningQString(failure->m_message)
                    + QStringLiteral(" | retryable=")
                    + (failure->m_retryable
                        ? QStringLiteral("yes")
                        : QStringLiteral("no"));
                if (!failure->m_signatureArtifactId.empty())
                {
                    value += QStringLiteral(" | artifact=")
                        + ToSigningQString(failure->m_signatureArtifactId);
                }
                values.push_back(AZStd::move(value));
            }
            return BoundedSigningCell(values.join(QStringLiteral(" | ")));
        }

        QString DiagnosticDetails(
            const AdapterReleaseSigningResultEnvelope& envelope,
            const AZStd::string& signatureArtifactId)
        {
            AZStd::vector<const AdapterReleaseSigningDiagnosticReference*>
                diagnostics;
            for (const AdapterReleaseSigningDiagnosticReference& diagnostic :
                 envelope.m_diagnosticReferences)
            {
                const bool appliesToRow = signatureArtifactId.empty()
                    || diagnostic.m_signatureArtifactIds.empty()
                    || AZStd::find(
                           diagnostic.m_signatureArtifactIds.begin(),
                           diagnostic.m_signatureArtifactIds.end(),
                           signatureArtifactId)
                        != diagnostic.m_signatureArtifactIds.end();
                if (appliesToRow)
                {
                    diagnostics.push_back(&diagnostic);
                }
            }
            AZStd::sort(
                diagnostics.begin(),
                diagnostics.end(),
                [](const auto* left, const auto* right)
                {
                    return left->m_diagnosticId < right->m_diagnosticId;
                });

            QStringList values;
            for (const AdapterReleaseSigningDiagnosticReference* diagnostic :
                 diagnostics)
            {
                values.push_back(
                    ToSigningQString(diagnostic->m_diagnosticId)
                    + QStringLiteral(" [")
                    + ToSigningQString(ToString(diagnostic->m_kind))
                    + QStringLiteral("] ")
                    + ToSigningQString(diagnostic->m_reference)
                    + QStringLiteral(" | ")
                    + ToSigningQString(diagnostic->m_fingerprint));
            }
            return BoundedSigningCell(values.join(QStringLiteral(" | ")));
        }
    } // namespace

    AdapterReleaseSigningResultWidget::AdapterReleaseSigningResultWidget(
        QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Release Signing Results"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 metadata supplied by a separately reviewed "
                "external signer. Registry rows have not been evaluated against the "
                "exact upstream artifact and assembly result. Supplied diagnostics are "
                "references only, not content the SDK has opened or declared safe. "
                "This pane does not open or modify archives, load keys or credentials, "
                "resolve identities, sign or verify data, write signature artifacts, "
                "upload, publish, launch FoA, call an adapter, mutate deployment state, "
                "or modify saves."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 10, this);
        m_table->setHorizontalHeaderLabels({
            tr("Result / contract state"),
            tr("Archive"),
            tr("Supplied signer review"),
            tr("Supplied signing identity"),
            tr("Reported signing outcome"),
            tr("Signature artifact"),
            tr("Failures"),
            tr("Supplied diagnostics \u2014 not evaluated"),
            tr("Context"),
            tr("Safety"),
        });
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setWordWrap(true);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table, 1);

        FoundationNotificationBus::Handler::BusConnect();
        Refresh();
    }

    AdapterReleaseSigningResultWidget::~AdapterReleaseSigningResultWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterReleaseSigningResultWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterReleaseSigningResultWidget::Refresh()
    {
        const AZStd::vector<AdapterReleaseSigningResultEnvelope>& envelopes =
            AdapterReleaseSigningResultRegistry::Get().GetEnvelopes();

        size_t requestedRows = 0;
        for (const AdapterReleaseSigningResultEnvelope& envelope : envelopes)
        {
            const size_t rows = envelope.m_signatureArtifacts.empty()
                ? 1
                : envelope.m_signatureArtifacts.size();
            if (requestedRows
                > MaximumReleaseSigningEditorRows - AZStd::min(
                    rows,
                    MaximumReleaseSigningEditorRows))
            {
                requestedRows = MaximumReleaseSigningEditorRows;
                break;
            }
            requestedRows += rows;
            if (requestedRows >= MaximumReleaseSigningEditorRows)
            {
                requestedRows = MaximumReleaseSigningEditorRows;
                break;
            }
        }
        const size_t displayRows = AZStd::min(
            requestedRows,
            MaximumReleaseSigningEditorRows);
        m_table->setRowCount(static_cast<int>(displayRows));

        AZ::u64 attemptedCount = 0;
        AZ::u64 succeededCount = 0;
        AZ::u64 failedCount = 0;
        AZ::u64 skippedCount = 0;
        AZ::u64 signatureArtifactCount = 0;
        AZ::u64 failureCount = 0;
        AZ::u64 diagnosticCount = 0;
        size_t rowIndex = 0;
        bool displayTruncated = false;

        for (const AdapterReleaseSigningResultEnvelope& envelope : envelopes)
        {
            attemptedCount += envelope.m_attempted ? 1 : 0;
            succeededCount +=
                envelope.m_outcome == AdapterReleaseSigningOutcome::Succeeded ? 1 : 0;
            failedCount +=
                envelope.m_outcome == AdapterReleaseSigningOutcome::Failed ? 1 : 0;
            skippedCount +=
                envelope.m_outcome == AdapterReleaseSigningOutcome::Skipped ? 1 : 0;
            signatureArtifactCount += static_cast<AZ::u64>(
                envelope.m_signatureArtifacts.size());
            failureCount += static_cast<AZ::u64>(envelope.m_failures.size());
            diagnosticCount += static_cast<AZ::u64>(
                envelope.m_diagnosticReferences.size());

            AZStd::vector<const AdapterReleaseSignatureArtifact*> artifacts;
            for (const AdapterReleaseSignatureArtifact& artifact :
                 envelope.m_signatureArtifacts)
            {
                artifacts.push_back(&artifact);
            }
            AZStd::sort(
                artifacts.begin(),
                artifacts.end(),
                [](const auto* left, const auto* right)
                {
                    return left->m_signatureArtifactId
                        < right->m_signatureArtifactId;
                });

            const size_t rows = artifacts.empty() ? 1 : artifacts.size();
            for (size_t localRow = 0; localRow < rows; ++localRow)
            {
                if (rowIndex >= displayRows)
                {
                    displayTruncated = true;
                    break;
                }
                const AdapterReleaseSignatureArtifact* signatureArtifact =
                    artifacts.empty() ? nullptr : artifacts[localRow];
                const AZStd::string signatureArtifactId = signatureArtifact
                    ? signatureArtifact->m_signatureArtifactId
                    : AZStd::string{};
                const int tableRow = static_cast<int>(rowIndex);

                SetSigningCell(
                    m_table,
                    tableRow,
                    0,
                    tr("%1 | reported fingerprint=%2 | SDK authority fingerprint="
                       "not available in registry-only view | contract status=not "
                       "evaluated | artifact=%3 (%4) | assembly=%5 (%6)")
                        .arg(
                            ToSigningQString(envelope.m_resultId),
                            ToSigningQString(envelope.m_resultFingerprint),
                            ToSigningQString(envelope.m_artifactId),
                            ToSigningQString(envelope.m_artifactFingerprint),
                            ToSigningQString(envelope.m_assemblyResultId),
                            ToSigningQString(envelope.m_assemblyResultFingerprint)));
                SetSigningCell(
                    m_table,
                    tableRow,
                    1,
                    tr("%1 | %2 | format=%3 | bytes=%4 | %5")
                        .arg(
                            ToSigningQString(envelope.m_archiveId),
                            ToSigningQString(envelope.m_archivePath),
                            ToSigningQString(envelope.m_archiveFormat),
                            QString::number(static_cast<qulonglong>(
                                envelope.m_archiveByteSize)),
                            ToSigningQString(envelope.m_archiveFingerprint)));
                SetSigningCell(
                    m_table,
                    tableRow,
                    2,
                    tr("%1 %2 (%3) | review=%4 | decision=%5 | reviewer=%6 | "
                       "reviewed=%7 | capabilities=%8 | evidence=%9")
                        .arg(
                            ToSigningQString(
                                envelope.m_signerReview.m_signerToolId),
                            ToSigningQString(
                                envelope.m_signerReview.m_signerToolVersion),
                            ToSigningQString(
                                envelope.m_signerReview.m_signerToolFingerprint),
                            ToSigningQString(envelope.m_signerReview.m_reviewId),
                            ToSigningQString(ToString(
                                envelope.m_signerReview.m_decision)),
                            ToSigningQString(envelope.m_signerReview.m_reviewer),
                            ToSigningQString(
                                envelope.m_signerReview.m_reviewedAtUtc),
                            JoinCapabilities(envelope.m_signerReview),
                            JoinSigningIds(
                                envelope.m_signerReview.m_evidenceIds)));
                SetSigningCell(
                    m_table,
                    tableRow,
                    3,
                    tr("intent=%1 | decision=%2 | kind=%3 | signer=%4 | locator=%5 | %6")
                        .arg(
                            ToSigningQString(envelope.m_signingIntentId),
                            ToSigningQString(ToString(
                                envelope.m_signingIntentDecision)),
                            ToSigningQString(ToString(
                                envelope.m_signingIdentityKind)),
                            ToSigningQString(envelope.m_signerId),
                            ToSigningQString(envelope.m_identityLocator),
                            ToSigningQString(envelope.m_identityFingerprint)));
                SetSigningCell(
                    m_table,
                    tableRow,
                    4,
                    tr("outcome=%1 | attempted=%2 | completed=%3 | captured=%4 | "
                       "failure refs=%5 | diagnostic refs=%6")
                        .arg(
                            ToSigningQString(ToString(envelope.m_outcome)),
                            envelope.m_attempted ? tr("yes") : tr("no"),
                            ToSigningQString(envelope.m_completedAtUtc),
                            ToSigningQString(envelope.m_capturedAtUtc),
                            JoinSigningIds(envelope.m_failureIds),
                            JoinSigningIds(
                                envelope.m_diagnosticReferenceIds)));
                if (signatureArtifact)
                {
                    SetSigningCell(
                        m_table,
                        tableRow,
                        5,
                        tr("%1 [%2] | %3 | media=%4 | bytes=%5 | %6 | created=%7 | "
                           "archive=%8 (%9)")
                            .arg(
                                ToSigningQString(
                                    signatureArtifact->m_signatureArtifactId),
                                ToSigningQString(ToString(
                                    signatureArtifact->m_kind)),
                                ToSigningQString(signatureArtifact->m_reference),
                                ToSigningQString(signatureArtifact->m_mediaType),
                                QString::number(static_cast<qulonglong>(
                                    signatureArtifact->m_byteSize)),
                                ToSigningQString(signatureArtifact->m_fingerprint),
                                ToSigningQString(
                                    signatureArtifact->m_createdAtUtc),
                                ToSigningQString(signatureArtifact->m_archiveId),
                                ToSigningQString(
                                    signatureArtifact->m_archiveFingerprint)));
                }
                SetSigningCell(
                    m_table,
                    tableRow,
                    6,
                    FailureDetails(envelope, signatureArtifactId));
                SetSigningCell(
                    m_table,
                    tableRow,
                    7,
                    DiagnosticDetails(envelope, signatureArtifactId));
                SetSigningCell(
                    m_table,
                    tableRow,
                    8,
                    tr("reconciliation=%1 | package=%2 | manifest=%3 | pack=%4 %5 | "
                       "profile=%6 | game=%7 | branch=%8 | target=%9")
                        .arg(
                            ToSigningQString(envelope.m_reconciliationId),
                            ToSigningQString(envelope.m_packagePreviewId),
                            ToSigningQString(envelope.m_manifestId),
                            ToSigningQString(envelope.m_packId),
                            ToSigningQString(envelope.m_packVersion),
                            ToSigningQString(envelope.m_profileId),
                            ToSigningQString(envelope.m_gameVersion),
                            ToSigningQString(envelope.m_branch),
                            ToSigningQString(envelope.m_runtimeTarget)));
                SetSigningCell(
                    m_table,
                    tableRow,
                    9,
                    tr("SDK read=no | archive-open=no | archive-write=no | key=no | "
                       "credential=no | identity-resolution=no | sign=no | verify=no | "
                       "signature-write=no | upload=no | publish=no | launch=no | "
                       "adapter=no | deploy=no | save=no"));
                ++rowIndex;
            }
            if (displayTruncated)
            {
                break;
            }
        }

        m_summary->setText(
            tr("Registered release signing-result envelopes: %1 | reported attempts: "
               "%2 | reported successes: %3 | reported failures: %4 | reported skips: "
               "%5 | supplied signature artifacts: %6 | supplied failure records: %7 | "
               "supplied diagnostic references: %8 | displayed rows: %9 of at most %10%11. "
               "These are external-result claims only. Contract acceptance and the SDK-"
               "derived authority fingerprint require the exact upstream inputs and the "
               "release-signing evidence service; this pane does not infer, authenticate, "
               "declare diagnostics safe, or verify acceptance.")
                .arg(
                    QString::number(static_cast<qulonglong>(envelopes.size())),
                    QString::number(static_cast<qulonglong>(attemptedCount)),
                    QString::number(static_cast<qulonglong>(succeededCount)),
                    QString::number(static_cast<qulonglong>(failedCount)),
                    QString::number(static_cast<qulonglong>(skippedCount)),
                    QString::number(static_cast<qulonglong>(signatureArtifactCount)),
                    QString::number(static_cast<qulonglong>(failureCount)),
                    QString::number(static_cast<qulonglong>(diagnosticCount)),
                    QString::number(static_cast<qulonglong>(rowIndex)),
                    QString::number(static_cast<qulonglong>(
                        MaximumReleaseSigningEditorRows)),
                    displayTruncated
                        ? tr("; display truncated")
                        : QString{}));
    }
} // namespace TaintedGrailModdingSDK
