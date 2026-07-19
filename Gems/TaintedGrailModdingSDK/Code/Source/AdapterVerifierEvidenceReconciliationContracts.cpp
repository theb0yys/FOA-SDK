/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterVerifierEvidenceReconciliationContracts.h"

#include "AdapterVerifierEvidenceReconciliationService.h"

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

        bool RequestWithinBounds(
            const AdapterVerifierEvidenceReconciliationRequest& request)
        {
            constexpr size_t MaxCanonicalJsonBytes = 1024 * 1024;
            constexpr size_t MaxCandidates = 4096;
            constexpr size_t MaxDispositions = 4096;
            constexpr size_t MaxEvidencePerDecision = 4096;
            constexpr size_t MaxTextBytes = 16 * 1024;

            const AdapterVerifierReleaseReview& review = request.m_releaseReview;
            if (request.m_reconciliationId.size() > 256
                || review.m_reportCanonicalJson.size() > MaxCanonicalJsonBytes
                || review.m_candidateSourceIds.size() > MaxCandidates
                || review.m_candidateEvidenceIds.size() > MaxCandidates
                || review.m_evidenceIds.size() > MaxEvidencePerDecision
                || review.m_dispositions.size() > MaxDispositions
                || review.m_reviewer.size() > MaxTextBytes
                || review.m_rationale.size() > MaxTextBytes)
            {
                return false;
            }
            for (const AdapterVerifierFindingDisposition& disposition :
                review.m_dispositions)
            {
                if (disposition.m_findingId.size() > 512
                    || disposition.m_rationale.size() > MaxTextBytes
                    || disposition.m_evidenceIds.size() > MaxEvidencePerDecision)
                {
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
        const AdapterVerifierEvidenceReconciliationRequest&,
        AZStd::string* error)
    {
        SetError(
            error,
            "Unbound verifier reconciliation registration is prohibited; supply the exact work order, execution result, report, verifier result, and verifier evidence return.");
        return false;
    }

    bool AdapterVerifierEvidenceReconciliationRegistry::RegisterRequest(
        const AdapterDeploymentWorkOrder& workOrder,
        const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
        const AdapterPostDeploymentVerificationReport& report,
        const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope,
        const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
        const AdapterVerifierEvidenceReconciliationRequest& request,
        AZStd::string* error)
    {
        if (!RequestWithinBounds(request))
        {
            SetError(
                error,
                "Verifier reconciliation request exceeds bounded canonical, candidate, disposition, evidence, or text limits.");
            return false;
        }

        AdapterVerifierEvidenceReconciliationService service;
        const AdapterVerifierEvidenceReconciliationResult result =
            service.BuildReconciliation(
                workOrder,
                executionEnvelope,
                report,
                verifierEnvelope,
                verifierEvidence,
                request);
        if (!result.m_accepted)
        {
            AZStd::string message =
                "Verifier reconciliation rejected with status "
                + ToString(result.m_envelope.m_status) + ".";
            if (!result.m_issues.empty())
            {
                message += " " + result.m_issues.front().m_code + ": "
                    + result.m_issues.front().m_message;
            }
            SetError(error, AZStd::move(message));
            return false;
        }

        for (const AdapterVerifierEvidenceReconciliationRequest& existing :
            m_requests)
        {
            if (existing.m_reconciliationId == request.m_reconciliationId)
            {
                SetError(
                    error,
                    "A verifier reconciliation request with this identity already exists.");
                return false;
            }
        }

        m_requests.push_back(request);
        AZStd::sort(
            m_requests.begin(),
            m_requests.end(),
            [](const AdapterVerifierEvidenceReconciliationRequest& left,
                const AdapterVerifierEvidenceReconciliationRequest& right)
            {
                return left.m_reconciliationId < right.m_reconciliationId;
            });
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
