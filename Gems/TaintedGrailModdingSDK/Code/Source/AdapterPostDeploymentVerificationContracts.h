/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterDeploymentExecutionEvidenceService.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    enum class AdapterPostDeploymentReportStatus : AZ::u8
    {
        ReviewReady,
        EvidenceRejected,
        EvidenceIncomplete,
        VerificationIncomplete,
        CompatibilityBlocked,
        RollbackIncomplete,
        ReleaseBlocked,
    };

    enum class AdapterPostDeploymentBlockerKind : AZ::u8
    {
        ExecutionEvidenceRejected,
        EvidenceBindingMismatch,
        CandidateEvidenceMissing,
        StepNotCompleted,
        StepFailed,
        BackupIncomplete,
        TargetNotChecked,
        TargetMismatch,
        RollbackIncomplete,
        DeploymentRolledBack,
        FailureReported,
        DiagnosticMissing,
    };

    enum class AdapterPostDeploymentBlockerSeverity : AZ::u8
    {
        Warning,
        Blocking,
    };

    AZStd::string ToString(AdapterPostDeploymentReportStatus status);
    AZStd::string ToString(AdapterPostDeploymentBlockerKind kind);
    AZStd::string ToString(AdapterPostDeploymentBlockerSeverity severity);

    bool TryParseAdapterPostDeploymentReportStatus(
        const AZStd::string& value,
        AdapterPostDeploymentReportStatus& status);
    bool TryParseAdapterPostDeploymentBlockerKind(
        const AZStd::string& value,
        AdapterPostDeploymentBlockerKind& kind);
    bool TryParseAdapterPostDeploymentBlockerSeverity(
        const AZStd::string& value,
        AdapterPostDeploymentBlockerSeverity& severity);

    struct AdapterPostDeploymentBlocker
    {
        AZStd::string m_blockerId;
        AdapterPostDeploymentBlockerKind m_kind =
            AdapterPostDeploymentBlockerKind::ExecutionEvidenceRejected;
        AdapterPostDeploymentBlockerSeverity m_severity =
            AdapterPostDeploymentBlockerSeverity::Blocking;
        AZStd::string m_code;
        AZStd::string m_subjectRef;
        AZStd::string m_message;
        AZStd::string m_stepId;
        AZStd::string m_rollbackResultId;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_logReferenceIds;
        bool m_blocksCompatibility = false;
        bool m_blocksRelease = true;
    };

    struct AdapterPostDeploymentVerificationReport
    {
        AZ::u32 m_contractVersion = 1;
        AZStd::string m_reportId;
        AZStd::string m_resultId;
        AZStd::string m_workOrderId;
        AZStd::string m_workOrderFingerprint;
        AZStd::string m_resultFingerprint;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AdapterPostDeploymentReportStatus m_status =
            AdapterPostDeploymentReportStatus::EvidenceRejected;
        AZ::u64 m_candidateSourceDocumentCount = 0;
        AZ::u64 m_candidateEvidenceRecordCount = 0;
        AZ::u64 m_stepCount = 0;
        AZ::u64 m_completedStepCount = 0;
        AZ::u64 m_failedStepCount = 0;
        AZ::u64 m_incompleteStepCount = 0;
        AZ::u64 m_backupResultCount = 0;
        AZ::u64 m_incompleteBackupCount = 0;
        AZ::u64 m_matchedVerificationCount = 0;
        AZ::u64 m_mismatchedVerificationCount = 0;
        AZ::u64 m_uncheckedVerificationCount = 0;
        AZ::u64 m_rollbackRequiredCount = 0;
        AZ::u64 m_rollbackSucceededCount = 0;
        AZ::u64 m_rollbackIncompleteCount = 0;
        AZ::u64 m_failureCount = 0;
        AZ::u64 m_diagnosticReferenceCount = 0;
        AZ::u64 m_compatibilityBlockerCount = 0;
        AZ::u64 m_releaseBlockerCount = 0;
        AZStd::vector<AZStd::string> m_candidateSourceIds;
        AZStd::vector<AZStd::string> m_candidateEvidenceIds;
        AZStd::vector<AZStd::string> m_diagnosticReferenceIds;
        AZStd::vector<AdapterPostDeploymentBlocker> m_blockers;
        bool m_executionEvidenceAccepted = false;
        bool m_compatibilityClear = false;
        bool m_releaseBlockerFree = false;
        bool m_humanReviewRequired = true;
        bool m_verifierExecuted = false;
        bool m_evidencePromoted = false;
        bool m_releasePublished = false;
        bool m_launchPerformed = false;
        bool m_adapterCalled = false;
    };
} // namespace TaintedGrailModdingSDK
