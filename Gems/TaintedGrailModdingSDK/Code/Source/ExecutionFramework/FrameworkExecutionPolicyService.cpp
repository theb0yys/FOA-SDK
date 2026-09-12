/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkExecutionPolicyService.h"
#include <AzCore/std/algorithm.h>
#include <atomic>
#include <cstdio>
#include <ctime>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
        AZStd::string Utc(std::chrono::system_clock::time_point at)
        {
            auto seconds = std::chrono::system_clock::to_time_t(at);
            std::tm tm{};
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
            gmtime_s(&tm, &seconds);
#else
            gmtime_r(&seconds, &tm);
#endif
            char buffer[32]{};
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
            return buffer;
        }
    } // namespace
    AZStd::string UtcNow()
    {
        return Utc(std::chrono::system_clock::now());
    }
    AZStd::string NewIdentity(const char* prefix)
    {
        static std::atomic_uint64_t counter{ 0 };
        const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return AZStd::string::format(
            "%s.%llu.%llu", prefix, static_cast<unsigned long long>(ticks), static_cast<unsigned long long>(++counter));
    }
    Result FrameworkExecutionPolicyService::SetPolicy(HostPolicy p)
    {
        if (!CE::IsStableId(p.m_context.m_workspaceId) || !CE::IsStableId(p.m_context.m_packId) ||
            !CE::IsDigest(p.m_context.m_profileFingerprint) || !CE::IsStableId(p.m_evidenceId) || p.m_bindingFingerprints.empty() ||
            p.m_bindingFingerprints.size() > CE::MaximumCollection || p.m_until <= Clock::now() ||
            !AZStd::all_of(p.m_bindingFingerprints.begin(), p.m_bindingFingerprints.end(), CE::IsDigest))
        {
            return { Error::Invalid };
        }
        std::lock_guard lock(m_mutex);
        m_policy = AZStd::move(p);
        ++m_revision;
        m_grants.clear();
        return {};
    }
    void FrameworkExecutionPolicyService::Revoke()
    {
        std::lock_guard lock(m_mutex);
        m_policy.reset();
        ++m_revision;
        m_grants.clear();
    }
    Result FrameworkExecutionPolicyService::EvaluateLocked(const PreparedPlan& p, CE::PolicyState& state, AZ::u64& revision) const
    {
        state = CE::PolicyState::DENIED;
        revision = m_revision;
        if (!m_policy || !m_policy->m_context.Matches(p.m_request) || m_policy->m_until <= Clock::now())
        {
            return { Error::PolicyDenied };
        }
        for (const auto& phase : p.m_phases)
        {
            if (AZStd::find(
                    m_policy->m_bindingFingerprints.begin(), m_policy->m_bindingFingerprints.end(), phase.m_binding.m_fingerprint) ==
                m_policy->m_bindingFingerprints.end())
            {
                return { Error::PolicyDenied };
            }
        }
        if (p.m_phases.empty())
        {
            return { Error::PolicyDenied };
        }
        state = m_policy->m_confirmationRequired ? CE::PolicyState::CONFIRMATION_REQUIRED : CE::PolicyState::ALLOWED;
        return {};
    }
    Result FrameworkExecutionPolicyService::Evaluate(const PreparedPlan& p, CE::PolicyState& state, AZ::u64& revision) const
    {
        std::lock_guard lock(m_mutex);
        return EvaluateLocked(p, state, revision);
    }
    Result FrameworkExecutionPolicyService::Confirm(
        const PreparedPlan& p, const AZStd::string& actor, std::chrono::seconds lifetime, CE::CapabilityAuthorizationReceiptV1& output)
    {
        std::lock_guard lock(m_mutex);
        CE::PolicyState state;
        AZ::u64 revision;
        auto result = EvaluateLocked(p, state, revision);
        if (!result)
        {
            return result;
        }
        if (revision != p.m_policyRevision || !CE::IsStableId(actor) || lifetime.count() <= 0 || lifetime > std::chrono::hours(1) ||
            Clock::now() + lifetime > m_policy->m_until || !CE::Validate(p.m_plan).IsSuccess())
        {
            return { Error::Invalid };
        }
        CE::CapabilityAuthorizationReceiptV1 receipt;
        receipt.m_id = NewIdentity("authorization");
        receipt.m_scope = CE::Reference(p.m_plan).GetValue();
        receipt.m_state = CE::AuthorizationState::GRANTED;
        receipt.m_actorId = actor;
        receipt.m_reason = "reason.host-confirmation";
        receipt.m_evidenceIds = { m_policy->m_evidenceId };
        receipt.m_issuedAt = UtcNow();
        receipt.m_expiresAt = Utc(std::chrono::system_clock::now() + lifetime);
        if (!Seal(receipt))
        {
            return { Error::Invalid };
        }
        m_grants.erase(
            AZStd::remove_if(
                m_grants.begin(),
                m_grants.end(),
                [&](const auto& g)
                {
                    return g.m_until <= Clock::now() || g.m_receipt.m_scope.m_fingerprint == p.m_plan.m_fingerprint;
                }),
            m_grants.end());
        if (m_grants.size() == MaximumLineages)
        {
            return { Error::StoreFull };
        }
        m_grants.push_back({ receipt, revision, Clock::now() + lifetime });
        output = AZStd::move(receipt);
        return {};
    }
    Result FrameworkExecutionPolicyService::Authorize(const PreparedPlan& p, CE::CapabilityAuthorizationReceiptV1& output) const
    {
        std::lock_guard lock(m_mutex);
        CE::PolicyState state;
        AZ::u64 revision;
        auto result = EvaluateLocked(p, state, revision);
        if (!result)
        {
            return result;
        }
        if (revision != p.m_policyRevision)
        {
            return { Error::Drifted };
        }
        if (state == CE::PolicyState::ALLOWED)
        {
            CE::CapabilityAuthorizationReceiptV1 receipt;
            receipt.m_id = "authorization.host-policy";
            receipt.m_scope = CE::Reference(p.m_plan).GetValue();
            receipt.m_state = CE::AuthorizationState::NOT_REQUIRED;
            receipt.m_reason = "reason.host-policy";
            receipt.m_evidenceIds = { m_policy->m_evidenceId };
            if (!Seal(receipt))
            {
                return { Error::Invalid };
            }
            output = AZStd::move(receipt);
            return {};
        }
        for (const auto& grant : m_grants)
        {
            if (grant.m_receipt.m_scope.m_fingerprint == p.m_plan.m_fingerprint && grant.m_revision == revision)
            {
                if (grant.m_until <= Clock::now())
                {
                    return { Error::Expired };
                }
                output = grant.m_receipt;
                return {};
            }
        }
        return { Error::AuthorizationRequired };
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
