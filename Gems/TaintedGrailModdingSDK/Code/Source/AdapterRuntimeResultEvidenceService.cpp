/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterRuntimeResultEvidenceService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
#include "AdapterRuntimeResultEvidenceServicePart1.inl"
#include "AdapterRuntimeResultEvidenceServicePart2.inl"
#include "AdapterRuntimeResultEvidenceServicePart3.inl"
    } // namespace

    AdapterRuntimeEvidenceReturn AdapterRuntimeResultEvidenceService::BuildEvidenceReturn(
        const AdapterWorkOrderPlan& plan,
        const AdapterRuntimeResultEnvelope& envelope) const
    {
        AdapterRuntimeEvidenceReturn result;
        result.m_resultId = envelope.m_resultId;
        result.m_planId = envelope.m_planId;
        result.m_planFingerprint = envelope.m_planFingerprint;
        result.m_resultFingerprint = envelope.m_resultFingerprint;
        result.m_stepResultCount = static_cast<AZ::u64>(envelope.m_stepResults.size());
        result.m_failureCount = static_cast<AZ::u64>(envelope.m_failures.size());
        result.m_logReferenceCount = static_cast<AZ::u64>(envelope.m_logReferences.size());

        AdapterRuntimeResultRegistry shapeRegistry;
        AZStd::string shapeError;
        if (!shapeRegistry.RegisterEnvelope(envelope, &shapeError))
        {
            AddIssue(result, "runtime_result.contract_shape", shapeError);
        }

        ValidatePlanBinding(plan, envelope, result);
        ValidateStepBindings(plan, envelope, result);
        ValidateRecoveryBindings(plan, envelope, result);
        ValidateFailureAndLogBindings(plan, envelope, result);

        AZStd::sort(
            result.m_issues.begin(),
            result.m_issues.end(),
            [](const AdapterRuntimeResultIssue& left, const AdapterRuntimeResultIssue& right)
            {
                if (left.m_code != right.m_code)
                {
                    return left.m_code < right.m_code;
                }
                if (left.m_stepId != right.m_stepId)
                {
                    return left.m_stepId < right.m_stepId;
                }
                return left.m_message < right.m_message;
            });
        if (!result.m_issues.empty())
        {
            return result;
        }

        BuildPrimaryEvidence(plan, envelope, result);
        BuildLogEvidence(envelope, result);
        FinalizeCounts(result);
        result.m_accepted = true;
        return result;
    }
} // namespace TaintedGrailModdingSDK
