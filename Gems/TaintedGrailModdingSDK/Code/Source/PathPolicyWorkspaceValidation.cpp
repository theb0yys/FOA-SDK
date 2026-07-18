/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PathPolicyService.h"

#include <AzCore/PlatformDef.h>

#include <filesystem>
#include <system_error>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        namespace Filesystem = std::filesystem;

#if AZ_TRAIT_OS_PLATFORM_WINDOWS
        constexpr bool PlatformPathsAreCaseInsensitive = true;
#else
        constexpr bool PlatformPathsAreCaseInsensitive = false;
#endif

        Filesystem::path FromUtf8(const AZStd::string& value)
        {
            return Filesystem::u8path(value.c_str());
        }

        AZStd::string ToUtf8(const Filesystem::path& value)
        {
#if defined(__cpp_lib_char8_t)
            const std::u8string text = value.generic_u8string();
            return AZStd::string(reinterpret_cast<const char*>(text.data()), text.size());
#else
            const std::string text = value.generic_u8string();
            return AZStd::string(text.data(), text.size());
#endif
        }

        AZ::Outcome<AZStd::string, AZStd::string> ResolvePath(
            const AZStd::string& value,
            const AZStd::string& canonicalWorkspaceRoot,
            const char* label)
        {
            if (value.empty())
            {
                return AZ::Failure(AZStd::string(label) + " is required.");
            }

            Filesystem::path path = FromUtf8(value);
            if (path.is_relative())
            {
                path = FromUtf8(canonicalWorkspaceRoot) / path;
            }

            std::error_code error;
            path = Filesystem::absolute(path, error);
            if (error)
            {
                return AZ::Failure(
                    AZStd::string("Unable to make ") + label + " absolute: " + error.message().c_str());
            }
            path = Filesystem::weakly_canonical(path, error);
            if (error)
            {
                return AZ::Failure(
                    AZStd::string("Unable to resolve canonical ") + label + ": " + error.message().c_str());
            }
            return AZ::Success(ToUtf8(path));
        }

        AZ::Outcome<void, AZStd::string> RequireWorkspaceContained(
            const AZStd::string& value,
            const AZStd::string& canonicalWorkspaceRoot,
            const char* label)
        {
            auto resolved = ResolvePath(value, canonicalWorkspaceRoot, label);
            if (!resolved.IsSuccess())
            {
                return AZ::Failure(AZStd::string(resolved.GetError()));
            }
            if (!PathPolicyService::IsCanonicalPathContained(
                    canonicalWorkspaceRoot,
                    resolved.GetValue(),
                    PlatformPathsAreCaseInsensitive))
            {
                return AZ::Failure(
                    AZStd::string(label) + " must remain inside the canonical workspace root: "
                    + resolved.GetValue());
            }
            return AZ::Success();
        }

        AZ::Outcome<void, AZStd::string> ValidateProfilePaths(
            const GameProfile& profile,
            const AZStd::string& canonicalWorkspaceRoot)
        {
            AZ::Outcome<void, AZStd::string> result = AZ::Success();
            if (!profile.m_diagnosticsPath.empty())
            {
                result = RequireWorkspaceContained(
                    profile.m_diagnosticsPath,
                    canonicalWorkspaceRoot,
                    "DiagnosticsPath");
                if (!result.IsSuccess())
                {
                    return AZ::Failure(
                        AZStd::string("Profile ") + profile.m_profileId + ": " + result.GetError());
                }
            }
            if (!profile.m_extractedDataPath.empty())
            {
                result = RequireWorkspaceContained(
                    profile.m_extractedDataPath,
                    canonicalWorkspaceRoot,
                    "ExtractedDataPath");
                if (!result.IsSuccess())
                {
                    return AZ::Failure(
                        AZStd::string("Profile ") + profile.m_profileId + ": " + result.GetError());
                }
            }

            auto installPath = ResolvePath(profile.m_installPath, canonicalWorkspaceRoot, "InstallPath");
            if (!installPath.IsSuccess())
            {
                return AZ::Failure(
                    AZStd::string("Profile ") + profile.m_profileId + ": " + installPath.GetError());
            }
            auto managedPath = ResolvePath(
                profile.m_managedAssembliesPath,
                canonicalWorkspaceRoot,
                "ManagedAssembliesPath");
            if (!managedPath.IsSuccess())
            {
                return AZ::Failure(
                    AZStd::string("Profile ") + profile.m_profileId + ": " + managedPath.GetError());
            }
            if (!PathPolicyService::IsCanonicalPathContained(
                    installPath.GetValue(),
                    managedPath.GetValue(),
                    PlatformPathsAreCaseInsensitive))
            {
                return AZ::Failure(
                    AZStd::string("Profile ") + profile.m_profileId
                    + ": ManagedAssembliesPath must remain inside the canonical InstallPath.");
            }

            if (profile.m_runtimeTarget == "Mono")
            {
                auto pluginPath = ResolvePath(profile.m_pluginPath, canonicalWorkspaceRoot, "PluginPath");
                if (!pluginPath.IsSuccess())
                {
                    return AZ::Failure(
                        AZStd::string("Profile ") + profile.m_profileId + ": " + pluginPath.GetError());
                }
                if (!PathPolicyService::IsCanonicalPathContained(
                        installPath.GetValue(),
                        pluginPath.GetValue(),
                        PlatformPathsAreCaseInsensitive))
                {
                    return AZ::Failure(
                        AZStd::string("Profile ") + profile.m_profileId
                        + ": PluginPath must remain inside the canonical InstallPath for Mono profiles.");
                }
            }

            return AZ::Success();
        }
    } // namespace

    AZ::Outcome<void, AZStd::string> PathPolicyService::ValidateWorkspacePaths(
        const WorkspaceModel& workspace,
        const AZStd::string& canonicalWorkspaceRoot) const
    {
        if (canonicalWorkspaceRoot.empty())
        {
            return AZ::Failure(AZStd::string("The canonical workspace root is required."));
        }

        auto result = RequireWorkspaceContained(
            workspace.m_outputPath,
            canonicalWorkspaceRoot,
            "OutputPath");
        if (!result.IsSuccess())
        {
            return AZ::Failure(AZStd::string(result.GetError()));
        }
        result = RequireWorkspaceContained(
            workspace.m_stagingPath,
            canonicalWorkspaceRoot,
            "StagingPath");
        if (!result.IsSuccess())
        {
            return AZ::Failure(AZStd::string(result.GetError()));
        }
        result = RequireWorkspaceContained(
            workspace.m_deploymentPath,
            canonicalWorkspaceRoot,
            "DeploymentPath");
        if (!result.IsSuccess())
        {
            return AZ::Failure(AZStd::string(result.GetError()));
        }

        if (!workspace.FindActiveGameProfile())
        {
            return AZ::Failure(AZStd::string("The active game profile binding is invalid."));
        }

        for (const GameProfile& profile : workspace.m_gameProfiles)
        {
            result = ValidateProfilePaths(profile, canonicalWorkspaceRoot);
            if (!result.IsSuccess())
            {
                return AZ::Failure(AZStd::string(result.GetError()));
            }
        }

        return AZ::Success();
    }
} // namespace TaintedGrailModdingSDK
