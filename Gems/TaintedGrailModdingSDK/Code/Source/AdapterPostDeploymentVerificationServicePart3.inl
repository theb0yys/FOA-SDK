namespace TaintedGrailModdingSDK
{
    namespace
    {
        void AggregateRollbacks(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AdapterDeploymentExecutionEvidenceReturn& evidenceReturn,
            AdapterPostDeploymentVerificationReport& report)
        {
            AZStd::vector<const AdapterDeploymentWorkOrderStep*> mutationSteps;
            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                if (IsMutationStep(step.m_kind))
                {
                    mutationSteps.push_back(&step);
                }
            }
            AZStd::sort(
                mutationSteps.begin(),
                mutationSteps.end(),
                [](const AdapterDeploymentWorkOrderStep* left,
                    const AdapterDeploymentWorkOrderStep* right)
                {
                    if (left->m_sequence != right->m_sequence)
                    {
                        return left->m_sequence < right->m_sequence;
                    }
                    return left->m_stepId < right->m_stepId;
                });

            for (const AdapterDeploymentWorkOrderStep* step : mutationSteps)
            {
                const AdapterDeploymentExecutionStepResult* stepResult =
                    FindStepResult(envelope, step->m_stepId);
                const AdapterDeploymentTargetVerification* verification =
                    FindVerification(envelope, step->m_stepId);
                const AdapterDeploymentRollbackResult* rollback =
                    FindRollback(envelope, step->m_stepId);
                const bool rollbackObserved = rollback
                    && (rollback->m_attempted
                        || rollback->m_outcome
                            != AdapterDeploymentExecutionOutcome::NotAttempted);
                const bool rollbackRequired =
                    (stepResult
                        && stepResult->m_outcome
                            == AdapterDeploymentExecutionOutcome::Failed)
                    || (verification
                        && verification->m_status
                            == AdapterDeploymentVerificationStatus::Mismatched)
                    || rollbackObserved;
                if (!rollbackRequired)
                {
                    continue;
                }

                ++report.m_rollbackRequiredCount;
                if (!rollback)
                {
                    ++report.m_rollbackIncompleteCount;
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::RollbackIncomplete,
                        step->m_stepId,
                        "post_deployment.rollback_missing",
                        StepSubject(envelope, step->m_stepId),
                        "A failed or mismatched mutation step requires one complete rollback "
                        "result, but none is available.",
                        step->m_stepId);
                    continue;
                }

                const AZStd::string subject = RollbackSubject(
                    envelope,
                    rollback->m_rollbackResultId);
                const AZStd::vector<AZStd::string> evidenceIds =
                    EvidenceIdsForSubject(evidenceReturn, subject);
                const AZStd::vector<AZStd::string> logIds = CombinedLogIds(
                    envelope,
                    rollback->m_logReferenceIds,
                    rollback->m_failureIds);
                if (rollback->m_outcome
                    == AdapterDeploymentExecutionOutcome::Succeeded)
                {
                    ++report.m_rollbackSucceededCount;
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::DeploymentRolledBack,
                        rollback->m_rollbackResultId,
                        "post_deployment.deployment_rolled_back",
                        subject,
                        "Rollback succeeded, so the attempted deployed state is not the "
                        "current release candidate and release remains blocked.",
                        step->m_stepId,
                        rollback->m_rollbackResultId,
                        evidenceIds,
                        logIds);
                    continue;
                }

                ++report.m_rollbackIncompleteCount;
                AddBlocker(
                    report,
                    envelope,
                    AdapterPostDeploymentBlockerKind::RollbackIncomplete,
                    rollback->m_rollbackResultId,
                    "post_deployment.rollback_incomplete",
                    subject,
                    "Required rollback or restore did not succeed and blocks release review.",
                    step->m_stepId,
                    rollback->m_rollbackResultId,
                    evidenceIds,
                    logIds);
                if (rollback->m_outcome
                        == AdapterDeploymentExecutionOutcome::Failed
                    && logIds.empty())
                {
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::DiagnosticMissing,
                        "rollback." + rollback->m_rollbackResultId,
                        "post_deployment.failed_rollback_diagnostic_missing",
                        subject,
                        "A failed rollback result has no referenced diagnostic log.",
                        step->m_stepId,
                        rollback->m_rollbackResultId,
                        evidenceIds);
                }
            }
        }

        void AggregateFailures(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AdapterDeploymentExecutionEvidenceReturn& evidenceReturn,
            AdapterPostDeploymentVerificationReport& report)
        {
            AZStd::vector<const AdapterDeploymentExecutionFailure*> failures;
            for (const AdapterDeploymentExecutionFailure& failure :
                envelope.m_failures)
            {
                failures.push_back(&failure);
            }
            AZStd::sort(
                failures.begin(),
                failures.end(),
                [](const AdapterDeploymentExecutionFailure* left,
                    const AdapterDeploymentExecutionFailure* right)
                {
                    return left->m_failureId < right->m_failureId;
                });

            report.m_failureCount = static_cast<AZ::u64>(failures.size());
            for (const AdapterDeploymentExecutionFailure* failure : failures)
            {
                AZStd::string subject =
                    "deployment-result:" + envelope.m_resultId;
                if (!failure->m_rollbackResultId.empty())
                {
                    subject = RollbackSubject(
                        envelope,
                        failure->m_rollbackResultId);
                }
                else if (!failure->m_stepId.empty())
                {
                    subject = StepSubject(envelope, failure->m_stepId);
                }
                const AZStd::vector<AZStd::string> evidenceIds =
                    EvidenceIdsForSubject(evidenceReturn, subject);
                AZStd::vector<AZStd::string> logIds =
                    failure->m_logReferenceIds;
                SortUnique(logIds);
                AddBlocker(
                    report,
                    envelope,
                    AdapterPostDeploymentBlockerKind::FailureReported,
                    failure->m_failureId,
                    "post_deployment.failure_reported",
                    subject,
                    "Execution metadata contains failure " + failure->m_failureId
                        + " (" + ToString(failure->m_kind) + "/"
                        + failure->m_code + "): " + failure->m_message,
                    failure->m_stepId,
                    failure->m_rollbackResultId,
                    evidenceIds,
                    logIds);
                if (logIds.empty())
                {
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::DiagnosticMissing,
                        "failure." + failure->m_failureId,
                        "post_deployment.failure_diagnostic_missing",
                        subject,
                        "A reported execution failure has no referenced diagnostic log.",
                        failure->m_stepId,
                        failure->m_rollbackResultId,
                        evidenceIds);
                }
            }
        }

        void FinalizeReport(
            bool evidenceBindingValid,
            AdapterPostDeploymentVerificationReport& report)
        {
            AZStd::sort(
                report.m_blockers.begin(),
                report.m_blockers.end(),
                [](const AdapterPostDeploymentBlocker& left,
                    const AdapterPostDeploymentBlocker& right)
                {
                    const AZStd::string leftKind = ToString(left.m_kind);
                    const AZStd::string rightKind = ToString(right.m_kind);
                    if (leftKind != rightKind)
                    {
                        return leftKind < rightKind;
                    }
                    if (left.m_stepId != right.m_stepId)
                    {
                        return left.m_stepId < right.m_stepId;
                    }
                    if (left.m_rollbackResultId != right.m_rollbackResultId)
                    {
                        return left.m_rollbackResultId
                            < right.m_rollbackResultId;
                    }
                    return left.m_blockerId < right.m_blockerId;
                });

            report.m_compatibilityBlockerCount = 0;
            report.m_releaseBlockerCount = 0;
            for (const AdapterPostDeploymentBlocker& blocker :
                report.m_blockers)
            {
                report.m_compatibilityBlockerCount +=
                    blocker.m_blocksCompatibility ? 1 : 0;
                report.m_releaseBlockerCount +=
                    blocker.m_blocksRelease ? 1 : 0;
            }

            report.m_executionEvidenceAccepted = evidenceBindingValid;
            report.m_compatibilityClear = evidenceBindingValid
                && report.m_compatibilityBlockerCount == 0;
            report.m_releaseBlockerFree = evidenceBindingValid
                && report.m_releaseBlockerCount == 0;

            const bool evidenceRejected = !evidenceBindingValid
                && AZStd::any_of(
                    report.m_blockers.begin(),
                    report.m_blockers.end(),
                    [](const AdapterPostDeploymentBlocker& blocker)
                    {
                        return blocker.m_kind
                            == AdapterPostDeploymentBlockerKind::
                                ExecutionEvidenceRejected;
                    });
            const bool evidenceIncomplete = !evidenceBindingValid
                && !evidenceRejected;
            if (evidenceRejected)
            {
                report.m_status =
                    AdapterPostDeploymentReportStatus::EvidenceRejected;
            }
            else if (evidenceIncomplete)
            {
                report.m_status =
                    AdapterPostDeploymentReportStatus::EvidenceIncomplete;
            }
            else if (report.m_uncheckedVerificationCount != 0)
            {
                report.m_status =
                    AdapterPostDeploymentReportStatus::VerificationIncomplete;
            }
            else if (report.m_compatibilityBlockerCount != 0)
            {
                report.m_status =
                    AdapterPostDeploymentReportStatus::CompatibilityBlocked;
            }
            else if (report.m_rollbackIncompleteCount != 0)
            {
                report.m_status =
                    AdapterPostDeploymentReportStatus::RollbackIncomplete;
            }
            else if (report.m_releaseBlockerCount != 0)
            {
                report.m_status =
                    AdapterPostDeploymentReportStatus::ReleaseBlocked;
            }
            else
            {
                report.m_status = AdapterPostDeploymentReportStatus::ReviewReady;
            }
        }
    } // namespace

    AdapterPostDeploymentVerificationReport
    AdapterPostDeploymentVerificationService::BuildReport(
        const AdapterDeploymentWorkOrder& workOrder,
        const AdapterDeploymentExecutionResultEnvelope& envelope,
        const AdapterDeploymentExecutionEvidenceReturn& evidenceReturn) const
    {
        AdapterPostDeploymentVerificationReport report;
        report.m_reportId =
            "post-deployment-verification." + envelope.m_resultId;
        report.m_resultId = envelope.m_resultId;
        report.m_workOrderId = envelope.m_workOrderId;
        report.m_workOrderFingerprint = envelope.m_workOrderFingerprint;
        report.m_resultFingerprint = envelope.m_resultFingerprint;
        report.m_profileId = envelope.m_profileId;
        report.m_gameVersion = envelope.m_gameVersion;
        report.m_branch = envelope.m_branch;
        report.m_runtimeTarget = envelope.m_runtimeTarget;

        const bool evidenceBindingValid = ValidateEvidenceReturnBinding(
            workOrder,
            envelope,
            evidenceReturn,
            report);
        if (evidenceBindingValid)
        {
            AggregateSteps(envelope, evidenceReturn, report);
            AggregateBackups(envelope, evidenceReturn, report);
            AggregateVerifications(envelope, evidenceReturn, report);
            AggregateRollbacks(
                workOrder,
                envelope,
                evidenceReturn,
                report);
            AggregateFailures(envelope, evidenceReturn, report);
        }

        for (const AdapterDeploymentExecutionLogReference& log :
            envelope.m_logReferences)
        {
            report.m_diagnosticReferenceIds.push_back(log.m_logId);
        }
        SortUnique(report.m_diagnosticReferenceIds);
        report.m_diagnosticReferenceCount = static_cast<AZ::u64>(
            report.m_diagnosticReferenceIds.size());

        FinalizeReport(evidenceBindingValid, report);
        return report;
    }
} // namespace TaintedGrailModdingSDK
