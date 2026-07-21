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
    constexpr wchar_t ManifestFileName[] = L"INSTALL_MANIFEST.json";
    constexpr wchar_t SelfTestArgument[] = L"--self-test";

    int Fail(const std::wstring& message)
    {
        MessageBoxW(nullptr, message.c_str(), ProductName, MB_OK | MB_ICONERROR);
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
        std::filesystem::path& project,
        std::wstring& error)
    {
        std::filesystem::path executable;
        if (!ResolveExecutable(executable, error))
        {
            return false;
        }

        binaryDirectory = executable.parent_path();
        std::filesystem::path installRoot = binaryDirectory;
        for (int parent = 0; parent < 4; ++parent)
        {
            if (!installRoot.has_parent_path() || installRoot == installRoot.parent_path())
            {
                error = L"The launcher is not inside the expected O3DE SDK binary layout.";
                return false;
            }
            installRoot = installRoot.parent_path();
        }

        editor = binaryDirectory / EditorFileName;
        project = installRoot / ProjectDirectoryName;
        if (!IsRegularFile(installRoot / ManifestFileName))
        {
            error = L"The installed SDK manifest is missing. Repair or reinstall the SDK.";
            return false;
        }
        if (!IsRegularFile(editor))
        {
            error = L"The installed Editor.exe is missing. Repair or reinstall the SDK.";
            return false;
        }
        if (!IsDirectory(project) || !IsRegularFile(project / L"project.json"))
        {
            error = L"The installed Tainted Grail editor project is missing. Repair or reinstall the SDK.";
            return false;
        }
        return true;
    }
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int)
{
    std::filesystem::path binaryDirectory;
    std::filesystem::path editor;
    std::filesystem::path project;
    std::wstring error;
    if (!ResolveInstalledLayout(binaryDirectory, editor, project, error))
    {
        return Fail(error);
    }

    const std::wstring extraArguments = commandLine ? commandLine : L"";
    if (extraArguments == SelfTestArgument)
    {
        return 0;
    }

    std::wstring editorCommand = QuoteArgument(editor) + L" --project-path " + QuoteArgument(project);
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
        return Fail(L"Unable to launch the installed Editor: " + WindowsError(GetLastError()));
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
