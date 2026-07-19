/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPostDeploymentVerifierContracts.h"

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

        constexpr EnumName<AdapterPostDeploymentVerifierReviewDecision>
            ReviewDecisions[] = {
                { AdapterPostDeploymentVerifierReviewDecision::Unknown, "unknown" },
                { AdapterPostDeploymentVerifierReviewDecision::Accepted, "accepted" },
                { AdapterPostDeploymentVerifierReviewDecision::Rejected, "rejected" },
            };

        constexpr EnumName<AdapterPostDeploymentVerifierCapability> Capabilities[] = {
            { AdapterPostDeploymentVerifierCapability::TargetPresence,
                "target_presence" },
            { AdapterPostDeploymentVerifierCapability::TargetFingerprint,
                "target_fingerprint" },
            { AdapterPostDeploymentVerifierCapability::TargetAbsence,
                "target_absence" },
        };

        constexpr EnumName<AdapterPostDeploymentVerifierCheckOutcome> Outcomes[] = {
            { AdapterPostDeploymentVerifierCheckOutcome::NotRun, "not_run" },
            { AdapterPostDeploymentVerifierCheckOutcome::Matched, "matched" },
            { AdapterPostDeploymentVerifierCheckOutcome::Mismatched, "mismatched" },
            { AdapterPostDeploymentVerifierCheckOutcome::Failed, "failed" },
            { AdapterPostDeploymentVerifierCheckOutcome::Inconclusive,
                "inconclusive" },
        };

        constexpr EnumName<AdapterPostDeploymentVerifierFailureKind> FailureKinds[] = {
            { AdapterPostDeploymentVerifierFailureKind::Contract, "contract" },
            { AdapterPostDeploymentVerifierFailureKind::InputBinding,
                "input_binding" },
            { AdapterPostDeploymentVerifierFailureKind::TargetUnavailable,
                "target_unavailable" },
            { AdapterPostDeploymentVerifierFailureKind::AccessDenied,
                "access_denied" },
            { AdapterPostDeploymentVerifierFailureKind::UnsupportedCheck,
                "unsupported_check" },
            { AdapterPostDeploymentVerifierFailureKind::Read, "read" },
            { AdapterPostDeploymentVerifierFailureKind::Hash, "hash" },
            { AdapterPostDeploymentVerifierFailureKind::Internal, "internal" },
            { AdapterPostDeploymentVerifierFailureKind::Unknown, "unknown" },
        };

        constexpr EnumName<AdapterPostDeploymentVerifierDiagnosticKind>
            DiagnosticKinds[] = {
                { AdapterPostDeploymentVerifierDiagnosticKind::Verifier,
                    "verifier" },
                { AdapterPostDeploymentVerifierDiagnosticKind::Filesystem,
                    "filesystem" },
                { AdapterPostDeploymentVerifierDiagnosticKind::Hash, "hash" },
                { AdapterPostDeploymentVerifierDiagnosticKind::Binding,
                    "binding" },
                { AdapterPostDeploymentVerifierDiagnosticKind::Diagnostic,
                    "diagnostic" },
            };

        constexpr EnumName<AdapterPostDeploymentVerifierEnvelopeStatus>
            EnvelopeStatuses[] = {
                { AdapterPostDeploymentVerifierEnvelopeStatus::Accepted,
                    "accepted" },
                { AdapterPostDeploymentVerifierEnvelopeStatus::ReportNotReady,
                    "report_not_ready" },
                { AdapterPostDeploymentVerifierEnvelopeStatus::VerifierUnreviewed,
                    "verifier_unreviewed" },
                { AdapterPostDeploymentVerifierEnvelopeStatus::ReportBindingMismatch,
                    "report_binding_mismatch" },
                { AdapterPostDeploymentVerifierEnvelopeStatus::EnvelopeInvalid,
                    "envelope_invalid" },
                { AdapterPostDeploymentVerifierEnvelopeStatus::CheckCoverageIncomplete,
                    "check_coverage_incomplete" },
                { AdapterPostDeploymentVerifierEnvelopeStatus::
                        FailureDiagnosticBindingMismatch,
                    "failure_diagnostic_binding_mismatch" },
                { AdapterPostDeploymentVerifierEnvelopeStatus::ObservationMismatch,
                    "observation_mismatch" },
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

    AZStd::string ToString(AdapterPostDeploymentVerifierReviewDecision decision)
    {
        return EnumToString(decision, ReviewDecisions);
    }

    AZStd::string ToString(AdapterPostDeploymentVerifierCapability capability)
    {
        return EnumToString(capability, Capabilities);
    }

    AZStd::string ToString(AdapterPostDeploymentVerifierCheckOutcome outcome)
    {
        return EnumToString(outcome, Outcomes);
    }

    AZStd::string ToString(AdapterPostDeploymentVerifierFailureKind kind)
    {
        return EnumToString(kind, FailureKinds);
    }

    AZStd::string ToString(AdapterPostDeploymentVerifierDiagnosticKind kind)
    {
        return EnumToString(kind, DiagnosticKinds);
    }

    AZStd::string ToString(AdapterPostDeploymentVerifierEnvelopeStatus status)
    {
        return EnumToString(status, EnvelopeStatuses);
    }

    bool TryParseAdapterPostDeploymentVerifierReviewDecision(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierReviewDecision& decision)
    {
        return TryParseEnum(value, decision, ReviewDecisions);
    }

    bool TryParseAdapterPostDeploymentVerifierCapability(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierCapability& capability)
    {
        return TryParseEnum(value, capability, Capabilities);
    }

    bool TryParseAdapterPostDeploymentVerifierCheckOutcome(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierCheckOutcome& outcome)
    {
        return TryParseEnum(value, outcome, Outcomes);
    }

    bool TryParseAdapterPostDeploymentVerifierFailureKind(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierFailureKind& kind)
    {
        return TryParseEnum(value, kind, FailureKinds);
    }

    bool TryParseAdapterPostDeploymentVerifierDiagnosticKind(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierDiagnosticKind& kind)
    {
        return TryParseEnum(value, kind, DiagnosticKinds);
    }

    bool TryParseAdapterPostDeploymentVerifierEnvelopeStatus(
        const AZStd::string& value,
        AdapterPostDeploymentVerifierEnvelopeStatus& status)
    {
        return TryParseEnum(value, status, EnvelopeStatuses);
    }

    bool IsAdapterPostDeploymentVerifierStableId(const AZStd::string& value)
    {
        return IsAdapterDeploymentExecutionStableId(value);
    }

    bool IsAdapterPostDeploymentVerifierFingerprint(const AZStd::string& value)
    {
        return IsAdapterDeploymentExecutionFingerprint(value);
    }

    bool IsAdapterPostDeploymentVerifierDiagnosticReference(
        const AZStd::string& value)
    {
        return IsAdapterDeploymentExecutionLogReference(value);
    }

    bool IsAdapterPostDeploymentVerifierUtcTimestamp(const AZStd::string& value)
    {
        return IsAdapterDeploymentExecutionUtcTimestamp(value);
    }

    AdapterPostDeploymentVerifierResultRegistry&
    AdapterPostDeploymentVerifierResultRegistry::Get()
    {
        static AdapterPostDeploymentVerifierResultRegistry registry;
        return registry;
    }

    bool AdapterPostDeploymentVerifierResultRegistry::RegisterEnvelope(
        const AdapterPostDeploymentVerifierResultEnvelope& envelope,
        AZStd::string* error)
    {
        if (!IsAdapterPostDeploymentVerifierStableId(envelope.m_verifierResultId)
            || !IsAdapterPostDeploymentVerifierStableId(envelope.m_reportId)
            || !IsAdapterPostDeploymentVerifierStableId(envelope.m_resultId)
            || envelope.m_workOrderId.empty()
            || !IsAdapterPostDeploymentVerifierFingerprint(
                envelope.m_workOrderFingerprint)
            || !IsAdapterPostDeploymentVerifierFingerprint(
                envelope.m_executionResultFingerprint)
            || !IsAdapterPostDeploymentVerifierFingerprint(
                envelope.m_resultFingerprint))
        {
            if (error)
            {
                *error =
                    "Post-deployment verifier registration requires stable result, report, "
                    "and execution-result identities plus exact work-order, execution-result, "
                    "and verifier-result fingerprints.";
            }
            return false;
        }

        for (const AdapterPostDeploymentVerifierResultEnvelope& existing :
            m_envelopes)
        {
            if (existing.m_verifierResultId == envelope.m_verifierResultId)
            {
                if (error)
                {
                    *error =
                        "A post-deployment verifier envelope with this result identity "
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

    void AdapterPostDeploymentVerifierResultRegistry::Clear()
    {
        m_envelopes.clear();
    }

    const AdapterPostDeploymentVerifierResultEnvelope*
    AdapterPostDeploymentVerifierResultRegistry::FindByResultId(
        const AZStd::string& verifierResultId) const
    {
        for (const AdapterPostDeploymentVerifierResultEnvelope& envelope :
            m_envelopes)
        {
            if (envelope.m_verifierResultId == verifierResultId)
            {
                return &envelope;
            }
        }
        return nullptr;
    }

    const AZStd::vector<AdapterPostDeploymentVerifierResultEnvelope>&
    AdapterPostDeploymentVerifierResultRegistry::GetEnvelopes() const
    {
        return m_envelopes;
    }
} // namespace TaintedGrailModdingSDK
