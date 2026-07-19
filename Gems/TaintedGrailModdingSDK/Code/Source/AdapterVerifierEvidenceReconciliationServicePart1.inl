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

        void AddReconciliationIssue(
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

        AZStd::string ReconciliationUnsignedString(AZ::u64 value)
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

        void SortUniqueReconciliationValues(
            AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        bool HasStableUniqueIds(
            const AZStd::vector<AZStd::string>& values,
            bool requireNonEmpty = true)
        {
            if (requireNonEmpty && values.empty())
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

        const AdapterVerifierReconciliationFinding* FindReconciliationFinding(
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

        void GatherReconciliationCandidateIds(
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
            SortUniqueReconciliationValues(envelope.m_inputCandidateSourceIds);
            SortUniqueReconciliationValues(envelope.m_inputCandidateEvidenceIds);
        }

        void ValidateReconciliationReportReadiness(
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
                AddReconciliationIssue(
                    result,
                    flags.m_reportNotReady,
                    "verifier_reconciliation.report_not_ready",
                    "Reconciliation requires one exact current structurally eligible "
                    "post-deployment report backed by a review-ready work order and "
                    "accepted execution evidence, with every execution, mutation, "
                    "promotion, launch, adapter, and publication flag false.");
            }
        }

        void ValidateReconciliationVerifierEvidence(
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope,
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            AdapterVerifierEvidenceReconciliationResult& result,
            ReconciliationFlags& flags)
        {
            const bool eligibleStatus = verifierEvidence.m_status
                    == AdapterPostDeploymentVerifierEnvelopeStatus::Accepted
                || verifierEvidence.m_status
                    == AdapterPostDeploymentVerifierEnvelopeStatus::ObservationMismatch;
            if (!verifierEvidence.m_contractValid
                || !eligibleStatus
                || verifierEvidence.m_verifierResultId
                    != verifierEnvelope.m_verifierResultId
                || verifierEvidence.m_reportId != report.m_reportId
                || verifierEvidence.m_resultId != report.m_resultId
                || verifierEvidence.m_workOrderId != report.m_workOrderId
                || verifierEvidence.m_resultFingerprint
                    != verifierEnvelope.m_resultFingerprint)
            {
                AddReconciliationIssue(
                    result,
                    flags.m_verifierEvidenceInvalid,
                    "verifier_reconciliation.verifier_evidence_invalid",
                    "Reconciliation requires one structurally valid independent-verifier "
                    "evidence return. Both accepted and observation_mismatch results are "
                    "eligible; malformed or rejected verifier evidence is not.");
            }
        }

        void ValidateReconciliationBinding(
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
                || review.m_reportId != report.m_reportId
                || review.m_reportCanonicalJson
                    != verifierEnvelope.m_reportCanonicalJson
                || review.m_verifierResultId
                    != verifierEnvelope.m_verifierResultId
                || review.m_workOrderFingerprint
                    != executionEnvelope.m_workOrderFingerprint
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
                AddReconciliationIssue(
                    result,
                    flags.m_bindingMismatch,
                    "verifier_reconciliation.binding_mismatch",
                    "The reconciliation request and release review must bind to the exact "
                    "current report JSON, verifier result, execution result, work order, "
                    "fingerprints, and profile/game/branch/runtime context.");
            }
        }
    } // namespace
} // namespace TaintedGrailModdingSDK
