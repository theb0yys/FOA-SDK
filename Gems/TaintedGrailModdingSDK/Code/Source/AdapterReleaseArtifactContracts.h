/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterPackageAssemblyPreviewService.h"
#include "AdapterVerifierEvidenceReconciliationService.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    enum class AdapterReleaseArtifactEnvelopeStatus : AZ::u8
    {
        Ready,
        ReconciliationNotApproved,
        PackageNotReady,
        BindingMismatch,
        ContentInvalid,
        ChecksumDeclarationInvalid,
        ProvenanceIncomplete,
        LegalDispositionIncomplete,
        SigningIntentInvalid,
        PublicationTargetInvalid,
    };

    enum class AdapterReleaseChecksumAlgorithm : AZ::u8
    {
        Sha256,
    };

    enum class AdapterReleaseLegalDisposition : AZ::u8
    {
        Unknown,
        Approved,
        Restricted,
        Rejected,
    };

    enum class AdapterReleaseSigningIntentDecision : AZ::u8
    {
        Unknown,
        Unsigned,
        SignExternally,
    };

    enum class AdapterReleaseSigningIdentityKind : AZ::u8
    {
        None,
        Pgp,
        X509,
        Sigstore,
        Platform,
        Other,
    };

    enum class AdapterReleasePublicationTargetKind : AZ::u8
    {
        GitHubRelease,
        ModPortal,
        InternalArchive,
        Other,
    };

    AZStd::string ToString(AdapterReleaseArtifactEnvelopeStatus status);
    AZStd::string ToString(AdapterReleaseChecksumAlgorithm algorithm);
    AZStd::string ToString(AdapterReleaseLegalDisposition disposition);
    AZStd::string ToString(AdapterReleaseSigningIntentDecision decision);
    AZStd::string ToString(AdapterReleaseSigningIdentityKind kind);
    AZStd::string ToString(AdapterReleasePublicationTargetKind kind);

    bool TryParseAdapterReleaseArtifactEnvelopeStatus(
        const AZStd::string& value,
        AdapterReleaseArtifactEnvelopeStatus& status);
    bool TryParseAdapterReleaseChecksumAlgorithm(
        const AZStd::string& value,
        AdapterReleaseChecksumAlgorithm& algorithm);
    bool TryParseAdapterReleaseLegalDisposition(
        const AZStd::string& value,
        AdapterReleaseLegalDisposition& disposition);
    bool TryParseAdapterReleaseSigningIntentDecision(
        const AZStd::string& value,
        AdapterReleaseSigningIntentDecision& decision);
    bool TryParseAdapterReleaseSigningIdentityKind(
        const AZStd::string& value,
        AdapterReleaseSigningIdentityKind& kind);
    bool TryParseAdapterReleasePublicationTargetKind(
        const AZStd::string& value,
        AdapterReleasePublicationTargetKind& kind);

    struct AdapterReleaseArtifactContent
    {
        AZStd::string m_contentId;
        AZStd::string m_packageEntryId;
        AZStd::string m_packagePath;
        AZStd::string m_role;
        AZStd::string m_mediaType;
        AZ::u64 m_byteSize = 0;
        AdapterReleaseChecksumAlgorithm m_checksumAlgorithm =
            AdapterReleaseChecksumAlgorithm::Sha256;
        AZStd::string m_expectedChecksum;
        AZStd::vector<AZStd::string> m_provenanceIds;
        AZStd::string m_legalDispositionId;
    };

    struct AdapterReleaseProvenanceRecord
    {
        AZStd::string m_provenanceId;
        AZStd::string m_contentId;
        AZStd::string m_subjectRef;
        AZStd::string m_sourceKind;
        AZStd::string m_sourceId;
        AZStd::string m_sourceFingerprint;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_capturedAtUtc;
        AZStd::string m_limitations;
    };

    struct AdapterReleaseLegalDispositionRecord
    {
        AZStd::string m_dispositionId;
        AZStd::string m_contentId;
        AdapterReleaseLegalDisposition m_disposition =
            AdapterReleaseLegalDisposition::Unknown;
        AZStd::string m_reviewer;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_reviewedAtUtc;
        AZStd::string m_rationale;
    };

    struct AdapterReleaseSigningIntent
    {
        AZStd::string m_intentId;
        AdapterReleaseSigningIntentDecision m_decision =
            AdapterReleaseSigningIntentDecision::Unknown;
        AdapterReleaseSigningIdentityKind m_identityKind =
            AdapterReleaseSigningIdentityKind::None;
        AZStd::string m_signerId;
        AZStd::string m_identityLocator;
        AZStd::string m_identityFingerprint;
        AZStd::string m_reviewer;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_reviewedAtUtc;
        AZStd::string m_rationale;
    };

    struct AdapterReleasePublicationTarget
    {
        AZStd::string m_targetId;
        AdapterReleasePublicationTargetKind m_kind =
            AdapterReleasePublicationTargetKind::Other;
        AZStd::string m_locator;
        AZStd::string m_channel;
        AZStd::string m_reviewer;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_reviewedAtUtc;
        AZStd::string m_rationale;
    };

    struct AdapterReleaseArtifactRequest
    {
        AZStd::string m_artifactId;
        AZStd::string m_evaluatedAtUtc;
        AZStd::string m_reconciliationCanonicalJson;
        AZStd::string m_packagePreviewCanonicalJson;
        AZStd::vector<AdapterReleaseArtifactContent> m_contents;
        AZStd::vector<AdapterReleaseProvenanceRecord> m_provenance;
        AZStd::vector<AdapterReleaseLegalDispositionRecord> m_legalDispositions;
        AdapterReleaseSigningIntent m_signingIntent;
        AZStd::vector<AdapterReleasePublicationTarget> m_publicationTargets;
    };

    struct AdapterReleaseArtifactBlocker
    {
        AZStd::string m_blockerId;
        AZStd::string m_code;
        AZStd::string m_subjectRef;
        AZStd::string m_message;
    };

    struct AdapterReleaseArtifactEnvelope
    {
        AZ::u32 m_contractVersion = 1;
        AZStd::string m_artifactId;
        AZStd::string m_reconciliationId;
        AZStd::string m_reconciliationCanonicalJson;
        AZStd::string m_reportId;
        AZStd::string m_verifierResultId;
        AZStd::string m_workOrderFingerprint;
        AZStd::string m_executionResultFingerprint;
        AZStd::string m_verifierResultFingerprint;
        AZStd::string m_packagePreviewId;
        AZStd::string m_packagePreviewCanonicalJson;
        AZStd::string m_manifestId;
        AZStd::string m_manifestFingerprint;
        AZStd::string m_packId;
        AZStd::string m_packVersion;
        AZStd::string m_adapterId;
        AZStd::string m_adapterVersion;
        AZStd::string m_inventoryId;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_evaluatedAtUtc;
        AdapterReleaseArtifactEnvelopeStatus m_status =
            AdapterReleaseArtifactEnvelopeStatus::ReconciliationNotApproved;
        AZStd::vector<AdapterReleaseArtifactContent> m_contents;
        AZStd::vector<AdapterReleaseProvenanceRecord> m_provenance;
        AZStd::vector<AdapterReleaseLegalDispositionRecord> m_legalDispositions;
        AdapterReleaseSigningIntent m_signingIntent;
        AZStd::vector<AdapterReleasePublicationTarget> m_publicationTargets;
        AZStd::vector<AdapterReleaseArtifactBlocker> m_blockers;
        AZ::u64 m_contentCount = 0;
        AZ::u64 m_provenanceCount = 0;
        AZ::u64 m_legalDispositionCount = 0;
        AZ::u64 m_publicationTargetCount = 0;
        AZStd::string m_canonicalJson;
        bool m_metadataReady = false;
        bool m_humanReviewRequired = true;
        bool m_filesRead = false;
        bool m_filesHashed = false;
        bool m_checksumGenerated = false;
        bool m_filesCopied = false;
        bool m_archiveAssembled = false;
        bool m_signingPerformed = false;
        bool m_uploadPerformed = false;
        bool m_releasePublished = false;
        bool m_launchPerformed = false;
        bool m_adapterCalled = false;
        bool m_deploymentMutated = false;
    };

    class AdapterReleaseArtifactRegistry
    {
    public:
        static AdapterReleaseArtifactRegistry& Get();

        bool RegisterRequest(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation,
            const AdapterPackageAssemblyPreview& packagePreview,
            const SourceEvidenceRegistry& sourceRegistry,
            const AdapterReleaseArtifactRequest& request,
            AZStd::string* error = nullptr);
        void Clear();

        const AZStd::vector<AdapterReleaseArtifactEnvelope>& GetEnvelopes() const;

    private:
        AZStd::vector<AdapterReleaseArtifactEnvelope> m_envelopes;
    };
} // namespace TaintedGrailModdingSDK
