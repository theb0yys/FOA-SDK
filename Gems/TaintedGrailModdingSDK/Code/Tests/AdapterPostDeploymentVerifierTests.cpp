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
        AdapterPostDeploymentVerifierTests,
        ExactAllMatchedVerifierEvidenceIsAccepted)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        EXPECT_TRUE(fixture.m_verifierEvidence.m_contractValid);
        EXPECT_TRUE(fixture.m_verifierEvidence.m_accepted);
        EXPECT_EQ(
            fixture.m_verifierEvidence.m_status,
            AdapterPostDeploymentVerifierEnvelopeStatus::Accepted);
        EXPECT_EQ(
            fixture.m_verifierEvidence.m_checkCount,
            fixture.m_verifierEnvelope.m_checkResults.size());
        EXPECT_FALSE(fixture.m_verifierEvidence.m_sourceDocuments.empty());
        EXPECT_FALSE(fixture.m_verifierEvidence.m_evidenceDocuments.empty());
    }

    TEST(
        AdapterPostDeploymentVerifierTests,
        NotRunRemainsContractValidAdverseEvidence)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        AdapterPostDeploymentVerifierResultEnvelope envelope =
            fixture.m_verifierEnvelope;
        ASSERT_FALSE(envelope.m_checkResults.empty());
        AdapterPostDeploymentVerifierCheckResult& check =
            envelope.m_checkResults.front();
        check.m_attempted = false;
        check.m_observationRecorded = false;
        check.m_observedPresent = false;
        check.m_observedFingerprint.clear();
        check.m_checkedAtUtc.clear();
        check.m_failureIds.clear();
        check.m_diagnosticReferenceIds.clear();
        check.m_outcome = AdapterPostDeploymentVerifierCheckOutcome::NotRun;
        envelope.m_resultFingerprint =
            CalculatePostDeploymentVerifierResultFingerprint(envelope);

        AdapterPostDeploymentVerifierEvidenceService service;
        const AdapterPostDeploymentVerifierEvidenceReturn result =
            service.BuildEvidenceReturn(
                fixture.m_workOrder,
                fixture.m_executionEnvelope,
                fixture.m_report,
                fixture.m_sourceRegistry,
                envelope);
        EXPECT_TRUE(result.m_contractValid);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_EQ(
            result.m_status,
            AdapterPostDeploymentVerifierEnvelopeStatus::ObservationMismatch);
        EXPECT_EQ(result.m_notRunCheckCount, 1);
        EXPECT_FALSE(result.m_sourceDocuments.empty());
        EXPECT_FALSE(result.m_evidenceDocuments.empty());
    }

    TEST(
        AdapterPostDeploymentVerifierTests,
        CallerSelectedVerifierFingerprintFailsClosed)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        AdapterPostDeploymentVerifierResultEnvelope envelope =
            fixture.m_verifierEnvelope;
        envelope.m_resultFingerprint =
            ::TaintedGrailModdingSDK::Test::FixtureFingerprint('0');

        AdapterPostDeploymentVerifierEvidenceService service;
        const AdapterPostDeploymentVerifierEvidenceReturn result =
            service.BuildEvidenceReturn(
                fixture.m_workOrder,
                fixture.m_executionEnvelope,
                fixture.m_report,
                fixture.m_sourceRegistry,
                envelope);
        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterPostDeploymentVerifierEnvelopeStatus::EnvelopeInvalid);
    }

    TEST(
        AdapterPostDeploymentVerifierTests,
        RegistryRequiresExactBoundValidation)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        AdapterPostDeploymentVerifierResultRegistry registry;
        AZStd::string error;
        EXPECT_FALSE(registry.RegisterEnvelope(fixture.m_verifierEnvelope, &error));
        EXPECT_NE(error.find("Unbound"), AZStd::string::npos);

        ASSERT_TRUE(registry.RegisterEnvelope(
            fixture.m_workOrder,
            fixture.m_executionEnvelope,
            fixture.m_report,
            fixture.m_sourceRegistry,
            fixture.m_verifierEnvelope,
            &error)) << error.c_str();
        EXPECT_FALSE(registry.RegisterEnvelope(
            fixture.m_workOrder,
            fixture.m_executionEnvelope,
            fixture.m_report,
            fixture.m_sourceRegistry,
            fixture.m_verifierEnvelope,
            &error));
    }

    TEST(
        AdapterPostDeploymentVerifierTests,
        UnknownFailureKindIsNotRecognised)
    {
        AdapterPostDeploymentVerifierFailureKind kind =
            AdapterPostDeploymentVerifierFailureKind::Unknown;
        EXPECT_FALSE(TryParseAdapterPostDeploymentVerifierFailureKind(
            "unknown",
            kind));
    }
} // namespace TaintedGrailModdingSDK
