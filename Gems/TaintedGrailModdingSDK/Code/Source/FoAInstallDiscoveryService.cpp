/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoAInstallDiscoveryService.h"

#include <AzCore/std/algorithm.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <system_error>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        namespace Filesystem = std::filesystem;

        constexpr const char* SteamAppId = "1466060";
        constexpr size_t MaximumSteamMetadataBytes = 4 * 1024 * 1024;
        constexpr size_t MaximumSteamRoots = 64;

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

        bool DirectoryExists(const Filesystem::path& path)
        {
            std::error_code error;
            return Filesystem::exists(path, error)
                && !error
                && Filesystem::is_directory(path, error)
                && !error;
        }

        bool RegularFileExists(const Filesystem::path& path)
        {
            std::error_code error;
            return Filesystem::exists(path, error)
                && !error
                && Filesystem::is_regular_file(path, error)
                && !error;
        }

        std::string ReadBoundedText(const Filesystem::path& path)
        {
            std::error_code error;
            if (!RegularFileExists(path))
            {
                return {};
            }
            const auto size = Filesystem::file_size(path, error);
            if (error || size == 0 || size > MaximumSteamMetadataBytes)
            {
                return {};
            }

            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return {};
            }
            return std::string(
                (std::istreambuf_iterator<char>(stream)),
                std::istreambuf_iterator<char>());
        }

        void AddUniquePath(
            AZStd::vector<AZStd::string>& paths,
            const Filesystem::path& value,
            bool requireDirectory)
        {
            if (value.empty())
            {
                return;
            }

            std::error_code error;
            Filesystem::path normalized = Filesystem::absolute(value, error).lexically_normal();
            if (error || normalized.empty())
            {
                return;
            }
            normalized = Filesystem::weakly_canonical(normalized, error);
            if (error || (requireDirectory && !DirectoryExists(normalized)))
            {
                return;
            }

            const AZStd::string text = ToUtf8(normalized);
            if (AZStd::find(paths.begin(), paths.end(), text) == paths.end())
            {
                paths.push_back(text);
            }
        }

        AZStd::string EnvironmentPath(const char* name)
        {
            const char* value = std::getenv(name);
            return value && *value ? AZStd::string(value) : AZStd::string{};
        }

        bool IsSafeInstallDirectoryName(const std::string& value)
        {
            if (value.empty() || value.size() > 260 || value == "." || value == "..")
            {
                return false;
            }
            return value.find('/') == std::string::npos
                && value.find('\\') == std::string::npos
                && value.find(':') == std::string::npos;
        }

        void AddLibraryRootsFromVdf(
            const Filesystem::path& steamRoot,
            AZStd::vector<AZStd::string>& libraryRoots)
        {
            AddUniquePath(libraryRoots, steamRoot, true);

            const std::string vdf = ReadBoundedText(
                steamRoot / "steamapps" / "libraryfolders.vdf");
            if (vdf.empty())
            {
                return;
            }

            const std::regex pathPattern(R"("path"\s+"([^"]+)")", std::regex::icase);
            size_t inspected = 0;
            for (std::sregex_iterator it(vdf.begin(), vdf.end(), pathPattern), end;
                 it != end && inspected < MaximumSteamRoots;
                 ++it, ++inspected)
            {
                std::string pathText = (*it)[1].str();
                std::string decoded;
                decoded.reserve(pathText.size());
                for (size_t index = 0; index < pathText.size(); ++index)
                {
                    if (pathText[index] == '\\'
                        && index + 1 < pathText.size()
                        && pathText[index + 1] == '\\')
                    {
                        decoded.push_back('\\');
                        ++index;
                    }
                    else
                    {
                        decoded.push_back(pathText[index]);
                    }
                }
                AddUniquePath(libraryRoots, Filesystem::path(decoded), true);
            }
        }

        void AddInstallCandidateFromLibrary(
            const Filesystem::path& libraryRoot,
            FoAInstallDiscoveryService::Result& result)
        {
            const Filesystem::path steamApps = libraryRoot / "steamapps";
            const Filesystem::path manifest =
                steamApps / (AZStd::string("appmanifest_") + SteamAppId + ".acf").c_str();
            const std::string manifestText = ReadBoundedText(manifest);
            if (!manifestText.empty())
            {
                const std::regex installDirPattern(
                    R"("installdir"\s+"([^"]+)")",
                    std::regex::icase);
                std::smatch match;
                if (std::regex_search(manifestText, match, installDirPattern)
                    && match.size() > 1
                    && IsSafeInstallDirectoryName(match[1].str()))
                {
                    AddUniquePath(
                        result.m_installPathCandidates,
                        steamApps / "common" / match[1].str(),
                        true);
                }
            }

            // Bounded fallbacks cover incomplete Steam metadata while still requiring the
            // candidate directory to exist. LocalSetupDetectionService performs the product
            // validation before any candidate becomes profile truth.
            const char* fallbackNames[] = {
                "Tainted Grail The Fall of Avalon",
                "Tainted Grail - The Fall of Avalon",
                "Fall of Avalon",
            };
            for (const char* fallbackName : fallbackNames)
            {
                AddUniquePath(
                    result.m_installPathCandidates,
                    steamApps / "common" / fallbackName,
                    true);
            }
        }
    } // namespace

    FoAInstallDiscoveryService::Result FoAInstallDiscoveryService::Discover() const
    {
        AZStd::vector<AZStd::string> steamRoots;
        const AZStd::string explicitSteamRoot = EnvironmentPath("STEAM_PATH");
        if (!explicitSteamRoot.empty())
        {
            steamRoots.push_back(explicitSteamRoot);
        }

        const AZStd::string programFilesX86 = EnvironmentPath("PROGRAMFILES(X86)");
        if (!programFilesX86.empty())
        {
            steamRoots.push_back(ToUtf8(FromUtf8(programFilesX86) / "Steam"));
        }
        const AZStd::string programFiles = EnvironmentPath("PROGRAMFILES");
        if (!programFiles.empty())
        {
            steamRoots.push_back(ToUtf8(FromUtf8(programFiles) / "Steam"));
        }

        Result result = DiscoverFromSteamRoots(steamRoots);
        if (!result.m_installPathCandidates.empty())
        {
            result.m_notes.push_back("Fall of Avalon installation discovered from local Steam metadata.");
        }
        return result;
    }

    FoAInstallDiscoveryService::Result FoAInstallDiscoveryService::DiscoverFromSteamRoots(
        const AZStd::vector<AZStd::string>& steamRoots)
    {
        Result result;
        AZStd::vector<AZStd::string> libraryRoots;
        size_t inspectedRoots = 0;
        for (const AZStd::string& steamRoot : steamRoots)
        {
            if (++inspectedRoots > MaximumSteamRoots)
            {
                break;
            }
            AddLibraryRootsFromVdf(FromUtf8(steamRoot), libraryRoots);
        }

        size_t inspectedLibraries = 0;
        for (const AZStd::string& libraryRoot : libraryRoots)
        {
            if (++inspectedLibraries > MaximumSteamRoots)
            {
                break;
            }
            AddInstallCandidateFromLibrary(FromUtf8(libraryRoot), result);
        }
        return result;
    }
} // namespace TaintedGrailModdingSDK
