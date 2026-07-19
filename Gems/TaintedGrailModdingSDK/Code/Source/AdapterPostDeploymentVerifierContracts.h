/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterPostDeploymentVerificationContracts.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    class SourceEvidenceRegistry;

    enum class AdapterPostDeploymentVerifierReviewDecision : AZ::u8
    {
        Unknown,
        Accepted,
        Rejected,
    };

    enum class AdapterPostDeploymentVerifierCapability : AZ::u8
    {
        TargetPresence,
        TargetFingerprint,
        TargetAbsence,
    };

    enum class AdapterPostDeploymentVerifierCheckOutcome : AZ::u8
    {
        NotRun,
        Matched,
        Mismatched,
        Failed,
        Inconclusive,
    };

    enum class AdapterPostDeploymentVerifierFailureKind : AZ::u8
    {
        Contract,
        InputBinding,
        TargetUnavailable,
        AccessDenied,
        UnsupportedCheck,
        Read,
        Hash,
        Internal,
        Unknown,
    };

    enum class AdapterPostDeploymentVerifierDiagnosticKind : AZ::u8
    {
        Verifier,
        Filesystem,
        Hash,
        Binding,
        Diagnostic,
    };

    enum class AdapterPostDeploymentVerifierEnvelopeStatus : AZ::u8
    {
        Accepted,
        ReportNotReady,
        VerifierUnreviewed,
        ReportBindingMismatch,
        EnvelopeInvalid,
        CheckCoverageIncomplete,
        FailureDiagnosticBindingMismatch,
        ObservationMismatch,
    };

    AZStd::string ToString(AdapterPostDeploymentVerifierReviewDecision decision);
    AZStd::string ToString(AdapterPostDeploymentVerifierCapability capability);
    AZStd::string ToString(AdapterPostDeploymentVerifierCheckOutcome outcome);
    AZStd::string ToString(AdapterPostDeploymentVerifierFailureKind kind);
    AZStd::string ToString(AdapterPostDeploymentVerifierDiagnosticKind kind);
    AZStd::string ToString(AdapterPostDeploymentVerifierEnvelopeStatus status);

    bool TryParseAdapterPostDeploymentVerifierReviewDecision(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierReviewDecision& decision);
    bool TryParseAdapterPostDeploymentVerifierCapability(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierCapability& capability);
    bool TryParseAdapterPostDeploymentVerifierCheckOutcome(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierCheckOutcome& outcome);
    bool TryParseAdapterPostDeploymentVerifierFailureKind(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierFailureKind& kind);
    bool TryParseAdapterPostDeploymentVerifierDiagnosticKind(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierDiagnosticKind& kind);
    bool TryParseAdapterPostDeploymentVerifierEnvelopeStatus(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierEnvelopeStatus& status);

    bool IsAdapterPostDeploymentVerifierStableId(const AZStd::string& value);
    bool IsAdapterPostDeploymentVerifierFingerprint(const AZStd::string& value);
    bool IsAdapterPostDeploymentVerifierDiagnosticReference(
        const AZStd::string& value);
    bool IsAdapterPostDeploymentVerifierUtcTimestamp(const AZStd::string& value);

    struct AdapterPostDeploymentVerifierReview
    {
        AZStd::string m_reviewId;
        AZStd::string m_verifierId;
        AZStd::string m_verifierVersion;
        AZStd::string m_verifierFingerprint;
        AdapterPostDeploymentVerifierReviewDecision m_decision =
            AdapterPostDeploymentVerifierReviewDecision::Unknown;
        AZStd::string m_reviewer;
        AZStd::vector<AdapterPostDeploymentVerifierCapability> m_capabilities;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_reviewedAtUtc;
        AZStd::string m_notes;
    };

    struct AdapterPostDeploymentVerifierFailure
    {
        AZStd::string m_failureId;
        AdapterPostDeploymentVerifierFailureKind m_kind =
            AdapterPostDeploymentVerifierFailureKind::Unknown;
        AZStd::string m_code;
        AZStd::string m_message;
        AZStd::string m_checkId;
        AZStd::vector<AZStd::string> m_diagnosticReferenceIds;
        bool m_retryable = false;
    };

    struct AdapterPostDeploymentVerifierDiagnosticReference
    {
        AZStd::string m_diagnosticId;
        AdapterPostDeploymentVerifierDiagnosticKind m_kind =
            AdapterPostDeploymentVerifierDiagnosticKind::Diagnostic;
        AZStd::string m_reference;
        AZStd::string m_fingerprint;
        AZStd::vector<AZStd::string> m_checkIds;
    };

    struct AdapterPostDeploymentVerifierCheckResult
    {
        AZStd::string m_checkId;
        AZ::u64 m_sequence = 0;
        AZStd::string m_stepId;
        AZStd::string m_targetPath;
        AZStd::string m_expectedFingerprint;
        AZStd::string m_observedFingerprint;
        bool m_expectedPresent = false;
        bool m_observedPresent = false;
        bool m_attempted = false;
        bool m_observationRecorded = false;
        AdapterPostDeploymentVerifierCheckOutcome m_outcome =
            AdapterPostDeploymentVerifierCheckOutcome::NotRun;
        AZStd::string m_checkedAtUtc;
        AZStd::vector<AZStd::string> m_failureIds;
        AZStd::vector<AZStd::string> m_diagnosticReferenceIds;
    };

    struct AdapterPostDeploymentVerifierResultEnvelope
    {
        AZ::u32 m_contractVersion = 1;
        AZStd::string m_verifierResultId;
        AZStd::string m_reportId;
        AdapterPostDeploymentReportStatus m_reportStatus =
            AdapterPostDeploymentReportStatus::EvidenceRejected;
        AZStd::string m_reportCanonicalJson;
        AZStd::string m_resultId;
        AZStd::string m_workOrderId;
        AZStd::string m_workOrderFingerprint;
        AZStd::string m_executionResultFingerprint;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AdapterPostDeploymentVerifierReview m_verifierReview;
        AZStd::string m_capturedAtUtc;
        AZStd::string m_resultFingerprint;
        AZStd::vector<AdapterPostDeploymentVerifierCheckResult> m_checkResults;
        AZStd::vector<AdapterPostDeploymentVerifierFailure> m_failures;
        AZStd::vector<AdapterPostDeploymentVerifierDiagnosticReference>
            m_diagnosticReferences;
    };

    class AdapterPostDeploymentVerifierResultRegistry
    {
    public:
        static AdapterPostDeploymentVerifierResultRegistry& Get();

        // Unbound registration is refused. A verifier result may enter session
        // state only after validation against the exact current work order,
        // execution result, post-deployment report, and active research registry.
        bool RegisterEnvelope(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AZStd::string* error = nullptr);

        bool RegisterEnvelope(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AdapterPostDeploymentVerificationReport& report,
            const SourceEvidenceRegistry& sourceRegistry,
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AZStd::string* error = nullptr);
        void Clear();

        const AdapterPostDeploymentVerifierResultEnvelope* FindByResultId(
            const AZStd::string& verifierResultId) const;
        const AZStd::vector<AdapterPostDeploymentVerifierResultEnvelope>&
            GetEnvelopes() const;

    private:
        AZStd::vector<AdapterPostDeploymentVerifierResultEnvelope> m_envelopes;
    };
} // namespace TaintedGrailModdingSDK
