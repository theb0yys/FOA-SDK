/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "../../ToolProcessBackend.h"
namespace ExternalToolchain
{
    class UnsupportedToolProcess final : public ToolProcessBackend
    {
    public:
        ToolInvocationRecordV2 Run(
            const ToolInvocationRequestV2& r,
            const ToolExecutionCommandV2&,
            const ToolExecutionHostConfiguration&,
            const AZStd::string&,
            ToolExecutionJournal&,
            std::atomic_bool&,
            const ToolRunCallbacks&) override
        {
            ToolInvocationRecordV2 record;
            record.m_status.m_attemptId = r.m_attemptId;
            record.m_requestFingerprint = r.m_fingerprint;
            record.m_commandFingerprint = r.m_commandFingerprint;
            record.m_profileFingerprint = ToolDigest(ToolExecutionProfile);
            record.m_status.m_error = ToolError::UnsupportedPlatform;
            return record;
        }
        ToolResult Recover(ToolExecutionJournal&) override
        {
            return { ToolError::UnsupportedPlatform };
        }
    };
    std::unique_ptr<ToolProcessBackend> MakeToolProcessBackend()
    {
        return std::make_unique<UnsupportedToolProcess>();
    }
} // namespace ExternalToolchain
