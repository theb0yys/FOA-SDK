/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogDatabase.h"
#include "SourceEvidenceRegistry.h"

namespace TaintedGrailModdingSDK
{
    class FoundationValidationService
    {
    public:
        AZStd::vector<BlockerRecord> Evaluate(
            const WorkspaceModel& workspace,
            const AZStd::vector<PackManifest>& packs,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog) const;

    private:
        static BlockerRecord MakeBlocker(
            AZStd::string blockerId,
            AZStd::string severity,
            AZStd::string area,
            AZStd::string subjectRef,
            AZStd::string reason,
            AZStd::vector<AZStd::string> affectedUsages = {});
    };
} // namespace TaintedGrailModdingSDK
