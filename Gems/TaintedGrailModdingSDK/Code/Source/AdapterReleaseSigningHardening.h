/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterReleaseAssemblyEvidenceService.h"
#include "AdapterReleaseSigningResultContracts.h"
#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"
#include "PackagePathValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/string/string.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    inline constexpr size_t MaximumReleaseSigningRegistryEnvelopes = 128;
    inline constexpr size_t MaximumReleaseSigningSignatureArtifacts = 128;
    inline constexpr size_t MaximumReleaseSigningFailures = 128;
    inline constexpr size_t MaximumReleaseSigningDiagnostics = 128;
    inline constexpr size_t MaximumReleaseSigningReferencesPerRecord = 64;
    inline constexpr size_t MaximumReleaseSigningCapabilities = 8;
    inline constexpr size_t MaximumReleaseSigningReviewEvidence = 64;
    inline constexpr size_t MaximumReleaseSigningPublicTextBytes = 256 * 1024;
    inline constexpr size_t MaximumReleaseSigningEditorRows = 1024;

    inline AZStd::string FoldReleaseSigningAscii(AZStd::string value)
    {
        for (char& character : value)
        {
            if (character >= 'A' && character <= 'Z')
            {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }
        return value;
    }

    inline bool HasReleaseSigningPrivatePathShape(const AZStd::string& value)
    {
        if (value.empty())
        {
            return false;
        }
        if (value[0] == '/' || value[0] == '\\')
        {
            return true;
        }
        if (value.size() >= 3
            && ((value[0] >= 'A' && value[0] <= 'Z')
                || (value[0] >= 'a' && value[0] <= 'z'))
            && value[1] == ':'
            && (value[2] == '/' || value[2] == '\\'))
        {
            return true;
        }
        const AZStd::string folded = FoldReleaseSigningAscii(value);
        return folded.find("file://") != AZStd::string::npos
            || folded.find("\\\\") == 0;
    }

    inline bool HasReleaseSigningSensitiveMarker(const AZStd::string& value)
    {
        const AZStd::string folded = FoldReleaseSigningAscii(value);
        constexpr const char* Markers[] = {
            "password=",
            "passwd=",
            "token=",
            "api_key",
            "apikey",
            "authorization:",
            "bearer ",
            "private key",
            "client_secret",
            "access_key",
            "secret=",
            "pin=",
        };
        for (const char* marker : Markers)
        {
            if (folded.find(marker) != AZStd::string::npos)
            {
                return true;
            }
        }

        const size_t scheme = folded.find("://");
        if (scheme != AZStd::string::npos)
        {
            const size_t authorityStart = scheme + 3;
            const size_t slash = folded.find('/', authorityStart);
            const size_t at = folded.find('@', authorityStart);
            if (at != AZStd::string::npos
                && (slash == AZStd::string::npos || at < slash))
            {
                return true;
            }
        }
        return false;
    }

    inline bool IsReleaseSigningPublicText(
        const AZStd::string& value,
        size_t maximumLength,
        bool allowEmpty = false,
        bool allowPathLikeText = false)
    {
        if ((!allowEmpty && value.empty()) || value.size() > maximumLength)
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
        return !HasReleaseSigningSensitiveMarker(value)
            && (allowPathLikeText || !HasReleaseSigningPrivatePathShape(value));
    }

    inline bool IsKnownReleaseSignerReviewDecision(
        AdapterReleaseSignerReviewDecision value)
    {
        switch (value)
        {
        case AdapterReleaseSignerReviewDecision::Unknown:
        case AdapterReleaseSignerReviewDecision::Accepted:
        case AdapterReleaseSignerReviewDecision::Rejected:
            return true;
        default:
            return false;
        }
    }

    inline bool IsKnownReleaseSignerCapability(
        AdapterReleaseSignerCapability value)
    {
        switch (value)
        {
        case AdapterReleaseSignerCapability::ArchiveSigning:
        case AdapterReleaseSignerCapability::SignatureArtifactFingerprint:
            return true;
        default:
            return false;
        }
    }

    inline bool IsKnownReleaseSigningOutcome(AdapterReleaseSigningOutcome value)
    {
        switch (value)
        {
        case AdapterReleaseSigningOutcome::NotAttempted:
        case AdapterReleaseSigningOutcome::Succeeded:
        case AdapterReleaseSigningOutcome::Failed:
        case AdapterReleaseSigningOutcome::Skipped:
            return true;
        default:
            return false;
        }
    }

    inline bool IsKnownReleaseSignatureArtifactKind(
        AdapterReleaseSignatureArtifactKind value)
    {
        switch (value)
        {
        case AdapterReleaseSignatureArtifactKind::DetachedSignature:
        case AdapterReleaseSignatureArtifactKind::SignatureBundle:
        case AdapterReleaseSignatureArtifactKind::CertificateBundle:
        case AdapterReleaseSignatureArtifactKind::TransparencyReceipt:
        case AdapterReleaseSignatureArtifactKind::Attestation:
        case AdapterReleaseSignatureArtifactKind::Other:
            return true;
        default:
            return false;
        }
    }

    inline bool IsKnownReleaseSigningFailureKind(
        AdapterReleaseSigningFailureKind value)
    {
        switch (value)
        {
        case AdapterReleaseSigningFailureKind::Contract:
        case AdapterReleaseSigningFailureKind::InputBinding:
        case AdapterReleaseSigningFailureKind::SignerUnavailable:
        case AdapterReleaseSigningFailureKind::IdentityUnavailable:
        case AdapterReleaseSigningFailureKind::AccessDenied:
        case AdapterReleaseSigningFailureKind::UnsupportedIdentity:
        case AdapterReleaseSigningFailureKind::UnsupportedFormat:
        case AdapterReleaseSigningFailureKind::ArchiveRead:
        case AdapterReleaseSigningFailureKind::Signing:
        case AdapterReleaseSigningFailureKind::SignatureArtifact:
        case AdapterReleaseSigningFailureKind::Internal:
        case AdapterReleaseSigningFailureKind::Unknown:
            return true;
        default:
            return false;
        }
    }

    inline bool IsKnownReleaseSigningDiagnosticKind(
        AdapterReleaseSigningDiagnosticKind value)
    {
        switch (value)
        {
        case AdapterReleaseSigningDiagnosticKind::Signer:
        case AdapterReleaseSigningDiagnosticKind::Identity:
        case AdapterReleaseSigningDiagnosticKind::Archive:
        case AdapterReleaseSigningDiagnosticKind::SignatureArtifact:
        case AdapterReleaseSigningDiagnosticKind::Binding:
        case AdapterReleaseSigningDiagnosticKind::Diagnostic:
            return true;
        default:
            return false;
        }
    }

    inline bool IsKnownReleaseSigningIntentDecision(
        AdapterReleaseSigningIntentDecision value)
    {
        switch (value)
        {
        case AdapterReleaseSigningIntentDecision::Unknown:
        case AdapterReleaseSigningIntentDecision::Unsigned:
        case AdapterReleaseSigningIntentDecision::SignExternally:
            return true;
        default:
            return false;
        }
    }

    inline bool IsKnownReleaseSigningIdentityKind(
        AdapterReleaseSigningIdentityKind value)
    {
        switch (value)
        {
        case AdapterReleaseSigningIdentityKind::None:
        case AdapterReleaseSigningIdentityKind::Pgp:
        case AdapterReleaseSigningIdentityKind::X509:
        case AdapterReleaseSigningIdentityKind::Sigstore:
        case AdapterReleaseSigningIdentityKind::Platform:
        case AdapterReleaseSigningIdentityKind::Other:
            return true;
        default:
            return false;
        }
    }

    inline bool AreReleaseSigningEnumsKnown(
        const AdapterReleaseSigningResultEnvelope& envelope)
    {
        if (!IsKnownReleaseSigningIntentDecision(
                envelope.m_signingIntentDecision)
            || !IsKnownReleaseSigningIdentityKind(
                envelope.m_signingIdentityKind)
            || !IsKnownReleaseSignerReviewDecision(
                envelope.m_signerReview.m_decision)
            || !IsKnownReleaseSigningOutcome(envelope.m_outcome))
        {
            return false;
        }
        for (AdapterReleaseSignerCapability capability :
             envelope.m_signerReview.m_capabilities)
        {
            if (!IsKnownReleaseSignerCapability(capability))
            {
                return false;
            }
        }
        for (const AdapterReleaseSignatureArtifact& artifact :
             envelope.m_signatureArtifacts)
        {
            if (!IsKnownReleaseSignatureArtifactKind(artifact.m_kind))
            {
                return false;
            }
        }
        for (const AdapterReleaseSigningFailure& failure : envelope.m_failures)
        {
            if (!IsKnownReleaseSigningFailureKind(failure.m_kind))
            {
                return false;
            }
        }
        for (const AdapterReleaseSigningDiagnosticReference& diagnostic :
             envelope.m_diagnosticReferences)
        {
            if (!IsKnownReleaseSigningDiagnosticKind(diagnostic.m_kind))
            {
                return false;
            }
        }
        return true;
    }

    inline bool AddReleaseSigningTextBudget(
        size_t& total,
        const AZStd::string& value)
    {
        if (value.size() > MaximumReleaseSigningPublicTextBytes - total)
        {
            return false;
        }
        total += value.size();
        return true;
    }

    inline bool IsReleaseSigningEnvelopeWithinLimits(
        const AdapterReleaseSigningResultEnvelope& envelope)
    {
        if (envelope.m_signatureArtifacts.size()
                > MaximumReleaseSigningSignatureArtifacts
            || envelope.m_failures.size() > MaximumReleaseSigningFailures
            || envelope.m_diagnosticReferences.size()
                > MaximumReleaseSigningDiagnostics
            || envelope.m_failureIds.size() > MaximumReleaseSigningFailures
            || envelope.m_diagnosticReferenceIds.size()
                > MaximumReleaseSigningDiagnostics
            || envelope.m_signerReview.m_capabilities.size()
                > MaximumReleaseSigningCapabilities
            || envelope.m_signerReview.m_evidenceIds.size()
                > MaximumReleaseSigningReviewEvidence)
        {
            return false;
        }

        size_t totalText = 0;
        const AZStd::string* topLevel[] = {
            &envelope.m_resultId,
            &envelope.m_resultFingerprint,
            &envelope.m_artifactId,
            &envelope.m_artifactFingerprint,
            &envelope.m_assemblyResultId,
            &envelope.m_assemblyResultFingerprint,
            &envelope.m_archiveId,
            &envelope.m_archivePath,
            &envelope.m_archiveFormat,
            &envelope.m_archiveFingerprint,
            &envelope.m_reconciliationId,
            &envelope.m_packagePreviewId,
            &envelope.m_manifestId,
            &envelope.m_manifestFingerprint,
            &envelope.m_packId,
            &envelope.m_packVersion,
            &envelope.m_profileId,
            &envelope.m_gameVersion,
            &envelope.m_branch,
            &envelope.m_runtimeTarget,
            &envelope.m_signingIntentId,
            &envelope.m_signerId,
            &envelope.m_identityLocator,
            &envelope.m_identityFingerprint,
            &envelope.m_completedAtUtc,
            &envelope.m_capturedAtUtc,
            &envelope.m_signerReview.m_reviewId,
            &envelope.m_signerReview.m_signerToolId,
            &envelope.m_signerReview.m_signerToolVersion,
            &envelope.m_signerReview.m_signerToolFingerprint,
            &envelope.m_signerReview.m_reviewer,
            &envelope.m_signerReview.m_reviewedAtUtc,
            &envelope.m_signerReview.m_notes,
        };
        for (const AZStd::string* value : topLevel)
        {
            if (!AddReleaseSigningTextBudget(totalText, *value))
            {
                return false;
            }
        }
        for (const AZStd::string& value : envelope.m_failureIds)
        {
            if (!AddReleaseSigningTextBudget(totalText, value))
            {
                return false;
            }
        }
        for (const AZStd::string& value : envelope.m_diagnosticReferenceIds)
        {
            if (!AddReleaseSigningTextBudget(totalText, value))
            {
                return false;
            }
        }
        for (const AZStd::string& value : envelope.m_signerReview.m_evidenceIds)
        {
            if (!AddReleaseSigningTextBudget(totalText, value))
            {
                return false;
            }
        }
        for (const AdapterReleaseSignatureArtifact& artifact :
             envelope.m_signatureArtifacts)
        {
            if (artifact.m_diagnosticReferenceIds.size()
                    > MaximumReleaseSigningReferencesPerRecord
                || !AddReleaseSigningTextBudget(
                    totalText,
                    artifact.m_signatureArtifactId)
                || !AddReleaseSigningTextBudget(totalText, artifact.m_reference)
                || !AddReleaseSigningTextBudget(totalText, artifact.m_mediaType)
                || !AddReleaseSigningTextBudget(totalText, artifact.m_fingerprint)
                || !AddReleaseSigningTextBudget(totalText, artifact.m_createdAtUtc)
                || !AddReleaseSigningTextBudget(totalText, artifact.m_archiveId)
                || !AddReleaseSigningTextBudget(
                    totalText,
                    artifact.m_archiveFingerprint))
            {
                return false;
            }
            for (const AZStd::string& value : artifact.m_diagnosticReferenceIds)
            {
                if (!AddReleaseSigningTextBudget(totalText, value))
                {
                    return false;
                }
            }
        }
        for (const AdapterReleaseSigningFailure& failure : envelope.m_failures)
        {
            if (failure.m_diagnosticReferenceIds.size()
                    > MaximumReleaseSigningReferencesPerRecord
                || !AddReleaseSigningTextBudget(totalText, failure.m_failureId)
                || !AddReleaseSigningTextBudget(totalText, failure.m_code)
                || !AddReleaseSigningTextBudget(totalText, failure.m_message)
                || !AddReleaseSigningTextBudget(
                    totalText,
                    failure.m_signatureArtifactId))
            {
                return false;
            }
            for (const AZStd::string& value : failure.m_diagnosticReferenceIds)
            {
                if (!AddReleaseSigningTextBudget(totalText, value))
                {
                    return false;
                }
            }
        }
        for (const AdapterReleaseSigningDiagnosticReference& diagnostic :
             envelope.m_diagnosticReferences)
        {
            if (diagnostic.m_signatureArtifactIds.size()
                    > MaximumReleaseSigningReferencesPerRecord
                || !AddReleaseSigningTextBudget(
                    totalText,
                    diagnostic.m_diagnosticId)
                || !AddReleaseSigningTextBudget(totalText, diagnostic.m_reference)
                || !AddReleaseSigningTextBudget(totalText, diagnostic.m_fingerprint))
            {
                return false;
            }
            for (const AZStd::string& value : diagnostic.m_signatureArtifactIds)
            {
                if (!AddReleaseSigningTextBudget(totalText, value))
                {
                    return false;
                }
            }
        }
        return true;
    }

    inline bool IsReleaseSigningEnvelopePublic(
        const AdapterReleaseSigningResultEnvelope& envelope)
    {
        if (!IsReleaseSigningPublicText(envelope.m_archiveFormat, 64)
            || !IsReleaseSigningPublicText(envelope.m_gameVersion, 256)
            || !IsReleaseSigningPublicText(envelope.m_branch, 256)
            || !IsReleaseSigningPublicText(envelope.m_runtimeTarget, 64)
            || !IsReleaseSigningPublicText(envelope.m_identityLocator, 1024)
            || !IsReleaseSigningPublicText(
                envelope.m_signerReview.m_reviewer,
                256)
            || !IsReleaseSigningPublicText(
                envelope.m_signerReview.m_notes,
                2048,
                true))
        {
            return false;
        }
        for (const AdapterReleaseSignatureArtifact& artifact :
             envelope.m_signatureArtifacts)
        {
            if (!IsReleaseSigningPublicText(artifact.m_mediaType, 256))
            {
                return false;
            }
        }
        for (const AdapterReleaseSigningFailure& failure : envelope.m_failures)
        {
            if (!IsReleaseSigningPublicText(failure.m_code, 128)
                || !IsReleaseSigningPublicText(failure.m_message, 2048))
            {
                return false;
            }
        }
        return true;
    }

    inline bool SameReleaseSigningImportIssue(
        const ImportIssue& left,
        const ImportIssue& right)
    {
        return left.m_issueId == right.m_issueId
            && left.m_severity == right.m_severity
            && left.m_code == right.m_code
            && left.m_message == right.m_message
            && left.m_locator == right.m_locator
            && left.m_recordPath == right.m_recordPath
            && left.m_line == right.m_line;
    }

    inline bool SameReleaseSigningSourceRecord(
        const SourceRecord& left,
        const SourceRecord& right)
    {
        return left.m_sourceId == right.m_sourceId
            && left.m_title == right.m_title
            && left.m_sourceKind == right.m_sourceKind
            && left.m_locator == right.m_locator
            && left.m_fingerprint == right.m_fingerprint
            && left.m_profileId == right.m_profileId
            && left.m_gameVersion == right.m_gameVersion
            && left.m_branch == right.m_branch
            && left.m_runtimeTarget == right.m_runtimeTarget
            && left.m_toolName == right.m_toolName
            && left.m_toolVersion == right.m_toolVersion
            && left.m_importerId == right.m_importerId
            && left.m_importerVersion == right.m_importerVersion
            && left.m_capturedAt == right.m_capturedAt
            && left.m_importedAt == right.m_importedAt
            && left.m_limitations == right.m_limitations
            && left.m_mediaType == right.m_mediaType
            && left.m_byteSize == right.m_byteSize
            && left.m_importStatus == right.m_importStatus;
    }

    inline bool SameReleaseSigningEvidenceRecord(
        const EvidenceRecord& left,
        const EvidenceRecord& right)
    {
        return left.m_evidenceId == right.m_evidenceId
            && left.m_sourceId == right.m_sourceId
            && left.m_sourceFingerprint == right.m_sourceFingerprint
            && left.m_profileId == right.m_profileId
            && left.m_gameVersion == right.m_gameVersion
            && left.m_branch == right.m_branch
            && left.m_subjectRef == right.m_subjectRef
            && left.m_claim == right.m_claim
            && left.m_evidenceKind == right.m_evidenceKind
            && left.m_confidence == right.m_confidence
            && left.m_locator == right.m_locator
            && left.m_recordPath == right.m_recordPath
            && left.m_extractedAt == right.m_extractedAt;
    }

    inline bool SameReleaseSigningSourceDocument(
        const SourceDocument& left,
        const SourceDocument& right)
    {
        if (left.m_schemaVersion != right.m_schemaVersion
            || !SameReleaseSigningSourceRecord(left.m_source, right.m_source)
            || left.m_issues.size() != right.m_issues.size())
        {
            return false;
        }
        for (size_t index = 0; index < left.m_issues.size(); ++index)
        {
            if (!SameReleaseSigningImportIssue(
                    left.m_issues[index],
                    right.m_issues[index]))
            {
                return false;
            }
        }
        return true;
    }

    inline bool SameReleaseSigningEvidenceDocument(
        const EvidenceDocument& left,
        const EvidenceDocument& right)
    {
        if (left.m_schemaVersion != right.m_schemaVersion
            || left.m_sourceId != right.m_sourceId
            || left.m_sourceFingerprint != right.m_sourceFingerprint
            || left.m_profileId != right.m_profileId
            || left.m_gameVersion != right.m_gameVersion
            || left.m_branch != right.m_branch
            || left.m_evidence.size() != right.m_evidence.size()
            || left.m_issues.size() != right.m_issues.size())
        {
            return false;
        }
        for (size_t index = 0; index < left.m_evidence.size(); ++index)
        {
            if (!SameReleaseSigningEvidenceRecord(
                    left.m_evidence[index],
                    right.m_evidence[index]))
            {
                return false;
            }
        }
        for (size_t index = 0; index < left.m_issues.size(); ++index)
        {
            if (!SameReleaseSigningImportIssue(
                    left.m_issues[index],
                    right.m_issues[index]))
            {
                return false;
            }
        }
        return true;
    }

    inline bool SameReleaseSigningAssemblyIssue(
        const AdapterReleaseAssemblyIssue& left,
        const AdapterReleaseAssemblyIssue& right)
    {
        return left.m_code == right.m_code
            && left.m_message == right.m_message
            && left.m_contentId == right.m_contentId
            && left.m_observationId == right.m_observationId
            && left.m_failureId == right.m_failureId
            && left.m_diagnosticId == right.m_diagnosticId;
    }

    inline bool SameReleaseAssemblyEvidenceReturn(
        const AdapterReleaseAssemblyEvidenceReturn& left,
        const AdapterReleaseAssemblyEvidenceReturn& right)
    {
        if (left.m_resultId != right.m_resultId
            || left.m_artifactId != right.m_artifactId
            || left.m_artifactFingerprint != right.m_artifactFingerprint
            || left.m_resultFingerprint != right.m_resultFingerprint
            || left.m_status != right.m_status
            || left.m_checksumObservationCount != right.m_checksumObservationCount
            || left.m_matchedChecksumCount != right.m_matchedChecksumCount
            || left.m_mismatchedChecksumCount != right.m_mismatchedChecksumCount
            || left.m_failedChecksumCount != right.m_failedChecksumCount
            || left.m_inconclusiveChecksumCount != right.m_inconclusiveChecksumCount
            || left.m_notObservedChecksumCount != right.m_notObservedChecksumCount
            || left.m_failureCount != right.m_failureCount
            || left.m_diagnosticReferenceCount != right.m_diagnosticReferenceCount
            || left.m_sourceDocumentCount != right.m_sourceDocumentCount
            || left.m_evidenceRecordCount != right.m_evidenceRecordCount
            || left.m_contractValid != right.m_contractValid
            || left.m_accepted != right.m_accepted
            || left.m_filesRead != right.m_filesRead
            || left.m_filesHashed != right.m_filesHashed
            || left.m_archiveAssembled != right.m_archiveAssembled
            || left.m_signingPerformed != right.m_signingPerformed
            || left.m_uploadPerformed != right.m_uploadPerformed
            || left.m_releasePublished != right.m_releasePublished
            || left.m_launchPerformed != right.m_launchPerformed
            || left.m_adapterCalled != right.m_adapterCalled
            || left.m_deploymentMutated != right.m_deploymentMutated
            || left.m_sourceDocuments.size() != right.m_sourceDocuments.size()
            || left.m_evidenceDocuments.size() != right.m_evidenceDocuments.size()
            || left.m_issues.size() != right.m_issues.size())
        {
            return false;
        }
        for (size_t index = 0; index < left.m_sourceDocuments.size(); ++index)
        {
            if (!SameReleaseSigningSourceDocument(
                    left.m_sourceDocuments[index],
                    right.m_sourceDocuments[index]))
            {
                return false;
            }
        }
        for (size_t index = 0; index < left.m_evidenceDocuments.size(); ++index)
        {
            if (!SameReleaseSigningEvidenceDocument(
                    left.m_evidenceDocuments[index],
                    right.m_evidenceDocuments[index]))
            {
                return false;
            }
        }
        for (size_t index = 0; index < left.m_issues.size(); ++index)
        {
            if (!SameReleaseSigningAssemblyIssue(
                    left.m_issues[index],
                    right.m_issues[index]))
            {
                return false;
            }
        }
        return true;
    }

    inline AdapterReleaseAssemblyEvidenceReturn RebuildReleaseAssemblyEvidence(
        const AdapterReleaseArtifactEnvelope& artifact,
        const AdapterReleaseAssemblyResultEnvelope& assembly)
    {
        return AdapterReleaseAssemblyEvidenceService{}.BuildEvidenceReturn(
            artifact,
            assembly);
    }

    inline bool ReleaseSigningChronologyIsValid(
        const AdapterReleaseAssemblyResultEnvelope& assembly,
        const AdapterReleaseSigningResultEnvelope& signing)
    {
        if (!IsAdapterReleaseSigningUtcTimestamp(
                assembly.m_archive.m_completedAtUtc)
            || !IsAdapterReleaseSigningUtcTimestamp(
                signing.m_signerReview.m_reviewedAtUtc)
            || !IsAdapterReleaseSigningUtcTimestamp(signing.m_capturedAtUtc))
        {
            return false;
        }
        if (!signing.m_completedAtUtc.empty()
            && (assembly.m_archive.m_completedAtUtc > signing.m_completedAtUtc
                || signing.m_signerReview.m_reviewedAtUtc
                    > signing.m_completedAtUtc))
        {
            return false;
        }
        for (const AdapterReleaseSignatureArtifact& artifact :
             signing.m_signatureArtifacts)
        {
            if (!IsAdapterReleaseSigningUtcTimestamp(artifact.m_createdAtUtc)
                || assembly.m_archive.m_completedAtUtc > artifact.m_createdAtUtc
                || signing.m_signerReview.m_reviewedAtUtc
                    > artifact.m_createdAtUtc)
            {
                return false;
            }
        }
        return true;
    }

    inline void AppendReleaseSigningReviewJson(
        AZStd::string& output,
        const AdapterReleaseSignerReview& review)
    {
        using namespace DeterministicContractJson;
        AppendName(output, "SignerReview");
        output.push_back('{');
        AppendString(output, "ReviewId", review.m_reviewId);
        AppendString(output, "SignerToolId", review.m_signerToolId);
        AppendString(output, "SignerToolVersion", review.m_signerToolVersion);
        AppendString(output, "SignerToolFingerprint", review.m_signerToolFingerprint);
        AppendString(output, "Decision", ToString(review.m_decision));
        AppendString(output, "Reviewer", review.m_reviewer);
        AZStd::vector<AZStd::string> capabilities;
        for (AdapterReleaseSignerCapability capability : review.m_capabilities)
        {
            capabilities.push_back(ToString(capability));
        }
        AppendSortedStringArray(output, "Capabilities", AZStd::move(capabilities));
        AppendSortedStringArray(output, "EvidenceIds", review.m_evidenceIds);
        AppendString(output, "ReviewedAtUtc", review.m_reviewedAtUtc);
        AppendString(output, "Notes", review.m_notes, false);
        output.push_back('}');
        output.push_back(',');
    }

    inline AZStd::string BuildReleaseSigningAuthorityCanonicalJson(
        const AdapterReleaseSigningResultEnvelope& envelope)
    {
        using namespace DeterministicContractJson;
        AZStd::string output;
        output.push_back('{');
        AppendUnsigned(output, "ContractVersion", envelope.m_contractVersion);
        AppendString(output, "ResultId", envelope.m_resultId);
        AppendString(output, "ArtifactId", envelope.m_artifactId);
        AppendString(output, "ArtifactFingerprint", envelope.m_artifactFingerprint);
        AppendString(output, "AssemblyResultId", envelope.m_assemblyResultId);
        AppendString(
            output,
            "AssemblyResultFingerprint",
            envelope.m_assemblyResultFingerprint);
        AppendString(output, "ArchiveId", envelope.m_archiveId);
        AppendString(output, "ArchivePath", envelope.m_archivePath);
        AppendString(output, "ArchiveFormat", envelope.m_archiveFormat);
        AppendUnsigned(output, "ArchiveByteSize", envelope.m_archiveByteSize);
        AppendString(output, "ArchiveFingerprint", envelope.m_archiveFingerprint);
        AppendString(output, "ReconciliationId", envelope.m_reconciliationId);
        AppendString(output, "PackagePreviewId", envelope.m_packagePreviewId);
        AppendString(output, "ManifestId", envelope.m_manifestId);
        AppendString(output, "ManifestFingerprint", envelope.m_manifestFingerprint);
        AppendString(output, "PackId", envelope.m_packId);
        AppendString(output, "PackVersion", envelope.m_packVersion);
        AppendString(output, "ProfileId", envelope.m_profileId);
        AppendString(output, "GameVersion", envelope.m_gameVersion);
        AppendString(output, "Branch", envelope.m_branch);
        AppendString(output, "RuntimeTarget", envelope.m_runtimeTarget);
        AppendString(output, "SigningIntentId", envelope.m_signingIntentId);
        AppendString(
            output,
            "SigningIntentDecision",
            ToString(envelope.m_signingIntentDecision));
        AppendString(
            output,
            "SigningIdentityKind",
            ToString(envelope.m_signingIdentityKind));
        AppendString(output, "SignerId", envelope.m_signerId);
        AppendString(output, "IdentityLocator", envelope.m_identityLocator);
        AppendString(output, "IdentityFingerprint", envelope.m_identityFingerprint);
        AppendReleaseSigningReviewJson(output, envelope.m_signerReview);
        AppendBool(output, "Attempted", envelope.m_attempted);
        AppendString(output, "Outcome", ToString(envelope.m_outcome));
        AppendString(output, "CompletedAtUtc", envelope.m_completedAtUtc);
        AppendString(output, "CapturedAtUtc", envelope.m_capturedAtUtc);
        AppendSortedStringArray(output, "FailureIds", envelope.m_failureIds);
        AppendSortedStringArray(
            output,
            "DiagnosticReferenceIds",
            envelope.m_diagnosticReferenceIds);

        AZStd::vector<const AdapterReleaseSignatureArtifact*> artifacts;
        for (const AdapterReleaseSignatureArtifact& artifact :
             envelope.m_signatureArtifacts)
        {
            artifacts.push_back(&artifact);
        }
        AZStd::sort(
            artifacts.begin(),
            artifacts.end(),
            [](const auto* left, const auto* right)
            {
                return left->m_signatureArtifactId
                    < right->m_signatureArtifactId;
            });
        AppendName(output, "SignatureArtifacts");
        output.push_back('[');
        for (size_t index = 0; index < artifacts.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            const AdapterReleaseSignatureArtifact& artifact = *artifacts[index];
            output.push_back('{');
            AppendString(output, "Id", artifact.m_signatureArtifactId);
            AppendString(output, "Kind", ToString(artifact.m_kind));
            AppendString(output, "Reference", artifact.m_reference);
            AppendString(output, "MediaType", artifact.m_mediaType);
            AppendUnsigned(output, "ByteSize", artifact.m_byteSize);
            AppendString(output, "Fingerprint", artifact.m_fingerprint);
            AppendString(output, "CreatedAtUtc", artifact.m_createdAtUtc);
            AppendString(output, "ArchiveId", artifact.m_archiveId);
            AppendString(
                output,
                "ArchiveFingerprint",
                artifact.m_archiveFingerprint);
            AppendSortedStringArray(
                output,
                "DiagnosticReferenceIds",
                artifact.m_diagnosticReferenceIds,
                false);
            output.push_back('}');
        }
        output.push_back(']');
        output.push_back(',');

        AZStd::vector<const AdapterReleaseSigningFailure*> failures;
        for (const AdapterReleaseSigningFailure& failure : envelope.m_failures)
        {
            failures.push_back(&failure);
        }
        AZStd::sort(
            failures.begin(),
            failures.end(),
            [](const auto* left, const auto* right)
            {
                return left->m_failureId < right->m_failureId;
            });
        AppendName(output, "Failures");
        output.push_back('[');
        for (size_t index = 0; index < failures.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            const AdapterReleaseSigningFailure& failure = *failures[index];
            output.push_back('{');
            AppendString(output, "Id", failure.m_failureId);
            AppendString(output, "Kind", ToString(failure.m_kind));
            AppendString(output, "Code", failure.m_code);
            AppendString(output, "Message", failure.m_message);
            AppendString(
                output,
                "SignatureArtifactId",
                failure.m_signatureArtifactId);
            AppendSortedStringArray(
                output,
                "DiagnosticReferenceIds",
                failure.m_diagnosticReferenceIds);
            AppendBool(output, "Retryable", failure.m_retryable, false);
            output.push_back('}');
        }
        output.push_back(']');
        output.push_back(',');

        AZStd::vector<const AdapterReleaseSigningDiagnosticReference*> diagnostics;
        for (const AdapterReleaseSigningDiagnosticReference& diagnostic :
             envelope.m_diagnosticReferences)
        {
            diagnostics.push_back(&diagnostic);
        }
        AZStd::sort(
            diagnostics.begin(),
            diagnostics.end(),
            [](const auto* left, const auto* right)
            {
                return left->m_diagnosticId < right->m_diagnosticId;
            });
        AppendName(output, "DiagnosticReferences");
        output.push_back('[');
        for (size_t index = 0; index < diagnostics.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            const AdapterReleaseSigningDiagnosticReference& diagnostic =
                *diagnostics[index];
            output.push_back('{');
            AppendString(output, "Id", diagnostic.m_diagnosticId);
            AppendString(output, "Kind", ToString(diagnostic.m_kind));
            AppendString(output, "Reference", diagnostic.m_reference);
            AppendString(output, "Fingerprint", diagnostic.m_fingerprint);
            AppendSortedStringArray(
                output,
                "SignatureArtifactIds",
                diagnostic.m_signatureArtifactIds,
                false);
            output.push_back('}');
        }
        output.push_back(']');
        output.push_back('}');
        return output;
    }

    inline AZStd::string CalculateReleaseSigningAuthorityFingerprint(
        const AdapterReleaseSigningResultEnvelope& envelope)
    {
        return CalculateCanonicalSha256(
            BuildReleaseSigningAuthorityCanonicalJson(envelope));
    }
} // namespace TaintedGrailModdingSDK
