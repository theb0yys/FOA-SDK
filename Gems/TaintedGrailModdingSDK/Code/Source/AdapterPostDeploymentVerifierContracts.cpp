/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPostDeploymentVerifierContracts.h"

#include "AdapterPostDeploymentVerifierEvidenceService.h"
#include "AdapterPostDeploymentVerifierResultCanonical.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
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

        void SetError(AZStd::string* error, AZStd::string value)
        {
            if (error)
            {
                *error = AZStd::move(value);
            }
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
        if (value == "unknown")
        {
            return false;
        }
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
        return IsStableContractId(value);
    }

    bool IsAdapterPostDeploymentVerifierFingerprint(const AZStd::string& value)
    {
        return IsSha256Fingerprint(value);
    }

    bool IsAdapterPostDeploymentVerifierDiagnosticReference(
        const AZStd::string& value)
    {
        return IsAdapterDeploymentExecutionLogReference(value);
    }

    bool IsAdapterPostDeploymentVerifierUtcTimestamp(const AZStd::string& value)
    {
        return IsStrictUtcTimestamp(value);
    }

    AdapterPostDeploymentVerifierResultRegistry&
    AdapterPostDeploymentVerifierResultRegistry::Get()
    {
        static AdapterPostDeploymentVerifierResultRegistry registry;
        return registry;
    }

    bool AdapterPostDeploymentVerifierResultRegistry::RegisterEnvelope(
        const AdapterPostDeploymentVerifierResultEnvelope&,
        AZStd::string* error)
    {
        SetError(
            error,
            "Unbound post-deployment verifier-result registration is prohibited; supply the exact work order, execution result, report, and source/evidence registry.");
        return false;
    }

    bool AdapterPostDeploymentVerifierResultRegistry::RegisterEnvelope(
        const AdapterDeploymentWorkOrder& workOrder,
        const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
        const AdapterPostDeploymentVerificationReport& report,
        const SourceEvidenceRegistry& sourceRegistry,
        const AdapterPostDeploymentVerifierResultEnvelope& envelope,
        AZStd::string* error)
    {
        if (!PostDeploymentVerifierResultFingerprintMatches(envelope))
        {
            SetError(
                error,
                "Verifier-result fingerprint must be the SHA-256 of the deterministic canonical verifier-result payload.");
            return false;
        }

        AdapterPostDeploymentVerifierEvidenceService service;
        const AdapterPostDeploymentVerifierEvidenceReturn result =
            service.BuildEvidenceReturn(
                workOrder,
                executionEnvelope,
                report,
                sourceRegistry,
                envelope);
        if (!result.m_contractValid)
        {
            AZStd::string message =
                "Post-deployment verifier result rejected with status "
                + ToString(result.m_status) + ".";
            if (!result.m_issues.empty())
            {
                message += " " + result.m_issues.front().m_code + ": "
                    + result.m_issues.front().m_message;
            }
            SetError(error, AZStd::move(message));
            return false;
        }

        for (const AdapterPostDeploymentVerifierResultEnvelope& existing :
            m_envelopes)
        {
            if (existing.m_verifierResultId == envelope.m_verifierResultId)
            {
                SetError(
                    error,
                    "A post-deployment verifier envelope with this result identity already exists.");
                return false;
            }
            if (existing.m_profileId == envelope.m_profileId
                && existing.m_resultFingerprint == envelope.m_resultFingerprint)
            {
                SetError(
                    error,
                    "A post-deployment verifier-result fingerprint is already registered for this profile.");
                return false;
            }
        }

        m_envelopes.push_back(envelope);
        AZStd::sort(
            m_envelopes.begin(),
            m_envelopes.end(),
            [](const AdapterPostDeploymentVerifierResultEnvelope& left,
                const AdapterPostDeploymentVerifierResultEnvelope& right)
            {
                return left.m_verifierResultId < right.m_verifierResultId;
            });
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
