namespace TaintedGrailModdingSDK
{
    namespace
    {
        AdapterDeploymentRollbackAction ExpectedRollbackAction(
            AdapterDeploymentWorkOrderStepKind kind)
        {
            switch (kind)
            {
            case AdapterDeploymentWorkOrderStepKind::Add:
                return AdapterDeploymentRollbackAction::RemoveAdded;
            case AdapterDeploymentWorkOrderStepKind::Replace:
                return AdapterDeploymentRollbackAction::RestoreReplaced;
            case AdapterDeploymentWorkOrderStepKind::Remove:
                return AdapterDeploymentRollbackAction::RestoreRemoved;
            default:
                return AdapterDeploymentRollbackAction::RemoveAdded;
            }
        }

        void ValidateBackupBindings(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterDeploymentExecutionEvidenceReturn& result,
            ValidationFlags& flags)
        {
            size_t expectedBackups = 0;
            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Backup)
                {
                    ++expectedBackups;
                    const AdapterDeploymentBackupResult* backup =
                        FindBackupResult(envelope, step.m_stepId);
                    if (!backup)
                    {
                        AddIssue(
                            result,
                            flags.m_backupBindingMismatch,
                            "deployment_result.backup_missing",
                            "Every backup work-order step requires one explicit backup "
                            "outcome.",
                            step.m_stepId);
                        continue;
                    }
                    if (backup->m_targetPath != step.m_targetPath
                        || backup->m_backupPath != step.m_backupPath
                        || backup->m_sourceFingerprint
                            != step.m_previousFingerprint)
                    {
                        AddIssue(
                            result,
                            flags.m_backupBindingMismatch,
                            "deployment_result.backup_binding_mismatch",
                            "The backup result must preserve the exact target path, "
                            "backup path, and pre-change source fingerprint.",
                            step.m_stepId);
                    }
                    if (!OutcomeShapeIsValid(
                            backup->m_outcome,
                            backup->m_attempted,
                            backup->m_failureIds))
                    {
                        AddIssue(
                            result,
                            flags.m_backupBindingMismatch,
                            "deployment_result.backup_outcome_invalid",
                            "The backup attempted flag and failure references do not "
                            "match the typed outcome.",
                            step.m_stepId);
                    }
                    if (backup->m_outcome
                            == AdapterDeploymentExecutionOutcome::Succeeded
                        && backup->m_backupFingerprint
                            != step.m_previousFingerprint)
                    {
                        AddIssue(
                            result,
                            flags.m_backupBindingMismatch,
                            "deployment_result.backup_fingerprint_mismatch",
                            "A successful backup must preserve the exact pre-change "
                            "fingerprint.",
                            step.m_stepId);
                    }
                }
            }

            if (envelope.m_backupResults.size() != expectedBackups)
            {
                AddIssue(
                    result,
                    flags.m_backupBindingMismatch,
                    "deployment_result.backup_count_mismatch",
                    "The envelope must contain exactly one backup result for every "
                    "backup work-order step.");
            }

