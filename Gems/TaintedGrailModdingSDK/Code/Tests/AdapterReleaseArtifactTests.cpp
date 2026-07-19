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
        AdapterReleaseArtifactTests,
        CompleteDeclaredMetadataIsReadyWithoutPerformingOperations)
    {
        Test::AdapterResearchPipelineFixture fixture;
        ASSERT_TRUE(fixture.m_reconciliation.m_accepted);
        EXPECT_TRUE(fixture.m_releaseArtifact.m_ready);
        EXPECT_EQ(
            fixture.m_releaseArtifact.m_envelope.m_status,
            AdapterReleaseArtifactEnvelopeStatus::Ready);
        EXPECT_TRUE(fixture.m_releaseArtifact.m_envelope.m_metadataReady);
        EXPECT_TRUE(fixture.m_releaseArtifact.m_envelope.m_blockers.empty());
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_filesRead);
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_filesHashed);
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_checksumGenerated);
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_filesCopied);
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_archiveAssembled);
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_signingPerformed);
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_uploadPerformed);
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_releasePublished);
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_launchPerformed);
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_adapterCalled);
        EXPECT_FALSE(fixture.m_releaseArtifact.m_envelope.m_deploymentMutated);
    }

    TEST(
        AdapterReleaseArtifactTests,
        ChecksumDeclarationMustEqualReviewedPackageDigest)
    {
        Test::AdapterResearchPipelineFixture fixture;
        AdapterReleaseArtifactRequest request = fixture.m_releaseRequest;
        ASSERT_FALSE(request.m_contents.empty());
        request.m_artifactId = "owner.release-artifact.checksum-drift";
        request.m_contents.front().m_expectedChecksum =
            Test::FixtureFingerprint('0');

        AdapterReleaseArtifactProvenanceService service;
        const AdapterReleaseArtifactResult result = service.BuildEnvelope(
            fixture.m_reconciliation,
            fixture.m_packagePreview,
            request);
        EXPECT_FALSE(result.m_ready);
        EXPECT_EQ(
            result.m_envelope.m_status,
            AdapterReleaseArtifactEnvelopeStatus::ChecksumDeclarationInvalid);
        EXPECT_FALSE(result.m_envelope.m_filesRead);
        EXPECT_FALSE(result.m_envelope.m_filesHashed);
        EXPECT_FALSE(result.m_envelope.m_checksumGenerated);
    }

    TEST(
        AdapterReleaseArtifactTests,
        MetadataCannotAdvanceWithoutApprovedReconciliation)
    {
        Test::AdapterResearchPipelineFixture fixture;
        AdapterVerifierEvidenceReconciliationResult reconciliation =
            fixture.m_reconciliation;
        reconciliation.m_accepted = false;
        reconciliation.m_envelope.m_releaseDecision =
            AdapterVerifierReleaseDecision::Hold;

        AdapterReleaseArtifactProvenanceService service;
        const AdapterReleaseArtifactResult result = service.BuildEnvelope(
            reconciliation,
            fixture.m_packagePreview,
            fixture.m_releaseRequest);
        EXPECT_FALSE(result.m_ready);
        EXPECT_EQ(
            result.m_envelope.m_status,
            AdapterReleaseArtifactEnvelopeStatus::ReconciliationNotApproved);
    }

    TEST(
        AdapterReleaseArtifactTests,
        RegistryRejectsNonReadyOrOperationalEnvelope)
    {
        Test::AdapterResearchPipelineFixture fixture;
        AdapterReleaseArtifactRegistry registry;
        AZStd::string error;
        ASSERT_TRUE(registry.RegisterEnvelope(
            fixture.m_releaseArtifact.m_envelope,
            &error)) << error.c_str();

        AdapterReleaseArtifactEnvelope unsafe =
            fixture.m_releaseArtifact.m_envelope;
        unsafe.m_artifactId = "owner.release-artifact.unsafe";
        unsafe.m_signingPerformed = true;
        EXPECT_FALSE(registry.RegisterEnvelope(unsafe, &error));

        AdapterReleaseArtifactEnvelope blocked =
            fixture.m_releaseArtifact.m_envelope;
        blocked.m_artifactId = "owner.release-artifact.blocked";
        blocked.m_status = AdapterReleaseArtifactEnvelopeStatus::BindingMismatch;
        blocked.m_metadataReady = false;
        EXPECT_FALSE(registry.RegisterEnvelope(blocked, &error));
    }
} // namespace TaintedGrailModdingSDK
