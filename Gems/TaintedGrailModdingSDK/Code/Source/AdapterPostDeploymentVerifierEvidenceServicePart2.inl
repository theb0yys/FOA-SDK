namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool IsVerifierMutationStep(AdapterDeploymentWorkOrderStepKind kind)
        {
            return kind == AdapterDeploymentWorkOrderStepKind::Add
                || kind == AdapterDeploymentWorkOrderStepKind::Replace
                || kind == AdapterDeploymentWorkOrderStepKind::Remove;
        }

        const AdapterDeploymentWorkOrderStep* FindVerifierWorkOrderStep(
            const AdapterDeploymentWorkOrder& workOrder,
            const AZStd::string& stepId)
        {
            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                if (step.m_stepId == stepId)
                {
                    return &step;
                }
            }
            return nullptr;
        }

        const AdapterPostDeploymentVerifierCheckResult* FindVerifierCheck(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            const AZStd::string& checkId)
        {
            for (const AdapterPostDeploymentVerifierCheckResult& check :
                envelope.m_checkResults)
            {
                if (check.m_checkId == checkId)
                {
                    return &check;
                }
            }
            return nullptr;
        }

        const AdapterPostDeploymentVerifierFailure* FindVerifierFailure(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            const AZStd::string& failureId)
        {
            for (const AdapterPostDeploymentVerifierFailure& failure :
                envelope.m_failures)
            {
                if (failure.m_failureId == failureId)
                {
                    return &failure;
                }
            }
            return nullptr;
        }

        const AdapterPostDeploymentVerifierDiagnosticReference*
        FindVerifierDiagnostic(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            const AZStd::string& diagnosticId)
        {
            for (const AdapterPostDeploymentVerifierDiagnosticReference& diagnostic :
                envelope.m_diagnosticReferences)
            {
                if (diagnostic.m_diagnosticId == diagnosticId)
                {
                    return &diagnostic;
                }
            }
            return nullptr;
        }

        bool HasVerifierCapability(
            const AdapterPostDeploymentVerifierReview& review,
            AdapterPostDeploymentVerifierCapability capability)
        {
            return AZStd::find(
                       review.m_capabilities.begin(),
                       review.m_capabilities.end(),
                       capability)
                != review.m_capabilities.end();
        }

        void ValidateVerifierReportReadiness(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AdapterPostDeploymentVerificationReport& report,
            AdapterPostDeploymentVerifierEvidenceReturn& result,
            VerifierValidationFlags& flags)
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
                AddVerifierIssue(
                    result,
                    flags.m_reportNotReady,
                    "post_deployment_verifier.report_not_ready",
                    "Independent verification requires one exact current structurally "
                    "eligible post-deployment report with accepted execution evidence, "
                    "human review retained, and every execution, mutation, verifier, "
                    "promotion, launch, adapter, and publication flag false. Existing "
                    "compatibility or release blockers remain input facts and are not "
                    "cleared by this contract.");
            }
        }

        void ValidateVerifierReview(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result,
            VerifierValidationFlags& flags)
        {
            const AdapterPostDeploymentVerifierReview& review =
                envelope.m_verifierReview;
            AdapterSemanticVersion version;
            bool invalid = !IsAdapterPostDeploymentVerifierStableId(review.m_reviewId)
                || !IsAdapterPostDeploymentVerifierStableId(review.m_verifierId)
                || !TryParseAdapterSemanticVersion(review.m_verifierVersion, version)
                || !IsAdapterPostDeploymentVerifierFingerprint(
                    review.m_verifierFingerprint)
                || review.m_decision
                    != AdapterPostDeploymentVerifierReviewDecision::Accepted
                || review.m_reviewer.empty()
                || review.m_evidenceIds.empty()
                || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                    review.m_reviewedAtUtc)
                || (!envelope.m_capturedAtUtc.empty()
                    && review.m_reviewedAtUtc > envelope.m_capturedAtUtc);

            AZStd::vector<AZStd::string> capabilityNames;
            for (AdapterPostDeploymentVerifierCapability capability :
                review.m_capabilities)
            {
                const AZStd::string capabilityName = ToString(capability);
                invalid = invalid || capabilityName == "unknown";
                capabilityNames.push_back(capabilityName);
            }
            AZStd::sort(capabilityNames.begin(), capabilityNames.end());
            invalid = invalid
                || AZStd::adjacent_find(
                       capabilityNames.begin(),
                       capabilityNames.end())
                    != capabilityNames.end();

            AZStd::vector<AZStd::string> reviewEvidenceIds = review.m_evidenceIds;
            for (const AZStd::string& evidenceId : reviewEvidenceIds)
            {
                invalid = invalid
                    || !IsAdapterPostDeploymentVerifierStableId(evidenceId);
            }
            AZStd::sort(reviewEvidenceIds.begin(), reviewEvidenceIds.end());
            invalid = invalid
                || AZStd::adjacent_find(
                       reviewEvidenceIds.begin(),
                       reviewEvidenceIds.end())
                    != reviewEvidenceIds.end();

            bool needsPresence = false;
            bool needsFingerprint = false;
            bool needsAbsence = false;
            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Add
                    || step.m_kind == AdapterDeploymentWorkOrderStepKind::Replace)
                {
                    needsPresence = true;
                    needsFingerprint = true;
                }
                else if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Remove)
                {
                    needsAbsence = true;
                }
            }

            invalid = invalid
                || (needsPresence
                    && !HasVerifierCapability(
                        review,
                        AdapterPostDeploymentVerifierCapability::TargetPresence))
                || (needsFingerprint
                    && !HasVerifierCapability(
                        review,
                        AdapterPostDeploymentVerifierCapability::TargetFingerprint))
                || (needsAbsence
                    && !HasVerifierCapability(
                        review,
                        AdapterPostDeploymentVerifierCapability::TargetAbsence));

            if (invalid)
            {
                AddVerifierIssue(
                    result,
                    flags.m_verifierUnreviewed,
                    "post_deployment_verifier.verifier_unreviewed",
                    "The supplied verifier requires accepted named evidence-backed review, "
                    "stable identity, strict semantic version, lowercase SHA-256 "
                    "fingerprint, UTC review time, unique typed capabilities, and exact "
                    "presence, fingerprint, and absence coverage required by the work order.");
            }
        }

        void ValidateVerifierReportBinding(
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result,
            VerifierValidationFlags& flags)
        {
            if (envelope.m_reportId != report.m_reportId
                || envelope.m_reportStatus != report.m_status
                || envelope.m_reportCanonicalJson
                    != SerializeVerifierInputReport(report)
                || envelope.m_resultId != report.m_resultId
                || envelope.m_workOrderId != report.m_workOrderId
                || envelope.m_workOrderFingerprint
                    != report.m_workOrderFingerprint
                || envelope.m_executionResultFingerprint
                    != report.m_resultFingerprint
                || envelope.m_profileId != report.m_profileId
                || envelope.m_gameVersion != report.m_gameVersion
                || envelope.m_branch != report.m_branch
                || envelope.m_runtimeTarget != report.m_runtimeTarget)
            {
                AddVerifierIssue(
                    result,
                    flags.m_reportBindingMismatch,
                    "post_deployment_verifier.report_binding_mismatch",
                    "The verifier envelope must bind to the exact current report identity, "
                    "status, canonical JSON, work order, execution result, fingerprints, "
                    "profile, game version, branch, and runtime target.");
            }
        }

        void ValidateVerifierEnvelopeShape(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result,
            VerifierValidationFlags& flags)
        {
            if (envelope.m_contractVersion != 1
                || !IsAdapterPostDeploymentVerifierStableId(
                    envelope.m_verifierResultId)
                || !IsAdapterPostDeploymentVerifierStableId(envelope.m_reportId)
                || !IsAdapterPostDeploymentVerifierStableId(envelope.m_resultId)
                || !IsAdapterPostDeploymentVerifierStableId(envelope.m_workOrderId)
                || !IsAdapterPostDeploymentVerifierFingerprint(
                    envelope.m_workOrderFingerprint)
                || !IsAdapterPostDeploymentVerifierFingerprint(
                    envelope.m_executionResultFingerprint)
                || !IsAdapterPostDeploymentVerifierFingerprint(
                    envelope.m_resultFingerprint)
                || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                    envelope.m_capturedAtUtc)
                || envelope.m_reportCanonicalJson.empty()
                || envelope.m_profileId.empty()
                || envelope.m_gameVersion.empty()
                || envelope.m_branch.empty()
                || (envelope.m_runtimeTarget != "Mono"
                    && envelope.m_runtimeTarget != "IL2CPP"))
            {
                AddVerifierIssue(
                    result,
                    flags.m_envelopeInvalid,
                    "post_deployment_verifier.envelope_invalid",
                    "The verifier envelope requires contract version 1, stable identity, "
                    "exact lowercase SHA-256 fingerprints, non-empty canonical report JSON, "
                    "UTC capture time, and explicit profile, game, branch, and Mono/IL2CPP "
                    "runtime context.");
            }
        }
    } // namespace
} // namespace TaintedGrailModdingSDK
