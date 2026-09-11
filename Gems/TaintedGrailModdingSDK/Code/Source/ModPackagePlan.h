/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "CatalogDatabase.h"
#include <AzCore/Outcome/Outcome.h>
namespace TaintedGrailModdingSDK
{
    struct ModPackageSelection
    {
        AZStd::vector<PackManifest> m_packs;
        CatalogDocument m_catalog;
        SourceEvidenceRegistry m_evidence;
        AZStd::vector<AZStd::string> m_warnings;
    };
    //! Pure selection/validation; never exports source payloads or grants usages.
    class ModPackagePlan final
    {
    public:
        static AZ::Outcome<ModPackageSelection, AZStd::string> Select(
            const WorkspaceModel&, const AZStd::vector<PackManifest>&,
            const AZStd::string& selectedPack, const CatalogDatabase&, const SourceEvidenceRegistry&);
    };
}
