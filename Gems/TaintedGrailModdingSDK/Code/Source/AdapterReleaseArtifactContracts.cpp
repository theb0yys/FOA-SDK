/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseArtifactContracts.h"

#include "AdapterReleaseArtifactProvenanceService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        template<class EnumType>
        struct EnumName
        {
            EnumType m_value;
            const char* m_name;
        };

        constexpr EnumName<AdapterReleaseArtifactEnvelopeStatus> EnvelopeStatuses[] = {
            { AdapterReleaseArtifactEnvelopeStatus::Ready, "ready" },
            { AdapterReleaseArtifactEnvelopeStatus::ReconciliationNotApproved,
                "reconciliation_not_approved" },
            { AdapterReleaseArtifactEnvelopeStatus::PackageNotReady,
                "package_not_ready" },
            { AdapterReleaseArtifactEnvelopeStatus::BindingMismatch,
                "binding_mismatch" },
            { AdapterReleaseArtifactEnvelopeStatus::ContentInvalid,
                "content_invalid" },
            { AdapterReleaseArtifactEnvelopeStatus::ChecksumDeclarationInvalid,
                "checksum_declaration_invalid" },
            { AdapterReleaseArtifactEnvelopeStatus::ProvenanceIncomplete,
                "provenance_incomplete" },
            { AdapterReleaseArtifactEnvelopeStatus::LegalDispositionIncomplete,
                "legal_disposition_incomplete" },
            { AdapterReleaseArtifactEnvelopeStatus::SigningIntentInvalid,
                "signing_intent_invalid" },
            { AdapterReleaseArtifactEnvelopeStatus::PublicationTargetInvalid,
                "publication_target_invalid" },
        };

        constexpr EnumName<AdapterReleaseChecksumAlgorithm> ChecksumAlgorithms[] = {
            { AdapterReleaseChecksumAlgorithm::Sha256, "sha256" },
        };

        constexpr EnumName<AdapterReleaseLegalDisposition> LegalDispositions[] = {
            { AdapterReleaseLegalDisposition::Unknown, "unknown" },
            { AdapterReleaseLegalDisposition::Approved, "approved" },
            { AdapterReleaseLegalDisposition::Restricted, "restricted" },
            { AdapterReleaseLegalDisposition::Rejected, "rejected" },
        };

        constexpr EnumName<AdapterReleaseSigningIntentDecision> SigningDecisions[] = {
            { AdapterReleaseSigningIntentDecision::Unknown, "unknown" },
            { AdapterReleaseSigningIntentDecision::Unsigned, "unsigned" },
            { AdapterReleaseSigningIntentDecision::SignExternally,
                "sign_externally" },
        };

        constexpr EnumName<AdapterReleaseSigningIdentityKind> SigningIdentityKinds[] = {
            { AdapterReleaseSigningIdentityKind::None, "none" },
            { AdapterReleaseSigningIdentityKind::Pgp, "pgp" },
            { AdapterReleaseSigningIdentityKind::X509, "x509" },
            { AdapterReleaseSigningIdentityKind::Sigstore, "sigstore" },
            { AdapterReleaseSigningIdentityKind::Platform, "platform" },
            { AdapterReleaseSigningIdentityKind::Other, "other" },
        };

        constexpr EnumName<AdapterReleasePublicationTargetKind> PublicationKinds[] = {
            { AdapterReleasePublicationTargetKind::GitHubRelease,
                "github_release" },
            { AdapterReleasePublicationTargetKind::ModPortal, "mod_portal" },
            { AdapterReleasePublicationTargetKind::InternalArchive,
                "internal_archive" },
            { AdapterReleasePublicationTargetKind::Other, "other" },
        };

        template<class EnumType, size_t Count>
        AZStd::string EnumToString(
            EnumType value,
            const EnumName<EnumType> (&names)[Count])
        {
            for (const EnumName<EnumType>& name : names)
            {
                if (name.m_value == value)
                {
                    return name.m_name;
                }
            }
            return "unknown";
        }

        template<class EnumType, size_t Count>
        bool TryParseEnum(
            const AZStd::string& value,
            EnumType& result,
            const EnumName<EnumType> (&names)[Count])
        {
            for (const EnumName<EnumType>& name : names)
            {
                if (value == name.m_name)
                {
                    result = name.m_value;
                    return true;
                }
            }
            return false;
        }

        void SetError(AZStd::string* error, AZStd::string value)
        {
            if (error)
            {
                *error = AZStd::move(value);
            }
        }

        bool HasOperationalMutation(const AdapterReleaseArtifactEnvelope& envelope)
        {
            return envelope.m_filesRead
                || envelope.m_filesHashed
                || envelope.m_checksumGenerated
                || envelope.m_filesCopied
                || envelope.m_archiveAssembled
                || envelope.m_signingPerformed
                || envelope.m_uploadPerformed
                || envelope.m_releasePublished
                || envelope.m_launchPerformed
                || envelope.m_adapterCalled
                || envelope.m_deploymentMutated;
        }
    } // namespace

    AZStd::string ToString(AdapterReleaseArtifactEnvelopeStatus status)
    {
        return EnumToString(status, EnvelopeStatuses);
    }

    AZStd::string ToString(AdapterReleaseChecksumAlgorithm algorithm)
    {
        return EnumToString(algorithm, ChecksumAlgorithms);
    }

    AZStd::string ToString(AdapterReleaseLegalDisposition disposition)
    {
        return EnumToString(disposition, LegalDispositions);
    }

    AZStd::string ToString(AdapterReleaseSigningIntentDecision decision)
    {
        return EnumToString(decision, SigningDecisions);
    }

    AZStd::string ToString(AdapterReleaseSigningIdentityKind kind)
    {
        return EnumToString(kind, SigningIdentityKinds);
    }

    AZStd::string ToString(AdapterReleasePublicationTargetKind kind)
    {
        return EnumToString(kind, PublicationKinds);
    }

    bool TryParseAdapterReleaseArtifactEnvelopeStatus(
        const AZStd::string& value,
        AdapterReleaseArtifactEnvelopeStatus& status)
    {
        return TryParseEnum(value, status, EnvelopeStatuses);
    }

    bool TryParseAdapterReleaseChecksumAlgorithm(
        const AZStd::string& value,
        AdapterReleaseChecksumAlgorithm& algorithm)
    {
        return TryParseEnum(value, algorithm, ChecksumAlgorithms);
    }

    bool TryParseAdapterReleaseLegalDisposition(
        const AZStd::string& value,
        AdapterReleaseLegalDisposition& disposition)
    {
        return TryParseEnum(value, disposition, LegalDispositions);
    }

    bool TryParseAdapterReleaseSigningIntentDecision(
        const AZStd::string& value,
        AdapterReleaseSigningIntentDecision& decision)
    {
        return TryParseEnum(value, decision, SigningDecisions);
    }

    bool TryParseAdapterReleaseSigningIdentityKind(
        const AZStd::string& value,
        AdapterReleaseSigningIdentityKind& kind)
    {
        return TryParseEnum(value, kind, SigningIdentityKinds);
    }

    bool TryParseAdapterReleasePublicationTargetKind(
        const AZStd::string& value,
        AdapterReleasePublicationTargetKind& kind)
    {
        return TryParseEnum(value, kind, PublicationKinds);
    }

    AdapterReleaseArtifactRegistry& AdapterReleaseArtifactRegistry::Get()
    {
        static AdapterReleaseArtifactRegistry registry;
        return registry;
    }

    bool AdapterReleaseArtifactRegistry::RegisterEnvelope(
        const AdapterReleaseArtifactEnvelope& envelope,
        AZStd::string* error)
    {
        AdapterReleaseArtifactProvenanceService service;
        if (!IsAdapterPostDeploymentVerifierStableId(envelope.m_artifactId)
            || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                envelope.m_evaluatedAtUtc)
            || envelope.m_status != AdapterReleaseArtifactEnvelopeStatus::Ready
            || !envelope.m_metadataReady
            || !envelope.m_humanReviewRequired
            || !envelope.m_blockers.empty()
            || envelope.m_contents.empty()
            || envelope.m_publicationTargets.empty()
            || envelope.m_contentCount
                != static_cast<AZ::u64>(envelope.m_contents.size())
            || envelope.m_provenanceCount
                != static_cast<AZ::u64>(envelope.m_provenance.size())
            || envelope.m_legalDispositionCount
                != static_cast<AZ::u64>(envelope.m_legalDispositions.size())
            || envelope.m_publicationTargetCount
                != static_cast<AZ::u64>(envelope.m_publicationTargets.size())
            || HasOperationalMutation(envelope)
            || envelope.m_canonicalJson.empty()
            || envelope.m_canonicalJson
                != service.SerializeCanonicalEnvelope(envelope))
        {
            SetError(
                error,
                "Release-artifact session registration requires one self-canonical, Ready, metadata-complete, blocker-free, operation-free envelope with exact counts and reviewed publication targets.");
            return false;
        }

        for (const AdapterReleaseArtifactEnvelope& existing : m_envelopes)
        {
            if (existing.m_artifactId == envelope.m_artifactId)
            {
                SetError(
                    error,
                    "A release-artifact envelope with this identity already exists.");
                return false;
            }
        }

        m_envelopes.push_back(envelope);
        AZStd::sort(
            m_envelopes.begin(),
            m_envelopes.end(),
            [](const AdapterReleaseArtifactEnvelope& left,
                const AdapterReleaseArtifactEnvelope& right)
            {
                return left.m_artifactId < right.m_artifactId;
            });
        if (error)
        {
            error->clear();
        }
        return true;
    }

    void AdapterReleaseArtifactRegistry::Clear()
    {
        m_envelopes.clear();
    }

    const AZStd::vector<AdapterReleaseArtifactEnvelope>&
    AdapterReleaseArtifactRegistry::GetEnvelopes() const
    {
        return m_envelopes;
    }
} // namespace TaintedGrailModdingSDK
