/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
    constexpr wchar_t ProductName[] = L"Tainted Grail Modding Editor";
    constexpr wchar_t EditorFileName[] = L"Editor.exe";
    constexpr wchar_t ProjectDirectoryName[] = L"TaintedGrailModdingEditor";
    constexpr wchar_t InstalledBinRelativePath[] = L"bin\\Windows\\profile\\Default";
    constexpr wchar_t StartupLevelRelativePath[] = L"Levels\\DefaultLevel\\DefaultLevel.prefab";
    constexpr wchar_t ManifestFileName[] = L"INSTALL_MANIFEST.json";
    constexpr wchar_t EngineMetadataFileName[] = L"engine.json";
    constexpr wchar_t BundledCMakeBinRelativePath[] = L"cmake\\runtime\\bin";
    constexpr wchar_t BundledCMakeFileName[] = L"cmake.exe";
    constexpr wchar_t InstalledUserDataRelativePath[] = L"O3DE\\TGEditor\\installed";
    constexpr wchar_t MaterializedProjectDirectoryName[] = L"project";
    constexpr wchar_t ExternalDirectoryName[] = L"External";
    constexpr wchar_t ProjectRegistryRelativePath[] = L"user\\Registry";
    constexpr wchar_t AssetProcessorSettingsFileName[] = L"asset_processor.setreg";
    constexpr wchar_t SelfTestArgument[] = L"--self-test";

    int Fail(const std::wstring& message, bool showDialog)
    {
        if (showDialog)
        {
            MessageBoxW(nullptr, message.c_str(), ProductName, MB_OK | MB_ICONERROR);
        }
        return 1;
    }

    std::wstring WindowsError(DWORD error)
    {
        wchar_t* buffer = nullptr;
        const DWORD length = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            0,
            reinterpret_cast<wchar_t*>(&buffer),
            0,
            nullptr);
        std::wstring message = length && buffer ? std::wstring(buffer, length) : L"unknown Windows error";
        if (buffer)
        {
            LocalFree(buffer);
        }
        return message;
    }

    bool ResolveExecutable(std::filesystem::path& executable, std::wstring& error)
    {
        std::vector<wchar_t> buffer(1024);
        for (;;)
        {
            SetLastError(ERROR_SUCCESS);
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                error = L"Unable to locate the installed launcher: " + WindowsError(GetLastError());
                return false;
            }
            if (length < buffer.size() - 1)
            {
                executable = std::filesystem::path(std::wstring(buffer.data(), length));
                return true;
            }
            if (buffer.size() >= 32768)
            {
                error = L"The installed launcher path exceeds the Windows path limit.";
                return false;
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    bool IsRegularFile(const std::filesystem::path& path)
    {
        std::error_code error;
        return std::filesystem::is_regular_file(path, error) && !error;
    }

    bool IsDirectory(const std::filesystem::path& path)
    {
        std::error_code error;
        return std::filesystem::is_directory(path, error) && !error;
    }

    bool ResolveLocalAppData(std::filesystem::path& localAppData, std::wstring& error)
    {
        const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        if (length == 0)
        {
            error = L"Unable to locate the Windows local application data directory.";
            return false;
        }

        std::vector<wchar_t> buffer(length);
        const DWORD copied = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), length);
        if (copied == 0 || copied >= length)
        {
            error = L"Unable to read the Windows local application data directory.";
            return false;
        }

        localAppData = std::filesystem::path(std::wstring(buffer.data(), copied));
        return true;
    }

    bool EnsureDirectory(const std::filesystem::path& path, const wchar_t* label, std::wstring& error)
    {
        std::error_code createError;
        std::filesystem::create_directories(path, createError);
        if (createError || !IsDirectory(path))
        {
            error = L"Unable to create the installed Editor ";
            error += label;
            error += L" directory: ";
            error += path.native();
            return false;
        }
        return true;
    }

    bool MaterializeInstalledDirectory(
        const std::filesystem::path& source,
        const std::filesystem::path& target,
        const wchar_t* label,
        std::wstring& error)
    {
        if (!IsDirectory(source))
        {
            error = L"The installed Editor ";
            error += label;
            error += L" directory is missing. Repair or reinstall the SDK.";
            return false;
        }
        if (!EnsureDirectory(target.parent_path(), L"user root", error))
        {
            return false;
        }

        std::error_code copyError;
        std::filesystem::copy(
            source,
            target,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
            copyError);
        if (copyError || !IsDirectory(target))
        {
            error = L"Unable to materialize the installed Editor ";
            error += label;
            error += L" directory: ";
            error += target.native();
            return false;
        }
        return true;
    }

    bool MaterializeInstalledProject(
        const std::filesystem::path& installedProject,
        const std::filesystem::path& launchProject,
        std::wstring& error)
    {
        if (!IsRegularFile(installedProject / ProjectRegistryRelativePath / AssetProcessorSettingsFileName))
        {
            error = L"The installed Editor asset-processor registry seed is missing. Repair or reinstall the SDK.";
            return false;
        }
        if (!MaterializeInstalledDirectory(installedProject, launchProject, L"project", error))
        {
            return false;
        }
        if (!IsRegularFile(launchProject / L"project.json"))
        {
            error = L"Unable to materialize the installed Editor project: ";
            error += launchProject.native();
            return false;
        }
        if (!IsRegularFile(launchProject / StartupLevelRelativePath))
        {
            error = L"The materialized installed Editor default level is missing.";
            return false;
        }
        return true;
    }

    bool ResolveWritableLaunchPaths(
        const std::filesystem::path& installedProject,
        std::filesystem::path& launchProject,
        std::filesystem::path& cachePath,
        std::filesystem::path& userPath,
        std::filesystem::path& logPath,
        std::filesystem::path& startupLevel,
        std::wstring& error)
    {
        std::filesystem::path localAppData;
        if (!ResolveLocalAppData(localAppData, error))
        {
            return false;
        }

        const std::filesystem::path installedUserRoot = localAppData / InstalledUserDataRelativePath;
        launchProject = installedUserRoot / MaterializedProjectDirectoryName;
        if (!MaterializeInstalledDirectory(
                installedProject.parent_path() / ExternalDirectoryName,
                installedUserRoot / ExternalDirectoryName,
                L"External",
                error))
        {
            return false;
        }
        if (!MaterializeInstalledProject(installedProject, launchProject, error))
        {
            return false;
        }

        cachePath = launchProject / L"Cache";
        userPath = launchProject / L"user";
        logPath = userPath / L"log";
        const std::filesystem::path registryPath = userPath / L"Registry";
        if (!EnsureDirectory(cachePath, L"cache", error) || !EnsureDirectory(logPath, L"log", error)
            || !EnsureDirectory(registryPath, L"registry", error))
        {
            return false;
        }

        const std::filesystem::path installedRegistry = registryPath / AssetProcessorSettingsFileName;
        if (!IsRegularFile(installedRegistry))
        {
            error = L"Unable to seed the installed Editor asset-processor registry: ";
            error += installedRegistry.native();
            return false;
        }
        startupLevel = launchProject / StartupLevelRelativePath;
        return true;
    }

    bool ReadEnvironmentVariable(const wchar_t* name, std::wstring& value, std::wstring& error)
    {
        const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
        if (length == 0)
        {
            const DWORD lastError = GetLastError();
            if (lastError == ERROR_ENVVAR_NOT_FOUND)
            {
                value.clear();
                return true;
            }
            error = L"Unable to read the ";
            error += name;
            error += L" environment variable: ";
            error += WindowsError(lastError);
            return false;
        }

        std::vector<wchar_t> buffer(length);
        const DWORD copied = GetEnvironmentVariableW(name, buffer.data(), length);
        if (copied == 0 || copied >= length)
        {
            error = L"Unable to read the ";
            error += name;
            error += L" environment variable.";
            return false;
        }
        value.assign(buffer.data(), copied);
        return true;
    }

    bool ConfigureBundledRuntimeEnvironment(const std::filesystem::path& installRoot, std::wstring& error)
    {
        const std::filesystem::path cmakeBin = installRoot / BundledCMakeBinRelativePath;
        const std::filesystem::path cmakeExecutable = cmakeBin / BundledCMakeFileName;
        if (!IsRegularFile(cmakeExecutable))
        {
            error = L"The installed SDK CMake runtime is missing. Repair or reinstall the SDK.";
            return false;
        }

        std::wstring pathValue;
        if (!ReadEnvironmentVariable(L"PATH", pathValue, error))
        {
            return false;
        }

        const std::wstring cmakeBinValue = cmakeBin.native();
        const std::wstring updatedPath = pathValue.empty() ? cmakeBinValue : cmakeBinValue + L";" + pathValue;
        if (!SetEnvironmentVariableW(L"PATH", updatedPath.c_str()))
        {
            error = L"Unable to configure the installed SDK runtime PATH: " + WindowsError(GetLastError());
            return false;
        }
        if (!SetEnvironmentVariableW(L"LY_CMAKE_PATH", cmakeBinValue.c_str()))
        {
            error = L"Unable to configure the installed SDK CMake path: " + WindowsError(GetLastError());
            return false;
        }
        return true;
    }

    std::wstring QuoteArgument(const std::filesystem::path& value)
    {
        std::wstring result = L"\"";
        size_t backslashes = 0;
        for (const wchar_t character : value.native())
        {
            if (character == L'\\')
            {
                ++backslashes;
                continue;
            }
            if (character == L'\"')
            {
                result.append(backslashes * 2 + 1, L'\\');
                result.push_back(character);
                backslashes = 0;
                continue;
            }
            result.append(backslashes, L'\\');
            backslashes = 0;
            result.push_back(character);
        }
        result.append(backslashes * 2, L'\\');
        result.push_back(L'\"');
        return result;
    }

    bool ResolveInstalledLayout(
        std::filesystem::path& binaryDirectory,
        std::filesystem::path& editor,
        std::filesystem::path& engineRoot,
        std::filesystem::path& project,
        std::filesystem::path& startupLevel,
        std::wstring& error)
    {
        std::filesystem::path executable;
        if (!ResolveExecutable(executable, error))
        {
            return false;
        }

        const std::filesystem::path launcherDirectory = executable.parent_path();
        std::vector<std::filesystem::path> installRootCandidates;
        std::filesystem::path candidate = launcherDirectory;
        for (int depth = 0; depth < 8; ++depth)
        {
            installRootCandidates.push_back(candidate);
            if (!candidate.has_parent_path() || candidate == candidate.parent_path())
            {
                break;
            }
            candidate = candidate.parent_path();
        }

        std::filesystem::path installRoot;
        for (const std::filesystem::path& rootCandidate : installRootCandidates)
        {
            if (IsRegularFile(rootCandidate / ManifestFileName)
                && IsDirectory(rootCandidate / ProjectDirectoryName)
                && IsRegularFile(rootCandidate / ProjectDirectoryName / L"project.json"))
            {
                installRoot = rootCandidate;
                break;
            }
        }

        if (installRoot.empty())
        {
            error = L"The launcher is not inside a self-contained FOA-SDK install. Repair or reinstall the SDK.";
            return false;
        }
        if (!IsRegularFile(installRoot / EngineMetadataFileName))
        {
            error = L"The installed O3DE engine metadata is missing. Repair or reinstall the SDK.";
            return false;
        }

        engineRoot = installRoot;
        project = installRoot / ProjectDirectoryName;
        startupLevel = project / StartupLevelRelativePath;
        const std::vector<std::filesystem::path> editorCandidates = {
            launcherDirectory / EditorFileName,
            installRoot / InstalledBinRelativePath / EditorFileName,
        };
        for (const std::filesystem::path& editorCandidate : editorCandidates)
        {
            if (IsRegularFile(editorCandidate))
            {
                editor = editorCandidate;
                binaryDirectory = editor.parent_path();
                break;
            }
        }
        if (editor.empty())
        {
            error = L"The installed Editor.exe is missing. Repair or reinstall the SDK.";
            return false;
        }
        if (!IsDirectory(project) || !IsRegularFile(project / L"project.json"))
        {
            error = L"The installed Tainted Grail editor project is missing. Repair or reinstall the SDK.";
            return false;
        }
        if (!IsRegularFile(startupLevel))
        {
            error = L"The installed default Editor level is missing. Repair or reinstall the SDK.";
            return false;
        }
        return true;
    }
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int)
{
    std::filesystem::path binaryDirectory;
    std::filesystem::path editor;
    std::filesystem::path engineRoot;
    std::filesystem::path installedProject;
    std::filesystem::path launchProject;
    std::filesystem::path startupLevel;
    std::filesystem::path cachePath;
    std::filesystem::path userPath;
    std::filesystem::path logPath;
    std::wstring error;
    const std::wstring extraArguments = commandLine ? commandLine : L"";
    const bool selfTest = extraArguments == SelfTestArgument;
    if (!ResolveInstalledLayout(binaryDirectory, editor, engineRoot, installedProject, startupLevel, error))
    {
        return Fail(error, !selfTest);
    }
    if (!ConfigureBundledRuntimeEnvironment(engineRoot, error))
    {
        return Fail(error, !selfTest);
    }
    if (!ResolveWritableLaunchPaths(installedProject, launchProject, cachePath, userPath, logPath, startupLevel, error))
    {
        return Fail(error, !selfTest);
    }

    if (selfTest)
    {
        return 0;
    }

    std::wstring editorCommand =
        QuoteArgument(editor) + L" --engine-path " + QuoteArgument(engineRoot) + L" --project-path "
        + QuoteArgument(launchProject) + L" --project-cache-path " + QuoteArgument(cachePath) + L" --project-user-path "
        + QuoteArgument(userPath) + L" --project-log-path " + QuoteArgument(logPath) + L" "
        + QuoteArgument(startupLevel);
    if (!extraArguments.empty())
    {
        editorCommand += L" " + extraArguments;
    }
    std::vector<wchar_t> mutableCommand(editorCommand.begin(), editorCommand.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(
            editor.c_str(),
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            binaryDirectory.c_str(),
            &startup,
            &process))
    {
        return Fail(L"Unable to launch the installed Editor: " + WindowsError(GetLastError()), true);
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