            AZStd::vector<AZStd::string> ids;
            for (const AdapterDeploymentBackupResult& backup :
                envelope.m_backupResults)
            {
                ids.push_back(backup.m_stepId);
                const AdapterDeploymentWorkOrderStep* step =
                    FindWorkOrderStep(workOrder, backup.m_stepId);
                if (!step
                    || step->m_kind
                        != AdapterDeploymentWorkOrderStepKind::Backup)
                {
                    AddIssue(
                        result,
                        flags.m_backupBindingMismatch,
                        "deployment_result.backup_step_unknown",
                        "A backup result references a step that is not a canonical "
                        "backup work-order step.",
                        backup.m_stepId);
                }
            }
            AZStd::sort(ids.begin(), ids.end());
            if (AZStd::adjacent_find(ids.begin(), ids.end()) != ids.end())
            {
                AddIssue(
                    result,
                    flags.m_backupBindingMismatch,
                    "deployment_result.backup_duplicate",
                    "Every backup work-order step may have exactly one backup result.");
            }
        }

        void ValidateVerificationBindings(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterDeploymentExecutionEvidenceReturn& result,
            ValidationFlags& flags)
        {
            size_t expectedVerifications = 0;
            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                if (!IsMutationStep(step.m_kind))
                {
                    continue;
                }
                ++expectedVerifications;
                const AdapterDeploymentTargetVerification* verification =
                    FindVerification(envelope, step.m_stepId);
                if (!verification)
                {
                    AddIssue(
                        result,
                        flags.m_verificationBindingMismatch,
                        "deployment_result.verification_missing",
                        "Every add, replace, and remove step requires one explicit "
                        "target verification result.",
                        step.m_stepId);
                    continue;
                }

                const bool expectedPresent =
                    step.m_kind != AdapterDeploymentWorkOrderStepKind::Remove;
                const AZStd::string expectedFingerprint = expectedPresent
                    ? step.m_desiredFingerprint
                    : AZStd::string{};
                if (verification->m_targetPath != step.m_targetPath
                    || verification->m_expectedPresent != expectedPresent
                    || verification->m_expectedFingerprint
                        != expectedFingerprint)
                {
                    AddIssue(
                        result,
                        flags.m_verificationBindingMismatch,
                        "deployment_result.verification_binding_mismatch",
                        "Target verification must preserve the exact mutation step, "
                        "path, expected presence, and expected fingerprint.",
                        step.m_stepId);
                }

                switch (verification->m_status)
                {
                case AdapterDeploymentVerificationStatus::NotChecked:
                    if (!verification->m_checkedAtUtc.empty()
                        || !verification->m_observedFingerprint.empty()
                        || verification->m_observedPresent)
                    {
                        AddIssue(
                            result,
                            flags.m_verificationBindingMismatch,
                            "deployment_result.verification_not_checked_invalid",
                            "A not_checked verification cannot report observed state "
                            "or a check time.",
                            step.m_stepId);
                    }
                    break;
                case AdapterDeploymentVerificationStatus::Matched:
                    if (!IsAdapterDeploymentExecutionUtcTimestamp(
                            verification->m_checkedAtUtc)
                        || verification->m_observedPresent != expectedPresent
                        || verification->m_observedFingerprint
                            != expectedFingerprint)
                    {
                        AddIssue(
                            result,
                            flags.m_verificationBindingMismatch,
                            "deployment_result.verification_match_invalid",
                            "A matched verification must report the exact expected "
                            "presence and fingerprint with a UTC check time.",
                            step.m_stepId);
                    }
                    break;
                case AdapterDeploymentVerificationStatus::Mismatched:
                {
                    const bool actuallyMatches =
                        verification->m_observedPresent == expectedPresent
                        && verification->m_observedFingerprint
                            == expectedFingerprint;
                    if (!IsAdapterDeploymentExecutionUtcTimestamp(
                            verification->m_checkedAtUtc)
                        || actuallyMatches)
                    {
                        AddIssue(
                            result,
                            flags.m_verificationBindingMismatch,
                            "deployment_result.verification_mismatch_invalid",
                            "A mismatched verification requires a UTC check time and "
                            "observed state that differs from the exact expectation.",
                            step.m_stepId);
                    }
                    break;
                }
                }
            }

            if (envelope.m_targetVerifications.size() != expectedVerifications)
            {
                AddIssue(
                    result,
                    flags.m_verificationBindingMismatch,
                    "deployment_result.verification_count_mismatch",
                    "The envelope must contain exactly one target verification for "
                    "every add, replace, and remove step.");
            }

            AZStd::vector<AZStd::string> ids;
            for (const AdapterDeploymentTargetVerification& verification :
                envelope.m_targetVerifications)
            {
                ids.push_back(verification.m_stepId);
                const AdapterDeploymentWorkOrderStep* step =
                    FindWorkOrderStep(workOrder, verification.m_stepId);
                if (!step || !IsMutationStep(step->m_kind))
                {
                    AddIssue(
                        result,
                        flags.m_verificationBindingMismatch,
                        "deployment_result.verification_step_unknown",
                        "A target verification references a step outside the exact "
                        "mutation work-order steps.",
                        verification.m_stepId);
                }
            }
            AZStd::sort(ids.begin(), ids.end());
            if (AZStd::adjacent_find(ids.begin(), ids.end()) != ids.end())
            {
                AddIssue(
                    result,
                    flags.m_verificationBindingMismatch,
                    "deployment_result.verification_duplicate",
                    "Every mutation step may have exactly one target verification.");
            }
        }

        void ValidateRollbackBindings(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterDeploymentExecutionEvidenceReturn& result,
            ValidationFlags& flags)
        {
            size_t expectedRollbacks = 0;
            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                if (!IsMutationStep(step.m_kind))
                {
                    continue;
                }
                ++expectedRollbacks;
                const AdapterDeploymentRollbackResult* rollback =
                    FindRollbackResult(envelope, step.m_stepId);
                if (!rollback)
                {
                    AddIssue(
                        result,
                        flags.m_rollbackBindingMismatch,
                        "deployment_result.rollback_missing",
                        "Every add, replace, and remove step requires one typed "
                        "rollback result, including not_attempted.",
                        step.m_stepId);
                    continue;
                }

                const AdapterDeploymentRollbackAction expectedAction =
                    ExpectedRollbackAction(step.m_kind);
                const AZStd::string expectedBackupPath =
                    step.m_kind == AdapterDeploymentWorkOrderStepKind::Add
                    ? AZStd::string{}
                    : step.m_backupPath;
                const AZStd::string expectedDeployedFingerprint =
                    step.m_kind == AdapterDeploymentWorkOrderStepKind::Remove
                    ? AZStd::string{}
                    : step.m_desiredFingerprint;
                const AZStd::string expectedRestoreFingerprint =
                    step.m_kind == AdapterDeploymentWorkOrderStepKind::Add
                    ? AZStd::string{}
                    : step.m_previousFingerprint;
                if (!IsAdapterDeploymentExecutionStableId(
                        rollback->m_rollbackResultId)
                    || rollback->m_action != expectedAction
                    || rollback->m_targetPath != step.m_targetPath
                    || rollback->m_backupPath != expectedBackupPath
                    || rollback->m_expectedDeployedFingerprint
                        != expectedDeployedFingerprint
                    || rollback->m_restoreFingerprint
                        != expectedRestoreFingerprint)
                {
                    AddIssue(
                        result,
                        flags.m_rollbackBindingMismatch,
                        "deployment_result.rollback_binding_mismatch",
                        "The rollback result must preserve the exact inverse action, "
                        "target, backup, deployed fingerprint, and restore fingerprint.",
                        step.m_stepId,
                        rollback->m_rollbackResultId);
                }
                if (!OutcomeShapeIsValid(
                        rollback->m_outcome,
                        rollback->m_attempted,
                        rollback->m_failureIds))
                {
                    AddIssue(
                        result,
                        flags.m_rollbackBindingMismatch,
                        "deployment_result.rollback_outcome_invalid",
                        "The rollback attempted flag and failure references do not "
                        "match the typed outcome.",
                        step.m_stepId,
                        rollback->m_rollbackResultId);
                }
                if (rollback->m_outcome
                    == AdapterDeploymentExecutionOutcome::Succeeded)
                {
                    const bool expectedPresent =
                        expectedAction
                            != AdapterDeploymentRollbackAction::RemoveAdded;
                    if (rollback->m_targetPresent != expectedPresent
                        || rollback->m_finalFingerprint
                            != expectedRestoreFingerprint)
                    {
                        AddIssue(
                            result,
                            flags.m_rollbackBindingMismatch,
                            "deployment_result.rollback_final_state_mismatch",
                            "A successful rollback must report the exact inverse final "
                            "presence and restored fingerprint.",
                            step.m_stepId,
                            rollback->m_rollbackResultId);
                    }
                }
            }

            if (envelope.m_rollbackResults.size() != expectedRollbacks)
            {
                AddIssue(
                    result,
                    flags.m_rollbackBindingMismatch,
                    "deployment_result.rollback_count_mismatch",
                    "The envelope must contain exactly one rollback result for every "
                    "add, replace, and remove step.");
            }

            AZStd::vector<AZStd::string> sourceIds;
            AZStd::vector<AZStd::string> rollbackIds;
            for (const AdapterDeploymentRollbackResult& rollback :
                envelope.m_rollbackResults)
            {
                sourceIds.push_back(rollback.m_sourceStepId);
                rollbackIds.push_back(rollback.m_rollbackResultId);
                const AdapterDeploymentWorkOrderStep* step =
                    FindWorkOrderStep(workOrder, rollback.m_sourceStepId);
                if (!step || !IsMutationStep(step->m_kind))
                {
                    AddIssue(
                        result,
                        flags.m_rollbackBindingMismatch,
                        "deployment_result.rollback_step_unknown",
                        "A rollback result references a step outside the exact mutation "
                        "work-order steps.",
                        rollback.m_sourceStepId,
                        rollback.m_rollbackResultId);
                }
            }
            AZStd::sort(sourceIds.begin(), sourceIds.end());
            AZStd::sort(rollbackIds.begin(), rollbackIds.end());
            if (AZStd::adjacent_find(sourceIds.begin(), sourceIds.end())
                    != sourceIds.end()
                || AZStd::adjacent_find(rollbackIds.begin(), rollbackIds.end())
                    != rollbackIds.end())
            {
                AddIssue(
                    result,
                    flags.m_rollbackBindingMismatch,
                    "deployment_result.rollback_duplicate",
                    "Rollback result and source-step identities must be unique.");
            }
        }

        void ValidateReferencedIds(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId,
            const AZStd::string& rollbackResultId,
            const AZStd::vector<AZStd::string>& failureIds,
            const AZStd::vector<AZStd::string>& logIds,
            AdapterDeploymentExecutionEvidenceReturn& result,
            ValidationFlags& flags)
        {
            for (const AZStd::string& failureId : failureIds)
            {
                const AdapterDeploymentExecutionFailure* failure =
                    FindFailure(envelope, failureId);
                if (!failure
                    || (!failure->m_stepId.empty()
                        && failure->m_stepId != stepId)
                    || (!failure->m_rollbackResultId.empty()
                        && failure->m_rollbackResultId != rollbackResultId))
                {
                    AddIssue(
                        result,
                        flags.m_failureLogBindingMismatch,
                        "deployment_result.failure_reference_mismatch",
                        "A referenced failure must resolve to the same exact step or "
                        "rollback identity.",
                        stepId,
                        rollbackResultId);
                }
            }
            for (const AZStd::string& logId : logIds)
            {
                const AdapterDeploymentExecutionLogReference* log =
                    FindLog(envelope, logId);
                const bool stepBound = stepId.empty()
                    || (log
                        && AZStd::find(
                            log->m_stepIds.begin(),
                            log->m_stepIds.end(),
                            stepId) != log->m_stepIds.end());
                const bool rollbackBound = rollbackResultId.empty()
                    || (log
                        && AZStd::find(
                            log->m_rollbackResultIds.begin(),
                            log->m_rollbackResultIds.end(),
                            rollbackResultId)
                            != log->m_rollbackResultIds.end());
                if (!log || !stepBound || !rollbackBound)
                {
                    AddIssue(
                        result,
                        flags.m_failureLogBindingMismatch,
                        "deployment_result.log_reference_mismatch",
                        "A referenced log must resolve back to the same exact step or "
                        "rollback identity.",
                        stepId,
                        rollbackResultId);
                }
            }
        }

        void ValidateFailureAndLogBindings(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterDeploymentExecutionEvidenceReturn& result,
            ValidationFlags& flags)
        {
            AZStd::vector<AZStd::string> failureIds;
            for (const AdapterDeploymentExecutionFailure& failure :
                envelope.m_failures)
            {
                failureIds.push_back(failure.m_failureId);
                const bool stepKnown = failure.m_stepId.empty()
                    || FindWorkOrderStep(workOrder, failure.m_stepId);
                bool rollbackKnown = failure.m_rollbackResultId.empty();
                if (!rollbackKnown)
                {
                    for (const AdapterDeploymentRollbackResult& rollback :
                        envelope.m_rollbackResults)
                    {
                        rollbackKnown = rollbackKnown
                            || rollback.m_rollbackResultId
                                == failure.m_rollbackResultId;
                    }
                }
                if (!IsAdapterDeploymentExecutionStableId(failure.m_failureId)
                    || failure.m_code.empty()
                    || failure.m_message.empty()
                    || !stepKnown
                    || !rollbackKnown
                    || (failure.m_stepId.empty()
                        && failure.m_rollbackResultId.empty()
                        && failure.m_kind
                            != AdapterDeploymentExecutionFailureKind::Contract
                        && failure.m_kind
                            != AdapterDeploymentExecutionFailureKind::Executor))
                {
                    AddIssue(
                        result,
                        flags.m_failureLogBindingMismatch,
                        "deployment_result.failure_invalid",
                        "Every failure requires stable identity, code, message, and a "
                        "known step/rollback binding unless it is contract- or "
                        "executor-wide.",
                        failure.m_stepId,
                        failure.m_rollbackResultId);
                }
                for (const AZStd::string& logId :
                    failure.m_logReferenceIds)
                {
                    if (!FindLog(envelope, logId))
                    {
                        AddIssue(
                            result,
                            flags.m_failureLogBindingMismatch,
                            "deployment_result.failure_log_unknown",
                            "A failure references an unknown execution log.",
                            failure.m_stepId,
                            failure.m_rollbackResultId);
                    }
                }
            }
            AZStd::sort(failureIds.begin(), failureIds.end());
            if (AZStd::adjacent_find(failureIds.begin(), failureIds.end())
                != failureIds.end())
            {
                AddIssue(
                    result,
                    flags.m_failureLogBindingMismatch,
                    "deployment_result.failure_duplicate",
                    "Failure identities must be unique.");
            }

            AZStd::vector<AZStd::string> logIds;
            for (const AdapterDeploymentExecutionLogReference& log :
                envelope.m_logReferences)
            {
                logIds.push_back(log.m_logId);
                if (!IsAdapterDeploymentExecutionStableId(log.m_logId)
                    || !IsAdapterDeploymentExecutionLogReference(log.m_reference)
                    || !IsAdapterDeploymentExecutionFingerprint(log.m_fingerprint))
                {
                    AddIssue(
                        result,
                        flags.m_failureLogBindingMismatch,
                        "deployment_result.log_invalid",
                        "Every log requires stable identity, a safe relative reference, "
                        "and an exact lowercase SHA-256 fingerprint.");
                }
                for (const AZStd::string& stepId : log.m_stepIds)
                {
                    if (!FindWorkOrderStep(workOrder, stepId))
                    {
                        AddIssue(
                            result,
                            flags.m_failureLogBindingMismatch,
                            "deployment_result.log_step_unknown",
                            "A log references a step outside the exact deployment work "
                            "order.",
                            stepId);
                    }
                }
                for (const AZStd::string& rollbackId :
                    log.m_rollbackResultIds)
                {
                    bool found = false;
                    for (const AdapterDeploymentRollbackResult& rollback :
                        envelope.m_rollbackResults)
                    {
                        found = found
                            || rollback.m_rollbackResultId == rollbackId;
                    }
                    if (!found)
                    {
                        AddIssue(
                            result,
                            flags.m_failureLogBindingMismatch,
                            "deployment_result.log_rollback_unknown",
                            "A log references an unknown rollback result.",
                            {},
                            rollbackId);
                    }
                }
            }
            AZStd::sort(logIds.begin(), logIds.end());
            if (AZStd::adjacent_find(logIds.begin(), logIds.end())
                != logIds.end())
            {
                AddIssue(
                    result,
                    flags.m_failureLogBindingMismatch,
                    "deployment_result.log_duplicate",
                    "Log reference identities must be unique.");
            }

            for (const AdapterDeploymentExecutionStepResult& step :
                envelope.m_stepResults)
            {
                ValidateReferencedIds(
                    envelope,
                    step.m_stepId,
                    {},
                    step.m_failureIds,
                    step.m_logReferenceIds,
                    result,
                    flags);
            }
            for (const AdapterDeploymentBackupResult& backup :
                envelope.m_backupResults)
            {
                ValidateReferencedIds(
                    envelope,
                    backup.m_stepId,
                    {},
                    backup.m_failureIds,
                    backup.m_logReferenceIds,
                    result,
                    flags);
            }
            for (const AdapterDeploymentRollbackResult& rollback :
                envelope.m_rollbackResults)
            {
                ValidateReferencedIds(
                    envelope,
                    rollback.m_sourceStepId,
                    rollback.m_rollbackResultId,
                    rollback.m_failureIds,
                    rollback.m_logReferenceIds,
                    result,
                    flags);
            }
            for (const AdapterDeploymentTargetVerification& verification :
                envelope.m_targetVerifications)
            {
                ValidateReferencedIds(
                    envelope,
                    verification.m_stepId,
                    {},
                    {},
                    verification.m_logReferenceIds,
                    result,
                    flags);
            }
        }

        AdapterDeploymentExecutionEnvelopeStatus ResolveStatus(
            const ValidationFlags& flags)
        {
            if (flags.m_workOrderNotReady)
            {
                return AdapterDeploymentExecutionEnvelopeStatus::WorkOrderNotReady;
            }
            if (flags.m_executorUnreviewed)
            {
                return AdapterDeploymentExecutionEnvelopeStatus::ExecutorUnreviewed;
            }
            if (flags.m_workOrderBindingMismatch)
            {
                return AdapterDeploymentExecutionEnvelopeStatus::
                    WorkOrderBindingMismatch;
            }
            if (flags.m_envelopeInvalid)
            {
                return AdapterDeploymentExecutionEnvelopeStatus::EnvelopeInvalid;
            }
            if (flags.m_stepBindingMismatch)
            {
                return AdapterDeploymentExecutionEnvelopeStatus::
                    StepBindingMismatch;
            }
            if (flags.m_backupBindingMismatch)
            {
                return AdapterDeploymentExecutionEnvelopeStatus::
                    BackupBindingMismatch;
            }
            if (flags.m_verificationBindingMismatch)
            {
                return AdapterDeploymentExecutionEnvelopeStatus::
                    VerificationBindingMismatch;
            }
            if (flags.m_rollbackBindingMismatch)
            {
                return AdapterDeploymentExecutionEnvelopeStatus::
                    RollbackBindingMismatch;
            }
            if (flags.m_failureLogBindingMismatch)
            {
                return AdapterDeploymentExecutionEnvelopeStatus::
                    FailureLogBindingMismatch;
            }
            return AdapterDeploymentExecutionEnvelopeStatus::Accepted;
        }

        void SortIssues(AdapterDeploymentExecutionEvidenceReturn& result)
        {
            AZStd::sort(
                result.m_issues.begin(),
                result.m_issues.end(),
                [](const AdapterDeploymentExecutionResultIssue& left,
                    const AdapterDeploymentExecutionResultIssue& right)
                {
                    if (left.m_code != right.m_code)
                    {
                        return left.m_code < right.m_code;
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
                    return left.m_message < right.m_message;
                });
        }
    } // namespace
} // namespace TaintedGrailModdingSDK
