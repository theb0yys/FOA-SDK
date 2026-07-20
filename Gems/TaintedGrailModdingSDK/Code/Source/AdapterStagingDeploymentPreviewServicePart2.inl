/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool AddBackupRequirement(
            AdapterStagingDeploymentPreview& preview,
            const AdapterDeploymentTargetEntry& targetEntry,
            const AZStd::string& backupRoot,
            const AZStd::string& reason)
        {
            if (!IsSha256Fingerprint(targetEntry.m_fingerprint))
            {
                AddBlocker(
                    preview.m_blockers,
                    "deployment.backup_fingerprint_missing",
                    targetEntry.m_targetPath,
                    "A replacement or removal requires the exact current target fingerprint before a backup can be described.");
                return false;
            }

            const AZStd::string backupPath = MakeBackupPath(
                backupRoot,
                targetEntry.m_targetPath);
            if (!IsSafeRelativePath(backupPath)
                || !PathIsInsideRoot(backupRoot, backupPath))
            {
                AddBlocker(
                    preview.m_blockers,
                    "deployment.backup_path_invalid",
                    targetEntry.m_targetPath,
                    "The deterministic backup path is not safe or contained beneath BackupRoot.");
                return false;
            }

            AdapterDeploymentBackupRequirement backup;
            backup.m_backupId = AZStd::string("deployment.backup:")
                + targetEntry.m_entryId;
            backup.m_targetEntryId = targetEntry.m_entryId;
            backup.m_targetPath = targetEntry.m_targetPath;
            backup.m_backupPath = backupPath;
            backup.m_fingerprint = targetEntry.m_fingerprint;
            backup.m_byteSize = targetEntry.m_byteSize;
            backup.m_reason = reason;
            preview.m_backups.push_back(AZStd::move(backup));
            return true;
        }

        const AdapterDeploymentBackupRequirement* FindBackupRequirement(
            const AdapterStagingDeploymentPreview& preview,
            const AZStd::string& targetEntryId)
        {
            for (const AdapterDeploymentBackupRequirement& backup : preview.m_backups)
            {
                if (backup.m_targetEntryId == targetEntryId)
                {
                    return &backup;
                }
            }
            return nullptr;
        }

        void AddRollbackStep(
            AdapterStagingDeploymentPreview& preview,
            AZ::u64 sequence,
            AdapterDeploymentRollbackAction action,
            const AdapterDeploymentChange& change,
            const AdapterDeploymentBackupRequirement* backup,
            const AZStd::string& reason)
        {
            AdapterDeploymentRollbackStep step;
            step.m_sequence = sequence;
            step.m_stepId = AZStd::string::format(
                "deployment.rollback:%llu:%s:%s",
                static_cast<unsigned long long>(sequence),
                ToString(action).c_str(),
                change.m_targetPath.c_str());
            step.m_action = action;
            step.m_targetPath = change.m_targetPath;
            step.m_expectedDeployedFingerprint = change.m_desiredFingerprint;
            step.m_restoreFingerprint = change.m_previousFingerprint;
            step.m_reason = reason;
            if (backup)
            {
                step.m_backupPath = backup->m_backupPath;
            }
            preview.m_rollbackSteps.push_back(AZStd::move(step));
        }

        void BuildRollbackPlan(AdapterStagingDeploymentPreview& preview)
        {
            AZ::u64 sequence = 1;
            for (auto iterator = preview.m_additions.rbegin();
                iterator != preview.m_additions.rend(); ++iterator)
            {
                AddRollbackStep(
                    preview,
                    sequence++,
                    AdapterDeploymentRollbackAction::RemoveAdded,
                    *iterator,
                    nullptr,
                    "Remove the newly added project-owned target entry.");
            }
            for (auto iterator = preview.m_replacements.rbegin();
                iterator != preview.m_replacements.rend(); ++iterator)
            {
                const AdapterDeploymentBackupRequirement* backup =
                    FindBackupRequirement(preview, iterator->m_targetEntryId);
                if (backup)
                {
                    AddRollbackStep(
                        preview,
                        sequence++,
                        AdapterDeploymentRollbackAction::RestoreReplaced,
                        *iterator,
                        backup,
                        "Restore the exact pre-replacement bytes from the declared backup.");
                }
            }
            for (auto iterator = preview.m_removals.rbegin();
                iterator != preview.m_removals.rend(); ++iterator)
            {
                const AdapterDeploymentBackupRequirement* backup =
                    FindBackupRequirement(preview, iterator->m_targetEntryId);
                if (backup)
                {
                    AddRollbackStep(
                        preview,
                        sequence++,
                        AdapterDeploymentRollbackAction::RestoreRemoved,
                        *iterator,
                        backup,
                        "Restore the exact removed project-owned bytes from the declared backup.");
                }
            }
        }
    } // namespace

    AdapterStagingDeploymentPreview AdapterStagingDeploymentPreviewService::BuildPreview(
        const AdapterStagingDeploymentPreviewRequest& request) const
    {
        AdapterStagingDeploymentPreview preview;
        preview.m_packagePreviewId = request.m_packagePreview.m_previewId;
        preview.m_packagePreviewFingerprint = request.m_packagePreviewFingerprint;
        preview.m_targetInventoryId = request.m_targetInventory.m_inventoryId;
        preview.m_targetInventoryFingerprint =
            request.m_targetInventory.m_inventoryFingerprint;
        preview.m_packId = request.m_packagePreview.m_packId;
        preview.m_packageRoot = request.m_packagePreview.m_packageRoot;
        preview.m_targetRoot = request.m_targetInventory.m_targetRoot;
        preview.m_backupRoot = request.m_targetInventory.m_backupRoot;
        preview.m_stagingMutationAllowed = false;
        preview.m_deploymentMutationAllowed = false;
        preview.m_rollbackExecutionAllowed = false;
        preview.m_launchAllowed = false;
        preview.m_previewId = AZStd::string::format(
            "deploymentpreview:%s:%s:%s",
            preview.m_packId.c_str(),
            preview.m_packagePreviewId.c_str(),
            preview.m_targetInventoryId.c_str());

        bool packageNotReady = false;
        bool targetUnreviewed = false;
        bool inventoryBindingMismatch = false;
        bool inventoryUntrusted = false;
        bool pathInvalid = false;
        bool backupIncomplete = false;
        bool rollbackIncomplete = false;

        AdapterPackageAssemblyPreviewService packagePreviewService;
        const AZStd::string derivedPackageCanonicalJson =
            packagePreviewService.SerializeCanonicalPreview(request.m_packagePreview);

        if (request.m_packagePreview.m_status
                != AdapterPackageAssemblyPreviewStatus::Ready
            || request.m_packagePreview.m_layout.empty()
            || request.m_packagePreview.m_assemblyAllowed
            || request.m_packagePreview.m_archiveAllowed
            || request.m_packagePreview.m_deploymentAllowed
            || request.m_packagePreview.m_canonicalJson.empty()
            || request.m_packagePreview.m_canonicalJson
                != derivedPackageCanonicalJson
            || !IsSha256Fingerprint(request.m_packagePreviewFingerprint)
            || !CanonicalSha256Matches(
                request.m_packagePreview.m_canonicalJson,
                request.m_packagePreviewFingerprint))
        {
            packageNotReady = true;
            AddBlocker(
                preview.m_blockers,
                "deployment.package_not_ready",
                preview.m_packageRoot,
                "A ready canonical package preview whose SHA-256 fingerprint is derived from its exact canonical JSON, with all mutation permissions false, is required.");
        }
        for (const AdapterPackageLayoutEntry& entry : request.m_packagePreview.m_layout)
        {
            if (!IsSha256Fingerprint(entry.m_outputDigest))
            {
                packageNotReady = true;
                AddBlocker(
                    preview.m_blockers,
                    "deployment.package_digest_missing",
                    entry.m_packagePath,
                    "Every ready package-layout entry requires an exact output digest.");
            }
        }

        const AdapterDeploymentTargetReview& review = request.m_targetReview;
        if (!IsStableNamespacedId(review.m_reviewId)
            || review.m_decision != AdapterDeploymentTargetReviewDecision::Accepted
            || review.m_inventoryId != request.m_targetInventory.m_inventoryId
            || review.m_inventoryFingerprint
                != request.m_targetInventory.m_inventoryFingerprint
            || !IsSha256Fingerprint(review.m_inventoryFingerprint)
            || review.m_reviewer.empty()
            || review.m_evidenceIds.empty())
        {
            targetUnreviewed = true;
            AddBlocker(
                preview.m_blockers,
                "deployment.target_unreviewed",
                preview.m_targetRoot,
                "The target inventory requires an accepted evidence-backed review bound to its exact identity and fingerprint.");
        }

        const AdapterDeploymentTargetInventory& inventory = request.m_targetInventory;
        if (inventory.m_formatVersion != 1
            || !IsStableNamespacedId(inventory.m_inventoryId)
            || !IsSha256Fingerprint(inventory.m_inventoryFingerprint)
            || inventory.m_packagePreviewId != request.m_packagePreview.m_previewId
            || inventory.m_packagePreviewFingerprint
                != request.m_packagePreviewFingerprint
            || inventory.m_packId != request.m_packagePreview.m_packId
            || inventory.m_targetRoot != request.m_packagePreview.m_packageRoot)
        {
            inventoryBindingMismatch = true;
            AddBlocker(
                preview.m_blockers,
                "deployment.inventory_binding_mismatch",
                inventory.m_targetRoot,
                "The target inventory must bind to the exact package preview, derived fingerprint, pack, and package root.");
        }

        if (!IsSafeRelativePath(inventory.m_targetRoot)
            || !IsSafeRelativePath(inventory.m_backupRoot)
            || inventory.m_targetRoot == inventory.m_backupRoot
            || PathIsInsideRoot(inventory.m_targetRoot, inventory.m_backupRoot)
            || PathIsInsideRoot(inventory.m_backupRoot, inventory.m_targetRoot))
        {
            pathInvalid = true;
            AddBlocker(
                preview.m_blockers,
                "deployment.root_path_invalid",
                inventory.m_targetRoot,
                "TargetRoot and BackupRoot must be distinct non-overlapping safe relative roots.");
        }

        AZStd::vector<AZStd::string> inventoryEntryIds;
        AZStd::vector<AZStd::string> inventoryTargetPaths;
        for (const AdapterDeploymentTargetEntry& entry : inventory.m_entries)
        {
            if (!IsStableNamespacedId(entry.m_entryId)
                || entry.m_targetPath.empty()
                || entry.m_role.empty()
                || entry.m_mediaType.empty()
                || !IsStableNamespacedId(entry.m_ownerPackId))
            {
                inventoryUntrusted = true;
                AddBlocker(
                    preview.m_blockers,
                    "deployment.inventory_entry_invalid",
                    entry.m_targetPath,
                    "Every target entry requires stable identities, a path, role, media type, and owner pack.");
            }
            inventoryEntryIds.push_back(entry.m_entryId);
            inventoryTargetPaths.push_back(entry.m_targetPath);
            if (!IsSafeRelativePath(entry.m_targetPath)
                || !PathIsInsideRoot(inventory.m_targetRoot, entry.m_targetPath))
            {
                pathInvalid = true;
                AddBlocker(
                    preview.m_blockers,
                    "deployment.target_path_invalid",
                    entry.m_targetPath,
                    "Every target entry must remain beneath the exact reviewed TargetRoot.");
            }
        }
        AZStd::sort(inventoryEntryIds.begin(), inventoryEntryIds.end());
        if (AZStd::adjacent_find(inventoryEntryIds.begin(), inventoryEntryIds.end())
            != inventoryEntryIds.end())
        {
            inventoryUntrusted = true;
            AddBlocker(
                preview.m_blockers,
                "deployment.inventory_entry_duplicate",
                inventory.m_targetRoot,
                "Target inventory entry IDs must be unique.");
        }

        AZStd::sort(inventoryTargetPaths.begin(), inventoryTargetPaths.end());
        for (size_t index = 1; index < inventoryTargetPaths.size(); ++index)
        {
            if (inventoryTargetPaths[index] == inventoryTargetPaths[index - 1]
                && (index == 1
                    || inventoryTargetPaths[index] != inventoryTargetPaths[index - 2]))
            {
                AZStd::vector<AZStd::string> ids;
                for (const AdapterDeploymentTargetEntry& entry : inventory.m_entries)
                {
                    if (entry.m_targetPath == inventoryTargetPaths[index])
                    {
                        ids.push_back(entry.m_entryId);
                    }
                }
                AddConflict(
                    preview,
                    inventoryTargetPaths[index],
                    {},
                    AZStd::move(ids),
                    "Multiple declared target entries occupy one exact target path; no deployment or removal winner is selected.");
            }
        }

        AZStd::vector<AZStd::string> matchedTargetEntryIds;
        for (const AdapterPackageLayoutEntry& packageEntry :
             request.m_packagePreview.m_layout)
        {
            const AZStd::string& targetPath = packageEntry.m_packagePath;
            if (!IsSafeRelativePath(targetPath)
                || !PathIsInsideRoot(inventory.m_targetRoot, targetPath))
            {
                pathInvalid = true;
                AddBlocker(
                    preview.m_blockers,
                    "deployment.package_target_path_invalid",
                    targetPath,
                    "Every package-layout path must remain beneath the exact TargetRoot.");
                continue;
            }

            const AZStd::vector<const AdapterDeploymentTargetEntry*> matches =
                FindTargetEntries(inventory, targetPath);
            if (matches.empty())
            {
                AdapterDeploymentChange addition = MakeChange(
                    AdapterDeploymentChangeKind::Add,
                    &packageEntry,
                    nullptr,
                    targetPath,
                    "The reviewed package output is absent from the declared target inventory.");
                preview.m_additions.push_back(AZStd::move(addition));
                continue;
            }
            if (matches.size() > 1)
            {
                AZStd::vector<AZStd::string> ids;
                for (const AdapterDeploymentTargetEntry* entry : matches)
                {
                    ids.push_back(entry->m_entryId);
                    matchedTargetEntryIds.push_back(entry->m_entryId);
                }
                AddConflict(
                    preview,
                    targetPath,
                    packageEntry.m_inventoryEntryId,
                    AZStd::move(ids),
                    "Multiple declared target entries occupy one exact package path; no winner is selected.");
                continue;
            }

            const AdapterDeploymentTargetEntry& targetEntry = *matches.front();
            matchedTargetEntryIds.push_back(targetEntry.m_entryId);
            if (!targetEntry.m_projectOwned
                || !targetEntry.m_managed
                || targetEntry.m_ownerPackId != preview.m_packId)
            {
                AddConflict(
                    preview,
                    targetPath,
                    packageEntry.m_inventoryEntryId,
                    { targetEntry.m_entryId },
                    "The package path is occupied by an unmanaged, non-project-owned, or differently owned target entry.");
                continue;
            }
            if (targetEntry.m_role != packageEntry.m_role
                || targetEntry.m_mediaType != packageEntry.m_mediaType)
            {
                AddConflict(
                    preview,
                    targetPath,
                    packageEntry.m_inventoryEntryId,
                    { targetEntry.m_entryId },
                    "The target role or media type does not match the reviewed package layout.");
                continue;
            }
            if (!IsSha256Fingerprint(targetEntry.m_fingerprint))
            {
                AdapterDeploymentChange replacement = MakeChange(
                    AdapterDeploymentChangeKind::Replace,
                    &packageEntry,
                    &targetEntry,
                    targetPath,
                    "The target digest is unavailable, so replacement and rollback cannot be proven safely.");
                replacement.m_backupRequired = true;
                preview.m_replacements.push_back(AZStd::move(replacement));
                backupIncomplete = true;
                AddBlocker(
                    preview.m_blockers,
                    "deployment.backup_fingerprint_missing",
                    targetPath,
                    "A current target digest is required before replacement backup and rollback can be described.");
                continue;
            }
            if (targetEntry.m_fingerprint == packageEntry.m_outputDigest)
            {
                preview.m_unchanged.push_back(MakeChange(
                    AdapterDeploymentChangeKind::Unchanged,
                    &packageEntry,
                    &targetEntry,
                    targetPath,
                    "The target already carries the exact reviewed output digest."));
                continue;
            }
            if (!targetEntry.m_replaceable)
            {
                AddConflict(
                    preview,
                    targetPath,
                    packageEntry.m_inventoryEntryId,
                    { targetEntry.m_entryId },
                    "The target entry differs from the reviewed output and is not declared replaceable.");
                continue;
            }

            AdapterDeploymentChange replacement = MakeChange(
                AdapterDeploymentChangeKind::Replace,
                &packageEntry,
                &targetEntry,
                targetPath,
                "The managed project-owned target digest differs from the reviewed package output.");
            replacement.m_backupRequired = true;
            preview.m_replacements.push_back(AZStd::move(replacement));
            if (!AddBackupRequirement(
                    preview,
                    targetEntry,
                    inventory.m_backupRoot,
                    "Back up the exact current target before a hypothetical replacement."))
            {
                backupIncomplete = true;
            }
        }

        for (const AdapterDeploymentTargetEntry& targetEntry : inventory.m_entries)
        {
            if (ContainsValue(matchedTargetEntryIds, targetEntry.m_entryId))
            {
                continue;
            }
            if (FindPackageEntry(request.m_packagePreview, targetEntry.m_targetPath))
            {
                continue;
            }
            if (FindTargetEntries(inventory, targetEntry.m_targetPath).size() > 1)
            {
                continue;
            }
            if (!targetEntry.m_projectOwned
                || !targetEntry.m_managed
                || targetEntry.m_ownerPackId != preview.m_packId)
            {
                AddConflict(
                    preview,
                    targetEntry.m_targetPath,
                    {},
                    { targetEntry.m_entryId },
                    "An unmanaged, non-project-owned, or differently owned entry exists beneath the reviewed target root and is not part of the new package layout.");
                continue;
            }
            if (!targetEntry.m_removable)
            {
                AddConflict(
                    preview,
                    targetEntry.m_targetPath,
                    {},
                    { targetEntry.m_entryId },
                    "The obsolete managed target entry is not declared removable.");
                continue;
            }

            AdapterDeploymentChange removal = MakeChange(
                AdapterDeploymentChangeKind::Remove,
                nullptr,
                &targetEntry,
                targetEntry.m_targetPath,
                "The managed project-owned target entry is absent from the reviewed package layout.");
            removal.m_backupRequired = true;
            preview.m_removals.push_back(AZStd::move(removal));
            if (!AddBackupRequirement(
                    preview,
                    targetEntry,
                    inventory.m_backupRoot,
                    "Back up the exact current target before a hypothetical removal."))
            {
                backupIncomplete = true;
            }
        }

        SortPreviewCollections(preview);
        BuildRollbackPlan(preview);
        const size_t expectedRollbackCount = preview.m_additions.size()
            + preview.m_replacements.size()
            + preview.m_removals.size();
        if (preview.m_rollbackSteps.size() != expectedRollbackCount)
        {
            rollbackIncomplete = true;
            AddBlocker(
                preview.m_blockers,
                "deployment.rollback_incomplete",
                preview.m_targetRoot,
                "Every addition, replacement, and removal requires one deterministic inverse rollback step.");
        }
        if (inventoryUntrusted)
        {
            AddBlocker(
                preview.m_blockers,
                "deployment.inventory_untrusted",
                preview.m_targetRoot,
                "The target inventory contains incomplete or duplicate identities and cannot be trusted for deployment planning.");
        }
        if (pathInvalid)
        {
            AddBlocker(
                preview.m_blockers,
                "deployment.path_invalid",
                preview.m_targetRoot,
                "One or more target, package, or backup paths are unsafe or outside their reviewed roots.");
        }

        if (packageNotReady)
        {
            preview.m_status = AdapterStagingDeploymentPreviewStatus::PackageNotReady;
        }
        else if (targetUnreviewed)
        {
            preview.m_status = AdapterStagingDeploymentPreviewStatus::TargetUnreviewed;
        }
        else if (inventoryBindingMismatch)
        {
            preview.m_status =
                AdapterStagingDeploymentPreviewStatus::InventoryBindingMismatch;
        }
        else if (inventoryUntrusted)
        {
            preview.m_status = AdapterStagingDeploymentPreviewStatus::InventoryUntrusted;
        }
        else if (pathInvalid)
        {
            preview.m_status = AdapterStagingDeploymentPreviewStatus::PathInvalid;
        }
        else if (!preview.m_conflicts.empty())
        {
            preview.m_status = AdapterStagingDeploymentPreviewStatus::Conflict;
            AddBlocker(
                preview.m_blockers,
                "deployment.conflict",
                preview.m_targetRoot,
                "One or more exact target paths have ownership, identity, type, or multiplicity conflicts.");
        }
        else if (backupIncomplete)
        {
            preview.m_status = AdapterStagingDeploymentPreviewStatus::BackupIncomplete;
        }
        else if (rollbackIncomplete)
        {
            preview.m_status = AdapterStagingDeploymentPreviewStatus::RollbackIncomplete;
        }
        else
        {
            preview.m_status = AdapterStagingDeploymentPreviewStatus::Ready;
            AddBlocker(
                preview.m_blockers,
                "deployment.preview_only",
                preview.m_targetRoot,
                "The deterministic deployment and rollback description is complete; all filesystem mutation and launch operations remain prohibited.");
        }

        SortPreviewCollections(preview);
        preview.m_canonicalJson = SerializeCanonicalPreview(preview);
        return preview;
    }
} // namespace TaintedGrailModdingSDK
