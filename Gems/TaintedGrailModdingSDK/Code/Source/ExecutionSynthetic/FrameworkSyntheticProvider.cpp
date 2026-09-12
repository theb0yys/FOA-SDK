/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Windows.h>
#include <string>
// This harmless provider understands only the repository-owned synthetic format.
// Paths are supplied as typed M2 input/output arguments inside its LPAC sandbox.
namespace
{

    std::wstring Environment(const wchar_t* key)
    {
        DWORD length = GetEnvironmentVariableW(key, nullptr, 0);
        if (!length || length > 32768)
            return {};
        std::wstring value(length, L'\0');
        if (GetEnvironmentVariableW(key, value.data(), length) != length - 1)
            return {};
        value.resize(length - 1);
        return value;
    }
    std::string Token(const wchar_t* key)
    {
        auto wide = Environment(key);
        if (wide.empty() || wide.size() > 128)
            return {};
        std::string token;
        for (auto c : wide)
        {
            if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'.' || c == L'-' || c == L'_' ||
                  c == L':'))
                return {};
            token += static_cast<char>(c);
        }
        return token;
    }
    std::string FixtureDigest(const std::string& bytes)
    {
        // V1 permits exactly these payloads. M2 independently hashes the actual files.
        struct Known
        {
            const char* bytes;
            const char* digest;
        };
        const Known known[] = {
            { "FOA-SYNTHETIC-V1\nVALUE=42\n", "sha256:80f000be81eb0043c5286298a12a7d39adebf4dd9332aacfbfe800a0d33732a7" },
            { "FOA-PACKAGE-V1\nFOA-SYNTHETIC-V1\nVALUE=42\n", "sha256:a3619f49ff73b29332a0e08e6555100d6253aa1acfe60cd54b7781e49978e732" },
            { "observed=42\n", "sha256:a26962b728027096822c1d95761f8b8ca292a59b5663de6af89a79f59dc8331e" },
            { "verified=42\n", "sha256:ce0a200a732e09452926cac57820e302ba0122b993313197a36290c9e0c2cef7" },
            { "FOA synthetic baseline v1\n", "sha256:a8e8ec64dc5e1d7e234279b6926422b5f1450612b72d420f2bb971ae0adfe601" }
        };
        for (const auto& value : known)
            if (bytes == value.bytes)
                return value.digest;
        return {};
    }

    bool Read(const wchar_t* path, std::string& data)
    {
        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        LARGE_INTEGER size{};
        DWORD bytes = 0;
        bool ok = GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart <= 65536;
        if (ok)
        {
            data.resize(static_cast<size_t>(size.QuadPart));
            ok = ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &bytes, nullptr) && bytes == data.size();
        }
        CloseHandle(file);
        return ok;
    }
    bool Write(const wchar_t* path, const std::string& data)
    {
        HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        DWORD bytes = 0;
        bool ok = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &bytes, nullptr) && bytes == data.size() &&
            FlushFileBuffers(file);
        CloseHandle(file);
        return ok;
    }
} // namespace
int wmain(int argc, wchar_t** argv)
{
    if (argc < 5 || argc > 6)
        return 2;
    if (wcscmp(argv[argc - 1], L"fail") == 0)
        return 37;
    if (wcscmp(argv[argc - 1], L"hang") == 0)
        Sleep(INFINITE);
    if (wcscmp(argv[argc - 1], L"delay") == 0)
        Sleep(1000); // Bounded synthetic admission-revocation fixture.
    else if (wcscmp(argv[argc - 1], L"normal") != 0)
        return 3;
    std::string input, second, output;
    if (!Read(argv[2], input))
        return 4;
    const std::wstring verb = argv[1];
    if (verb == L"build" && argc == 5 && input == "value:42\n")
        output = "FOA-SYNTHETIC-V1\nVALUE=42\n";
    else if (verb == L"package" && argc == 5 && input == "FOA-SYNTHETIC-V1\nVALUE=42\n")
        output = "FOA-PACKAGE-V1\n" + input;
    else if (verb == L"deploy" && argc == 5 && input == "FOA-PACKAGE-V1\nFOA-SYNTHETIC-V1\nVALUE=42\n")
        output = input.substr(15);
    else if (verb == L"launch" && argc == 5)
    {
        // Interpret the installed synthetic document; no general command/script execution.
        if (input != "FOA-SYNTHETIC-V1\nVALUE=42\n")
            return 5;
        unsigned value = 0;
        for (size_t i = input.find("VALUE=") + 6; i < input.size() && input[i] != '\n'; ++i)
        {
            if (input[i] < '0' || input[i] > '9')
                return 6;
            value = value * 10 + static_cast<unsigned>(input[i] - '0');
        }
        output = "observed=" + std::to_string(value) + "\n";
    }
    else if (verb == L"verify" && argc == 6 && Read(argv[3], second))
    {
        // Independent of the launch parser: compare exact expected source and observation bytes.
        if (input != "FOA-SYNTHETIC-V1\nVALUE=42\n" || second != "observed=42\n")
            return 7;
        output = "verified=42\n";
    }
    else if (verb == L"rollback" && argc == 5 && input == "FOA synthetic baseline v1\n")
        output = input;
    else
        return 8;
    if (!Write(argv[argc - 2], output))
        return 9;
    const auto attempt = Token(L"FOA_INVOCATION_ID"), request = Token(L"FOA_REQUEST_FINGERPRINT"), digest = FixtureDigest(output);
    const auto manifestPath = Environment(L"FOA_OUTPUT_MANIFEST");
    if (attempt.empty() || request.empty() || digest.empty() || manifestPath.empty())
        return 10;
    const char* relative = verb == L"package" ? "package.txt"
        : verb == L"launch"                   ? "result.txt"
        : verb == L"verify"                   ? "verified.txt"
        : verb == L"rollback"                 ? "baseline.txt"
                                              : "payload.txt";
    const std::string manifest = "{\"contract\":\"foa-tool-output-manifest-v2\",\"version\":2,\"attempt\":\"" + attempt +
        "\",\"requestFingerprint\":\"" + request + "\",\"outputs\":[{\"id\":\"output\",\"root\":\"output\",\"path\":\"" + relative +
        "\",\"kind\":\"text/plain\",\"sha256\":\"" + digest + "\",\"bytes\":" + std::to_string(output.size()) + "}]}";
    return Write(manifestPath.c_str(), manifest) ? 0 : 11;
}
