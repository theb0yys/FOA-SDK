/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    enum class AdapterReleaseAssemblerReviewDecision : AZ::u8
    {
        Unknown,
        Accepted,
        Rejected,
    };

    enum class AdapterReleaseAssemblerCapability : AZ::u8
    {
        ContentChecksum,
        ArchiveAssembly,
        ArchiveChecksum,
    };

    enum class AdapterReleaseAssemblyOutcome : AZ::u8
    {
        NotAttempted,
        Succeeded,
        Failed,
        Skipped,
    };

    enum class AdapterReleaseChecksumObservationOutcome : AZ::u8
    {
        NotObserved,
        Matched,
        Mismatched,
        Failed,
        Inconclusive,
    };

    enum class AdapterReleaseAssemblyFailureKind : AZ::u8
    {
        Contract,
        InputBinding,
        ContentUnavailable,
        AccessDenied,
        UnsupportedArchive,
        Read,
        Hash,
        Archive,
        Internal,
        Unknown,
    };

    enum class AdapterReleaseAssemblyDiagnosticKind : AZ::u8
    {
        Assembler,
        Filesystem,
        Hash,
        Archive,
        Binding,
        Diagnostic,
    };

    enum class AdapterReleaseAssemblyEnvelopeStatus : AZ::u8
    {
        Accepted,
        ArtifactNotReady,
        AssemblerUnreviewed,
        ArtifactBindingMismatch,
        EnvelopeInvalid,
        ContentCoverageIncomplete,
        ChecksumObservationMismatch,
        ArchiveBindingMismatch,
        FailureDiagnosticBindingMismatch,
    };

    AZStd::string ToString(AdapterReleaseAssemblerReviewDecision decision);
    AZStd::string ToString(AdapterReleaseAssemblerCapability capability);
    AZStd::string ToString(AdapterReleaseAssemblyOutcome outcome);
    AZStd::string ToString(AdapterReleaseChecksumObservationOutcome outcome);
    AZStd::string ToString(AdapterReleaseAssemblyFailureKind kind);
    AZStd::string ToString(AdapterReleaseAssemblyDiagnosticKind kind);
    AZStd::string ToString(AdapterReleaseAssemblyEnvelopeStatus status);

    bool TryParseAdapterReleaseAssemblerReviewDecision(
        const AZStd::string& value,
        AdapterReleaseAssemblerReviewDecision& decision);
    bool TryParseAdapterReleaseAssemblerCapability(
        const AZStd::string& value,
        AdapterReleaseAssemblerCapability& capability);
    bool TryParseAdapterReleaseAssemblyOutcome(
        const AZStd::string& value,
        AdapterReleaseAssemblyOutcome& outcome);
    bool TryParseAdapterReleaseChecksumObservationOutcome(
        const AZStd::string& value,
        AdapterReleaseChecksumObservationOutcome& outcome);
    bool TryParseAdapterReleaseAssemblyFailureKind(
        const AZStd::string& value,
        AdapterReleaseAssemblyFailureKind& kind);
    bool TryParseAdapterReleaseAssemblyDiagnosticKind(
        const AZStd::string& value,
        AdapterReleaseAssemblyDiagnosticKind& kind);
    bool TryParseAdapterReleaseAssemblyEnvelopeStatus(
        const AZStd::string& value,
        AdapterReleaseAssemblyEnvelopeStatus& status);

    bool IsAdapterReleaseAssemblyStableId(const AZStd::string& value);
    bool IsAdapterReleaseAssemblyFingerprint(const AZStd::string& value);
    bool IsAdapterReleaseAssemblySafeReference(const AZStd::string& value);
    bool IsAdapterReleaseAssemblyUtcTimestamp(const AZStd::string& value);

    struct AdapterReleaseAssemblerReview
    {
        AZStd::string m_reviewId;
        AZStd::string m_assemblerId;
        AZStd::string m_assemblerVersion;
        AZStd::string m_assemblerFingerprint;
        AdapterReleaseAssemblerReviewDecision m_decision =
            AdapterReleaseAssemblerReviewDecision::Unknown;
        AZStd::string m_reviewer;
        AZStd::vector<AdapterReleaseAssemblerCapability> m_capabilities;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_reviewedAtUtc;
        AZStd::string m_notes;
    };

    struct AdapterReleaseAssemblyFailure
    {
        AZStd::string m_failureId;
        AdapterReleaseAssemblyFailureKind m_kind =
            AdapterReleaseAssemblyFailureKind::Unknown;
        AZStd::string m_code;
        AZStd::string m_message;
        AZStd::string m_contentId;
        AZStd::vector<AZStd::string> m_diagnosticReferenceIds;
        bool m_retryable = false;
    };

    struct AdapterReleaseAssemblyDiagnosticReference
    {
        AZStd::string m_diagnosticId;
        AdapterReleaseAssemblyDiagnosticKind m_kind =
            AdapterReleaseAssemblyDiagnosticKind::Diagnostic;
        AZStd::string m_reference;
        AZStd::string m_fingerprint;
        AZStd::vector<AZStd::string> m_contentIds;
    };

    struct AdapterReleaseChecksumObservation
    {
        AZStd::string m_observationId;
        AZStd::string m_contentId;
        AZStd::string m_packagePath;
        AZStd::string m_expectedChecksum;
        AZStd::string m_observedChecksum;
        bool m_attempted = false;
        bool m_observationRecorded = false;
        AdapterReleaseChecksumObservationOutcome m_outcome =
            AdapterReleaseChecksumObservationOutcome::NotObserved;
        AZStd::string m_observedAtUtc;
        AZStd::vector<AZStd::string> m_failureIds;
        AZStd::vector<AZStd::string> m_diagnosticReferenceIds;
    };

    struct AdapterReleaseArchiveResult
    {
        AZStd::string m_archiveId;
        AZStd::string m_archivePath;
        AZStd::string m_archiveFormat;
        AdapterReleaseAssemblyOutcome m_outcome =
            AdapterReleaseAssemblyOutcome::NotAttempted;
        bool m_attempted = false;
        bool m_archivePresent = false;
        AZ::u64 m_byteSize = 0;
        AZStd::string m_archiveFingerprint;
        AZStd::string m_completedAtUtc;
        AZStd::vector<AZStd::string> m_failureIds;
        AZStd::vector<AZStd::string> m_diagnosticReferenceIds;
    };

    struct AdapterReleaseAssemblyResultEnvelope
    {
        AZ::u32 m_contractVersion = 1;
        AZStd::string m_resultId;
        AZStd::string m_artifactId;
        AZStd::string m_artifactCanonicalJson;
        AZStd::string m_artifactFingerprint;
        AZStd::string m_reconciliationId;
        AZStd::string m_packagePreviewId;
        AZStd::string m_manifestId;
        AZStd::string m_manifestFingerprint;
        AZStd::string m_packId;
        AZStd::string m_packVersion;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AdapterReleaseAssemblerReview m_assemblerReview;
        AZStd::string m_capturedAtUtc;
        AZStd::string m_resultFingerprint;
        AdapterReleaseArchiveResult m_archive;
        AZStd::vector<AdapterReleaseChecksumObservation> m_checksumObservations;
        AZStd::vector<AdapterReleaseAssemblyFailure> m_failures;
        AZStd::vector<AdapterReleaseAssemblyDiagnosticReference>
            m_diagnosticReferences;
    };

    class AdapterReleaseAssemblyResultRegistry
    {
    public:
        static AdapterReleaseAssemblyResultRegistry& Get();

        bool RegisterEnvelope(
            const AdapterReleaseAssemblyResultEnvelope& envelope,
            AZStd::string* error = nullptr);
        void Clear();

        const AdapterReleaseAssemblyResultEnvelope* FindByResultId(
            const AZStd::string& resultId) const;
        const AZStd::vector<AdapterReleaseAssemblyResultEnvelope>&
            GetEnvelopes() const;

    private:
        AZStd::vector<AdapterReleaseAssemblyResultEnvelope> m_envelopes;
    };
} // namespace TaintedGrailModdingSDK
