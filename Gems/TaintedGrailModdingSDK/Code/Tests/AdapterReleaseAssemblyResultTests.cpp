/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseAssemblyEvidenceService.h"
#include "CanonicalFingerprint.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string Fingerprint(char digit)
        {
            return "sha256:" + AZStd::string(64, digit);
        }

        AdapterReleaseArtifactEnvelope BuildReadyArtifact()
        {
            AdapterReleaseArtifactEnvelope artifact;
            artifact.m_artifactId = "release.artifact.one";
            artifact.m_reconciliationId = "reconciliation.one";
            artifact.m_packagePreviewId = "package.preview.one";
            artifact.m_manifestId = "manifest.one";
            artifact.m_manifestFingerprint = Fingerprint('1');
            artifact.m_packId = "pack.one";
            artifact.m_packVersion = "1.0.0";
            artifact.m_profileId = "profile.one";
            artifact.m_gameVersion = "1.0.0";
            artifact.m_branch = "stable";
            artifact.m_runtimeTarget = "windows.x64";
            artifact.m_status = AdapterReleaseArtifactEnvelopeStatus::Ready;
            artifact.m_metadataReady = true;
            artifact.m_canonicalJson =
                "{\"ArtifactId\":\"release.artifact.one\",\"ContractVersion\":1}";

            AdapterReleaseArtifactContent content;
            content.m_contentId = "content.one";
            content.m_packagePath = "package/content.bin";
            content.m_expectedChecksum = Fingerprint('2');
            artifact.m_contents.push_back(content);
            artifact.m_contentCount = 1;
            return artifact;
        }

        AdapterReleaseAssemblyResultEnvelope BuildMatchedResult(
            const AdapterReleaseArtifactEnvelope& artifact)
        {
            AdapterReleaseAssemblyResultEnvelope envelope;
            envelope.m_resultId = "release.assembly.result.one";
            envelope.m_artifactId = artifact.m_artifactId;
            envelope.m_artifactCanonicalJson = artifact.m_canonicalJson;
            envelope.m_artifactFingerprint =
                CalculateCanonicalSha256(artifact.m_canonicalJson);
            envelope.m_reconciliationId = artifact.m_reconciliationId;
            envelope.m_packagePreviewId = artifact.m_packagePreviewId;
            envelope.m_manifestId = artifact.m_manifestId;
            envelope.m_manifestFingerprint = artifact.m_manifestFingerprint;
            envelope.m_packId = artifact.m_packId;
            envelope.m_packVersion = artifact.m_packVersion;
            envelope.m_profileId = artifact.m_profileId;
            envelope.m_gameVersion = artifact.m_gameVersion;
            envelope.m_branch = artifact.m_branch;
            envelope.m_runtimeTarget = artifact.m_runtimeTarget;
            envelope.m_capturedAtUtc = "2026-07-19T12:00:00Z";
            envelope.m_resultFingerprint = Fingerprint('3');

            envelope.m_assemblerReview.m_reviewId = "review.assembler.one";
            envelope.m_assemblerReview.m_assemblerId = "assembler.external.one";
            envelope.m_assemblerReview.m_assemblerVersion = "1.0.0";
            envelope.m_assemblerReview.m_assemblerFingerprint = Fingerprint('4');
            envelope.m_assemblerReview.m_decision =
                AdapterReleaseAssemblerReviewDecision::Accepted;
            envelope.m_assemblerReview.m_reviewer = "release-reviewer";
            envelope.m_assemblerReview.m_capabilities = {
                AdapterReleaseAssemblerCapability::ContentChecksum,
                AdapterReleaseAssemblerCapability::ArchiveAssembly,
                AdapterReleaseAssemblerCapability::ArchiveChecksum,
            };
            envelope.m_assemblerReview.m_evidenceIds = { "evidence.review.one" };
            envelope.m_assemblerReview.m_reviewedAtUtc =
                "2026-07-19T11:00:00Z";

            envelope.m_archive.m_archiveId = "archive.release.one";
            envelope.m_archive.m_archivePath = "archives/release-one.zip";
            envelope.m_archive.m_archiveFormat = "zip";
            envelope.m_archive.m_outcome = AdapterReleaseAssemblyOutcome::Succeeded;
            envelope.m_archive.m_attempted = true;
            envelope.m_archive.m_archivePresent = true;
            envelope.m_archive.m_byteSize = 128;
            envelope.m_archive.m_archiveFingerprint = Fingerprint('5');
            envelope.m_archive.m_completedAtUtc = "2026-07-19T11:30:00Z";

            AdapterReleaseChecksumObservation observation;
            observation.m_observationId = "observation.content.one";
            observation.m_contentId = artifact.m_contents[0].m_contentId;
            observation.m_packagePath = artifact.m_contents[0].m_packagePath;
            observation.m_expectedChecksum =
                artifact.m_contents[0].m_expectedChecksum;
            observation.m_observedChecksum =
                artifact.m_contents[0].m_expectedChecksum;
            observation.m_attempted = true;
            observation.m_observationRecorded = true;
            observation.m_outcome =
                AdapterReleaseChecksumObservationOutcome::Matched;
            observation.m_observedAtUtc = "2026-07-19T11:20:00Z";
            envelope.m_checksumObservations.push_back(observation);
            return envelope;
        }
    } // namespace

    TEST(AdapterReleaseAssemblyResultTests, ExactMatchedResultReturnsCandidateEvidence)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope envelope =
            BuildMatchedResult(artifact);

        const AdapterReleaseAssemblyEvidenceReturn result =
            AdapterReleaseAssemblyEvidenceService{}.BuildEvidenceReturn(
                artifact,
                envelope);

        EXPECT_TRUE(result.m_contractValid);
        EXPECT_TRUE(result.m_accepted);
        EXPECT_EQ(result.m_status, AdapterReleaseAssemblyEnvelopeStatus::Accepted);
        EXPECT_EQ(result.m_checksumObservationCount, 1);
        EXPECT_EQ(result.m_matchedChecksumCount, 1);
        EXPECT_EQ(result.m_sourceDocumentCount, 1);
        EXPECT_GT(result.m_evidenceRecordCount, 3);
        EXPECT_FALSE(result.m_filesRead);
        EXPECT_FALSE(result.m_filesHashed);
        EXPECT_FALSE(result.m_archiveAssembled);
        EXPECT_FALSE(result.m_signingPerformed);
        EXPECT_FALSE(result.m_uploadPerformed);
        EXPECT_FALSE(result.m_releasePublished);
        EXPECT_FALSE(result.m_launchPerformed);
        EXPECT_FALSE(result.m_adapterCalled);
        EXPECT_FALSE(result.m_deploymentMutated);
    }

    TEST(AdapterReleaseAssemblyResultTests, DifferentCanonicalArtifactIsRejected)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        AdapterReleaseAssemblyResultEnvelope envelope = BuildMatchedResult(artifact);
        envelope.m_artifactCanonicalJson += " ";

        const AdapterReleaseAssemblyEvidenceReturn result =
            AdapterReleaseAssemblyEvidenceService{}.BuildEvidenceReturn(
                artifact,
                envelope);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseAssemblyEnvelopeStatus::ArtifactBindingMismatch);
        EXPECT_TRUE(result.m_sourceDocuments.empty());
        EXPECT_TRUE(result.m_evidenceDocuments.empty());
    }

    TEST(AdapterReleaseAssemblyResultTests, MissingContentObservationFailsClosed)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        AdapterReleaseAssemblyResultEnvelope envelope = BuildMatchedResult(artifact);
        envelope.m_checksumObservations.clear();

        const AdapterReleaseAssemblyEvidenceReturn result =
            AdapterReleaseAssemblyEvidenceService{}.BuildEvidenceReturn(
                artifact,
                envelope);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseAssemblyEnvelopeStatus::ContentCoverageIncomplete);
    }

    TEST(AdapterReleaseAssemblyResultTests, ValidAdverseOutcomesRemainAccepted)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        AdapterReleaseAssemblyResultEnvelope envelope = BuildMatchedResult(artifact);
        envelope.m_checksumObservations[0].m_observedChecksum = Fingerprint('6');
        envelope.m_checksumObservations[0].m_outcome =
            AdapterReleaseChecksumObservationOutcome::Mismatched;

        AdapterReleaseAssemblyFailure failure;
        failure.m_failureId = "failure.archive.one";
        failure.m_kind = AdapterReleaseAssemblyFailureKind::Archive;
        failure.m_code = "archive_failed";
        failure.m_message = "The external assembler reported archive failure.";
        envelope.m_failures.push_back(failure);
        envelope.m_archive.m_outcome = AdapterReleaseAssemblyOutcome::Failed;
        envelope.m_archive.m_archivePresent = false;
        envelope.m_archive.m_byteSize = 0;
        envelope.m_archive.m_archiveFingerprint.clear();
        envelope.m_archive.m_failureIds = { failure.m_failureId };

        const AdapterReleaseAssemblyEvidenceReturn result =
            AdapterReleaseAssemblyEvidenceService{}.BuildEvidenceReturn(
                artifact,
                envelope);

        EXPECT_TRUE(result.m_contractValid);
        EXPECT_TRUE(result.m_accepted);
        EXPECT_EQ(result.m_mismatchedChecksumCount, 1);
        EXPECT_EQ(result.m_failureCount, 1);
    }

    TEST(AdapterReleaseAssemblyResultTests, UnsafeDiagnosticReferenceIsRejected)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        AdapterReleaseAssemblyResultEnvelope envelope = BuildMatchedResult(artifact);
        AdapterReleaseAssemblyDiagnosticReference diagnostic;
        diagnostic.m_diagnosticId = "diagnostic.unsafe.one";
        diagnostic.m_reference = "C:/private/release.log";
        diagnostic.m_fingerprint = Fingerprint('7');
        envelope.m_diagnosticReferences.push_back(diagnostic);

        const AdapterReleaseAssemblyEvidenceReturn result =
            AdapterReleaseAssemblyEvidenceService{}.BuildEvidenceReturn(
                artifact,
                envelope);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseAssemblyEnvelopeStatus::FailureDiagnosticBindingMismatch);
    }

    TEST(AdapterReleaseAssemblyResultTests, RegistryRejectsDuplicateResultIdentity)
    {
        AdapterReleaseAssemblyResultRegistry& registry =
            AdapterReleaseAssemblyResultRegistry::Get();
        registry.Clear();
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope envelope =
            BuildMatchedResult(artifact);
        AZStd::string error;

        EXPECT_TRUE(registry.RegisterEnvelope(envelope, &error));
        EXPECT_TRUE(error.empty());
        EXPECT_FALSE(registry.RegisterEnvelope(envelope, &error));
        EXPECT_FALSE(error.empty());
        ASSERT_NE(registry.FindByResultId(envelope.m_resultId), nullptr);
        EXPECT_EQ(registry.GetEnvelopes().size(), 1);
        registry.Clear();
    }
} // namespace TaintedGrailModdingSDK
