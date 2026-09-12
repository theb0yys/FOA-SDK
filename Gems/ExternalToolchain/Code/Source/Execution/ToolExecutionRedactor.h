/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include <ExternalToolchain/ToolExecutionTypes.h>
namespace ExternalToolchain
{
    class ToolExecutionRedactor
    {
    public:
        explicit ToolExecutionRedactor(AZStd::vector<AZStd::string> privateMarkers);
        AZStd::string Push(const char* bytes, size_t length, bool finish = false);

    private:
        AZStd::vector<AZStd::string> m_markers;
        AZStd::string m_pending;
        size_t m_overlap = 0;
        bool m_escape = false, m_csi = false;
    };
} // namespace ExternalToolchain
