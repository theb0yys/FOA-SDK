/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkExecutionEvidenceProjection.h"
#include "../CanonicalFingerprint.h"
#include "../ResearchContractValidation.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    AZStd::string ProfileFingerprint(const GameProfile& profile)
    {
        AZStd::string bytes = "foa-framework-host-profile-v1";
        auto append = [&](const AZStd::string& value)
        {
            bytes += AZStd::string::format("/%llu:", static_cast<unsigned long long>(value.size())) + value;
        };
        append(profile.m_profileId);
        append(profile.m_gameVersion);
        append(profile.m_branch);
        append(profile.m_runtimeTarget);
        append(profile.m_unityVersion);
        append(profile.m_bepInExVersion);
        // Machine locators contribute only to a private host snapshot hash, never persisted plaintext.
        append(profile.m_installPath);
        append(profile.m_managedAssembliesPath);
        append(profile.m_pluginPath);
        auto scopes = profile.m_dlcScopes;
        AZStd::sort(scopes.begin(), scopes.end());
        for (const auto& scope : scopes)
        {
            append(scope);
        }
        return CalculateCanonicalSha256(bytes);
    }
    Result ProjectCandidate(const StoredAttempt& attempt, const GameProfile& profile, CandidateProjection& output)
    {
        if (!attempt.m_receipt || attempt.m_quarantined || !ValidateStoredPlan(attempt.m_plan))
        {
            return { Error::Quarantined };
        }
        CE::CapabilityExecutionRequestV1 request;
        if (!Decode(attempt.m_plan.m_request.m_canonicalJson, request) || request.m_profileFingerprint != ProfileFingerprint(profile))
        {
            return { Error::Drifted };
        }
        auto bounded = [](const AZStd::string& value)
        {
            return !value.empty() && value.size() <= 128 &&
                AZStd::all_of(
                    value.begin(),
                    value.end(),
                    [](unsigned char c)
                    {
                        return c >= 0x20 && c != 0x7f;
                    });
        };
        if (!IsStableContractId(profile.m_profileId) || !bounded(profile.m_gameVersion) || !bounded(profile.m_branch) ||
            !IsSupportedRuntimeTarget(profile.m_runtimeTarget) || attempt.m_receipt->m_id != attempt.m_executionId ||
            !IsSafePersistenceId("source." + attempt.m_executionId) || !IsStrictUtcTimestamp(attempt.m_receipt->m_startedAt) ||
            !IsStrictUtcTimestamp(attempt.m_receipt->m_finishedAt))
        {
            return { Error::Invalid };
        }
        AZStd::vector<CE::PhaseExtensionReferenceV1> extensions;
        for (const auto& phase : attempt.m_receipt->m_phaseReceipts)
        {
            if (phase.m_extension)
            {
                extensions.push_back(*phase.m_extension);
            }
        }
        if (!CE::Validate(*attempt.m_receipt, attempt.m_plan, attempt.m_receipt->m_authorization, extensions).IsSuccess())
        {
            return { Error::Invalid };
        }
        CandidateProjection result;
        result.m_source.m_sourceId = "source." + attempt.m_executionId;
        result.m_source.m_title = "Framework execution observation";
        result.m_source.m_sourceKind = "execution-receipt";
        result.m_source.m_locator = "execution/" + attempt.m_executionId;
        result.m_source.m_fingerprint = attempt.m_receipt->m_fingerprint;
        // The source is a parsed observation; only candidate evidence may be registered separately.
        result.m_source.m_importStatus = "warning";
        result.m_source.m_profileId = profile.m_profileId;
        result.m_source.m_gameVersion = profile.m_gameVersion;
        result.m_source.m_branch = profile.m_branch;
        result.m_source.m_runtimeTarget = profile.m_runtimeTarget;
        result.m_source.m_toolName = "FOA-SDK Framework execution";
        result.m_source.m_toolVersion = "1.0.0";
        result.m_source.m_importerId = "importer.framework-execution";
        result.m_source.m_importerVersion = "1.0.0";
        result.m_source.m_capturedAt = attempt.m_receipt->m_startedAt;
        result.m_source.m_importedAt = attempt.m_receipt->m_finishedAt;
        result.m_source.m_mediaType = "application/json";
        result.m_source.m_byteSize = CE::Canonicalize(*attempt.m_receipt).GetValue().m_json.size();
        result.m_source.m_limitations =
            "Candidate host execution evidence. No game runtime, qualification, deployment or release authority.";
        for (const auto& phase : attempt.m_receipt->m_phaseReceipts)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = "evidence.framework." + phase.m_fingerprint.substr(7);
            evidence.m_sourceId = result.m_source.m_sourceId;
            evidence.m_sourceFingerprint = result.m_source.m_fingerprint;
            evidence.m_profileId = profile.m_profileId;
            evidence.m_gameVersion = profile.m_gameVersion;
            evidence.m_branch = profile.m_branch;
            evidence.m_subjectRef = phase.m_phasePlan.m_id;
            evidence.m_claim = AZStd::string(CE::Token(phase.m_outcome));
            evidence.m_evidenceKind = "execution-observation";
            evidence.m_confidence = "candidate";
            evidence.m_locator = result.m_source.m_locator;
            evidence.m_recordPath = phase.m_id;
            evidence.m_extractedAt = phase.m_finishedAt.empty() ? attempt.m_receipt->m_finishedAt : phase.m_finishedAt;
            if (!IsStrictUtcTimestamp(evidence.m_extractedAt) || evidence.m_extractedAt < result.m_source.m_capturedAt ||
                evidence.m_extractedAt > result.m_source.m_importedAt)
            {
                return { Error::Invalid };
            }
            result.m_evidence.push_back(AZStd::move(evidence));
        }
        output = AZStd::move(result);
        return {};
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
