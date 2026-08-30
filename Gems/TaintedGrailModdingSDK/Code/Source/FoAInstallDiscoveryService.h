/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    //! Discovers local Fall of Avalon installation roots without assigning profile semantics.
    //! LocalSetupDetectionService remains the authority that validates a candidate and derives
    //! the exact game profile from it.
    class FoAInstallDiscoveryService
    {
    public:
        struct Result
        {
            AZStd::vector<AZStd::string> m_installPathCandidates;
            AZStd::vector<AZStd::string> m_notes;
        };

        Result Discover() const;

        //! Deterministic entry point for tests and additional store integrations. Each supplied
        //! path is a Steam root containing a steamapps directory.
        static Result DiscoverFromSteamRoots(const AZStd::vector<AZStd::string>& steamRoots);
    };
} // namespace TaintedGrailModdingSDK
