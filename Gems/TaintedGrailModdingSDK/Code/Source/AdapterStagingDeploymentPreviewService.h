/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterPackageAssemblyPreviewService.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    enum class AdapterDeploymentTargetReviewDecision : AZ::u8
    {
        Unknown,
        Accepted,
        Rejected,
    };

    enum class AdapterDeploymentChangeKind : AZ::u8
    {
        Add,
        Replace,
        Remove,
        Unchanged,
        Conflict,
    };

    enum class AdapterDeploymentRollbackAction : AZ::u8
    {
        RemoveAdded,
        RestoreReplaced,
        RestoreRemoved,
    };

    enum class AdapterStagingDeploymentPreviewStatus : AZ::u8
    {
        Ready,
        PackageNotReady,
        TargetUnreviewed,
        InventoryBindingMismatch,
        InventoryUntrusted,
        PathInvalid,
        Conflict,
        BackupIncomplete,
        RollbackIncomplete,
    };

    AZStd::string ToString(AdapterDeploymentTargetReviewDecision decision);
    AZStd::string ToString(AdapterDeploymentChangeKind kind);
    AZStd::string ToString(AdapterDeploymentRollbackAction action);
    AZStd::string ToString(AdapterStagingDeploymentPreviewStatus status);
    bool TryParseAdapterDeploymentTargetReviewDecision(
        const AZStd::string& value,
        AdapterDeploymentTargetReviewDecision& decision);
    bool TryParseAdapterDeploymentChangeKind(
        const AZStd::string& value,
        AdapterDeploymentChangeKind& kind);
    bool TryParseAdapterDeploymentRollbackAction(
        const AZStd::string& value,
        AdapterDeploymentRollbackAction& action);
    bool TryParseAdapterStagingDeploymentPreviewStatus(
        const AZStd::string& value,
        AdapterStagingDeploymentPreviewStatus& status);

    struct AdapterDeploymentTargetReview
    {
        AZStd::string m_reviewId;
        AZStd::string m_inventoryId;
        AZStd::string m_inventoryFingerprint;
        AdapterDeploymentTargetReviewDecision m_decision =
            AdapterDeploymentTargetReviewDecision::Unknown;
        AZStd::string m_reviewer;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_notes;
    };

    struct AdapterDeploymentTargetEntry
    {
        AZStd::string m_entryId;
        AZStd::string m_targetPath;
        AZStd::string m_role;
        AZStd::string m_mediaType;
        AZStd::string m_fingerprint;
        AZ::u64 m_byteSize = 0;
        AZStd::string m_ownerPackId;
        bool m_projectOwned = false;
        bool m_managed = false;
        bool m_replaceable = false;
        bool m_removable = false;
    };

    struct AdapterDeploymentTargetInventory
    {
        AZ::u32 m_formatVersion = 1;
        AZStd::string m_inventoryId;
        AZStd::string m_inventoryFingerprint;
        AZStd::string m_packagePreviewId;
        AZStd::string m_packagePreviewFingerprint;
        AZStd::string m_packId;
        AZStd::string m_targetRoot;
        AZStd::string m_backupRoot;
        AZStd::vector<AdapterDeploymentTargetEntry> m_entries;
    };

    struct AdapterDeploymentChange
    {
        AZStd::string m_changeId;
        AdapterDeploymentChangeKind m_kind = AdapterDeploymentChangeKind::Unchanged;
        AZStd::string m_packageEntryId;
        AZStd::string m_targetEntryId;
        AZStd::string m_packagePath;
        AZStd::string m_targetPath;
        AZStd::string m_role;
        AZStd::string m_mediaType;
        AZStd::string m_previousFingerprint;
        AZStd::string m_desiredFingerprint;
        AZ::u64 m_previousByteSize = 0;
        AZ::u64 m_desiredByteSize = 0;
        bool m_backupRequired = false;
        AZStd::string m_reason;
    };

    struct AdapterDeploymentConflict
    {
        AZStd::string m_targetPath;
        AZStd::string m_packageEntryId;
        AZStd::vector<AZStd::string> m_targetEntryIds;
        AZStd::string m_reason;
    };

    struct AdapterDeploymentBackupRequirement
    {
        AZStd::string m_backupId;
        AZStd::string m_targetEntryId;
        AZStd::string m_targetPath;
        AZStd::string m_backupPath;
        AZStd::string m_fingerprint;
        AZ::u64 m_byteSize = 0;
        AZStd::string m_reason;
    };

    struct AdapterDeploymentRollbackStep
    {
        AZ::u64 m_sequence = 0;
        AZStd::string m_stepId;
        AdapterDeploymentRollbackAction m_action =
            AdapterDeploymentRollbackAction::RemoveAdded;
        AZStd::string m_targetPath;
        AZStd::string m_backupPath;
        AZStd::string m_expectedDeployedFingerprint;
        AZStd::string m_restoreFingerprint;
        AZStd::string m_reason;
    };

    struct AdapterDeploymentPreviewBlocker
    {
        AZStd::string m_code;
        AZStd::string m_targetPath;
        AZStd::string m_reason;
    };

    struct AdapterStagingDeploymentPreviewRequest
    {
        AdapterPackageAssemblyPreview m_packagePreview;
        AZStd::string m_packagePreviewFingerprint;
        AdapterDeploymentTargetReview m_targetReview;
        AdapterDeploymentTargetInventory m_targetInventory;
    };

    struct AdapterStagingDeploymentPreview
    {
        AZ::u32 m_formatVersion = 1;
        AZStd::string m_previewId;
        AZStd::string m_packagePreviewId;
        AZStd::string m_packagePreviewFingerprint;
        AZStd::string m_targetInventoryId;
        AZStd::string m_targetInventoryFingerprint;
        AZStd::string m_packId;
        AZStd::string m_packageRoot;
        AZStd::string m_targetRoot;
        AZStd::string m_backupRoot;
        AdapterStagingDeploymentPreviewStatus m_status =
            AdapterStagingDeploymentPreviewStatus::PackageNotReady;
        AZStd::vector<AdapterDeploymentChange> m_additions;
        AZStd::vector<AdapterDeploymentChange> m_replacements;
        AZStd::vector<AdapterDeploymentChange> m_removals;
        AZStd::vector<AdapterDeploymentChange> m_unchanged;
        AZStd::vector<AdapterDeploymentConflict> m_conflicts;
        AZStd::vector<AdapterDeploymentBackupRequirement> m_backups;
        AZStd::vector<AdapterDeploymentRollbackStep> m_rollbackSteps;
        AZStd::vector<AdapterDeploymentPreviewBlocker> m_blockers;
        AZStd::string m_canonicalJson;
        bool m_stagingMutationAllowed = false;
        bool m_deploymentMutationAllowed = false;
        bool m_rollbackExecutionAllowed = false;
        bool m_launchAllowed = false;
    };

    class AdapterStagingDeploymentPreviewRegistry
    {
    public:
        static AdapterStagingDeploymentPreviewRegistry& Get();

        bool RegisterRequest(
            const AdapterStagingDeploymentPreviewRequest& request,
            AZStd::string* error = nullptr);
        void Clear();
        const AZStd::vector<AdapterStagingDeploymentPreviewRequest>& GetRequests() const;

    private:
        AZStd::vector<AdapterStagingDeploymentPreviewRequest> m_requests;
    };

    class AdapterStagingDeploymentPreviewService
    {
    public:
        AdapterStagingDeploymentPreview BuildPreview(
            const AdapterStagingDeploymentPreviewRequest& request) const;
        AZStd::string SerializeCanonicalPreview(
            const AdapterStagingDeploymentPreview& preview) const;
    };
} // namespace TaintedGrailModdingSDK
