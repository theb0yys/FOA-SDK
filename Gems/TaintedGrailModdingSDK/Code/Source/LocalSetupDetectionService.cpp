/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "LocalSetupDetectionService.h"

#include "FoARuntimeAdapterRoutes.h"

#include <AzCore/PlatformDef.h>
#if AZ_TRAIT_USE_WINDOWS_FILE_API
#   include <AzCore/PlatformIncl.h>
#endif
#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <regex>
#include <string>
#include <system_error>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        namespace Filesystem = std::filesystem;

        constexpr const char* DefaultWorkspaceId = "tgfoa.workspace.default";
        constexpr const char* DefaultWorkspaceName = "Tainted Grail Modding Workspace";
        constexpr const char* DefaultMonoProfileId = "foa.mono.current";
        constexpr const char* DefaultIl2CppProfileId = "foa.il2cpp.current";
        constexpr const char* DefaultMonoProfileName = "FoA Mono Current";
        constexpr const char* DefaultIl2CppProfileName = "FoA IL2CPP Current";
        constexpr size_t MaximumDirectoryEntries = 4096;
        constexpr size_t MaximumVersionProbeBytes = 2 * 1024 * 1024;

        Filesystem::path FromUtf8(const AZStd::string& value)
        {
#if defined(__cpp_lib_char8_t)
            return Filesystem::path(std::u8string(
                reinterpret_cast<const char8_t*>(value.data()),
                value.size()));
#else
            return Filesystem::u8path(value.c_str());
#endif
        }

        AZStd::string ToUtf8(const Filesystem::path& value)
        {
#if defined(__cpp_lib_char8_t)
            const std::u8string text = value.generic_u8string();
            return AZStd::string(
                reinterpret_cast<const char*>(text.data()),
                text.size());
#else
            const std::string text = value.generic_u8string();
            return AZStd::string(text.data(), text.size());
#endif
        }

        void AddNote(
            LocalSetupDetectionService::Result& result,
            const char* note)
        {
            const AZStd::string value(note);
            if (AZStd::find(result.m_notes.begin(), result.m_notes.end(), value)
                == result.m_notes.end())
            {
                result.m_notes.push_back(value);
            }
        }

        void SetIfEmpty(
            AZStd::string& field,
            AZStd::string value,
            LocalSetupDetectionService::Result& result)
        {
            if (field.empty() && !value.empty())
            {
                field = AZStd::move(value);
                result.m_changed = true;
            }
        }

        void SetRouteField(
            AZStd::string& field,
            AZStd::string value,
            bool profileWasConfigured,
            LocalSetupDetectionService::Result& result)
        {
            if (!value.empty() && (field.empty() || !profileWasConfigured))
            {
                if (field != value)
                {
                    field = AZStd::move(value);
                    result.m_changed = true;
                }
            }
        }

        bool ResolveExistingDirectory(
            const AZStd::string& value,
            Filesystem::path& resolved)
        {
            if (value.empty())
            {
                return false;
            }

            std::error_code error;
            Filesystem::path path = Filesystem::absolute(FromUtf8(value), error).lexically_normal();
            if (error || path.empty())
            {
                return false;
            }
            path = Filesystem::weakly_canonical(path, error);
            if (error
                || !Filesystem::exists(path, error)
                || error
                || !Filesystem::is_directory(path, error)
                || error)
            {
                return false;
            }
            resolved = AZStd::move(path);
            return true;
        }

        AZStd::string NormalizeFutureDirectory(const AZStd::string& value)
        {
            if (value.empty())
            {
                return {};
            }
            std::error_code error;
            Filesystem::path path = Filesystem::absolute(FromUtf8(value), error).lexically_normal();
            if (error || path.empty())
            {
                return value;
            }
            const Filesystem::path absolutePath = path;
            path = Filesystem::weakly_canonical(path, error);
            return error ? ToUtf8(absolutePath) : ToUtf8(path);
        }

        bool IsFilesystemRoot(const Filesystem::path& path)
        {
            return path == path.root_path()
                || path.lexically_normal() == path.root_path().lexically_normal();
        }

        bool HasStorageIndirection(const Filesystem::path& path)
        {
            if (path.empty())
            {
                return true;
            }

            Filesystem::path current = path.root_path();
            for (const Filesystem::path& component : path.relative_path())
            {
                current /= component;
                std::error_code error;
                const Filesystem::file_status linkStatus =
                    Filesystem::symlink_status(current, error);
                if (error)
                {
                    return true;
                }
                if (Filesystem::is_symlink(linkStatus))
                {
                    return true;
                }
#if AZ_TRAIT_USE_WINDOWS_FILE_API
                const DWORD attributes = GetFileAttributesW(current.c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES
                    || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                {
                    return true;
                }
#endif
            }
            return false;
        }

        bool RegularFileExists(const Filesystem::path& path)
        {
            std::error_code error;
            return Filesystem::exists(path, error)
                && !error
                && Filesystem::is_regular_file(path, error)
                && !error;
        }

        bool DirectoryExists(const Filesystem::path& path)
        {
            std::error_code error;
            return Filesystem::exists(path, error)
                && !error
                && Filesystem::is_directory(path, error)
                && !error;
        }

        bool DirectoryContainsDll(const Filesystem::path& directory)
        {
            if (!DirectoryExists(directory))
            {
                return false;
            }

            std::error_code error;
            size_t inspected = 0;
            for (Filesystem::directory_iterator iterator(directory, error);
                 !error && iterator != Filesystem::directory_iterator();
                 iterator.increment(error))
            {
                if (++inspected > MaximumDirectoryEntries)
                {
                    return false;
                }

                const Filesystem::file_status status =
                    iterator->symlink_status(error);
                if (error)
                {
                    return false;
                }

                std::string extension = iterator->path().extension().string();
                std::transform(
                    extension.begin(),
                    extension.end(),
                    extension.begin(),
                    [](unsigned char value)
                    {
                        return static_cast<char>(std::tolower(value));
                    });
                if (Filesystem::is_regular_file(status) && extension == ".dll")
                {
                    return true;
                }
            }
            return false;
        }

        bool TryFindDataDirectory(
            const Filesystem::path& installRoot,
            Filesystem::path& dataRoot)
        {
            const Filesystem::path current = installRoot / "Fall of Avalon_Data";
            if (DirectoryExists(current))
            {
                dataRoot = current;
                return true;
            }
            const Filesystem::path spaced = installRoot / "Tainted Grail_Data";
            if (DirectoryExists(spaced))
            {
                dataRoot = spaced;
                return true;
            }
            const Filesystem::path compact = installRoot / "TaintedGrail_Data";
            if (DirectoryExists(compact))
            {
                dataRoot = compact;
                return true;
            }
            return false;
        }

        bool TryFindManagedDirectory(
            const Filesystem::path& installRoot,
            Filesystem::path& managedRoot)
        {
            Filesystem::path dataRoot;
            if (TryFindDataDirectory(installRoot, dataRoot)
                && DirectoryContainsDll(dataRoot / "Managed"))
            {
                managedRoot = dataRoot / "Managed";
                return true;
            }
            return false;
        }

        bool TryFindIl2CppInteropDirectory(
            const Filesystem::path& installRoot,
            Filesystem::path& interopRoot)
        {
            const Filesystem::path candidate = installRoot / "BepInEx" / "interop";
            if (DirectoryContainsDll(candidate))
            {
                interopRoot = candidate;
                return true;
            }
            return false;
        }

        std::string ReadBoundedBinary(const Filesystem::path& path)
        {
            std::error_code error;
            if (!RegularFileExists(path)
                || Filesystem::file_size(path, error) > MaximumVersionProbeBytes
                || error)
            {
                return {};
            }

            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return {};
            }
            std::string data(
                (std::istreambuf_iterator<char>(stream)),
                std::istreambuf_iterator<char>());
            if (data.size() > MaximumVersionProbeBytes)
            {
                data.resize(MaximumVersionProbeBytes);
            }
            return data;
        }

        AZStd::string ExtractVersionToken(
            const std::string& data,
            const std::regex& pattern)
        {
            std::smatch match;
            if (std::regex_search(data, match, pattern) && match.size() > 1)
            {
                return AZStd::string(match[1].str().c_str());
            }

            std::string collapsed;
            collapsed.reserve(data.size());
            for (char character : data)
            {
                if (character != '\0')
                {
                    collapsed.push_back(character);
                }
            }
            if (std::regex_search(collapsed, match, pattern) && match.size() > 1)
            {
                return AZStd::string(match[1].str().c_str());
            }
            return {};
        }

        AZStd::string FindVersionInFiles(
            const AZStd::vector<Filesystem::path>& paths,
            const std::regex& pattern)
        {
            for (const Filesystem::path& path : paths)
            {
                AZStd::string version = ExtractVersionToken(
                    ReadBoundedBinary(path),
                    pattern);
                if (!version.empty())
                {
                    return version;
                }
            }
            return {};
        }

        const FoARuntimeAdapterRoutes::RouteDescriptor* FindRouteForRuntime(
            const AZStd::string& runtimeTarget)
        {
            const auto& routes = FoARuntimeAdapterRoutes::GetCanonicalRoutes();
            const auto found = AZStd::find_if(
                routes.begin(),
                routes.end(),
                [&runtimeTarget](const FoARuntimeAdapterRoutes::RouteDescriptor& route)
                {
                    return route.m_runtimeTarget == runtimeTarget;
                });
            return found == routes.end() ? nullptr : &*found;
        }

        bool AddCandidate(
            AZStd::vector<AZStd::string>& candidates,
            const AZStd::string& value)
        {
            if (value.empty()
                || AZStd::find(candidates.begin(), candidates.end(), value) != candidates.end())
            {
                return false;
            }
            candidates.push_back(value);
            return true;
        }

        GameProfile SelectEditableProfile(const WorkspaceModel& workspace)
        {
            if (const GameProfile* active = workspace.FindActiveGameProfile())
            {
                return *active;
            }
            if (!workspace.m_gameProfiles.empty())
            {
                return workspace.m_gameProfiles.front();
            }
            return {};
        }

        void PublishEditableProfile(
            WorkspaceModel& workspace,
            const AZStd::string& previousActiveProfileId,
            const GameProfile& profile)
        {
            bool replaced = false;
            for (GameProfile& existing : workspace.m_gameProfiles)
            {
                if (existing.m_profileId == previousActiveProfileId
                    || (!profile.m_profileId.empty()
                        && existing.m_profileId == profile.m_profileId))
                {
                    existing = profile;
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
            {
                workspace.m_gameProfiles.push_back(profile);
            }
            workspace.m_activeGameProfileId = profile.m_profileId;
        }

        AZStd::string ChildPath(const AZStd::string& root, const char* child)
        {
            if (root.empty())
            {
                return {};
            }
            return ToUtf8((FromUtf8(root) / child).lexically_normal());
        }
    } // namespace

    bool LocalSetupDetectionService::LooksLikeTaintedGrailInstall(
        const AZStd::string& installPath)
    {
        Filesystem::path resolved;
        if (!ResolveExistingDirectory(installPath, resolved)
            || IsFilesystemRoot(resolved)
            || HasStorageIndirection(resolved))
        {
            return false;
        }

        Filesystem::path dataRoot;
        return RegularFileExists(resolved / "UnityPlayer.dll")
            || RegularFileExists(resolved / "Fall of Avalon.exe")
            || RegularFileExists(resolved / "TaintedGrail.exe")
            || RegularFileExists(resolved / "Tainted Grail.exe")
            || TryFindDataDirectory(resolved, dataRoot);
    }

    LocalSetupDetectionService::Result LocalSetupDetectionService::Detect(
        const WorkspaceModel& current,
        const Hints& hints) const
    {
        Result result;
        result.m_workspace = current;

        WorkspaceModel& workspace = result.m_workspace;
        SetIfEmpty(workspace.m_workspaceId, DefaultWorkspaceId, result);
        SetIfEmpty(workspace.m_displayName, DefaultWorkspaceName, result);

        if (workspace.m_rootPath.empty() && !hints.m_workspaceRoot.empty())
        {
            const AZStd::string normalized = NormalizeFutureDirectory(hints.m_workspaceRoot);
            SetIfEmpty(workspace.m_rootPath, normalized, result);
            result.m_workspaceRootDetected = !workspace.m_rootPath.empty();
            AddNote(result, "Workspace root filled from local setup hints.");
        }
        if (!workspace.m_rootPath.empty())
        {
            result.m_workspaceRootDetected = true;
            SetIfEmpty(workspace.m_outputPath, ChildPath(workspace.m_rootPath, "Build"), result);
            SetIfEmpty(workspace.m_stagingPath, ChildPath(workspace.m_rootPath, "Staging"), result);
            SetIfEmpty(workspace.m_deploymentPath, ChildPath(workspace.m_rootPath, "Deployment"), result);
        }

        const AZStd::string previousActiveProfileId = workspace.m_activeGameProfileId;
        GameProfile profile = SelectEditableProfile(workspace);
        const bool profileWasConfigured = profile.IsConfigured();
        SetIfEmpty(profile.m_profileId, DefaultMonoProfileId, result);
        SetIfEmpty(profile.m_displayName, DefaultMonoProfileName, result);
        SetIfEmpty(profile.m_branch, "mono", result);
        SetIfEmpty(profile.m_runtimeTarget, "Mono", result);

        AZStd::vector<AZStd::string> installCandidates;
        AddCandidate(installCandidates, profile.m_installPath);
        for (const AZStd::string& candidate : hints.m_installPathCandidates)
        {
            AddCandidate(installCandidates, candidate);
        }

        Filesystem::path installRoot;
        bool hasInstallRoot = !profile.m_installPath.empty()
            && ResolveExistingDirectory(profile.m_installPath, installRoot)
            && LooksLikeTaintedGrailInstall(profile.m_installPath);
        if (!hasInstallRoot)
        {
            for (const AZStd::string& candidate : installCandidates)
            {
                if (LooksLikeTaintedGrailInstall(candidate)
                    && ResolveExistingDirectory(candidate, installRoot))
                {
                    profile.m_installPath = ToUtf8(installRoot);
                    hasInstallRoot = true;
                    result.m_changed = true;
                    AddNote(result, "FoA installation filled from bounded local setup detection.");
                    break;
                }
            }
        }
        result.m_gameInstallDetected = hasInstallRoot;

        SetIfEmpty(profile.m_diagnosticsPath, ChildPath(workspace.m_rootPath, "Diagnostics"), result);
        SetIfEmpty(profile.m_extractedDataPath, ChildPath(workspace.m_rootPath, "Extracted"), result);
        if (profile.m_dlcScopes.empty())
        {
            profile.m_dlcScopes.push_back("base-game");
            result.m_changed = true;
        }

        if (hasInstallRoot)
        {
            Filesystem::path managedRoot;
            Filesystem::path interopRoot;
            const bool hasIl2Cpp = RegularFileExists(installRoot / "GameAssembly.dll");
            const bool hasInterop = TryFindIl2CppInteropDirectory(installRoot, interopRoot);
            const AZStd::string detectedRuntime = hasIl2Cpp ? "IL2CPP" : "Mono";
            const FoARuntimeAdapterRoutes::RouteDescriptor* route =
                FindRouteForRuntime(detectedRuntime);

            if (route)
            {
                SetRouteField(
                    profile.m_profileId,
                    detectedRuntime == "IL2CPP" ? DefaultIl2CppProfileId : DefaultMonoProfileId,
                    profileWasConfigured,
                    result);
                SetRouteField(
                    profile.m_displayName,
                    detectedRuntime == "IL2CPP" ? DefaultIl2CppProfileName : DefaultMonoProfileName,
                    profileWasConfigured,
                    result);
                SetRouteField(profile.m_branch, route->m_branch, profileWasConfigured, result);
                SetRouteField(profile.m_runtimeTarget, route->m_runtimeTarget, profileWasConfigured, result);

                const std::regex gameVersionPattern(R"((\d+\.\d+\.\d+))");
                const AZStd::string observedGameVersion = FindVersionInFiles(
                    {
                        installRoot / "Fall of Avalon.exe",
                        installRoot / "TaintedGrail.exe",
                        installRoot / "Tainted Grail.exe",
                    },
                    gameVersionPattern);
                if (observedGameVersion.empty())
                {
                    AddNote(result, "Game version filled from reviewed route defaults; local executable version was not observed.");
                }
                SetRouteField(
                    profile.m_gameVersion,
                    observedGameVersion.empty() ? route->m_gameVersion : observedGameVersion,
                    profileWasConfigured,
                    result);

                Filesystem::path dataRoot;
                AZStd::vector<Filesystem::path> unityVersionFiles;
                if (TryFindDataDirectory(installRoot, dataRoot))
                {
                    unityVersionFiles.push_back(dataRoot / "globalgamemanagers");
                    unityVersionFiles.push_back(dataRoot / "boot.config");
                }
                const std::regex unityVersionPattern(R"((\d{4}\.\d+\.\d+[abfp]\d+))");
                const AZStd::string observedUnityVersion = FindVersionInFiles(
                    unityVersionFiles,
                    unityVersionPattern);
                if (observedUnityVersion.empty())
                {
                    AddNote(result, "Unity version filled from reviewed route defaults; local Unity version was not observed.");
                }
                SetRouteField(
                    profile.m_unityVersion,
                    observedUnityVersion.empty() ? route->m_unityVersion : observedUnityVersion,
                    profileWasConfigured,
                    result);

                const std::regex bepInExVersionPattern(R"((\d+\.\d+\.\d+(?:[-+._a-zA-Z0-9]*)))");
                const AZStd::string observedBepInExVersion = FindVersionInFiles(
                    {
                        installRoot / "BepInEx" / "core" / "BepInEx.dll",
                        installRoot / "BepInEx" / "core" / "BepInEx.Core.dll",
                        installRoot / "BepInEx" / "core" / "BepInEx.Unity.IL2CPP.dll",
                    },
                    bepInExVersionPattern);
                if (observedBepInExVersion.empty())
                {
                    AddNote(result, "BepInEx version filled from reviewed route defaults; local BepInEx version was not observed.");
                }
                SetRouteField(
                    profile.m_bepInExVersion,
                    observedBepInExVersion.empty() ? route->m_bepInExVersion : observedBepInExVersion,
                    profileWasConfigured,
                    result);
            }

            if (detectedRuntime == "IL2CPP")
            {
                if (hasInterop)
                {
                    SetRouteField(profile.m_managedAssembliesPath, ToUtf8(interopRoot), profileWasConfigured, result);
                }
                else if (TryFindManagedDirectory(installRoot, managedRoot))
                {
                    SetRouteField(profile.m_managedAssembliesPath, ToUtf8(managedRoot), profileWasConfigured, result);
                }
                if (!profile.m_pluginPath.empty() && !profileWasConfigured)
                {
                    profile.m_pluginPath.clear();
                    result.m_changed = true;
                }
            }
            else
            {
                if (TryFindManagedDirectory(installRoot, managedRoot))
                {
                    SetRouteField(profile.m_managedAssembliesPath, ToUtf8(managedRoot), profileWasConfigured, result);
                }
                const Filesystem::path pluginRoot = installRoot / "BepInEx" / "plugins";
                if (DirectoryExists(pluginRoot))
                {
                    SetRouteField(profile.m_pluginPath, ToUtf8(pluginRoot), profileWasConfigured, result);
                }
            }
        }
        else
        {
            AddNote(result, "No bounded FoA install candidate was detected.");
        }

        result.m_gameProfileComplete = profile.IsConfigured();
        PublishEditableProfile(workspace, previousActiveProfileId, profile);
        return result;
    }
} // namespace TaintedGrailModdingSDK
