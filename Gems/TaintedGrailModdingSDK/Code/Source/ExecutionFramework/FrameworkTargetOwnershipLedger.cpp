/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkTargetOwnershipLedger.h"
#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
        AZStd::string Location(const AZStd::string& root, const AZStd::string& relative)
        {
            AZStd::string key = root + "/" + relative;
            for (auto& c : key)
            {
                if (c >= 'A' && c <= 'Z')
                {
                    c += 'a' - 'A';
                }
            }
            return key;
        }
    } // namespace
    Result FrameworkTargetOwnershipLedger::Reserve(const CE::CapabilityExecutionPlanV1& plan)
    {
        if (!CE::Validate(plan).IsSuccess())
        {
            return { Error::Invalid };
        }
        std::lock_guard lock(m_mutex);
        auto next = m_entries;
        for (const auto& phase : plan.m_phases)
        {
            for (const auto& a : phase.m_expectedOutputs)
            {
                auto key = Location(a.m_storageRootId, a.m_relativePath);
                auto found = AZStd::find_if(
                    next.begin(),
                    next.end(),
                    [&](const auto& e)
                    {
                        return e.m_location == key;
                    });
                if (found != next.end())
                {
                    if (found->m_owner != a.m_ownerPackId || found->m_artifactId != a.m_id ||
                        (found->m_reserved && found->m_plan != plan.m_fingerprint) ||
                        (!a.m_expectedDigest.empty() && !found->m_digest.empty() && found->m_digest != a.m_expectedDigest))
                    {
                        return { Error::Conflict };
                    }
                    found->m_plan = plan.m_fingerprint;
                    continue;
                }
                next.push_back({ key, a.m_ownerPackId, a.m_expectedDigest, a.m_id, plan.m_fingerprint, true });
            }
        }
        if (next.size() > MaximumLineages * CE::MaximumCollection)
        {
            return { Error::StoreFull };
        }
        m_entries = AZStd::move(next);
        return {};
    }
    Result FrameworkTargetOwnershipLedger::Observe(
        const CE::CapabilityExecutionPlanV1& plan, const CE::CapabilityExecutionReceiptV1& receipt, bool publish)
    {
        AZStd::vector<CE::PhaseExtensionReferenceV1> extensions;
        for (const auto& p : receipt.m_phaseReceipts)
        {
            if (p.m_extension)
            {
                extensions.push_back(*p.m_extension);
            }
        }
        if (!CE::Validate(receipt, plan, receipt.m_authorization, extensions).IsSuccess())
        {
            return { Error::Invalid };
        }
        std::lock_guard lock(m_mutex);
        auto next = m_entries;
        for (const auto& phase : receipt.m_phaseReceipts)
        {
            if (phase.m_outcome != CE::Outcome::SUCCEEDED)
            {
                continue;
            }
            for (const auto& record : phase.m_outputs)
            {
                const auto& a = record.m_artifact;
                auto key = Location(a.m_storageRootId, a.m_relativePath);
                auto e = AZStd::find_if(
                    next.begin(),
                    next.end(),
                    [&](const auto& value)
                    {
                        return value.m_location == key;
                    });
                if (e == next.end() || e->m_owner != a.m_ownerPackId || e->m_artifactId != a.m_id || e->m_plan != plan.m_fingerprint ||
                    (!e->m_digest.empty() && e->m_digest != a.m_digest))
                {
                    return { Error::Conflict };
                }
                e->m_digest = a.m_digest;
                e->m_reserved = false;
            }
        }
        if (publish)
        {
            m_entries = AZStd::move(next);
        }
        return {};
    }
    Result FrameworkTargetOwnershipLedger::CheckPreimage(
        const CE::TargetMutationClaimV1& claim, const CE::TargetObservationV1& observation) const
    {
        if (!CE::Validate(claim).IsSuccess() || !CE::Validate(observation).IsSuccess())
        {
            return { Error::Invalid };
        }
        if (Location(claim.m_targetRootId, claim.m_relativePath) != Location(observation.m_targetRootId, observation.m_relativePath) ||
            observation.m_mutationId != claim.m_id || observation.m_verification != CE::VerificationState::PASSED ||
            claim.m_preimagePresence != observation.m_presence || claim.m_preimageFingerprint != observation.m_contentFingerprint ||
            claim.m_preimageOwnerId != observation.m_ownerPackId)
        {
            return { Error::Drifted };
        }
        return {};
    }
    void FrameworkTargetOwnershipLedger::Release(const AZStd::string& plan, bool cleanupConfirmed)
    {
        if (!cleanupConfirmed)
        {
            return;
        }
        std::lock_guard lock(m_mutex);
        m_entries.erase(
            AZStd::remove_if(
                m_entries.begin(),
                m_entries.end(),
                [&](const auto& e)
                {
                    return e.m_plan == plan && e.m_reserved;
                }),
            m_entries.end());
    }
    Result FrameworkTargetOwnershipLedger::Owns(const CE::ArtifactRecordV1& a) const
    {
        std::lock_guard lock(m_mutex);
        for (const auto& e : m_entries)
        {
            if (!e.m_reserved && e.m_location == Location(a.m_artifact.m_storageRootId, a.m_artifact.m_relativePath) &&
                e.m_owner == a.m_artifact.m_ownerPackId && e.m_digest == a.m_artifact.m_digest && e.m_artifactId == a.m_id)
            {
                return {};
            }
        }
        return { Error::Conflict };
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
