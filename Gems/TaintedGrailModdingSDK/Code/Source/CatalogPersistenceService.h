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
    class CatalogPersistenceService
    {
    public:
        AZStd::string GetCatalogPath(const AZStd::string& workspaceRoot) const;
        bool Exists(const AZStd::string& workspaceRoot) const;

        AZ::Outcome<AZStd::string, AZStd::string> Save(
            const CatalogDocument& document,
            const AZStd::string& workspaceRoot) const;

        AZ::Outcome<CatalogDocument, AZStd::string> Load(
            const AZStd::string& workspaceRoot) const;
    };
} // namespace TaintedGrailModdingSDK
