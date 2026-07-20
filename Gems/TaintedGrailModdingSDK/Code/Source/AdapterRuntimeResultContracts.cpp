/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterRuntimeResultContracts.h"

#include "AdapterRuntimeResultCanonical.h"
#include "CanonicalFingerprint.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

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

        constexpr EnumName<AdapterRuntimeOutcome> Outcomes[] = {
            { AdapterRuntimeOutcome::NotAttempted, "not_attempted" },
            { AdapterRuntimeOutcome::Succeeded, "succeeded" },
            { AdapterRuntimeOutcome::Failed, "failed" },
            { AdapterRuntimeOutcome::Skipped, "skipped" },
        };

        constexpr EnumName<AdapterRuntimeFailureKind> FailureKinds[] = {
            { AdapterRuntimeFailureKind::Contract, "contract" },
            { AdapterRuntimeFailureKind::Capability, "capability" },
            { AdapterRuntimeFailureKind::Runtime, "runtime" },
            { AdapterRuntimeFailureKind::Persistence, "persistence" },
            { AdapterRuntimeFailureKind::Cleanup, "cleanup" },
            { AdapterRuntimeFailureKind::Rollback, "rollback" },
            { AdapterRuntimeFailureKind::Unknown, "unknown" },
        };

        constexpr EnumName<AdapterRuntimeLogKind> LogKinds[] = {
            { AdapterRuntimeLogKind::Adapter, "adapter" },
            { AdapterRuntimeLogKind::Game, "game" },
            { AdapterRuntimeLogKind::Diagnostic, "diagnostic" },
            { AdapterRuntimeLogKind::Cleanup, "cleanup" },
            { AdapterRuntimeLogKind::Rollback, "rollback" },
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

        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        bool HasDuplicate(const AZStd::vector<AZStd::string>& values)
        {
            AZStd::vector<AZStd::string> sorted = values;
            AZStd::sort(sorted.begin(), sorted.end());
            return AZStd::adjacent_find(sorted.begin(), sorted.end())
                != sorted.end();
        }

        bool IsAsciiAlphaNumeric(char value)
        {
            return (value >= 'a' && value <= 'z')
                || (value >= 'A' && value <= 'Z')
                || (value >= '0' && value <= '9');
        }

        bool IsOpaqueIdentity(const AZStd::string& value)
        {
            if (value.empty() || value.size() > 512)
            {
                return false;
            }
            for (char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (byte <= 0x20 || byte == 0x7f)
                {
                    return false;
                }
            }
            return true;
        }

        bool IsLowerToken(const AZStd::string& value)
        {
            if (value.empty() || value.size() > 160)
            {
                return false;
            }
            for (char character : value)
            {
                if (!((character >= 'a' && character <= 'z')
                        || (character >= '0' && character <= '9')
                        || character == '.'
                        || character == '-'
                        || character == '_'))
                {
                    return false;
                }
            }
            return true;
        }

        bool OutcomeIsAttempted(AdapterRuntimeOutcome outcome)
        {
            return outcome == AdapterRuntimeOutcome::Succeeded
                || outcome == AdapterRuntimeOutcome::Failed;
        }

        bool OutcomeRequiresFailure(AdapterRuntimeOutcome outcome)
        {
            return outcome == AdapterRuntimeOutcome::Failed
                || outcome == AdapterRuntimeOutcome::Skipped;
        }

        bool ValidateOutcomeShape(
            AdapterRuntimeOutcome outcome,
            bool attempted,
            const AZStd::vector<AZStd::string>& failureIds,
            const AZStd::string& outputFingerprint,
            AZStd::string& error)
        {
            if (attempted != OutcomeIsAttempted(outcome))
            {
                error = "Attempted must be true only for succeeded or failed outcomes.";
                return false;
            }
            if (OutcomeRequiresFailure(outcome) != !failureIds.empty())
            {
                error = "Failed and skipped outcomes require failures; succeeded and not_attempted do not.";
                return false;
            }
            if (outcome == AdapterRuntimeOutcome::Succeeded
                && !IsSha256Fingerprint(outputFingerprint))
            {
                error = "Succeeded step results require a lowercase SHA-256 output fingerprint.";
                return false;
            }
            if (!outputFingerprint.empty()
                && !IsSha256Fingerprint(outputFingerprint))
            {
                error = "Step output fingerprints must use lowercase sha256:<64 hex>.";
                return false;
            }
            if (HasDuplicate(failureIds))
            {
                error = "Step failure references must be unique.";
                return false;
            }
            return true;
        }

        bool ValidateRecoveryShape(
            const AdapterRuntimeRecoveryResult& recovery,
            AZStd::string& error)
        {
            if (!IsOpaqueIdentity(recovery.m_stepId))
            {
                error = "Cleanup and rollback results require an exact step identity.";
                return false;
            }
            return ValidateOutcomeShape(
                recovery.m_outcome,
                OutcomeIsAttempted(recovery.m_outcome),
                recovery.m_failureIds,
                recovery.m_outputFingerprint,
                error)
                && !HasDuplicate(recovery.m_logReferenceIds);
        }

        bool ValidateEnvelopeShape(
            const AdapterRuntimeResultEnvelope& envelope,
            AZStd::string& error)
        {
            if (envelope.m_contractVersion != 1)
            {
                error = "Adapter runtime result contract version must be 1.";
                return false;
            }
            if (!IsStableContractId(envelope.m_resultId)
                || !IsOpaqueIdentity(envelope.m_planId)
                || envelope.m_planCanonicalJson.empty())
            {
                error = "Stable result identity, exact plan identity, and canonical plan JSON are required.";
                return false;
            }
            if (!IsSha256Fingerprint(envelope.m_planFingerprint)
                || !IsSha256Fingerprint(envelope.m_resultFingerprint))
            {
                error = "Plan and result fingerprints must use lowercase sha256:<64 hex>.";
                return false;
            }
            if (!CanonicalSha256Matches(
                    envelope.m_planCanonicalJson,
                    envelope.m_planFingerprint))
            {
                error = "Plan fingerprint must be the SHA-256 of the exact canonical plan JSON.";
                return false;
            }
            if (!IsStableContractId(envelope.m_packId)
                || !IsStrictSemanticVersion(envelope.m_packVersion)
                || !IsStableContractId(envelope.m_adapterId)
                || !IsStrictSemanticVersion(envelope.m_adapterVersion)
                || !IsStableContractId(envelope.m_profileId)
                || envelope.m_gameVersion.empty()
                || envelope.m_branch.empty()
                || !IsSupportedRuntimeTarget(envelope.m_runtimeTarget)
                || !IsStrictUtcTimestamp(envelope.m_capturedAt))
            {
                error = "Pack, adapter, profile, strict versions, supported runtime target, and a real UTC capture time are required.";
                return false;
            }
            if (envelope.m_stepResults.empty())
            {
                error = "At least one attempted-plan step result is required.";
                return false;
            }

            AZStd::vector<AZStd::string> stepIds;
            AZStd::vector<AZ::u64> sequences;
            for (const AdapterRuntimeStepResult& step : envelope.m_stepResults)
            {
                if (!IsOpaqueIdentity(step.m_stepId)
                    || step.m_sequence == 0
                    || step.m_capability.empty()
                    || step.m_subjectKind.empty()
                    || step.m_subjectId.empty()
                    || step.m_subjectRef.empty())
                {
                    error = "Every step result requires exact identity, sequence, capability, and subject binding.";
                    return false;
                }
                AZStd::string outcomeError;
                if (!ValidateOutcomeShape(
                        step.m_outcome,
                        step.m_attempted,
                        step.m_failureIds,
                        step.m_outputFingerprint,
                        outcomeError))
                {
                    error = step.m_stepId + ": " + outcomeError;
                    return false;
                }
                if (HasDuplicate(step.m_logReferenceIds))
                {
                    error = step.m_stepId + ": log references must be unique.";
                    return false;
                }
                stepIds.push_back(step.m_stepId);
                sequences.push_back(step.m_sequence);
            }
            if (HasDuplicate(stepIds))
            {
                error = "Attempted step identities must be unique.";
                return false;
            }
            AZStd::sort(sequences.begin(), sequences.end());
            if (AZStd::adjacent_find(sequences.begin(), sequences.end())
                != sequences.end())
            {
                error = "Attempted step sequences must be unique.";
                return false;
            }

            AZStd::vector<AZStd::string> failureIds;
            for (const AdapterRuntimeFailure& failure : envelope.m_failures)
            {
                if (!IsStableContractId(failure.m_failureId)
                    || failure.m_kind == AdapterRuntimeFailureKind::Unknown
                    || !IsLowerToken(failure.m_code)
                    || failure.m_message.empty()
                    || (!failure.m_stepId.empty()
                        && !Contains(stepIds, failure.m_stepId))
                    || HasDuplicate(failure.m_logReferenceIds))
                {
                    error = "Failures require stable identity, a known kind, lowercase code, message, exact step binding, and unique logs.";
                    return false;
                }
                failureIds.push_back(failure.m_failureId);
            }
            if (HasDuplicate(failureIds))
            {
                error = "Failure identities must be unique.";
                return false;
            }

            AZStd::vector<AZStd::string> logIds;
            for (const AdapterRuntimeLogReference& log : envelope.m_logReferences)
            {
                if (!IsStableContractId(log.m_logId)
                    || !IsAdapterRuntimeLogReference(log.m_reference)
                    || !IsSha256Fingerprint(log.m_fingerprint)
                    || HasDuplicate(log.m_stepIds))
                {
                    error = "Log references require stable IDs, safe relative locators, SHA-256 fingerprints, and unique step references.";
                    return false;
                }
                for (const AZStd::string& stepId : log.m_stepIds)
                {
                    if (!Contains(stepIds, stepId))
                    {
                        error = log.m_logId + ": log references an unknown step.";
                        return false;
                    }
                }
                logIds.push_back(log.m_logId);
            }
            if (HasDuplicate(logIds))
            {
                error = "Log identities must be unique.";
                return false;
            }

            for (const AdapterRuntimeStepResult& step : envelope.m_stepResults)
            {
                for (const AZStd::string& failureId : step.m_failureIds)
                {
                    if (!Contains(failureIds, failureId))
                    {
                        error = step.m_stepId + ": step references an unknown failure.";
                        return false;
                    }
                }
                for (const AZStd::string& logId : step.m_logReferenceIds)
                {
                    if (!Contains(logIds, logId))
                    {
                        error = step.m_stepId + ": step references an unknown log.";
                        return false;
                    }
                }
            }
            for (const AdapterRuntimeFailure& failure : envelope.m_failures)
            {
                for (const AZStd::string& logId : failure.m_logReferenceIds)
                {
                    if (!Contains(logIds, logId))
                    {
                        error = failure.m_failureId + ": failure references an unknown log.";
                        return false;
                    }
                }
            }

            if (!ValidateRecoveryShape(envelope.m_cleanupResult, error)
                || !ValidateRecoveryShape(envelope.m_rollbackResult, error))
            {
                return false;
            }
            for (const AdapterRuntimeRecoveryResult* recovery : {
                     &envelope.m_cleanupResult,
                     &envelope.m_rollbackResult })
            {
                if (!Contains(stepIds, recovery->m_stepId))
                {
                    error = "Cleanup or rollback summary references an unknown step.";
                    return false;
                }
                for (const AZStd::string& failureId : recovery->m_failureIds)
                {
                    if (!Contains(failureIds, failureId))
                    {
                        error = "Cleanup or rollback summary references an unknown failure.";
                        return false;
                    }
                }
                for (const AZStd::string& logId : recovery->m_logReferenceIds)
                {
                    if (!Contains(logIds, logId))
                    {
                        error = "Cleanup or rollback summary references an unknown log.";
                        return false;
                    }
                }
            }

            if (!RuntimeResultFingerprintMatches(envelope))
            {
                error = "Result fingerprint must be the SHA-256 of the deterministic canonical runtime-result payload.";
                return false;
            }
            return true;
        }
    } // namespace

    AZStd::string ToString(AdapterRuntimeOutcome outcome)
    {
        return EnumToString(outcome, Outcomes);
    }

    AZStd::string ToString(AdapterRuntimeFailureKind kind)
    {
        return EnumToString(kind, FailureKinds);
    }

    AZStd::string ToString(AdapterRuntimeLogKind kind)
    {
        return EnumToString(kind, LogKinds);
    }

    bool TryParseAdapterRuntimeOutcome(
        const AZStd::string& value,
        AdapterRuntimeOutcome& outcome)
    {
        return TryParseEnum(value, outcome, Outcomes);
    }

    bool TryParseAdapterRuntimeFailureKind(
        const AZStd::string& value,
        AdapterRuntimeFailureKind& kind)
    {
        if (value == "unknown")
        {
            return false;
        }
        return TryParseEnum(value, kind, FailureKinds);
    }

    bool TryParseAdapterRuntimeLogKind(
        const AZStd::string& value,
        AdapterRuntimeLogKind& kind)
    {
        return TryParseEnum(value, kind, LogKinds);
    }

    bool IsAdapterRuntimeStableId(const AZStd::string& value)
    {
        return IsStableContractId(value);
    }

    bool IsAdapterRuntimeFingerprint(const AZStd::string& value)
    {
        return IsSha256Fingerprint(value);
    }

    bool IsAdapterRuntimeLogReference(const AZStd::string& value)
    {
        if (value.empty()
            || value.size() > 1024
            || value.front() == '/'
            || value.front() == '\\'
            || value.find("..") != AZStd::string::npos
            || (value.size() > 1 && value[1] == ':'))
        {
            return false;
        }
        for (char character : value)
        {
            if (!IsAsciiAlphaNumeric(character)
                && character != '.'
                && character != '-'
                && character != '_'
                && character != '/')
            {
                return false;
            }
        }
        return true;
    }

    AdapterRuntimeResultRegistry& AdapterRuntimeResultRegistry::Get()
    {
        static AdapterRuntimeResultRegistry registry;
        return registry;
    }

    bool AdapterRuntimeResultRegistry::RegisterEnvelope(
        const AdapterRuntimeResultEnvelope& envelope,
        AZStd::string* error)
    {
        AZStd::string validationError;
        if (!ValidateEnvelopeShape(envelope, validationError))
        {
            if (error)
            {
                *error = AZStd::move(validationError);
            }
            return false;
        }
        for (const AdapterRuntimeResultEnvelope& existing : m_envelopes)
        {
            if (existing.m_resultId == envelope.m_resultId)
            {
                if (error)
                {
                    *error = "Adapter runtime result ID is already registered.";
                }
                return false;
            }
            if (existing.m_profileId == envelope.m_profileId
                && existing.m_resultFingerprint == envelope.m_resultFingerprint)
            {
                if (error)
                {
                    *error = "Adapter runtime result fingerprint is already registered for this profile.";
                }
                return false;
            }
        }
        m_envelopes.push_back(envelope);
        AZStd::sort(
            m_envelopes.begin(),
            m_envelopes.end(),
            [](const AdapterRuntimeResultEnvelope& left,
                const AdapterRuntimeResultEnvelope& right)
            {
                return left.m_resultId < right.m_resultId;
            });
        if (error)
        {
            error->clear();
        }
        return true;
    }

    void AdapterRuntimeResultRegistry::Clear()
    {
        m_envelopes.clear();
    }

    const AdapterRuntimeResultEnvelope*
    AdapterRuntimeResultRegistry::FindByResultId(
        const AZStd::string& resultId) const
    {
        for (const AdapterRuntimeResultEnvelope& envelope : m_envelopes)
        {
            if (envelope.m_resultId == resultId)
            {
                return &envelope;
            }
        }
        return nullptr;
    }

    const AZStd::vector<AdapterRuntimeResultEnvelope>&
    AdapterRuntimeResultRegistry::GetEnvelopes() const
    {
        return m_envelopes;
    }
} // namespace TaintedGrailModdingSDK
