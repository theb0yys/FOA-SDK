/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterPostDeploymentVerifierContracts.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    struct AdapterPostDeploymentVerifierEvidenceReturn;

    enum class AdapterVerifierCompatibilityAssessment : AZ::u8
    {
        Unassessed,
        Clear,
        Blocked,
        Inconclusive,
    };

    enum class AdapterVerifierReleaseDecision : AZ::u8
    {
        Pending,
        Hold,
        Rejected,
        Approved,
    };

    enum class AdapterVerifierHumanReviewState : AZ::u8
    {
        Missing,
        Invalid,
        DispositionRequired,
        Complete,
    };

    enum class AdapterVerifierReleaseReviewDecision : AZ::u8
    {
        Unknown,
        Hold,
        Reject,
        Approve,
    };

    enum class AdapterVerifierReconciliationFindingKind : AZ::u8
    {
        ReportBlocker,
        VerifierMatched,
        VerifierMismatched,
        VerifierFailed,
        VerifierInconclusive,
        VerifierNotRun,
    };

    enum class AdapterVerifierEvidenceRelationship : AZ::u8
    {
        Preserved,
        Supporting,
        Contradictory,
        NewFinding,
    };

    enum class AdapterVerifierFindingDispositionDecision : AZ::u8
    {
        Unknown,
        Accepted,
        Rejected,
        Deferred,
    };

    enum class AdapterVerifierReconciliationEnvelopeStatus : AZ::u8
    {
        Accepted,
        ReportNotReady,
        VerifierEvidenceInvalid,
        BindingMismatch,
        ReviewMissing,
        ReviewInvalid,
        DispositionIncomplete,
        DecisionInconsistent,
    };

    AZStd::string ToString(AdapterVerifierCompatibilityAssessment assessment);
    AZStd::string ToString(AdapterVerifierReleaseDecision decision);
    AZStd::string ToString(AdapterVerifierHumanReviewState state);
    AZStd::string ToString(AdapterVerifierReleaseReviewDecision decision);
    AZStd::string ToString(AdapterVerifierReconciliationFindingKind kind);
    AZStd::string ToString(AdapterVerifierEvidenceRelationship relationship);
    AZStd::string ToString(AdapterVerifierFindingDispositionDecision decision);
    AZStd::string ToString(AdapterVerifierReconciliationEnvelopeStatus status);

    bool TryParseAdapterVerifierCompatibilityAssessment(
        const AZStd::string& value,
        AdapterVerifierCompatibilityAssessment& assessment);
    bool TryParseAdapterVerifierReleaseDecision(
        const AZStd::string& value,
        AdapterVerifierReleaseDecision& decision);
    bool TryParseAdapterVerifierHumanReviewState(
        const AZStd::string& value,
        AdapterVerifierHumanReviewState& state);
    bool TryParseAdapterVerifierReleaseReviewDecision(
        const AZStd::string& value,
        AdapterVerifierReleaseReviewDecision& decision);
    bool TryParseAdapterVerifierReconciliationFindingKind(
        const AZStd::string& value,
        AdapterVerifierReconciliationFindingKind& kind);
    bool TryParseAdapterVerifierEvidenceRelationship(
        const AZStd::string& value,
        AdapterVerifierEvidenceRelationship& relationship);
    bool TryParseAdapterVerifierFindingDispositionDecision(
        const AZStd::string& value,
        AdapterVerifierFindingDispositionDecision& decision);
    bool TryParseAdapterVerifierReconciliationEnvelopeStatus(
        const AZStd::string& value,
        AdapterVerifierReconciliationEnvelopeStatus& status);

    struct AdapterVerifierFindingDisposition
    {
        AZStd::string m_findingId;
        AdapterVerifierFindingDispositionDecision m_decision =
            AdapterVerifierFindingDispositionDecision::Unknown;
        AZStd::string m_rationale;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };

    struct AdapterVerifierReleaseReview
    {
        AZStd::string m_reviewId;
        AZStd::string m_reportId;
        AZStd::string m_reportCanonicalJson;
        AZStd::string m_verifierResultId;
        AZStd::string m_workOrderFingerprint;
        AZStd::string m_executionResultFingerprint;
        AZStd::string m_verifierResultFingerprint;
        AZStd::vector<AZStd::string> m_candidateSourceIds;
        AZStd::vector<AZStd::string> m_candidateEvidenceIds;
        AdapterVerifierReleaseReviewDecision m_decision =
            AdapterVerifierReleaseReviewDecision::Unknown;
        AZStd::string m_reviewer;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_reviewedAtUtc;
        AZStd::string m_rationale;
        AZStd::vector<AdapterVerifierFindingDisposition> m_dispositions;
    };

    struct AdapterVerifierReconciliationFinding
    {
        AZStd::string m_findingId;
        AdapterVerifierReconciliationFindingKind m_kind =
            AdapterVerifierReconciliationFindingKind::ReportBlocker;
        AdapterVerifierEvidenceRelationship m_relationship =
            AdapterVerifierEvidenceRelationship::Preserved;
        AZStd::string m_subjectRef;
        AZStd::string m_reportBlockerId;
        AZStd::string m_checkId;
        AZStd::string m_stepId;
        AZStd::string m_message;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_diagnosticReferenceIds;
        bool m_existingBlockerPreserved = false;
        bool m_blocksCompatibility = false;
        bool m_blocksRelease = false;
        bool m_requiresHumanDisposition = false;
    };

    struct AdapterVerifierEvidenceReconciliationRequest
    {
        AZStd::string m_reconciliationId;
        AZStd::string m_evaluatedAtUtc;
        AdapterVerifierReleaseReview m_releaseReview;
    };

    struct AdapterVerifierEvidenceReconciliationEnvelope
    {
        AZ::u32 m_contractVersion = 1;
        AZStd::string m_reconciliationId;
        AZStd::string m_reportId;
        AdapterPostDeploymentReportStatus m_reportStatus =
            AdapterPostDeploymentReportStatus::EvidenceRejected;
        AZStd::string m_reportCanonicalJson;
        AZStd::string m_verifierResultId;
        AdapterPostDeploymentVerifierEnvelopeStatus m_verifierEvidenceStatus =
            AdapterPostDeploymentVerifierEnvelopeStatus::ReportNotReady;
        AZStd::string m_executionResultId;
        AZStd::string m_workOrderId;
        AZStd::string m_workOrderFingerprint;
        AZStd::string m_executionResultFingerprint;
        AZStd::string m_verifierResultFingerprint;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_packId;
        AZStd::string m_previewId;
        AZStd::string m_previewFingerprint;
        AZStd::string m_targetInventoryId;
        AZStd::string m_evaluatedAtUtc;
        AdapterVerifierReconciliationEnvelopeStatus m_status =
            AdapterVerifierReconciliationEnvelopeStatus::ReportNotReady;
        AdapterVerifierCompatibilityAssessment m_compatibilityAssessment =
            AdapterVerifierCompatibilityAssessment::Unassessed;
        AdapterVerifierReleaseDecision m_releaseDecision =
            AdapterVerifierReleaseDecision::Pending;
        AdapterVerifierHumanReviewState m_humanReviewState =
            AdapterVerifierHumanReviewState::Missing;
        AdapterVerifierReleaseReview m_releaseReview;
        AZ::u64 m_reportBlockerCount = 0;
        AZ::u64 m_verifierCheckCount = 0;
        AZ::u64 m_findingCount = 0;
        AZ::u64 m_compatibilityBlockerCount = 0;
        AZ::u64 m_releaseBlockerCount = 0;
        AZ::u64 m_requiredDispositionCount = 0;
        AZ::u64 m_completedDispositionCount = 0;
        AZStd::vector<AZStd::string> m_inputCandidateSourceIds;
        AZStd::vector<AZStd::string> m_inputCandidateEvidenceIds;
        AZStd::vector<AdapterVerifierReconciliationFinding> m_findings;
        AZStd::string m_canonicalJson;
        bool m_humanReviewRequired = true;
        bool m_verifierExecuted = false;
        bool m_targetAccessed = false;
        bool m_filesMutated = false;
        bool m_evidencePromoted = false;
        bool m_archiveAssembled = false;
        bool m_archiveSigned = false;
        bool m_releasePublished = false;
        bool m_launchPerformed = false;
        bool m_adapterCalled = false;
    };

    class AdapterVerifierEvidenceReconciliationRegistry
    {
    public:
        static AdapterVerifierEvidenceReconciliationRegistry& Get();

        // Unbound registration is refused. Requests enter session state only
        // after the complete reconciliation service accepts the exact upstream
        // report, verifier evidence, bindings, review, and dispositions.
        bool RegisterRequest(
            const AdapterVerifierEvidenceReconciliationRequest& request,
            AZStd::string* error = nullptr);

        bool RegisterRequest(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope,
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            const AdapterVerifierEvidenceReconciliationRequest& request,
            AZStd::string* error = nullptr);
        void Clear();

        const AZStd::vector<AdapterVerifierEvidenceReconciliationRequest>&
            GetRequests() const;

    private:
        AZStd::vector<AdapterVerifierEvidenceReconciliationRequest> m_requests;
    };
} // namespace TaintedGrailModdingSDK
