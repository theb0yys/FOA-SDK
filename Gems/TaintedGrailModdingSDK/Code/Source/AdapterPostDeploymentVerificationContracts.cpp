/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPostDeploymentVerificationContracts.h"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        template<class EnumType>
        struct EnumName
        {
            EnumType m_value;
            const char* m_name;
        };

        constexpr EnumName<AdapterPostDeploymentReportStatus> ReportStatuses[] = {
            { AdapterPostDeploymentReportStatus::ReviewReady, "review_ready" },
            { AdapterPostDeploymentReportStatus::EvidenceRejected,
                "evidence_rejected" },
            { AdapterPostDeploymentReportStatus::EvidenceIncomplete,
                "evidence_incomplete" },
            { AdapterPostDeploymentReportStatus::VerificationIncomplete,
                "verification_incomplete" },
            { AdapterPostDeploymentReportStatus::CompatibilityBlocked,
                "compatibility_blocked" },
            { AdapterPostDeploymentReportStatus::RollbackIncomplete,
                "rollback_incomplete" },
            { AdapterPostDeploymentReportStatus::ReleaseBlocked,
                "release_blocked" },
        };

        constexpr EnumName<AdapterPostDeploymentBlockerKind> BlockerKinds[] = {
            { AdapterPostDeploymentBlockerKind::ExecutionEvidenceRejected,
                "execution_evidence_rejected" },
            { AdapterPostDeploymentBlockerKind::EvidenceBindingMismatch,
                "evidence_binding_mismatch" },
            { AdapterPostDeploymentBlockerKind::CandidateEvidenceMissing,
                "candidate_evidence_missing" },
            { AdapterPostDeploymentBlockerKind::StepNotCompleted,
                "step_not_completed" },
            { AdapterPostDeploymentBlockerKind::StepFailed, "step_failed" },
            { AdapterPostDeploymentBlockerKind::BackupIncomplete,
                "backup_incomplete" },
            { AdapterPostDeploymentBlockerKind::TargetNotChecked,
                "target_not_checked" },
            { AdapterPostDeploymentBlockerKind::TargetMismatch,
                "target_mismatch" },
            { AdapterPostDeploymentBlockerKind::RollbackIncomplete,
                "rollback_incomplete" },
            { AdapterPostDeploymentBlockerKind::DeploymentRolledBack,
                "deployment_rolled_back" },
            { AdapterPostDeploymentBlockerKind::FailureReported,
                "failure_reported" },
            { AdapterPostDeploymentBlockerKind::DiagnosticMissing,
                "diagnostic_missing" },
        };

        constexpr EnumName<AdapterPostDeploymentBlockerSeverity>
            BlockerSeverities[] = {
                { AdapterPostDeploymentBlockerSeverity::Warning, "warning" },
                { AdapterPostDeploymentBlockerSeverity::Blocking, "blocking" },
            };

        template<class EnumType, size_t Count>
        AZStd::string EnumToString(
            EnumType value,
            const EnumName<EnumType> (&names)[Count])
        {
            for (const EnumName<EnumType>& name : names)
            {
                if (name.m_value == value)
                {
                    return name.m_name;
                }
            }
            return "unknown";
        }

        template<class EnumType, size_t Count>
        bool TryParseEnum(
            const AZStd::string& value,
            EnumType& result,
            const EnumName<EnumType> (&names)[Count])
        {
            for (const EnumName<EnumType>& name : names)
            {
                if (value == name.m_name)
                {
                    result = name.m_value;
                    return true;
                }
            }
            return false;
        }
    } // namespace

    AZStd::string ToString(AdapterPostDeploymentReportStatus status)
    {
        return EnumToString(status, ReportStatuses);
    }

    AZStd::string ToString(AdapterPostDeploymentBlockerKind kind)
    {
        return EnumToString(kind, BlockerKinds);
    }

    AZStd::string ToString(AdapterPostDeploymentBlockerSeverity severity)
    {
        return EnumToString(severity, BlockerSeverities);
    }

    bool TryParseAdapterPostDeploymentReportStatus(
        const AZStd::string& value,
        AdapterPostDeploymentReportStatus& status)
    {
        return TryParseEnum(value, status, ReportStatuses);
    }

    bool TryParseAdapterPostDeploymentBlockerKind(
        const AZStd::string& value,
        AdapterPostDeploymentBlockerKind& kind)
    {
        return TryParseEnum(value, kind, BlockerKinds);
    }

    bool TryParseAdapterPostDeploymentBlockerSeverity(
        const AZStd::string& value,
        AdapterPostDeploymentBlockerSeverity& severity)
    {
        return TryParseEnum(value, severity, BlockerSeverities);
    }
} // namespace TaintedGrailModdingSDK
