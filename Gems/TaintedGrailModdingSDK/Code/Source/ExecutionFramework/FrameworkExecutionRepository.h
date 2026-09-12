/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "FrameworkExecutionTypes.h"
#include <atomic>
#include <mutex>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    struct StoredPhaseIntent
    {
        AZStd::string m_toolAttemptId, m_phaseFingerprint, m_requestFingerprint, m_commandFingerprint, m_providerId, m_commandId;
    };
    struct StoredAttempt
    {
        AZStd::string m_executionId, m_operationKey;
        AZ::u64 m_attempt = 0;
        CE::CapabilityExecutionPlanV1 m_plan;
        AZStd::optional<CE::CapabilityExecutionReceiptV1> m_receipt;
        AZStd::vector<ET::ToolInvocationRecordV2> m_tools;
        AZStd::vector<StoredPhaseIntent> m_phaseIntents;
        AZStd::vector<ET::ToolInvocationRecordV2> m_recoveredTools;
        bool m_quarantined = false;
    };
    class FrameworkExecutionRepository
    {
    public:
        FrameworkExecutionRepository();
        ~FrameworkExecutionRepository();
        Result Open(const AZStd::string& privateRoot, const Context&);
        Result Begin(const StoredAttempt&);
        Result PhaseIntent(
            const AZStd::string& executionId, AZ::u64 attempt, const AZStd::string& phaseFingerprint, const ET::ToolInvocationRequestV2&);
        Result Commit(const StoredAttempt&);
        Result Reconcile(const AZStd::vector<ET::ToolInvocationRecordV2>&);
        AZStd::vector<StoredAttempt> Read(size_t offset, size_t count) const;
        Result CheckCapacity(AZ::u64 additionalBytes, const std::atomic_bool* cancelled = nullptr) const;
        bool Healthy() const;
        AZStd::string Root() const;
        AZ::u64 Bytes() const;
        void StopWrites();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
    // Reconstruct and contextually validate all typed upstream values of a saved plan.
    bool ValidateStoredPlan(const CE::CapabilityExecutionPlanV1&);
} // namespace TaintedGrailModdingSDK::ExecutionFramework
