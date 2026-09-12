/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "ToolExecutionAdmission.h"
#include "ToolExecutionJournal.h"
#include <atomic>
#include <functional>
#include <memory>

namespace ExternalToolchain
{
    struct ToolRunCallbacks
    {
        // Only the service's bounded in-memory observations; no Editor bus dispatch on workers.
        std::function<void(const ToolInvocationRecordV2&)> m_observe;
        std::function<void(bool, const AZStd::string&)> m_log;
        std::function<bool()> m_beginDrain;
    };
    class ToolProcessBackend
    {
    public:
        virtual ~ToolProcessBackend() = default;
        virtual ToolInvocationRecordV2 Run(
            const ToolInvocationRequestV2&,
            const ToolExecutionCommandV2&,
            const ToolExecutionHostConfiguration&,
            const AZStd::string& hostInstance,
            ToolExecutionJournal&,
            std::atomic_bool& cancel,
            const ToolRunCallbacks&) = 0;
        virtual ToolResult Recover(ToolExecutionJournal&) = 0;
    };
    std::unique_ptr<ToolProcessBackend> MakeToolProcessBackend();
} // namespace ExternalToolchain
