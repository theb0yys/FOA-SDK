/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

    TEST(TaintedGrailAdapterWorkOrderPlanningTests, AnyNonSupportedCompatibilityRowRefusesWholePlan)
    {
        AdapterWorkOrderPlanningService service;
        AdapterContractRegistry registry;
        const AdapterWorkOrderPlanSet result = service.BuildPlans(
            MakeWorkspace(),
            { MakePack() },
            registry,
            {},
            {},
            {});

        EXPECT_EQ(result.m_candidatePlanCount, 1);
        EXPECT_EQ(result.m_generatedPlanCount, 0);
        EXPECT_EQ(result.m_refusedPlanCount, 1);
        ASSERT_EQ(result.m_refusals.size(), 1);
        EXPECT_EQ(result.m_refusals.front().m_failedCapabilities.size(), 11);
        EXPECT_TRUE(result.m_plans.empty());
    }

    TEST(TaintedGrailAdapterWorkOrderPlanningTests, FullySupportedCatalogBuildsCanonicalPlanOnly)
    {
        ReadyFixture fixture = MakeReadyFixture();
        AdapterWorkOrderPlanningService service;
        const AdapterWorkOrderPlanSet result = service.BuildPlans(
            fixture.m_workspace,
            fixture.m_packs,
            fixture.m_adapterRegistry,
            fixture.m_sourceRegistry,
            fixture.m_catalog,
            fixture.m_blockers);

        ASSERT_EQ(result.m_generatedPlanCount, 1)
            << RefusalSummary(result).c_str();
        EXPECT_EQ(result.m_refusedPlanCount, 0);
        ASSERT_EQ(result.m_plans.size(), 1);
        const AdapterWorkOrderPlan& plan = result.m_plans.front();
        EXPECT_EQ(plan.m_formatVersion, 1);
        EXPECT_FALSE(plan.m_executionAllowed);
        EXPECT_EQ(plan.m_steps.size(), 11);
        EXPECT_EQ(result.m_stepCount, 11);
        EXPECT_NE(plan.m_canonicalJson.find("\"ExecutionAllowed\":false"), AZStd::string::npos);
        EXPECT_NE(plan.m_canonicalJson.find("\"PlanId\":\"workorder.plan:"), AZStd::string::npos);
        for (const AdapterWorkOrderStep& step : plan.m_steps)
        {
            EXPECT_FALSE(step.m_executionAllowed);
            EXPECT_FALSE(step.m_stepId.empty());
            EXPECT_GT(step.m_sequence, 0);
            EXPECT_FALSE(step.m_declarationEvidenceIds.empty());
        }
    }

    TEST(TaintedGrailAdapterWorkOrderPlanningTests, AggregateSupportCannotLeakUnreviewedSubjectsIntoSteps)
    {
        ReadyFixture fixture = MakeReadyFixture();
        AZStd::string error;
        CatalogRecord unreviewed = MakeRecord(
            "item.unreviewed",
            "subject:work-order:item:unreviewed",
            "item",
            "source_scoped",
            { "existing_item_grant" });
        RegisterEvidence(
            fixture.m_sourceRegistry,
            MakeEvidence(
                unreviewed.m_evidenceIds.front(),
                unreviewed.m_subjectRef));
        ASSERT_TRUE(fixture.m_catalog.InsertNew(unreviewed, &error)) << error.c_str();

        AdapterWorkOrderPlanningService service;
        const AdapterWorkOrderPlanSet result = service.BuildPlans(
            fixture.m_workspace,
            fixture.m_packs,
            fixture.m_adapterRegistry,
            fixture.m_sourceRegistry,
            fixture.m_catalog,
            fixture.m_blockers);

        ASSERT_EQ(result.m_generatedPlanCount, 1);
        const AdapterWorkOrderPlan& plan = result.m_plans.front();
        EXPECT_NE(FindStep(plan, "item_grant", "item.native"), nullptr);
        EXPECT_EQ(FindStep(plan, "item_grant", "item.unreviewed"), nullptr);
    }

