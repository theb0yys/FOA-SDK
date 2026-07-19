/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentWorkOrderService.h"
#include "CanonicalFingerprint.h"

#include <AzTest/AzTest.h>

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string Fingerprint(char digit)
        {
            return AZStd::string("sha256:") + AZStd::string(64, digit);
        }

        AdapterDeploymentChange Change(
            AZStd::string id,
            AZStd::string targetPath,
            AZStd::string previous,
            AZStd::string desired,
            AZStd::string targetEntryId = {})
        {
            AdapterDeploymentChange change;
            change.m_changeId = AZStd::move(id);
            change.m_targetPath = AZStd::move(targetPath);
            change.m_previousFingerprint = AZStd::move(previous);
            change.m_desiredFingerprint = AZStd::move(desired);
            change.m_targetEntryId = AZStd::move(targetEntryId);
            return change;
        }

        AdapterDeploymentWorkOrderRequest MakeReadyRequest()
        {
            AdapterDeploymentWorkOrderRequest request;
            request.m_preview.m_previewId = "owner.deployment-preview";
            request.m_preview.m_targetInventoryId = "owner.target-inventory";
            request.m_preview.m_packId = "owner.preview-pack";
            request.m_preview.m_targetRoot =
                "BepInEx/plugins/owner.preview-pack";
            request.m_preview.m_backupRoot = "Backups/owner.preview-pack";
            request.m_preview.m_status =
                AdapterStagingDeploymentPreviewStatus::Ready;
            request.m_preview.m_canonicalJson =
                "{\"DeploymentMutationAllowed\":false,\"LaunchAllowed\":false}";
            request.m_preview.m_stagingMutationAllowed = false;
            request.m_preview.m_deploymentMutationAllowed = false;
            request.m_preview.m_rollbackExecutionAllowed = false;
            request.m_preview.m_launchAllowed = false;

            request.m_preview.m_additions = {
                Change(
                    "change.add",
                    "BepInEx/plugins/owner.preview-pack/new.dll",
                    {},
                    Fingerprint('1')),
            };
            request.m_preview.m_replacements = {
                Change(
                    "change.replace",
                    "BepInEx/plugins/owner.preview-pack/plugin.dll",
                    Fingerprint('2'),
                    Fingerprint('3'),
                    "owner.target.replace"),
            };
            request.m_preview.m_removals = {
                Change(
                    "change.remove",
                    "BepInEx/plugins/owner.preview-pack/old.dll",
                    Fingerprint('4'),
                    {},
                    "owner.target.remove"),
            };
            request.m_preview.m_unchanged = {
                Change(
                    "change.unchanged",
                    "BepInEx/plugins/owner.preview-pack/README.md",
                    Fingerprint('5'),
                    Fingerprint('5'),
                    "owner.target.readme"),
            };

            AdapterDeploymentBackupRequirement replaceBackup;
            replaceBackup.m_backupId = "owner.backup.replace";
            replaceBackup.m_targetEntryId = "owner.target.replace";
            replaceBackup.m_targetPath =
                "BepInEx/plugins/owner.preview-pack/plugin.dll";
            replaceBackup.m_backupPath =
                "Backups/owner.preview-pack/plugin.dll";
            replaceBackup.m_fingerprint = Fingerprint('2');
            request.m_preview.m_backups.push_back(replaceBackup);

            AdapterDeploymentBackupRequirement removeBackup;
            removeBackup.m_backupId = "owner.backup.remove";
            removeBackup.m_targetEntryId = "owner.target.remove";
            removeBackup.m_targetPath =
                "BepInEx/plugins/owner.preview-pack/old.dll";
            removeBackup.m_backupPath =
                "Backups/owner.preview-pack/old.dll";
            removeBackup.m_fingerprint = Fingerprint('4');
            request.m_preview.m_backups.push_back(removeBackup);

            for (int index = 0; index < 3; ++index)
            {
                AdapterDeploymentRollbackStep step;
                step.m_sequence = static_cast<AZ::u64>(index + 1);
                request.m_preview.m_rollbackSteps.push_back(step);
            }

            request.m_previewFingerprint =
                CalculateCanonicalSha256(request.m_preview.m_canonicalJson);
            request.m_confirmation.m_confirmationId = "owner.confirmation";
            request.m_confirmation.m_previewId = request.m_preview.m_previewId;
            request.m_confirmation.m_previewFingerprint =
                request.m_previewFingerprint;
            request.m_confirmation.m_decision =
                AdapterDeploymentConfirmationDecision::Confirmed;
            request.m_confirmation.m_scope =
                AdapterDeploymentConfirmationScope::FullPreview;
            request.m_confirmation.m_reviewer = "release-reviewer";
            request.m_confirmation.m_evidenceIds = { "evidence.confirmation" };
            request.m_confirmation.m_issuedAtUtc = "2026-07-19T12:00:00Z";
            request.m_confirmation.m_expiresAtUtc = "2026-07-19T14:00:00Z";

            request.m_maintenanceWindow.m_windowId = "owner.window";
            request.m_maintenanceWindow.m_previewId = request.m_preview.m_previewId;
            request.m_maintenanceWindow.m_previewFingerprint =
                request.m_previewFingerprint;
            request.m_maintenanceWindow.m_startAtUtc = "2026-07-19T12:30:00Z";
            request.m_maintenanceWindow.m_endAtUtc = "2026-07-19T13:30:00Z";
            request.m_maintenanceWindow.m_operatorGroup = "release-operators";
            request.m_maintenanceWindow.m_evidenceIds = { "evidence.window" };
            request.m_evaluatedAtUtc = "2026-07-19T13:00:00Z";

            const AdapterDeploymentPreflightKind kinds[] = {
                AdapterDeploymentPreflightKind::PackageIntegrity,
                AdapterDeploymentPreflightKind::TargetInventory,
                AdapterDeploymentPreflightKind::BackupReadiness,
                AdapterDeploymentPreflightKind::RollbackReadiness,
                AdapterDeploymentPreflightKind::OperatorReadiness,
            };
            int index = 0;
            for (AdapterDeploymentPreflightKind kind : kinds)
            {
                AdapterDeploymentPreflightEvidence evidence;
                evidence.m_preflightId =
                    AZStd::string("owner.preflight-") + char('a' + index);
                evidence.m_kind = kind;
                evidence.m_status = AdapterDeploymentPreflightStatus::Passed;
                evidence.m_previewId = request.m_preview.m_previewId;
                evidence.m_previewFingerprint = request.m_previewFingerprint;
                evidence.m_checkedAtUtc = "2026-07-19T12:45:00Z";
                evidence.m_checker = "preflight-checker";
                evidence.m_evidenceIds = {
                    AZStd::string("evidence.preflight-") + char('a' + index),
                };
                request.m_preflightEvidence.push_back(evidence);
                ++index;
            }
            return request;
        }

        size_t CountSteps(
            const AdapterDeploymentWorkOrder& workOrder,
            AdapterDeploymentWorkOrderStepKind kind)
        {
            return static_cast<size_t>(AZStd::count_if(
                workOrder.m_steps.begin(),
                workOrder.m_steps.end(),
                [kind](const AdapterDeploymentWorkOrderStep& step)
                {
                    return step.m_kind == kind;
                }));
        }
    } // namespace

    TEST(AdapterDeploymentWorkOrderTests, TypedConfirmationScopePreflightAndStatusVocabulariesAreStrict)
    {
        AdapterDeploymentConfirmationDecision decision =
            AdapterDeploymentConfirmationDecision::Unknown;
        EXPECT_TRUE(TryParseAdapterDeploymentConfirmationDecision("confirmed", decision));
        EXPECT_EQ(decision, AdapterDeploymentConfirmationDecision::Confirmed);
        EXPECT_FALSE(TryParseAdapterDeploymentConfirmationDecision("approved", decision));

        AdapterDeploymentConfirmationScope scope =
            AdapterDeploymentConfirmationScope::FullPreview;
        EXPECT_TRUE(TryParseAdapterDeploymentConfirmationScope("additions_only", scope));
        EXPECT_EQ(scope, AdapterDeploymentConfirmationScope::AdditionsOnly);

        AdapterDeploymentPreflightKind kind =
            AdapterDeploymentPreflightKind::PackageIntegrity;
        EXPECT_TRUE(TryParseAdapterDeploymentPreflightKind("rollback_readiness", kind));
        EXPECT_EQ(kind, AdapterDeploymentPreflightKind::RollbackReadiness);

        AdapterDeploymentWorkOrderStatus status =
            AdapterDeploymentWorkOrderStatus::PreviewNotReady;
        EXPECT_TRUE(TryParseAdapterDeploymentWorkOrderStatus("review_ready", status));
        EXPECT_EQ(status, AdapterDeploymentWorkOrderStatus::ReviewReady);
        EXPECT_FALSE(TryParseAdapterDeploymentWorkOrderStatus("execution_ready", status));
    }

    TEST(AdapterDeploymentWorkOrderTests, RegistryRejectsDuplicateConfirmationOrPreviewIdentity)
    {
        AdapterDeploymentWorkOrderRegistry& registry =
            AdapterDeploymentWorkOrderRegistry::Get();
        registry.Clear();
        const AdapterDeploymentWorkOrderRequest request = MakeReadyRequest();
        AZStd::string error;
        EXPECT_TRUE(registry.RegisterRequest(request, &error)) << error.c_str();
        EXPECT_FALSE(registry.RegisterRequest(request, &error));
        EXPECT_NE(error.find("already exists"), AZStd::string::npos);
        registry.Clear();
    }

    TEST(AdapterDeploymentWorkOrderTests, CompleteConfirmationProducesReviewReadyWorkOrderOnly)
    {
        AdapterDeploymentWorkOrderService service;
        const AdapterDeploymentWorkOrder workOrder =
            service.BuildWorkOrder(MakeReadyRequest());
        EXPECT_EQ(workOrder.m_status, AdapterDeploymentWorkOrderStatus::ReviewReady);
        EXPECT_TRUE(workOrder.m_blockers.empty());
        EXPECT_FALSE(workOrder.m_executionAllowed);
        EXPECT_FALSE(workOrder.m_copyAllowed);
        EXPECT_FALSE(workOrder.m_deleteAllowed);
        EXPECT_FALSE(workOrder.m_backupAllowed);
        EXPECT_FALSE(workOrder.m_restoreAllowed);
        EXPECT_FALSE(workOrder.m_deploymentAllowed);
        EXPECT_FALSE(workOrder.m_launchAllowed);
        EXPECT_NE(
            workOrder.m_canonicalJson.find("\"ExecutionAllowed\":false"),
            AZStd::string::npos);
        EXPECT_NE(
            workOrder.m_canonicalJson.find("\"AcknowledgementRecorded\":false"),
            AZStd::string::npos);
    }

    TEST(AdapterDeploymentWorkOrderTests, CallerSelectedPreviewFingerprintIsRejected)
    {
        AdapterDeploymentWorkOrderRequest request = MakeReadyRequest();
        request.m_previewFingerprint = Fingerprint('a');
        request.m_confirmation.m_previewFingerprint = request.m_previewFingerprint;
        request.m_maintenanceWindow.m_previewFingerprint = request.m_previewFingerprint;
        for (AdapterDeploymentPreflightEvidence& preflight : request.m_preflightEvidence)
        {
            preflight.m_previewFingerprint = request.m_previewFingerprint;
        }
        AdapterDeploymentWorkOrderService service;
        EXPECT_EQ(
            service.BuildWorkOrder(request).m_status,
            AdapterDeploymentWorkOrderStatus::PreviewNotReady);
    }

    TEST(AdapterDeploymentWorkOrderTests, PreviewNotReadyPrecedesConfirmationAndPreflightFailures)
    {
        AdapterDeploymentWorkOrderRequest request = MakeReadyRequest();
        request.m_preview.m_status = AdapterStagingDeploymentPreviewStatus::Conflict;
        request.m_confirmation.m_decision = AdapterDeploymentConfirmationDecision::Rejected;
        request.m_preflightEvidence.clear();
        AdapterDeploymentWorkOrderService service;
        EXPECT_EQ(
            service.BuildWorkOrder(request).m_status,
            AdapterDeploymentWorkOrderStatus::PreviewNotReady);
    }

    TEST(AdapterDeploymentWorkOrderTests, ConfirmationDecisionBindingAndScopeFailClosed)
    {
        AdapterDeploymentWorkOrderService service;

        AdapterDeploymentWorkOrderRequest rejected = MakeReadyRequest();
        rejected.m_confirmation.m_decision = AdapterDeploymentConfirmationDecision::Rejected;
        EXPECT_EQ(
            service.BuildWorkOrder(rejected).m_status,
            AdapterDeploymentWorkOrderStatus::ConfirmationRejected);

        AdapterDeploymentWorkOrderRequest drifted = MakeReadyRequest();
        drifted.m_confirmation.m_previewFingerprint = Fingerprint('f');
        EXPECT_EQ(
            service.BuildWorkOrder(drifted).m_status,
            AdapterDeploymentWorkOrderStatus::ConfirmationBindingMismatch);

        AdapterDeploymentWorkOrderRequest narrow = MakeReadyRequest();
        narrow.m_confirmation.m_scope = AdapterDeploymentConfirmationScope::AdditionsOnly;
        EXPECT_EQ(
            service.BuildWorkOrder(narrow).m_status,
            AdapterDeploymentWorkOrderStatus::ScopeMismatch);
    }

    TEST(AdapterDeploymentWorkOrderTests, ExpiryAndMaintenanceWindowFailClosed)
    {
        AdapterDeploymentWorkOrderService service;

        AdapterDeploymentWorkOrderRequest expired = MakeReadyRequest();
        expired.m_evaluatedAtUtc = "2026-07-19T14:30:00Z";
        EXPECT_EQ(
            service.BuildWorkOrder(expired).m_status,
            AdapterDeploymentWorkOrderStatus::ConfirmationExpired);

        AdapterDeploymentWorkOrderRequest impossibleDate = MakeReadyRequest();
        impossibleDate.m_confirmation.m_issuedAtUtc = "2026-02-31T12:00:00Z";
        EXPECT_EQ(
            service.BuildWorkOrder(impossibleDate).m_status,
            AdapterDeploymentWorkOrderStatus::ConfirmationExpired);

        AdapterDeploymentWorkOrderRequest invalidWindow = MakeReadyRequest();
        invalidWindow.m_maintenanceWindow.m_startAtUtc = "2026-07-19T13:30:00Z";
        invalidWindow.m_maintenanceWindow.m_endAtUtc = "2026-07-19T12:30:00Z";
        EXPECT_EQ(
            service.BuildWorkOrder(invalidWindow).m_status,
            AdapterDeploymentWorkOrderStatus::MaintenanceWindowInvalid);

        AdapterDeploymentWorkOrderRequest outside = MakeReadyRequest();
        outside.m_maintenanceWindow.m_endAtUtc = "2026-07-19T12:50:00Z";
        EXPECT_EQ(
            service.BuildWorkOrder(outside).m_status,
            AdapterDeploymentWorkOrderStatus::OutsideMaintenanceWindow);
    }

    TEST(AdapterDeploymentWorkOrderTests, MissingAndFailedPreflightRemainDistinct)
    {
        AdapterDeploymentWorkOrderService service;

        AdapterDeploymentWorkOrderRequest missing = MakeReadyRequest();
        missing.m_preflightEvidence.pop_back();
        EXPECT_EQ(
            service.BuildWorkOrder(missing).m_status,
            AdapterDeploymentWorkOrderStatus::PreflightMissing);

        AdapterDeploymentWorkOrderRequest failed = MakeReadyRequest();
        failed.m_preflightEvidence.front().m_status =
            AdapterDeploymentPreflightStatus::Failed;
        EXPECT_EQ(
            service.BuildWorkOrder(failed).m_status,
            AdapterDeploymentWorkOrderStatus::PreflightFailed);
    }

    TEST(AdapterDeploymentWorkOrderTests, WorkOrderStepsCoverChangesBackupsAndRollback)
    {
        AdapterDeploymentWorkOrderService service;
        const AdapterDeploymentWorkOrder workOrder =
            service.BuildWorkOrder(MakeReadyRequest());
        EXPECT_EQ(CountSteps(workOrder, AdapterDeploymentWorkOrderStepKind::Backup), 2);
        EXPECT_EQ(CountSteps(workOrder, AdapterDeploymentWorkOrderStepKind::Add), 1);
        EXPECT_EQ(CountSteps(workOrder, AdapterDeploymentWorkOrderStepKind::Replace), 1);
        EXPECT_EQ(CountSteps(workOrder, AdapterDeploymentWorkOrderStepKind::Remove), 1);
        EXPECT_EQ(workOrder.m_steps.size(), 9);
    }

    TEST(AdapterDeploymentWorkOrderTests, OperatorChecklistRemainsPendingAndNonExecutable)
    {
        AdapterDeploymentWorkOrderService service;
        const AdapterDeploymentWorkOrder workOrder =
            service.BuildWorkOrder(MakeReadyRequest());
        ASSERT_FALSE(workOrder.m_checklist.empty());
        size_t operatorActionCount = 0;
        for (const AdapterDeploymentOperatorChecklistItem& item : workOrder.m_checklist)
        {
            EXPECT_FALSE(item.m_acknowledgementRecorded);
            if (item.m_state == AdapterDeploymentChecklistState::OperatorActionRequired)
            {
                ++operatorActionCount;
            }
        }
        EXPECT_GT(operatorActionCount, 0);
        for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
        {
            EXPECT_FALSE(step.m_executionAllowed);
        }
    }

    TEST(AdapterDeploymentWorkOrderTests, SerializerExposesMutatedSafetyFlags)
    {
        AdapterDeploymentWorkOrderService service;
        AdapterDeploymentWorkOrder workOrder = service.BuildWorkOrder(MakeReadyRequest());
        ASSERT_FALSE(workOrder.m_steps.empty());
        ASSERT_FALSE(workOrder.m_checklist.empty());
        workOrder.m_executionAllowed = true;
        workOrder.m_steps.front().m_executionAllowed = true;
        workOrder.m_checklist.front().m_acknowledgementRecorded = true;
        const AZStd::string json = service.SerializeCanonicalWorkOrder(workOrder);
        EXPECT_NE(json.find("\"ExecutionAllowed\":true"), AZStd::string::npos);
        EXPECT_NE(json.find("\"AcknowledgementRecorded\":true"), AZStd::string::npos);
    }

    TEST(AdapterDeploymentWorkOrderTests, JsonEscapingDistinguishesControlBytes)
    {
        AdapterDeploymentWorkOrderService service;
        AdapterDeploymentWorkOrder first;
        AdapterDeploymentWorkOrder second;
        first.m_workOrderId = AZStd::string("owner.") + char(0x01);
        second.m_workOrderId = AZStd::string("owner.") + char(0x02);
        const AZStd::string firstJson = service.SerializeCanonicalWorkOrder(first);
        const AZStd::string secondJson = service.SerializeCanonicalWorkOrder(second);
        EXPECT_NE(firstJson, secondJson);
        EXPECT_NE(firstJson.find("\\u0001"), AZStd::string::npos);
        EXPECT_NE(secondJson.find("\\u0002"), AZStd::string::npos);
    }

    TEST(AdapterDeploymentWorkOrderTests, CanonicalWorkOrderIsDeterministic)
    {
        AdapterDeploymentWorkOrderRequest first = MakeReadyRequest();
        AdapterDeploymentWorkOrderRequest second = first;
        AZStd::reverse(second.m_preflightEvidence.begin(), second.m_preflightEvidence.end());
        AdapterDeploymentWorkOrderService service;
        EXPECT_EQ(
            service.BuildWorkOrder(first).m_canonicalJson,
            service.BuildWorkOrder(second).m_canonicalJson);
    }

    TEST(AdapterDeploymentWorkOrderTests, WorkOrderGenerationDoesNotMutateInputs)
    {
        AdapterDeploymentWorkOrderRequest request = MakeReadyRequest();
        const size_t additionCount = request.m_preview.m_additions.size();
        const size_t replacementCount = request.m_preview.m_replacements.size();
        const size_t removalCount = request.m_preview.m_removals.size();
        const size_t backupCount = request.m_preview.m_backups.size();
        const size_t preflightCount = request.m_preflightEvidence.size();
        const AZStd::string previewJson = request.m_preview.m_canonicalJson;
        const AZStd::string confirmationId = request.m_confirmation.m_confirmationId;

        AdapterDeploymentWorkOrderService service;
        const AdapterDeploymentWorkOrder workOrder = service.BuildWorkOrder(request);
        EXPECT_EQ(workOrder.m_status, AdapterDeploymentWorkOrderStatus::ReviewReady);
        EXPECT_EQ(request.m_preview.m_additions.size(), additionCount);
        EXPECT_EQ(request.m_preview.m_replacements.size(), replacementCount);
        EXPECT_EQ(request.m_preview.m_removals.size(), removalCount);
        EXPECT_EQ(request.m_preview.m_backups.size(), backupCount);
        EXPECT_EQ(request.m_preflightEvidence.size(), preflightCount);
        EXPECT_EQ(request.m_preview.m_canonicalJson, previewJson);
        EXPECT_EQ(request.m_confirmation.m_confirmationId, confirmationId);
    }
} // namespace TaintedGrailModdingSDK
