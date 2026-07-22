/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "GameInformationAcquisition.h"

#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK::GameInformationAcquisition
{
    namespace
    {
        constexpr const char* PinnedTaintedGrailCommit =
            "d7e740e7f167b73152b53409e483dab07d80d048";

        void SetError(AZStd::string* error, AZStd::string message)
        {
            if (error)
            {
                *error = AZStd::move(message);
            }
        }

        bool IsBoundedText(
            const AZStd::string& value,
            size_t maximumLength,
            bool allowEmpty = false)
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
            return true;
        }

        bool IsGitCommit(const AZStd::string& value)
        {
            if (value.size() != 40)
            {
                return false;
            }
            for (char character : value)
            {
                if (!((character >= '0' && character <= '9')
                    || (character >= 'a' && character <= 'f')))
                {
                    return false;
                }
            }
            return true;
        }

        AZStd::vector<ProviderDescriptor> BuildProviders()
        {
            ProviderDescriptor local;
            local.m_providerId = "provider.foa-local-capture";
            local.m_displayName = "Lawful Local FoA Capture";
            local.m_contractVersion = "1.0.0";
            local.m_kind = ProviderKind::LocalInstall;
            local.m_qualification = QualificationState::ContractOnly;
            local.m_licenseExpression = "NOASSERTION";
            local.m_precedence = 0;
            local.m_requiresLocalFileRead = true;

            ProviderDescriptor github;
            github.m_providerId = "provider.tainted-grail-github";
            github.m_displayName = "Pinned Tainted Grail GitHub Snapshot";
            github.m_contractVersion = "1.0.0";
            github.m_kind = ProviderKind::PinnedGitHub;
            github.m_qualification = QualificationState::SourcePinned;
            github.m_sourceRepository =
                "theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods";
            github.m_sourceRevision = PinnedTaintedGrailCommit;
            github.m_licenseExpression = "NOASSERTION";
            github.m_precedence = 1;
            github.m_requiresNetwork = true;

            ProviderDescriptor merlin;
            merlin.m_providerId = "provider.merlin-workshop";
            merlin.m_displayName = "Optional Merlin Workshop";
            merlin.m_contractVersion = "1.0.0";
            merlin.m_kind = ProviderKind::MerlinWorkshop;
            merlin.m_qualification = QualificationState::ContractOnly;
            merlin.m_sourceRepository = "AR-Questline/merlin-workshop";
            merlin.m_licenseExpression = "NOASSERTION";
            merlin.m_precedence = 2;
            merlin.m_optional = true;
            merlin.m_requiresNetwork = true;

            return { AZStd::move(local), AZStd::move(github), AZStd::move(merlin) };
        }

        void AddUnique(AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            if (AZStd::find(values.begin(), values.end(), value) == values.end())
            {
                values.push_back(value);
            }
        }
    } // namespace

    const AZStd::vector<ProviderDescriptor>& GetCanonicalProviders()
    {
        static const AZStd::vector<ProviderDescriptor> providers = BuildProviders();
        return providers;
    }

    const ProviderDescriptor* FindProvider(const AZStd::string& providerId)
    {
        const auto& providers = GetCanonicalProviders();
        const auto found = AZStd::find_if(
            providers.begin(), providers.end(),
            [&providerId](const ProviderDescriptor& provider)
            {
                return provider.m_providerId == providerId;
            });
        return found == providers.end() ? nullptr : &*found;
    }

    bool ValidateProvider(const ProviderDescriptor& provider, AZStd::string* error)
    {
        if (!IsStableContractId(provider.m_providerId)
            || !IsBoundedText(provider.m_displayName, 256)
            || !IsStrictSemanticVersion(provider.m_contractVersion)
            || !IsBoundedText(provider.m_licenseExpression, 128)
            || provider.m_precedence < 0 || provider.m_precedence > 32
            || provider.m_autoPromotionAllowed)
        {
            SetError(error, "Acquisition provider identity, version, precedence, or authority is invalid.");
            return false;
        }
        if (provider.m_qualification == QualificationState::SourcePinned
            && (!IsBoundedText(provider.m_sourceRepository, 512)
                || !IsGitCommit(provider.m_sourceRevision)))
        {
            SetError(error, "Source-pinned providers require an exact repository and 40-character commit.");
            return false;
        }
        if (provider.m_kind == ProviderKind::LocalInstall
            && (!provider.m_requiresLocalFileRead || provider.m_requiresNetwork))
        {
            SetError(error, "Local capture must remain a local-file provider without network authority.");
            return false;
        }
        if (provider.m_kind == ProviderKind::MerlinWorkshop && !provider.m_optional)
        {
            SetError(error, "Merlin Workshop must remain optional.");
            return false;
        }
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool ValidateObservation(
        const CandidateObservation& observation,
        const GameProfile& profile,
        AZStd::string* error)
    {
        const ProviderDescriptor* provider = FindProvider(observation.m_providerId);
        AZStd::string providerError;
        if (!provider || !ValidateProvider(*provider, &providerError))
        {
            SetError(error, "Candidate observation references an unknown or invalid provider.");
            return false;
        }
        if (provider->m_qualification == QualificationState::ContractOnly)
        {
            SetError(error, "Candidate observation provider is contract-only and not qualified for intake.");
            return false;
        }
        if (!profile.IsConfigured()
            || observation.m_profileId != profile.m_profileId
            || observation.m_gameVersion != profile.m_gameVersion
            || observation.m_branch != profile.m_branch
            || observation.m_runtimeTarget != profile.m_runtimeTarget)
        {
            SetError(error, "Candidate observation must match the exact configured active profile.");
            return false;
        }
        if (!IsStableContractId(observation.m_observationId)
            || !IsStrictSemanticVersion(observation.m_providerVersion)
            || observation.m_providerVersion != provider->m_contractVersion
            || !IsSafePersistenceId(observation.m_sourceId)
            || !IsSha256Fingerprint(observation.m_sourceFingerprint)
            || !IsSha256Fingerprint(observation.m_valueFingerprint)
            || !IsBoundedText(observation.m_subjectRef, 1024)
            || !IsBoundedText(observation.m_claim, 8192)
            || !IsBoundedText(observation.m_evidenceKind, 128)
            || !IsBoundedText(observation.m_confidence, 64)
            || !IsBoundedText(observation.m_locator, 2048)
            || !IsBoundedText(observation.m_recordPath, 2048)
            || !IsStrictUtcTimestamp(observation.m_capturedAtUtc)
            || observation.m_promoteAutomatically
            || observation.m_grantsRuntimePermission)
        {
            SetError(error, "Candidate observation is incomplete, unbounded, or attempts to escalate authority.");
            return false;
        }
        if (provider->m_qualification == QualificationState::SourcePinned
            && observation.m_sourceRevision != provider->m_sourceRevision)
        {
            SetError(error, "Candidate observation does not match the provider's exact pinned source revision.");
            return false;
        }
        if (error)
        {
            error->clear();
        }
        return true;
    }

    ReconciliationResult Reconcile(
        const AZStd::vector<CandidateObservation>& observations,
        const GameProfile& profile)
    {
        ReconciliationResult result;
        result.m_canPromoteAutomatically = false;
        result.m_grantsRuntimePermission = false;
        for (const CandidateObservation& observation : observations)
        {
            if (!ValidateObservation(observation, profile))
            {
                AddUnique(result.m_rejectedObservationIds, observation.m_observationId);
                continue;
            }
            const auto duplicate = AZStd::find_if(
                result.m_candidates.begin(), result.m_candidates.end(),
                [&observation](const CandidateObservation& candidate)
                {
                    return candidate.m_observationId == observation.m_observationId;
                });
            if (duplicate != result.m_candidates.end())
            {
                AddUnique(result.m_rejectedObservationIds, observation.m_observationId);
                continue;
            }
            result.m_candidates.push_back(observation);
        }

        AZStd::sort(
            result.m_candidates.begin(), result.m_candidates.end(),
            [](const CandidateObservation& left, const CandidateObservation& right)
            {
                if (left.m_subjectRef != right.m_subjectRef)
                {
                    return left.m_subjectRef < right.m_subjectRef;
                }
                const ProviderDescriptor* leftProvider = FindProvider(left.m_providerId);
                const ProviderDescriptor* rightProvider = FindProvider(right.m_providerId);
                const int leftPrecedence = leftProvider ? leftProvider->m_precedence : 32;
                const int rightPrecedence = rightProvider ? rightProvider->m_precedence : 32;
                return leftPrecedence != rightPrecedence
                    ? leftPrecedence < rightPrecedence
                    : left.m_observationId < right.m_observationId;
            });

        size_t begin = 0;
        while (begin < result.m_candidates.size())
        {
            size_t end = begin + 1;
            while (end < result.m_candidates.size()
                && result.m_candidates[end].m_subjectRef
                    == result.m_candidates[begin].m_subjectRef)
            {
                ++end;
            }
            AZStd::vector<AZStd::string> fingerprints;
            for (size_t index = begin; index < end; ++index)
            {
                AddUnique(fingerprints, result.m_candidates[index].m_valueFingerprint);
            }
            if (fingerprints.size() > 1)
            {
                Conflict conflict;
                conflict.m_subjectRef = result.m_candidates[begin].m_subjectRef;
                conflict.m_valueFingerprints = AZStd::move(fingerprints);
                conflict.m_reason =
                    "Providers disagree; precedence does not silently override a conflicting observation.";
                for (size_t index = begin; index < end; ++index)
                {
                    conflict.m_observationIds.push_back(
                        result.m_candidates[index].m_observationId);
                }
                AZStd::sort(
                    conflict.m_observationIds.begin(), conflict.m_observationIds.end());
                AZStd::sort(
                    conflict.m_valueFingerprints.begin(), conflict.m_valueFingerprints.end());
                result.m_conflicts.push_back(AZStd::move(conflict));
            }
            begin = end;
        }

        AZStd::sort(
            result.m_rejectedObservationIds.begin(),
            result.m_rejectedObservationIds.end());
        return result;
    }

    bool BuildEvidenceCandidate(
        const CandidateObservation& observation,
        const GameProfile& profile,
        EvidenceRecord& evidence,
        AZStd::string* error)
    {
        if (!ValidateObservation(observation, profile, error))
        {
            return false;
        }
        evidence = {};
        evidence.m_evidenceId = observation.m_observationId;
        evidence.m_sourceId = observation.m_sourceId;
        evidence.m_sourceFingerprint = observation.m_sourceFingerprint;
        evidence.m_profileId = observation.m_profileId;
        evidence.m_gameVersion = observation.m_gameVersion;
        evidence.m_branch = observation.m_branch;
        evidence.m_subjectRef = observation.m_subjectRef;
        evidence.m_claim = observation.m_claim;
        evidence.m_evidenceKind = observation.m_evidenceKind;
        evidence.m_confidence = observation.m_confidence;
        evidence.m_locator = observation.m_locator;
        evidence.m_recordPath = observation.m_recordPath;
        evidence.m_extractedAt = observation.m_capturedAtUtc;
        if (error)
        {
            error->clear();
        }
        return true;
    }
} // namespace TaintedGrailModdingSDK::GameInformationAcquisition
