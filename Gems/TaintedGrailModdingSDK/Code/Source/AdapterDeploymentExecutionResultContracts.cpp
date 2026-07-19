/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentExecutionResultContracts.h"

#include <AzCore/std/algorithm.h>

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

        bool IsLowerHex(char value)
        {
            return (value >= '0' && value <= '9')
                || (value >= 'a' && value <= 'f');
        }

        bool IsDigit(char value)
        {
            return value >= '0' && value <= '9';
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
        if (value.size() < 3)
        {
            return false;
        }
        bool hasNamespaceSeparator = false;
        for (char character : value)
        {
            const bool allowed =
                (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9')
                || character == '.'
                || character == '-'
                || character == '_'
                || character == ':';
            if (!allowed)
            {
                return false;
            }
            hasNamespaceSeparator = hasNamespaceSeparator
                || character == '.'
                || character == ':';
        }
        const char first = value.front();
        const char last = value.back();
        const bool firstAllowed =
            (first >= 'a' && first <= 'z') || (first >= '0' && first <= '9');
        const bool lastAllowed =
            (last >= 'a' && last <= 'z') || (last >= '0' && last <= '9');
        return hasNamespaceSeparator && firstAllowed && lastAllowed;
    }

    bool IsAdapterDeploymentExecutionFingerprint(const AZStd::string& value)
    {
        constexpr size_t PrefixLength = 7;
        constexpr size_t DigestLength = 64;
        if (value.size() != PrefixLength + DigestLength
            || value.substr(0, PrefixLength) != "sha256:")
        {
            return false;
        }
        for (size_t index = PrefixLength; index < value.size(); ++index)
        {
            if (!IsLowerHex(value[index]))
            {
                return false;
            }
        }
        return true;
    }

    bool IsAdapterDeploymentExecutionLogReference(const AZStd::string& value)
    {
        if (value.empty() || value.front() == '/' || value.front() == '\\')
        {
            return false;
        }
        if (value.size() > 1 && value[1] == ':')
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
        if (value.size() != 20
            || value[4] != '-'
            || value[7] != '-'
            || value[10] != 'T'
            || value[13] != ':'
            || value[16] != ':'
            || value[19] != 'Z')
        {
            return false;
        }
        constexpr size_t DigitPositions[] = {
            0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18,
        };
        for (size_t position : DigitPositions)
        {
            if (!IsDigit(value[position]))
            {
                return false;
            }
        }
        return value.substr(5, 2) >= "01"
            && value.substr(5, 2) <= "12"
            && value.substr(8, 2) >= "01"
            && value.substr(8, 2) <= "31"
            && value.substr(11, 2) <= "23"
            && value.substr(14, 2) <= "59"
            && value.substr(17, 2) <= "59";
    }

    AdapterDeploymentExecutionResultRegistry&
    AdapterDeploymentExecutionResultRegistry::Get()
    {
        static AdapterDeploymentExecutionResultRegistry registry;
        return registry;
    }

    bool AdapterDeploymentExecutionResultRegistry::RegisterEnvelope(
        const AdapterDeploymentExecutionResultEnvelope& envelope,
        AZStd::string* error)
    {
        if (!IsAdapterDeploymentExecutionStableId(envelope.m_resultId)
            || envelope.m_workOrderId.empty()
            || !IsAdapterDeploymentExecutionFingerprint(
                envelope.m_workOrderFingerprint)
            || !IsAdapterDeploymentExecutionFingerprint(
                envelope.m_resultFingerprint))
        {
            if (error)
            {
                *error =
                    "Deployment execution-result registration requires stable result "
                    "identity and exact work-order/result fingerprints.";
            }
            return false;
        }
        for (const AdapterDeploymentExecutionResultEnvelope& existing :
            m_envelopes)
        {
            if (existing.m_resultId == envelope.m_resultId)
            {
                if (error)
                {
                    *error =
                        "A deployment execution-result envelope with this result "
                        "identity already exists.";
                }
                return false;
            }
        }
        m_envelopes.push_back(envelope);
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
