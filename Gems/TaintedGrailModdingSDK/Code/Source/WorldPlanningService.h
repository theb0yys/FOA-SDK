/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "WorldModels.h"
#include <AzCore/Outcome/Outcome.h>
namespace TaintedGrailModdingSDK
{
    class CatalogDatabase;
    class WorldPlanningService
    {
    public:
        static bool IsSingleLine(const AZStd::string& value, size_t maximumBytes, bool emptyAllowed = true);
        static AZ::Outcome<void, AZStd::string> ValidatePlace(const WorldPlaceProfile& place,
            const AZStd::string& kind, const CatalogDatabase& catalog);
        static AZ::Outcome<void, AZStd::string> ValidatePath(const WorldPathProfile& path,
            const AZStd::string& kind, const CatalogDatabase& catalog);
        static WorldPathAnalysis Analyze(const WorldPathDefinition& path, const AZStd::string& kind,
            const CatalogDatabase& catalog);
    };
}
