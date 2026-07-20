/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool ResolvePopulationAuthoringContext(
            const WorkspaceModel& workspace,
            const AZStd::string& resolvedWorkspaceRoot,
            const PackManifest* activePack,
            const GameProfile*& profile,
            AZStd::string& workspaceRoot,
            AZStd::string* error)
        {
            profile = workspace.FindActiveGameProfile();
            if (!profile || !profile->IsConfigured())
            {
                if (error)
                {
                    *error = "Population authoring requires one exact configured "
                        "active game profile.";
                }
                return false;
            }
            if (!activePack)
            {
                if (error)
                {
                    *error = "Population authoring requires one active pack.";
                }
                return false;
            }
            workspaceRoot = resolvedWorkspaceRoot.empty()
                ? workspace.m_rootPath
                : resolvedWorkspaceRoot;
            if (workspace.m_workspaceId.empty() || workspaceRoot.empty())
            {
                if (error)
                {
                    *error = "Population authoring requires one canonical "
                        "workspace root.";
                }
                return false;
            }
            return true;
        }

        bool SetCandidateError(
            const AZ::Outcome<CatalogDatabase, AZStd::string>& candidate,
            AZStd::string* error)
        {
            if (candidate.IsSuccess())
            {
                return true;
            }
            if (error)
            {
                *error = AZStd::string(candidate.GetError());
            }
            return false;
        }
    } // namespace

    bool FoundationService::UpsertPopulationActorProfile(
        const PopulationActorProfile& actor,
        AZStd::string* error)
    {
        const GameProfile* profile = nullptr;
        const PackManifest* activePack = GetActivePack();
        AZStd::string workspaceRoot;
        if (!ResolvePopulationAuthoringContext(
                m_workspace,
                m_workspaceRootPath,
                activePack,
                profile,
                workspaceRoot,
                error))
        {
            return false;
        }

        auto candidate = m_populationAuthoring.BuildActorProfileCandidate(
            actor,
            workspaceRoot,
            m_workspace,
            *profile,
            *activePack,
            m_sourceRegistry,
            m_catalog);
        if (!SetCandidateError(candidate, error))
        {
            return false;
        }
        return PersistCatalogCandidate(candidate.GetValue(), error);
    }

    bool FoundationService::UpsertPopulationTroopDefinition(
        const PopulationTroopDefinition& definition,
        AZStd::string* error)
    {
        const GameProfile* profile = nullptr;
        const PackManifest* activePack = GetActivePack();
        AZStd::string workspaceRoot;
        if (!ResolvePopulationAuthoringContext(
                m_workspace,
                m_workspaceRootPath,
                activePack,
                profile,
                workspaceRoot,
                error))
        {
            return false;
        }

        auto candidate = m_populationAuthoring.BuildTroopDefinitionCandidate(
            definition,
            workspaceRoot,
            m_workspace,
            *profile,
            *activePack,
            m_sourceRegistry,
            m_catalog);
        if (!SetCandidateError(candidate, error))
        {
            return false;
        }
        return PersistCatalogCandidate(candidate.GetValue(), error);
    }

    bool FoundationService::UpsertPopulationTroopProfile(
        const PopulationTroopProfile& troop,
        AZStd::string* error)
    {
        const GameProfile* profile = nullptr;
        const PackManifest* activePack = GetActivePack();
        AZStd::string workspaceRoot;
        if (!ResolvePopulationAuthoringContext(
                m_workspace,
                m_workspaceRootPath,
                activePack,
                profile,
                workspaceRoot,
                error))
        {
            return false;
        }

        auto candidate = m_populationAuthoring.BuildTroopProfileCandidate(
            troop,
            workspaceRoot,
            m_workspace,
            *profile,
            *activePack,
            m_sourceRegistry,
            m_catalog);
        if (!SetCandidateError(candidate, error))
        {
            return false;
        }
        return PersistCatalogCandidate(candidate.GetValue(), error);
    }

    bool FoundationService::UpsertPopulationTroopMember(
        const PopulationTroopMember& member,
        AZStd::string* error)
    {
        const GameProfile* profile = nullptr;
        const PackManifest* activePack = GetActivePack();
        AZStd::string workspaceRoot;
        if (!ResolvePopulationAuthoringContext(
                m_workspace,
                m_workspaceRootPath,
                activePack,
                profile,
                workspaceRoot,
                error))
        {
            return false;
        }

        auto candidate = m_populationAuthoring.BuildTroopMemberCandidate(
            member,
            workspaceRoot,
            m_workspace,
            *profile,
            *activePack,
            m_sourceRegistry,
            m_catalog);
        if (!SetCandidateError(candidate, error))
        {
            return false;
        }
        return PersistCatalogCandidate(candidate.GetValue(), error);
    }
} // namespace TaintedGrailModdingSDK
