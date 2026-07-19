/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentExecutionEvidenceService.h"
#include "AdapterDeploymentExecutionResultCanonical.h"
#include "CanonicalFingerprint.h"

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
                buffer[--position] = static_cast<char>('0' + (value % 10));
                value /= 10;
            } while (value != 0);
            return AZStd::string(buffer + position, sizeof(buffer) - position);
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
            step.m_previousFingerprint = AZStd::move(previousFingerprint);
            step.m_desiredFingerprint = AZStd::move(desiredFingerprint);
            step.m_executionAllowed = false;
            return step;
        }

        AdapterDeploymentWorkOrder MakeWorkOrder()
        {
            AdapterDeploymentWorkOrder workOrder;
            workOrder.m_workOrderId = "deployment.work-order:owner.preview";
            workOrder.m_previewId = "deploymentpreview:owner.preview";
            workOrder.m_previewFingerprint = Fingerprint('a');
            workOrder.m_packId = "owner.preview";
            workOrder.m_targetInventoryId = "owner.target-inventory";
            workOrder.m_status = AdapterDeploymentWorkOrderStatus::ReviewReady;
            workOrder.m_canonicalJson =
                "{\"WorkOrderId\":\"deployment.work-order:owner.preview\","
                "\"ExecutionAllowed\":false}";
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
                    AdapterDeploymentWorkOrderStepKind::ConfirmMaintenanceWindow,
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
            result.m_previousFingerprint = step.m_previousFingerprint;
            result.m_desiredFingerprint = step.m_desiredFingerprint;
            result.m_outcome = AdapterDeploymentExecutionOutcome::Succeeded;
            result.m_attempted = true;
            if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Add
                || step.m_kind == AdapterDeploymentWorkOrderStepKind::Replace)
            {
                result.m_targetPresent = true;
                result.m_observedFingerprint = step.m_desiredFingerprint;
            }
            else if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Backup)
            {
                result.m_targetPresent = true;
                result.m_observedFingerprint = step.m_previousFingerprint;
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
                step.m_kind != AdapterDeploymentWorkOrderStepKind::Remove;
            verification.m_expectedFingerprint = verification.m_expectedPresent
                ? step.m_desiredFingerprint
                : AZStd::string{};
            verification.m_observedPresent = verification.m_expectedPresent;
            verification.m_observedFingerprint =
                verification.m_expectedFingerprint;
            verification.m_status = AdapterDeploymentVerificationStatus::Matched;
            verification.m_checkedAtUtc = "2026-07-19T14:05:00Z";
            return verification;
        }

        AdapterDeploymentRollbackResult Rollback(
            const AdapterDeploymentWorkOrderStep& step)
        {
            AdapterDeploymentRollbackResult rollback;
            rollback.m_rollbackResultId =
                "deployment.rollback-result:" + UnsignedString(step.m_sequence);
            rollback.m_sourceStepId = step.m_stepId;
            rollback.m_targetPath = step.m_targetPath;
            rollback.m_outcome =
                AdapterDeploymentExecutionOutcome::NotAttempted;
            rollback.m_attempted = false;
            if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Add)
            {
                rollback.m_action =
                    AdapterDeploymentRollbackAction::RemoveAdded;
                rollback.m_expectedDeployedFingerprint =
                    step.m_desiredFingerprint;
            }
            else if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Replace)
            {
                rollback.m_action =
                    AdapterDeploymentRollbackAction::RestoreReplaced;
                rollback.m_backupPath = step.m_backupPath;
                rollback.m_expectedDeployedFingerprint =
                    step.m_desiredFingerprint;
                rollback.m_restoreFingerprint = step.m_previousFingerprint;
            }
            else
            {
                rollback.m_action =
                    AdapterDeploymentRollbackAction::RestoreRemoved;
                rollback.m_backupPath = step.m_backupPath;
                rollback.m_restoreFingerprint = step.m_previousFingerprint;
            }
            return rollback;
        }

        void RefreshFingerprint(
            AdapterDeploymentExecutionResultEnvelope& envelope)
        {
            envelope.m_resultFingerprint =
                CalculateDeploymentExecutionResultFingerprint(envelope);
        }

        AdapterDeploymentExecutionResultEnvelope MakeEnvelope(
            const AdapterDeploymentWorkOrder& workOrder)
        {
            AdapterDeploymentExecutionResultEnvelope envelope;
            envelope.m_resultId = "deployment.result:owner.preview";
            envelope.m_workOrderId = workOrder.m_workOrderId;
            envelope.m_workOrderCanonicalJson = workOrder.m_canonicalJson;
            envelope.m_workOrderFingerprint =
                CalculateCanonicalSha256(workOrder.m_canonicalJson);
            envelope.m_previewId = workOrder.m_previewId;
            envelope.m_previewFingerprint = workOrder.m_previewFingerprint;
            envelope.m_packId = workOrder.m_packId;
            envelope.m_targetInventoryId = workOrder.m_targetInventoryId;
            envelope.m_profileId = "owner.profile";
            envelope.m_gameVersion = "1.0.0";
            envelope.m_branch = "release";
            envelope.m_runtimeTarget = "Mono";
            envelope.m_capturedAtUtc = "2026-07-19T14:10:00Z";

            envelope.m_executorReview.m_reviewId = "owner.executor-review";
            envelope.m_executorReview.m_executorId =
                "owner.deployment-executor";
            envelope.m_executorReview.m_executorVersion = "0.1.0";
            envelope.m_executorReview.m_executorFingerprint = Fingerprint('d');
            envelope.m_executorReview.m_decision =
                AdapterDeploymentExecutorReviewDecision::Accepted;
            envelope.m_executorReview.m_reviewer = "deployment-reviewer";
            envelope.m_executorReview.m_evidenceIds = {
                "evidence.executor.review",
            };
            envelope.m_executorReview.m_reviewedAtUtc =
                "2026-07-19T13:00:00Z";

            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                envelope.m_stepResults.push_back(StepResult(step));
                if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Backup)
                {
                    AdapterDeploymentBackupResult backup;
                    backup.m_stepId = step.m_stepId;
                    backup.m_targetPath = step.m_targetPath;
                    backup.m_backupPath = step.m_backupPath;
                    backup.m_sourceFingerprint = step.m_previousFingerprint;
                    backup.m_backupFingerprint = step.m_previousFingerprint;
                    backup.m_outcome =
                        AdapterDeploymentExecutionOutcome::Succeeded;
                    backup.m_attempted = true;
                    envelope.m_backupResults.push_back(AZStd::move(backup));
                }
                if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Add
                    || step.m_kind
                        == AdapterDeploymentWorkOrderStepKind::Replace
                    || step.m_kind
                        == AdapterDeploymentWorkOrderStepKind::Remove)
                {
                    envelope.m_targetVerifications.push_back(
                        Verification(step));
                    envelope.m_rollbackResults.push_back(Rollback(step));
                }
            }
            RefreshFingerprint(envelope);
            return envelope;
        }

        bool HasIssue(
            const AdapterDeploymentExecutionEvidenceReturn& result,
            const AZStd::string& code)
        {
            for (const AdapterDeploymentExecutionResultIssue& issue : result.m_issues)
            {
                if (issue.m_code == code)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    TEST(
        AdapterDeploymentExecutionResultTests,
        TypedVocabulariesReferencesAndDatesAreStrict)
    {
        AdapterDeploymentExecutionOutcome outcome =
            AdapterDeploymentExecutionOutcome::NotAttempted;
        EXPECT_TRUE(TryParseAdapterDeploymentExecutionOutcome("succeeded", outcome));
        EXPECT_FALSE(TryParseAdapterDeploymentExecutionOutcome("complete", outcome));

        AdapterDeploymentExecutionFailureKind failureKind =
            AdapterDeploymentExecutionFailureKind::Unknown;
        EXPECT_TRUE(TryParseAdapterDeploymentExecutionFailureKind("backup", failureKind));
        EXPECT_FALSE(TryParseAdapterDeploymentExecutionFailureKind("unknown", failureKind));

        EXPECT_TRUE(IsAdapterDeploymentExecutionFingerprint(Fingerprint('a')));
        EXPECT_FALSE(IsAdapterDeploymentExecutionFingerprint("sha256:ABC"));
        EXPECT_TRUE(IsAdapterDeploymentExecutionLogReference("Reports/deployment.log"));
        EXPECT_FALSE(IsAdapterDeploymentExecutionLogReference("../deployment.log"));
        EXPECT_TRUE(IsAdapterDeploymentExecutionUtcTimestamp("2024-02-29T12:00:00Z"));
        EXPECT_FALSE(IsAdapterDeploymentExecutionUtcTimestamp("2026-02-31T12:00:00Z"));
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        UnboundRegistrationIsProhibited)
    {
        const AdapterDeploymentWorkOrder workOrder = MakeWorkOrder();
        const AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope(workOrder);
        AdapterDeploymentExecutionResultRegistry registry;
        AZStd::string error;
        EXPECT_FALSE(registry.RegisterEnvelope(envelope, &error));
        EXPECT_NE(error.find("Unbound"), AZStd::string::npos);
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        BoundRegistryRejectsDuplicateResultIdentity)
    {
        const AdapterDeploymentWorkOrder workOrder = MakeWorkOrder();
        const AdapterDeploymentExecutionResultEnvelope envelope =
            MakeEnvelope(workOrder);
        AdapterDeploymentExecutionResultRegistry registry;
        AZStd::string error;
        ASSERT_TRUE(registry.RegisterEnvelope(workOrder, envelope, &error))
            << error.c_str();
        EXPECT_FALSE(registry.RegisterEnvelope(workOrder, envelope, &error));
        EXPECT_NE(error.find("already exists"), AZStd::string::npos);
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        CompleteEnvelopeReturnsCandidateEvidenceOnly)
    {
        const AdapterDeploymentWorkOrder workOrder = MakeWorkOrder();
        AdapterDeploymentExecutionEvidenceService service;
        const AdapterDeploymentExecutionEvidenceReturn result =
            service.BuildEvidenceReturn(workOrder, MakeEnvelope(workOrder));
        EXPECT_TRUE(result.m_accepted);
        EXPECT_EQ(result.m_status, AdapterDeploymentExecutionEnvelopeStatus::Accepted);
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
        CallerSelectedWorkOrderAndResultFingerprintsFailClosed)
    {
        const AdapterDeploymentWorkOrder workOrder = MakeWorkOrder();
        AdapterDeploymentExecutionEvidenceService service;

        AdapterDeploymentExecutionResultEnvelope workOrderTampered =
            MakeEnvelope(workOrder);
        workOrderTampered.m_workOrderFingerprint = Fingerprint('b');
        RefreshFingerprint(workOrderTampered);
        const auto workOrderResult =
            service.BuildEvidenceReturn(workOrder, workOrderTampered);
        EXPECT_FALSE(workOrderResult.m_accepted);
        EXPECT_TRUE(HasIssue(
            workOrderResult,
            "deployment_result.work_order_binding_mismatch"));

        AdapterDeploymentExecutionResultEnvelope resultTampered =
            MakeEnvelope(workOrder);
        resultTampered.m_resultFingerprint = Fingerprint('c');
        const auto result = service.BuildEvidenceReturn(workOrder, resultTampered);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "deployment_result.envelope_invalid"));
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        MissingStepBackupVerificationAndRollbackFailClosed)
    {
        const AdapterDeploymentWorkOrder workOrder = MakeWorkOrder();
        AdapterDeploymentExecutionEvidenceService service;

        AdapterDeploymentExecutionResultEnvelope missingStep =
            MakeEnvelope(workOrder);
        missingStep.m_stepResults.pop_back();
        RefreshFingerprint(missingStep);
        auto result = service.BuildEvidenceReturn(workOrder, missingStep);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "deployment_result.step_count_mismatch"));

        AdapterDeploymentExecutionResultEnvelope missingBackup =
            MakeEnvelope(workOrder);
        missingBackup.m_backupResults.clear();
        RefreshFingerprint(missingBackup);
        result = service.BuildEvidenceReturn(workOrder, missingBackup);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "deployment_result.backup_missing"));

        AdapterDeploymentExecutionResultEnvelope missingVerification =
            MakeEnvelope(workOrder);
        missingVerification.m_targetVerifications.clear();
        RefreshFingerprint(missingVerification);
        result = service.BuildEvidenceReturn(workOrder, missingVerification);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "deployment_result.verification_missing"));

        AdapterDeploymentExecutionResultEnvelope missingRollback =
            MakeEnvelope(workOrder);
        missingRollback.m_rollbackResults.clear();
        RefreshFingerprint(missingRollback);
        result = service.BuildEvidenceReturn(workOrder, missingRollback);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "deployment_result.rollback_missing"));
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        UnknownFailureKindAndImpossibleCaptureDateAreRejectedBeforeStorage)
    {
        const AdapterDeploymentWorkOrder workOrder = MakeWorkOrder();
        AdapterDeploymentExecutionResultRegistry registry;
        AZStd::string error;

        AdapterDeploymentExecutionResultEnvelope impossible =
            MakeEnvelope(workOrder);
        impossible.m_capturedAtUtc = "2026-02-31T12:00:00Z";
        RefreshFingerprint(impossible);
        EXPECT_FALSE(registry.RegisterEnvelope(workOrder, impossible, &error));
        EXPECT_NE(error.find("UTC capture"), AZStd::string::npos);

        AdapterDeploymentExecutionResultEnvelope unknown =
            MakeEnvelope(workOrder);
        AdapterDeploymentExecutionFailure failure;
        failure.m_failureId = "deployment.failure:unknown";
        failure.m_kind = AdapterDeploymentExecutionFailureKind::Unknown;
        failure.m_code = "unknown_failure";
        failure.m_message = "Unknown classifications are not evidence.";
        failure.m_stepId = workOrder.m_steps[3].m_stepId;
        unknown.m_failures.push_back(failure);
        unknown.m_stepResults[3].m_outcome =
            AdapterDeploymentExecutionOutcome::Failed;
        unknown.m_stepResults[3].m_attempted = true;
        unknown.m_stepResults[3].m_failureIds = { failure.m_failureId };
        unknown.m_stepResults[3].m_observedFingerprint.clear();
        unknown.m_stepResults[3].m_targetPresent = false;
        RefreshFingerprint(unknown);
        EXPECT_FALSE(registry.RegisterEnvelope(workOrder, unknown, &error));
        EXPECT_NE(error.find("Unknown"), AZStd::string::npos);
    }

    TEST(
        AdapterDeploymentExecutionResultTests,
        CanonicalPayloadIsOrderIndependentAndContentSensitive)
    {
        const AdapterDeploymentWorkOrder workOrder = MakeWorkOrder();
        AdapterDeploymentExecutionResultEnvelope first = MakeEnvelope(workOrder);
        AdapterDeploymentExecutionResultEnvelope reordered = first;
        AZStd::reverse(
            reordered.m_stepResults.begin(),
            reordered.m_stepResults.end());
        AZStd::reverse(
            reordered.m_rollbackResults.begin(),
            reordered.m_rollbackResults.end());
        EXPECT_EQ(
            SerializeCanonicalDeploymentExecutionResult(first),
            SerializeCanonicalDeploymentExecutionResult(reordered));

        reordered.m_branch = "different";
        EXPECT_NE(
            SerializeCanonicalDeploymentExecutionResult(first),
            SerializeCanonicalDeploymentExecutionResult(reordered));
    }
} // namespace TaintedGrailModdingSDK
