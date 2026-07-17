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
    class PackPersistenceService
    {
    public:
        AZ::Outcome<void, AZStd::string> Save(const PackManifest& pack, const AZStd::string& filePath) const;
        AZ::Outcome<PackManifest, AZStd::string> Load(const AZStd::string& filePath) const;
    };
} // namespace TaintedGrailModdingSDK
