/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterVerifierEvidenceReconciliationService.h"

#include "AdapterContractRegistry.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        struct ReconciliationFlags
        {
            bool m_reportNotReady = false;
            bool m_verifierEvidenceInvalid = false;
            bool m_bindingMismatch = false;
            bool m_reviewMissing = false;
            bool m_reviewInvalid = false;
            bool m_dispositionIncomplete = false;
            bool m_decisionInconsistent = false;
        };

        void AddIssue(
            AdapterVerifierEvidenceReconciliationResult& result,
            bool& flag,
            AZStd::string code,
            AZStd::string message,
            AZStd::string findingId = {},
            AZStd::string reportBlockerId = {},
            AZStd::string checkId = {},
            AZStd::string dispositionFindingId = {})
        {
            flag = true;
            AdapterVerifierEvidenceReconciliationIssue issue;
            issue.m_code = AZStd::move(code);
            issue.m_message = AZStd::move(message);
            issue.m_findingId = AZStd::move(findingId);
            issue.m_reportBlockerId = AZStd::move(reportBlockerId);
            issue.m_checkId = AZStd::move(checkId);
            issue.m_dispositionFindingId = AZStd::move(dispositionFindingId);
            result.m_issues.push_back(AZStd::move(issue));
        }

        AZStd::string UnsignedString(AZ::u64 value)
        {
            char buffer[32];
            size_t position = sizeof(buffer);
            do
            {
                buffer[--position] = static_cast<char>('0' + (value % 10));
                value /= 10;
            } while (value != 0);
            return AZStd::string(buffer + position, sizeof(buffer) - position);
        }

        void SortUnique(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        bool IsMutationStep(AdapterDeploymentWorkOrderStepKind kind)
        {
            return kind == AdapterDeploymentWorkOrderStepKind::Add
                || kind == AdapterDeploymentWorkOrderStepKind::Replace
                || kind == AdapterDeploymentWorkOrderStepKind::Remove;
        }

        bool ContainsStableUniqueIds(const AZStd::vector<AZStd::string>& values)
        {
            if (values.empty())
            {
                return false;
            }
            AZStd::vector<AZStd::string> sorted = values;
            for (const AZStd::string& value : sorted)
            {
                if (!IsAdapterPostDeploymentVerifierStableId(value))
                {
                    return false;
                }
            }
            AZStd::sort(sorted.begin(), sorted.end());
            return AZStd::adjacent_find(sorted.begin(), sorted.end())
                == sorted.end();
        }

        const AdapterPostDeploymentBlocker* FindReportBlockerForStep(
            const AdapterPostDeploymentVerificationReport& report,
            const AZStd::string& stepId)
        {
            for (const AdapterPostDeploymentBlocker& blocker : report.m_blockers)
            {
                if (!stepId.empty() && blocker.m_stepId == stepId)
                {
                    return &blocker;
                }
            }
            return nullptr;
        }

        const AdapterVerifierReconciliationFinding* FindFinding(
            const AdapterVerifierEvidenceReconciliationEnvelope& envelope,
            const AZStd::string& findingId)
        {
            for (const AdapterVerifierReconciliationFinding& finding :
                 envelope.m_findings)
            {
                if (finding.m_findingId == findingId)
                {
                    return &finding;
                }
            }
            return nullptr;
        }

        void GatherCandidateIds(
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            AdapterVerifierEvidenceReconciliationEnvelope& envelope)
        {
            envelope.m_inputCandidateSourceIds = report.m_candidateSourceIds;
            envelope.m_inputCandidateEvidenceIds = report.m_candidateEvidenceIds;

            for (const SourceDocument& document : verifierEvidence.m_sourceDocuments)
            {
                envelope.m_inputCandidateSourceIds.push_back(
                    document.m_source.m_sourceId);
            }
            for (const EvidenceDocument& document : verifierEvidence.m_evidenceDocuments)
            {
                for (const EvidenceRecord& evidence : document.m_evidence)
                {
                    envelope.m_inputCandidateEvidenceIds.push_back(
                        evidence.m_evidenceId);
                }
            }
            SortUnique(envelope.m_inputCandidateSourceIds);
            SortUnique(envelope.m_inputCandidateEvidenceIds);
        }

        void ValidateReportReadiness(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AdapterPostDeploymentVerificationReport& report,
            AdapterVerifierEvidenceReconciliationResult& result,
            ReconciliationFlags& flags)
        {
            if (workOrder.m_status
                    != AdapterDeploymentWorkOrderStatus::ReviewReady
                || workOrder.m_workOrderId != executionEnvelope.m_workOrderId
                || workOrder.m_canonicalJson
                    != executionEnvelope.m_workOrderCanonicalJson
                || workOrder.m_previewId != executionEnvelope.m_previewId
                || workOrder.m_previewFingerprint
                    != executionEnvelope.m_previewFingerprint
                || workOrder.m_packId != executionEnvelope.m_packId
                || workOrder.m_targetInventoryId
                    != executionEnvelope.m_targetInventoryId
                || workOrder.m_executionAllowed
                || workOrder.m_copyAllowed
                || workOrder.m_deleteAllowed
                || workOrder.m_backupAllowed
                || workOrder.m_restoreAllowed
                || workOrder.m_deploymentAllowed
                || workOrder.m_launchAllowed
                || report.m_status
                    == AdapterPostDeploymentReportStatus::EvidenceRejected
                || report.m_status
                    == AdapterPostDeploymentReportStatus::EvidenceIncomplete
                || !report.m_executionEvidenceAccepted
                || !report.m_humanReviewRequired
                || report.m_verifierExecuted
                || report.m_evidencePromoted
                || report.m_releasePublished
                || report.m_launchPerformed
                || report.m_adapterCalled
                || report.m_reportId.empty()
                || report.m_resultId != executionEnvelope.m_resultId
                || report.m_workOrderId != executionEnvelope.m_workOrderId
                || report.m_workOrderFingerprint
                    != executionEnvelope.m_workOrderFingerprint
                || report.m_resultFingerprint
                    != executionEnvelope.m_resultFingerprint
                || report.m_profileId != executionEnvelope.m_profileId
                || report.m_gameVersion != executionEnvelope.m_gameVersion
                || report.m_branch != executionEnvelope.m_branch
                || report.m_runtimeTarget != executionEnvelope.m_runtimeTarget)
            {
                AddIssue(
                    result,
                    flags.m_reportNotReady,
                    "verifier_reconciliation.report_not_ready",
                    "Reconciliation requires one exact current structurally eligible "
                    "post-deployment report backed by a review-ready work order and "
                    "accepted execution evidence, with every execution, mutation, "
                    "promotion, launch, adapter, and publication flag false.");
            }
        }

        void ValidateVerifierEvidence(
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope,
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            AdapterVerifierEvidenceReconciliationResult& result,
            ReconciliationFlags& flags)
        {
            const bool structurallyValidStatus = verifierEvidence.m_status
                    == AdapterPostDeploymentVerifierEnvelopeStatus::Accepted
                || verifierEvidence.m_status
                    == AdapterPostDeploymentVerifierEnvelopeStatus::ObservationMismatch;
            if (!verifierEvidence.m_contractValid
                || !structurallyValidStatus
                || verifierEvidence.m_verifierResultId
                    != verifierEnvelope.m_verifierResultId
                || verifierEvidence.m_reportId != report.m_reportId
                || verifierEvidence.m_resultId != report.m_resultId
                || verifierEvidence.m_workOrderId != report.m_workOrderId
                || verifierEvidence.m_resultFingerprint
                    != verifierEnvelope.m_resultFingerprint)
            {
                AddIssue(
                    result,
                    flags.m_verifierEvidenceInvalid,
                    "verifier_reconciliation.verifier_evidence_invalid",
                    "Reconciliation requires one structurally valid independent-verifier "
                    "evidence return. Both accepted and observation_mismatch results are "
                    "eligible; malformed or rejected verifier evidence is not.");
            }
        }

        void ValidateExactBinding(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope,
            const AdapterVerifierEvidenceReconciliationRequest& request,
            AdapterVerifierEvidenceReconciliationResult& result,
            ReconciliationFlags& flags)
        {
            const AdapterVerifierReleaseReview& review = request.m_releaseReview;
            if (!IsAdapterPostDeploymentVerifierStableId(
                    request.m_reconciliationId)
                || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                    request.m_evaluatedAtUtc)
                || request.m_reconciliationId.empty()
                || review.m_reportId != report.m_reportId
                || review.m_reportCanonicalJson
                    != verifierEnvelope.m_reportCanonicalJson
                || review.m_verifierResultId
                    != verifierEnvelope.m_verifierResultId
                || review.m_workOrderFingerprint
                    != workOrder.m_canonicalJson.empty()
                        ? review.m_workOrderFingerprint
                            != executionEnvelope.m_workOrderFingerprint
                        : true
                || review.m_executionResultFingerprint
                    != executionEnvelope.m_resultFingerprint
                || review.m_verifierResultFingerprint
                    != verifierEnvelope.m_resultFingerprint
                || verifierEnvelope.m_reportId != report.m_reportId
                || verifierEnvelope.m_reportStatus != report.m_status
                || verifierEnvelope.m_resultId != executionEnvelope.m_resultId
                || verifierEnvelope.m_workOrderId != workOrder.m_workOrderId
                || verifierEnvelope.m_workOrderFingerprint
                    != executionEnvelope.m_workOrderFingerprint
                || verifierEnvelope.m_executionResultFingerprint
                    != executionEnvelope.m_resultFingerprint
                || verifierEnvelope.m_profileId != executionEnvelope.m_profileId
                || verifierEnvelope.m_gameVersion != executionEnvelope.m_gameVersion
                || verifierEnvelope.m_branch != executionEnvelope.m_branch
                || verifierEnvelope.m_runtimeTarget
                    != executionEnvelope.m_runtimeTarget)
            {
                AddIssue(
                    result,
                    flags.m_bindingMismatch,
                    "verifier_reconciliation.binding_mismatch",
                    "The reconciliation request and release review must bind to the exact "
                    "current report JSON, verifier result, execution result, work order, "
                    "fingerprints, and profile/game/branch/runtime context.");
            }
        }

        AdapterVerifierReconciliationFinding MakeReportBlockerFinding(
            const AdapterPostDeploymentBlocker& blocker)
        {
            AdapterVerifierReconciliationFinding finding;
            finding.m_findingId =
                "verifier-reconciliation.finding.report." + blocker.m_blockerId;
            finding.m_kind =
                AdapterVerifierReconciliationFindingKind::ReportBlocker;
            finding.m_relationship =
                AdapterVerifierEvidenceRelationship::Preserved;
            finding.m_subjectRef = blocker.m_subjectRef;
            finding.m_reportBlockerId = blocker.m_blockerId;
            finding.m_stepId = blocker.m_stepId;
            finding.m_message = blocker.m_message;
            finding.m_evidenceIds = blocker.m_evidenceIds;
            finding.m_diagnosticReferenceIds = blocker.m_logReferenceIds;
            finding.m_existingBlockerPreserved = true;
            finding.m_blocksCompatibility = blocker.m_blocksCompatibility;
            finding.m_blocksRelease = blocker.m_blocksRelease;
            finding.m_requiresHumanDisposition = true;
            return finding;
        }

        AdapterVerifierReconciliationFinding MakeCheckFinding(
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierCheckResult& check)
        {
            AdapterVerifierReconciliationFinding finding;
            finding.m_findingId =
                "verifier-reconciliation.finding.check." + check.m_checkId;
            finding.m_subjectRef = "deployment-work-order-step:" + check.m_stepId;
            finding.m_checkId = check.m_checkId;
            finding.m_stepId = check.m_stepId;
            finding.m_evidenceIds.push_back(
                "evidence.post-deployment-verifier.check." + check.m_checkId);
            finding.m_diagnosticReferenceIds =
                check.m_diagnosticReferenceIds;

            const AdapterPostDeploymentBlocker* relatedBlocker =
                FindReportBlockerForStep(report, check.m_stepId);
            switch (check.m_outcome)
            {
            case AdapterPostDeploymentVerifierCheckOutcome::Matched:
                finding.m_kind =
                    AdapterVerifierReconciliationFindingKind::VerifierMatched;
                finding.m_relationship = relatedBlocker
                    ? AdapterVerifierEvidenceRelationship::Contradictory
                    : AdapterVerifierEvidenceRelationship::Supporting;
                finding.m_message = relatedBlocker
                    ? "The supplied independent observation matches the expected target "
                      "state but does not clear the preserved report blocker."
                    : "The supplied independent observation matches the exact expected "
                      "target state.";
                finding.m_requiresHumanDisposition = relatedBlocker != nullptr;
                finding.m_blocksRelease = relatedBlocker != nullptr;
                break;
            case AdapterPostDeploymentVerifierCheckOutcome::Mismatched:
                finding.m_kind =
                    AdapterVerifierReconciliationFindingKind::VerifierMismatched;
                finding.m_relationship =
                    AdapterVerifierEvidenceRelationship::NewFinding;
                finding.m_message =
                    "The supplied independent observation differs from the exact expected "
                    "target state.";
                finding.m_blocksCompatibility = true;
                finding.m_blocksRelease = true;
                finding.m_requiresHumanDisposition = true;
                break;
            case AdapterPostDeploymentVerifierCheckOutcome::Failed:
                finding.m_kind =
                    AdapterVerifierReconciliationFindingKind::VerifierFailed;
                finding.m_relationship =
                    AdapterVerifierEvidenceRelationship::NewFinding;
                finding.m_message =
                    "The supplied independent-verifier check failed.";
                finding.m_blocksCompatibility = true;
                finding.m_blocksRelease = true;
                finding.m_requiresHumanDisposition = true;
                break;
            case AdapterPostDeploymentVerifierCheckOutcome::Inconclusive:
                finding.m_kind =
                    AdapterVerifierReconciliationFindingKind::VerifierInconclusive;
                finding.m_relationship =
                    AdapterVerifierEvidenceRelationship::NewFinding;
                finding.m_message =
                    "The supplied independent-verifier check was inconclusive.";
                finding.m_blocksCompatibility = true;
                finding.m_blocksRelease = true;
                finding.m_requiresHumanDisposition = true;
                break;
            case AdapterPostDeploymentVerifierCheckOutcome::NotRun:
                finding.m_kind =
                    AdapterVerifierReconciliationFindingKind::VerifierNotRun;
                finding.m_relationship =
                    AdapterVerifierEvidenceRelationship::NewFinding;
                finding.m_message =
                    "The required independent-verifier check was not run.";
                finding.m_blocksCompatibility = true;
                finding.m_blocksRelease = true;
                finding.m_requiresHumanDisposition = true;
                break;
            }
            return finding;
        }

        void BuildFindings(
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope,
            AdapterVerifierEvidenceReconciliationEnvelope& envelope)
        {
            for (const AdapterPostDeploymentBlocker& blocker : report.m_blockers)
            {
                envelope.m_findings.push_back(
                    MakeReportBlockerFinding(blocker));
            }
            for (const AdapterPostDeploymentVerifierCheckResult& check :
                 verifierEnvelope.m_checkResults)
            {
                envelope.m_findings.push_back(
                    MakeCheckFinding(report, check));
            }

            AZStd::sort(
                envelope.m_findings.begin(),
                envelope.m_findings.end(),
                [](const AdapterVerifierReconciliationFinding& left,
                    const AdapterVerifierReconciliationFinding& right)
                {
                    const AZStd::string leftKind = ToString(left.m_kind);
                    const AZStd::string rightKind = ToString(right.m_kind);
                    if (leftKind != rightKind)
                    {
                        return leftKind < rightKind;
                    }
                    if (left.m_subjectRef != right.m_subjectRef)
                    {
                        return left.m_subjectRef < right.m_subjectRef;
                    }
                    if (left.m_stepId != right.m_stepId)
                    {
                        return left.m_stepId < right.m_stepId;
                    }
                    if (left.m_checkId != right.m_checkId)
                    {
                        return left.m_checkId < right.m_checkId;
                    }
                    return left.m_findingId < right.m_findingId;
                });

            envelope.m_reportBlockerCount =
                static_cast<AZ::u64>(report.m_blockers.size());
            envelope.m_verifierCheckCount =
                static_cast<AZ::u64>(verifierEnvelope.m_checkResults.size());
            envelope.m_findingCount =
                static_cast<AZ::u64>(envelope.m_findings.size());
            for (const AdapterVerifierReconciliationFinding& finding :
                 envelope.m_findings)
            {
                envelope.m_compatibilityBlockerCount +=
                    finding.m_blocksCompatibility ? 1 : 0;
                envelope.m_releaseBlockerCount +=
                    finding.m_blocksRelease ? 1 : 0;
                envelope.m_requiredDispositionCount +=
                    finding.m_requiresHumanDisposition ? 1 : 0;
            }
        }

        void ResolveCompatibilityAssessment(
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            AdapterVerifierEvidenceReconciliationEnvelope& envelope)
        {
            if (!verifierEvidence.m_contractValid)
            {
                envelope.m_compatibilityAssessment =
                    AdapterVerifierCompatibilityAssessment::Unassessed;
            }
            else if (envelope.m_compatibilityBlockerCount != 0)
            {
                envelope.m_compatibilityAssessment =
                    AdapterVerifierCompatibilityAssessment::Blocked;
            }
            else if (verifierEvidence.m_failedCheckCount != 0
                || verifierEvidence.m_inconclusiveCheckCount != 0
                || verifierEvidence.m_notRunCheckCount != 0)
            {
                envelope.m_compatibilityAssessment =
                    AdapterVerifierCompatibilityAssessment::Inconclusive;
            }
            else
            {
                envelope.m_compatibilityAssessment =
                    AdapterVerifierCompatibilityAssessment::Clear;
            }
        }

        void ValidateReleaseReview(
            const AdapterVerifierEvidenceReconciliationRequest& request,
            AdapterVerifierEvidenceReconciliationResult& result,
            ReconciliationFlags& flags)
        {
            AdapterVerifierEvidenceReconciliationEnvelope& envelope =
                result.m_envelope;
            const AdapterVerifierReleaseReview& review = request.m_releaseReview;

            if (review.m_reviewId.empty()
                || review.m_reviewer.empty()
                || review.m_decision
                    == AdapterVerifierReleaseReviewDecision::Unknown)
            {
                envelope.m_humanReviewState =
                    AdapterVerifierHumanReviewState::Missing;
                AddIssue(
                    result,
                    flags.m_reviewMissing,
                    "verifier_reconciliation.review_missing",
                    "A named human release review with one explicit hold, reject, or "
                    "approve decision is required. No decision is inferred from verifier "
                    "matching or report status.");
                return;
            }

            bool invalid = !IsAdapterPostDeploymentVerifierStableId(review.m_reviewId)
                || !ContainsStableUniqueIds(review.m_evidenceIds)
                || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                    review.m_reviewedAtUtc)
                || review.m_reviewedAtUtc > request.m_evaluatedAtUtc
                || review.m_rationale.empty();

            AZStd::vector<AZStd::string> dispositionIds;
            AZStd::vector<AZStd::string> validCompletedIds;
            for (const AdapterVerifierFindingDisposition& disposition :
                 review.m_dispositions)
            {
                dispositionIds.push_back(disposition.m_findingId);
                const AdapterVerifierReconciliationFinding* finding =
                    FindFinding(envelope, disposition.m_findingId);
                const bool dispositionValid = finding
                    && finding->m_requiresHumanDisposition
                    && disposition.m_decision
                        != AdapterVerifierFindingDispositionDecision::Unknown
                    && !disposition.m_rationale.empty()
                    && ContainsStableUniqueIds(disposition.m_evidenceIds);
                invalid = invalid || !dispositionValid;
                if (dispositionValid)
                {
                    validCompletedIds.push_back(disposition.m_findingId);
                }
                else
                {
                    AddIssue(
                        result,
                        flags.m_reviewInvalid,
                        "verifier_reconciliation.disposition_invalid",
                        "Every supplied disposition must bind to one exact finding that "
                        "requires human review and include an explicit decision, rationale, "
                        "and stable evidence IDs.",
                        {},
                        {},
                        {},
                        disposition.m_findingId);
                }
            }

            AZStd::sort(dispositionIds.begin(), dispositionIds.end());
            if (AZStd::adjacent_find(
                    dispositionIds.begin(),
                    dispositionIds.end())
                != dispositionIds.end())
            {
                invalid = true;
                AddIssue(
                    result,
                    flags.m_reviewInvalid,
                    "verifier_reconciliation.duplicate_disposition",
                    "A reconciliation finding may have at most one human disposition.");
            }

            SortUnique(validCompletedIds);
            envelope.m_completedDispositionCount =
                static_cast<AZ::u64>(validCompletedIds.size());

            for (const AdapterVerifierReconciliationFinding& finding :
                 envelope.m_findings)
            {
                if (finding.m_requiresHumanDisposition
                    && AZStd::find(
                           validCompletedIds.begin(),
                           validCompletedIds.end(),
                           finding.m_findingId)
                        == validCompletedIds.end())
                {
                    AddIssue(
                        result,
                        flags.m_dispositionIncomplete,
                        "verifier_reconciliation.disposition_missing",
                        "Every preserved blocker, adverse verifier result, and contradictory "
                        "matched observation requires one explicit human disposition.",
                        finding.m_findingId);
                }
            }

            if (invalid)
            {
                envelope.m_humanReviewState =
                    AdapterVerifierHumanReviewState::Invalid;
                AddIssue(
                    result,
                    flags.m_reviewInvalid,
                    "verifier_reconciliation.review_invalid",
                    "The supplied release review requires stable identity, named reviewer, "
                    "unique evidence, UTC review time no later than evaluation, rationale, "
                    "and exact finding dispositions.");
            }
            else if (flags.m_dispositionIncomplete)
            {
                envelope.m_humanReviewState =
                    AdapterVerifierHumanReviewState::DispositionRequired;
            }
            else
            {
                envelope.m_humanReviewState =
                    AdapterVerifierHumanReviewState::Complete;
            }
        }

        void ResolveReleaseDecision(
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            AdapterVerifierEvidenceReconciliationResult& result,
            ReconciliationFlags& flags)
        {
            AdapterVerifierEvidenceReconciliationEnvelope& envelope =
                result.m_envelope;
            const AdapterVerifierReleaseReviewDecision reviewDecision =
                envelope.m_releaseReview.m_decision;
            switch (reviewDecision)
            {
            case AdapterVerifierReleaseReviewDecision::Unknown:
                envelope.m_releaseDecision =
                    AdapterVerifierReleaseDecision::Pending;
                return;
            case AdapterVerifierReleaseReviewDecision::Hold:
                envelope.m_releaseDecision =
                    AdapterVerifierReleaseDecision::Hold;
                break;
            case AdapterVerifierReleaseReviewDecision::Reject:
                envelope.m_releaseDecision =
                    AdapterVerifierReleaseDecision::Rejected;
                break;
            case AdapterVerifierReleaseReviewDecision::Approve:
                envelope.m_releaseDecision =
                    AdapterVerifierReleaseDecision::Approved;
                break;
            }

            if (reviewDecision == AdapterVerifierReleaseReviewDecision::Approve
                && (envelope.m_compatibilityAssessment
                        != AdapterVerifierCompatibilityAssessment::Clear
                    || envelope.m_releaseBlockerCount != 0
                    || verifierEvidence.m_status
                        != AdapterPostDeploymentVerifierEnvelopeStatus::Accepted
                    || !report.m_compatibilityClear
                    || !report.m_releaseBlockerFree
                    || envelope.m_humanReviewState
                        != AdapterVerifierHumanReviewState::Complete))
            {
                AddIssue(
                    result,
                    flags.m_decisionInconsistent,
                    "verifier_reconciliation.approval_inconsistent",
                    "Approval is invalid while any preserved report blocker, adverse or "
                    "incomplete verifier observation, compatibility uncertainty, release "
                    "blocker, or incomplete human disposition remains. Matching metadata "
                    "never grants approval automatically.");
            }
        }

        AdapterVerifierReconciliationEnvelopeStatus ResolveStatus(
            const ReconciliationFlags& flags)
        {
            if (flags.m_reportNotReady)
            {
                return AdapterVerifierReconciliationEnvelopeStatus::ReportNotReady;
            }
            if (flags.m_verifierEvidenceInvalid)
            {
                return AdapterVerifierReconciliationEnvelopeStatus::
                    VerifierEvidenceInvalid;
            }
            if (flags.m_bindingMismatch)
            {
                return AdapterVerifierReconciliationEnvelopeStatus::BindingMismatch;
            }
            if (flags.m_reviewMissing)
            {
                return AdapterVerifierReconciliationEnvelopeStatus::ReviewMissing;
            }
            if (flags.m_reviewInvalid)
            {
                return AdapterVerifierReconciliationEnvelopeStatus::ReviewInvalid;
            }
            if (flags.m_dispositionIncomplete)
            {
                return AdapterVerifierReconciliationEnvelopeStatus::
                    DispositionIncomplete;
            }
            if (flags.m_decisionInconsistent)
            {
                return AdapterVerifierReconciliationEnvelopeStatus::
                    DecisionInconsistent;
            }
            return AdapterVerifierReconciliationEnvelopeStatus::Accepted;
        }

        void SortIssues(AdapterVerifierEvidenceReconciliationResult& result)
        {
            AZStd::sort(
                result.m_issues.begin(),
                result.m_issues.end(),
                [](const AdapterVerifierEvidenceReconciliationIssue& left,
                    const AdapterVerifierEvidenceReconciliationIssue& right)
                {
                    if (left.m_code != right.m_code)
                    {
                        return left.m_code < right.m_code;
                    }
                    if (left.m_findingId != right.m_findingId)
                    {
                        return left.m_findingId < right.m_findingId;
                    }
                    if (left.m_reportBlockerId != right.m_reportBlockerId)
                    {
                        return left.m_reportBlockerId < right.m_reportBlockerId;
                    }
                    if (left.m_checkId != right.m_checkId)
                    {
                        return left.m_checkId < right.m_checkId;
                    }
                    if (left.m_dispositionFindingId
                        != right.m_dispositionFindingId)
                    {
                        return left.m_dispositionFindingId
                            < right.m_dispositionFindingId;
                    }
                    return left.m_message < right.m_message;
                });
        }

        void AppendJsonEscaped(AZStd::string& output, const AZStd::string& value)
        {
            constexpr char HexDigits[] = "0123456789abcdef";
            output.push_back('"');
            for (char character : value)
            {
                const unsigned char byte =
                    static_cast<unsigned char>(character);
                switch (character)
                {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\b':
                    output += "\\b";
                    break;
                case '\f':
                    output += "\\f";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    if (byte < 0x20)
                    {
                        output += "\\u00";
                        output.push_back(HexDigits[(byte >> 4) & 0x0f]);
                        output.push_back(HexDigits[byte & 0x0f]);
                    }
                    else
                    {
                        output.push_back(character);
                    }
                    break;
                }
            }
            output.push_back('"');
        }

        void AppendName(AZStd::string& output, const char* name)
        {
            AppendJsonEscaped(output, name);
            output.push_back(':');
        }

        void AppendString(
            AZStd::string& output,
            const char* name,
            const AZStd::string& value,
            bool comma = true)
        {
            AppendName(output, name);
            AppendJsonEscaped(output, value);
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendUnsigned(
            AZStd::string& output,
            const char* name,
            AZ::u64 value,
            bool comma = true)
        {
            AppendName(output, name);
            output += UnsignedString(value);
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendBool(
            AZStd::string& output,
            const char* name,
            bool value,
            bool comma = true)
        {
            AppendName(output, name);
            output += value ? "true" : "false";
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendStringArray(
            AZStd::string& output,
            const char* name,
            AZStd::vector<AZStd::string> values,
            bool comma = true)
        {
            SortUnique(values);
            AppendName(output, name);
            output.push_back('[');
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                AppendJsonEscaped(output, values[index]);
            }
            output.push_back(']');
            if (comma)
            {
                output.push_back(',');
            }
        }

        AZStd::string SerializeEnvelope(
            const AdapterVerifierEvidenceReconciliationEnvelope& envelope)
        {
            AZStd::string output;
            output.push_back('{');
            AppendUnsigned(output, "ContractVersion", envelope.m_contractVersion);
            AppendString(output, "ReconciliationId", envelope.m_reconciliationId);
            AppendString(output, "ReportId", envelope.m_reportId);
            AppendString(output, "ReportStatus", ToString(envelope.m_reportStatus));
            AppendString(output, "ReportCanonicalJson", envelope.m_reportCanonicalJson);
            AppendString(output, "VerifierResultId", envelope.m_verifierResultId);
            AppendString(
                output,
                "VerifierEvidenceStatus",
                ToString(envelope.m_verifierEvidenceStatus));
            AppendString(output, "ExecutionResultId", envelope.m_executionResultId);
            AppendString(output, "WorkOrderId", envelope.m_workOrderId);
            AppendString(
                output,
                "WorkOrderFingerprint",
                envelope.m_workOrderFingerprint);
            AppendString(
                output,
                "ExecutionResultFingerprint",
                envelope.m_executionResultFingerprint);
            AppendString(
                output,
                "VerifierResultFingerprint",
                envelope.m_verifierResultFingerprint);
            AppendString(output, "ProfileId", envelope.m_profileId);
            AppendString(output, "GameVersion", envelope.m_gameVersion);
            AppendString(output, "Branch", envelope.m_branch);
            AppendString(output, "RuntimeTarget", envelope.m_runtimeTarget);
            AppendString(output, "PackId", envelope.m_packId);
            AppendString(output, "PreviewId", envelope.m_previewId);
            AppendString(
                output,
                "PreviewFingerprint",
                envelope.m_previewFingerprint);
            AppendString(
                output,
                "TargetInventoryId",
                envelope.m_targetInventoryId);
            AppendString(output, "EvaluatedAtUtc", envelope.m_evaluatedAtUtc);
            AppendString(output, "Status", ToString(envelope.m_status));
            AppendString(
                output,
                "CompatibilityAssessment",
                ToString(envelope.m_compatibilityAssessment));
            AppendString(
                output,
                "ReleaseDecision",
                ToString(envelope.m_releaseDecision));
            AppendString(
                output,
                "HumanReviewState",
                ToString(envelope.m_humanReviewState));
            AppendUnsigned(
                output,
                "ReportBlockerCount",
                envelope.m_reportBlockerCount);
            AppendUnsigned(
                output,
                "VerifierCheckCount",
                envelope.m_verifierCheckCount);
            AppendUnsigned(output, "FindingCount", envelope.m_findingCount);
            AppendUnsigned(
                output,
                "CompatibilityBlockerCount",
                envelope.m_compatibilityBlockerCount);
            AppendUnsigned(
                output,
                "ReleaseBlockerCount",
                envelope.m_releaseBlockerCount);
            AppendUnsigned(
                output,
                "RequiredDispositionCount",
                envelope.m_requiredDispositionCount);
            AppendUnsigned(
                output,
                "CompletedDispositionCount",
                envelope.m_completedDispositionCount);
            AppendStringArray(
                output,
                "InputCandidateSourceIds",
                envelope.m_inputCandidateSourceIds);
            AppendStringArray(
                output,
                "InputCandidateEvidenceIds",
                envelope.m_inputCandidateEvidenceIds);

            AppendName(output, "ReleaseReview");
            output.push_back('{');
            AppendString(output, "ReviewId", envelope.m_releaseReview.m_reviewId);
            AppendString(output, "ReportId", envelope.m_releaseReview.m_reportId);
            AppendString(
                output,
                "VerifierResultId",
                envelope.m_releaseReview.m_verifierResultId);
            AppendString(
                output,
                "Decision",
                ToString(envelope.m_releaseReview.m_decision));
            AppendString(output, "Reviewer", envelope.m_releaseReview.m_reviewer);
            AppendStringArray(
                output,
                "EvidenceIds",
                envelope.m_releaseReview.m_evidenceIds);
            AppendString(
                output,
                "ReviewedAtUtc",
                envelope.m_releaseReview.m_reviewedAtUtc);
            AppendString(
                output,
                "Rationale",
                envelope.m_releaseReview.m_rationale);

            AZStd::vector<const AdapterVerifierFindingDisposition*> dispositions;
            for (const AdapterVerifierFindingDisposition& disposition :
                 envelope.m_releaseReview.m_dispositions)
            {
                dispositions.push_back(&disposition);
            }
            AZStd::sort(
                dispositions.begin(),
                dispositions.end(),
                [](const AdapterVerifierFindingDisposition* left,
                    const AdapterVerifierFindingDisposition* right)
                {
                    return left->m_findingId < right->m_findingId;
                });
            AppendName(output, "Dispositions");
            output.push_back('[');
            for (size_t index = 0; index < dispositions.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const AdapterVerifierFindingDisposition& disposition =
                    *dispositions[index];
                output.push_back('{');
                AppendString(output, "FindingId", disposition.m_findingId);
                AppendString(
                    output,
                    "Decision",
                    ToString(disposition.m_decision));
                AppendString(output, "Rationale", disposition.m_rationale);
                AppendStringArray(
                    output,
                    "EvidenceIds",
                    disposition.m_evidenceIds,
                    false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back('}');
            output.push_back(',');

            AppendName(output, "Findings");
            output.push_back('[');
            for (size_t index = 0; index < envelope.m_findings.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const AdapterVerifierReconciliationFinding& finding =
                    envelope.m_findings[index];
                output.push_back('{');
                AppendString(output, "FindingId", finding.m_findingId);
                AppendString(output, "Kind", ToString(finding.m_kind));
                AppendString(
                    output,
                    "Relationship",
                    ToString(finding.m_relationship));
                AppendString(output, "SubjectRef", finding.m_subjectRef);
                AppendString(
                    output,
                    "ReportBlockerId",
                    finding.m_reportBlockerId);
                AppendString(output, "CheckId", finding.m_checkId);
                AppendString(output, "StepId", finding.m_stepId);
                AppendString(output, "Message", finding.m_message);
                AppendStringArray(
                    output,
                    "EvidenceIds",
                    finding.m_evidenceIds);
                AppendStringArray(
                    output,
                    "DiagnosticReferenceIds",
                    finding.m_diagnosticReferenceIds);
                AppendBool(
                    output,
                    "ExistingBlockerPreserved",
                    finding.m_existingBlockerPreserved);
                AppendBool(
                    output,
                    "BlocksCompatibility",
                    finding.m_blocksCompatibility);
                AppendBool(
                    output,
                    "BlocksRelease",
                    finding.m_blocksRelease);
                AppendBool(
                    output,
                    "RequiresHumanDisposition",
                    finding.m_requiresHumanDisposition,
                    false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back(',');
            AppendBool(
                output,
                "HumanReviewRequired",
                envelope.m_humanReviewRequired);
            AppendBool(output, "VerifierExecuted", envelope.m_verifierExecuted);
            AppendBool(output, "TargetAccessed", envelope.m_targetAccessed);
            AppendBool(output, "FilesMutated", envelope.m_filesMutated);
            AppendBool(output, "EvidencePromoted", envelope.m_evidencePromoted);
            AppendBool(output, "ArchiveAssembled", envelope.m_archiveAssembled);
            AppendBool(output, "ArchiveSigned", envelope.m_archiveSigned);
            AppendBool(output, "ReleasePublished", envelope.m_releasePublished);
            AppendBool(output, "LaunchPerformed", envelope.m_launchPerformed);
            AppendBool(output, "AdapterCalled", envelope.m_adapterCalled, false);
            output.push_back('}');
            return output;
        }

        EvidenceRecord BuildEvidence(
            const AZStd::string& evidenceId,
            const SourceRecord& source,
            const AdapterVerifierEvidenceReconciliationEnvelope& envelope,
            const AZStd::string& subjectRef,
            const AZStd::string& claim,
            const AZStd::string& evidenceKind,
            const AZStd::string& recordPath)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = evidenceId;
            evidence.m_sourceId = source.m_sourceId;
            evidence.m_sourceFingerprint = source.m_fingerprint;
            evidence.m_profileId = envelope.m_profileId;
            evidence.m_gameVersion = envelope.m_gameVersion;
            evidence.m_branch = envelope.m_branch;
            evidence.m_subjectRef = subjectRef;
            evidence.m_claim = claim;
            evidence.m_evidenceKind = evidenceKind;
            evidence.m_confidence = "unrated";
            evidence.m_locator = source.m_locator;
            evidence.m_recordPath = recordPath;
            evidence.m_extractedAt = envelope.m_evaluatedAtUtc;
            return evidence;
        }

        void BuildCandidateEvidence(
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            AdapterVerifierEvidenceReconciliationResult& result)
        {
            result.m_sourceDocuments = verifierEvidence.m_sourceDocuments;
            result.m_evidenceDocuments = verifierEvidence.m_evidenceDocuments;
            if (result.m_sourceDocuments.empty())
            {
                return;
            }

            const SourceRecord& source = result.m_sourceDocuments.front().m_source;
            EvidenceDocument document;
            document.m_sourceId = source.m_sourceId;
            document.m_sourceFingerprint = source.m_fingerprint;
            document.m_profileId = result.m_envelope.m_profileId;
            document.m_gameVersion = result.m_envelope.m_gameVersion;
            document.m_branch = result.m_envelope.m_branch;

            const AZStd::string prefix =
                "evidence.verifier-reconciliation."
                + result.m_envelope.m_reconciliationId;
            document.m_evidence.push_back(BuildEvidence(
                prefix + ".binding",
                source,
                result.m_envelope,
                "post-deployment-report:" + result.m_envelope.m_reportId,
                "Reconciliation binds the exact post-deployment report, verifier "
                "evidence return, execution result, work order, context, candidate "
                "identities, and human release review.",
                "verifier_reconciliation_binding",
                "Reconciliation/Binding"));

            for (size_t index = 0;
                 index < result.m_envelope.m_findings.size();
                 ++index)
            {
                const AdapterVerifierReconciliationFinding& finding =
                    result.m_envelope.m_findings[index];
                document.m_evidence.push_back(BuildEvidence(
                    prefix + ".finding." + UnsignedString(index + 1),
                    source,
                    result.m_envelope,
                    finding.m_subjectRef,
                    "Reconciliation preserved finding " + finding.m_findingId
                        + " as " + ToString(finding.m_kind) + " with relationship "
                        + ToString(finding.m_relationship) + ".",
                    "verifier_reconciliation_finding",
                    "Reconciliation/Findings/" + UnsignedString(index + 1)));
            }

            document.m_evidence.push_back(BuildEvidence(
                prefix + ".human-review",
                source,
                result.m_envelope,
                "post-deployment-report:" + result.m_envelope.m_reportId,
                "Named human review "
                    + result.m_envelope.m_releaseReview.m_reviewId + " recorded "
                    + ToString(result.m_envelope.m_releaseReview.m_decision)
                    + " while preserving compatibility assessment "
                    + ToString(result.m_envelope.m_compatibilityAssessment)
                    + " and release decision "
                    + ToString(result.m_envelope.m_releaseDecision) + ".",
                "verifier_reconciliation_human_review",
                "Reconciliation/HumanReview"));

            result.m_evidenceDocuments.push_back(AZStd::move(document));
            result.m_sourceDocumentCount = static_cast<AZ::u64>(
                result.m_sourceDocuments.size());
            result.m_evidenceRecordCount = 0;
            for (const EvidenceDocument& evidenceDocument :
                 result.m_evidenceDocuments)
            {
                result.m_evidenceRecordCount += static_cast<AZ::u64>(
                    evidenceDocument.m_evidence.size());
            }
        }
    } // namespace

    AZStd::string
    AdapterVerifierEvidenceReconciliationService::SerializeCanonicalEnvelope(
        const AdapterVerifierEvidenceReconciliationEnvelope& envelope) const
    {
        return SerializeEnvelope(envelope);
    }

    AdapterVerifierEvidenceReconciliationResult
    AdapterVerifierEvidenceReconciliationService::BuildReconciliation(
        const AdapterDeploymentWorkOrder& workOrder,
        const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
        const AdapterPostDeploymentVerificationReport& report,
        const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope,
        const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
        const AdapterVerifierEvidenceReconciliationRequest& request) const
    {
        AdapterVerifierEvidenceReconciliationResult result;
        AdapterVerifierEvidenceReconciliationEnvelope& envelope =
            result.m_envelope;
        envelope.m_reconciliationId = request.m_reconciliationId;
        envelope.m_reportId = report.m_reportId;
        envelope.m_reportStatus = report.m_status;
        envelope.m_reportCanonicalJson = verifierEnvelope.m_reportCanonicalJson;
        envelope.m_verifierResultId = verifierEnvelope.m_verifierResultId;
        envelope.m_verifierEvidenceStatus = verifierEvidence.m_status;
        envelope.m_executionResultId = executionEnvelope.m_resultId;
        envelope.m_workOrderId = workOrder.m_workOrderId;
        envelope.m_workOrderFingerprint =
            executionEnvelope.m_workOrderFingerprint;
        envelope.m_executionResultFingerprint =
            executionEnvelope.m_resultFingerprint;
        envelope.m_verifierResultFingerprint =
            verifierEnvelope.m_resultFingerprint;
        envelope.m_profileId = executionEnvelope.m_profileId;
        envelope.m_gameVersion = executionEnvelope.m_gameVersion;
        envelope.m_branch = executionEnvelope.m_branch;
        envelope.m_runtimeTarget = executionEnvelope.m_runtimeTarget;
        envelope.m_packId = executionEnvelope.m_packId;
        envelope.m_previewId = executionEnvelope.m_previewId;
        envelope.m_previewFingerprint = executionEnvelope.m_previewFingerprint;
        envelope.m_targetInventoryId = executionEnvelope.m_targetInventoryId;
        envelope.m_evaluatedAtUtc = request.m_evaluatedAtUtc;
        envelope.m_releaseReview = request.m_releaseReview;

        GatherCandidateIds(report, verifierEvidence, envelope);
        BuildFindings(report, verifierEnvelope, envelope);
        ResolveCompatibilityAssessment(verifierEvidence, envelope);

        ReconciliationFlags flags;
        ValidateReportReadiness(
            workOrder,
            executionEnvelope,
            report,
            result,
            flags);
        ValidateVerifierEvidence(
            report,
            verifierEnvelope,
            verifierEvidence,
            result,
            flags);
        ValidateExactBinding(
            workOrder,
            executionEnvelope,
            report,
            verifierEnvelope,
            request,
            result,
            flags);
        ValidateReleaseReview(request, result, flags);
        ResolveReleaseDecision(report, verifierEvidence, result, flags);

        envelope.m_status = ResolveStatus(flags);
        result.m_accepted = envelope.m_status
            == AdapterVerifierReconciliationEnvelopeStatus::Accepted;
        SortIssues(result);
        envelope.m_canonicalJson = SerializeEnvelope(envelope);

        if (result.m_accepted)
        {
            BuildCandidateEvidence(verifierEvidence, result);
        }
        return result;
    }
} // namespace TaintedGrailModdingSDK
