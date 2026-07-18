/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "WorkspaceSchemaService.h"

#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/string/regex.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool IsStableId(const AZStd::string& value)
        {
            static const AZStd::regex pattern(
                "^[a-z0-9][a-z0-9._-]*\\.[a-z0-9][a-z0-9._-]*$");
            return AZStd::regex_match(value, pattern);
        }
    } // namespace

    AZ::Outcome<WorkspaceModel, AZStd::string> WorkspaceSchemaService::MigrateAndValidate(
        WorkspaceModel workspace,
        AZ::u32 detectedSchemaVersion) const
    {
        if (detectedSchemaVersion != LegacySchemaVersion
            && detectedSchemaVersion != CurrentSchemaVersion)
        {
            return AZ::Failure(AZStd::string::format(
                "Workspace schema version %u is unsupported; this editor supports schema 0 migration and schema 1.",
                detectedSchemaVersion));
        }

        const AZ::Outcome<void, AZStd::string> validation = Validate(workspace);
        if (!validation.IsSuccess())
        {
            const char* prefix = detectedSchemaVersion == LegacySchemaVersion
                ? "Legacy workspace schema 0 cannot be migrated safely: "
                : "Workspace schema 1 validation failed: ";
            return AZ::Failure(AZStd::string(prefix) + validation.GetError());
        }

        return AZ::Success(AZStd::move(workspace));
    }

    AZ::Outcome<void, AZStd::string> WorkspaceSchemaService::Validate(
        const WorkspaceModel& workspace) const
    {
        if (!IsStableId(workspace.m_workspaceId))
        {
            return AZ::Failure(AZStd::string(
                "WorkspaceId must be a lowercase namespaced stable ID such as owner.workspace."));
        }
        if (workspace.m_displayName.empty())
        {
            return AZ::Failure(AZStd::string("DisplayName is required."));
        }
        if (workspace.m_rootPath.empty()
            || workspace.m_outputPath.empty()
            || workspace.m_stagingPath.empty()
            || workspace.m_deploymentPath.empty())
        {
            return AZ::Failure(AZStd::string(
                "RootPath, OutputPath, StagingPath, and DeploymentPath are required."));
        }
        if (workspace.m_gameProfiles.empty())
        {
            return AZ::Failure(AZStd::string("At least one game profile is required."));
        }
        if (workspace.m_activeGameProfileId.empty())
        {
            return AZ::Failure(AZStd::string("ActiveGameProfileId is required."));
        }

        AZStd::unordered_set<AZStd::string> profileIds;
        const GameProfile* activeProfile = nullptr;
        for (const GameProfile& profile : workspace.m_gameProfiles)
        {
            if (!IsStableId(profile.m_profileId))
            {
                return AZ::Failure(AZStd::string(
                    "Every ProfileId must be a lowercase namespaced stable ID."));
            }
            if (!profileIds.insert(profile.m_profileId).second)
            {
                return AZ::Failure(
                    AZStd::string("GameProfiles contains duplicate ProfileId: ") + profile.m_profileId);
            }
            if (profile.m_profileId == workspace.m_activeGameProfileId)
            {
                activeProfile = &profile;
            }
            if (!profile.IsConfigured())
            {
                return AZ::Failure(
                    AZStd::string("Game profile is not fully configured: ") + profile.m_profileId);
            }
            if (profile.m_runtimeTarget != "Mono" && profile.m_runtimeTarget != "IL2CPP")
            {
                return AZ::Failure(
                    AZStd::string("RuntimeTarget must be Mono or IL2CPP for profile: ")
                    + profile.m_profileId);
            }
            if (profile.m_runtimeTarget == "Mono"
                && (profile.m_bepInExVersion.empty() || profile.m_pluginPath.empty()))
            {
                return AZ::Failure(
                    AZStd::string("Mono profiles require BepInExVersion and PluginPath: ")
                    + profile.m_profileId);
            }
        }

        if (!activeProfile)
        {
            return AZ::Failure(AZStd::string(
                "ActiveGameProfileId does not bind to exactly one GameProfiles entry."));
        }

        return AZ::Success();
    }
} // namespace TaintedGrailModdingSDK
