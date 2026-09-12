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
    class FrameworkTargetOwnershipLedger
    {
    public:
        Result Reserve(const CE::CapabilityExecutionPlanV1&);
        Result Observe(const CE::CapabilityExecutionPlanV1&, const CE::CapabilityExecutionReceiptV1&, bool publish = true);
        Result CheckPreimage(const CE::TargetMutationClaimV1&, const CE::TargetObservationV1&) const;
        void Release(const AZStd::string& planFingerprint, bool cleanupConfirmed);
        Result Owns(const CE::ArtifactRecordV1&) const;

    private:
        struct Entry
        {
            AZStd::string m_location, m_owner, m_digest, m_artifactId, m_plan;
            bool m_reserved = false;
        };
        mutable std::mutex m_mutex;
        AZStd::vector<Entry> m_entries;
    };
} // namespace TaintedGrailModdingSDK::ExecutionFramework
