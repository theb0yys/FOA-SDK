/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterStagingDeploymentPreviewService.h"

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

        AdapterPackageLayoutEntry PackageEntry(
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
            entry.m_outputDigest = Fingerprint(fingerprintDigit);
            entry.m_byteSize = 1024;
            entry.m_projectOwned = true;
            entry.m_redistributable = true;
            return entry;
        }

        AdapterDeploymentTargetEntry TargetEntry(
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
            entry.m_fingerprint = Fingerprint(fingerprintDigit);
            entry.m_byteSize = 900;
            entry.m_ownerPackId = "owner.preview-pack";
            entry.m_projectOwned = true;
            entry.m_managed = true;
            entry.m_replaceable = true;
            entry.m_removable = true;
            return entry;
        }

        AdapterStagingDeploymentPreviewRequest MakeReadyRequest()
        {
            AdapterStagingDeploymentPreviewRequest request;
            request.m_packagePreview.m_previewId =
                "packagepreview:owner.preview-pack:owner.staging-inventory";
            request.m_packagePreview.m_manifestId =
                "buildmanifest:owner.preview-pack:owner.foa-adapter";
            request.m_packagePreview.m_manifestFingerprint = Fingerprint('9');
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
                "{\"AssemblyAllowed\":false,\"ArchiveAllowed\":false,\"DeploymentAllowed\":false}";
            request.m_packagePreview.m_assemblyAllowed = false;
            request.m_packagePreview.m_archiveAllowed = false;
            request.m_packagePreview.m_deploymentAllowed = false;
            const AZStd::string root = request.m_packagePreview.m_packageRoot + "/";
            request.m_packagePreview.m_layout = {
                PackageEntry(
                    "owner.package.plugin",
                    root + "owner.preview-pack.dll",
                    "plugin_binary",
                    "application/vnd.microsoft.portable-executable",
                    '1'),
                PackageEntry(
                    "owner.package.readme",
                    root + "README.md",
                    "readme",
                    "text/markdown",
                    '2'),
                PackageEntry(
                    "owner.package.changelog",
                    root + "CHANGELOG.md",
                    "changelog",
                    "text/markdown",
                    '3'),
                PackageEntry(
                    "owner.package.manifest",
                    root + "MANIFEST.md",
                    "manifest",
                    "text/markdown",
                    '4'),
                PackageEntry(
                    "owner.package.license",
                    root + "LICENSE",
                    "license",
                    "text/plain",
                    '5'),
            };
            request.m_packagePreviewFingerprint = Fingerprint('a');

            request.m_targetInventory.m_inventoryId = "owner.target-inventory";
            request.m_targetInventory.m_inventoryFingerprint = Fingerprint('b');
            request.m_targetInventory.m_packagePreviewId =
                request.m_packagePreview.m_previewId;
            request.m_targetInventory.m_packagePreviewFingerprint =
                request.m_packagePreviewFingerprint;
            request.m_targetInventory.m_packId = request.m_packagePreview.m_packId;
            request.m_targetInventory.m_targetRoot = request.m_packagePreview.m_packageRoot;
            request.m_targetInventory.m_backupRoot =
                "Backups/deployment.owner-preview";
            request.m_targetInventory.m_entries = {
                TargetEntry(
                    "owner.target.plugin",
                    root + "owner.preview-pack.dll",
                    "plugin_binary",
                    "application/vnd.microsoft.portable-executable",
                    '8'),
                TargetEntry(
                    "owner.target.readme",
                    root + "README.md",
                    "readme",
                    "text/markdown",
                    '2'),
                TargetEntry(
                    "owner.target.obsolete",
                    root + "obsolete.json",
                    "config",
                    "application/json",
                    'f'),
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
    } // namespace

    TEST(AdapterStagingDeploymentPreviewTests, TypedStatusChangeAndRollbackVocabulariesAreStrict)
    {
        AdapterDeploymentTargetReviewDecision decision =
            AdapterDeploymentTargetReviewDecision::Unknown;
        EXPECT_TRUE(TryParseAdapterDeploymentTargetReviewDecision("accepted", decision));
        EXPECT_EQ(decision, AdapterDeploymentTargetReviewDecision::Accepted);
        EXPECT_FALSE(TryParseAdapterDeploymentTargetReviewDecision("approved", decision));

        AdapterDeploymentChangeKind kind = AdapterDeploymentChangeKind::Unchanged;
        EXPECT_TRUE(TryParseAdapterDeploymentChangeKind("replace", kind));
        EXPECT_EQ(kind, AdapterDeploymentChangeKind::Replace);
        EXPECT_FALSE(TryParseAdapterDeploymentChangeKind("overwrite", kind));

        AdapterDeploymentRollbackAction action =
            AdapterDeploymentRollbackAction::RemoveAdded;
        EXPECT_TRUE(TryParseAdapterDeploymentRollbackAction("restore_removed", action));
        EXPECT_EQ(action, AdapterDeploymentRollbackAction::RestoreRemoved);
        EXPECT_FALSE(TryParseAdapterDeploymentRollbackAction("undo", action));

        AdapterStagingDeploymentPreviewStatus status =
            AdapterStagingDeploymentPreviewStatus::PackageNotReady;
        EXPECT_TRUE(TryParseAdapterStagingDeploymentPreviewStatus("backup_incomplete", status));
        EXPECT_EQ(status, AdapterStagingDeploymentPreviewStatus::BackupIncomplete);
        EXPECT_FALSE(TryParseAdapterStagingDeploymentPreviewStatus("deployed", status));
    }

    TEST(AdapterStagingDeploymentPreviewTests, RegistryRejectsDuplicateTargetInventoryIdentity)
    {
        AdapterStagingDeploymentPreviewRegistry& registry =
            AdapterStagingDeploymentPreviewRegistry::Get();
        registry.Clear();
        const AdapterStagingDeploymentPreviewRequest request = MakeReadyRequest();
        AZStd::string error;
        EXPECT_TRUE(registry.RegisterRequest(request, &error)) << error.c_str();
        EXPECT_FALSE(registry.RegisterRequest(request, &error));
        EXPECT_NE(error.find("already exists"), AZStd::string::npos);
        registry.Clear();
    }

    TEST(AdapterStagingDeploymentPreviewTests, ReadyPreviewDerivesAllChangeKindsAndProhibitsMutation)
    {
        AdapterStagingDeploymentPreviewService service;
        const AdapterStagingDeploymentPreview preview =
            service.BuildPreview(MakeReadyRequest());
        EXPECT_EQ(preview.m_status, AdapterStagingDeploymentPreviewStatus::Ready);
        EXPECT_EQ(preview.m_additions.size(), 3);
        EXPECT_EQ(preview.m_replacements.size(), 1);
        EXPECT_EQ(preview.m_removals.size(), 1);
        EXPECT_EQ(preview.m_unchanged.size(), 1);
        EXPECT_TRUE(preview.m_conflicts.empty());
        EXPECT_EQ(preview.m_backups.size(), 2);
        EXPECT_EQ(preview.m_rollbackSteps.size(), 5);
        EXPECT_FALSE(preview.m_stagingMutationAllowed);
        EXPECT_FALSE(preview.m_deploymentMutationAllowed);
        EXPECT_FALSE(preview.m_rollbackExecutionAllowed);
        EXPECT_FALSE(preview.m_launchAllowed);
        EXPECT_NE(
            preview.m_canonicalJson.find("\"DeploymentMutationAllowed\":false"),
            AZStd::string::npos);
    }

    TEST(AdapterStagingDeploymentPreviewTests, PackageNotReadyPrecedesTargetAndInventoryFailures)
    {
        AdapterStagingDeploymentPreviewRequest request = MakeReadyRequest();
        request.m_packagePreview.m_status =
            AdapterPackageAssemblyPreviewStatus::Collision;
        request.m_targetReview.m_decision =
            AdapterDeploymentTargetReviewDecision::Rejected;
        request.m_targetInventory.m_entries.clear();
        AdapterStagingDeploymentPreviewService service;
        EXPECT_EQ(
            service.BuildPreview(request).m_status,
            AdapterStagingDeploymentPreviewStatus::PackageNotReady);
    }

    TEST(AdapterStagingDeploymentPreviewTests, ReviewAndInventoryBindingsFailClosed)
    {
        AdapterStagingDeploymentPreviewService service;
        AdapterStagingDeploymentPreviewRequest rejected = MakeReadyRequest();
        rejected.m_targetReview.m_decision =
            AdapterDeploymentTargetReviewDecision::Rejected;
        EXPECT_EQ(
            service.BuildPreview(rejected).m_status,
            AdapterStagingDeploymentPreviewStatus::TargetUnreviewed);

        AdapterStagingDeploymentPreviewRequest drifted = MakeReadyRequest();
        drifted.m_targetInventory.m_packagePreviewFingerprint = Fingerprint('e');
        EXPECT_EQ(
            service.BuildPreview(drifted).m_status,
            AdapterStagingDeploymentPreviewStatus::InventoryBindingMismatch);
    }

    TEST(AdapterStagingDeploymentPreviewTests, ForeignOwnershipAndDuplicateTargetsProduceConflicts)
    {
        AdapterStagingDeploymentPreviewService service;
        AdapterStagingDeploymentPreviewRequest foreign = MakeReadyRequest();
        foreign.m_targetInventory.m_entries.front().m_ownerPackId = "other.pack";
        const AdapterStagingDeploymentPreview foreignPreview =
            service.BuildPreview(foreign);
        EXPECT_EQ(
            foreignPreview.m_status,
            AdapterStagingDeploymentPreviewStatus::Conflict);
        EXPECT_FALSE(foreignPreview.m_conflicts.empty());

        AdapterStagingDeploymentPreviewRequest duplicate = MakeReadyRequest();
        AdapterDeploymentTargetEntry copy = duplicate.m_targetInventory.m_entries.front();
        copy.m_entryId = "owner.target.plugin-copy";
        duplicate.m_targetInventory.m_entries.push_back(copy);
        const AdapterStagingDeploymentPreview duplicatePreview =
            service.BuildPreview(duplicate);
        EXPECT_EQ(
            duplicatePreview.m_status,
            AdapterStagingDeploymentPreviewStatus::Conflict);
        EXPECT_FALSE(duplicatePreview.m_conflicts.empty());
    }

    TEST(AdapterStagingDeploymentPreviewTests, MissingCurrentDigestMakesBackupAndRollbackIncomplete)
    {
        AdapterStagingDeploymentPreviewRequest request = MakeReadyRequest();
        request.m_targetInventory.m_entries.front().m_fingerprint.clear();
        AdapterStagingDeploymentPreviewService service;
        const AdapterStagingDeploymentPreview preview = service.BuildPreview(request);
        EXPECT_EQ(
            preview.m_status,
            AdapterStagingDeploymentPreviewStatus::BackupIncomplete);
        EXPECT_EQ(preview.m_replacements.size(), 1);
        EXPECT_EQ(preview.m_backups.size(), 1);
        EXPECT_EQ(preview.m_rollbackSteps.size(), 4);
    }

    TEST(AdapterStagingDeploymentPreviewTests, UnsafeTargetOrBackupPathIsRejected)
    {
        AdapterStagingDeploymentPreviewService service;
        AdapterStagingDeploymentPreviewRequest unsafe = MakeReadyRequest();
        unsafe.m_targetInventory.m_entries.front().m_targetPath = "../escape.dll";
        EXPECT_EQ(
            service.BuildPreview(unsafe).m_status,
            AdapterStagingDeploymentPreviewStatus::PathInvalid);

        AdapterStagingDeploymentPreviewRequest overlapping = MakeReadyRequest();
        overlapping.m_targetInventory.m_backupRoot =
            overlapping.m_targetInventory.m_targetRoot + "/Backups";
        EXPECT_EQ(
            service.BuildPreview(overlapping).m_status,
            AdapterStagingDeploymentPreviewStatus::PathInvalid);
    }

    TEST(AdapterStagingDeploymentPreviewTests, RollbackStepsAreExactDeterministicInverses)
    {
        AdapterStagingDeploymentPreviewService service;
        const AdapterStagingDeploymentPreview preview =
            service.BuildPreview(MakeReadyRequest());
        ASSERT_EQ(preview.m_rollbackSteps.size(), 5);
        EXPECT_EQ(
            preview.m_rollbackSteps.front().m_action,
            AdapterDeploymentRollbackAction::RemoveAdded);
        EXPECT_EQ(
            preview.m_rollbackSteps[3].m_action,
            AdapterDeploymentRollbackAction::RestoreReplaced);
        EXPECT_EQ(
            preview.m_rollbackSteps[4].m_action,
            AdapterDeploymentRollbackAction::RestoreRemoved);
        EXPECT_FALSE(preview.m_rollbackSteps[3].m_backupPath.empty());
        EXPECT_FALSE(preview.m_rollbackSteps[4].m_restoreFingerprint.empty());
        for (size_t index = 0; index < preview.m_rollbackSteps.size(); ++index)
        {
            EXPECT_EQ(preview.m_rollbackSteps[index].m_sequence, index + 1);
        }
    }

    TEST(AdapterStagingDeploymentPreviewTests, CanonicalChangesBackupsAndRollbackAreDeterministic)
    {
        AdapterStagingDeploymentPreviewRequest first = MakeReadyRequest();
        AdapterStagingDeploymentPreviewRequest second = first;
        AZStd::reverse(
            second.m_packagePreview.m_layout.begin(),
            second.m_packagePreview.m_layout.end());
        AZStd::reverse(
            second.m_targetInventory.m_entries.begin(),
            second.m_targetInventory.m_entries.end());
        AZStd::reverse(
            second.m_targetReview.m_evidenceIds.begin(),
            second.m_targetReview.m_evidenceIds.end());

        AdapterStagingDeploymentPreviewService service;
        const AdapterStagingDeploymentPreview firstPreview = service.BuildPreview(first);
        const AdapterStagingDeploymentPreview secondPreview = service.BuildPreview(second);
        EXPECT_EQ(firstPreview.m_status, AdapterStagingDeploymentPreviewStatus::Ready);
        EXPECT_EQ(firstPreview.m_canonicalJson, secondPreview.m_canonicalJson);
        EXPECT_EQ(
            firstPreview.m_rollbackSteps.front().m_stepId,
            secondPreview.m_rollbackSteps.front().m_stepId);
    }

    TEST(AdapterStagingDeploymentPreviewTests, PreviewGenerationDoesNotMutateInputs)
    {
        AdapterStagingDeploymentPreviewRequest request = MakeReadyRequest();
        const size_t packageEntryCount = request.m_packagePreview.m_layout.size();
        const size_t targetEntryCount = request.m_targetInventory.m_entries.size();
        const AZStd::string firstPackagePath =
            request.m_packagePreview.m_layout.front().m_packagePath;
        const AZStd::string firstTargetId =
            request.m_targetInventory.m_entries.front().m_entryId;
        const AZStd::string packageJson = request.m_packagePreview.m_canonicalJson;

        AdapterStagingDeploymentPreviewService service;
        const AdapterStagingDeploymentPreview preview = service.BuildPreview(request);
        EXPECT_EQ(preview.m_status, AdapterStagingDeploymentPreviewStatus::Ready);
        EXPECT_EQ(request.m_packagePreview.m_layout.size(), packageEntryCount);
        EXPECT_EQ(request.m_targetInventory.m_entries.size(), targetEntryCount);
        EXPECT_EQ(request.m_packagePreview.m_layout.front().m_packagePath, firstPackagePath);
        EXPECT_EQ(request.m_targetInventory.m_entries.front().m_entryId, firstTargetId);
        EXPECT_EQ(request.m_packagePreview.m_canonicalJson, packageJson);
    }
} // namespace TaintedGrailModdingSDK
