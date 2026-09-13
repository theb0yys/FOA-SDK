/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterWorkOrderPlanningService.h"
#include "ExecutionPlanning/FrameworkPlannerService.h"

#include <AzTest/AzTest.h>

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
#include "AdapterWorkOrderPlanningTestFixturePart1.inl"
#include "AdapterWorkOrderPlanningTestFixturePart2.inl"
#include "AdapterWorkOrderPlanningTestFixturePart3.inl"
#include "AdapterWorkOrderPlanningTestFixturePart4.inl"
#include "AdapterWorkOrderPlanningTestFixturePart5.inl"
#include "AdapterWorkOrderPlanningTestsPart1.inl"
#include "AdapterWorkOrderPlanningTestsPart2.inl"
#include "AdapterWorkOrderPlanningTestsPart3.inl"

    TEST(FrameworkPlannerWorkOrders, PreservesReadyPlansAndExactRefusals)
    {
        auto fixture = MakeReadyFixture();
        AdapterWorkOrderPlanningService owner;
        ExecutionFramework::FrameworkPlannerService framework;
        const auto direct = owner.BuildPlans(
            fixture.m_workspace,
            fixture.m_packs,
            fixture.m_adapterRegistry,
            fixture.m_sourceRegistry,
            fixture.m_catalog,
            fixture.m_blockers);
        const auto wrapped = framework.BuildPlans(
            fixture.m_workspace,
            fixture.m_packs,
            fixture.m_adapterRegistry,
            fixture.m_sourceRegistry,
            fixture.m_catalog,
            fixture.m_blockers);
        ASSERT_EQ(direct.m_plans.size(), 1);
        ASSERT_EQ(wrapped.m_plans.size(), direct.m_plans.size());
        EXPECT_EQ(wrapped.m_plans[0].m_canonicalJson, direct.m_plans[0].m_canonicalJson);
        EXPECT_EQ(wrapped.m_stepCount, direct.m_stepCount);
        AdapterContractRegistry missing;
        const auto refused = framework.BuildPlans(MakeWorkspace(), { MakePack() }, missing, {}, {}, {});
        const auto expected = owner.BuildPlans(MakeWorkspace(), { MakePack() }, missing, {}, {}, {});
        ASSERT_EQ(refused.m_refusals.size(), 1);
        ASSERT_EQ(expected.m_refusals.size(), 1);
        EXPECT_EQ(refused.m_generatedPlanCount, 0);
        EXPECT_EQ(refused.m_refusals[0].m_reasons, expected.m_refusals[0].m_reasons);
        EXPECT_EQ(refused.m_refusals[0].m_failedCapabilities, expected.m_refusals[0].m_failedCapabilities);
        EXPECT_EQ(refused.m_refusals[0].m_subjectIds, expected.m_refusals[0].m_subjectIds);
    }
} // namespace TaintedGrailModdingSDK
