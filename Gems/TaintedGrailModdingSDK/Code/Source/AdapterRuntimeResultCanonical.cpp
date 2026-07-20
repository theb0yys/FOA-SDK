/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterRuntimeResultCanonical.h"

#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        namespace Json = DeterministicContractJson;

        void AppendStep(
            AZStd::string& output,
            const AdapterRuntimeStepResult& step)
        {
            output.push_back('{');
            Json::AppendString(output, "StepId", step.m_stepId);
            Json::AppendUnsigned(output, "Sequence", step.m_sequence);
            Json::AppendString(output, "Capability", step.m_capability);
            Json::AppendString(output, "SubjectKind", step.m_subjectKind);
            Json::AppendString(output, "SubjectId", step.m_subjectId);
            Json::AppendString(output, "SubjectRef", step.m_subjectRef);
            Json::AppendString(output, "Outcome", ToString(step.m_outcome));
            Json::AppendSortedStringArray(output, "FailureIds", step.m_failureIds);
            Json::AppendSortedStringArray(
                output,
                "LogReferenceIds",
                step.m_logReferenceIds);
            Json::AppendString(
                output,
                "OutputFingerprint",
                step.m_outputFingerprint);
            Json::AppendBool(output, "Attempted", step.m_attempted, false);
            output.push_back('}');
        }

        void AppendRecovery(
            AZStd::string& output,
            const char* name,
            const AdapterRuntimeRecoveryResult& recovery)
        {
            Json::AppendName(output, name);
            output.push_back('{');
            Json::AppendString(output, "StepId", recovery.m_stepId);
            Json::AppendString(output, "Outcome", ToString(recovery.m_outcome));
            Json::AppendSortedStringArray(
                output,
                "FailureIds",
                recovery.m_failureIds);
            Json::AppendSortedStringArray(
                output,
                "LogReferenceIds",
                recovery.m_logReferenceIds);
            Json::AppendString(
                output,
                "OutputFingerprint",
                recovery.m_outputFingerprint,
                false);
            output.push_back('}');
            output.push_back(',');
        }

        void AppendFailure(
            AZStd::string& output,
            const AdapterRuntimeFailure& failure)
        {
            output.push_back('{');
            Json::AppendString(output, "FailureId", failure.m_failureId);
            Json::AppendString(output, "Kind", ToString(failure.m_kind));
            Json::AppendString(output, "Code", failure.m_code);
            Json::AppendString(output, "Message", failure.m_message);
            Json::AppendString(output, "StepId", failure.m_stepId);
            Json::AppendSortedStringArray(
                output,
                "LogReferenceIds",
                failure.m_logReferenceIds);
            Json::AppendBool(output, "Retryable", failure.m_retryable, false);
            output.push_back('}');
        }

        void AppendLog(
            AZStd::string& output,
            const AdapterRuntimeLogReference& log)
        {
            output.push_back('{');
            Json::AppendString(output, "LogId", log.m_logId);
            Json::AppendString(output, "Kind", ToString(log.m_kind));
            Json::AppendString(output, "Reference", log.m_reference);
            Json::AppendString(output, "Fingerprint", log.m_fingerprint);
            Json::AppendSortedStringArray(
                output,
                "StepIds",
                log.m_stepIds,
                false);
            output.push_back('}');
        }
    } // namespace

    AZStd::string SerializeCanonicalRuntimeResult(
        const AdapterRuntimeResultEnvelope& envelope)
    {
        AZStd::string output;
        output.push_back('{');
        Json::AppendUnsigned(output, "ContractVersion", envelope.m_contractVersion);
        Json::AppendString(output, "ResultId", envelope.m_resultId);
        Json::AppendString(output, "PlanId", envelope.m_planId);
        Json::AppendString(
            output,
            "PlanCanonicalJson",
            envelope.m_planCanonicalJson);
        Json::AppendString(output, "PlanFingerprint", envelope.m_planFingerprint);
        Json::AppendString(output, "PackId", envelope.m_packId);
        Json::AppendString(output, "PackVersion", envelope.m_packVersion);
        Json::AppendString(output, "AdapterId", envelope.m_adapterId);
        Json::AppendString(output, "AdapterVersion", envelope.m_adapterVersion);
        Json::AppendString(output, "ProfileId", envelope.m_profileId);
        Json::AppendString(output, "GameVersion", envelope.m_gameVersion);
        Json::AppendString(output, "Branch", envelope.m_branch);
        Json::AppendString(output, "RuntimeTarget", envelope.m_runtimeTarget);
        Json::AppendString(output, "CapturedAt", envelope.m_capturedAt);

        AZStd::vector<const AdapterRuntimeStepResult*> steps;
        steps.reserve(envelope.m_stepResults.size());
        for (const AdapterRuntimeStepResult& step : envelope.m_stepResults)
        {
            steps.push_back(&step);
        }
        AZStd::sort(
            steps.begin(),
            steps.end(),
            [](const AdapterRuntimeStepResult* left,
                const AdapterRuntimeStepResult* right)
            {
                if (left->m_sequence != right->m_sequence)
                {
                    return left->m_sequence < right->m_sequence;
                }
                return left->m_stepId < right->m_stepId;
            });
        Json::AppendName(output, "StepResults");
        output.push_back('[');
        for (size_t index = 0; index < steps.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendStep(output, *steps[index]);
        }
        output.push_back(']');
        output.push_back(',');

        AppendRecovery(output, "CleanupResult", envelope.m_cleanupResult);
        AppendRecovery(output, "RollbackResult", envelope.m_rollbackResult);

        AZStd::vector<const AdapterRuntimeFailure*> failures;
        failures.reserve(envelope.m_failures.size());
        for (const AdapterRuntimeFailure& failure : envelope.m_failures)
        {
            failures.push_back(&failure);
        }
        AZStd::sort(
            failures.begin(),
            failures.end(),
            [](const AdapterRuntimeFailure* left,
                const AdapterRuntimeFailure* right)
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

        AZStd::vector<const AdapterRuntimeLogReference*> logs;
        logs.reserve(envelope.m_logReferences.size());
        for (const AdapterRuntimeLogReference& log : envelope.m_logReferences)
        {
            logs.push_back(&log);
        }
        AZStd::sort(
            logs.begin(),
            logs.end(),
            [](const AdapterRuntimeLogReference* left,
                const AdapterRuntimeLogReference* right)
            {
                return left->m_logId < right->m_logId;
            });
        Json::AppendName(output, "LogReferences");
        output.push_back('[');
        for (size_t index = 0; index < logs.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendLog(output, *logs[index]);
        }
        output.push_back(']');
        output.push_back('}');
        return output;
    }

    AZStd::string CalculateRuntimeResultFingerprint(
        const AdapterRuntimeResultEnvelope& envelope)
    {
        return CalculateCanonicalSha256(SerializeCanonicalRuntimeResult(envelope));
    }

    bool RuntimeResultFingerprintMatches(
        const AdapterRuntimeResultEnvelope& envelope)
    {
        return envelope.m_resultFingerprint
            == CalculateRuntimeResultFingerprint(envelope);
    }
} // namespace TaintedGrailModdingSDK
