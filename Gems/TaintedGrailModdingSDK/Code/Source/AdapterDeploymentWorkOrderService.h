/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterStagingDeploymentPreviewService.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    enum class AdapterDeploymentConfirmationDecision : AZ::u8
    {
        Unknown,
        Confirmed,
        Rejected,
    };

    enum class AdapterDeploymentConfirmationScope : AZ::u8
    {
        AdditionsOnly,
        AdditionsAndReplacements,
        FullPreview,
    };

    enum class AdapterDeploymentPreflightKind : AZ::u8
    {
        PackageIntegrity,
        TargetInventory,
        BackupReadiness,
        RollbackReadiness,
        OperatorReadiness,
    };

    enum class AdapterDeploymentPreflightStatus : AZ::u8
    {
        Unknown,
        Passed,
        Failed,
    };

    enum class AdapterDeploymentWorkOrderStepKind : AZ::u8
    {
        VerifyPreflight,
        ConfirmMaintenanceWindow,
        Backup,
        Add,
        Replace,
        Remove,
        VerifyDeployment,
        PreserveRollback,
    };

    enum class AdapterDeploymentChecklistState : AZ::u8
    {
        ContractSatisfied,
        OperatorActionRequired,
        Blocked,
    };

    enum class AdapterDeploymentWorkOrderStatus : AZ::u8
    {
        ReviewReady,
        PreviewNotReady,
        ConfirmationMissing,
        ConfirmationRejected,
        ConfirmationBindingMismatch,
        ScopeMismatch,
        ConfirmationExpired,
        MaintenanceWindowInvalid,
        OutsideMaintenanceWindow,
        PreflightMissing,
        PreflightFailed,
        WorkOrderIncomplete,
    };

    AZStd::string ToString(AdapterDeploymentConfirmationDecision decision);
    AZStd::string ToString(AdapterDeploymentConfirmationScope scope);
    AZStd::string ToString(AdapterDeploymentPreflightKind kind);
    AZStd::string ToString(AdapterDeploymentPreflightStatus status);
    AZStd::string ToString(AdapterDeploymentWorkOrderStepKind kind);
    AZStd::string ToString(AdapterDeploymentChecklistState state);
    AZStd::string ToString(AdapterDeploymentWorkOrderStatus status);

    bool TryParseAdapterDeploymentConfirmationDecision(
        const AZStd::string& value,
        AdapterDeploymentConfirmationDecision& decision);
    bool TryParseAdapterDeploymentConfirmationScope(
        const AZStd::string& value,
        AdapterDeploymentConfirmationScope& scope);
    bool TryParseAdapterDeploymentPreflightKind(
        const AZStd::string& value,
        AdapterDeploymentPreflightKind& kind);
    bool TryParseAdapterDeploymentPreflightStatus(
        const AZStd::string& value,
        AdapterDeploymentPreflightStatus& status);
    bool TryParseAdapterDeploymentWorkOrderStepKind(
        const AZStd::string& value,
        AdapterDeploymentWorkOrderStepKind& kind);
    bool TryParseAdapterDeploymentChecklistState(
        const AZStd::string& value,
        AdapterDeploymentChecklistState& state);
    bool TryParseAdapterDeploymentWorkOrderStatus(
        const AZStd::string& value,
        AdapterDeploymentWorkOrderStatus& status);

    struct AdapterDeploymentConfirmation
    {
        AZStd::string m_confirmationId;
        AZStd::string m_previewId;
        AZStd::string m_previewFingerprint;
        AdapterDeploymentConfirmationDecision m_decision =
            AdapterDeploymentConfirmationDecision::Unknown;
        AdapterDeploymentConfirmationScope m_scope =
            AdapterDeploymentConfirmationScope::FullPreview;
        AZStd::string m_reviewer;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_issuedAtUtc;
        AZStd::string m_expiresAtUtc;
        AZStd::string m_notes;
    };

    struct AdapterDeploymentMaintenanceWindow
    {
        AZStd::string m_windowId;
        AZStd::string m_previewId;
        AZStd::string m_previewFingerprint;
        AZStd::string m_startAtUtc;
        AZStd::string m_endAtUtc;
        AZStd::string m_operatorGroup;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };

    struct AdapterDeploymentPreflightEvidence
    {
        AZStd::string m_preflightId;
        AdapterDeploymentPreflightKind m_kind =
            AdapterDeploymentPreflightKind::PackageIntegrity;
        AdapterDeploymentPreflightStatus m_status =
            AdapterDeploymentPreflightStatus::Unknown;
        AZStd::string m_previewId;
        AZStd::string m_previewFingerprint;
        AZStd::string m_checkedAtUtc;
        AZStd::string m_checker;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_notes;
    };

    struct AdapterDeploymentWorkOrderStep
    {
        AZ::u64 m_sequence = 0;
        AZStd::string m_stepId;
        AdapterDeploymentWorkOrderStepKind m_kind =
            AdapterDeploymentWorkOrderStepKind::VerifyPreflight;
        AZStd::string m_targetPath;
        AZStd::string m_backupPath;
        AZStd::string m_previousFingerprint;
        AZStd::string m_desiredFingerprint;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_description;
        bool m_executionAllowed = false;
    };

    struct AdapterDeploymentOperatorChecklistItem
    {
        AZ::u64 m_sequence = 0;
        AZStd::string m_itemId;
        AdapterDeploymentChecklistState m_state =
            AdapterDeploymentChecklistState::OperatorActionRequired;
        AZStd::string m_label;
        AZStd::string m_detail;
        AZStd::vector<AZStd::string> m_evidenceIds;
        bool m_acknowledgementRecorded = false;
    };

    struct AdapterDeploymentWorkOrderBlocker
    {
        AZStd::string m_code;
        AZStd::string m_subject;
        AZStd::string m_reason;
    };

    struct AdapterDeploymentWorkOrderRequest
    {
        AdapterStagingDeploymentPreview m_preview;
        AZStd::string m_previewFingerprint;
        AdapterDeploymentConfirmation m_confirmation;
        AdapterDeploymentMaintenanceWindow m_maintenanceWindow;
        AZStd::string m_evaluatedAtUtc;
        AZStd::vector<AdapterDeploymentPreflightEvidence> m_preflightEvidence;
    };

    struct AdapterDeploymentWorkOrder
    {
        AZ::u32 m_formatVersion = 1;
        AZStd::string m_workOrderId;
        AZStd::string m_previewId;
        AZStd::string m_previewFingerprint;
        AZStd::string m_packId;
        AZStd::string m_targetInventoryId;
        AZStd::string m_confirmationId;
        AdapterDeploymentConfirmationScope m_confirmationScope =
            AdapterDeploymentConfirmationScope::FullPreview;
        AZStd::string m_reviewer;
        AZStd::string m_evaluatedAtUtc;
        AZStd::string m_confirmationExpiresAtUtc;
        AZStd::string m_maintenanceWindowId;
        AZStd::string m_maintenanceStartAtUtc;
        AZStd::string m_maintenanceEndAtUtc;
        AZStd::string m_operatorGroup;
        AdapterDeploymentWorkOrderStatus m_status =
            AdapterDeploymentWorkOrderStatus::PreviewNotReady;
        AZStd::vector<AdapterDeploymentWorkOrderStep> m_steps;
        AZStd::vector<AdapterDeploymentOperatorChecklistItem> m_checklist;
        AZStd::vector<AdapterDeploymentWorkOrderBlocker> m_blockers;
        AZStd::string m_canonicalJson;
        bool m_executionAllowed = false;
        bool m_copyAllowed = false;
        bool m_deleteAllowed = false;
        bool m_backupAllowed = false;
        bool m_restoreAllowed = false;
        bool m_deploymentAllowed = false;
        bool m_launchAllowed = false;
    };

    class AdapterDeploymentWorkOrderRegistry
    {
    public:
        static AdapterDeploymentWorkOrderRegistry& Get();

        bool RegisterRequest(
            const AdapterDeploymentWorkOrderRequest& request,
            AZStd::string* error = nullptr);
        void Clear();
        const AZStd::vector<AdapterDeploymentWorkOrderRequest>& GetRequests() const;

    private:
        AZStd::vector<AdapterDeploymentWorkOrderRequest> m_requests;
    };

    class AdapterDeploymentWorkOrderService
    {
    public:
        AdapterDeploymentWorkOrder BuildWorkOrder(
            const AdapterDeploymentWorkOrderRequest& request) const;
        AZStd::string SerializeCanonicalWorkOrder(
            const AdapterDeploymentWorkOrder& workOrder) const;
    };
} // namespace TaintedGrailModdingSDK
