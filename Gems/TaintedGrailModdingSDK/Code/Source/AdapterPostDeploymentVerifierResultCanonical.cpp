/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPostDeploymentVerifierResultCanonical.h"

#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        namespace Json = DeterministicContractJson;

        void AppendReview(
            AZStd::string& output,
            const AdapterPostDeploymentVerifierReview& review)
        {
            Json::AppendName(output, "VerifierReview");
            output.push_back('{');
            Json::AppendString(output, "ReviewId", review.m_reviewId);
            Json::AppendString(output, "VerifierId", review.m_verifierId);
            Json::AppendString(output, "VerifierVersion", review.m_verifierVersion);
            Json::AppendString(
                output,
                "VerifierFingerprint",
                review.m_verifierFingerprint);
            Json::AppendString(output, "Decision", ToString(review.m_decision));
            Json::AppendString(output, "Reviewer", review.m_reviewer);

            AZStd::vector<AZStd::string> capabilities;
            for (AdapterPostDeploymentVerifierCapability capability :
                review.m_capabilities)
            {
                capabilities.push_back(ToString(capability));
            }
            Json::AppendSortedStringArray(
                output,
                "Capabilities",
                AZStd::move(capabilities));
            Json::AppendSortedStringArray(
                output,
                "EvidenceIds",
                review.m_evidenceIds);
            Json::AppendString(output, "ReviewedAtUtc", review.m_reviewedAtUtc);
            Json::AppendString(output, "Notes", review.m_notes, false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendCheck(
            AZStd::string& output,
            const AdapterPostDeploymentVerifierCheckResult& check)
        {
            output.push_back('{');
            Json::AppendString(output, "CheckId", check.m_checkId);
            Json::AppendUnsigned(output, "Sequence", check.m_sequence);
            Json::AppendString(output, "StepId", check.m_stepId);
            Json::AppendString(output, "TargetPath", check.m_targetPath);
            Json::AppendString(
                output,
                "ExpectedFingerprint",
                check.m_expectedFingerprint);
            Json::AppendString(
                output,
                "ObservedFingerprint",
                check.m_observedFingerprint);
            Json::AppendBool(output, "ExpectedPresent", check.m_expectedPresent);
            Json::AppendBool(output, "ObservedPresent", check.m_observedPresent);
            Json::AppendBool(output, "Attempted", check.m_attempted);
            Json::AppendBool(
                output,
                "ObservationRecorded",
                check.m_observationRecorded);
            Json::AppendString(output, "Outcome", ToString(check.m_outcome));
            Json::AppendString(output, "CheckedAtUtc", check.m_checkedAtUtc);
            Json::AppendSortedStringArray(
                output,
                "FailureIds",
                check.m_failureIds);
            Json::AppendSortedStringArray(
                output,
                "DiagnosticReferenceIds",
                check.m_diagnosticReferenceIds,
                false);
            output.push_back('}');
        }

        void AppendFailure(
            AZStd::string& output,
            const AdapterPostDeploymentVerifierFailure& failure)
        {
            output.push_back('{');
            Json::AppendString(output, "FailureId", failure.m_failureId);
            Json::AppendString(output, "Kind", ToString(failure.m_kind));
            Json::AppendString(output, "Code", failure.m_code);
            Json::AppendString(output, "Message", failure.m_message);
            Json::AppendString(output, "CheckId", failure.m_checkId);
            Json::AppendSortedStringArray(
                output,
                "DiagnosticReferenceIds",
                failure.m_diagnosticReferenceIds);
            Json::AppendBool(output, "Retryable", failure.m_retryable, false);
            output.push_back('}');
        }

        void AppendDiagnostic(
            AZStd::string& output,
            const AdapterPostDeploymentVerifierDiagnosticReference& diagnostic)
        {
            output.push_back('{');
            Json::AppendString(
                output,
                "DiagnosticId",
                diagnostic.m_diagnosticId);
            Json::AppendString(output, "Kind", ToString(diagnostic.m_kind));
            Json::AppendString(output, "Reference", diagnostic.m_reference);
            Json::AppendString(output, "Fingerprint", diagnostic.m_fingerprint);
            Json::AppendSortedStringArray(
                output,
                "CheckIds",
                diagnostic.m_checkIds,
                false);
            output.push_back('}');
        }
    } // namespace

    AZStd::string SerializeCanonicalPostDeploymentVerifierResult(
        const AdapterPostDeploymentVerifierResultEnvelope& envelope)
    {
        AZStd::string output;
        output.push_back('{');
        Json::AppendUnsigned(output, "ContractVersion", envelope.m_contractVersion);
        Json::AppendString(
            output,
            "VerifierResultId",
            envelope.m_verifierResultId);
        Json::AppendString(output, "ReportId", envelope.m_reportId);
        Json::AppendString(output, "ReportStatus", ToString(envelope.m_reportStatus));
        Json::AppendString(
            output,
            "ReportCanonicalJson",
            envelope.m_reportCanonicalJson);
        Json::AppendString(output, "ResultId", envelope.m_resultId);
        Json::AppendString(output, "WorkOrderId", envelope.m_workOrderId);
        Json::AppendString(
            output,
            "WorkOrderFingerprint",
            envelope.m_workOrderFingerprint);
        Json::AppendString(
            output,
            "ExecutionResultFingerprint",
            envelope.m_executionResultFingerprint);
        Json::AppendString(output, "ProfileId", envelope.m_profileId);
        Json::AppendString(output, "GameVersion", envelope.m_gameVersion);
        Json::AppendString(output, "Branch", envelope.m_branch);
        Json::AppendString(output, "RuntimeTarget", envelope.m_runtimeTarget);
        Json::AppendString(output, "CapturedAtUtc", envelope.m_capturedAtUtc);
        AppendReview(output, envelope.m_verifierReview);

        AZStd::vector<const AdapterPostDeploymentVerifierCheckResult*> checks;
        for (const AdapterPostDeploymentVerifierCheckResult& check :
            envelope.m_checkResults)
        {
            checks.push_back(&check);
        }
        AZStd::sort(
            checks.begin(),
            checks.end(),
            [](const AdapterPostDeploymentVerifierCheckResult* left,
                const AdapterPostDeploymentVerifierCheckResult* right)
            {
                if (left->m_sequence != right->m_sequence)
                {
                    return left->m_sequence < right->m_sequence;
                }
                return left->m_checkId < right->m_checkId;
            });
        Json::AppendName(output, "CheckResults");
        output.push_back('[');
        for (size_t index = 0; index < checks.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendCheck(output, *checks[index]);
        }
        output.push_back(']');
        output.push_back(',');

        AZStd::vector<const AdapterPostDeploymentVerifierFailure*> failures;
        for (const AdapterPostDeploymentVerifierFailure& failure : envelope.m_failures)
        {
            failures.push_back(&failure);
        }
        AZStd::sort(
            failures.begin(),
            failures.end(),
            [](const AdapterPostDeploymentVerifierFailure* left,
                const AdapterPostDeploymentVerifierFailure* right)
            {
                return left->m_failureId < right->m_failureId;
            });
        Json::AppendName(output, "Failures");
        output.push_back('[');
        for (size_t index = 0; index < failures.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendFailure(output, *failures[index]);
        }
        output.push_back(']');
        output.push_back(',');

        AZStd::vector<const AdapterPostDeploymentVerifierDiagnosticReference*>
            diagnostics;
        for (const AdapterPostDeploymentVerifierDiagnosticReference& diagnostic :
            envelope.m_diagnosticReferences)
        {
            diagnostics.push_back(&diagnostic);
        }
        AZStd::sort(
            diagnostics.begin(),
            diagnostics.end(),
            [](const AdapterPostDeploymentVerifierDiagnosticReference* left,
                const AdapterPostDeploymentVerifierDiagnosticReference* right)
            {
                return left->m_diagnosticId < right->m_diagnosticId;
            });
        Json::AppendName(output, "DiagnosticReferences");
        output.push_back('[');
        for (size_t index = 0; index < diagnostics.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendDiagnostic(output, *diagnostics[index]);
        }
        output.push_back(']');
        output.push_back('}');
        return output;
    }

    AZStd::string CalculatePostDeploymentVerifierResultFingerprint(
        const AdapterPostDeploymentVerifierResultEnvelope& envelope)
    {
        return CalculateCanonicalSha256(
            SerializeCanonicalPostDeploymentVerifierResult(envelope));
    }

    bool PostDeploymentVerifierResultFingerprintMatches(
        const AdapterPostDeploymentVerifierResultEnvelope& envelope)
    {
        return envelope.m_resultFingerprint
            == CalculatePostDeploymentVerifierResultFingerprint(envelope);
    }
} // namespace TaintedGrailModdingSDK
