/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "FrameworkExecutionTypes.h"
#include <atomic>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    class FrameworkArtifactRepository
    {
    public:
        explicit FrameworkArtifactRepository(AZStd::string root);
        Result Capture(
            const CE::CapabilityPhasePlanV1&,
            const ET::ToolInvocationRequestV2&,
            const ET::ToolInvocationRecordV2&,
            const AZStd::string& toolStore,
            const AZStd::string& executionId,
            const CE::PhaseExtensionReferenceV1&,
            const std::atomic_bool& cancelled,
            AZStd::vector<CE::ArtifactRecordV1>&,
            std::function<bool(const CE::ArtifactRecordV1&)> ownsExisting = {});
        Result Verify(const CE::ArtifactRecordV1&) const;
        AZStd::string Path(const CE::ArtifactReferenceV1&) const;

    private:
        AZStd::string m_root;
        AZ::u64 m_bytes = 0;
    };
} // namespace TaintedGrailModdingSDK::ExecutionFramework
