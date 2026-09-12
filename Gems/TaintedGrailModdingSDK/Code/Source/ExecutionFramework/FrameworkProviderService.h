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
    class FrameworkProviderService
    {
    public:
        Result Register(HostBinding binding);
        void Finalize();
        Result ReviewQualification(Qualification observation);
        void RevokeQualification(const AZStd::string& bindingFingerprint);
        Result Resolve(
            const CE::CapabilityDescriptorV1&,
            const CE::CapabilityExecutionRequestV1&,
            CE::Phase,
            const AZStd::string& workspaceDefault,
            HostBinding&) const;
        Result Prepare(const HostBinding&, const CE::CapabilityExecutionRequestV1&, PreparedPhase&) const;
        Result Recheck(const PreparedPhase&) const;
        AZStd::vector<HostBinding> Bindings() const;
        bool CurrentConfiguration(const ET::ToolExecutionCommandV2&, ET::ToolResolvedConfiguration&) const;
        static AZStd::string DescriptorFingerprint(const ET::ExternalToolProviderDescriptor&);
        static bool Supported(const CE::CapabilityDescriptorV1&);

    private:
        mutable std::mutex m_mutex;
        bool m_finalized = false;
        AZStd::vector<HostBinding> m_bindings;
        AZStd::vector<Qualification> m_qualifications;
        AZ::u64 m_revision = 0;
    };
} // namespace TaintedGrailModdingSDK::ExecutionFramework
