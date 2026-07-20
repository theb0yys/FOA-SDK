/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

namespace
{
        AZStd::string RefusalSummary(const AdapterWorkOrderPlanSet& result)
        {
            AZStd::string summary;
            for (const AdapterWorkOrderRefusal& refusal : result.m_refusals)
            {
                for (const AZStd::string& status : refusal.m_compatibilityStatuses)
                {
                    summary += status + "; ";
                }
                for (const AZStd::string& reason : refusal.m_reasons)
                {
                    summary += reason + "; ";
                }
            }
            return summary;
        }

        const AdapterWorkOrderStep* FindStep(
            const AdapterWorkOrderPlan& plan,
            const AZStd::string& capability,
            const AZStd::string& subjectId)
        {
            for (const AdapterWorkOrderStep& step : plan.m_steps)
            {
                if (step.m_capability == capability
                    && step.m_subjectId == subjectId)
                {
                    return &step;
                }
            }
            return nullptr;
        }
} // namespace
