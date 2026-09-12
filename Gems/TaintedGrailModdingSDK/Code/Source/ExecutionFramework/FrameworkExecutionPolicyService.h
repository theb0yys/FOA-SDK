/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "FrameworkExecutionTypes.h"
#include <mutex>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    struct HostPolicy
    {
        Context m_context;
        AZStd::vector<AZStd::string> m_bindingFingerprints;
        AZStd::string m_evidenceId;
        bool m_confirmationRequired = true;
        Clock::time_point m_until;
    };
    class FrameworkExecutionPolicyService
    {
    public:
        Result SetPolicy(HostPolicy policy);
        void Revoke();
        Result Evaluate(const PreparedPlan&, CE::PolicyState&, AZ::u64& revision) const;
        Result Confirm(
            const PreparedPlan&, const AZStd::string& actorId, std::chrono::seconds lifetime, CE::CapabilityAuthorizationReceiptV1&);
        Result Authorize(const PreparedPlan&, CE::CapabilityAuthorizationReceiptV1&) const;

    private:
        struct Grant
        {
            CE::CapabilityAuthorizationReceiptV1 m_receipt;
            AZ::u64 m_revision;
            Clock::time_point m_until;
        };
        Result EvaluateLocked(const PreparedPlan&, CE::PolicyState&, AZ::u64&) const;
        mutable std::mutex m_mutex;
        AZStd::optional<HostPolicy> m_policy;
        AZStd::vector<Grant> m_grants;
        AZ::u64 m_revision = 0;
    };
} // namespace TaintedGrailModdingSDK::ExecutionFramework
