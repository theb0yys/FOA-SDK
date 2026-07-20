/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentExecutionResultContracts.h"

#include "AdapterDeploymentExecutionEvidenceService.h"
#include "AdapterDeploymentExecutionResultCanonical.h"
#include "CanonicalFingerprint.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

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

        constexpr EnumName<AdapterDeploymentExecutorReviewDecision>
            ExecutorReviewDecisions[] = {
                { AdapterDeploymentExecutorReviewDecision::Unknown, "unknown" },
                { AdapterDeploymentExecutorReviewDecision::Accepted, "accepted" },
                { AdapterDeploymentExecutorReviewDecision::Rejected, "rejected" },
            };

        constexpr EnumName<AdapterDeploymentExecutionOutcome> ExecutionOutcomes[] = {
            { AdapterDeploymentExecutionOutcome::NotAttempted, "not_attempted" },
            { AdapterDeploymentExecutionOutcome::Succeeded, "succeeded" },
            { AdapterDeploymentExecutionOutcome::Failed, "failed" },
            { AdapterDeploymentExecutionOutcome::Skipped, "skipped" },
        };

        constexpr EnumName<AdapterDeploymentVerificationStatus>
            VerificationStatuses[] = {
                { AdapterDeploymentVerificationStatus::NotChecked, "not_checked" },
                { AdapterDeploymentVerificationStatus::Matched, "matched" },
                { AdapterDeploymentVerificationStatus::Mismatched, "mismatched" },
            };

        constexpr EnumName<AdapterDeploymentExecutionFailureKind> FailureKinds[] = {
            { AdapterDeploymentExecutionFailureKind::Contract, "contract" },
            { AdapterDeploymentExecutionFailureKind::Executor, "executor" },
            { AdapterDeploymentExecutionFailureKind::Preflight, "preflight" },
            { AdapterDeploymentExecutionFailureKind::Backup, "backup" },
            { AdapterDeploymentExecutionFailureKind::Copy, "copy" },
            { AdapterDeploymentExecutionFailureKind::Replace, "replace" },
            { AdapterDeploymentExecutionFailureKind::Delete, "delete" },
            { AdapterDeploymentExecutionFailureKind::Verification, "verification" },
            { AdapterDeploymentExecutionFailureKind::Rollback, "rollback" },
            { AdapterDeploymentExecutionFailureKind::Restore, "restore" },
            { AdapterDeploymentExecutionFailureKind::Unknown, "unknown" },
        };

        constexpr EnumName<AdapterDeploymentExecutionLogKind> LogKinds[] = {
            { AdapterDeploymentExecutionLogKind::Executor, "executor" },
            { AdapterDeploymentExecutionLogKind::Filesystem, "filesystem" },
            { AdapterDeploymentExecutionLogKind::Backup, "backup" },
            { AdapterDeploymentExecutionLogKind::Verification, "verification" },
            { AdapterDeploymentExecutionLogKind::Rollback, "rollback" },
            { AdapterDeploymentExecutionLogKind::Diagnostic, "diagnostic" },
        };

        constexpr EnumName<AdapterDeploymentExecutionEnvelopeStatus>
            EnvelopeStatuses[] = {
                { AdapterDeploymentExecutionEnvelopeStatus::Accepted, "accepted" },
                { AdapterDeploymentExecutionEnvelopeStatus::WorkOrderNotReady,
                    "work_order_not_ready" },
                { AdapterDeploymentExecutionEnvelopeStatus::ExecutorUnreviewed,
                    "executor_unreviewed" },
                { AdapterDeploymentExecutionEnvelopeStatus::WorkOrderBindingMismatch,
                    "work_order_binding_mismatch" },
                { AdapterDeploymentExecutionEnvelopeStatus::EnvelopeInvalid,
                    "envelope_invalid" },
                { AdapterDeploymentExecutionEnvelopeStatus::StepBindingMismatch,
                    "step_binding_mismatch" },
                { AdapterDeploymentExecutionEnvelopeStatus::BackupBindingMismatch,
                    "backup_binding_mismatch" },
                { AdapterDeploymentExecutionEnvelopeStatus::VerificationBindingMismatch,
                    "verification_binding_mismatch" },
                { AdapterDeploymentExecutionEnvelopeStatus::RollbackBindingMismatch,
                    "rollback_binding_mismatch" },
                { AdapterDeploymentExecutionEnvelopeStatus::FailureLogBindingMismatch,
                    "failure_log_binding_mismatch" },
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

        bool HasStableUniqueIds(const AZStd::vector<AZStd::string>& values)
        {
            if (values.empty())
            {
                return false;
            }
            AZStd::vector<AZStd::string> sorted = values;
            for (const AZStd::string& value : sorted)
            {
                if (!IsStableContractId(value))
                {
                    return false;
                }
            }
            AZStd::sort(sorted.begin(), sorted.end());
            return AZStd::adjacent_find(sorted.begin(), sorted.end())
                == sorted.end();
        }

        bool HasIntrinsicShape(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AZStd::string& error)
        {
            if (envelope.m_contractVersion != 1
                || !IsStableContractId(envelope.m_resultId)
                || !IsStableContractId(envelope.m_workOrderId)
                || envelope.m_workOrderCanonicalJson.empty()
                || !IsSha256Fingerprint(envelope.m_workOrderFingerprint)
                || !CanonicalSha256Matches(
                    envelope.m_workOrderCanonicalJson,
                    envelope.m_workOrderFingerprint)
                || !IsStableContractId(envelope.m_previewId)
                || !IsSha256Fingerprint(envelope.m_previewFingerprint)
                || !IsStableContractId(envelope.m_packId)
                || !IsStableContractId(envelope.m_targetInventoryId)
                || !IsStableContractId(envelope.m_profileId)
                || envelope.m_gameVersion.empty()
                || envelope.m_branch.empty()
                || !IsSupportedRuntimeTarget(envelope.m_runtimeTarget)
                || !IsStrictUtcTimestamp(envelope.m_capturedAtUtc)
                || !DeploymentExecutionResultFingerprintMatches(envelope))
            {
                error = "Deployment execution-result intrinsic shape, canonical hashes, context, or UTC capture time is invalid.";
                return false;
            }

            const AdapterDeploymentExecutorReview& review = envelope.m_executorReview;
            if (!IsStableContractId(review.m_reviewId)
                || !IsStableContractId(review.m_executorId)
                || !IsStrictSemanticVersion(review.m_executorVersion)
                || !IsSha256Fingerprint(review.m_executorFingerprint)
                || review.m_decision
                    != AdapterDeploymentExecutorReviewDecision::Accepted
                || review.m_reviewer.empty()
                || !HasStableUniqueIds(review.m_evidenceIds)
                || !IsStrictUtcTimestamp(review.m_reviewedAtUtc)
                || review.m_reviewedAtUtc > envelope.m_capturedAtUtc)
            {
                error = "Deployment executor review is not exact, accepted, evidence-backed, and time-bound.";
                return false;
            }
            for (const AdapterDeploymentExecutionFailure& failure : envelope.m_failures)
            {
                if (failure.m_kind == AdapterDeploymentExecutionFailureKind::Unknown)
                {
                    error = "Unknown deployment failure classifications are not accepted.";
                    return false;
                }
            }
            return true;
        }

        void SetError(AZStd::string* error, AZStd::string value)
        {
            if (error)
            {
                *error = AZStd::move(value);
            }
        }
    } // namespace

    AZStd::string ToString(AdapterDeploymentExecutorReviewDecision decision)
    {
        return EnumToString(decision, ExecutorReviewDecisions);
    }

    AZStd::string ToString(AdapterDeploymentExecutionOutcome outcome)
    {
        return EnumToString(outcome, ExecutionOutcomes);
    }

    AZStd::string ToString(AdapterDeploymentVerificationStatus status)
    {
        return EnumToString(status, VerificationStatuses);
    }

    AZStd::string ToString(AdapterDeploymentExecutionFailureKind kind)
    {
        return EnumToString(kind, FailureKinds);
    }

    AZStd::string ToString(AdapterDeploymentExecutionLogKind kind)
    {
        return EnumToString(kind, LogKinds);
    }

    AZStd::string ToString(AdapterDeploymentExecutionEnvelopeStatus status)
    {
        return EnumToString(status, EnvelopeStatuses);
    }

    bool TryParseAdapterDeploymentExecutorReviewDecision(
        const AZStd::string& value,
        AdapterDeploymentExecutorReviewDecision& decision)
    {
        return TryParseEnum(value, decision, ExecutorReviewDecisions);
    }

    bool TryParseAdapterDeploymentExecutionOutcome(
        const AZStd::string& value,
        AdapterDeploymentExecutionOutcome& outcome)
    {
        return TryParseEnum(value, outcome, ExecutionOutcomes);
    }

    bool TryParseAdapterDeploymentVerificationStatus(
        const AZStd::string& value,
        AdapterDeploymentVerificationStatus& status)
    {
        return TryParseEnum(value, status, VerificationStatuses);
    }

    bool TryParseAdapterDeploymentExecutionFailureKind(
        const AZStd::string& value,
        AdapterDeploymentExecutionFailureKind& kind)
    {
        if (value == "unknown")
        {
            return false;
        }
        return TryParseEnum(value, kind, FailureKinds);
    }

    bool TryParseAdapterDeploymentExecutionLogKind(
        const AZStd::string& value,
        AdapterDeploymentExecutionLogKind& kind)
    {
        return TryParseEnum(value, kind, LogKinds);
    }

    bool TryParseAdapterDeploymentExecutionEnvelopeStatus(
        const AZStd::string& value,
        AdapterDeploymentExecutionEnvelopeStatus& status)
    {
        return TryParseEnum(value, status, EnvelopeStatuses);
    }

    bool IsAdapterDeploymentExecutionStableId(const AZStd::string& value)
    {
        return IsStableContractId(value);
    }

    bool IsAdapterDeploymentExecutionFingerprint(const AZStd::string& value)
    {
        return IsSha256Fingerprint(value);
    }

    bool IsAdapterDeploymentExecutionLogReference(const AZStd::string& value)
    {
        if (value.empty()
            || value.size() > 1024
            || value.front() == '/'
            || value.front() == '\\'
            || (value.size() > 1 && value[1] == ':'))
        {
            return false;
        }
        size_t start = 0;
        while (start <= value.size())
        {
            const size_t end = value.find('/', start);
            const size_t length = end == AZStd::string::npos
                ? value.size() - start
                : end - start;
            if (length == 0)
            {
                return false;
            }
            const AZStd::string component = value.substr(start, length);
            if (component == "." || component == "..")
            {
                return false;
            }
            for (char character : component)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (byte <= 0x20
                    || byte == 0x7f
                    || character == '\\'
                    || character == ':')
                {
                    return false;
                }
            }
            if (end == AZStd::string::npos)
            {
                break;
            }
            start = end + 1;
        }
        return true;
    }

    bool IsAdapterDeploymentExecutionUtcTimestamp(const AZStd::string& value)
    {
        return IsStrictUtcTimestamp(value);
    }

    AdapterDeploymentExecutionResultRegistry&
    AdapterDeploymentExecutionResultRegistry::Get()
    {
        static AdapterDeploymentExecutionResultRegistry registry;
        return registry;
    }

    bool AdapterDeploymentExecutionResultRegistry::RegisterEnvelope(
        const AdapterDeploymentExecutionResultEnvelope&,
        AZStd::string* error)
    {
        SetError(
            error,
            "Unbound deployment execution-result registration is prohibited; supply the exact current work order.");
        return false;
    }

    bool AdapterDeploymentExecutionResultRegistry::RegisterEnvelope(
        const AdapterDeploymentWorkOrder& workOrder,
        const AdapterDeploymentExecutionResultEnvelope& envelope,
        AZStd::string* error)
    {
        AZStd::string validationError;
        if (!HasIntrinsicShape(envelope, validationError))
        {
            SetError(error, AZStd::move(validationError));
            return false;
        }

        AdapterDeploymentExecutionEvidenceService service;
        const AdapterDeploymentExecutionEvidenceReturn result =
            service.BuildEvidenceReturn(workOrder, envelope);
        if (!result.m_accepted)
        {
            AZStd::string message =
                "Deployment execution-result contract rejected with status "
                + ToString(result.m_status) + ".";
            if (!result.m_issues.empty())
            {
                message += " " + result.m_issues.front().m_code + ": "
                    + result.m_issues.front().m_message;
            }
            SetError(error, AZStd::move(message));
            return false;
        }

        for (const AdapterDeploymentExecutionResultEnvelope& existing : m_envelopes)
        {
            if (existing.m_resultId == envelope.m_resultId)
            {
                SetError(
                    error,
                    "A deployment execution-result envelope with this result identity already exists.");
                return false;
            }
            if (existing.m_profileId == envelope.m_profileId
                && existing.m_resultFingerprint == envelope.m_resultFingerprint)
            {
                SetError(
                    error,
                    "A deployment execution-result fingerprint is already registered for this profile.");
                return false;
            }
        }
        m_envelopes.push_back(envelope);
        AZStd::sort(
            m_envelopes.begin(),
            m_envelopes.end(),
            [](const AdapterDeploymentExecutionResultEnvelope& left,
                const AdapterDeploymentExecutionResultEnvelope& right)
            {
                return left.m_resultId < right.m_resultId;
            });
        if (error)
        {
            error->clear();
        }
        return true;
    }

    void AdapterDeploymentExecutionResultRegistry::Clear()
    {
        m_envelopes.clear();
    }

    const AdapterDeploymentExecutionResultEnvelope*
    AdapterDeploymentExecutionResultRegistry::FindByResultId(
        const AZStd::string& resultId) const
    {
        for (const AdapterDeploymentExecutionResultEnvelope& envelope :
            m_envelopes)
        {
            if (envelope.m_resultId == resultId)
            {
                return &envelope;
            }
        }
        return nullptr;
    }

    const AZStd::vector<AdapterDeploymentExecutionResultEnvelope>&
    AdapterDeploymentExecutionResultRegistry::GetEnvelopes() const
    {
        return m_envelopes;
    }
} // namespace TaintedGrailModdingSDK
