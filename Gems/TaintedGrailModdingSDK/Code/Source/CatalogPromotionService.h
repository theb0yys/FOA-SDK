/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/algorithm.h>

#include <QByteArray>

namespace TaintedGrailModdingSDK
{
    class CatalogPromotionService
    {
    public:
        AZ::Outcome<CatalogRecord, AZStd::string> BuildReviewedRecord(
            const CatalogPromotionRequest& request,
            const WorkspaceModel& workspace,
            const AZStd::vector<PackManifest>& packs,
            const SourceEvidenceRegistry& sourceRegistry) const;

    private:
        static const PackManifest* FindPack(
            const AZStd::vector<PackManifest>& packs,
            const AZStd::string& packId);
    };
} // namespace TaintedGrailModdingSDK
