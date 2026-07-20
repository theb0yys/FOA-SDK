/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseSigningResultWidget.h"

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
        QString ToSigningQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        void SetSigningCell(
            QTableWidget* table,
            int row,
            int column,
            const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }

        QString JoinCapabilities(const AdapterReleaseSignerReview& review)
        {
            QStringList values;
            for (AdapterReleaseSignerCapability capability : review.m_capabilities)
            {
                values.push_back(ToSigningQString(ToString(capability)));
            }
            values.sort();
            return values.join(QStringLiteral(", "));
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
            return values.join(QStringLiteral(", "));
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
                [](const AdapterReleaseSigningFailure* left,
                   const AdapterReleaseSigningFailure* right)
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
                values.push_back(value);
            }
            return values.join(QStringLiteral(" | "));
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
                [](const AdapterReleaseSigningDiagnosticReference* left,
                   const AdapterReleaseSigningDiagnosticReference* right)
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
            return values.join(QStringLiteral(" | "));
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
                "Read-only Phase 8 evidence supplied by a separately reviewed external "
                "signer. Supplied release-artifact, assembly-result, archive, signing-"
                "intent, signer-review, reported-outcome, signature-artifact, failure, "
                "and safe diagnostic binding fields are displayed. This pane does not "
                "open or modify "
                "archives, load keys or credentials, resolve identities, sign or verify "
                "data, write signature artifacts, upload, publish, launch FoA, call an "
                "adapter, mutate deployment state, or modify saves."),
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
            tr("Safe diagnostics"),
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
        int rowCount = 0;
        for (const AdapterReleaseSigningResultEnvelope& envelope : envelopes)
        {
            rowCount += envelope.m_signatureArtifacts.empty()
                ? 1
                : static_cast<int>(envelope.m_signatureArtifacts.size());
        }
        m_table->setRowCount(rowCount);

        AZ::u64 attemptedCount = 0;
        AZ::u64 succeededCount = 0;
        AZ::u64 failedCount = 0;
        AZ::u64 skippedCount = 0;
        AZ::u64 signatureArtifactCount = 0;
        AZ::u64 failureCount = 0;
        AZ::u64 diagnosticCount = 0;
        int rowIndex = 0;
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
                [](const AdapterReleaseSignatureArtifact* left,
                   const AdapterReleaseSignatureArtifact* right)
                {
                    return left->m_signatureArtifactId
                        < right->m_signatureArtifactId;
                });

            const int rows = artifacts.empty()
                ? 1
                : static_cast<int>(artifacts.size());
            for (int localRow = 0; localRow < rows; ++localRow)
            {
                const AdapterReleaseSignatureArtifact* signatureArtifact =
                    artifacts.empty()
                    ? nullptr
                    : artifacts[static_cast<size_t>(localRow)];
                const AZStd::string signatureArtifactId = signatureArtifact
                    ? signatureArtifact->m_signatureArtifactId
                    : AZStd::string{};

                SetSigningCell(
                    m_table,
                    rowIndex,
                    0,
                    tr("%1 (%2) | contract status=not evaluated | artifact=%3 (%4) | "
                       "assembly=%5 (%6)")
                        .arg(
                            ToSigningQString(envelope.m_resultId),
                            ToSigningQString(envelope.m_resultFingerprint),
                            ToSigningQString(envelope.m_artifactId),
                            ToSigningQString(envelope.m_artifactFingerprint),
                            ToSigningQString(envelope.m_assemblyResultId),
                            ToSigningQString(envelope.m_assemblyResultFingerprint)));
                SetSigningCell(
                    m_table,
                    rowIndex,
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
                    rowIndex,
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
                    rowIndex,
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
                    rowIndex,
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
                        rowIndex,
                        5,
                        tr("%1 [%2] | %3 | media=%4 | bytes=%5 | %6 | created=%7 | "
                           "archive=%8 (%9)")
                            .arg(
                                ToSigningQString(
                                    signatureArtifact->m_signatureArtifactId),
                                ToSigningQString(ToString(signatureArtifact->m_kind)),
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
                    rowIndex,
                    6,
                    FailureDetails(envelope, signatureArtifactId));
                SetSigningCell(
                    m_table,
                    rowIndex,
                    7,
                    DiagnosticDetails(envelope, signatureArtifactId));
                SetSigningCell(
                    m_table,
                    rowIndex,
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
                    rowIndex,
                    9,
                    tr("SDK read=no | archive-open=no | archive-write=no | key=no | "
                       "credential=no | identity-resolution=no | sign=no | verify=no | "
                       "signature-write=no | upload=no | publish=no | launch=no | "
                       "adapter=no | deploy=no | save=no"));
                ++rowIndex;
            }
        }

        m_summary->setText(
            tr("Registered release signing-result envelopes: %1 | reported attempts: "
               "%2 | reported successes: %3 | reported failures: %4 | reported skips: "
               "%5 | supplied signature artifacts: %6 | supplied failure records: %7 | "
               "safe diagnostic references: %8. These are external-result claims only. "
               "Contract acceptance requires the exact upstream inputs and the release-"
               "signing evidence service; this pane does not infer, authenticate, or "
               "verify acceptance.")
                .arg(
                    QString::number(static_cast<qulonglong>(envelopes.size())),
                    QString::number(static_cast<qulonglong>(attemptedCount)),
                    QString::number(static_cast<qulonglong>(succeededCount)),
                    QString::number(static_cast<qulonglong>(failedCount)),
                    QString::number(static_cast<qulonglong>(skippedCount)),
                    QString::number(static_cast<qulonglong>(signatureArtifactCount)),
                    QString::number(static_cast<qulonglong>(failureCount)),
                    QString::number(static_cast<qulonglong>(diagnosticCount))));
    }
} // namespace TaintedGrailModdingSDK
