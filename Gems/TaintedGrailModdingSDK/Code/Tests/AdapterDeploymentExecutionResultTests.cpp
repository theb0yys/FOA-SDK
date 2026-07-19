/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentExecutionEvidenceService.h"

#include <AzTest/AzTest.h>

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string Fingerprint(char digit)
        {
            return AZStd::string("sha256:") + AZStd::string(64, digit);
        }

        AZStd::string UnsignedString(AZ::u64 value)
        {
            char buffer[32];
            size_t position = sizeof(buffer);
            do
            {
                buffer[--position] =
                    static_cast<char>('0' + (value % 10));
                value /= 10;
            } while (value != 0);
            return AZStd::string(
                buffer + position,
                sizeof(buffer) - position);
        }

        AdapterDeploymentWorkOrderStep Step(
            AZ::u64 sequence,
            AdapterDeploymentWorkOrderStepKind kind,
            AZStd::string targetPath = {},
            AZStd::string backupPath = {},
            AZStd::string previousFingerprint = {},
            AZStd::string desiredFingerprint = {})
        {
            AdapterDeploymentWorkOrderStep step;
            step.m_sequence = sequence;
            step.m_stepId =
                "deployment.work-order:owner.preview:step:"
                + UnsignedString(sequence);
            step.m_kind = kind;
            step.m_targetPath = AZStd::move(targetPath);
            step.m_backupPath = AZStd::move(backupPath);
            step.m_previousFingerprint =
                AZStd::move(previousFingerprint);
            step.m_desiredFingerprint =
                AZStd::move(desiredFingerprint);
            step.m_executionAllowed = false;
            return step;
        }

        AdapterDeploymentWorkOrder MakeWorkOrder()
        {
            AdapterDeploymentWorkOrder workOrder;
            workOrder.m_workOrderId =
                "deployment.work-order:owner.preview";
            workOrder.m_previewId =
                "deploymentpreview:owner.preview";
            workOrder.m_previewFingerprint = Fingerprint('a');
            workOrder.m_packId = "owner.preview";
            workOrder.m_targetInventoryId =
                "owner.target-inventory";
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::ReviewReady;
            workOrder.m_canonicalJson =
                "{\"ExecutionAllowed\":false}";
            workOrder.m_steps = {
                Step(
                    1,
                    AdapterDeploymentWorkOrderStepKind::VerifyPreflight,
                    {},
                    {},
                    {},
                    Fingerprint('a')),
                Step(
                    2,
                    AdapterDeploymentWorkOrderStepKind::
                        ConfirmMaintenanceWindow,
                    "BepInEx/plugins/owner.preview"),
                Step(
                    3,
                    AdapterDeploymentWorkOrderStepKind::Backup,
                    "BepInEx/plugins/owner.preview/old.dll",
                    "Backups/owner.preview/old.dll",
                    Fingerprint('1'),
                    Fingerprint('1')),
                Step(
                    4,
                    AdapterDeploymentWorkOrderStepKind::Add,
                    "BepInEx/plugins/owner.preview/new.dll",
                    {},
                    {},
                    Fingerprint('2')),
                Step(
                    5,
                    AdapterDeploymentWorkOrderStepKind::Replace,
                    "BepInEx/plugins/owner.preview/replace.dll",
                    "Backups/owner.preview/replace.dll",
                    Fingerprint('3'),
                    Fingerprint('4')),
                Step(
                    6,
                    AdapterDeploymentWorkOrderStepKind::Remove,
                    "BepInEx/plugins/owner.preview/old.dll",
                    "Backups/owner.preview/old.dll",
                    Fingerprint('1'),
                    {}),
                Step(
                    7,
                    AdapterDeploymentWorkOrderStepKind::VerifyDeployment,
                    "BepInEx/plugins/owner.preview",
                    {},
                    {},
                    Fingerprint('a')),
                Step(
                    8,
                    AdapterDeploymentWorkOrderStepKind::PreserveRollback,
                    "Backups/owner.preview",
                    "Backups/owner.preview"),
            };
            return workOrder;
        }

        AdapterDeploymentExecutionStepResult StepResult(
            const AdapterDeploymentWorkOrderStep& step)
        {
            AdapterDeploymentExecutionStepResult result;
            result.m_stepId = step.m_stepId;
            result.m_sequence = step.m_sequence;
            result.m_kind = step.m_kind;
            result.m_targetPath = step.m_targetPath;
            result.m_backupPath = step.m_backupPath;
            result.m_previousFingerprint =
                step.m_previousFingerprint;
            result.m_desiredFingerprint =
                step.m_desiredFingerprint;
            result.m_outcome =
                AdapterDeploymentExecutionOutcome::Succeeded;
            result.m_attempted = true;
            if (step.m_kind
                    == AdapterDeploymentWorkOrderStepKind::Add
                || step.m_kind
                    == AdapterDeploymentWorkOrderStepKind::Replace)
            {
                result.m_targetPresent = true;
                result.m_observedFingerprint =
                    step.m_desiredFingerprint;
            }
            else if (step.m_kind
                == AdapterDeploymentWorkOrderStepKind::Backup)
            {
                result.m_targetPresent = true;
                result.m_observedFingerprint =
                    step.m_previousFingerprint;
            }
            return result;
        }

        AdapterDeploymentTargetVerification Verification(
            const AdapterDeploymentWorkOrderStep& step)
        {
            AdapterDeploymentTargetVerification verification;
            verification.m_stepId = step.m_stepId;
            verification.m_targetPath = step.m_targetPath;
            verification.m_expectedPresent =
                step.m_kind
                != AdapterDeploymentWorkOrderStepKind::Remove;
            verification.m_expectedFingerprint =
                verification.m_expectedPresent
                ? step.m_desiredFingerprint
                : AZStd::string{};
            verification.m_observedPresent =
                verification.m_expectedPresent;
            verification.m_observedFingerprint =
                verification.m_expectedFingerprint;
            verification.m_status =
                AdapterDeploymentVerificationStatus::Matched;
            verification.m_checkedAtUtc =
                "2026-07-19T14:05:00Z";
            return verification;
        }

        AdapterDeploymentRollbackResult Rollback(
            const AdapterDeploymentWorkOrderStep& step)
        {
            AdapterDeploymentRollbackResult rollback;
            rollback.m_rollbackResultId =
                "deployment.rollback-result:"
                + UnsignedString(step.m_sequence);
            rollback.m_sourceStepId = step.m_stepId;
            rollback.m_targetPath = step.m_targetPath;
            rollback.m_outcome =
                AdapterDeploymentExecutionOutcome::NotAttempted;
            rollback.m_attempted = false;
            if (step.m_kind
                == AdapterDeploymentWorkOrderStepKind::Add)
            {
                rollback.m_action =
                    AdapterDeploymentRollbackAction::RemoveAdded;
                rollback.m_expectedDeployedFingerprint =
                    step.m_desiredFingerprint;
            }
            else if (step.m_kind
                == AdapterDeploymentWorkOrderStepKind::Replace)
            {
                rollback.m_action =
                    AdapterDeploymentRollbackAction::RestoreReplaced;
                rollback.m_backupPath = step.m_backupPath;
                rollback.m_expectedDeployedFingerprint =
                    step.m_desiredFingerprint;
                rollback.m_restoreFingerprint =
                    step.m_previousFingerprint;
            }
            else
            {
                rollback.m_action =
                    AdapterDeploymentRollbackAction::RestoreRemoved;
                rollback.m_backupPath = step.m_backupPath;
                rollback.m_restoreFingerprint =
                    step.m_previousFingerprint;
            }
            return rollback;
        }

        AdapterDeploymentExecutionResultEnvelope MakeEnvelope()
        {
            const AdapterDeploymentWorkOrder workOrder =
                MakeWorkOrder();
            AdapterDeploymentExecutionResultEnvelope envelope;
            envelope.m_resultId =
                "deployment.result:owner.preview";
            envelope.m_workOrderId = workOrder.m_workOrderId;
            envelope.m_workOrderCanonicalJson =
                workOrder.m_canonicalJson;
            envelope.m_workOrderFingerprint = Fingerprint('b');
            envelope.m_previewId = workOrder.m_previewId;
            envelope.m_previewFingerprint =
                workOrder.m_previewFingerprint;
            envelope.m_packId = workOrder.m_packId;
            envelope.m_targetInventoryId =
                workOrder.m_targetInventoryId;
            envelope.m_profileId = "owner.profile";
            envelope.m_gameVersion = "1.0.0";
            envelope.m_branch = "release";
            envelope.m_runtimeTarget = "Mono";
            envelope.m_capturedAtUtc =
                "2026-07-19T14:10:00Z";
            envelope.m_resultFingerprint = Fingerprint('c');

            envelope.m_executorReview.m_reviewId =
                "owner.executor-review";
            envelope.m_executorReview.m_executorId =
                "owner.deployment-executor";
            envelope.m_executorReview.m_executorVersion =
                "0.1.0";
            envelope.m_executorReview.m_executorFingerprint =
                Fingerprint('d');
            envelope.m_executorReview.m_decision =
                AdapterDeploymentExecutorReviewDecision::Accepted;
            envelope.m_executorReview.m_reviewer =
                "deployment-reviewer";
            envelope.m_executorReview.m_evidenceIds = {
                "evidence.executor.review",
            };
            envelope.m_executorReview.m_reviewedAtUtc =
                "2026-07-19T13:00:00Z";

            for (const AdapterDeploymentWorkOrderStep& step :
                workOrder.m_steps)
            {
                envelope.m_stepResults.push_back(
                    StepResult(step));
                if (step.m_kind
                    == AdapterDeploymentWorkOrderStepKind::Backup)
                {
                    AdapterDeploymentBackupResult backup;
                    backup.m_stepId = step.m_stepId;
                    backup.m_targetPath = step.m_targetPath;
                    backup.m_backupPath = step.m_backupPath;
                    backup.m_sourceFingerprint =
                        step.m_previousFingerprint;
                    backup.m_backupFingerprint =
                        step.m_previousFingerprint;
                    backup.m_outcome =
                        AdapterDeploymentExecutionOutcome::Succeeded;
                    backup.m_attempted = true;
                    envelope.m_backupResults.push_back(
                        AZStd::move(backup));
                }
                if (step.m_kind
                        == AdapterDeploymentWorkOrderStepKind::Add
                    || step.m_kind
                        == AdapterDeploymentWorkOrderStepKind::Replace
                    || step.m_kind
                        == AdapterDeploymentWorkOrderStepKind::Remove)
                {
                    envelope.m_targetVerifications.push_back(
                        Verification(step));
                    envelope.m_rollbackResults.push_back(
                        Rollback(step));
                }
            }
            return envelope;
        }
    } // namespace

    TEST(
        AdapterDeploymentExecutionResultTests,
        TypedVocabulariesAndReferenceBoundariesAreStrict)
    {
        AdapterDeploymentExecutionOutcome outcome =
            AdapterDeploymentExecutionOutcome::NotAttempted;
        EXPECT_TRUE(
            TryParseAdapterDeploymentExecutionOutcome(
                "succeeded",
                outcome));
        EXPECT_EQ(
            outcome,
            AdapterDeploymentExecutionOutcome::Succeeded);
        EXPECT_FALSE(
            TryParseAdapterDeploymentExecutionOutcome(
                "complete",
                outcome));

        AdapterDeploymentVerificationStatus verification =
            AdapterDeploymentVerificationStatus::NotChecked;
        EXPECT_TRUE(
            TryParseAdapterDeploymentVerificationStatus(
                "mismatched",
                verification));
        EXPECT_FALSE(
            TryParseAdapterDeploymentVerificationStatus(
                "close_enough",
                verification));

        EXPECT_TRUE(
            IsAdapterDeploymentExecutionFingerprint(
                Fingerprint('a')));
        EXPECT_FALSE(
            IsAdapterDeploymentExecutionFingerprint(
                "sha256:ABC"));
        EXPECT_TRUE(
            IsAdapterDeploymentExecutionLogReference(
                "Reports/deployment.log"));
        EXPECT_FALSE(
            IsAdapterDeploymentExecutionLogReference(
                "../deployment.log"));
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        RegistryRejectsDuplicateResultIdentity)
    {
        AdapterDeploymentExecutionResultRegistry& registry =
            AdapterDeploymentExecutionResultRegistry::Get();
        registry.Clear();
        const AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope();
        AZStd::string error;
        EXPECT_TRUE(
            registry.RegisterEnvelope(envelope, &error))
            << error.c_str();
        EXPECT_FALSE(
            registry.RegisterEnvelope(envelope, &error));
        EXPECT_NE(
            error.find("already exists"),
            AZStd::string::npos);
        registry.Clear();
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        CompleteEnvelopeReturnsCandidateEvidenceOnly)
    {
        AdapterDeploymentExecutionEvidenceService service;
        const AdapterDeploymentExecutionEvidenceReturn result =
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                MakeEnvelope());
        EXPECT_TRUE(result.m_accepted);
        EXPECT_EQ(
            result.m_status,
            AdapterDeploymentExecutionEnvelopeStatus::Accepted);
        EXPECT_TRUE(result.m_issues.empty());
        EXPECT_EQ(result.m_stepResultCount, 8);
        EXPECT_EQ(result.m_backupResultCount, 1);
        EXPECT_EQ(result.m_verificationCount, 3);
        EXPECT_EQ(result.m_rollbackResultCount, 3);
        EXPECT_FALSE(result.m_sourceDocuments.empty());
        EXPECT_FALSE(result.m_evidenceDocuments.empty());
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        WorkOrderNotReadyPrecedesExecutorAndEnvelopeFailures)
    {
        AdapterDeploymentWorkOrder workOrder = MakeWorkOrder();
        workOrder.m_status =
            AdapterDeploymentWorkOrderStatus::PreflightFailed;
        AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope();
        envelope.m_executorReview.m_decision =
            AdapterDeploymentExecutorReviewDecision::Rejected;
        envelope.m_stepResults.clear();

        AdapterDeploymentExecutionEvidenceService service;
        const AdapterDeploymentExecutionEvidenceReturn result =
            service.BuildEvidenceReturn(workOrder, envelope);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_EQ(
            result.m_status,
            AdapterDeploymentExecutionEnvelopeStatus::
                WorkOrderNotReady);
        EXPECT_TRUE(result.m_sourceDocuments.empty());
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        ExecutorReviewAndExactWorkOrderBindingFailClosed)
    {
        AdapterDeploymentExecutionEvidenceService service;
        AdapterDeploymentExecutionResultEnvelope unreviewed =
            MakeEnvelope();
        unreviewed.m_executorReview.m_evidenceIds.clear();
        EXPECT_EQ(
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                unreviewed).m_status,
            AdapterDeploymentExecutionEnvelopeStatus::
                ExecutorUnreviewed);

        AdapterDeploymentExecutionResultEnvelope drifted =
            MakeEnvelope();
        drifted.m_previewId = "deploymentpreview:other";
        EXPECT_EQ(
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                drifted).m_status,
            AdapterDeploymentExecutionEnvelopeStatus::
                WorkOrderBindingMismatch);
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        StepIdentityAndOutcomeShapeAreExact)
    {
        AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope();
        envelope.m_stepResults[3].m_sequence = 99;

        AdapterDeploymentExecutionEvidenceService service;
        EXPECT_EQ(
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                envelope).m_status,
            AdapterDeploymentExecutionEnvelopeStatus::
                StepBindingMismatch);

        envelope = MakeEnvelope();
        envelope.m_stepResults[3].m_outcome =
            AdapterDeploymentExecutionOutcome::Failed;
        envelope.m_stepResults[3].m_failureIds.clear();
        EXPECT_EQ(
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                envelope).m_status,
            AdapterDeploymentExecutionEnvelopeStatus::
                StepBindingMismatch);
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        BackupOutcomePreservesExactPreChangeFingerprint)
    {
        AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope();
        envelope.m_backupResults.front().m_backupFingerprint =
            Fingerprint('f');

        AdapterDeploymentExecutionEvidenceService service;
        const AdapterDeploymentExecutionEvidenceReturn result =
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                envelope);
        EXPECT_EQ(
            result.m_status,
            AdapterDeploymentExecutionEnvelopeStatus::
                BackupBindingMismatch);
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        TargetVerificationKeepsMatchedAndMismatchedDistinct)
    {
        AdapterDeploymentExecutionEvidenceService service;
        AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope();
        envelope.m_targetVerifications.front().m_status =
            AdapterDeploymentVerificationStatus::Mismatched;
        EXPECT_EQ(
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                envelope).m_status,
            AdapterDeploymentExecutionEnvelopeStatus::
                VerificationBindingMismatch);

        envelope = MakeEnvelope();
        envelope.m_targetVerifications.front().m_status =
            AdapterDeploymentVerificationStatus::Mismatched;
        envelope.m_targetVerifications.front().
            m_observedFingerprint = Fingerprint('f');
        EXPECT_TRUE(
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                envelope).m_accepted);
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        RollbackRestoreOutcomesBindExactInverseState)
    {
        AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope();
        AdapterDeploymentRollbackResult& replacement =
            envelope.m_rollbackResults[1];
        replacement.m_outcome =
            AdapterDeploymentExecutionOutcome::Succeeded;
        replacement.m_attempted = true;
        replacement.m_targetPresent = true;
        replacement.m_finalFingerprint =
            replacement.m_restoreFingerprint;

        AdapterDeploymentExecutionEvidenceService service;
        EXPECT_TRUE(
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                envelope).m_accepted);

        replacement.m_finalFingerprint = Fingerprint('f');
        EXPECT_EQ(
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                envelope).m_status,
            AdapterDeploymentExecutionEnvelopeStatus::
                RollbackBindingMismatch);
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        FailureAndLogReferencesRemainSameStepBound)
    {
        AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope();
        AdapterDeploymentExecutionStepResult& add =
            envelope.m_stepResults[3];
        add.m_outcome =
            AdapterDeploymentExecutionOutcome::Failed;
        add.m_failureIds = { "deployment.failure:add" };

        AdapterDeploymentExecutionFailure failure;
        failure.m_failureId = "deployment.failure:add";
        failure.m_kind =
            AdapterDeploymentExecutionFailureKind::Copy;
        failure.m_code = "copy_failed";
        failure.m_message = "Executor reported copy failure.";
        failure.m_stepId = add.m_stepId;
        failure.m_logReferenceIds = {
            "deployment.log:executor",
        };
        envelope.m_failures.push_back(failure);

        AdapterDeploymentExecutionLogReference log;
        log.m_logId = "deployment.log:executor";
        log.m_kind =
            AdapterDeploymentExecutionLogKind::Executor;
        log.m_reference = "Reports/executor.log";
        log.m_fingerprint = Fingerprint('e');
        log.m_stepIds = { add.m_stepId };
        envelope.m_logReferences.push_back(log);
        add.m_logReferenceIds = { log.m_logId };

        AdapterDeploymentExecutionEvidenceService service;
        EXPECT_TRUE(
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                envelope).m_accepted);

        envelope.m_logReferences.front().m_stepIds.clear();
        EXPECT_EQ(
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                envelope).m_status,
            AdapterDeploymentExecutionEnvelopeStatus::
                FailureLogBindingMismatch);
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        FailedExecutionCanStillBeContractAcceptedEvidence)
    {
        AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope();
        AdapterDeploymentExecutionStepResult& replace =
            envelope.m_stepResults[4];
        replace.m_outcome =
            AdapterDeploymentExecutionOutcome::Failed;
        replace.m_failureIds = {
            "deployment.failure:replace",
        };

        AdapterDeploymentExecutionFailure failure;
        failure.m_failureId =
            "deployment.failure:replace";
        failure.m_kind =
            AdapterDeploymentExecutionFailureKind::Replace;
        failure.m_code = "replace_failed";
        failure.m_message =
            "Executor reported replacement failure.";
        failure.m_stepId = replace.m_stepId;
        envelope.m_failures.push_back(failure);

        AdapterDeploymentExecutionEvidenceService service;
        const AdapterDeploymentExecutionEvidenceReturn result =
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                envelope);
        EXPECT_TRUE(result.m_accepted);
        EXPECT_EQ(result.m_failureCount, 1);
        EXPECT_GT(result.m_evidenceRecordCount, 0);
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        CandidateEvidenceOrderingIsDeterministic)
    {
        AdapterDeploymentExecutionResultEnvelope first =
            MakeEnvelope();
        AdapterDeploymentExecutionResultEnvelope second =
            first;
        AZStd::reverse(
            second.m_stepResults.begin(),
            second.m_stepResults.end());
        AZStd::reverse(
            second.m_targetVerifications.begin(),
            second.m_targetVerifications.end());
        AZStd::reverse(
            second.m_rollbackResults.begin(),
            second.m_rollbackResults.end());

        AdapterDeploymentExecutionEvidenceService service;
        const AdapterDeploymentExecutionEvidenceReturn left =
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                first);
        const AdapterDeploymentExecutionEvidenceReturn right =
            service.BuildEvidenceReturn(
                MakeWorkOrder(),
                second);
        ASSERT_TRUE(left.m_accepted);
        ASSERT_TRUE(right.m_accepted);
        ASSERT_EQ(
            left.m_evidenceDocuments.size(),
            right.m_evidenceDocuments.size());
        EXPECT_EQ(
            left.m_evidenceDocuments.front().m_evidence.size(),
            right.m_evidenceDocuments.front().m_evidence.size());
        for (size_t index = 0;
            index < left.m_evidenceDocuments.front().
                m_evidence.size();
            ++index)
        {
            EXPECT_EQ(
                left.m_evidenceDocuments.front().
                    m_evidence[index].m_evidenceId,
                right.m_evidenceDocuments.front().
                    m_evidence[index].m_evidenceId);
        }
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        EvidenceReturnDoesNotMutateWorkOrderOrEnvelope)
    {
        AdapterDeploymentWorkOrder workOrder = MakeWorkOrder();
        AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope();
        const size_t stepCount = workOrder.m_steps.size();
        const size_t resultCount = envelope.m_stepResults.size();
        const AZStd::string workOrderJson =
            workOrder.m_canonicalJson;
        const AZStd::string resultFingerprint =
            envelope.m_resultFingerprint;

        AdapterDeploymentExecutionEvidenceService service;
        const AdapterDeploymentExecutionEvidenceReturn result =
            service.BuildEvidenceReturn(workOrder, envelope);
        EXPECT_TRUE(result.m_accepted);
        EXPECT_EQ(workOrder.m_steps.size(), stepCount);
        EXPECT_EQ(envelope.m_stepResults.size(), resultCount);
        EXPECT_EQ(workOrder.m_canonicalJson, workOrderJson);
        EXPECT_EQ(
            envelope.m_resultFingerprint,
            resultFingerprint);
    }
} // namespace TaintedGrailModdingSDK
