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

        bool VerifierReviewEvidenceIsBound(
            const AdapterPostDeploymentVerifierReview& review,
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            const SourceEvidenceRegistry& sourceRegistry)
        {
            if (review.m_evidenceIds.empty())
            {
                return false;
            }
            AZStd::vector<AZStd::string> evidenceIds = review.m_evidenceIds;
            AZStd::sort(evidenceIds.begin(), evidenceIds.end());
            if (AZStd::adjacent_find(evidenceIds.begin(), evidenceIds.end())
                != evidenceIds.end())
            {
                return false;
            }

            const AZStd::string expectedSubject =
                "post-deployment-verifier:" + review.m_verifierId;
            for (const AZStd::string& evidenceId : evidenceIds)
            {
                if (!IsStableContractId(evidenceId))
                {
                    return false;
                }
                const EvidenceRecord* evidence =
                    sourceRegistry.FindEvidence(evidenceId);
                if (!evidence
                    || evidence->m_subjectRef != expectedSubject
                    || evidence->m_claim.empty()
                    || evidence->m_evidenceKind.empty()
                    || evidence->m_locator.empty()
                    || evidence->m_recordPath.empty()
                    || !IsStrictUtcTimestamp(evidence->m_extractedAt)
                    || evidence->m_extractedAt > review.m_reviewedAtUtc
                    || !IsSha256Fingerprint(evidence->m_sourceFingerprint))
                {
                    return false;
                }
                const SourceRecord* source =
                    sourceRegistry.FindSource(evidence->m_sourceId);
                if (!source
                    || !IsUsableImportStatus(source->m_importStatus)
                    || source->m_fingerprint != evidence->m_sourceFingerprint
                    || source->m_profileId != envelope.m_profileId
                    || source->m_gameVersion != envelope.m_gameVersion
                    || source->m_branch != envelope.m_branch
                    || source->m_runtimeTarget != envelope.m_runtimeTarget
                    || evidence->m_profileId != envelope.m_profileId
                    || evidence->m_gameVersion != envelope.m_gameVersion
                    || evidence->m_branch != envelope.m_branch)
                {
                    return false;
                }
            }
            return true;
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
                || !CanonicalSha256Matches(
                    workOrder.m_canonicalJson,
                    executionEnvelope.m_workOrderFingerprint)
                || !DeploymentExecutionResultFingerprintMatches(executionEnvelope)
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
                || !IsStableContractId(report.m_reportId)
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
                    "eligible post-deployment report with derived work-order and execution "
                    "fingerprints, accepted execution evidence, human review retained, and "
                    "every execution, mutation, verifier, promotion, launch, adapter, and "
                    "publication flag false. Existing compatibility or release blockers "
                    "remain input facts and are not cleared by this contract.");
            }
        }

        void ValidateVerifierReview(
            const AdapterDeploymentWorkOrder& workOrder,
            const SourceEvidenceRegistry& sourceRegistry,
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result,
            VerifierValidationFlags& flags)
        {
            const AdapterPostDeploymentVerifierReview& review =
                envelope.m_verifierReview;
            bool invalid = !IsStableContractId(review.m_reviewId)
                || !IsStableContractId(review.m_verifierId)
                || !IsStrictSemanticVersion(review.m_verifierVersion)
                || !IsSha256Fingerprint(review.m_verifierFingerprint)
                || review.m_decision
                    != AdapterPostDeploymentVerifierReviewDecision::Accepted
                || review.m_reviewer.empty()
                || !IsStrictUtcTimestamp(review.m_reviewedAtUtc)
                || (!envelope.m_capturedAtUtc.empty()
                    && review.m_reviewedAtUtc > envelope.m_capturedAtUtc)
                || !VerifierReviewEvidenceIsBound(
                    review,
                    envelope,
                    sourceRegistry);

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
                || capabilityNames.empty()
                || AZStd::adjacent_find(
                       capabilityNames.begin(),
                       capabilityNames.end())
                    != capabilityNames.end();

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
                    "The supplied verifier requires accepted named review, stable identity, "
                    "strict semantic version, lowercase SHA-256 fingerprint, real UTC "
                    "review time, unique typed capabilities, exact required capability "
                    "coverage, and registered usable-source evidence that proves the exact "
                    "verifier identity for the active profile.");
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
                    "status, canonical JSON, work order, execution result, derived "
                    "fingerprints, profile, game version, branch, and runtime target.");
            }
        }

        void ValidateVerifierEnvelopeShape(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result,
            VerifierValidationFlags& flags)
        {
            if (envelope.m_contractVersion != 1
                || !IsStableContractId(envelope.m_verifierResultId)
                || !IsStableContractId(envelope.m_reportId)
                || !IsStableContractId(envelope.m_resultId)
                || !IsStableContractId(envelope.m_workOrderId)
                || !IsSha256Fingerprint(envelope.m_workOrderFingerprint)
                || !IsSha256Fingerprint(envelope.m_executionResultFingerprint)
                || !IsSha256Fingerprint(envelope.m_resultFingerprint)
                || !PostDeploymentVerifierResultFingerprintMatches(envelope)
                || !IsStrictUtcTimestamp(envelope.m_capturedAtUtc)
                || envelope.m_reportCanonicalJson.empty()
                || !IsStableContractId(envelope.m_profileId)
                || envelope.m_gameVersion.empty()
                || envelope.m_branch.empty()
                || !IsSupportedRuntimeTarget(envelope.m_runtimeTarget))
            {
                AddVerifierIssue(
                    result,
                    flags.m_envelopeInvalid,
                    "post_deployment_verifier.envelope_invalid",
                    "The verifier envelope requires contract version 1, stable identity, "
                    "exact derived lowercase SHA-256 fingerprints, non-empty canonical "
                    "report JSON, real UTC capture time, and explicit profile, game, "
                    "branch, and Mono/IL2CPP runtime context.");
            }
        }
    } // namespace
} // namespace TaintedGrailModdingSDK
