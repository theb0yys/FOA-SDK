/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/parallel/mutex.h>
#include <ExternalToolchain/ToolExecutionTypes.h>

namespace ExternalToolchain
{
    class ToolExecutionRequests : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        using MutexType = AZStd::recursive_mutex;
        virtual ~ToolExecutionRequests() = default;
        virtual ToolResult RegisterExecutionCommand(const ToolExecutionCommandV2&) = 0;
        virtual ToolResult Submit(const ToolInvocationRequestV2&) = 0;
        virtual ToolResult GetStatus(const AZStd::string& attemptId, ToolInvocationStatusV2&) const = 0;
        virtual ToolResult Cancel(const AZStd::string& attemptId) = 0;
        virtual ToolLogPage ReadLog(const AZStd::string& attemptId, bool stderrStream, AZ::u64 cursor) const = 0;
        virtual ToolRecordPage EnumerateRecords(size_t offset, size_t count) const = 0;
    };
    using ToolExecutionRequestBus = AZ::EBus<ToolExecutionRequests>;
} // namespace ExternalToolchain
