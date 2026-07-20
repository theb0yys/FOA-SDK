/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

    TEST(TaintedGrailAdapterWorkOrderPlanningTests, RelationshipStepsBindResolvedTargetsAndProof)
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

        ASSERT_EQ(result.m_generatedPlanCount, 1);
        const AdapterWorkOrderStep* vendor = FindStep(
            result.m_plans.front(),
            "vendor_mutation",
            "relationship.vendor");
        ASSERT_NE(vendor, nullptr);
        EXPECT_EQ(vendor->m_subjectKind, "relationship");
        EXPECT_EQ(vendor->m_sourceRecordId, "item.native");
        EXPECT_EQ(vendor->m_targetRecordId, "vendor.one");
        EXPECT_EQ(vendor->m_targetSubjectRef, "subject:work-order:vendor:one");
        EXPECT_TRUE(Contains(
            vendor->m_validationProofIds,
            "validation.relationship.relationship.vendor"));
        EXPECT_TRUE(Contains(
            vendor->m_validationProofIds,
            "validation.record.item.native"));
    }

    TEST(TaintedGrailAdapterWorkOrderPlanningTests, MissingRelationshipProofRefusesWholePlan)
    {
        ReadyFixture fixture = MakeReadyFixture(false, false);
        AdapterWorkOrderPlanningService service;
        const AdapterWorkOrderPlanSet result = service.BuildPlans(
            fixture.m_workspace,
            fixture.m_packs,
            fixture.m_adapterRegistry,
            fixture.m_sourceRegistry,
            fixture.m_catalog,
            fixture.m_blockers);

        EXPECT_EQ(result.m_generatedPlanCount, 0);
        ASSERT_EQ(result.m_refusedPlanCount, 1);
        EXPECT_TRUE(Contains(
            result.m_refusals.front().m_failedCapabilities,
            "vendor_mutation"))
            << RefusalSummary(result).c_str();
        EXPECT_TRUE(Contains(
            result.m_refusals.front().m_compatibilityStatuses,
            "vendor_mutation:proof_missing"))
            << RefusalSummary(result).c_str();
        bool relationshipReasonFound = false;
        for (const AZStd::string& reason : result.m_refusals.front().m_reasons)
        {
            relationshipReasonFound = relationshipReasonFound
                || (reason.find("relationship and target proof chain")
                        != AZStd::string::npos
                    && reason.find("vendor_or_loot_injection")
                        != AZStd::string::npos);
        }
        EXPECT_TRUE(relationshipReasonFound)
            << RefusalSummary(result).c_str();
    }

    TEST(TaintedGrailAdapterWorkOrderPlanningTests, InvalidTypedPayloadEvidenceRefusesWholePlan)
    {
        ReadyFixture fixture = MakeReadyFixture();
        EconomyItemProfile invalidProfile;
        invalidProfile.m_recordId = "item.synthetic";
        invalidProfile.m_category = "test";
        invalidProfile.m_subtype = "synthetic";
        invalidProfile.m_evidenceIds = { "evidence.missing.typed-item" };
        AZStd::string error;
        ASSERT_TRUE(fixture.m_catalog.UpsertEconomyItem(invalidProfile, &error))
            << error.c_str();

        AdapterWorkOrderPlanningService service;
        const AdapterWorkOrderPlanSet result = service.BuildPlans(
            fixture.m_workspace,
            fixture.m_packs,
            fixture.m_adapterRegistry,
            fixture.m_sourceRegistry,
            fixture.m_catalog,
            fixture.m_blockers);

        EXPECT_EQ(result.m_generatedPlanCount, 0);
        ASSERT_EQ(result.m_refusedPlanCount, 1);
        bool evidenceReasonFound = false;
        for (const AZStd::string& reason : result.m_refusals.front().m_reasons)
        {
            evidenceReasonFound = evidenceReasonFound
                || reason.find("typed item profile") != AZStd::string::npos;
        }
        EXPECT_TRUE(evidenceReasonFound);
    }

