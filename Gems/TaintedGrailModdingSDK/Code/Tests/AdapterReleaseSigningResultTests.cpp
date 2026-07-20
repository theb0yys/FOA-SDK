/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseSigningEvidenceService.h"
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
            artifact.m_artifactId = "release.artifact.signing.one";
            artifact.m_reconciliationId = "reconciliation.signing.one";
            artifact.m_packagePreviewId = "package.preview.signing.one";
            artifact.m_manifestId = "manifest.signing.one";
            artifact.m_manifestFingerprint = Fingerprint('1');
            artifact.m_packId = "pack.signing.one";
            artifact.m_packVersion = "1.0.0";
            artifact.m_profileId = "profile.signing.one";
            artifact.m_gameVersion = "1.0.0";
            artifact.m_branch = "stable";
            artifact.m_runtimeTarget = "Mono";
            artifact.m_status = AdapterReleaseArtifactEnvelopeStatus::Ready;
            artifact.m_metadataReady = true;
            artifact.m_canonicalJson =
                "{\"ArtifactId\":\"release.artifact.signing.one\","
                "\"ContractVersion\":1}";

            AdapterReleaseArtifactContent content;
            content.m_contentId = "content.signing.one";
            content.m_packagePath = "package/content.bin";
            content.m_expectedChecksum = Fingerprint('2');
            artifact.m_contents.push_back(content);
            artifact.m_contentCount = 1;

            artifact.m_signingIntent.m_intentId = "signing.intent.one";
            artifact.m_signingIntent.m_decision =
                AdapterReleaseSigningIntentDecision::SignExternally;
            artifact.m_signingIntent.m_identityKind =
                AdapterReleaseSigningIdentityKind::Pgp;
            artifact.m_signingIntent.m_signerId = "signer.release.one";
            artifact.m_signingIntent.m_identityLocator =
                "signing-identities/release-one";
            artifact.m_signingIntent.m_identityFingerprint = Fingerprint('3');
            artifact.m_signingIntent.m_reviewer = "release-reviewer";
            artifact.m_signingIntent.m_evidenceIds = {
                "evidence.signing.intent.one",
            };
            artifact.m_signingIntent.m_reviewedAtUtc =
                "2026-07-20T10:00:00Z";
            return artifact;
        }

        AdapterReleaseAssemblyResultEnvelope BuildAssemblyResult(
            const AdapterReleaseArtifactEnvelope& artifact)
        {
            AdapterReleaseAssemblyResultEnvelope envelope;
            envelope.m_resultId = "release.assembly.signing.one";
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
            envelope.m_capturedAtUtc = "2026-07-20T11:45:00Z";
            envelope.m_resultFingerprint = Fingerprint('4');

            envelope.m_assemblerReview.m_reviewId = "review.assembler.signing.one";
            envelope.m_assemblerReview.m_assemblerId =
                "assembler.external.signing.one";
            envelope.m_assemblerReview.m_assemblerVersion = "1.0.0";
            envelope.m_assemblerReview.m_assemblerFingerprint = Fingerprint('5');
            envelope.m_assemblerReview.m_decision =
                AdapterReleaseAssemblerReviewDecision::Accepted;
            envelope.m_assemblerReview.m_reviewer = "assembly-reviewer";
            envelope.m_assemblerReview.m_capabilities = {
                AdapterReleaseAssemblerCapability::ContentChecksum,
                AdapterReleaseAssemblerCapability::ArchiveAssembly,
                AdapterReleaseAssemblerCapability::ArchiveChecksum,
            };
            envelope.m_assemblerReview.m_evidenceIds = {
                "evidence.assembler.review.one",
            };
            envelope.m_assemblerReview.m_reviewedAtUtc =
                "2026-07-20T10:30:00Z";

            envelope.m_archive.m_archiveId = "archive.release.signing.one";
            envelope.m_archive.m_archivePath = "archives/release-signing-one.zip";
            envelope.m_archive.m_archiveFormat = "zip";
            envelope.m_archive.m_outcome =
                AdapterReleaseAssemblyOutcome::Succeeded;
            envelope.m_archive.m_attempted = true;
            envelope.m_archive.m_archivePresent = true;
            envelope.m_archive.m_byteSize = 4096;
            envelope.m_archive.m_archiveFingerprint = Fingerprint('6');
            envelope.m_archive.m_completedAtUtc =
                "2026-07-20T11:30:00Z";

            AdapterReleaseChecksumObservation observation;
            observation.m_observationId = "observation.signing.content.one";
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
            observation.m_observedAtUtc = "2026-07-20T11:15:00Z";
            envelope.m_checksumObservations.push_back(observation);
            return envelope;
        }

        AdapterReleaseAssemblyEvidenceReturn BuildAssemblyEvidence(
            const AdapterReleaseArtifactEnvelope& artifact,
            const AdapterReleaseAssemblyResultEnvelope& assembly)
        {
            return AdapterReleaseAssemblyEvidenceService{}.BuildEvidenceReturn(
                artifact,
                assembly);
        }

        AdapterReleaseSigningResultEnvelope BuildSucceededSigningResult(
            const AdapterReleaseArtifactEnvelope& artifact,
            const AdapterReleaseAssemblyResultEnvelope& assembly)
        {
            AdapterReleaseSigningResultEnvelope envelope;
            envelope.m_resultId = "release.signing.result.one";
            envelope.m_resultFingerprint = Fingerprint('7');
            envelope.m_artifactId = artifact.m_artifactId;
            envelope.m_artifactFingerprint =
                CalculateCanonicalSha256(artifact.m_canonicalJson);
            envelope.m_assemblyResultId = assembly.m_resultId;
            envelope.m_assemblyResultFingerprint =
                assembly.m_resultFingerprint;
            envelope.m_archiveId = assembly.m_archive.m_archiveId;
            envelope.m_archivePath = assembly.m_archive.m_archivePath;
            envelope.m_archiveFormat = assembly.m_archive.m_archiveFormat;
            envelope.m_archiveByteSize = assembly.m_archive.m_byteSize;
            envelope.m_archiveFingerprint =
                assembly.m_archive.m_archiveFingerprint;
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
            envelope.m_signingIntentId = artifact.m_signingIntent.m_intentId;
            envelope.m_signingIntentDecision =
                artifact.m_signingIntent.m_decision;
            envelope.m_signingIdentityKind =
                artifact.m_signingIntent.m_identityKind;
            envelope.m_signerId = artifact.m_signingIntent.m_signerId;
            envelope.m_identityLocator =
                artifact.m_signingIntent.m_identityLocator;
            envelope.m_identityFingerprint =
                artifact.m_signingIntent.m_identityFingerprint;

            envelope.m_signerReview.m_reviewId = "review.signer.one";
            envelope.m_signerReview.m_signerToolId = "signer.tool.one";
            envelope.m_signerReview.m_signerToolVersion = "1.0.0";
            envelope.m_signerReview.m_signerToolFingerprint = Fingerprint('8');
            envelope.m_signerReview.m_decision =
                AdapterReleaseSignerReviewDecision::Accepted;
            envelope.m_signerReview.m_reviewer = "signer-reviewer";
            envelope.m_signerReview.m_capabilities = {
                AdapterReleaseSignerCapability::ArchiveSigning,
                AdapterReleaseSignerCapability::SignatureArtifactFingerprint,
            };
            envelope.m_signerReview.m_evidenceIds = {
                "evidence.signer.review.one",
            };
            envelope.m_signerReview.m_reviewedAtUtc =
                "2026-07-20T11:50:00Z";

            envelope.m_attempted = true;
            envelope.m_outcome = AdapterReleaseSigningOutcome::Succeeded;
            envelope.m_completedAtUtc = "2026-07-20T12:30:00Z";
            envelope.m_capturedAtUtc = "2026-07-20T13:00:00Z";

            AdapterReleaseSignatureArtifact signatureArtifact;
            signatureArtifact.m_signatureArtifactId = "signature.artifact.one";
            signatureArtifact.m_kind =
                AdapterReleaseSignatureArtifactKind::DetachedSignature;
            signatureArtifact.m_reference =
                "signatures/release-signing-one.zip.sig";
            signatureArtifact.m_mediaType = "application/pgp-signature";
            signatureArtifact.m_byteSize = 96;
            signatureArtifact.m_fingerprint = Fingerprint('9');
            signatureArtifact.m_createdAtUtc = "2026-07-20T12:25:00Z";
            signatureArtifact.m_archiveId = envelope.m_archiveId;
            signatureArtifact.m_archiveFingerprint =
                envelope.m_archiveFingerprint;
            envelope.m_signatureArtifacts.push_back(signatureArtifact);
            return envelope;
        }

        AdapterReleaseSigningEvidenceReturn BuildSigningEvidence(
            const AdapterReleaseArtifactEnvelope& artifact,
            const AdapterReleaseAssemblyResultEnvelope& assembly,
            const AdapterReleaseSigningResultEnvelope& signing)
        {
            const AdapterReleaseAssemblyEvidenceReturn assemblyEvidence =
                BuildAssemblyEvidence(artifact, assembly);
            return AdapterReleaseSigningEvidenceService{}.BuildEvidenceReturn(
                artifact,
                assembly,
                assemblyEvidence,
                signing);
        }
    } // namespace

    TEST(AdapterReleaseSigningResultTests, ExactSucceededResultReturnsCandidateEvidence)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        const AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_TRUE(result.m_contractValid);
        EXPECT_TRUE(result.m_accepted);
        EXPECT_EQ(result.m_status, AdapterReleaseSigningEnvelopeStatus::Accepted);
        EXPECT_EQ(result.m_signatureArtifactCount, 1);
        EXPECT_EQ(result.m_failureCount, 0);
        EXPECT_EQ(result.m_sourceDocumentCount, 1);
        EXPECT_GE(result.m_evidenceRecordCount, 6);
        EXPECT_FALSE(result.m_filesRead);
        EXPECT_FALSE(result.m_filesHashed);
        EXPECT_FALSE(result.m_archiveOpened);
        EXPECT_FALSE(result.m_archiveModified);
        EXPECT_FALSE(result.m_keysLoaded);
        EXPECT_FALSE(result.m_identityResolved);
        EXPECT_FALSE(result.m_signingPerformed);
        EXPECT_FALSE(result.m_signatureVerified);
        EXPECT_FALSE(result.m_signatureArtifactsWritten);
        EXPECT_FALSE(result.m_uploadPerformed);
        EXPECT_FALSE(result.m_releasePublished);
        EXPECT_FALSE(result.m_launchPerformed);
        EXPECT_FALSE(result.m_adapterCalled);
        EXPECT_FALSE(result.m_deploymentMutated);
        EXPECT_FALSE(result.m_saveMutated);
    }

    TEST(AdapterReleaseSigningResultTests, RejectedAssemblyEvidenceFailsFirst)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseAssemblyEvidenceReturn assemblyEvidence =
            BuildAssemblyEvidence(artifact, assembly);
        assemblyEvidence.m_accepted = false;
        assemblyEvidence.m_contractValid = false;
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        signing.m_signerReview.m_decision =
            AdapterReleaseSignerReviewDecision::Rejected;

        const AdapterReleaseSigningEvidenceReturn result =
            AdapterReleaseSigningEvidenceService{}.BuildEvidenceReturn(
                artifact,
                assembly,
                assemblyEvidence,
                signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::AssemblyResultNotAccepted);
        EXPECT_TRUE(result.m_sourceDocuments.empty());
        EXPECT_TRUE(result.m_evidenceDocuments.empty());
    }

    TEST(AdapterReleaseSigningResultTests, UnsignedIntentCannotBecomeSigningEvidence)
    {
        AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        artifact.m_signingIntent.m_decision =
            AdapterReleaseSigningIntentDecision::Unsigned;
        artifact.m_signingIntent.m_identityKind =
            AdapterReleaseSigningIdentityKind::None;
        artifact.m_signingIntent.m_signerId.clear();
        artifact.m_signingIntent.m_identityLocator.clear();
        artifact.m_signingIntent.m_identityFingerprint.clear();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        const AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::SigningNotRequested);
        EXPECT_TRUE(result.m_sourceDocuments.empty());
    }

    TEST(AdapterReleaseSigningResultTests, MissingSignerCapabilityIsRejected)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        signing.m_signerReview.m_capabilities.pop_back();

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::SignerUnreviewed);
    }

    TEST(AdapterReleaseSigningResultTests, ArchiveFingerprintDriftFailsClosed)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        signing.m_archiveFingerprint = Fingerprint('a');
        signing.m_signatureArtifacts[0].m_archiveFingerprint =
            signing.m_archiveFingerprint;

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::AssemblyBindingMismatch);
    }

    TEST(AdapterReleaseSigningResultTests, SigningIntentIdentityDriftFailsClosed)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        signing.m_signerId = "signer.release.different";

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::SigningIntentBindingMismatch);
    }

    TEST(AdapterReleaseSigningResultTests, MalformedResultFingerprintIsRejected)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        signing.m_resultFingerprint = "SHA256:" + AZStd::string(64, 'a');

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::EnvelopeInvalid);
    }

    TEST(AdapterReleaseSigningResultTests, SucceededOutcomeRequiresSignatureArtifact)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        signing.m_signatureArtifacts.clear();

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::SigningOutcomeMismatch);
    }

    TEST(AdapterReleaseSigningResultTests, ValidFailedOutcomeRemainsAcceptedEvidence)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        signing.m_outcome = AdapterReleaseSigningOutcome::Failed;

        AdapterReleaseSigningFailure failure;
        failure.m_failureId = "failure.signing.one";
        failure.m_kind = AdapterReleaseSigningFailureKind::Signing;
        failure.m_code = "signing_failed";
        failure.m_message = "The external signer reported a signing failure.";
        failure.m_signatureArtifactId =
            signing.m_signatureArtifacts[0].m_signatureArtifactId;
        signing.m_failures.push_back(failure);
        signing.m_failureIds.push_back(failure.m_failureId);

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_TRUE(result.m_contractValid);
        EXPECT_TRUE(result.m_accepted);
        EXPECT_EQ(result.m_status, AdapterReleaseSigningEnvelopeStatus::Accepted);
        EXPECT_EQ(result.m_failureCount, 1);
        EXPECT_EQ(result.m_signatureArtifactCount, 1);
    }

    TEST(AdapterReleaseSigningResultTests, UnsafeSignatureReferenceIsRejected)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        signing.m_signatureArtifacts[0].m_reference =
            "C:/private/release-signing-one.zip.sig";

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::SignatureArtifactBindingMismatch);
    }

    TEST(AdapterReleaseSigningResultTests, CaseFoldedSignaturePathCollisionIsRejected)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        AdapterReleaseSignatureArtifact duplicate =
            signing.m_signatureArtifacts[0];
        duplicate.m_signatureArtifactId = "signature.artifact.two";
        duplicate.m_reference = "SIGNATURES/RELEASE-SIGNING-ONE.ZIP.SIG";
        duplicate.m_fingerprint = Fingerprint('b');
        signing.m_signatureArtifacts.push_back(duplicate);

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::SignatureArtifactBindingMismatch);
    }

    TEST(AdapterReleaseSigningResultTests, ArtifactChronologyAfterCompletionIsRejected)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        signing.m_signatureArtifacts[0].m_createdAtUtc =
            "2026-07-20T12:45:00Z";

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::SignatureArtifactBindingMismatch);
    }

    TEST(AdapterReleaseSigningResultTests, OrphanDiagnosticReferenceIsRejected)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        signing.m_diagnosticReferenceIds = { "diagnostic.orphan.one" };

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::FailureDiagnosticBindingMismatch);
    }

    TEST(AdapterReleaseSigningResultTests, NonReciprocalDiagnosticBindingIsRejected)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);

        AdapterReleaseSigningDiagnosticReference diagnostic;
        diagnostic.m_diagnosticId = "diagnostic.signing.one";
        diagnostic.m_kind = AdapterReleaseSigningDiagnosticKind::Signer;
        diagnostic.m_reference = "diagnostics/signer-one.log";
        diagnostic.m_fingerprint = Fingerprint('c');
        diagnostic.m_signatureArtifactIds = {
            signing.m_signatureArtifacts[0].m_signatureArtifactId,
        };
        signing.m_diagnosticReferences.push_back(diagnostic);
        signing.m_diagnosticReferenceIds.push_back(diagnostic.m_diagnosticId);

        const AdapterReleaseSigningEvidenceReturn result =
            BuildSigningEvidence(artifact, assembly, signing);

        EXPECT_FALSE(result.m_contractValid);
        EXPECT_EQ(
            result.m_status,
            AdapterReleaseSigningEnvelopeStatus::FailureDiagnosticBindingMismatch);
    }

    TEST(AdapterReleaseSigningResultTests, CandidateEvidenceOrderingIsDeterministic)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        AdapterReleaseSigningResultEnvelope left =
            BuildSucceededSigningResult(artifact, assembly);

        AdapterReleaseSignatureArtifact second = left.m_signatureArtifacts[0];
        second.m_signatureArtifactId = "signature.artifact.two";
        second.m_reference = "signatures/release-signing-one.zip.asc";
        second.m_fingerprint = Fingerprint('d');
        left.m_signatureArtifacts.push_back(second);
        AdapterReleaseSigningResultEnvelope right = left;
        right.m_signatureArtifacts[0] = left.m_signatureArtifacts[1];
        right.m_signatureArtifacts[1] = left.m_signatureArtifacts[0];

        const AdapterReleaseSigningEvidenceReturn leftResult =
            BuildSigningEvidence(artifact, assembly, left);
        const AdapterReleaseSigningEvidenceReturn rightResult =
            BuildSigningEvidence(artifact, assembly, right);

        ASSERT_TRUE(leftResult.m_contractValid);
        ASSERT_TRUE(rightResult.m_contractValid);
        ASSERT_EQ(
            leftResult.m_evidenceDocuments.size(),
            rightResult.m_evidenceDocuments.size());
        ASSERT_FALSE(leftResult.m_evidenceDocuments.empty());
        const AZStd::vector<EvidenceRecord>& leftEvidence =
            leftResult.m_evidenceDocuments[0].m_evidence;
        const AZStd::vector<EvidenceRecord>& rightEvidence =
            rightResult.m_evidenceDocuments[0].m_evidence;
        ASSERT_EQ(leftEvidence.size(), rightEvidence.size());
        for (size_t index = 0; index < leftEvidence.size(); ++index)
        {
            EXPECT_EQ(
                leftEvidence[index].m_evidenceId,
                rightEvidence[index].m_evidenceId);
        }
    }

    TEST(AdapterReleaseSigningResultTests, ValidationDoesNotMutateInputs)
    {
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        const AdapterReleaseAssemblyEvidenceReturn assemblyEvidence =
            BuildAssemblyEvidence(artifact, assembly);
        const AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        const AZStd::string originalReference =
            signing.m_signatureArtifacts[0].m_reference;
        const AZStd::string originalArtifactFingerprint =
            signing.m_artifactFingerprint;
        const size_t originalArtifactCount = signing.m_signatureArtifacts.size();

        const AdapterReleaseSigningEvidenceReturn result =
            AdapterReleaseSigningEvidenceService{}.BuildEvidenceReturn(
                artifact,
                assembly,
                assemblyEvidence,
                signing);

        EXPECT_TRUE(result.m_contractValid);
        EXPECT_EQ(signing.m_signatureArtifacts.size(), originalArtifactCount);
        EXPECT_EQ(
            signing.m_signatureArtifacts[0].m_reference,
            originalReference);
        EXPECT_EQ(signing.m_artifactFingerprint, originalArtifactFingerprint);
    }

    TEST(AdapterReleaseSigningResultTests, RegistryRejectsDuplicateResultIdentity)
    {
        AdapterReleaseSigningResultRegistry& registry =
            AdapterReleaseSigningResultRegistry::Get();
        registry.Clear();
        const AdapterReleaseArtifactEnvelope artifact = BuildReadyArtifact();
        const AdapterReleaseAssemblyResultEnvelope assembly =
            BuildAssemblyResult(artifact);
        const AdapterReleaseSigningResultEnvelope signing =
            BuildSucceededSigningResult(artifact, assembly);
        AZStd::string error;

        EXPECT_TRUE(registry.RegisterEnvelope(signing, &error));
        EXPECT_TRUE(error.empty());
        EXPECT_FALSE(registry.RegisterEnvelope(signing, &error));
        EXPECT_FALSE(error.empty());
        ASSERT_NE(registry.FindByResultId(signing.m_resultId), nullptr);
        EXPECT_EQ(registry.GetEnvelopes().size(), 1);
        registry.Clear();
    }
} // namespace TaintedGrailModdingSDK
