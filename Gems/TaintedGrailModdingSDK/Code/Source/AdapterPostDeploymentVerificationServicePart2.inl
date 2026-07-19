namespace TaintedGrailModdingSDK
{
    namespace
    {
        void AggregateSteps(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AdapterDeploymentExecutionEvidenceReturn& evidenceReturn,
            AdapterPostDeploymentVerificationReport& report)
        {
            AZStd::vector<const AdapterDeploymentExecutionStepResult*> steps;
            for (const AdapterDeploymentExecutionStepResult& step :
                envelope.m_stepResults)
            {
                steps.push_back(&step);
            }
            AZStd::sort(
                steps.begin(),
                steps.end(),
                [](const AdapterDeploymentExecutionStepResult* left,
                    const AdapterDeploymentExecutionStepResult* right)
                {
                    if (left->m_sequence != right->m_sequence)
                    {
                        return left->m_sequence < right->m_sequence;
                    }
                    return left->m_stepId < right->m_stepId;
                });

            report.m_stepCount = static_cast<AZ::u64>(steps.size());
            for (const AdapterDeploymentExecutionStepResult* step : steps)
            {
                const AZStd::string subject = StepSubject(envelope, step->m_stepId);
                const AZStd::vector<AZStd::string> evidenceIds =
                    EvidenceIdsForSubject(evidenceReturn, subject);
                const AZStd::vector<AZStd::string> logIds = CombinedLogIds(
                    envelope,
                    step->m_logReferenceIds,
                    step->m_failureIds);

                switch (step->m_outcome)
                {
                case AdapterDeploymentExecutionOutcome::Succeeded:
                    ++report.m_completedStepCount;
                    break;
                case AdapterDeploymentExecutionOutcome::Failed:
                    ++report.m_failedStepCount;
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::StepFailed,
                        step->m_stepId,
                        "post_deployment.step_failed",
                        subject,
                        "A deployment work-order step reported failed and blocks release "
                        "review until the failure and final target state are resolved.",
                        step->m_stepId,
                        {},
                        evidenceIds,
                        logIds);
                    if (logIds.empty())
                    {
                        AddBlocker(
                            report,
                            envelope,
                            AdapterPostDeploymentBlockerKind::DiagnosticMissing,
                            "step." + step->m_stepId,
                            "post_deployment.failed_step_diagnostic_missing",
                            subject,
                            "A failed deployment step has no referenced diagnostic log.",
                            step->m_stepId,
                            {},
                            evidenceIds);
                    }
                    break;
                case AdapterDeploymentExecutionOutcome::NotAttempted:
                case AdapterDeploymentExecutionOutcome::Skipped:
                    ++report.m_incompleteStepCount;
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::StepNotCompleted,
                        step->m_stepId,
                        "post_deployment.step_not_completed",
                        subject,
                        "A canonical deployment work-order step was not completed. The "
                        "report cannot represent a release-blocker-free deployment.",
                        step->m_stepId,
                        {},
                        evidenceIds,
                        logIds);
                    break;
                }
            }
        }

        void AggregateBackups(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AdapterDeploymentExecutionEvidenceReturn& evidenceReturn,
            AdapterPostDeploymentVerificationReport& report)
        {
            AZStd::vector<const AdapterDeploymentBackupResult*> backups;
            for (const AdapterDeploymentBackupResult& backup :
                envelope.m_backupResults)
            {
                backups.push_back(&backup);
            }
            AZStd::sort(
                backups.begin(),
                backups.end(),
                [](const AdapterDeploymentBackupResult* left,
                    const AdapterDeploymentBackupResult* right)
                {
                    return left->m_stepId < right->m_stepId;
                });

            report.m_backupResultCount =
                static_cast<AZ::u64>(backups.size());
            for (const AdapterDeploymentBackupResult* backup : backups)
            {
                if (backup->m_outcome
                    == AdapterDeploymentExecutionOutcome::Succeeded)
                {
                    continue;
                }

                ++report.m_incompleteBackupCount;
                const AZStd::string subject =
                    StepSubject(envelope, backup->m_stepId);
                const AZStd::vector<AZStd::string> evidenceIds =
                    EvidenceIdsForSubject(evidenceReturn, subject);
                const AZStd::vector<AZStd::string> logIds = CombinedLogIds(
                    envelope,
                    backup->m_logReferenceIds,
                    backup->m_failureIds);
                AddBlocker(
                    report,
                    envelope,
                    AdapterPostDeploymentBlockerKind::BackupIncomplete,
                    backup->m_stepId,
                    "post_deployment.backup_incomplete",
                    subject,
                    "A required backup outcome is not succeeded and blocks release review.",
                    backup->m_stepId,
                    {},
                    evidenceIds,
                    logIds);
                if (backup->m_outcome
                        == AdapterDeploymentExecutionOutcome::Failed
                    && logIds.empty())
                {
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::DiagnosticMissing,
                        "backup." + backup->m_stepId,
                        "post_deployment.failed_backup_diagnostic_missing",
                        subject,
                        "A failed backup result has no referenced diagnostic log.",
                        backup->m_stepId,
                        {},
                        evidenceIds);
                }
            }
        }

        void AggregateVerifications(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AdapterDeploymentExecutionEvidenceReturn& evidenceReturn,
            AdapterPostDeploymentVerificationReport& report)
        {
            AZStd::vector<const AdapterDeploymentTargetVerification*> verifications;
            for (const AdapterDeploymentTargetVerification& verification :
                envelope.m_targetVerifications)
            {
                verifications.push_back(&verification);
            }
            AZStd::sort(
                verifications.begin(),
                verifications.end(),
                [](const AdapterDeploymentTargetVerification* left,
                    const AdapterDeploymentTargetVerification* right)
                {
                    return left->m_stepId < right->m_stepId;
                });

            for (const AdapterDeploymentTargetVerification* verification :
                verifications)
            {
                const AZStd::string subject =
                    StepSubject(envelope, verification->m_stepId);
                const AZStd::vector<AZStd::string> evidenceIds =
                    EvidenceIdsForSubject(evidenceReturn, subject);
                AZStd::vector<AZStd::string> logIds =
                    verification->m_logReferenceIds;
                SortUnique(logIds);

                switch (verification->m_status)
                {
                case AdapterDeploymentVerificationStatus::Matched:
                    ++report.m_matchedVerificationCount;
                    break;
                case AdapterDeploymentVerificationStatus::Mismatched:
                    ++report.m_mismatchedVerificationCount;
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::TargetMismatch,
                        verification->m_stepId,
                        "post_deployment.target_mismatch",
                        subject,
                        "The observed target presence or fingerprint does not match the "
                        "expected deployed state. Compatibility and release remain blocked.",
                        verification->m_stepId,
                        {},
                        evidenceIds,
                        logIds,
                        true);
                    if (logIds.empty())
                    {
                        AddBlocker(
                            report,
                            envelope,
                            AdapterPostDeploymentBlockerKind::DiagnosticMissing,
                            "verification." + verification->m_stepId,
                            "post_deployment.mismatch_diagnostic_missing",
                            subject,
                            "A mismatched target verification has no referenced diagnostic "
                            "log.",
                            verification->m_stepId,
                            {},
                            evidenceIds);
                    }
                    break;
                case AdapterDeploymentVerificationStatus::NotChecked:
                    ++report.m_uncheckedVerificationCount;
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::TargetNotChecked,
                        verification->m_stepId,
                        "post_deployment.target_not_checked",
                        subject,
                        "The deployed target was not checked. Compatibility and release "
                        "remain blocked without claiming independent verification.",
                        verification->m_stepId,
                        {},
                        evidenceIds,
                        logIds,
                        true);
                    break;
                }
            }
        }

    } // namespace
} // namespace TaintedGrailModdingSDK
