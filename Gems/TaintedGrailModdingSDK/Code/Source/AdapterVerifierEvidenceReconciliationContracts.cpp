/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterVerifierEvidenceReconciliationContracts.h"

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

        constexpr EnumName<AdapterVerifierCompatibilityAssessment>
            CompatibilityAssessments[] = {
                { AdapterVerifierCompatibilityAssessment::Unassessed,
                    "unassessed" },
                { AdapterVerifierCompatibilityAssessment::Clear, "clear" },
                { AdapterVerifierCompatibilityAssessment::Blocked, "blocked" },
                { AdapterVerifierCompatibilityAssessment::Inconclusive,
                    "inconclusive" },
            };

        constexpr EnumName<AdapterVerifierReleaseDecision> ReleaseDecisions[] = {
            { AdapterVerifierReleaseDecision::Pending, "pending" },
            { AdapterVerifierReleaseDecision::Hold, "hold" },
            { AdapterVerifierReleaseDecision::Rejected, "rejected" },
            { AdapterVerifierReleaseDecision::Approved, "approved" },
        };

        constexpr EnumName<AdapterVerifierHumanReviewState> HumanReviewStates[] = {
            { AdapterVerifierHumanReviewState::Missing, "missing" },
            { AdapterVerifierHumanReviewState::Invalid, "invalid" },
            { AdapterVerifierHumanReviewState::DispositionRequired,
                "disposition_required" },
            { AdapterVerifierHumanReviewState::Complete, "complete" },
        };

        constexpr EnumName<AdapterVerifierReleaseReviewDecision>
            ReleaseReviewDecisions[] = {
                { AdapterVerifierReleaseReviewDecision::Unknown, "unknown" },
                { AdapterVerifierReleaseReviewDecision::Hold, "hold" },
                { AdapterVerifierReleaseReviewDecision::Reject, "reject" },
                { AdapterVerifierReleaseReviewDecision::Approve, "approve" },
            };

        constexpr EnumName<AdapterVerifierReconciliationFindingKind>
            FindingKinds[] = {
                { AdapterVerifierReconciliationFindingKind::ReportBlocker,
                    "report_blocker" },
                { AdapterVerifierReconciliationFindingKind::VerifierMatched,
                    "verifier_matched" },
                { AdapterVerifierReconciliationFindingKind::VerifierMismatched,
                    "verifier_mismatched" },
                { AdapterVerifierReconciliationFindingKind::VerifierFailed,
                    "verifier_failed" },
                { AdapterVerifierReconciliationFindingKind::VerifierInconclusive,
                    "verifier_inconclusive" },
                { AdapterVerifierReconciliationFindingKind::VerifierNotRun,
                    "verifier_not_run" },
            };

        constexpr EnumName<AdapterVerifierEvidenceRelationship>
            EvidenceRelationships[] = {
                { AdapterVerifierEvidenceRelationship::Preserved, "preserved" },
                { AdapterVerifierEvidenceRelationship::Supporting, "supporting" },
                { AdapterVerifierEvidenceRelationship::Contradictory,
                    "contradictory" },
                { AdapterVerifierEvidenceRelationship::NewFinding,
                    "new_finding" },
            };

        constexpr EnumName<AdapterVerifierFindingDispositionDecision>
            FindingDispositionDecisions[] = {
                { AdapterVerifierFindingDispositionDecision::Unknown, "unknown" },
                { AdapterVerifierFindingDispositionDecision::Accepted,
                    "accepted" },
                { AdapterVerifierFindingDispositionDecision::Rejected,
                    "rejected" },
                { AdapterVerifierFindingDispositionDecision::Deferred,
                    "deferred" },
            };

        constexpr EnumName<AdapterVerifierReconciliationEnvelopeStatus>
            EnvelopeStatuses[] = {
                { AdapterVerifierReconciliationEnvelopeStatus::Accepted,
                    "accepted" },
                { AdapterVerifierReconciliationEnvelopeStatus::ReportNotReady,
                    "report_not_ready" },
                { AdapterVerifierReconciliationEnvelopeStatus::
                        VerifierEvidenceInvalid,
                    "verifier_evidence_invalid" },
                { AdapterVerifierReconciliationEnvelopeStatus::BindingMismatch,
                    "binding_mismatch" },
                { AdapterVerifierReconciliationEnvelopeStatus::ReviewMissing,
                    "review_missing" },
                { AdapterVerifierReconciliationEnvelopeStatus::ReviewInvalid,
                    "review_invalid" },
                { AdapterVerifierReconciliationEnvelopeStatus::
                        DispositionIncomplete,
                    "disposition_incomplete" },
                { AdapterVerifierReconciliationEnvelopeStatus::
                        DecisionInconsistent,
                    "decision_inconsistent" },
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

    AZStd::string ToString(AdapterVerifierCompatibilityAssessment assessment)
    {
        return EnumToString(assessment, CompatibilityAssessments);
    }

    AZStd::string ToString(AdapterVerifierReleaseDecision decision)
    {
        return EnumToString(decision, ReleaseDecisions);
    }

    AZStd::string ToString(AdapterVerifierHumanReviewState state)
    {
        return EnumToString(state, HumanReviewStates);
    }

    AZStd::string ToString(AdapterVerifierReleaseReviewDecision decision)
    {
        return EnumToString(decision, ReleaseReviewDecisions);
    }

    AZStd::string ToString(AdapterVerifierReconciliationFindingKind kind)
    {
        return EnumToString(kind, FindingKinds);
    }

    AZStd::string ToString(AdapterVerifierEvidenceRelationship relationship)
    {
        return EnumToString(relationship, EvidenceRelationships);
    }

    AZStd::string ToString(AdapterVerifierFindingDispositionDecision decision)
    {
        return EnumToString(decision, FindingDispositionDecisions);
    }

    AZStd::string ToString(AdapterVerifierReconciliationEnvelopeStatus status)
    {
        return EnumToString(status, EnvelopeStatuses);
    }

    bool TryParseAdapterVerifierCompatibilityAssessment(
        const AZStd::string& value,
        AdapterVerifierCompatibilityAssessment& assessment)
    {
        return TryParseEnum(value, assessment, CompatibilityAssessments);
    }

    bool TryParseAdapterVerifierReleaseDecision(
        const AZStd::string& value,
        AdapterVerifierReleaseDecision& decision)
    {
        return TryParseEnum(value, decision, ReleaseDecisions);
    }

    bool TryParseAdapterVerifierHumanReviewState(
        const AZStd::string& value,
        AdapterVerifierHumanReviewState& state)
    {
        return TryParseEnum(value, state, HumanReviewStates);
    }

    bool TryParseAdapterVerifierReleaseReviewDecision(
        const AZStd::string& value,
        AdapterVerifierReleaseReviewDecision& decision)
    {
        return TryParseEnum(value, decision, ReleaseReviewDecisions);
    }

    bool TryParseAdapterVerifierReconciliationFindingKind(
        const AZStd::string& value,
        AdapterVerifierReconciliationFindingKind& kind)
    {
        return TryParseEnum(value, kind, FindingKinds);
    }

    bool TryParseAdapterVerifierEvidenceRelationship(
        const AZStd::string& value,
        AdapterVerifierEvidenceRelationship& relationship)
    {
        return TryParseEnum(value, relationship, EvidenceRelationships);
    }

    bool TryParseAdapterVerifierFindingDispositionDecision(
        const AZStd::string& value,
        AdapterVerifierFindingDispositionDecision& decision)
    {
        return TryParseEnum(value, decision, FindingDispositionDecisions);
    }

    bool TryParseAdapterVerifierReconciliationEnvelopeStatus(
        const AZStd::string& value,
        AdapterVerifierReconciliationEnvelopeStatus& status)
    {
        return TryParseEnum(value, status, EnvelopeStatuses);
    }

    AdapterVerifierEvidenceReconciliationRegistry&
    AdapterVerifierEvidenceReconciliationRegistry::Get()
    {
        static AdapterVerifierEvidenceReconciliationRegistry registry;
        return registry;
    }

    bool AdapterVerifierEvidenceReconciliationRegistry::RegisterRequest(
        const AdapterVerifierEvidenceReconciliationRequest& request,
        AZStd::string* error)
    {
        if (!IsAdapterPostDeploymentVerifierStableId(
                request.m_reconciliationId)
            || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                request.m_evaluatedAtUtc))
        {
            if (error)
            {
                *error =
                    "Verifier reconciliation registration requires stable identity "
                    "and one explicit UTC evaluation time.";
            }
            return false;
        }

        for (const AdapterVerifierEvidenceReconciliationRequest& existing :
             m_requests)
        {
            if (existing.m_reconciliationId == request.m_reconciliationId)
            {
                if (error)
                {
                    *error =
                        "A verifier reconciliation request with this identity "
                        "already exists.";
                }
                return false;
            }
        }

        m_requests.push_back(request);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    void AdapterVerifierEvidenceReconciliationRegistry::Clear()
    {
        m_requests.clear();
    }

    const AZStd::vector<AdapterVerifierEvidenceReconciliationRequest>&
    AdapterVerifierEvidenceReconciliationRegistry::GetRequests() const
    {
        return m_requests;
    }
} // namespace TaintedGrailModdingSDK
