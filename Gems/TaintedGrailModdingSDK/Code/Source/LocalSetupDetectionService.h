/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    class LocalSetupDetectionService
    {
    public:
        struct Hints
        {
            AZStd::string m_workspaceRoot;
            AZStd::vector<AZStd::string> m_installPathCandidates;
        };

        struct Result
        {
            WorkspaceModel m_workspace;
            AZStd::vector<AZStd::string> m_notes;
            bool m_changed = false;
            bool m_workspaceRootDetected = false;
            bool m_gameInstallDetected = false;
            bool m_gameProfileComplete = false;
        };

        Result Detect(const WorkspaceModel& current, const Hints& hints) const;

        static bool LooksLikeTaintedGrailInstall(const AZStd::string& installPath);
    };
} // namespace TaintedGrailModdingSDK
