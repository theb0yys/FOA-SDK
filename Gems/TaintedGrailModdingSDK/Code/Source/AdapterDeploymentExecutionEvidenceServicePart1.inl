#include "AdapterDeploymentExecutionResultCanonical.h"
#include "CanonicalFingerprint.h"
#include "ResearchContractValidation.h"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        struct ValidationFlags
        {
            bool m_workOrderNotReady = false;
            bool m_executorUnreviewed = false;
            bool m_workOrderBindingMismatch = false;
            bool m_envelopeInvalid = false;
            bool m_stepBindingMismatch = false;
            bool m_backupBindingMismatch = false;
            bool m_verificationBindingMismatch = false;
            bool m_rollbackBindingMismatch = false;
            bool m_failureLogBindingMismatch = false;
        };

        void AddIssue(
            AdapterDeploymentExecutionEvidenceReturn& result,
            bool& flag,
            AZStd::string code,
            AZStd::string message,
            AZStd::string stepId = {},
            AZStd::string rollbackResultId = {})
        {
            flag = true;
            AdapterDeploymentExecutionResultIssue issue;
            issue.m_code = AZStd::move(code);
            issue.m_message = AZStd::move(message);
            issue.m_stepId = AZStd::move(stepId);
            issue.m_rollbackResultId = AZStd::move(rollbackResultId);
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

        bool HasStableUniqueIds(const AZStd::vector<AZStd::string>& values)
        {
            if (values.empty())
            {
                return false;
            }
            AZStd::vector<AZStd::string> sorted = values;
            for (const AZStd::string& value : sorted)
            {
                if (!IsStableContractId(value))
                {
                    return false;
                }
            }
            AZStd::sort(sorted.begin(), sorted.end());
            return AZStd::adjacent_find(sorted.begin(), sorted.end())
                == sorted.end();
        }

        const AdapterDeploymentWorkOrderStep* FindWorkOrderStep(
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

        const AdapterDeploymentExecutionStepResult* FindStepResult(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            for (const AdapterDeploymentExecutionStepResult& step :
                envelope.m_stepResults)
            {
                if (step.m_stepId == stepId)
                {
                    return &step;
                }
            }
            return nullptr;
        }

        const AdapterDeploymentBackupResult* FindBackupResult(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            for (const AdapterDeploymentBackupResult& backup :
                envelope.m_backupResults)
            {
                if (backup.m_stepId == stepId)
                {
                    return &backup;
                }
            }
            return nullptr;
        }

        const AdapterDeploymentTargetVerification* FindVerification(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            for (const AdapterDeploymentTargetVerification& verification :
                envelope.m_targetVerifications)
            {
                if (verification.m_stepId == stepId)
                {
                    return &verification;
                }
            }
            return nullptr;
        }

        const AdapterDeploymentRollbackResult* FindRollbackResult(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& sourceStepId)
        {
            for (const AdapterDeploymentRollbackResult& rollback :
                envelope.m_rollbackResults)
            {
                if (rollback.m_sourceStepId == sourceStepId)
                {
                    return &rollback;
                }
            }
            return nullptr;
        }

        const AdapterDeploymentExecutionFailure* FindFailure(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& failureId)
        {
            for (const AdapterDeploymentExecutionFailure& failure :
                envelope.m_failures)
            {
                if (failure.m_failureId == failureId)
                {
                    return &failure;
                }
            }
            return nullptr;
        }

        const AdapterDeploymentExecutionLogReference* FindLog(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& logId)
        {
            for (const AdapterDeploymentExecutionLogReference& log :
                envelope.m_logReferences)
            {
                if (log.m_logId == logId)
                {
                    return &log;
                }
            }
            return nullptr;
        }

        bool IsMutationStep(AdapterDeploymentWorkOrderStepKind kind)
        {
            return kind == AdapterDeploymentWorkOrderStepKind::Add
                || kind == AdapterDeploymentWorkOrderStepKind::Replace
                || kind == AdapterDeploymentWorkOrderStepKind::Remove;
        }

        bool OutcomeShapeIsValid(
            AdapterDeploymentExecutionOutcome outcome,
            bool attempted,
            const AZStd::vector<AZStd::string>& failureIds)
        {
            switch (outcome)
            {
            case AdapterDeploymentExecutionOutcome::NotAttempted:
                return !attempted && failureIds.empty();
            case AdapterDeploymentExecutionOutcome::Succeeded:
                return attempted && failureIds.empty();
            case AdapterDeploymentExecutionOutcome::Failed:
                return attempted && !failureIds.empty();
            case AdapterDeploymentExecutionOutcome::Skipped:
                return !attempted && !failureIds.empty();
            }
            return false;
        }

        void ValidateWorkOrderReadiness(
            const AdapterDeploymentWorkOrder& workOrder,
            AdapterDeploymentExecutionEvidenceReturn& result,
            ValidationFlags& flags)
        {
            if (workOrder.m_status != AdapterDeploymentWorkOrderStatus::ReviewReady
                || workOrder.m_canonicalJson.empty()
                || !workOrder.m_blockers.empty()
                || workOrder.m_executionAllowed
                || workOrder.m_copyAllowed
                || workOrder.m_deleteAllowed
                || workOrder.m_backupAllowed
                || workOrder.m_restoreAllowed
                || workOrder.m_deploymentAllowed
                || workOrder.m_launchAllowed)
            {
                AddIssue(
                    result,
                    flags.m_workOrderNotReady,
                    "deployment_result.work_order_not_ready",
                    "Execution-result metadata requires one exact review_ready work "
                    "order with every execution and mutation flag false.");
            }
            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                if (step.m_executionAllowed)
                {
                    AddIssue(
                        result,
                        flags.m_workOrderNotReady,
                        "deployment_result.work_order_step_executable",
                        "Execution-result metadata cannot bind to a work-order step "
                        "that permits execution.",
                        step.m_stepId);
                }
            }
        }

        void ValidateExecutorReview(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterDeploymentExecutionEvidenceReturn& result,
            ValidationFlags& flags)
        {
            const AdapterDeploymentExecutorReview& review =
                envelope.m_executorReview;
            if (!IsStableContractId(review.m_reviewId)
                || !IsStableContractId(review.m_executorId)
                || !IsStrictSemanticVersion(review.m_executorVersion)
                || !IsSha256Fingerprint(review.m_executorFingerprint)
                || review.m_decision
                    != AdapterDeploymentExecutorReviewDecision::Accepted
                || review.m_reviewer.empty()
                || !HasStableUniqueIds(review.m_evidenceIds)
                || !IsStrictUtcTimestamp(review.m_reviewedAtUtc)
                || (!envelope.m_capturedAtUtc.empty()
                    && review.m_reviewedAtUtc > envelope.m_capturedAtUtc))
            {
                AddIssue(
                    result,
                    flags.m_executorUnreviewed,
                    "deployment_result.executor_unreviewed",
                    "The supplied executor metadata requires an accepted, named, "
                    "evidence-backed review with stable identity, strict semantic "
                    "version, fingerprint, unique evidence, and a real UTC review time.");
            }
        }

        void ValidateEnvelopeAndWorkOrderBinding(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterDeploymentExecutionEvidenceReturn& result,
            ValidationFlags& flags)
        {
            const bool workOrderFingerprintMatches =
                !workOrder.m_canonicalJson.empty()
                && CanonicalSha256Matches(
                    workOrder.m_canonicalJson,
                    envelope.m_workOrderFingerprint)
                && CanonicalSha256Matches(
                    envelope.m_workOrderCanonicalJson,
                    envelope.m_workOrderFingerprint);
            if (envelope.m_workOrderId != workOrder.m_workOrderId
                || envelope.m_workOrderCanonicalJson != workOrder.m_canonicalJson
                || !workOrderFingerprintMatches
                || envelope.m_previewId != workOrder.m_previewId
                || envelope.m_previewFingerprint != workOrder.m_previewFingerprint
                || envelope.m_packId != workOrder.m_packId
                || envelope.m_targetInventoryId != workOrder.m_targetInventoryId)
            {
                AddIssue(
                    result,
                    flags.m_workOrderBindingMismatch,
                    "deployment_result.work_order_binding_mismatch",
                    "The result envelope must bind to the exact work-order identity, "
                    "canonical JSON and derived SHA-256 fingerprint, preview, pack, and "
                    "target inventory.");
            }

            if (envelope.m_contractVersion != 1
                || !IsStableContractId(envelope.m_resultId)
                || !IsSha256Fingerprint(envelope.m_workOrderFingerprint)
                || !IsSha256Fingerprint(envelope.m_resultFingerprint)
                || !DeploymentExecutionResultFingerprintMatches(envelope)
                || !IsSha256Fingerprint(envelope.m_previewFingerprint)
                || !IsStrictUtcTimestamp(envelope.m_capturedAtUtc)
                || !IsStableContractId(envelope.m_profileId)
                || envelope.m_gameVersion.empty()
                || envelope.m_branch.empty()
                || !IsSupportedRuntimeTarget(envelope.m_runtimeTarget))
            {
                AddIssue(
                    result,
                    flags.m_envelopeInvalid,
                    "deployment_result.envelope_invalid",
                    "The result envelope requires contract version 1, stable identity, "
                    "canonical work-order and result SHA-256 fingerprints, a real UTC "
                    "capture time, and exact profile/game/branch/runtime context.");
            }
        }

        void ValidateStepBindings(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterDeploymentExecutionEvidenceReturn& result,
            ValidationFlags& flags)
        {
            if (envelope.m_stepResults.size() != workOrder.m_steps.size())
            {
                AddIssue(
                    result,
                    flags.m_stepBindingMismatch,
                    "deployment_result.step_count_mismatch",
                    "The envelope must contain exactly one result for every canonical "
                    "deployment work-order step.");
            }

            AZStd::vector<AZStd::string> resultStepIds;
            for (const AdapterDeploymentExecutionStepResult& runtimeStep :
                envelope.m_stepResults)
            {
                resultStepIds.push_back(runtimeStep.m_stepId);
                const AdapterDeploymentWorkOrderStep* step =
                    FindWorkOrderStep(workOrder, runtimeStep.m_stepId);
                if (!step)
                {
                    AddIssue(
                        result,
                        flags.m_stepBindingMismatch,
                        "deployment_result.step_unknown",
                        "The envelope contains a step identity outside the exact "
                        "deployment work order.",
                        runtimeStep.m_stepId);
                    continue;
                }
                if (runtimeStep.m_sequence != step->m_sequence
                    || runtimeStep.m_kind != step->m_kind
                    || runtimeStep.m_targetPath != step->m_targetPath
                    || runtimeStep.m_backupPath != step->m_backupPath
                    || runtimeStep.m_previousFingerprint
                        != step->m_previousFingerprint
                    || runtimeStep.m_desiredFingerprint
                        != step->m_desiredFingerprint)
                {
                    AddIssue(
                        result,
                        flags.m_stepBindingMismatch,
                        "deployment_result.step_binding_mismatch",
                        "The attempted step sequence, kind, paths, or fingerprints do "
                        "not match the exact deployment work-order step.",
                        step->m_stepId);
                }
                if (!OutcomeShapeIsValid(
                        runtimeStep.m_outcome,
                        runtimeStep.m_attempted,
                        runtimeStep.m_failureIds))
                {
                    AddIssue(
                        result,
                        flags.m_stepBindingMismatch,
                        "deployment_result.step_outcome_invalid",
                        "The attempted flag and failure references do not match the "
                        "typed step outcome.",
                        step->m_stepId);
                }
                if (runtimeStep.m_outcome
                    == AdapterDeploymentExecutionOutcome::Succeeded)
                {
                    if ((step->m_kind == AdapterDeploymentWorkOrderStepKind::Add
                            || step->m_kind
                                == AdapterDeploymentWorkOrderStepKind::Replace)
                        && (!runtimeStep.m_targetPresent
                            || runtimeStep.m_observedFingerprint
                                != step->m_desiredFingerprint))
                    {
                        AddIssue(
                            result,
                            flags.m_stepBindingMismatch,
                            "deployment_result.step_success_fingerprint_mismatch",
                            "A successful add or replace step must report the exact "
                            "desired fingerprint as present.",
                            step->m_stepId);
                    }
                    if (step->m_kind
                            == AdapterDeploymentWorkOrderStepKind::Remove
                        && (runtimeStep.m_targetPresent
                            || !runtimeStep.m_observedFingerprint.empty()))
                    {
                        AddIssue(
                            result,
                            flags.m_stepBindingMismatch,
                            "deployment_result.step_success_presence_mismatch",
                            "A successful remove step must report the exact target as "
                            "absent with no observed fingerprint.",
                            step->m_stepId);
                    }
                }
            }

            AZStd::sort(resultStepIds.begin(), resultStepIds.end());
            if (AZStd::adjacent_find(
                    resultStepIds.begin(),
                    resultStepIds.end()) != resultStepIds.end())
            {
                AddIssue(
                    result,
                    flags.m_stepBindingMismatch,
                    "deployment_result.step_duplicate",
                    "Every deployment work-order step may appear exactly once.");
            }

            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                if (!FindStepResult(envelope, step.m_stepId))
                {
                    AddIssue(
                        result,
                        flags.m_stepBindingMismatch,
                        "deployment_result.step_missing",
                        "The envelope is missing a canonical deployment work-order "
                        "step result.",
                        step.m_stepId);
                }
            }
        }
    } // namespace
} // namespace TaintedGrailModdingSDK
