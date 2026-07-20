/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentExecutionResultCanonical.h"

#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        namespace Json = DeterministicContractJson;

        void AppendExecutorReview(
            AZStd::string& output,
            const AdapterDeploymentExecutorReview& review)
        {
            Json::AppendName(output, "ExecutorReview");
            output.push_back('{');
            Json::AppendString(output, "ReviewId", review.m_reviewId);
            Json::AppendString(output, "ExecutorId", review.m_executorId);
            Json::AppendString(output, "ExecutorVersion", review.m_executorVersion);
            Json::AppendString(
                output,
                "ExecutorFingerprint",
                review.m_executorFingerprint);
            Json::AppendString(output, "Decision", ToString(review.m_decision));
            Json::AppendString(output, "Reviewer", review.m_reviewer);
            Json::AppendSortedStringArray(
                output,
                "EvidenceIds",
                review.m_evidenceIds);
            Json::AppendString(output, "ReviewedAtUtc", review.m_reviewedAtUtc);
            Json::AppendString(output, "Notes", review.m_notes, false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendStep(
            AZStd::string& output,
            const AdapterDeploymentExecutionStepResult& step)
        {
            output.push_back('{');
            Json::AppendString(output, "StepId", step.m_stepId);
            Json::AppendUnsigned(output, "Sequence", step.m_sequence);
            Json::AppendString(output, "Kind", ToString(step.m_kind));
            Json::AppendString(output, "TargetPath", step.m_targetPath);
            Json::AppendString(output, "BackupPath", step.m_backupPath);
            Json::AppendString(
                output,
                "PreviousFingerprint",
                step.m_previousFingerprint);
            Json::AppendString(
                output,
                "DesiredFingerprint",
                step.m_desiredFingerprint);
            Json::AppendString(output, "Outcome", ToString(step.m_outcome));
            Json::AppendString(
                output,
                "ObservedFingerprint",
                step.m_observedFingerprint);
            Json::AppendBool(output, "TargetPresent", step.m_targetPresent);
            Json::AppendBool(output, "Attempted", step.m_attempted);
            Json::AppendSortedStringArray(
                output,
                "FailureIds",
                step.m_failureIds);
            Json::AppendSortedStringArray(
                output,
                "LogReferenceIds",
                step.m_logReferenceIds,
                false);
            output.push_back('}');
        }

        void AppendBackup(
            AZStd::string& output,
            const AdapterDeploymentBackupResult& backup)
        {
            output.push_back('{');
            Json::AppendString(output, "StepId", backup.m_stepId);
            Json::AppendString(output, "TargetPath", backup.m_targetPath);
            Json::AppendString(output, "BackupPath", backup.m_backupPath);
            Json::AppendString(
                output,
                "SourceFingerprint",
                backup.m_sourceFingerprint);
            Json::AppendString(
                output,
                "BackupFingerprint",
                backup.m_backupFingerprint);
            Json::AppendString(output, "Outcome", ToString(backup.m_outcome));
            Json::AppendBool(output, "Attempted", backup.m_attempted);
            Json::AppendSortedStringArray(
                output,
                "FailureIds",
                backup.m_failureIds);
            Json::AppendSortedStringArray(
                output,
                "LogReferenceIds",
                backup.m_logReferenceIds,
                false);
            output.push_back('}');
        }

        void AppendVerification(
            AZStd::string& output,
            const AdapterDeploymentTargetVerification& verification)
        {
            output.push_back('{');
            Json::AppendString(output, "StepId", verification.m_stepId);
            Json::AppendString(output, "TargetPath", verification.m_targetPath);
            Json::AppendString(
                output,
                "ExpectedFingerprint",
                verification.m_expectedFingerprint);
            Json::AppendString(
                output,
                "ObservedFingerprint",
                verification.m_observedFingerprint);
            Json::AppendBool(
                output,
                "ExpectedPresent",
                verification.m_expectedPresent);
            Json::AppendBool(
                output,
                "ObservedPresent",
                verification.m_observedPresent);
            Json::AppendString(output, "Status", ToString(verification.m_status));
            Json::AppendString(
                output,
                "CheckedAtUtc",
                verification.m_checkedAtUtc);
            Json::AppendSortedStringArray(
                output,
                "LogReferenceIds",
                verification.m_logReferenceIds,
                false);
            output.push_back('}');
        }

        void AppendRollback(
            AZStd::string& output,
            const AdapterDeploymentRollbackResult& rollback)
        {
            output.push_back('{');
            Json::AppendString(
                output,
                "RollbackResultId",
                rollback.m_rollbackResultId);
            Json::AppendString(output, "SourceStepId", rollback.m_sourceStepId);
            Json::AppendString(output, "Action", ToString(rollback.m_action));
            Json::AppendString(output, "TargetPath", rollback.m_targetPath);
            Json::AppendString(output, "BackupPath", rollback.m_backupPath);
            Json::AppendString(
                output,
                "ExpectedDeployedFingerprint",
                rollback.m_expectedDeployedFingerprint);
            Json::AppendString(
                output,
                "RestoreFingerprint",
                rollback.m_restoreFingerprint);
            Json::AppendString(output, "Outcome", ToString(rollback.m_outcome));
            Json::AppendString(
                output,
                "FinalFingerprint",
                rollback.m_finalFingerprint);
            Json::AppendBool(output, "TargetPresent", rollback.m_targetPresent);
            Json::AppendBool(output, "Attempted", rollback.m_attempted);
            Json::AppendSortedStringArray(
                output,
                "FailureIds",
                rollback.m_failureIds);
            Json::AppendSortedStringArray(
                output,
                "LogReferenceIds",
                rollback.m_logReferenceIds,
                false);
            output.push_back('}');
        }

        void AppendFailure(
            AZStd::string& output,
            const AdapterDeploymentExecutionFailure& failure)
        {
            output.push_back('{');
            Json::AppendString(output, "FailureId", failure.m_failureId);
            Json::AppendString(output, "Kind", ToString(failure.m_kind));
            Json::AppendString(output, "Code", failure.m_code);
            Json::AppendString(output, "Message", failure.m_message);
            Json::AppendString(output, "StepId", failure.m_stepId);
            Json::AppendString(
                output,
                "RollbackResultId",
                failure.m_rollbackResultId);
            Json::AppendSortedStringArray(
                output,
                "LogReferenceIds",
                failure.m_logReferenceIds);
            Json::AppendBool(output, "Retryable", failure.m_retryable, false);
            output.push_back('}');
        }

        void AppendLog(
            AZStd::string& output,
            const AdapterDeploymentExecutionLogReference& log)
        {
            output.push_back('{');
            Json::AppendString(output, "LogId", log.m_logId);
            Json::AppendString(output, "Kind", ToString(log.m_kind));
            Json::AppendString(output, "Reference", log.m_reference);
            Json::AppendString(output, "Fingerprint", log.m_fingerprint);
            Json::AppendSortedStringArray(output, "StepIds", log.m_stepIds);
            Json::AppendSortedStringArray(
                output,
                "RollbackResultIds",
                log.m_rollbackResultIds,
                false);
            output.push_back('}');
        }
    } // namespace

    AZStd::string SerializeCanonicalDeploymentExecutionResult(
        const AdapterDeploymentExecutionResultEnvelope& envelope)
    {
        AZStd::string output;
        output.push_back('{');
        Json::AppendUnsigned(output, "ContractVersion", envelope.m_contractVersion);
        Json::AppendString(output, "ResultId", envelope.m_resultId);
        Json::AppendString(output, "WorkOrderId", envelope.m_workOrderId);
        Json::AppendString(
            output,
            "WorkOrderCanonicalJson",
            envelope.m_workOrderCanonicalJson);
        Json::AppendString(
            output,
            "WorkOrderFingerprint",
            envelope.m_workOrderFingerprint);
        Json::AppendString(output, "PreviewId", envelope.m_previewId);
        Json::AppendString(
            output,
            "PreviewFingerprint",
            envelope.m_previewFingerprint);
        Json::AppendString(output, "PackId", envelope.m_packId);
        Json::AppendString(
            output,
            "TargetInventoryId",
            envelope.m_targetInventoryId);
        Json::AppendString(output, "ProfileId", envelope.m_profileId);
        Json::AppendString(output, "GameVersion", envelope.m_gameVersion);
        Json::AppendString(output, "Branch", envelope.m_branch);
        Json::AppendString(output, "RuntimeTarget", envelope.m_runtimeTarget);
        Json::AppendString(output, "CapturedAtUtc", envelope.m_capturedAtUtc);
        AppendExecutorReview(output, envelope.m_executorReview);

        AZStd::vector<const AdapterDeploymentExecutionStepResult*> steps;
        for (const AdapterDeploymentExecutionStepResult& step : envelope.m_stepResults)
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
        Json::AppendName(output, "StepResults");
        output.push_back('[');
        for (size_t index = 0; index < steps.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendStep(output, *steps[index]);
        }
        output.push_back(']');
        output.push_back(',');

        AZStd::vector<const AdapterDeploymentBackupResult*> backups;
        for (const AdapterDeploymentBackupResult& backup : envelope.m_backupResults)
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
        Json::AppendName(output, "BackupResults");
        output.push_back('[');
        for (size_t index = 0; index < backups.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendBackup(output, *backups[index]);
        }
        output.push_back(']');
        output.push_back(',');

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
        Json::AppendName(output, "TargetVerifications");
        output.push_back('[');
        for (size_t index = 0; index < verifications.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendVerification(output, *verifications[index]);
        }
        output.push_back(']');
        output.push_back(',');

        AZStd::vector<const AdapterDeploymentRollbackResult*> rollbacks;
        for (const AdapterDeploymentRollbackResult& rollback : envelope.m_rollbackResults)
        {
            rollbacks.push_back(&rollback);
        }
        AZStd::sort(
            rollbacks.begin(),
            rollbacks.end(),
            [](const AdapterDeploymentRollbackResult* left,
                const AdapterDeploymentRollbackResult* right)
            {
                return left->m_rollbackResultId < right->m_rollbackResultId;
            });
        Json::AppendName(output, "RollbackResults");
        output.push_back('[');
        for (size_t index = 0; index < rollbacks.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendRollback(output, *rollbacks[index]);
        }
        output.push_back(']');
        output.push_back(',');

        AZStd::vector<const AdapterDeploymentExecutionFailure*> failures;
        for (const AdapterDeploymentExecutionFailure& failure : envelope.m_failures)
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
        Json::AppendName(output, "Failures");
        output.push_back('[');
        for (size_t index = 0; index < failures.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendFailure(output, *failures[index]);
        }
        output.push_back(']');
        output.push_back(',');

        AZStd::vector<const AdapterDeploymentExecutionLogReference*> logs;
        for (const AdapterDeploymentExecutionLogReference& log : envelope.m_logReferences)
        {
            logs.push_back(&log);
        }
        AZStd::sort(
            logs.begin(),
            logs.end(),
            [](const AdapterDeploymentExecutionLogReference* left,
                const AdapterDeploymentExecutionLogReference* right)
            {
                return left->m_logId < right->m_logId;
            });
        Json::AppendName(output, "LogReferences");
        output.push_back('[');
        for (size_t index = 0; index < logs.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendLog(output, *logs[index]);
        }
        output.push_back(']');
        output.push_back('}');
        return output;
    }

    AZStd::string CalculateDeploymentExecutionResultFingerprint(
        const AdapterDeploymentExecutionResultEnvelope& envelope)
    {
        return CalculateCanonicalSha256(
            SerializeCanonicalDeploymentExecutionResult(envelope));
    }

    bool DeploymentExecutionResultFingerprintMatches(
        const AdapterDeploymentExecutionResultEnvelope& envelope)
    {
        return envelope.m_resultFingerprint
            == CalculateDeploymentExecutionResultFingerprint(envelope);
    }
} // namespace TaintedGrailModdingSDK
