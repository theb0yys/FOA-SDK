/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseSigningEvidenceService.h"

#include "CanonicalFingerprint.h"
#include "PackagePathValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        struct SigningValidationFlags
        {
            bool m_assemblyResultNotAccepted = false;
            bool m_signingNotRequested = false;
            bool m_signerUnreviewed = false;
            bool m_assemblyBindingMismatch = false;
            bool m_signingIntentBindingMismatch = false;
            bool m_envelopeInvalid = false;
            bool m_signingOutcomeMismatch = false;
            bool m_signatureArtifactBindingMismatch = false;
            bool m_failureDiagnosticBindingMismatch = false;
        };

        void AddSigningIssue(
            AdapterReleaseSigningEvidenceReturn& result,
            bool& flag,
            AZStd::string code,
            AZStd::string message,
            AZStd::string signatureArtifactId = {},
            AZStd::string failureId = {},
            AZStd::string diagnosticId = {})
        {
            flag = true;
            AdapterReleaseSigningIssue issue;
            issue.m_code = AZStd::move(code);
            issue.m_message = AZStd::move(message);
            issue.m_signatureArtifactId = AZStd::move(signatureArtifactId);
            issue.m_failureId = AZStd::move(failureId);
            issue.m_diagnosticId = AZStd::move(diagnosticId);
            result.m_issues.push_back(AZStd::move(issue));
        }

        bool IsBoundedPublicText(
            const AZStd::string& value,
            size_t maximumLength,
            bool requireNonEmpty = true)
        {
            if ((requireNonEmpty && value.empty()) || value.size() > maximumLength)
            {
                return false;
            }
            for (char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (byte < 0x20 || byte == 0x7f)
                {
                    return false;
                }
            }
            return true;
        }

        bool ContainsDuplicate(const AZStd::vector<AZStd::string>& values)
        {
            AZStd::vector<AZStd::string> sorted = values;
            AZStd::sort(sorted.begin(), sorted.end());
            return AZStd::adjacent_find(sorted.begin(), sorted.end())
                != sorted.end();
        }

        bool HasStableUniqueIds(
            const AZStd::vector<AZStd::string>& values,
            bool requireNonEmpty)
        {
            if (requireNonEmpty && values.empty())
            {
                return false;
            }
            for (const AZStd::string& value : values)
            {
                if (!IsAdapterReleaseSigningStableId(value))
                {
                    return false;
                }
            }
            return !ContainsDuplicate(values);
        }

        bool HaveSameValues(
            AZStd::vector<AZStd::string> left,
            AZStd::vector<AZStd::string> right)
        {
            if (left.size() != right.size())
            {
                return false;
            }
            AZStd::sort(left.begin(), left.end());
            AZStd::sort(right.begin(), right.end());
            for (size_t index = 0; index < left.size(); ++index)
            {
                if (left[index] != right[index])
                {
                    return false;
                }
            }
            return true;
        }

        bool HasCapability(
            const AdapterReleaseSignerReview& review,
            AdapterReleaseSignerCapability capability)
        {
            return AZStd::find(
                       review.m_capabilities.begin(),
                       review.m_capabilities.end(),
                       capability)
                != review.m_capabilities.end();
        }

        bool HasDuplicateCapabilities(const AdapterReleaseSignerReview& review)
        {
            for (size_t left = 0; left < review.m_capabilities.size(); ++left)
            {
                for (size_t right = left + 1;
                     right < review.m_capabilities.size();
                     ++right)
                {
                    if (review.m_capabilities[left] == review.m_capabilities[right])
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        const AdapterReleaseSignatureArtifact* FindSignatureArtifact(
            const AdapterReleaseSigningResultEnvelope& envelope,
            const AZStd::string& signatureArtifactId)
        {
            for (const AdapterReleaseSignatureArtifact& artifact :
                 envelope.m_signatureArtifacts)
            {
                if (artifact.m_signatureArtifactId == signatureArtifactId)
                {
                    return &artifact;
                }
            }
            return nullptr;
        }

        const AdapterReleaseSigningFailure* FindFailure(
            const AdapterReleaseSigningResultEnvelope& envelope,
            const AZStd::string& failureId)
        {
            for (const AdapterReleaseSigningFailure& failure : envelope.m_failures)
            {
                if (failure.m_failureId == failureId)
                {
                    return &failure;
                }
            }
            return nullptr;
        }

        const AdapterReleaseSigningDiagnosticReference* FindDiagnostic(
            const AdapterReleaseSigningResultEnvelope& envelope,
            const AZStd::string& diagnosticId)
        {
            for (const AdapterReleaseSigningDiagnosticReference& diagnostic :
                 envelope.m_diagnosticReferences)
            {
                if (diagnostic.m_diagnosticId == diagnosticId)
                {
                    return &diagnostic;
                }
            }
            return nullptr;
        }

        AZStd::string UnsignedSigningString(AZ::u64 value)
        {
            char buffer[32];
            size_t position = sizeof(buffer);
            do
            {
                buffer[--position] = static_cast<char>('0' + (value % 10));
                value /= 10;
            } while (value != 0);
            return AZStd::string(buffer + position, sizeof(buffer) - position);
        }

        void ValidateAssemblyAcceptance(
            const AdapterReleaseArtifactEnvelope& artifact,
            const AdapterReleaseAssemblyResultEnvelope& assembly,
            const AdapterReleaseAssemblyEvidenceReturn& assemblyEvidence,
            AdapterReleaseSigningEvidenceReturn& result,
            SigningValidationFlags& flags)
        {
            const bool artifactReady =
                artifact.m_status == AdapterReleaseArtifactEnvelopeStatus::Ready
                && artifact.m_metadataReady
                && !artifact.m_canonicalJson.empty();
            const bool acceptedEvidence = assemblyEvidence.m_contractValid
                && assemblyEvidence.m_accepted
                && assemblyEvidence.m_status
                    == AdapterReleaseAssemblyEnvelopeStatus::Accepted
                && assemblyEvidence.m_resultId == assembly.m_resultId
                && assemblyEvidence.m_artifactId == assembly.m_artifactId
                && assemblyEvidence.m_artifactFingerprint
                    == assembly.m_artifactFingerprint
                && assemblyEvidence.m_resultFingerprint
                    == assembly.m_resultFingerprint;
            const bool successfulArchive =
                assembly.m_archive.m_outcome
                    == AdapterReleaseAssemblyOutcome::Succeeded
                && assembly.m_archive.m_attempted
                && assembly.m_archive.m_archivePresent
                && assembly.m_archive.m_byteSize > 0
                && IsAdapterReleaseSigningStableId(
                    assembly.m_archive.m_archiveId)
                && IsAdapterReleaseSigningSafeReference(
                    assembly.m_archive.m_archivePath)
                && IsBoundedPublicText(
                    assembly.m_archive.m_archiveFormat,
                    64)
                && IsAdapterReleaseSigningFingerprint(
                    assembly.m_archive.m_archiveFingerprint)
                && IsAdapterReleaseSigningUtcTimestamp(
                    assembly.m_archive.m_completedAtUtc)
                && assembly.m_archive.m_failureIds.empty();
            if (!artifactReady || !acceptedEvidence || !successfulArchive)
            {
                AddSigningIssue(
                    result,
                    flags.m_assemblyResultNotAccepted,
                    "release_signing.assembly_result_not_accepted",
                    "Release signing evidence requires one ready release artifact and "
                    "one exact accepted assembly/checksum result with a successful "
                    "reported archive.");
            }
        }

        void ValidateSigningRequested(
            const AdapterReleaseArtifactEnvelope& artifact,
            AdapterReleaseSigningEvidenceReturn& result,
            SigningValidationFlags& flags)
        {
            if (artifact.m_signingIntent.m_decision
                == AdapterReleaseSigningIntentDecision::Unsigned)
            {
                AddSigningIssue(
                    result,
                    flags.m_signingNotRequested,
                    "release_signing.signing_not_requested",
                    "The reviewed release artifact explicitly requests an unsigned "
                    "release and cannot produce signing-result evidence.");
            }
        }

        void ValidateSignerReview(
            const AdapterReleaseSigningResultEnvelope& envelope,
            AdapterReleaseSigningEvidenceReturn& result,
            SigningValidationFlags& flags)
        {
            const AdapterReleaseSignerReview& review = envelope.m_signerReview;
            const bool valid =
                review.m_decision == AdapterReleaseSignerReviewDecision::Accepted
                && IsAdapterReleaseSigningStableId(review.m_reviewId)
                && IsAdapterReleaseSigningStableId(review.m_signerToolId)
                && IsAdapterReleaseSigningSemanticVersion(
                    review.m_signerToolVersion)
                && IsAdapterReleaseSigningFingerprint(
                    review.m_signerToolFingerprint)
                && IsBoundedPublicText(review.m_reviewer, 256)
                && HasStableUniqueIds(review.m_evidenceIds, true)
                && IsAdapterReleaseSigningUtcTimestamp(review.m_reviewedAtUtc)
                && IsAdapterReleaseSigningUtcTimestamp(envelope.m_capturedAtUtc)
                && review.m_reviewedAtUtc <= envelope.m_capturedAtUtc
                && HasCapability(
                    review,
                    AdapterReleaseSignerCapability::ArchiveSigning)
                && HasCapability(
                    review,
                    AdapterReleaseSignerCapability::SignatureArtifactFingerprint)
                && !HasDuplicateCapabilities(review);
            if (!valid)
            {
                AddSigningIssue(
                    result,
                    flags.m_signerUnreviewed,
                    "release_signing.signer_unreviewed",
                    "The external signer requires an accepted, evidence-backed, "
                    "time-bounded review with archive-signing and signature-artifact "
                    "fingerprint capabilities.");
            }
        }

        void ValidateAssemblyBinding(
            const AdapterReleaseArtifactEnvelope& artifact,
            const AdapterReleaseAssemblyResultEnvelope& assembly,
            const AdapterReleaseAssemblyEvidenceReturn& assemblyEvidence,
            const AdapterReleaseSigningResultEnvelope& signing,
            AdapterReleaseSigningEvidenceReturn& result,
            SigningValidationFlags& flags)
        {
            const AZStd::string expectedArtifactFingerprint =
                CalculateCanonicalSha256(artifact.m_canonicalJson);
            const bool exactBinding = signing.m_artifactId == artifact.m_artifactId
                && signing.m_artifactId == assembly.m_artifactId
                && signing.m_artifactFingerprint == expectedArtifactFingerprint
                && signing.m_artifactFingerprint
                    == assembly.m_artifactFingerprint
                && signing.m_artifactFingerprint
                    == assemblyEvidence.m_artifactFingerprint
                && signing.m_assemblyResultId == assembly.m_resultId
                && signing.m_assemblyResultId == assemblyEvidence.m_resultId
                && signing.m_assemblyResultFingerprint
                    == assembly.m_resultFingerprint
                && signing.m_assemblyResultFingerprint
                    == assemblyEvidence.m_resultFingerprint
                && signing.m_archiveId == assembly.m_archive.m_archiveId
                && signing.m_archivePath == assembly.m_archive.m_archivePath
                && signing.m_archiveFormat == assembly.m_archive.m_archiveFormat
                && signing.m_archiveByteSize == assembly.m_archive.m_byteSize
                && signing.m_archiveFingerprint
                    == assembly.m_archive.m_archiveFingerprint
                && signing.m_reconciliationId == artifact.m_reconciliationId
                && signing.m_reconciliationId == assembly.m_reconciliationId
                && signing.m_packagePreviewId == artifact.m_packagePreviewId
                && signing.m_packagePreviewId == assembly.m_packagePreviewId
                && signing.m_manifestId == artifact.m_manifestId
                && signing.m_manifestId == assembly.m_manifestId
                && signing.m_manifestFingerprint
                    == artifact.m_manifestFingerprint
                && signing.m_manifestFingerprint
                    == assembly.m_manifestFingerprint
                && signing.m_packId == artifact.m_packId
                && signing.m_packId == assembly.m_packId
                && signing.m_packVersion == artifact.m_packVersion
                && signing.m_packVersion == assembly.m_packVersion
                && signing.m_profileId == artifact.m_profileId
                && signing.m_profileId == assembly.m_profileId
                && signing.m_gameVersion == artifact.m_gameVersion
                && signing.m_gameVersion == assembly.m_gameVersion
                && signing.m_branch == artifact.m_branch
                && signing.m_branch == assembly.m_branch
                && signing.m_runtimeTarget == artifact.m_runtimeTarget
                && signing.m_runtimeTarget == assembly.m_runtimeTarget;
            if (!exactBinding)
            {
                AddSigningIssue(
                    result,
                    flags.m_assemblyBindingMismatch,
                    "release_signing.assembly_binding_mismatch",
                    "The signing result must bind to the exact accepted assembly "
                    "result, successful archive, canonical artifact fingerprint, "
                    "and release context.");
            }
        }

        void ValidateSigningIntentBinding(
            const AdapterReleaseArtifactEnvelope& artifact,
            const AdapterReleaseSigningResultEnvelope& signing,
            AdapterReleaseSigningEvidenceReturn& result,
            SigningValidationFlags& flags)
        {
            const AdapterReleaseSigningIntent& intent = artifact.m_signingIntent;
            const bool exactBinding =
                intent.m_decision
                    == AdapterReleaseSigningIntentDecision::SignExternally
                && intent.m_identityKind
                    != AdapterReleaseSigningIdentityKind::None
                && IsAdapterReleaseSigningStableId(intent.m_intentId)
                && IsAdapterReleaseSigningStableId(intent.m_signerId)
                && IsBoundedPublicText(intent.m_identityLocator, 1024)
                && IsAdapterReleaseSigningFingerprint(
                    intent.m_identityFingerprint)
                && signing.m_signingIntentId == intent.m_intentId
                && signing.m_signingIntentDecision == intent.m_decision
                && signing.m_signingIdentityKind == intent.m_identityKind
                && signing.m_signerId == intent.m_signerId
                && signing.m_identityLocator == intent.m_identityLocator
                && signing.m_identityFingerprint
                    == intent.m_identityFingerprint;
            if (!exactBinding)
            {
                AddSigningIssue(
                    result,
                    flags.m_signingIntentBindingMismatch,
                    "release_signing.signing_intent_binding_mismatch",
                    "The signing result must reproduce the exact approved external "
                    "signing intent, signer identity, locator, and fingerprint.");
            }
        }

        void ValidateEnvelopeShape(
            const AdapterReleaseSigningResultEnvelope& envelope,
            AdapterReleaseSigningEvidenceReturn& result,
            SigningValidationFlags& flags)
        {
            const bool valid = envelope.m_contractVersion == 1
                && IsAdapterReleaseSigningStableId(envelope.m_resultId)
                && IsAdapterReleaseSigningFingerprint(
                    envelope.m_resultFingerprint)
                && IsAdapterReleaseSigningStableId(envelope.m_artifactId)
                && IsAdapterReleaseSigningFingerprint(
                    envelope.m_artifactFingerprint)
                && IsAdapterReleaseSigningStableId(
                    envelope.m_assemblyResultId)
                && IsAdapterReleaseSigningFingerprint(
                    envelope.m_assemblyResultFingerprint)
                && IsAdapterReleaseSigningStableId(envelope.m_archiveId)
                && IsAdapterReleaseSigningSafeReference(envelope.m_archivePath)
                && IsBoundedPublicText(envelope.m_archiveFormat, 64)
                && envelope.m_archiveByteSize > 0
                && IsAdapterReleaseSigningFingerprint(
                    envelope.m_archiveFingerprint)
                && IsAdapterReleaseSigningStableId(
                    envelope.m_reconciliationId)
                && IsAdapterReleaseSigningStableId(
                    envelope.m_packagePreviewId)
                && IsAdapterReleaseSigningStableId(envelope.m_manifestId)
                && IsAdapterReleaseSigningFingerprint(
                    envelope.m_manifestFingerprint)
                && IsAdapterReleaseSigningStableId(envelope.m_packId)
                && IsAdapterReleaseSigningSemanticVersion(envelope.m_packVersion)
                && IsAdapterReleaseSigningStableId(envelope.m_profileId)
                && IsBoundedPublicText(envelope.m_gameVersion, 256)
                && IsBoundedPublicText(envelope.m_branch, 256)
                && IsBoundedPublicText(envelope.m_runtimeTarget, 64)
                && IsAdapterReleaseSigningStableId(
                    envelope.m_signingIntentId)
                && IsAdapterReleaseSigningStableId(envelope.m_signerId)
                && IsBoundedPublicText(envelope.m_identityLocator, 1024)
                && IsAdapterReleaseSigningFingerprint(
                    envelope.m_identityFingerprint)
                && IsAdapterReleaseSigningUtcTimestamp(
                    envelope.m_capturedAtUtc)
                && HasStableUniqueIds(envelope.m_failureIds, false)
                && HasStableUniqueIds(
                    envelope.m_diagnosticReferenceIds,
                    false);
            if (!valid)
            {
                AddSigningIssue(
                    result,
                    flags.m_envelopeInvalid,
                    "release_signing.envelope_invalid",
                    "The signing-result envelope requires contract version one, "
                    "stable exact bindings, bounded public metadata, safe relative "
                    "references, lowercase SHA-256 fingerprints, and UTC timestamps.");
            }
        }

        void ValidateSigningOutcome(
            const AdapterReleaseSigningResultEnvelope& envelope,
            AdapterReleaseSigningEvidenceReturn& result,
            SigningValidationFlags& flags)
        {
            bool valid = true;
            switch (envelope.m_outcome)
            {
            case AdapterReleaseSigningOutcome::NotAttempted:
                valid = !envelope.m_attempted
                    && envelope.m_completedAtUtc.empty()
                    && envelope.m_failureIds.empty()
                    && envelope.m_signatureArtifacts.empty();
                break;
            case AdapterReleaseSigningOutcome::Skipped:
                valid = !envelope.m_attempted
                    && envelope.m_completedAtUtc.empty()
                    && envelope.m_signatureArtifacts.empty();
                break;
            case AdapterReleaseSigningOutcome::Succeeded:
                valid = envelope.m_attempted
                    && IsAdapterReleaseSigningUtcTimestamp(
                        envelope.m_completedAtUtc)
                    && !envelope.m_signatureArtifacts.empty()
                    && envelope.m_failureIds.empty();
                break;
            case AdapterReleaseSigningOutcome::Failed:
                valid = envelope.m_attempted
                    && IsAdapterReleaseSigningUtcTimestamp(
                        envelope.m_completedAtUtc)
                    && !envelope.m_failureIds.empty();
                break;
            }
            if (!envelope.m_completedAtUtc.empty()
                && (!IsAdapterReleaseSigningUtcTimestamp(
                        envelope.m_completedAtUtc)
                    || !IsAdapterReleaseSigningUtcTimestamp(
                        envelope.m_capturedAtUtc)
                    || envelope.m_completedAtUtc > envelope.m_capturedAtUtc))
            {
                valid = false;
            }
            if (!valid)
            {
                AddSigningIssue(
                    result,
                    flags.m_signingOutcomeMismatch,
                    "release_signing.signing_outcome_mismatch",
                    "Signing attempted state, outcome, completion time, signature "
                    "artifacts, and failure references are not self-consistent.");
            }
        }

        void ValidateSignatureArtifacts(
            const AdapterReleaseSigningResultEnvelope& envelope,
            AdapterReleaseSigningEvidenceReturn& result,
            SigningValidationFlags& flags)
        {
            AZStd::vector<AZStd::string> artifactIds;
            AZStd::vector<AZStd::string> referenceIdentities;
            for (const AdapterReleaseSignatureArtifact& artifact :
                 envelope.m_signatureArtifacts)
            {
                artifactIds.push_back(artifact.m_signatureArtifactId);
                PackagePathIdentity referenceIdentity;
                const bool safeReference = TryBuildPackagePathIdentity(
                    artifact.m_reference,
                    referenceIdentity);
                if (safeReference)
                {
                    referenceIdentities.push_back(
                        referenceIdentity.m_windowsIdentity);
                }
                bool valid =
                    IsAdapterReleaseSigningStableId(
                        artifact.m_signatureArtifactId)
                    && safeReference
                    && IsBoundedPublicText(artifact.m_mediaType, 256)
                    && artifact.m_byteSize > 0
                    && IsAdapterReleaseSigningFingerprint(
                        artifact.m_fingerprint)
                    && IsAdapterReleaseSigningUtcTimestamp(
                        artifact.m_createdAtUtc)
                    && artifact.m_archiveId == envelope.m_archiveId
                    && artifact.m_archiveFingerprint
                        == envelope.m_archiveFingerprint
                    && HasStableUniqueIds(
                        artifact.m_diagnosticReferenceIds,
                        false)
                    && artifact.m_createdAtUtc <= envelope.m_capturedAtUtc;
                if (!envelope.m_completedAtUtc.empty())
                {
                    valid = valid
                        && artifact.m_createdAtUtc
                            <= envelope.m_completedAtUtc;
                }
                if (!valid)
                {
                    AddSigningIssue(
                        result,
                        flags.m_signatureArtifactBindingMismatch,
                        "release_signing.signature_artifact_invalid",
                        "Each signature artifact requires stable identity, a unique "
                        "safe relative reference, non-zero size, exact archive binding, "
                        "a lowercase SHA-256 fingerprint, and a bounded UTC timestamp.",
                        artifact.m_signatureArtifactId);
                }
            }
            if (ContainsDuplicate(artifactIds)
                || ContainsDuplicate(referenceIdentities))
            {
                AddSigningIssue(
                    result,
                    flags.m_signatureArtifactBindingMismatch,
                    "release_signing.duplicate_signature_artifact",
                    "Signature artifact identities and case-insensitive path identities "
                    "must be unique.");
            }
        }

        void ValidateFailureDiagnosticBindings(
            const AdapterReleaseSigningResultEnvelope& envelope,
            AdapterReleaseSigningEvidenceReturn& result,
            SigningValidationFlags& flags)
        {
            AZStd::vector<AZStd::string> failureIds;
            AZStd::vector<AZStd::string> diagnosticIds;
            AZStd::vector<AZStd::string> diagnosticReferenceIdentities;

            for (const AdapterReleaseSigningFailure& failure : envelope.m_failures)
            {
                failureIds.push_back(failure.m_failureId);
                const bool artifactBindingValid =
                    failure.m_signatureArtifactId.empty()
                    || FindSignatureArtifact(
                        envelope,
                        failure.m_signatureArtifactId);
                const bool valid =
                    IsAdapterReleaseSigningStableId(failure.m_failureId)
                    && failure.m_kind
                        != AdapterReleaseSigningFailureKind::Unknown
                    && IsBoundedPublicText(failure.m_code, 128)
                    && IsBoundedPublicText(failure.m_message, 2048)
                    && artifactBindingValid
                    && HasStableUniqueIds(
                        failure.m_diagnosticReferenceIds,
                        false);
                if (!valid)
                {
                    AddSigningIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "release_signing.failure_invalid",
                        "Signing failures require stable identity, a known kind, bounded "
                        "public code and message, an optional existing artifact binding, "
                        "and unique diagnostic references.",
                        failure.m_signatureArtifactId,
                        failure.m_failureId);
                }
            }

            for (const AdapterReleaseSigningDiagnosticReference& diagnostic :
                 envelope.m_diagnosticReferences)
            {
                diagnosticIds.push_back(diagnostic.m_diagnosticId);
                PackagePathIdentity referenceIdentity;
                const bool safeReference = TryBuildPackagePathIdentity(
                    diagnostic.m_reference,
                    referenceIdentity);
                if (safeReference)
                {
                    diagnosticReferenceIdentities.push_back(
                        referenceIdentity.m_windowsIdentity);
                }
                bool valid =
                    IsAdapterReleaseSigningStableId(diagnostic.m_diagnosticId)
                    && safeReference
                    && IsAdapterReleaseSigningFingerprint(
                        diagnostic.m_fingerprint)
                    && HasStableUniqueIds(
                        diagnostic.m_signatureArtifactIds,
                        false);
                for (const AZStd::string& signatureArtifactId :
                     diagnostic.m_signatureArtifactIds)
                {
                    valid = valid
                        && FindSignatureArtifact(
                            envelope,
                            signatureArtifactId);
                }
                if (!valid)
                {
                    AddSigningIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "release_signing.diagnostic_invalid",
                        "Signing diagnostics require stable identity, a unique safe "
                        "relative reference, a lowercase SHA-256 fingerprint, and only "
                        "existing signature-artifact bindings.",
                        {},
                        {},
                        diagnostic.m_diagnosticId);
                }
            }

            if (ContainsDuplicate(failureIds)
                || ContainsDuplicate(diagnosticIds)
                || ContainsDuplicate(diagnosticReferenceIdentities))
            {
                AddSigningIssue(
                    result,
                    flags.m_failureDiagnosticBindingMismatch,
                    "release_signing.duplicate_failure_or_diagnostic",
                    "Failure identities, diagnostic identities, and case-insensitive "
                    "diagnostic path identities must be unique.");
            }

            if (!HaveSameValues(failureIds, envelope.m_failureIds))
            {
                AddSigningIssue(
                    result,
                    flags.m_failureDiagnosticBindingMismatch,
                    "release_signing.failure_coverage_mismatch",
                    "The signing result failure-identity list must cover every supplied "
                    "failure exactly once and contain no unknown identity.");
            }
            if (!HaveSameValues(
                    diagnosticIds,
                    envelope.m_diagnosticReferenceIds))
            {
                AddSigningIssue(
                    result,
                    flags.m_failureDiagnosticBindingMismatch,
                    "release_signing.diagnostic_coverage_mismatch",
                    "The signing result diagnostic-identity list must cover every "
                    "supplied diagnostic exactly once and contain no unknown identity.");
            }

            for (const AdapterReleaseSignatureArtifact& artifact :
                 envelope.m_signatureArtifacts)
            {
                for (const AZStd::string& diagnosticId :
                     artifact.m_diagnosticReferenceIds)
                {
                    const AdapterReleaseSigningDiagnosticReference* diagnostic =
                        FindDiagnostic(envelope, diagnosticId);
                    const bool reciprocal = diagnostic
                        && AZStd::find(
                               diagnostic->m_signatureArtifactIds.begin(),
                               diagnostic->m_signatureArtifactIds.end(),
                               artifact.m_signatureArtifactId)
                            != diagnostic->m_signatureArtifactIds.end();
                    if (!reciprocal)
                    {
                        AddSigningIssue(
                            result,
                            flags.m_failureDiagnosticBindingMismatch,
                            "release_signing.artifact_diagnostic_mismatch",
                            "A signature artifact references an absent or non-reciprocal "
                            "diagnostic.",
                            artifact.m_signatureArtifactId,
                            {},
                            diagnosticId);
                    }
                }
            }

            for (const AdapterReleaseSigningDiagnosticReference& diagnostic :
                 envelope.m_diagnosticReferences)
            {
                for (const AZStd::string& signatureArtifactId :
                     diagnostic.m_signatureArtifactIds)
                {
                    const AdapterReleaseSignatureArtifact* artifact =
                        FindSignatureArtifact(envelope, signatureArtifactId);
                    const bool reciprocal = artifact
                        && AZStd::find(
                               artifact->m_diagnosticReferenceIds.begin(),
                               artifact->m_diagnosticReferenceIds.end(),
                               diagnostic.m_diagnosticId)
                            != artifact->m_diagnosticReferenceIds.end();
                    if (!reciprocal)
                    {
                        AddSigningIssue(
                            result,
                            flags.m_failureDiagnosticBindingMismatch,
                            "release_signing.diagnostic_artifact_mismatch",
                            "A diagnostic references an absent or non-reciprocal "
                            "signature artifact.",
                            signatureArtifactId,
                            {},
                            diagnostic.m_diagnosticId);
                    }
                }
            }

            for (const AdapterReleaseSigningFailure& failure : envelope.m_failures)
            {
                for (const AZStd::string& diagnosticId :
                     failure.m_diagnosticReferenceIds)
                {
                    const AdapterReleaseSigningDiagnosticReference* diagnostic =
                        FindDiagnostic(envelope, diagnosticId);
                    const bool compatible = diagnostic
                        && (failure.m_signatureArtifactId.empty()
                            || diagnostic->m_signatureArtifactIds.empty()
                            || AZStd::find(
                                   diagnostic->m_signatureArtifactIds.begin(),
                                   diagnostic->m_signatureArtifactIds.end(),
                                   failure.m_signatureArtifactId)
                                != diagnostic->m_signatureArtifactIds.end());
                    if (!compatible)
                    {
                        AddSigningIssue(
                            result,
                            flags.m_failureDiagnosticBindingMismatch,
                            "release_signing.failure_diagnostic_mismatch",
                            "A signing failure references an absent or differently "
                            "bound diagnostic.",
                            failure.m_signatureArtifactId,
                            failure.m_failureId,
                            diagnosticId);
                    }
                }
            }

            for (const AZStd::string& failureId : envelope.m_failureIds)
            {
                if (!FindFailure(envelope, failureId))
                {
                    AddSigningIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "release_signing.failure_reference_missing",
                        "The signing outcome references a failure that is not supplied.",
                        {},
                        failureId);
                }
            }
            for (const AZStd::string& diagnosticId :
                 envelope.m_diagnosticReferenceIds)
            {
                if (!FindDiagnostic(envelope, diagnosticId))
                {
                    AddSigningIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "release_signing.diagnostic_reference_missing",
                        "The signing outcome references a diagnostic that is not "
                        "supplied.",
                        {},
                        {},
                        diagnosticId);
                }
            }
        }

        AdapterReleaseSigningEnvelopeStatus ResolveStatus(
            const SigningValidationFlags& flags)
        {
            if (flags.m_assemblyResultNotAccepted)
            {
                return AdapterReleaseSigningEnvelopeStatus::AssemblyResultNotAccepted;
            }
            if (flags.m_signingNotRequested)
            {
                return AdapterReleaseSigningEnvelopeStatus::SigningNotRequested;
            }
            if (flags.m_signerUnreviewed)
            {
                return AdapterReleaseSigningEnvelopeStatus::SignerUnreviewed;
            }
            if (flags.m_assemblyBindingMismatch)
            {
                return AdapterReleaseSigningEnvelopeStatus::AssemblyBindingMismatch;
            }
            if (flags.m_signingIntentBindingMismatch)
            {
                return AdapterReleaseSigningEnvelopeStatus::SigningIntentBindingMismatch;
            }
            if (flags.m_envelopeInvalid)
            {
                return AdapterReleaseSigningEnvelopeStatus::EnvelopeInvalid;
            }
            if (flags.m_signingOutcomeMismatch)
            {
                return AdapterReleaseSigningEnvelopeStatus::SigningOutcomeMismatch;
            }
            if (flags.m_signatureArtifactBindingMismatch)
            {
                return AdapterReleaseSigningEnvelopeStatus::SignatureArtifactBindingMismatch;
            }
            if (flags.m_failureDiagnosticBindingMismatch)
            {
                return AdapterReleaseSigningEnvelopeStatus::FailureDiagnosticBindingMismatch;
            }
            return AdapterReleaseSigningEnvelopeStatus::Accepted;
        }

        bool ContractIsValid(const SigningValidationFlags& flags)
        {
            return !flags.m_assemblyResultNotAccepted
                && !flags.m_signingNotRequested
                && !flags.m_signerUnreviewed
                && !flags.m_assemblyBindingMismatch
                && !flags.m_signingIntentBindingMismatch
                && !flags.m_envelopeInvalid
                && !flags.m_signingOutcomeMismatch
                && !flags.m_signatureArtifactBindingMismatch
                && !flags.m_failureDiagnosticBindingMismatch;
        }

        SourceDocument BuildSourceDocument(
            const AZStd::string& sourceId,
            const AZStd::string& title,
            const AZStd::string& sourceKind,
            const AZStd::string& locator,
            const AZStd::string& fingerprint,
            const AdapterReleaseSigningResultEnvelope& envelope,
            const AZStd::string& mediaType,
            const AZStd::string& limitations)
        {
            SourceDocument document;
            document.m_source.m_sourceId = sourceId;
            document.m_source.m_title = title;
            document.m_source.m_sourceKind = sourceKind;
            document.m_source.m_locator = locator;
            document.m_source.m_fingerprint = fingerprint;
            document.m_source.m_profileId = envelope.m_profileId;
            document.m_source.m_gameVersion = envelope.m_gameVersion;
            document.m_source.m_branch = envelope.m_branch;
            document.m_source.m_runtimeTarget = envelope.m_runtimeTarget;
            document.m_source.m_toolName =
                envelope.m_signerReview.m_signerToolId;
            document.m_source.m_toolVersion =
                envelope.m_signerReview.m_signerToolVersion;
            document.m_source.m_importerId = "tg.release-signing-result";
            document.m_source.m_importerVersion = "1";
            document.m_source.m_capturedAt = envelope.m_capturedAtUtc;
            document.m_source.m_importedAt = envelope.m_capturedAtUtc;
            document.m_source.m_limitations = limitations;
            document.m_source.m_mediaType = mediaType;
            document.m_source.m_importStatus = "contract_validated";
            return document;
        }

        EvidenceRecord BuildEvidence(
            const AZStd::string& evidenceId,
            const SourceRecord& source,
            const AdapterReleaseSigningResultEnvelope& envelope,
            const AZStd::string& subjectRef,
            const AZStd::string& claim,
            const AZStd::string& evidenceKind,
            const AZStd::string& locator,
            const AZStd::string& recordPath)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = evidenceId;
            evidence.m_sourceId = source.m_sourceId;
            evidence.m_sourceFingerprint = source.m_fingerprint;
            evidence.m_profileId = envelope.m_profileId;
            evidence.m_gameVersion = envelope.m_gameVersion;
            evidence.m_branch = envelope.m_branch;
            evidence.m_subjectRef = subjectRef;
            evidence.m_claim = claim;
            evidence.m_evidenceKind = evidenceKind;
            evidence.m_confidence = "unrated";
            evidence.m_locator = locator;
            evidence.m_recordPath = recordPath;
            evidence.m_extractedAt = envelope.m_capturedAtUtc;
            return evidence;
        }

        void BuildPrimaryEvidence(
            const AdapterReleaseSigningResultEnvelope& envelope,
            AdapterReleaseSigningEvidenceReturn& result)
        {
            const AZStd::string sourceId =
                "source.release-signing." + envelope.m_resultId;
            const AZStd::string locator =
                "release-signing-results/" + envelope.m_resultId + ".json";
            SourceDocument sourceDocument = BuildSourceDocument(
                sourceId,
                "Release signing result " + envelope.m_resultId,
                "release_signing_result",
                locator,
                envelope.m_resultFingerprint,
                envelope,
                "application/vnd.taintedgrail.release-signing-result+json",
                "Contract-validated separately supplied signer metadata only. The TG "
                "SDK did not open or modify the archive, load a key, resolve an "
                "identity, sign or verify data, write a signature artifact, upload, "
                "publish, launch FoA, call an adapter, mutate deployment, or modify a "
                "save.");

            EvidenceDocument evidenceDocument;
            evidenceDocument.m_sourceId = sourceId;
            evidenceDocument.m_sourceFingerprint = envelope.m_resultFingerprint;
            evidenceDocument.m_profileId = envelope.m_profileId;
            evidenceDocument.m_gameVersion = envelope.m_gameVersion;
            evidenceDocument.m_branch = envelope.m_branch;

            evidenceDocument.m_evidence.push_back(BuildEvidence(
                "evidence.release-signing." + envelope.m_resultId
                    + ".artifact-binding",
                sourceDocument.m_source,
                envelope,
                "release-artifact:" + envelope.m_artifactId,
                "External signing result binds to exact ready release artifact "
                    + envelope.m_artifactId + " with fingerprint "
                    + envelope.m_artifactFingerprint + ".",
                "release_signing_artifact_binding",
                locator,
                "ArtifactBinding"));
            evidenceDocument.m_evidence.push_back(BuildEvidence(
                "evidence.release-signing." + envelope.m_resultId
                    + ".assembly-binding",
                sourceDocument.m_source,
                envelope,
                "release-assembly-result:" + envelope.m_assemblyResultId,
                "External signing result binds to accepted assembly result "
                    + envelope.m_assemblyResultId + " and archive "
                    + envelope.m_archiveId + " with fingerprint "
                    + envelope.m_archiveFingerprint + ".",
                "release_signing_assembly_binding",
                locator,
                "AssemblyBinding"));
            evidenceDocument.m_evidence.push_back(BuildEvidence(
                "evidence.release-signing." + envelope.m_resultId
                    + ".signer-review",
                sourceDocument.m_source,
                envelope,
                "release-signing-result:" + envelope.m_resultId,
                "Signer tool " + envelope.m_signerReview.m_signerToolId
                    + " version " + envelope.m_signerReview.m_signerToolVersion
                    + " was supplied with accepted review "
                    + envelope.m_signerReview.m_reviewId + ".",
                "release_signer_review",
                locator,
                "SignerReview"));
            evidenceDocument.m_evidence.push_back(BuildEvidence(
                "evidence.release-signing." + envelope.m_resultId
                    + ".signing-intent",
                sourceDocument.m_source,
                envelope,
                "release-artifact:" + envelope.m_artifactId
                    + ":signing-intent:" + envelope.m_signingIntentId,
                "External signing result reproduces approved signing intent "
                    + envelope.m_signingIntentId + " for signer "
                    + envelope.m_signerId + ".",
                "release_signing_intent_binding",
                locator,
                "SigningIntent"));
            evidenceDocument.m_evidence.push_back(BuildEvidence(
                "evidence.release-signing." + envelope.m_resultId + ".outcome",
                sourceDocument.m_source,
                envelope,
                "release-signing-result:" + envelope.m_resultId,
                "External signer reported outcome " + ToString(envelope.m_outcome)
                    + " for archive " + envelope.m_archiveId + ".",
                "release_signing_outcome",
                locator,
                "Outcome"));

            AZStd::vector<const AdapterReleaseSignatureArtifact*> artifacts;
            for (const AdapterReleaseSignatureArtifact& artifact :
                 envelope.m_signatureArtifacts)
            {
                artifacts.push_back(&artifact);
            }
            AZStd::sort(
                artifacts.begin(),
                artifacts.end(),
                [](const AdapterReleaseSignatureArtifact* left,
                   const AdapterReleaseSignatureArtifact* right)
                {
                    return left->m_signatureArtifactId
                        < right->m_signatureArtifactId;
                });
            for (const AdapterReleaseSignatureArtifact* artifact : artifacts)
            {
                evidenceDocument.m_evidence.push_back(BuildEvidence(
                    "evidence.release-signing." + envelope.m_resultId
                        + ".signature-artifact."
                        + artifact->m_signatureArtifactId,
                    sourceDocument.m_source,
                    envelope,
                    "release-signature-artifact:"
                        + artifact->m_signatureArtifactId,
                    "External signer reported " + ToString(artifact->m_kind)
                        + " at " + artifact->m_reference + " with fingerprint "
                        + artifact->m_fingerprint + ".",
                    "release_signature_artifact",
                    locator,
                    "SignatureArtifacts/"
                        + artifact->m_signatureArtifactId));
            }

            AZStd::vector<const AdapterReleaseSigningFailure*> failures;
            for (const AdapterReleaseSigningFailure& failure : envelope.m_failures)
            {
                failures.push_back(&failure);
            }
            AZStd::sort(
                failures.begin(),
                failures.end(),
                [](const AdapterReleaseSigningFailure* left,
                   const AdapterReleaseSigningFailure* right)
                {
                    return left->m_failureId < right->m_failureId;
                });
            for (const AdapterReleaseSigningFailure* failure : failures)
            {
                evidenceDocument.m_evidence.push_back(BuildEvidence(
                    "evidence.release-signing." + envelope.m_resultId + ".failure."
                        + failure->m_failureId,
                    sourceDocument.m_source,
                    envelope,
                    failure->m_signatureArtifactId.empty()
                        ? "release-signing-result:" + envelope.m_resultId
                        : "release-signature-artifact:"
                            + failure->m_signatureArtifactId,
                    "External signing failure " + failure->m_failureId
                        + " reported kind " + ToString(failure->m_kind)
                        + ", code " + failure->m_code + ": "
                        + failure->m_message,
                    "release_signing_failure",
                    locator,
                    "Failures/" + failure->m_failureId));
            }

            result.m_sourceDocuments.push_back(AZStd::move(sourceDocument));
            result.m_evidenceDocuments.push_back(AZStd::move(evidenceDocument));
        }

        void BuildDiagnosticEvidence(
            const AdapterReleaseSigningResultEnvelope& envelope,
            AdapterReleaseSigningEvidenceReturn& result)
        {
            AZStd::vector<const AdapterReleaseSigningDiagnosticReference*>
                diagnostics;
            for (const AdapterReleaseSigningDiagnosticReference& diagnostic :
                 envelope.m_diagnosticReferences)
            {
                diagnostics.push_back(&diagnostic);
            }
            AZStd::sort(
                diagnostics.begin(),
                diagnostics.end(),
                [](const AdapterReleaseSigningDiagnosticReference* left,
                   const AdapterReleaseSigningDiagnosticReference* right)
                {
                    return left->m_diagnosticId < right->m_diagnosticId;
                });

            for (const AdapterReleaseSigningDiagnosticReference* diagnostic :
                 diagnostics)
            {
                const AZStd::string sourceId =
                    "source.release-signing-diagnostic." + envelope.m_resultId
                    + "." + diagnostic->m_diagnosticId;
                SourceDocument sourceDocument = BuildSourceDocument(
                    sourceId,
                    "Release signing diagnostic " + diagnostic->m_diagnosticId,
                    "release_signing_diagnostic",
                    diagnostic->m_reference,
                    diagnostic->m_fingerprint,
                    envelope,
                    "text/plain",
                    "Referenced signer diagnostic content is not opened, persisted, "
                    "parsed, or inspected by the TG SDK.");

                EvidenceDocument evidenceDocument;
                evidenceDocument.m_sourceId = sourceId;
                evidenceDocument.m_sourceFingerprint = diagnostic->m_fingerprint;
                evidenceDocument.m_profileId = envelope.m_profileId;
                evidenceDocument.m_gameVersion = envelope.m_gameVersion;
                evidenceDocument.m_branch = envelope.m_branch;

                AZStd::vector<AZStd::string> artifactIds =
                    diagnostic->m_signatureArtifactIds;
                AZStd::sort(artifactIds.begin(), artifactIds.end());
                if (artifactIds.empty())
                {
                    evidenceDocument.m_evidence.push_back(BuildEvidence(
                        "evidence.release-signing-diagnostic." + envelope.m_resultId
                            + "." + diagnostic->m_diagnosticId + ".result",
                        sourceDocument.m_source,
                        envelope,
                        "release-signing-result:" + envelope.m_resultId,
                        "External " + ToString(diagnostic->m_kind)
                            + " diagnostic " + diagnostic->m_diagnosticId
                            + " is referenced with fingerprint "
                            + diagnostic->m_fingerprint + ".",
                        "release_signing_diagnostic_reference",
                        diagnostic->m_reference,
                        "Diagnostics/" + diagnostic->m_diagnosticId));
                }
                for (size_t index = 0; index < artifactIds.size(); ++index)
                {
                    evidenceDocument.m_evidence.push_back(BuildEvidence(
                        "evidence.release-signing-diagnostic." + envelope.m_resultId
                            + "." + diagnostic->m_diagnosticId + "."
                            + UnsignedSigningString(index + 1),
                        sourceDocument.m_source,
                        envelope,
                        "release-signature-artifact:" + artifactIds[index],
                        "External " + ToString(diagnostic->m_kind)
                            + " diagnostic " + diagnostic->m_diagnosticId
                            + " is referenced with fingerprint "
                            + diagnostic->m_fingerprint + ".",
                        "release_signing_diagnostic_reference",
                        diagnostic->m_reference,
                        "Diagnostics/" + diagnostic->m_diagnosticId + "/"
                            + UnsignedSigningString(index + 1)));
                }
                result.m_sourceDocuments.push_back(AZStd::move(sourceDocument));
                result.m_evidenceDocuments.push_back(AZStd::move(evidenceDocument));
            }
        }

        void FinalizeCounts(
            const AdapterReleaseSigningResultEnvelope& envelope,
            AdapterReleaseSigningEvidenceReturn& result)
        {
            result.m_signatureArtifactCount = static_cast<AZ::u64>(
                envelope.m_signatureArtifacts.size());
            result.m_failureCount =
                static_cast<AZ::u64>(envelope.m_failures.size());
            result.m_diagnosticReferenceCount = static_cast<AZ::u64>(
                envelope.m_diagnosticReferences.size());
            result.m_sourceDocumentCount =
                static_cast<AZ::u64>(result.m_sourceDocuments.size());
            for (const EvidenceDocument& document : result.m_evidenceDocuments)
            {
                result.m_evidenceRecordCount +=
                    static_cast<AZ::u64>(document.m_evidence.size());
            }
        }

        void SortIssues(AdapterReleaseSigningEvidenceReturn& result)
        {
            AZStd::sort(
                result.m_issues.begin(),
                result.m_issues.end(),
                [](const AdapterReleaseSigningIssue& left,
                   const AdapterReleaseSigningIssue& right)
                {
                    if (left.m_code != right.m_code)
                    {
                        return left.m_code < right.m_code;
                    }
                    if (left.m_signatureArtifactId
                        != right.m_signatureArtifactId)
                    {
                        return left.m_signatureArtifactId
                            < right.m_signatureArtifactId;
                    }
                    if (left.m_failureId != right.m_failureId)
                    {
                        return left.m_failureId < right.m_failureId;
                    }
                    if (left.m_diagnosticId != right.m_diagnosticId)
                    {
                        return left.m_diagnosticId < right.m_diagnosticId;
                    }
                    return left.m_message < right.m_message;
                });
        }
    } // namespace

    AdapterReleaseSigningEvidenceReturn
    AdapterReleaseSigningEvidenceService::BuildEvidenceReturn(
        const AdapterReleaseArtifactEnvelope& artifactEnvelope,
        const AdapterReleaseAssemblyResultEnvelope& assemblyEnvelope,
        const AdapterReleaseAssemblyEvidenceReturn& assemblyEvidence,
        const AdapterReleaseSigningResultEnvelope& signingEnvelope) const
    {
        AdapterReleaseSigningEvidenceReturn result;
        result.m_resultId = signingEnvelope.m_resultId;
        result.m_artifactId = signingEnvelope.m_artifactId;
        result.m_artifactFingerprint = signingEnvelope.m_artifactFingerprint;
        result.m_assemblyResultId = signingEnvelope.m_assemblyResultId;
        result.m_assemblyResultFingerprint =
            signingEnvelope.m_assemblyResultFingerprint;
        result.m_archiveId = signingEnvelope.m_archiveId;
        result.m_archiveFingerprint = signingEnvelope.m_archiveFingerprint;

        SigningValidationFlags flags;
        ValidateAssemblyAcceptance(
            artifactEnvelope,
            assemblyEnvelope,
            assemblyEvidence,
            result,
            flags);
        ValidateSigningRequested(artifactEnvelope, result, flags);
        ValidateSignerReview(signingEnvelope, result, flags);
        ValidateAssemblyBinding(
            artifactEnvelope,
            assemblyEnvelope,
            assemblyEvidence,
            signingEnvelope,
            result,
            flags);
        ValidateSigningIntentBinding(
            artifactEnvelope,
            signingEnvelope,
            result,
            flags);
        ValidateEnvelopeShape(signingEnvelope, result, flags);
        ValidateSigningOutcome(signingEnvelope, result, flags);
        ValidateSignatureArtifacts(signingEnvelope, result, flags);
        ValidateFailureDiagnosticBindings(signingEnvelope, result, flags);

        result.m_status = ResolveStatus(flags);
        result.m_contractValid = ContractIsValid(flags);
        result.m_accepted =
            result.m_status == AdapterReleaseSigningEnvelopeStatus::Accepted;
        SortIssues(result);
        if (result.m_contractValid)
        {
            BuildPrimaryEvidence(signingEnvelope, result);
            BuildDiagnosticEvidence(signingEnvelope, result);
        }
        FinalizeCounts(signingEnvelope, result);
        return result;
    }
} // namespace TaintedGrailModdingSDK
