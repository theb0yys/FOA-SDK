/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <AzCore/Outcome/Outcome.h>

namespace TaintedGrailModdingSDK
{
    class WorkspaceSchemaService
    {
    public:
        static constexpr AZ::u32 LegacySchemaVersion = 0;
        static constexpr AZ::u32 CurrentSchemaVersion = 1;

        AZ::Outcome<WorkspaceModel, AZStd::string> MigrateAndValidate(
            WorkspaceModel workspace,
            AZ::u32 detectedSchemaVersion) const;

        AZ::Outcome<void, AZStd::string> Validate(const WorkspaceModel& workspace) const;
    };
} // namespace TaintedGrailModdingSDK
