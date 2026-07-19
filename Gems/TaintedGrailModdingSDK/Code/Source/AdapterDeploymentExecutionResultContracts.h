/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterDeploymentWorkOrderService.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    enum class AdapterDeploymentExecutorReviewDecision : AZ::u8
    {
        Unknown,
        Accepted,
        Rejected,
    };

    enum class AdapterDeploymentExecutionOutcome : AZ::u8
    {
        NotAttempted,
        Succeeded,
        Failed,
        Skipped,
    };

    enum class AdapterDeploymentVerificationStatus : AZ::u8
    {
        NotChecked,
        Matched,
        Mismatched,
    };

    enum class AdapterDeploymentExecutionFailureKind : AZ::u8
    {
        Contract,
        Executor,
        Preflight,
        Backup,
        Copy,
        Replace,
        Delete,
        Verification,
        Rollback,
        Restore,
        Unknown,
    };

    enum class AdapterDeploymentExecutionLogKind : AZ::u8
    {
        Executor,
        Filesystem,
        Backup,
        Verification,
        Rollback,
        Diagnostic,
    };

    enum class AdapterDeploymentExecutionEnvelopeStatus : AZ::u8
    {
        Accepted,
        WorkOrderNotReady,
        ExecutorUnreviewed,
        WorkOrderBindingMismatch,
        EnvelopeInvalid,
        StepBindingMismatch,
        BackupBindingMismatch,
        VerificationBindingMismatch,
        RollbackBindingMismatch,
        FailureLogBindingMismatch,
    };

    AZStd::string ToString(AdapterDeploymentExecutorReviewDecision decision);
    AZStd::string ToString(AdapterDeploymentExecutionOutcome outcome);
    AZStd::string ToString(AdapterDeploymentVerificationStatus status);
    AZStd::string ToString(AdapterDeploymentExecutionFailureKind kind);
    AZStd::string ToString(AdapterDeploymentExecutionLogKind kind);
    AZStd::string ToString(AdapterDeploymentExecutionEnvelopeStatus status);

    bool TryParseAdapterDeploymentExecutorReviewDecision(
        const AZStd::string& value,
        AdapterDeploymentExecutorReviewDecision& decision);
    bool TryParseAdapterDeploymentExecutionOutcome(
        const AZStd::string& value,
        AdapterDeploymentExecutionOutcome& outcome);
    bool TryParseAdapterDeploymentVerificationStatus(
        const AZStd::string& value,
        AdapterDeploymentVerificationStatus& status);
    bool TryParseAdapterDeploymentExecutionFailureKind(
        const AZStd::string& value,
        AdapterDeploymentExecutionFailureKind& kind);
    bool TryParseAdapterDeploymentExecutionLogKind(
        const AZStd::string& value,
        AdapterDeploymentExecutionLogKind& kind);
    bool TryParseAdapterDeploymentExecutionEnvelopeStatus(
        const AZStd::string& value,
        AdapterDeploymentExecutionEnvelopeStatus& status);

    bool IsAdapterDeploymentExecutionStableId(const AZStd::string& value);
    bool IsAdapterDeploymentExecutionFingerprint(const AZStd::string& value);
    bool IsAdapterDeploymentExecutionLogReference(const AZStd::string& value);
    bool IsAdapterDeploymentExecutionUtcTimestamp(const AZStd::string& value);

    struct AdapterDeploymentExecutorReview
    {
        AZStd::string m_reviewId;
        AZStd::string m_executorId;
        AZStd::string m_executorVersion;
        AZStd::string m_executorFingerprint;
        AdapterDeploymentExecutorReviewDecision m_decision =
            AdapterDeploymentExecutorReviewDecision::Unknown;
        AZStd::string m_reviewer;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_reviewedAtUtc;
        AZStd::string m_notes;
    };

    struct AdapterDeploymentExecutionFailure
    {
        AZStd::string m_failureId;
        AdapterDeploymentExecutionFailureKind m_kind =
            AdapterDeploymentExecutionFailureKind::Unknown;
        AZStd::string m_code;
        AZStd::string m_message;
        AZStd::string m_stepId;
        AZStd::string m_rollbackResultId;
        AZStd::vector<AZStd::string> m_logReferenceIds;
        bool m_retryable = false;
    };

    struct AdapterDeploymentExecutionLogReference
    {
        AZStd::string m_logId;
        AdapterDeploymentExecutionLogKind m_kind =
            AdapterDeploymentExecutionLogKind::Diagnostic;
        AZStd::string m_reference;
        AZStd::string m_fingerprint;
        AZStd::vector<AZStd::string> m_stepIds;
        AZStd::vector<AZStd::string> m_rollbackResultIds;
    };

    struct AdapterDeploymentExecutionStepResult
    {
        AZStd::string m_stepId;
        AZ::u64 m_sequence = 0;
        AdapterDeploymentWorkOrderStepKind m_kind =
            AdapterDeploymentWorkOrderStepKind::VerifyPreflight;
        AZStd::string m_targetPath;
        AZStd::string m_backupPath;
        AZStd::string m_previousFingerprint;
        AZStd::string m_desiredFingerprint;
        AdapterDeploymentExecutionOutcome m_outcome =
            AdapterDeploymentExecutionOutcome::NotAttempted;
        AZStd::string m_observedFingerprint;
        bool m_targetPresent = false;
        bool m_attempted = false;
        AZStd::vector<AZStd::string> m_failureIds;
        AZStd::vector<AZStd::string> m_logReferenceIds;
    };

    struct AdapterDeploymentBackupResult
    {
        AZStd::string m_stepId;
        AZStd::string m_targetPath;
        AZStd::string m_backupPath;
        AZStd::string m_sourceFingerprint;
        AZStd::string m_backupFingerprint;
        AdapterDeploymentExecutionOutcome m_outcome =
            AdapterDeploymentExecutionOutcome::NotAttempted;
        bool m_attempted = false;
        AZStd::vector<AZStd::string> m_failureIds;
        AZStd::vector<AZStd::string> m_logReferenceIds;
    };

    struct AdapterDeploymentTargetVerification
    {
        AZStd::string m_stepId;
        AZStd::string m_targetPath;
        AZStd::string m_expectedFingerprint;
        AZStd::string m_observedFingerprint;
        bool m_expectedPresent = false;
        bool m_observedPresent = false;
        AdapterDeploymentVerificationStatus m_status =
            AdapterDeploymentVerificationStatus::NotChecked;
        AZStd::string m_checkedAtUtc;
        AZStd::vector<AZStd::string> m_logReferenceIds;
    };

    // Reuses Slice 13 enum class AdapterDeploymentRollbackAction with canonical values:
    // "remove_added", "restore_replaced", and "restore_removed".
    struct AdapterDeploymentRollbackResult
    {
        AZStd::string m_rollbackResultId;
        AZStd::string m_sourceStepId;
        AdapterDeploymentRollbackAction m_action =
            AdapterDeploymentRollbackAction::RemoveAdded;
        AZStd::string m_targetPath;
        AZStd::string m_backupPath;
        AZStd::string m_expectedDeployedFingerprint;
        AZStd::string m_restoreFingerprint;
        AdapterDeploymentExecutionOutcome m_outcome =
            AdapterDeploymentExecutionOutcome::NotAttempted;
        AZStd::string m_finalFingerprint;
        bool m_targetPresent = false;
        bool m_attempted = false;
        AZStd::vector<AZStd::string> m_failureIds;
        AZStd::vector<AZStd::string> m_logReferenceIds;
    };

    struct AdapterDeploymentExecutionResultEnvelope
    {
        AZ::u32 m_contractVersion = 1;
        AZStd::string m_resultId;
        AZStd::string m_workOrderId;
        AZStd::string m_workOrderCanonicalJson;
        AZStd::string m_workOrderFingerprint;
        AZStd::string m_previewId;
        AZStd::string m_previewFingerprint;
        AZStd::string m_packId;
        AZStd::string m_targetInventoryId;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AdapterDeploymentExecutorReview m_executorReview;
        AZStd::string m_capturedAtUtc;
        AZStd::string m_resultFingerprint;
        AZStd::vector<AdapterDeploymentExecutionStepResult> m_stepResults;
        AZStd::vector<AdapterDeploymentBackupResult> m_backupResults;
        AZStd::vector<AdapterDeploymentTargetVerification> m_targetVerifications;
        AZStd::vector<AdapterDeploymentRollbackResult> m_rollbackResults;
        AZStd::vector<AdapterDeploymentExecutionFailure> m_failures;
        AZStd::vector<AdapterDeploymentExecutionLogReference> m_logReferences;
    };

    class AdapterDeploymentExecutionResultRegistry
    {
    public:
        static AdapterDeploymentExecutionResultRegistry& Get();

        bool RegisterEnvelope(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AZStd::string* error = nullptr);
        void Clear();

        const AdapterDeploymentExecutionResultEnvelope* FindByResultId(
            const AZStd::string& resultId) const;
        const AZStd::vector<AdapterDeploymentExecutionResultEnvelope>&
            GetEnvelopes() const;

    private:
        AZStd::vector<AdapterDeploymentExecutionResultEnvelope> m_envelopes;
    };
} // namespace TaintedGrailModdingSDK
