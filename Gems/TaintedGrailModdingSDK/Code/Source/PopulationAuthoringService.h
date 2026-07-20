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
    struct PopulationTroopDefinition
    {
        PopulationTroopProfile m_profile;
        AZStd::vector<PopulationTroopMember> m_members;
    };

    //! Builds fully validated population catalog candidates without publishing them.
    class PopulationAuthoringService final
    {
    public:
        AZ::Outcome<CatalogDatabase, AZStd::string> BuildActorProfileCandidate(
            const PopulationActorProfile& actor,
            const AZStd::string& workspaceRoot,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const PackManifest& activePack,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog) const;

        AZ::Outcome<CatalogDatabase, AZStd::string> BuildTroopDefinitionCandidate(
            const PopulationTroopDefinition& definition,
            const AZStd::string& workspaceRoot,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const PackManifest& activePack,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog) const;

        AZ::Outcome<CatalogDatabase, AZStd::string> BuildTroopProfileCandidate(
            const PopulationTroopProfile& troop,
            const AZStd::string& workspaceRoot,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const PackManifest& activePack,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog) const;

        AZ::Outcome<CatalogDatabase, AZStd::string> BuildTroopMemberCandidate(
            const PopulationTroopMember& member,
            const AZStd::string& workspaceRoot,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const PackManifest& activePack,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog) const;
    };
} // namespace TaintedGrailModdingSDK
