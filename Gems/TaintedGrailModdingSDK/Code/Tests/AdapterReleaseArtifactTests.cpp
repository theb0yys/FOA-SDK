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
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
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
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        AdapterReleaseArtifactRequest request = fixture.m_releaseRequest;
        ASSERT_FALSE(request.m_contents.empty());
        request.m_contents.front().m_expectedChecksum =
            ::TaintedGrailModdingSDK::Test::FixtureFingerprint('0');

        AdapterReleaseArtifactProvenanceService service;
        const AdapterReleaseArtifactResult result = service.BuildEnvelope(
            fixture.m_reconciliation,
            fixture.m_packagePreview,
            fixture.m_sourceRegistry,
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
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        AdapterVerifierEvidenceReconciliationResult reconciliation =
            fixture.m_reconciliation;
        reconciliation.m_accepted = false;
        reconciliation.m_envelope.m_releaseDecision =
            AdapterVerifierReleaseDecision::Hold;

        AdapterReleaseArtifactProvenanceService service;
        const AdapterReleaseArtifactResult result = service.BuildEnvelope(
            reconciliation,
            fixture.m_packagePreview,
            fixture.m_sourceRegistry,
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
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        AdapterReleaseArtifactRegistry registry;
        AZStd::string error;
        ASSERT_TRUE(registry.RegisterRequest(
            fixture.m_reconciliation,
            fixture.m_packagePreview,
            fixture.m_sourceRegistry,
            fixture.m_releaseRequest,
            &error)) << error.c_str();

        AdapterReleaseArtifactRequest unsafe = fixture.m_releaseRequest;
        unsafe.m_artifactId = "owner.release-artifact.unsafe";
        unsafe.m_publicationTargets.front().m_locator = "../outside";
        EXPECT_FALSE(registry.RegisterRequest(
            fixture.m_reconciliation,
            fixture.m_packagePreview,
            fixture.m_sourceRegistry,
            unsafe,
            &error));

        AdapterVerifierEvidenceReconciliationResult blocked =
            fixture.m_reconciliation;
        blocked.m_accepted = false;
        EXPECT_FALSE(registry.RegisterRequest(
            blocked,
            fixture.m_packagePreview,
            fixture.m_sourceRegistry,
            fixture.m_releaseRequest,
            &error));
    }

    TEST(
        AdapterReleaseArtifactTests,
        InventedReviewEvidenceCannotProduceReadyMetadata)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        AdapterReleaseArtifactRequest request = fixture.m_releaseRequest;
        request.m_provenance.front().m_evidenceIds = {
            "owner.evidence.invented",
        };

        const AdapterReleaseArtifactResult result =
            AdapterReleaseArtifactProvenanceService().BuildEnvelope(
                fixture.m_reconciliation,
                fixture.m_packagePreview,
                fixture.m_sourceRegistry,
                request);

        EXPECT_FALSE(result.m_ready);
        EXPECT_EQ(
            result.m_envelope.m_status,
            AdapterReleaseArtifactEnvelopeStatus::ProvenanceIncomplete);
    }

    TEST(
        AdapterReleaseArtifactTests,
        OutOfRangeSigningAndPublicationEnumsFailClosed)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        AdapterReleaseArtifactRequest invalidSigning = fixture.m_releaseRequest;
        invalidSigning.m_signingIntent.m_decision =
            static_cast<AdapterReleaseSigningIntentDecision>(255);
        AdapterReleaseArtifactResult result =
            AdapterReleaseArtifactProvenanceService().BuildEnvelope(
                fixture.m_reconciliation,
                fixture.m_packagePreview,
                fixture.m_sourceRegistry,
                invalidSigning);
        EXPECT_FALSE(result.m_ready);
        EXPECT_EQ(
            result.m_envelope.m_status,
            AdapterReleaseArtifactEnvelopeStatus::SigningIntentInvalid);

        AdapterReleaseArtifactRequest invalidTarget = fixture.m_releaseRequest;
        invalidTarget.m_publicationTargets.front().m_kind =
            static_cast<AdapterReleasePublicationTargetKind>(255);
        result = AdapterReleaseArtifactProvenanceService().BuildEnvelope(
            fixture.m_reconciliation,
            fixture.m_packagePreview,
            fixture.m_sourceRegistry,
            invalidTarget);
        EXPECT_FALSE(result.m_ready);
        EXPECT_EQ(
            result.m_envelope.m_status,
            AdapterReleaseArtifactEnvelopeStatus::PublicationTargetInvalid);
    }

    TEST(
        AdapterReleaseArtifactTests,
        PublicationLocatorsRejectTraversalCredentialsAndUnboundedInput)
    {
        ::TaintedGrailModdingSDK::Test::AdapterResearchPipelineFixture fixture;
        const AZStd::vector<AZStd::string> unsafeLocators = {
            "../outside",
            "github.com/releases?token=secret",
            AZStd::string(513, 'a'),
        };
        for (size_t index = 0; index < unsafeLocators.size(); ++index)
        {
            AdapterReleaseArtifactRequest request = fixture.m_releaseRequest;
            request.m_publicationTargets.front().m_locator =
                unsafeLocators[index];

            const AdapterReleaseArtifactResult result =
                AdapterReleaseArtifactProvenanceService().BuildEnvelope(
                    fixture.m_reconciliation,
                    fixture.m_packagePreview,
                    fixture.m_sourceRegistry,
                    request);
            EXPECT_FALSE(result.m_ready);
            EXPECT_EQ(
                result.m_envelope.m_status,
                AdapterReleaseArtifactEnvelopeStatus::PublicationTargetInvalid);
        }
    }
} // namespace TaintedGrailModdingSDK
