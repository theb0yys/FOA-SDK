/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterStagingDeploymentPreviewService.h"
#include "CanonicalFingerprint.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>
#include <locale>
#include <sstream>

#include "AdapterStagingDeploymentPreviewServicePart1.inl"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        void AddDeploymentPreviewFinding(
            AZStd::vector<AdapterDeploymentPreviewBlocker>& blockers,
            AZStd::string code,
            AZStd::string targetPath,
            AZStd::string reason)
        {
            // The read-only operational boundary is represented by the explicit
            // false permission fields and canonical JSON. It is not a blocker and
            // must not poison the exact Ready handoff to the work-order slice.
            if (code == "deployment.preview_only")
            {
                return;
            }
            AddBlocker(
                blockers,
                AZStd::move(code),
                AZStd::move(targetPath),
                AZStd::move(reason));
        }
    } // namespace
} // namespace TaintedGrailModdingSDK

#define AddBlocker AddDeploymentPreviewFinding
#include "AdapterStagingDeploymentPreviewServicePart2.inl"
#undef AddBlocker

#include "AdapterStagingDeploymentPreviewServicePart3.inl"
