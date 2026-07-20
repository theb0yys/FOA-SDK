/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseAssemblyEvidenceService.h"

#include "CanonicalFingerprint.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        struct AssemblyValidationFlags
        {
            bool m_artifactNotReady = false;
            bool m_assemblerUnreviewed = false;
            bool m_artifactBindingMismatch = false;
            bool m_envelopeInvalid = false;
            bool m_contentCoverageIncomplete = false;
            bool m_checksumObservationMismatch = false;
            bool m_archiveBindingMismatch = false;
            bool m_failureDiagnosticBindingMismatch = false;
        };

        void AddAssemblyIssue(
            AdapterReleaseAssemblyEvidenceReturn& result,
            bool& flag,
            AZStd::string code,
            AZStd::string message,
            AZStd::string contentId = {},
            AZStd::string observationId = {},
            AZStd::string failureId = {},
            AZStd::string diagnosticId = {})
        {
            flag = true;
            AdapterReleaseAssemblyIssue issue;
            issue.m_code = AZStd::move(code);
            issue.m_message = AZStd::move(message);
            issue.m_contentId = AZStd::move(contentId);
            issue.m_observationId = AZStd::move(observationId);
            issue.m_failureId = AZStd::move(failureId);
            issue.m_diagnosticId = AZStd::move(diagnosticId);
            result.m_issues.push_back(AZStd::move(issue));
        }

        AZStd::string UnsignedAssemblyString(AZ::u64 value)
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

        bool HasCapability(
            const AdapterReleaseAssemblerReview& review,
            AdapterReleaseAssemblerCapability capability)
        {
            return AZStd::find(
                       review.m_capabilities.begin(),
                       review.m_capabilities.end(),
                       capability)
                != review.m_capabilities.end();
        }

        const AdapterReleaseArtifactContent* FindContent(
            const AdapterReleaseArtifactEnvelope& artifact,
            const AZStd::string& contentId)
        {
            for (const AdapterReleaseArtifactContent& content : artifact.m_contents)
            {
                if (content.m_contentId == contentId)
                {
                    return &content;
                }
            }
            return nullptr;
        }

        const AdapterReleaseAssemblyFailure* FindFailure(
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            const AZStd::string& failureId)
        {
            for (const AdapterReleaseAssemblyFailure& failure : envelope.m_failures)
            {
                if (failure.m_failureId == failureId)
                {
                    return &failure;
                }
            }
            return nullptr;
        }

        const AdapterReleaseAssemblyDiagnosticReference* FindDiagnostic(
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            const AZStd::string& diagnosticId)
        {
            for (const AdapterReleaseAssemblyDiagnosticReference& diagnostic :
                 envelope.m_diagnosticReferences)
            {
                if (diagnostic.m_diagnosticId == diagnosticId)
                {
                    return &diagnostic;
                }
            }
            return nullptr;
        }

        bool ContainsDuplicate(const AZStd::vector<AZStd::string>& values)
        {
            AZStd::vector<AZStd::string> sorted = values;
            AZStd::sort(sorted.begin(), sorted.end());
            return AZStd::adjacent_find(sorted.begin(), sorted.end())
                != sorted.end();
        }

        void ValidateArtifactReadiness(
            const AdapterReleaseArtifactEnvelope& artifact,
            AdapterReleaseAssemblyEvidenceReturn& result,
            AssemblyValidationFlags& flags)
        {
            if (artifact.m_status != AdapterReleaseArtifactEnvelopeStatus::Ready
                || !artifact.m_metadataReady
                || artifact.m_canonicalJson.empty())
            {
                AddAssemblyIssue(
                    result,
                    flags.m_artifactNotReady,
                    "release_assembly.artifact_not_ready",
                    "Release assembly evidence requires one ready release-artifact "
                    "envelope with canonical metadata.");
            }
        }

        void ValidateAssemblerReview(
            const AdapterReleaseAssemblerReview& review,
            AdapterReleaseAssemblyEvidenceReturn& result,
            AssemblyValidationFlags& flags)
        {
            if (review.m_decision
                    != AdapterReleaseAssemblerReviewDecision::Accepted
                || !IsAdapterReleaseAssemblyStableId(review.m_reviewId)
                || !IsAdapterReleaseAssemblyStableId(review.m_assemblerId)
                || review.m_assemblerVersion.empty()
                || !IsAdapterReleaseAssemblyFingerprint(
                    review.m_assemblerFingerprint)
                || review.m_reviewer.empty()
                || review.m_evidenceIds.empty()
                || !IsAdapterReleaseAssemblyUtcTimestamp(review.m_reviewedAtUtc)
                || !HasCapability(
                    review,
                    AdapterReleaseAssemblerCapability::ContentChecksum)
                || !HasCapability(
                    review,
                    AdapterReleaseAssemblerCapability::ArchiveAssembly)
                || !HasCapability(
                    review,
                    AdapterReleaseAssemblerCapability::ArchiveChecksum))
            {
                AddAssemblyIssue(
                    result,
                    flags.m_assemblerUnreviewed,
                    "release_assembly.assembler_unreviewed",
                    "The external assembler/checksummer must have accepted review "
                    "metadata and content-checksum, archive-assembly, and "
                    "archive-checksum capabilities.");
            }
            if (ContainsDuplicate(review.m_evidenceIds))
            {
                AddAssemblyIssue(
                    result,
                    flags.m_assemblerUnreviewed,
                    "release_assembly.duplicate_review_evidence",
                    "Assembler review evidence identities must be unique.");
            }
        }

        void ValidateArtifactBinding(
            const AdapterReleaseArtifactEnvelope& artifact,
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            AdapterReleaseAssemblyEvidenceReturn& result,
            AssemblyValidationFlags& flags)
        {
            const AZStd::string expectedFingerprint =
                CalculateCanonicalSha256(artifact.m_canonicalJson);
            const bool exactBinding =
                envelope.m_artifactId == artifact.m_artifactId
                && envelope.m_artifactCanonicalJson == artifact.m_canonicalJson
                && envelope.m_artifactFingerprint == expectedFingerprint
                && envelope.m_reconciliationId == artifact.m_reconciliationId
                && envelope.m_packagePreviewId == artifact.m_packagePreviewId
                && envelope.m_manifestId == artifact.m_manifestId
                && envelope.m_manifestFingerprint == artifact.m_manifestFingerprint
                && envelope.m_packId == artifact.m_packId
                && envelope.m_packVersion == artifact.m_packVersion
                && envelope.m_profileId == artifact.m_profileId
                && envelope.m_gameVersion == artifact.m_gameVersion
                && envelope.m_branch == artifact.m_branch
                && envelope.m_runtimeTarget == artifact.m_runtimeTarget;
            if (!exactBinding)
            {
                AddAssemblyIssue(
                    result,
                    flags.m_artifactBindingMismatch,
                    "release_assembly.artifact_binding_mismatch",
                    "The assembler/checksummer result must bind to the exact ready "
                    "release-artifact canonical bytes, fingerprint, identity, and "
                    "release context.");
            }
        }

        void ValidateEnvelopeShape(
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            AdapterReleaseAssemblyEvidenceReturn& result,
            AssemblyValidationFlags& flags)
        {
            if (envelope.m_contractVersion != 1
                || !IsAdapterReleaseAssemblyStableId(envelope.m_resultId)
                || !IsAdapterReleaseAssemblyStableId(envelope.m_artifactId)
                || !IsAdapterReleaseAssemblyFingerprint(
                    envelope.m_artifactFingerprint)
                || !IsAdapterReleaseAssemblyFingerprint(
                    envelope.m_resultFingerprint)
                || !IsAdapterReleaseAssemblyUtcTimestamp(envelope.m_capturedAtUtc))
            {
                AddAssemblyIssue(
                    result,
                    flags.m_envelopeInvalid,
                    "release_assembly.envelope_invalid",
                    "The release assembly-result envelope requires contract version "
                    "one, stable identities, lowercase SHA-256 fingerprints, and a "
                    "UTC capture timestamp.");
            }
        }

        void ValidateObservationState(
            const AdapterReleaseChecksumObservation& observation,
            AdapterReleaseAssemblyEvidenceReturn& result,
            AssemblyValidationFlags& flags)
        {
            bool valid = true;
            switch (observation.m_outcome)
            {
            case AdapterReleaseChecksumObservationOutcome::NotObserved:
                valid = !observation.m_attempted
                    && !observation.m_observationRecorded
                    && observation.m_observedChecksum.empty()
                    && observation.m_observedAtUtc.empty()
                    && observation.m_failureIds.empty();
                break;
            case AdapterReleaseChecksumObservationOutcome::Matched:
                valid = observation.m_attempted
                    && observation.m_observationRecorded
                    && observation.m_observedChecksum
                        == observation.m_expectedChecksum
                    && IsAdapterReleaseAssemblyUtcTimestamp(
                        observation.m_observedAtUtc)
                    && observation.m_failureIds.empty();
                break;
            case AdapterReleaseChecksumObservationOutcome::Mismatched:
                valid = observation.m_attempted
                    && observation.m_observationRecorded
                    && IsAdapterReleaseAssemblyFingerprint(
                        observation.m_observedChecksum)
                    && observation.m_observedChecksum
                        != observation.m_expectedChecksum
                    && IsAdapterReleaseAssemblyUtcTimestamp(
                        observation.m_observedAtUtc);
                break;
            case AdapterReleaseChecksumObservationOutcome::Failed:
                valid = observation.m_attempted
                    && !observation.m_failureIds.empty()
                    && ((observation.m_observationRecorded
                            && IsAdapterReleaseAssemblyFingerprint(
                                observation.m_observedChecksum))
                        || (!observation.m_observationRecorded
                            && observation.m_observedChecksum.empty()))
                    && IsAdapterReleaseAssemblyUtcTimestamp(
                        observation.m_observedAtUtc);
                break;
            case AdapterReleaseChecksumObservationOutcome::Inconclusive:
                valid = observation.m_attempted
                    && IsAdapterReleaseAssemblyUtcTimestamp(
                        observation.m_observedAtUtc)
                    && ((observation.m_observationRecorded
                            && IsAdapterReleaseAssemblyFingerprint(
                                observation.m_observedChecksum))
                        || (!observation.m_observationRecorded
                            && observation.m_observedChecksum.empty()))
                    && (!observation.m_failureIds.empty()
                        || !observation.m_diagnosticReferenceIds.empty());
                break;
            }
            if (!valid)
            {
                AddAssemblyIssue(
                    result,
                    flags.m_checksumObservationMismatch,
                    "release_assembly.checksum_outcome_mismatch",
                    "Checksum observation flags, checksum values, timestamp, and "
                    "references do not agree with the supplied outcome.",
                    observation.m_contentId,
                    observation.m_observationId);
            }
        }

        void ValidateChecksumCoverage(
            const AdapterReleaseArtifactEnvelope& artifact,
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            AdapterReleaseAssemblyEvidenceReturn& result,
            AssemblyValidationFlags& flags)
        {
            AZStd::vector<AZStd::string> observationIds;
            AZStd::vector<AZStd::string> observedContentIds;
            for (const AdapterReleaseChecksumObservation& observation :
                 envelope.m_checksumObservations)
            {
                observationIds.push_back(observation.m_observationId);
                observedContentIds.push_back(observation.m_contentId);
                const AdapterReleaseArtifactContent* content =
                    FindContent(artifact, observation.m_contentId);
                if (!IsAdapterReleaseAssemblyStableId(observation.m_observationId)
                    || !content
                    || observation.m_packagePath !=
                        (content ? content->m_packagePath : AZStd::string{})
                    || observation.m_expectedChecksum !=
                        (content ? content->m_expectedChecksum : AZStd::string{})
                    || !IsAdapterReleaseAssemblyFingerprint(
                        observation.m_expectedChecksum))
                {
                    AddAssemblyIssue(
                        result,
                        flags.m_checksumObservationMismatch,
                        "release_assembly.checksum_binding_mismatch",
                        "Each checksum observation must bind to one declared content "
                        "identity, package path, and expected SHA-256 checksum.",
                        observation.m_contentId,
                        observation.m_observationId);
                }
                if (ContainsDuplicate(observation.m_failureIds)
                    || ContainsDuplicate(observation.m_diagnosticReferenceIds))
                {
                    AddAssemblyIssue(
                        result,
                        flags.m_checksumObservationMismatch,
                        "release_assembly.duplicate_observation_reference",
                        "Checksum failure and diagnostic references must be unique.",
                        observation.m_contentId,
                        observation.m_observationId);
                }
                ValidateObservationState(observation, result, flags);
            }

            if (envelope.m_checksumObservations.size() != artifact.m_contents.size()
                || ContainsDuplicate(observationIds)
                || ContainsDuplicate(observedContentIds))
            {
                AddAssemblyIssue(
                    result,
                    flags.m_contentCoverageIncomplete,
                    "release_assembly.content_coverage_incomplete",
                    "Every declared release-artifact content requires exactly one "
                    "uniquely identified checksum observation.");
            }
            for (const AdapterReleaseArtifactContent& content : artifact.m_contents)
            {
                if (AZStd::find(
                        observedContentIds.begin(),
                        observedContentIds.end(),
                        content.m_contentId)
                    == observedContentIds.end())
                {
                    AddAssemblyIssue(
                        result,
                        flags.m_contentCoverageIncomplete,
                        "release_assembly.content_observation_missing",
                        "Declared content has no supplied checksum observation.",
                        content.m_contentId);
                }
            }
        }

        void ValidateArchiveResult(
            const AdapterReleaseArchiveResult& archive,
            AdapterReleaseAssemblyEvidenceReturn& result,
            AssemblyValidationFlags& flags)
        {
            bool valid = IsAdapterReleaseAssemblyStableId(archive.m_archiveId)
                && IsAdapterReleaseAssemblySafeReference(archive.m_archivePath)
                && !archive.m_archiveFormat.empty()
                && !ContainsDuplicate(archive.m_failureIds)
                && !ContainsDuplicate(archive.m_diagnosticReferenceIds);
            switch (archive.m_outcome)
            {
            case AdapterReleaseAssemblyOutcome::NotAttempted:
            case AdapterReleaseAssemblyOutcome::Skipped:
                valid = valid
                    && !archive.m_attempted
                    && !archive.m_archivePresent
                    && archive.m_byteSize == 0
                    && archive.m_archiveFingerprint.empty()
                    && archive.m_completedAtUtc.empty();
                break;
            case AdapterReleaseAssemblyOutcome::Succeeded:
                valid = valid
                    && archive.m_attempted
                    && archive.m_archivePresent
                    && archive.m_byteSize > 0
                    && IsAdapterReleaseAssemblyFingerprint(
                        archive.m_archiveFingerprint)
                    && IsAdapterReleaseAssemblyUtcTimestamp(
                        archive.m_completedAtUtc)
                    && archive.m_failureIds.empty();
                break;
            case AdapterReleaseAssemblyOutcome::Failed:
                valid = valid
                    && archive.m_attempted
                    && IsAdapterReleaseAssemblyUtcTimestamp(
                        archive.m_completedAtUtc)
                    && !archive.m_failureIds.empty()
                    && ((!archive.m_archivePresent
                            && archive.m_byteSize == 0
                            && archive.m_archiveFingerprint.empty())
                        || (archive.m_byteSize > 0
                            && IsAdapterReleaseAssemblyFingerprint(
                                archive.m_archiveFingerprint)));
                break;
            }
            if (!valid)
            {
                AddAssemblyIssue(
                    result,
                    flags.m_archiveBindingMismatch,
                    "release_assembly.archive_result_mismatch",
                    "Archive identity, safe reference, outcome flags, size, "
                    "fingerprint, timestamp, and references must describe one "
                    "self-consistent supplied archive result.");
            }
        }

        void ValidateFailureDiagnosticBindings(
            const AdapterReleaseArtifactEnvelope& artifact,
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            AdapterReleaseAssemblyEvidenceReturn& result,
            AssemblyValidationFlags& flags)
        {
            AZStd::vector<AZStd::string> failureIds;
            AZStd::vector<AZStd::string> diagnosticIds;
            for (const AdapterReleaseAssemblyFailure& failure : envelope.m_failures)
            {
                failureIds.push_back(failure.m_failureId);
                if (!IsAdapterReleaseAssemblyStableId(failure.m_failureId)
                    || failure.m_code.empty()
                    || failure.m_message.empty()
                    || (!failure.m_contentId.empty()
                        && !FindContent(artifact, failure.m_contentId))
                    || ContainsDuplicate(failure.m_diagnosticReferenceIds))
                {
                    AddAssemblyIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "release_assembly.failure_invalid",
                        "Failures require stable identity, safe content binding, code, "
                        "message, and unique diagnostic references.",
                        failure.m_contentId,
                        {},
                        failure.m_failureId);
                }
            }
            for (const AdapterReleaseAssemblyDiagnosticReference& diagnostic :
                 envelope.m_diagnosticReferences)
            {
                diagnosticIds.push_back(diagnostic.m_diagnosticId);
                bool valid =
                    IsAdapterReleaseAssemblyStableId(diagnostic.m_diagnosticId)
                    && IsAdapterReleaseAssemblySafeReference(diagnostic.m_reference)
                    && IsAdapterReleaseAssemblyFingerprint(diagnostic.m_fingerprint)
                    && !ContainsDuplicate(diagnostic.m_contentIds);
                for (const AZStd::string& contentId : diagnostic.m_contentIds)
                {
                    valid = valid && FindContent(artifact, contentId);
                }
                if (!valid)
                {
                    AddAssemblyIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "release_assembly.diagnostic_invalid",
                        "Diagnostics require stable identity, safe reference, "
                        "fingerprint, and declared content bindings.",
                        {},
                        {},
                        {},
                        diagnostic.m_diagnosticId);
                }
            }
            if (ContainsDuplicate(failureIds) || ContainsDuplicate(diagnosticIds))
            {
                AddAssemblyIssue(
                    result,
                    flags.m_failureDiagnosticBindingMismatch,
                    "release_assembly.duplicate_failure_or_diagnostic",
                    "Failure and diagnostic identities must be unique.");
            }

            auto validateReferences = [&](const AZStd::vector<AZStd::string>& failures,
                                          const AZStd::vector<AZStd::string>& diagnostics,
                                          const AZStd::string& contentId,
                                          const AZStd::string& observationId)
            {
                for (const AZStd::string& failureId : failures)
                {
                    const AdapterReleaseAssemblyFailure* failure =
                        FindFailure(envelope, failureId);
                    if (!failure
                        || (!failure->m_contentId.empty()
                            && failure->m_contentId != contentId))
                    {
                        AddAssemblyIssue(
                            result,
                            flags.m_failureDiagnosticBindingMismatch,
                            "release_assembly.failure_reference_mismatch",
                            "A checksum/archive result references an absent or "
                            "differently bound failure.",
                            contentId,
                            observationId,
                            failureId);
                    }
                }
                for (const AZStd::string& diagnosticId : diagnostics)
                {
                    const AdapterReleaseAssemblyDiagnosticReference* diagnostic =
                        FindDiagnostic(envelope, diagnosticId);
                    const bool contentBindingMatches = !diagnostic
                        || contentId.empty()
                        || diagnostic->m_contentIds.empty()
                        || AZStd::find(
                               diagnostic->m_contentIds.begin(),
                               diagnostic->m_contentIds.end(),
                               contentId)
                            != diagnostic->m_contentIds.end();
                    if (!diagnostic || !contentBindingMatches)
                    {
                        AddAssemblyIssue(
                            result,
                            flags.m_failureDiagnosticBindingMismatch,
                            "release_assembly.diagnostic_reference_mismatch",
                            "A checksum/archive result references an absent or "
                            "differently bound diagnostic.",
                            contentId,
                            observationId,
                            {},
                            diagnosticId);
                    }
                }
            };

            for (const AdapterReleaseChecksumObservation& observation :
                 envelope.m_checksumObservations)
            {
                validateReferences(
                    observation.m_failureIds,
                    observation.m_diagnosticReferenceIds,
                    observation.m_contentId,
                    observation.m_observationId);
            }
            validateReferences(
                envelope.m_archive.m_failureIds,
                envelope.m_archive.m_diagnosticReferenceIds,
                {},
                {});
            for (const AdapterReleaseAssemblyFailure& failure : envelope.m_failures)
            {
                for (const AZStd::string& diagnosticId :
                     failure.m_diagnosticReferenceIds)
                {
                    const AdapterReleaseAssemblyDiagnosticReference* diagnostic =
                        FindDiagnostic(envelope, diagnosticId);
                    const bool contentBindingMatches = !diagnostic
                        || failure.m_contentId.empty()
                        || diagnostic->m_contentIds.empty()
                        || AZStd::find(
                               diagnostic->m_contentIds.begin(),
                               diagnostic->m_contentIds.end(),
                               failure.m_contentId)
                            != diagnostic->m_contentIds.end();
                    if (!diagnostic || !contentBindingMatches)
                    {
                        AddAssemblyIssue(
                            result,
                            flags.m_failureDiagnosticBindingMismatch,
                            "release_assembly.failure_diagnostic_mismatch",
                            "A failure references an absent or differently bound "
                            "diagnostic.",
                            failure.m_contentId,
                            {},
                            failure.m_failureId,
                            diagnosticId);
                    }
                }
            }
        }

        AdapterReleaseAssemblyEnvelopeStatus ResolveStatus(
            const AssemblyValidationFlags& flags)
        {
            if (flags.m_artifactNotReady)
            {
                return AdapterReleaseAssemblyEnvelopeStatus::ArtifactNotReady;
            }
            if (flags.m_assemblerUnreviewed)
            {
                return AdapterReleaseAssemblyEnvelopeStatus::AssemblerUnreviewed;
            }
            if (flags.m_artifactBindingMismatch)
            {
                return AdapterReleaseAssemblyEnvelopeStatus::ArtifactBindingMismatch;
            }
            if (flags.m_envelopeInvalid)
            {
                return AdapterReleaseAssemblyEnvelopeStatus::EnvelopeInvalid;
            }
            if (flags.m_contentCoverageIncomplete)
            {
                return AdapterReleaseAssemblyEnvelopeStatus::ContentCoverageIncomplete;
            }
            if (flags.m_checksumObservationMismatch)
            {
                return AdapterReleaseAssemblyEnvelopeStatus::ChecksumObservationMismatch;
            }
            if (flags.m_archiveBindingMismatch)
            {
                return AdapterReleaseAssemblyEnvelopeStatus::ArchiveBindingMismatch;
            }
            if (flags.m_failureDiagnosticBindingMismatch)
            {
                return AdapterReleaseAssemblyEnvelopeStatus::FailureDiagnosticBindingMismatch;
            }
            return AdapterReleaseAssemblyEnvelopeStatus::Accepted;
        }

        bool ContractIsValid(const AssemblyValidationFlags& flags)
        {
            return !flags.m_artifactNotReady
                && !flags.m_assemblerUnreviewed
                && !flags.m_artifactBindingMismatch
                && !flags.m_envelopeInvalid
                && !flags.m_contentCoverageIncomplete
                && !flags.m_checksumObservationMismatch
                && !flags.m_archiveBindingMismatch
                && !flags.m_failureDiagnosticBindingMismatch;
        }

        SourceDocument BuildSourceDocument(
            const AZStd::string& sourceId,
            const AZStd::string& title,
            const AZStd::string& sourceKind,
            const AZStd::string& locator,
            const AZStd::string& fingerprint,
            const AdapterReleaseAssemblyResultEnvelope& envelope,
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
                envelope.m_assemblerReview.m_assemblerId;
            document.m_source.m_toolVersion =
                envelope.m_assemblerReview.m_assemblerVersion;
            document.m_source.m_importerId = "tg.release-assembly-result";
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
            const AdapterReleaseAssemblyResultEnvelope& envelope,
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
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            AdapterReleaseAssemblyEvidenceReturn& result)
        {
            const AZStd::string sourceId =
                "source.release-assembly." + envelope.m_resultId;
            const AZStd::string locator =
                "release-assembly-results/" + envelope.m_resultId + ".json";
            SourceDocument sourceDocument = BuildSourceDocument(
                sourceId,
                "Release assembly result " + envelope.m_resultId,
                "release_assembly_result",
                locator,
                envelope.m_resultFingerprint,
                envelope,
                "application/vnd.taintedgrail.release-assembly-result+json",
                "Contract-validated separately supplied assembler/checksummer metadata "
                "only. The TG SDK did not read or hash files, assemble an archive, sign, "
                "upload, publish, launch FoA, call an adapter, or mutate deployment.");

            EvidenceDocument evidenceDocument;
            evidenceDocument.m_sourceId = sourceId;
            evidenceDocument.m_sourceFingerprint = envelope.m_resultFingerprint;
            evidenceDocument.m_profileId = envelope.m_profileId;
            evidenceDocument.m_gameVersion = envelope.m_gameVersion;
            evidenceDocument.m_branch = envelope.m_branch;
            evidenceDocument.m_evidence.push_back(BuildEvidence(
                "evidence.release-assembly." + envelope.m_resultId
                    + ".artifact-binding",
                sourceDocument.m_source,
                envelope,
                "release-artifact:" + envelope.m_artifactId,
                "External release assembly result binds to exact ready artifact "
                    + envelope.m_artifactId + " with fingerprint "
                    + envelope.m_artifactFingerprint + ".",
                "release_assembly_artifact_binding",
                locator,
                "ArtifactBinding"));
            evidenceDocument.m_evidence.push_back(BuildEvidence(
                "evidence.release-assembly." + envelope.m_resultId
                    + ".assembler-review",
                sourceDocument.m_source,
                envelope,
                "release-assembly-result:" + envelope.m_resultId,
                "Assembler/checksummer "
                    + envelope.m_assemblerReview.m_assemblerId + " version "
                    + envelope.m_assemblerReview.m_assemblerVersion
                    + " was supplied with accepted review "
                    + envelope.m_assemblerReview.m_reviewId + ".",
                "release_assembler_review",
                locator,
                "AssemblerReview"));
            evidenceDocument.m_evidence.push_back(BuildEvidence(
                "evidence.release-assembly." + envelope.m_resultId + ".archive",
                sourceDocument.m_source,
                envelope,
                "release-archive:" + envelope.m_archive.m_archiveId,
                "External assembler reported archive outcome "
                    + ToString(envelope.m_archive.m_outcome) + " for "
                    + envelope.m_archive.m_archivePath + " with fingerprint "
                    + envelope.m_archive.m_archiveFingerprint + ".",
                "release_archive_assembly_result",
                locator,
                "Archive"));

            AZStd::vector<const AdapterReleaseChecksumObservation*> observations;
            for (const AdapterReleaseChecksumObservation& observation :
                 envelope.m_checksumObservations)
            {
                observations.push_back(&observation);
            }
            AZStd::sort(
                observations.begin(),
                observations.end(),
                [](const AdapterReleaseChecksumObservation* left,
                   const AdapterReleaseChecksumObservation* right)
                {
                    return left->m_contentId < right->m_contentId;
                });
            for (size_t index = 0; index < observations.size(); ++index)
            {
                const AdapterReleaseChecksumObservation& observation =
                    *observations[index];
                evidenceDocument.m_evidence.push_back(BuildEvidence(
                    "evidence.release-assembly." + envelope.m_resultId
                        + ".checksum." + UnsignedAssemblyString(index + 1),
                    sourceDocument.m_source,
                    envelope,
                    "release-artifact:" + envelope.m_artifactId + ":content:"
                        + observation.m_contentId,
                    "External checksummer reported "
                        + ToString(observation.m_outcome) + " for "
                        + observation.m_packagePath + " with expected checksum "
                        + observation.m_expectedChecksum + " and observed checksum "
                        + observation.m_observedChecksum + ".",
                    "release_content_checksum_observation",
                    locator,
                    "ChecksumObservations/" + UnsignedAssemblyString(index + 1)));
            }
            AZStd::vector<const AdapterReleaseAssemblyFailure*> failures;
            for (const AdapterReleaseAssemblyFailure& failure : envelope.m_failures)
            {
                failures.push_back(&failure);
            }
            AZStd::sort(
                failures.begin(),
                failures.end(),
                [](const AdapterReleaseAssemblyFailure* left,
                   const AdapterReleaseAssemblyFailure* right)
                {
                    return left->m_failureId < right->m_failureId;
                });
            for (const AdapterReleaseAssemblyFailure* failure : failures)
            {
                evidenceDocument.m_evidence.push_back(BuildEvidence(
                    "evidence.release-assembly." + envelope.m_resultId + ".failure."
                        + failure->m_failureId,
                    sourceDocument.m_source,
                    envelope,
                    failure->m_contentId.empty()
                        ? "release-assembly-result:" + envelope.m_resultId
                        : "release-artifact:" + envelope.m_artifactId + ":content:"
                            + failure->m_contentId,
                    "External release assembly failure " + failure->m_failureId
                        + " reported kind " + ToString(failure->m_kind) + ", code "
                        + failure->m_code + ": " + failure->m_message,
                    "release_assembly_failure",
                    locator,
                    "Failures/" + failure->m_failureId));
            }
            result.m_sourceDocuments.push_back(AZStd::move(sourceDocument));
            result.m_evidenceDocuments.push_back(AZStd::move(evidenceDocument));
        }

        void BuildDiagnosticEvidence(
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            AdapterReleaseAssemblyEvidenceReturn& result)
        {
            AZStd::vector<const AdapterReleaseAssemblyDiagnosticReference*>
                diagnostics;
            for (const AdapterReleaseAssemblyDiagnosticReference& diagnostic :
                 envelope.m_diagnosticReferences)
            {
                diagnostics.push_back(&diagnostic);
            }
            AZStd::sort(
                diagnostics.begin(),
                diagnostics.end(),
                [](const AdapterReleaseAssemblyDiagnosticReference* left,
                   const AdapterReleaseAssemblyDiagnosticReference* right)
                {
                    return left->m_diagnosticId < right->m_diagnosticId;
                });
            for (const AdapterReleaseAssemblyDiagnosticReference* diagnostic :
                 diagnostics)
            {
                const AZStd::string sourceId =
                    "source.release-assembly-diagnostic." + envelope.m_resultId + "."
                    + diagnostic->m_diagnosticId;
                SourceDocument sourceDocument = BuildSourceDocument(
                    sourceId,
                    "Release assembly diagnostic " + diagnostic->m_diagnosticId,
                    "release_assembly_diagnostic",
                    diagnostic->m_reference,
                    diagnostic->m_fingerprint,
                    envelope,
                    "text/plain",
                    "Referenced assembler/checksummer diagnostic content is not opened, "
                    "persisted, or inspected by the TG SDK.");
                EvidenceDocument evidenceDocument;
                evidenceDocument.m_sourceId = sourceId;
                evidenceDocument.m_sourceFingerprint = diagnostic->m_fingerprint;
                evidenceDocument.m_profileId = envelope.m_profileId;
                evidenceDocument.m_gameVersion = envelope.m_gameVersion;
                evidenceDocument.m_branch = envelope.m_branch;

                AZStd::vector<AZStd::string> contentIds = diagnostic->m_contentIds;
                AZStd::sort(contentIds.begin(), contentIds.end());
                if (contentIds.empty())
                {
                    evidenceDocument.m_evidence.push_back(BuildEvidence(
                        "evidence.release-assembly-diagnostic." + envelope.m_resultId
                            + "." + diagnostic->m_diagnosticId + ".archive",
                        sourceDocument.m_source,
                        envelope,
                        "release-archive:" + envelope.m_archive.m_archiveId,
                        "External " + ToString(diagnostic->m_kind) + " diagnostic "
                            + diagnostic->m_diagnosticId
                            + " is referenced with fingerprint "
                            + diagnostic->m_fingerprint + ".",
                        "release_assembly_diagnostic_reference",
                        diagnostic->m_reference,
                        "Diagnostics/" + diagnostic->m_diagnosticId));
                }
                for (size_t index = 0; index < contentIds.size(); ++index)
                {
                    evidenceDocument.m_evidence.push_back(BuildEvidence(
                        "evidence.release-assembly-diagnostic." + envelope.m_resultId
                            + "." + diagnostic->m_diagnosticId + "."
                            + UnsignedAssemblyString(index + 1),
                        sourceDocument.m_source,
                        envelope,
                        "release-artifact:" + envelope.m_artifactId + ":content:"
                            + contentIds[index],
                        "External " + ToString(diagnostic->m_kind) + " diagnostic "
                            + diagnostic->m_diagnosticId
                            + " is referenced with fingerprint "
                            + diagnostic->m_fingerprint + ".",
                        "release_assembly_diagnostic_reference",
                        diagnostic->m_reference,
                        "Diagnostics/" + diagnostic->m_diagnosticId + "/"
                            + UnsignedAssemblyString(index + 1)));
                }
                result.m_sourceDocuments.push_back(AZStd::move(sourceDocument));
                result.m_evidenceDocuments.push_back(AZStd::move(evidenceDocument));
            }
        }

        void FinalizeCounts(
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            AdapterReleaseAssemblyEvidenceReturn& result)
        {
            result.m_checksumObservationCount = static_cast<AZ::u64>(
                envelope.m_checksumObservations.size());
            for (const AdapterReleaseChecksumObservation& observation :
                 envelope.m_checksumObservations)
            {
                switch (observation.m_outcome)
                {
                case AdapterReleaseChecksumObservationOutcome::Matched:
                    ++result.m_matchedChecksumCount;
                    break;
                case AdapterReleaseChecksumObservationOutcome::Mismatched:
                    ++result.m_mismatchedChecksumCount;
                    break;
                case AdapterReleaseChecksumObservationOutcome::Failed:
                    ++result.m_failedChecksumCount;
                    break;
                case AdapterReleaseChecksumObservationOutcome::Inconclusive:
                    ++result.m_inconclusiveChecksumCount;
                    break;
                case AdapterReleaseChecksumObservationOutcome::NotObserved:
                    ++result.m_notObservedChecksumCount;
                    break;
                }
            }
            result.m_failureCount = static_cast<AZ::u64>(envelope.m_failures.size());
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

        void SortIssues(AdapterReleaseAssemblyEvidenceReturn& result)
        {
            AZStd::sort(
                result.m_issues.begin(),
                result.m_issues.end(),
                [](const AdapterReleaseAssemblyIssue& left,
                   const AdapterReleaseAssemblyIssue& right)
                {
                    if (left.m_code != right.m_code)
                    {
                        return left.m_code < right.m_code;
                    }
                    if (left.m_contentId != right.m_contentId)
                    {
                        return left.m_contentId < right.m_contentId;
                    }
                    if (left.m_observationId != right.m_observationId)
                    {
                        return left.m_observationId < right.m_observationId;
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

    AdapterReleaseAssemblyEvidenceReturn
    AdapterReleaseAssemblyEvidenceService::BuildEvidenceReturn(
        const AdapterReleaseArtifactEnvelope& artifactEnvelope,
        const AdapterReleaseAssemblyResultEnvelope& resultEnvelope) const
    {
        AdapterReleaseAssemblyEvidenceReturn result;
        result.m_resultId = resultEnvelope.m_resultId;
        result.m_artifactId = resultEnvelope.m_artifactId;
        result.m_artifactFingerprint = resultEnvelope.m_artifactFingerprint;
        result.m_resultFingerprint = resultEnvelope.m_resultFingerprint;

        AssemblyValidationFlags flags;
        ValidateArtifactReadiness(artifactEnvelope, result, flags);
        ValidateAssemblerReview(resultEnvelope.m_assemblerReview, result, flags);
        ValidateArtifactBinding(artifactEnvelope, resultEnvelope, result, flags);
        ValidateEnvelopeShape(resultEnvelope, result, flags);
        ValidateChecksumCoverage(
            artifactEnvelope,
            resultEnvelope,
            result,
            flags);
        ValidateArchiveResult(resultEnvelope.m_archive, result, flags);
        ValidateFailureDiagnosticBindings(
            artifactEnvelope,
            resultEnvelope,
            result,
            flags);

        result.m_status = ResolveStatus(flags);
        result.m_contractValid = ContractIsValid(flags);
        result.m_accepted =
            result.m_status == AdapterReleaseAssemblyEnvelopeStatus::Accepted;
        SortIssues(result);
        if (result.m_contractValid)
        {
            BuildPrimaryEvidence(resultEnvelope, result);
            BuildDiagnosticEvidence(resultEnvelope, result);
        }
        FinalizeCounts(resultEnvelope, result);
        return result;
    }
} // namespace TaintedGrailModdingSDK
