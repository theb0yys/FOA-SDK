/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

// Native synthetic provider. Never linked into an Editor or runtime target.
#include <Windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <string>
#include <vector>
#include <winsock2.h>

namespace
{
    std::wstring Env(const wchar_t* key)
    {
        DWORD size = GetEnvironmentVariableW(key, nullptr, 0);
        if (!size || size > 32768)
        {
            return {};
        }
        std::wstring v(size, L'\0');
        GetEnvironmentVariableW(key, v.data(), size);
        v.resize(size - 1);
        return v;
    }
    std::string Utf8(const std::wstring& w)
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        std::string s(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
        return s;
    }
    bool Write(const std::wstring& path, const std::string& bytes)
    {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        DWORD n = 0;
        bool ok = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &n, nullptr) && n == bytes.size();
        CloseHandle(h);
        return ok;
    }
    std::string Read(const std::wstring& path)
    {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            return {};
        }
        char bytes[8192];
        DWORD n = 0;
        bool ok = ReadFile(h, bytes, sizeof(bytes), &n, nullptr);
        CloseHandle(h);
        return ok ? std::string(bytes, n) : std::string{};
    }
    std::string Digest(const std::string& bytes)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        {
            return {};
        }
        DWORD size = 0, n = 0;
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&size), sizeof(size), &n, 0);
        std::vector<unsigned char> object(size);
        unsigned char digest[32]{};
        bool ok = BCryptCreateHash(algorithm, &hash, object.data(), size, nullptr, 0, 0) >= 0 &&
            BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())), static_cast<ULONG>(bytes.size()), 0) >= 0 &&
            BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
        if (hash)
        {
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (!ok)
        {
            return {};
        }
        const char hex[] = "0123456789abcdef";
        std::string result = "sha256:";
        for (auto c : digest)
        {
            result += hex[c >> 4];
            result += hex[c & 15];
        }
        return result;
    }
    std::string Hex(const std::wstring& value)
    {
        const char hex[] = "0123456789abcdef";
        std::string result;
        for (unsigned char c : Utf8(value))
        {
            result += hex[c >> 4];
            result += hex[c & 15];
        }
        return result;
    }
    LONG WINAPI EndSyntheticCrash(EXCEPTION_POINTERS*)
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    bool CanReadPrivateHandle(HANDLE handle)
    {
        __try
        {
            char bytes[32];
            DWORD count = 0;
            return GetFileType(handle) == FILE_TYPE_DISK && ReadFile(handle, bytes, sizeof(bytes), &count, nullptr) && count == 14 &&
                memcmp(bytes, "private-canary", 14) == 0;
        } __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }
    void Print(HANDLE stream, const std::string& value)
    {
        DWORD n = 0;
        WriteFile(stream, value.data(), static_cast<DWORD>(value.size()), &n, nullptr);
    }
} // namespace
int wmain(int argc, wchar_t** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    std::wstring mode = argc > 1 ? argv[1] : L"success";
    std::string payload = "hello\n";
    if (mode == L"heartbeat-hang")
    {
        if (!Write(L"live.txt", "running"))
        {
            return 46;
        }
        for (;;)
        {
            Sleep(1000);
        }
    }
    if (mode == L"hang")
    {
        for (;;)
        {
            Sleep(1000);
        }
    }
    if (mode == L"exit")
    {
        return 17;
    }
    if (mode == L"crash")
    {
        SetUnhandledExceptionFilter(EndSyntheticCrash);
        volatile int* invalid = nullptr;
        *invalid = 1;
        return 18;
    }
    if (mode == L"slow")
    {
        Sleep(1500);
    }
    if (mode == L"memory-limit")
    {
        std::vector<void*> allocations;
        size_t total = 0;
        for (unsigned i = 0; i < 8; ++i)
        {
            void* block = VirtualAlloc(nullptr, 32 * 1024 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (!block)
            {
                break;
            }
            allocations.push_back(block);
            total += 32 * 1024 * 1024;
        }
        for (void* block : allocations)
        {
            VirtualFree(block, 0, MEM_RELEASE);
        }
        payload = "allocated=" + std::to_string(total) + "\n";
    }
    if (mode == L"flood")
    {
        std::string chunk(8192, 'x');
        for (unsigned i = 0; i < 20000; ++i)
        {
            Print(GetStdHandle(STD_OUTPUT_HANDLE), chunk);
            Print(GetStdHandle(STD_ERROR_HANDLE), chunk);
        }
    }
    if (mode == L"child")
    {
        wchar_t executable[32768];
        GetModuleFileNameW(nullptr, executable, 32768);
        std::wstring line = L"\"" + std::wstring(executable) + L"\" hang";
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        HANDLE image = CreateFileW(executable, GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (image == INVALID_HANDLE_VALUE)
        {
            Print(GetStdHandle(STD_ERROR_HANDLE), "child-image-error=" + std::to_string(GetLastError()) + "\n");
        }
        else
        {
            CloseHandle(image);
        }
        if (!CreateProcessW(executable, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            Print(GetStdHandle(STD_ERROR_HANDLE), "child-create-error=" + std::to_string(GetLastError()) + "\n");
            return 31;
        }
        Print(GetStdHandle(STD_OUTPUT_HANDLE), "child=" + std::to_string(pi.dwProcessId) + "\n");
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }
    if (mode == L"argv")
    {
        payload.clear();
        for (int i = 2; i < argc; ++i)
        {
            payload += std::to_string(i - 2) + ":" + Hex(argv[i]) + "\n";
        }
    }
    if (mode == L"environment")
    {
        if (!Env(L"PATH").empty() || !Env(L"COMSPEC").empty() || !Env(L"FOA_PARENT_SECRET").empty())
        {
            return 32;
        }
        if (Env(L"FIXTURE_VALUE") != L"expected")
        {
            return 33;
        }
        wchar_t cwd[32768];
        GetCurrentDirectoryW(32768, cwd);
        Print(GetStdHandle(STD_OUTPUT_HANDLE), "working=" + Utf8(cwd) + "\n");
        payload = "environment-isolated\n";
    }
    if (mode == L"network")
    {
        WSADATA data{};
        int startupError = WSAStartup(MAKEWORD(2, 2), &data);
        if (startupError)
        {
            Print(GetStdHandle(STD_ERROR_HANDLE), "network-startup-error=" + std::to_string(startupError) + "\n");
            return 34;
        }
        SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        int error = WSAGetLastError();
        if (socketHandle != INVALID_SOCKET)
        {
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(9);
            int result = connect(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address));
            error = result == 0 ? 0 : WSAGetLastError();
            closesocket(socketHandle);
        }
        WSACleanup();
        if (error != WSAEACCES)
        {
            return 35;
        }
        payload = "network-denied\n";
    }
    if (mode == L"write-probe")
    {
        if (argc < 3)
        {
            return 36;
        }
        std::string list = Read(argv[2]);
        if (list.empty())
        {
            return 45;
        }
        size_t pos = 0;
        while (pos < list.size())
        {
            auto end = list.find('\n', pos);
            if (end == std::string::npos)
            {
                end = list.size();
            }
            auto path = list.substr(pos, end - pos);
            int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), nullptr, 0);
            std::wstring w(n, L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), w.data(), n);
            if (Write(w, "escape"))
            {
                return 37;
            }
            pos = end + 1;
        }
        if (Write(argv[2], "changed"))
        {
            return 38;
        }
        payload = "writes-denied\n";
    }
    if (mode == L"handle")
    {
        auto value = Env(L"FIXTURE_HANDLE");
        HANDLE alleged = reinterpret_cast<HANDLE>(_wcstoui64(value.c_str(), nullptr, 10));
        if (CanReadPrivateHandle(alleged))
        {
            return 39;
        }
        payload = "handle-isolated\n";
    }
    std::wstring output = L"result.txt";
    if (!Write(output, payload))
    {
        return 40;
    }
    if (mode == L"extra" && !Write(L"extra.txt", "unexpected"))
    {
        return 41;
    }
    if (mode == L"malformed")
    {
        return Write(Env(L"FOA_OUTPUT_MANIFEST"), "{") ? 0 : 42;
    }
    std::string digest = Digest(payload);
    if (digest.empty())
    {
        return 43;
    }
    if (mode == L"wrong-hash")
    {
        digest = "sha256:" + std::string(64, '0');
    }
    auto attempt = Utf8(Env(L"FOA_INVOCATION_ID")), request = Utf8(Env(L"FOA_REQUEST_FINGERPRINT"));
    std::string manifest = "{\"contract\":\"foa-tool-output-manifest-v2\",\"version\":2,\"attempt\":\"" + attempt +
        "\",\"requestFingerprint\":\"" + request +
        "\",\"outputs\":[{\"id\":\"result\",\"root\":\"output\",\"path\":\"result.txt\","
        "\"kind\":\"text/plain\",\"sha256\":\"" +
        digest + "\",\"bytes\":" + std::to_string(payload.size()) + "}]}";
    return Write(Env(L"FOA_OUTPUT_MANIFEST"), manifest) ? 0 : 44;
}
