/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterResearchPipelineTestFixture.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    TEST(
        AdapterVerifierEvidenceReconciliationTests,
        ExactAllMatchedPipelineRequiresExplicitHumanApproval)
    {
        Test::AdapterResearchPipelineFixture fixture;
        EXPECT_TRUE(fixture.m_reconciliation.m_accepted);
        EXPECT_EQ(
            fixture.m_reconciliation.m_envelope.m_status,
            AdapterVerifierReconciliationEnvelopeStatus::Accepted);
        EXPECT_EQ(
            fixture.m_reconciliation.m_envelope.m_compatibilityAssessment,
            AdapterVerifierCompatibilityAssessment::Clear);
        EXPECT_EQ(
            fixture.m_reconciliation.m_envelope.m_releaseDecision,
            AdapterVerifierReleaseDecision::Approved);
        EXPECT_EQ(
            fixture.m_reconciliation.m_envelope.m_humanReviewState,
            AdapterVerifierHumanReviewState::Complete);
        EXPECT_FALSE(
            fixture.m_reconciliation.m_envelope.m_canonicalJson.empty());
        EXPECT_FALSE(fixture.m_reconciliation.m_sourceDocuments.empty());
        EXPECT_FALSE(fixture.m_reconciliation.m_evidenceDocuments.empty());
    }

    TEST(
        AdapterVerifierEvidenceReconciliationTests,
        MissingReviewIsNotMisclassifiedAsBindingMismatch)
    {
        Test::AdapterResearchPipelineFixture fixture;
        AdapterVerifierEvidenceReconciliationRequest request =
            fixture.m_reconciliationRequest;
        request.m_reconciliationId = "owner.reconciliation.missing-review";
        request.m_releaseReview = {};

        AdapterVerifierEvidenceReconciliationService service;
        const AdapterVerifierEvidenceReconciliationResult result =
            service.BuildReconciliation(
                fixture.m_workOrder,
                fixture.m_executionEnvelope,
                fixture.m_report,
                fixture.m_verifierEnvelope,
                fixture.m_verifierEvidence,
                request);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_EQ(
            result.m_envelope.m_status,
            AdapterVerifierReconciliationEnvelopeStatus::ReviewMissing);
        EXPECT_EQ(
            result.m_envelope.m_humanReviewState,
            AdapterVerifierHumanReviewState::Missing);
    }

    TEST(
        AdapterVerifierEvidenceReconciliationTests,
        HumanReviewCandidateClaimsMustMatchExactCandidateSet)
    {
        Test::AdapterResearchPipelineFixture fixture;
        AdapterVerifierEvidenceReconciliationRequest request =
            fixture.m_reconciliationRequest;
        request.m_reconciliationId = "owner.reconciliation.candidate-drift";
        request.m_releaseReview.m_candidateEvidenceIds.push_back(
            "owner.evidence.unrelated");

        AdapterVerifierEvidenceReconciliationService service;
        const AdapterVerifierEvidenceReconciliationResult result =
            service.BuildReconciliation(
                fixture.m_workOrder,
                fixture.m_executionEnvelope,
                fixture.m_report,
                fixture.m_verifierEnvelope,
                fixture.m_verifierEvidence,
                request);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_EQ(
            result.m_envelope.m_status,
            AdapterVerifierReconciliationEnvelopeStatus::BindingMismatch);
        EXPECT_NE(
            result.m_envelope.m_canonicalJson,
            fixture.m_reconciliation.m_envelope.m_canonicalJson);
    }

    TEST(
        AdapterVerifierEvidenceReconciliationTests,
        ReviewEvidenceMustBelongToCandidateEvidence)
    {
        Test::AdapterResearchPipelineFixture fixture;
        AdapterVerifierEvidenceReconciliationRequest request =
            fixture.m_reconciliationRequest;
        request.m_reconciliationId = "owner.reconciliation.unbound-evidence";
        request.m_releaseReview.m_evidenceIds = {
            "owner.evidence.unrelated",
        };

        AdapterVerifierEvidenceReconciliationService service;
        const AdapterVerifierEvidenceReconciliationResult result =
            service.BuildReconciliation(
                fixture.m_workOrder,
                fixture.m_executionEnvelope,
                fixture.m_report,
                fixture.m_verifierEnvelope,
                fixture.m_verifierEvidence,
                request);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_EQ(
            result.m_envelope.m_status,
            AdapterVerifierReconciliationEnvelopeStatus::ReviewInvalid);
    }

    TEST(
        AdapterVerifierEvidenceReconciliationTests,
        RegistryStoresOnlyCompleteAcceptedReconciliations)
    {
        Test::AdapterResearchPipelineFixture fixture;
        AdapterVerifierEvidenceReconciliationRegistry registry;
        AZStd::string error;
        EXPECT_FALSE(registry.RegisterRequest(
            fixture.m_reconciliationRequest,
            &error));
        EXPECT_NE(error.find("Unbound"), AZStd::string::npos);

        ASSERT_TRUE(registry.RegisterRequest(
            fixture.m_workOrder,
            fixture.m_executionEnvelope,
            fixture.m_report,
            fixture.m_verifierEnvelope,
            fixture.m_verifierEvidence,
            fixture.m_reconciliationRequest,
            &error)) << error.c_str();
        EXPECT_FALSE(registry.RegisterRequest(
            fixture.m_workOrder,
            fixture.m_executionEnvelope,
            fixture.m_report,
            fixture.m_verifierEnvelope,
            fixture.m_verifierEvidence,
            fixture.m_reconciliationRequest,
            &error));
    }
} // namespace TaintedGrailModdingSDK
