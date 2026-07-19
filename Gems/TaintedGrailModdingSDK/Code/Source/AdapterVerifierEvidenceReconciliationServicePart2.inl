namespace TaintedGrailModdingSDK
{
    namespace
    {
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

        AdapterVerifierReconciliationFinding MakeVerifierCheckFinding(
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierCheckResult& check)
        {
            AdapterVerifierReconciliationFinding finding;
            finding.m_findingId =
                "verifier-reconciliation.finding.check." + check.m_checkId;
            finding.m_subjectRef =
                "deployment-work-order-step:" + check.m_stepId;
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

        void BuildReconciliationFindings(
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
                    MakeVerifierCheckFinding(report, check));
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

        void ResolveReconciliationCompatibility(
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

        void ValidateReconciliationReleaseReview(
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
                AddReconciliationIssue(
                    result,
                    flags.m_reviewMissing,
                    "verifier_reconciliation.review_missing",
                    "A named human release review with one explicit hold, reject, or "
                    "approve decision is required. No decision is inferred from verifier "
                    "matching or report status.");
                return;
            }

            bool invalid = !IsAdapterPostDeploymentVerifierStableId(review.m_reviewId)
                || !HasStableUniqueIds(review.m_evidenceIds)
                || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                    review.m_reviewedAtUtc)
                || review.m_reviewedAtUtc > request.m_evaluatedAtUtc
                || review.m_rationale.empty();

            AZStd::vector<AZStd::string> dispositionIds;
            AZStd::vector<AZStd::string> completedIds;
            for (const AdapterVerifierFindingDisposition& disposition :
                 review.m_dispositions)
            {
                dispositionIds.push_back(disposition.m_findingId);
                const AdapterVerifierReconciliationFinding* finding =
                    FindReconciliationFinding(
                        envelope,
                        disposition.m_findingId);
                const bool dispositionValid = finding
                    && finding->m_requiresHumanDisposition
                    && disposition.m_decision
                        != AdapterVerifierFindingDispositionDecision::Unknown
                    && !disposition.m_rationale.empty()
                    && HasStableUniqueIds(disposition.m_evidenceIds);
                invalid = invalid || !dispositionValid;
                if (dispositionValid)
                {
                    completedIds.push_back(disposition.m_findingId);
                }
                else
                {
                    AddReconciliationIssue(
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
                AddReconciliationIssue(
                    result,
                    flags.m_reviewInvalid,
                    "verifier_reconciliation.duplicate_disposition",
                    "A reconciliation finding may have at most one human disposition.");
            }

            SortUniqueReconciliationValues(completedIds);
            envelope.m_completedDispositionCount =
                static_cast<AZ::u64>(completedIds.size());
            for (const AdapterVerifierReconciliationFinding& finding :
                 envelope.m_findings)
            {
                if (finding.m_requiresHumanDisposition
                    && AZStd::find(
                           completedIds.begin(),
                           completedIds.end(),
                           finding.m_findingId)
                        == completedIds.end())
                {
                    AddReconciliationIssue(
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
                AddReconciliationIssue(
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

        void ResolveReconciliationReleaseDecision(
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            AdapterVerifierEvidenceReconciliationResult& result,
            ReconciliationFlags& flags)
        {
            AdapterVerifierEvidenceReconciliationEnvelope& envelope =
                result.m_envelope;
            switch (envelope.m_releaseReview.m_decision)
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

            if (envelope.m_releaseReview.m_decision
                    == AdapterVerifierReleaseReviewDecision::Approve
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
                AddReconciliationIssue(
                    result,
                    flags.m_decisionInconsistent,
                    "verifier_reconciliation.approval_inconsistent",
                    "Approval is invalid while any preserved report blocker, adverse or "
                    "incomplete verifier observation, compatibility uncertainty, release "
                    "blocker, or incomplete human disposition remains. Matching metadata "
                    "never grants approval automatically.");
            }
        }

        AdapterVerifierReconciliationEnvelopeStatus ResolveReconciliationStatus(
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

        void SortReconciliationIssues(
            AdapterVerifierEvidenceReconciliationResult& result)
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
    } // namespace
} // namespace TaintedGrailModdingSDK
