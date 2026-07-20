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
    namespace
    {
        bool HasReportBlockerCode(
            const AdapterPostDeploymentVerificationReport& report,
            const AZStd::string& code)
        {
            for (const AdapterPostDeploymentBlocker& blocker : report.m_blockers)
            {
                if (blocker.m_code == code)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    TEST(
        AdapterPostDeploymentVerificationTests,
        ExactAcceptedExecutionEvidenceProducesReviewReadyReport)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        ASSERT_TRUE(fixture.m_executionEvidence.m_accepted);
        EXPECT_EQ(
            fixture.m_report.m_status,
            AdapterPostDeploymentReportStatus::ReviewReady);
        EXPECT_TRUE(fixture.m_report.m_executionEvidenceAccepted);
        EXPECT_TRUE(fixture.m_report.m_compatibilityClear);
        EXPECT_TRUE(fixture.m_report.m_releaseBlockerFree);
        EXPECT_TRUE(fixture.m_report.m_blockers.empty());
        EXPECT_FALSE(fixture.m_report.m_candidateSourceIds.empty());
        EXPECT_FALSE(fixture.m_report.m_candidateEvidenceIds.empty());
        EXPECT_TRUE(fixture.m_report.m_humanReviewRequired);
        EXPECT_FALSE(fixture.m_report.m_verifierExecuted);
        EXPECT_FALSE(fixture.m_report.m_evidencePromoted);
        EXPECT_FALSE(fixture.m_report.m_releasePublished);
        EXPECT_FALSE(fixture.m_report.m_launchPerformed);
        EXPECT_FALSE(fixture.m_report.m_adapterCalled);
    }

    TEST(
        AdapterPostDeploymentVerificationTests,
        EvidenceKindAloneCannotSatisfyRequiredBindingEvidence)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        AdapterDeploymentExecutionEvidenceReturn tampered =
            fixture.m_executionEvidence;
        bool changed = false;
        for (EvidenceDocument& document : tampered.m_evidenceDocuments)
        {
            for (EvidenceRecord& evidence : document.m_evidence)
            {
                if (evidence.m_evidenceKind == "deployment_work_order_binding")
                {
                    evidence.m_subjectRef = "deployment-work-order:unrelated";
                    changed = true;
                }
            }
        }
        ASSERT_TRUE(changed);

        AdapterPostDeploymentVerificationService service;
        const AdapterPostDeploymentVerificationReport report = service.BuildReport(
            fixture.m_workOrder,
            fixture.m_executionEnvelope,
            tampered);
        EXPECT_NE(report.m_status, AdapterPostDeploymentReportStatus::ReviewReady);
        EXPECT_FALSE(report.m_executionEvidenceAccepted);
        EXPECT_TRUE(HasReportBlockerCode(
            report,
            "post_deployment.evidence_record_binding_mismatch"));
        EXPECT_TRUE(HasReportBlockerCode(
            report,
            "post_deployment.candidate_evidence_missing"));
    }

    TEST(
        AdapterPostDeploymentVerificationTests,
        CallerSelectedExecutionFingerprintCannotEnterReport)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        AdapterDeploymentExecutionResultEnvelope tampered =
            fixture.m_executionEnvelope;
        tampered.m_resultFingerprint =
            ::TaintedGrailModdingSDK::Test::FixtureFingerprint('0');

        AdapterPostDeploymentVerificationService service;
        const AdapterPostDeploymentVerificationReport report = service.BuildReport(
            fixture.m_workOrder,
            tampered,
            fixture.m_executionEvidence);
        EXPECT_NE(report.m_status, AdapterPostDeploymentReportStatus::ReviewReady);
        EXPECT_TRUE(HasReportBlockerCode(
            report,
            "post_deployment.work_order_binding_mismatch"));
    }
} // namespace TaintedGrailModdingSDK
