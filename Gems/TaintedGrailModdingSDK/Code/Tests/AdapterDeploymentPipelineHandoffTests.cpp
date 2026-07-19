/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentWorkOrderService.h"
#include "AdapterStagingDeploymentPreviewService.h"
#include "CanonicalFingerprint.h"

#include <AzTest/AzTest.h>

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string PipelineFingerprint(char digit)
        {
            return AZStd::string("sha256:") + AZStd::string(64, digit);
        }

        AdapterPackageLayoutEntry PipelinePackageEntry(
            AZStd::string id,
            AZStd::string path,
            AZStd::string role,
            AZStd::string mediaType,
            char fingerprintDigit)
        {
            AdapterPackageLayoutEntry entry;
            entry.m_inventoryEntryId = AZStd::move(id);
            entry.m_stagingPath = AZStd::string("Staging/") + path;
            entry.m_packagePath = AZStd::move(path);
            entry.m_role = AZStd::move(role);
            entry.m_mediaType = AZStd::move(mediaType);
            entry.m_outputDigest = PipelineFingerprint(fingerprintDigit);
            entry.m_byteSize = 1024;
            entry.m_projectOwned = true;
            entry.m_redistributable = true;
            return entry;
        }

        AdapterDeploymentTargetEntry PipelineTargetEntry(
            AZStd::string id,
            AZStd::string path,
            AZStd::string role,
            AZStd::string mediaType,
            char fingerprintDigit)
        {
            AdapterDeploymentTargetEntry entry;
            entry.m_entryId = AZStd::move(id);
            entry.m_targetPath = AZStd::move(path);
            entry.m_role = AZStd::move(role);
            entry.m_mediaType = AZStd::move(mediaType);
            entry.m_fingerprint = PipelineFingerprint(fingerprintDigit);
            entry.m_byteSize = 900;
            entry.m_ownerPackId = "owner.preview-pack";
            entry.m_projectOwned = true;
            entry.m_managed = true;
            entry.m_replaceable = true;
            entry.m_removable = true;
            return entry;
        }

        AdapterStagingDeploymentPreviewRequest MakePipelinePreviewRequest()
        {
            AdapterStagingDeploymentPreviewRequest request;
            request.m_packagePreview.m_previewId =
                "packagepreview:owner.preview-pack:owner.staging-inventory";
            request.m_packagePreview.m_manifestId =
                "buildmanifest:owner.preview-pack:owner.foa-adapter";
            request.m_packagePreview.m_manifestFingerprint = PipelineFingerprint('9');
            request.m_packagePreview.m_packId = "owner.preview-pack";
            request.m_packagePreview.m_packVersion = "1.2.3";
            request.m_packagePreview.m_adapterId = "owner.foa-adapter";
            request.m_packagePreview.m_adapterVersion = "0.4.1";
            request.m_packagePreview.m_planId =
                "workorder.plan:owner.preview-pack:owner.foa-adapter";
            request.m_packagePreview.m_inventoryId = "owner.staging-inventory";
            request.m_packagePreview.m_packageRoot =
                "BepInEx/plugins/owner.preview-pack";
            request.m_packagePreview.m_status =
                AdapterPackageAssemblyPreviewStatus::Ready;
            request.m_packagePreview.m_canonicalJson =
                "{\"AssemblyAllowed\":false,\"ArchiveAllowed\":false,"
                "\"DeploymentAllowed\":false}";
            request.m_packagePreview.m_assemblyAllowed = false;
            request.m_packagePreview.m_archiveAllowed = false;
            request.m_packagePreview.m_deploymentAllowed = false;

            const AZStd::string root = request.m_packagePreview.m_packageRoot + "/";
            request.m_packagePreview.m_layout = {
                PipelinePackageEntry(
                    "owner.package.plugin",
                    root + "owner.preview-pack.dll",
                    "plugin_binary",
                    "application/vnd.microsoft.portable-executable",
                    '1'),
                PipelinePackageEntry(
                    "owner.package.readme",
                    root + "README.md",
                    "readme",
                    "text/markdown",
                    '2'),
            };
            request.m_packagePreviewFingerprint =
                CalculateCanonicalSha256(request.m_packagePreview.m_canonicalJson);

            request.m_targetInventory.m_inventoryId = "owner.target-inventory";
            request.m_targetInventory.m_inventoryFingerprint = PipelineFingerprint('b');
            request.m_targetInventory.m_packagePreviewId =
                request.m_packagePreview.m_previewId;
            request.m_targetInventory.m_packagePreviewFingerprint =
                request.m_packagePreviewFingerprint;
            request.m_targetInventory.m_packId = request.m_packagePreview.m_packId;
            request.m_targetInventory.m_targetRoot =
                request.m_packagePreview.m_packageRoot;
            request.m_targetInventory.m_backupRoot =
                "Backups/deployment.owner-preview";
            request.m_targetInventory.m_entries = {
                PipelineTargetEntry(
                    "owner.target.plugin",
                    root + "owner.preview-pack.dll",
                    "plugin_binary",
                    "application/vnd.microsoft.portable-executable",
                    '8'),
                PipelineTargetEntry(
                    "owner.target.readme",
                    root + "README.md",
                    "readme",
                    "text/markdown",
                    '2'),
            };

            request.m_targetReview.m_reviewId = "owner.target-review";
            request.m_targetReview.m_inventoryId =
                request.m_targetInventory.m_inventoryId;
            request.m_targetReview.m_inventoryFingerprint =
                request.m_targetInventory.m_inventoryFingerprint;
            request.m_targetReview.m_decision =
                AdapterDeploymentTargetReviewDecision::Accepted;
            request.m_targetReview.m_reviewer = "deployment-reviewer";
            request.m_targetReview.m_evidenceIds = { "evidence.target.review" };
            return request;
        }

        AdapterDeploymentWorkOrderRequest MakePipelineWorkOrderRequest(
            const AdapterStagingDeploymentPreview& preview)
        {
            AdapterDeploymentWorkOrderRequest request;
            request.m_preview = preview;
            request.m_previewFingerprint =
                CalculateCanonicalSha256(preview.m_canonicalJson);

            request.m_confirmation.m_confirmationId = "owner.confirmation";
            request.m_confirmation.m_previewId = preview.m_previewId;
            request.m_confirmation.m_previewFingerprint = request.m_previewFingerprint;
            request.m_confirmation.m_decision =
                AdapterDeploymentConfirmationDecision::Confirmed;
            request.m_confirmation.m_scope =
                AdapterDeploymentConfirmationScope::FullPreview;
            request.m_confirmation.m_reviewer = "release-reviewer";
            request.m_confirmation.m_evidenceIds = { "evidence.confirmation" };
            request.m_confirmation.m_issuedAtUtc = "2026-07-19T12:00:00Z";
            request.m_confirmation.m_expiresAtUtc = "2026-07-19T14:00:00Z";

            request.m_maintenanceWindow.m_windowId = "owner.window";
            request.m_maintenanceWindow.m_previewId = preview.m_previewId;
            request.m_maintenanceWindow.m_previewFingerprint = request.m_previewFingerprint;
            request.m_maintenanceWindow.m_startAtUtc = "2026-07-19T12:30:00Z";
            request.m_maintenanceWindow.m_endAtUtc = "2026-07-19T13:30:00Z";
            request.m_maintenanceWindow.m_operatorGroup = "release-operators";
            request.m_maintenanceWindow.m_evidenceIds = { "evidence.window" };
            request.m_evaluatedAtUtc = "2026-07-19T13:00:00Z";

            AZStd::vector<AdapterDeploymentPreflightKind> kinds = {
                AdapterDeploymentPreflightKind::PackageIntegrity,
                AdapterDeploymentPreflightKind::TargetInventory,
                AdapterDeploymentPreflightKind::RollbackReadiness,
                AdapterDeploymentPreflightKind::OperatorReadiness,
            };
            if (!preview.m_backups.empty())
            {
                kinds.push_back(AdapterDeploymentPreflightKind::BackupReadiness);
            }

            for (size_t index = 0; index < kinds.size(); ++index)
            {
                AdapterDeploymentPreflightEvidence evidence;
                evidence.m_preflightId = AZStd::string::format(
                    "owner.preflight-%llu",
                    static_cast<unsigned long long>(index + 1));
                evidence.m_kind = kinds[index];
                evidence.m_status = AdapterDeploymentPreflightStatus::Passed;
                evidence.m_previewId = preview.m_previewId;
                evidence.m_previewFingerprint = request.m_previewFingerprint;
                evidence.m_checkedAtUtc = "2026-07-19T12:45:00Z";
                evidence.m_checker = "preflight-checker";
                evidence.m_evidenceIds = {
                    AZStd::string::format(
                        "evidence.preflight-%llu",
                        static_cast<unsigned long long>(index + 1)),
                };
                request.m_preflightEvidence.push_back(AZStd::move(evidence));
            }
            return request;
        }
    } // namespace

    TEST(
        AdapterDeploymentPipelineHandoffTests,
        RealReadyPreviewCanProduceReviewReadyWorkOrder)
    {
        AdapterStagingDeploymentPreviewService previewService;
        const AdapterStagingDeploymentPreview preview =
            previewService.BuildPreview(MakePipelinePreviewRequest());

        ASSERT_EQ(preview.m_status, AdapterStagingDeploymentPreviewStatus::Ready);
        EXPECT_TRUE(preview.m_blockers.empty());
        EXPECT_FALSE(preview.m_stagingMutationAllowed);
        EXPECT_FALSE(preview.m_deploymentMutationAllowed);
        EXPECT_FALSE(preview.m_rollbackExecutionAllowed);
        EXPECT_FALSE(preview.m_launchAllowed);

        AdapterDeploymentWorkOrderService workOrderService;
        const AdapterDeploymentWorkOrder workOrder =
            workOrderService.BuildWorkOrder(MakePipelineWorkOrderRequest(preview));

        EXPECT_EQ(workOrder.m_status, AdapterDeploymentWorkOrderStatus::ReviewReady);
        EXPECT_TRUE(workOrder.m_blockers.empty());
        EXPECT_FALSE(workOrder.m_steps.empty());
        EXPECT_FALSE(workOrder.m_executionAllowed);
        EXPECT_FALSE(workOrder.m_deploymentAllowed);
        EXPECT_FALSE(workOrder.m_launchAllowed);
    }
} // namespace TaintedGrailModdingSDK
