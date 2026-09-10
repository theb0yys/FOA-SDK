/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "SocietyModels.h"
#include <AzCore/Outcome/Outcome.h>

namespace TaintedGrailModdingSDK
{
    class CatalogDatabase;
    class SocietyPlanningService
    {
    public:
        static bool IsSingleLine(const AZStd::string& text, size_t maximumBytes, bool emptyAllowed = true);
        static AZ::Outcome<void, AZStd::string> ValidateCulture(const CultureProfile& profile);
        static AZ::Outcome<void, AZStd::string> ValidateFactionProfile(const FactionProfile& profile, const CatalogDatabase& catalog);
        static FactionAnalysis Analyze(const FactionDefinition& definition, const CatalogDatabase& catalog);
    };
}
