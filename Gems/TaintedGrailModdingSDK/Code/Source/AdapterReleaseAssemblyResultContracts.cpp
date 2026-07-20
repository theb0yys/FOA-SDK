/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseAssemblyResultContracts.h"

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

        constexpr EnumName<AdapterReleaseAssemblerReviewDecision>
            ReviewDecisions[] = {
                { AdapterReleaseAssemblerReviewDecision::Unknown, "unknown" },
                { AdapterReleaseAssemblerReviewDecision::Accepted, "accepted" },
                { AdapterReleaseAssemblerReviewDecision::Rejected, "rejected" },
            };
        constexpr EnumName<AdapterReleaseAssemblerCapability> Capabilities[] = {
            { AdapterReleaseAssemblerCapability::ContentChecksum,
              "content_checksum" },
            { AdapterReleaseAssemblerCapability::ArchiveAssembly,
              "archive_assembly" },
            { AdapterReleaseAssemblerCapability::ArchiveChecksum,
              "archive_checksum" },
        };
        constexpr EnumName<AdapterReleaseAssemblyOutcome> AssemblyOutcomes[] = {
            { AdapterReleaseAssemblyOutcome::NotAttempted, "not_attempted" },
            { AdapterReleaseAssemblyOutcome::Succeeded, "succeeded" },
            { AdapterReleaseAssemblyOutcome::Failed, "failed" },
            { AdapterReleaseAssemblyOutcome::Skipped, "skipped" },
        };
        constexpr EnumName<AdapterReleaseChecksumObservationOutcome>
            ChecksumOutcomes[] = {
                { AdapterReleaseChecksumObservationOutcome::NotObserved,
                  "not_observed" },
                { AdapterReleaseChecksumObservationOutcome::Matched, "matched" },
                { AdapterReleaseChecksumObservationOutcome::Mismatched,
                  "mismatched" },
                { AdapterReleaseChecksumObservationOutcome::Failed, "failed" },
                { AdapterReleaseChecksumObservationOutcome::Inconclusive,
                  "inconclusive" },
            };
        constexpr EnumName<AdapterReleaseAssemblyFailureKind> FailureKinds[] = {
            { AdapterReleaseAssemblyFailureKind::Contract, "contract" },
            { AdapterReleaseAssemblyFailureKind::InputBinding, "input_binding" },
            { AdapterReleaseAssemblyFailureKind::ContentUnavailable,
              "content_unavailable" },
            { AdapterReleaseAssemblyFailureKind::AccessDenied, "access_denied" },
            { AdapterReleaseAssemblyFailureKind::UnsupportedArchive,
              "unsupported_archive" },
            { AdapterReleaseAssemblyFailureKind::Read, "read" },
            { AdapterReleaseAssemblyFailureKind::Hash, "hash" },
            { AdapterReleaseAssemblyFailureKind::Archive, "archive" },
            { AdapterReleaseAssemblyFailureKind::Internal, "internal" },
            { AdapterReleaseAssemblyFailureKind::Unknown, "unknown" },
        };
        constexpr EnumName<AdapterReleaseAssemblyDiagnosticKind>
            DiagnosticKinds[] = {
                { AdapterReleaseAssemblyDiagnosticKind::Assembler, "assembler" },
                { AdapterReleaseAssemblyDiagnosticKind::Filesystem, "filesystem" },
                { AdapterReleaseAssemblyDiagnosticKind::Hash, "hash" },
                { AdapterReleaseAssemblyDiagnosticKind::Archive, "archive" },
                { AdapterReleaseAssemblyDiagnosticKind::Binding, "binding" },
                { AdapterReleaseAssemblyDiagnosticKind::Diagnostic, "diagnostic" },
            };
        constexpr EnumName<AdapterReleaseAssemblyEnvelopeStatus>
            EnvelopeStatuses[] = {
                { AdapterReleaseAssemblyEnvelopeStatus::Accepted, "accepted" },
                { AdapterReleaseAssemblyEnvelopeStatus::ArtifactNotReady,
                  "artifact_not_ready" },
                { AdapterReleaseAssemblyEnvelopeStatus::AssemblerUnreviewed,
                  "assembler_unreviewed" },
                { AdapterReleaseAssemblyEnvelopeStatus::ArtifactBindingMismatch,
                  "artifact_binding_mismatch" },
                { AdapterReleaseAssemblyEnvelopeStatus::EnvelopeInvalid,
                  "envelope_invalid" },
                { AdapterReleaseAssemblyEnvelopeStatus::ContentCoverageIncomplete,
                  "content_coverage_incomplete" },
                { AdapterReleaseAssemblyEnvelopeStatus::ChecksumObservationMismatch,
                  "checksum_observation_mismatch" },
                { AdapterReleaseAssemblyEnvelopeStatus::ArchiveBindingMismatch,
                  "archive_binding_mismatch" },
                { AdapterReleaseAssemblyEnvelopeStatus::FailureDiagnosticBindingMismatch,
                  "failure_diagnostic_binding_mismatch" },
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

    AZStd::string ToString(AdapterReleaseAssemblerReviewDecision decision)
    {
        return EnumToString(decision, ReviewDecisions);
    }

    AZStd::string ToString(AdapterReleaseAssemblerCapability capability)
    {
        return EnumToString(capability, Capabilities);
    }

    AZStd::string ToString(AdapterReleaseAssemblyOutcome outcome)
    {
        return EnumToString(outcome, AssemblyOutcomes);
    }

    AZStd::string ToString(AdapterReleaseChecksumObservationOutcome outcome)
    {
        return EnumToString(outcome, ChecksumOutcomes);
    }

    AZStd::string ToString(AdapterReleaseAssemblyFailureKind kind)
    {
        return EnumToString(kind, FailureKinds);
    }

    AZStd::string ToString(AdapterReleaseAssemblyDiagnosticKind kind)
    {
        return EnumToString(kind, DiagnosticKinds);
    }

    AZStd::string ToString(AdapterReleaseAssemblyEnvelopeStatus status)
    {
        return EnumToString(status, EnvelopeStatuses);
    }

    bool TryParseAdapterReleaseAssemblerReviewDecision(
        const AZStd::string& value,
        AdapterReleaseAssemblerReviewDecision& decision)
    {
        return TryParseEnum(value, decision, ReviewDecisions);
    }

    bool TryParseAdapterReleaseAssemblerCapability(
        const AZStd::string& value,
        AdapterReleaseAssemblerCapability& capability)
    {
        return TryParseEnum(value, capability, Capabilities);
    }

    bool TryParseAdapterReleaseAssemblyOutcome(
        const AZStd::string& value,
        AdapterReleaseAssemblyOutcome& outcome)
    {
        return TryParseEnum(value, outcome, AssemblyOutcomes);
    }

    bool TryParseAdapterReleaseChecksumObservationOutcome(
        const AZStd::string& value,
        AdapterReleaseChecksumObservationOutcome& outcome)
    {
        return TryParseEnum(value, outcome, ChecksumOutcomes);
    }

    bool TryParseAdapterReleaseAssemblyFailureKind(
        const AZStd::string& value,
        AdapterReleaseAssemblyFailureKind& kind)
    {
        return TryParseEnum(value, kind, FailureKinds);
    }

    bool TryParseAdapterReleaseAssemblyDiagnosticKind(
        const AZStd::string& value,
        AdapterReleaseAssemblyDiagnosticKind& kind)
    {
        return TryParseEnum(value, kind, DiagnosticKinds);
    }

    bool TryParseAdapterReleaseAssemblyEnvelopeStatus(
        const AZStd::string& value,
        AdapterReleaseAssemblyEnvelopeStatus& status)
    {
        return TryParseEnum(value, status, EnvelopeStatuses);
    }

    bool IsAdapterReleaseAssemblyStableId(const AZStd::string& value)
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

    bool IsAdapterReleaseAssemblyFingerprint(const AZStd::string& value)
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

    bool IsAdapterReleaseAssemblySafeReference(const AZStd::string& value)
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

    bool IsAdapterReleaseAssemblyUtcTimestamp(const AZStd::string& value)
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

    AdapterReleaseAssemblyResultRegistry&
    AdapterReleaseAssemblyResultRegistry::Get()
    {
        static AdapterReleaseAssemblyResultRegistry registry;
        return registry;
    }

    bool AdapterReleaseAssemblyResultRegistry::RegisterEnvelope(
        const AdapterReleaseAssemblyResultEnvelope& envelope,
        AZStd::string* error)
    {
        if (!IsAdapterReleaseAssemblyStableId(envelope.m_resultId)
            || !IsAdapterReleaseAssemblyStableId(envelope.m_artifactId)
            || !IsAdapterReleaseAssemblyFingerprint(
                envelope.m_artifactFingerprint)
            || !IsAdapterReleaseAssemblyFingerprint(envelope.m_resultFingerprint))
        {
            if (error)
            {
                *error =
                    "Release assembly-result registration requires stable result and "
                    "artifact identities plus exact artifact/result fingerprints.";
            }
            return false;
        }
        for (const AdapterReleaseAssemblyResultEnvelope& existing : m_envelopes)
        {
            if (existing.m_resultId == envelope.m_resultId)
            {
                if (error)
                {
                    *error =
                        "A release assembly-result envelope with this result identity "
                        "already exists.";
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

    void AdapterReleaseAssemblyResultRegistry::Clear()
    {
        m_envelopes.clear();
    }

    const AdapterReleaseAssemblyResultEnvelope*
    AdapterReleaseAssemblyResultRegistry::FindByResultId(
        const AZStd::string& resultId) const
    {
        for (const AdapterReleaseAssemblyResultEnvelope& envelope : m_envelopes)
        {
            if (envelope.m_resultId == resultId)
            {
                return &envelope;
            }
        }
        return nullptr;
    }

    const AZStd::vector<AdapterReleaseAssemblyResultEnvelope>&
    AdapterReleaseAssemblyResultRegistry::GetEnvelopes() const
    {
        return m_envelopes;
    }
} // namespace TaintedGrailModdingSDK
