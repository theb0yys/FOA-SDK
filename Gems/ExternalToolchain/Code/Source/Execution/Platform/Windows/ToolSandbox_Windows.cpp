/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ToolSandbox_Windows.h"
#include <Aclapi.h>
#include <AzCore/Math/Uuid.h>
#include <Sddl.h>
#include <UserEnv.h>
#include <algorithm>
#include <filesystem>
#include <set>

namespace ExternalToolchain::Windows
{
    namespace
    {
        AZStd::string Identity(const BY_HANDLE_FILE_INFORMATION& info)
        {
            return AZStd::string::format("%08lx:%08lx%08lx", info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow);
        }
        bool PrivateDescriptor(PSECURITY_DESCRIPTOR& descriptor)
        {
            HANDLE raw = nullptr;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw))
            {
                return false;
            }
            Handle token(raw);
            DWORD needed = 0;
            GetTokenInformation(token.Get(), TokenUser, nullptr, 0, &needed);
            if (!needed || needed > 16384)
            {
                return false;
            }
            std::vector<unsigned char> bytes(needed);
            if (!GetTokenInformation(token.Get(), TokenUser, bytes.data(), needed, &needed))
            {
                return false;
            }
            LPWSTR sid = nullptr;
            if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid, &sid))
            {
                return false;
            }
            std::wstring sddl = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;";
            sddl += sid;
            sddl += L")S:(ML;OICI;NW;;;LW)";
            LocalFree(sid);
            return ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr) != FALSE;
        }
        bool Walk(
            const std::wstring& path,
            const std::wstring& relative,
            unsigned depth,
            AZStd::vector<AZStd::string>& files,
            AZ::u64& bytes,
            size_t& entries)
        {
            if (depth > 16)
            {
                return false;
            }
            PinnedPath parent;
            if (!parent.Open(Utf8(path), true))
            {
                return false;
            }
            WIN32_FIND_DATAW data{};
            HANDLE find = FindFirstFileW((path + L"\\*").c_str(), &data);
            if (find == INVALID_HANDLE_VALUE)
            {
                return GetLastError() == ERROR_FILE_NOT_FOUND;
            }
            bool ok = true;
            do
            {
                std::wstring name = data.cFileName;
                if (name == L"." || name == L"..")
                {
                    continue;
                }
                if (++entries > 1024 || data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                {
                    ok = false;
                    break;
                }
                auto child = path + L"\\" + name;
                auto rel = relative.empty() ? name : relative + L"/" + name;
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (!Walk(child, rel, depth + 1, files, bytes, entries))
                    {
                        ok = false;
                        break;
                    }
                }
                else
                {
                    PinnedPath pin;
                    if (!pin.Open(Utf8(child), false) || pin.m_bytes > ToolMaxArtifactBytes - bytes)
                    {
                        ok = false;
                        break;
                    }
                    bytes += pin.m_bytes;
                    files.push_back(Utf8(rel));
                }
            } while (FindNextFileW(find, &data));
            DWORD error = GetLastError();
            FindClose(find);
            return ok && (error == ERROR_NO_MORE_FILES || error == ERROR_SUCCESS);
        }
        bool Remove(const std::wstring& path, unsigned depth, size_t& entries)
        {
            if (depth > 16)
            {
                return false;
            }
            WIN32_FIND_DATAW data{};
            HANDLE find = FindFirstFileW((path + L"\\*").c_str(), &data);
            if (find == INVALID_HANDLE_VALUE)
            {
                return GetLastError() == ERROR_FILE_NOT_FOUND && RemoveDirectoryW(path.c_str());
            }
            bool ok = true;
            do
            {
                std::wstring name = data.cFileName;
                if (name == L"." || name == L"..")
                {
                    continue;
                }
                if (++entries > 2048 || data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                {
                    ok = false;
                    break;
                }
                auto child = path + L"\\" + name;
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (!Remove(child, depth + 1, entries))
                    {
                        ok = false;
                        break;
                    }
                }
                else
                {
                    {
                        PinnedPath pin;
                        if (!pin.Open(Utf8(child), false))
                        {
                            ok = false;
                            break;
                        }
                    }
                    if (!DeleteFileW(child.c_str()))
                    {
                        ok = false;
                        break;
                    }
                }
            } while (FindNextFileW(find, &data));
            FindClose(find);
            return ok && RemoveDirectoryW(path.c_str());
        }
    } // namespace

    std::wstring Wide(const AZStd::string& s)
    {
        if (s.empty() || s.size() > 32768 || s.find('\0') != AZStd::string::npos)
        {
            return {};
        }
        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (!count)
        {
            return {};
        }
        std::wstring w(count, L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), w.data(), count))
        {
            return {};
        }
        return w;
    }
    AZStd::string Utf8(const std::wstring& w)
    {
        if (w.empty() || w.size() > 32768)
        {
            return {};
        }
        int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (!count)
        {
            return {};
        }
        AZStd::string s(count, '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), static_cast<int>(w.size()), s.data(), count, nullptr, nullptr))
        {
            return {};
        }
        return s;
    }
    AZStd::string NewId()
    {
        auto id = AZ::Uuid::CreateRandom().ToString<AZStd::string>(false, false);
        for (auto& c : id)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c = c - 'A' + 'a';
            }
        }
        return id;
    }
    bool PinnedPath::Open(const AZStd::string& input, bool directory, DWORD access, DWORD share)
    {
        m_handles.clear();
        m_identity.clear();
        m_path = Wide(input);
        if (m_path.size() < 3 || m_path.size() > 1024 || m_path[1] != L':' ||
            !((m_path[0] >= L'A' && m_path[0] <= L'Z') || (m_path[0] >= L'a' && m_path[0] <= L'z')))
        {
            return false;
        }
        std::replace(m_path.begin(), m_path.end(), L'/', L'\\');
        if (m_path[2] != L'\\' || m_path.find(L':', 2) != std::wstring::npos)
        {
            return false;
        }
        if (m_path.size() > 3 && m_path.back() == L'\\')
        {
            m_path.pop_back();
        }
        if (GetDriveTypeW(m_path.substr(0, 3).c_str()) != DRIVE_FIXED)
        {
            return false;
        }
        std::vector<std::wstring> parts{ m_path.substr(0, 3) };
        size_t pos = 3;
        while (pos < m_path.size())
        {
            auto end = m_path.find(L'\\', pos);
            if (end == std::wstring::npos)
            {
                end = m_path.size();
            }
            auto part = m_path.substr(pos, end - pos);
            if (part.empty() || part == L"." || part == L".." || part.back() == L' ' || part.back() == L'.' ||
                part.find_first_of(L"*?<>|\"") != std::wstring::npos)
            {
                return false;
            }
            parts.push_back(m_path.substr(0, end));
            pos = end + 1;
        }
        for (size_t i = 0; i < parts.size(); ++i)
        {
            bool leaf = i + 1 == parts.size();
            Handle handle(CreateFileW(
                parts[i].c_str(),
                leaf ? access : FILE_READ_ATTRIBUTES,
                leaf ? share : FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
                nullptr));
            BY_HANDLE_FILE_INFORMATION info{};
            if (!handle || !GetFileInformationByHandle(handle.Get(), &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
                ((!leaf || directory) != bool(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ||
                (!directory && leaf && info.nNumberOfLinks != 1))
            {
                m_handles.clear();
                return false;
            }
            if (leaf)
            {
                wchar_t finalPath[32768];
                DWORD length = GetFinalPathNameByHandleW(handle.Get(), finalPath, 32768, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
                if (length < 4 || length >= 32768 || _wcsicmp(finalPath + 4, m_path.c_str()) != 0)
                {
                    m_handles.clear();
                    return false;
                }
                m_identity = Identity(info);
                m_bytes = (AZ::u64(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
            }
            m_handles.push_back(std::move(handle));
        }
        return true;
    }
    bool CreatePrivateDirectory(const std::wstring& path)
    {
        auto parent = std::filesystem::path(path).parent_path().wstring();
        PinnedPath pin;
        if (!pin.Open(Utf8(parent), true))
        {
            return false;
        }
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        if (!PrivateDescriptor(descriptor))
        {
            return false;
        }
        SECURITY_ATTRIBUTES attributes{ sizeof(attributes), descriptor, FALSE };
        bool created = CreateDirectoryW(path.c_str(), &attributes) != FALSE;
        LocalFree(descriptor);
        return created;
    }
    bool GrantPath(const std::wstring& path, PSID sid, bool writable, bool directory)
    {
        PinnedPath pin;
        if (!pin.Open(Utf8(path), directory, READ_CONTROL | WRITE_DAC))
        {
            return false;
        }
        PACL oldAcl = nullptr;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        if (GetSecurityInfo(pin.Leaf(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &oldAcl, nullptr, &descriptor) !=
            ERROR_SUCCESS)
        {
            return false;
        }
        EXPLICIT_ACCESS_W entry{};
        entry.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;
        if (writable)
        {
            entry.grfAccessPermissions |= FILE_GENERIC_WRITE | DELETE;
            if (directory)
            {
                entry.grfAccessPermissions |= FILE_DELETE_CHILD;
            }
        }
        entry.grfAccessMode = GRANT_ACCESS;
        entry.grfInheritance = directory ? SUB_CONTAINERS_AND_OBJECTS_INHERIT : NO_INHERITANCE;
        BuildTrusteeWithSidW(&entry.Trustee, sid);
        PACL newAcl = nullptr;
        DWORD result = SetEntriesInAclW(1, &entry, oldAcl, &newAcl);
        if (result == ERROR_SUCCESS)
        {
            result = SetSecurityInfo(
                pin.Leaf(),
                SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                nullptr,
                nullptr,
                newAcl,
                nullptr);
        }
        if (newAcl)
        {
            LocalFree(newAcl);
        }
        LocalFree(descriptor);
        return result == ERROR_SUCCESS;
    }
    bool ReadFileBounded(const std::wstring& path, size_t limit, AZStd::string& contents)
    {
        PinnedPath pin;
        if (!pin.Open(Utf8(path), false, GENERIC_READ, FILE_SHARE_READ) || pin.m_bytes > limit)
        {
            return false;
        }
        contents.resize(static_cast<size_t>(pin.m_bytes));
        DWORD read = 0;
        return ReadFile(pin.Leaf(), contents.data(), static_cast<DWORD>(contents.size()), &read, nullptr) && read == contents.size();
    }
    bool WriteFileAtomic(const std::wstring& path, const AZStd::string& contents)
    {
        PinnedPath parent;
        if (!parent.Open(Utf8(std::filesystem::path(path).parent_path().wstring()), true))
        {
            return false;
        }
        DWORD old = GetFileAttributesW(path.c_str());
        if (old != INVALID_FILE_ATTRIBUTES)
        {
            PinnedPath pin;
            if (!pin.Open(Utf8(path), false))
            {
                return false;
            }
        }
        auto temporary = path + L"." + Wide(NewId()) + L".tmp";
        Handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file)
        {
            return false;
        }
        DWORD written = 0;
        bool ok = WriteFile(file.Get(), contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) &&
            written == contents.size() && FlushFileBuffers(file.Get());
        file.Reset();
        if (ok)
        {
            ok = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        }
        if (!ok)
        {
            DeleteFileW(temporary.c_str());
        }
        return ok;
    }
    bool HashFile(const std::wstring& path, AZStd::string& digest, AZ::u64& bytes, AZ::u64 maximum)
    {
        PinnedPath pin;
        if (!pin.Open(Utf8(path), false, GENERIC_READ, FILE_SHARE_READ) || pin.m_bytes > maximum)
        {
            return false;
        }
        ToolSha256 sha;
        unsigned char buffer[65536];
        bytes = 0;
        for (;;)
        {
            DWORD read = 0;
            if (!ReadFile(pin.Leaf(), buffer, sizeof(buffer), &read, nullptr))
            {
                return false;
            }
            if (!read)
            {
                break;
            }
            if (read > maximum - bytes)
            {
                return false;
            }
            bytes += read;
            sha.Update(buffer, read);
        }
        if (bytes != pin.m_bytes)
        {
            return false;
        }
        digest = sha.Finish();
        return true;
    }
    bool CopyCheckedFile(const std::wstring& source, const std::wstring& destination, const AZStd::string& digest, AZ::u64 bytes)
    {
        PinnedPath pin;
        if (!pin.Open(Utf8(source), false, GENERIC_READ, FILE_SHARE_READ) || pin.m_bytes != bytes || bytes > ToolMaxArtifactBytes)
        {
            return false;
        }
        PinnedPath parent;
        if (!parent.Open(Utf8(std::filesystem::path(destination).parent_path().wstring()), true))
        {
            return false;
        }
        Handle output(CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!output)
        {
            return false;
        }
        ToolSha256 hash;
        unsigned char buffer[65536];
        AZ::u64 total = 0;
        for (;;)
        {
            DWORD read = 0, written = 0;
            if (!ReadFile(pin.Leaf(), buffer, sizeof(buffer), &read, nullptr))
            {
                return false;
            }
            if (!read)
            {
                break;
            }
            if (read > bytes - total)
            {
                return false;
            }
            total += read;
            hash.Update(buffer, read);
            if (!WriteFile(output.Get(), buffer, read, &written, nullptr) || written != read)
            {
                return false;
            }
        }
        return total == bytes && hash.Finish() == digest && FlushFileBuffers(output.Get());
    }
    bool EnumerateTree(const std::wstring& root, AZStd::vector<AZStd::string>& files, AZ::u64& bytes)
    {
        files.clear();
        bytes = 0;
        size_t entries = 0;
        return Walk(root, L"", 0, files, bytes, entries);
    }
    bool RemoveOwnedTree(const std::wstring& path, const AZStd::string& identity)
    {
        // Check the exact absolute root identity and every descendant; never traverse a reparse.
        {
            PinnedPath root;
            if (!root.Open(Utf8(path), true) || root.m_identity != identity)
            {
                return false;
            }
        }
        size_t entries = 0;
        return Remove(path, 0, entries);
    }
    PSID RegistryReadCapabilitySid()
    {
        // Windows-derived identity for registryRead. The native test independently
        // derives it by name; no registry contents or additional capability are used.
        struct Capability
        {
            PSID value = nullptr;
            Capability()
            {
                ConvertStringSidToSidW(
                    L"S-1-15-3-1024-1065365936-1281604716-3511738428-1654721687-"
                    L"432734479-3232135806-4053264122-3456934681",
                    &value);
            }
            ~Capability()
            {
                if (value)
                {
                    LocalFree(value);
                }
            }
        };
        static Capability capability;
        return capability.value;
    }
    bool VerifyLpacToken(HANDLE token, PSID expectedPackage)
    {
        DWORD app = 0, size = 0;
        if (!GetTokenInformation(token, TokenIsAppContainer, &app, sizeof(app), &size) || !app)
        {
            return false;
        }
        alignas(void*) unsigned char package[sizeof(TOKEN_APPCONTAINER_INFORMATION) + SECURITY_MAX_SID_SIZE]{};
        if (!GetTokenInformation(token, TokenAppContainerSid, package, sizeof(package), &size))
        {
            return false;
        }
        auto* identity = reinterpret_cast<TOKEN_APPCONTAINER_INFORMATION*>(package);
        if (!identity->TokenAppContainer || !EqualSid(identity->TokenAppContainer, expectedPackage))
        {
            return false;
        }
        alignas(void*) unsigned char capabilityBytes[4096]{};
        auto* capabilities = reinterpret_cast<TOKEN_GROUPS*>(capabilityBytes);
        if (!RegistryReadCapabilitySid() ||
            !GetTokenInformation(token, TokenCapabilities, capabilityBytes, sizeof(capabilityBytes), &size) ||
            capabilities->GroupCount != 1 || !IsValidSid(capabilities->Groups[0].Sid) ||
            !EqualSid(capabilities->Groups[0].Sid, RegistryReadCapabilitySid()) || capabilities->Groups[0].Attributes != SE_GROUP_ENABLED)
        {
            return false;
        }
        alignas(void*) unsigned char integrity[sizeof(TOKEN_MANDATORY_LABEL) + SECURITY_MAX_SID_SIZE]{};
        if (!GetTokenInformation(token, TokenIntegrityLevel, integrity, sizeof(integrity), &size))
        {
            return false;
        }
        auto* label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(integrity);
        if (!IsValidSid(label->Label.Sid) || !*GetSidSubAuthorityCount(label->Label.Sid) ||
            *GetSidSubAuthority(label->Label.Sid, *GetSidSubAuthorityCount(label->Label.Sid) - 1) > SECURITY_MANDATORY_LOW_RID)
        {
            return false;
        }
        HANDLE raw = nullptr;
        if (!DuplicateToken(token, SecurityImpersonation, &raw))
        {
            return false;
        }
        Handle client(raw);
        auto access = [&](const std::wstring& packageSid, bool expected) -> bool
        {
            // Inspect the token's effective lowbox access with in-memory descriptors.
            // Every check must execute successfully; denial is distinct from API failure.
            std::wstring sddl = L"O:SYG:SYD:P(A;;0x1;;;WD)(A;;0x1;;;" + packageSid + L")";
            PSECURITY_DESCRIPTOR descriptor = nullptr;
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr))
            {
                return false;
            }
            GENERIC_MAPPING mapping{ 1, 1, 1, 1 };
            PRIVILEGE_SET privileges{};
            DWORD length = sizeof(privileges), granted = 0;
            BOOL allowed = FALSE;
            BOOL checked = AccessCheck(descriptor, client.Get(), 1, &mapping, &privileges, &length, &granted, &allowed);
            LocalFree(descriptor);
            return checked && bool(allowed) == expected && (expected ? granted == 1 : granted == 0);
        };
        LPWSTR packageText = nullptr;
        if (!ConvertSidToStringSidW(expectedPackage, &packageText))
        {
            return false;
        }
        bool exact = access(packageText, true);
        LocalFree(packageText);
        // LPAC must ignore ALL APPLICATION PACKAGES while accepting its own identity.
        // The operational control also verifies that an ordinary AppContainer fails this.
        return exact && access(L"S-1-15-2-1", false);
    }
    bool AcquireLocks(const PinnedPath& target, const AZStd::string& provider, ToolLocks& locks)
    {
        std::set<AZStd::string> keys;
        keys.insert("provider/" + provider);
        for (const auto& directory : target.m_handles)
        {
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(directory.Get(), &info))
            {
                return false;
            }
            keys.insert("target/" + Identity(info));
        }
        for (const auto& key : keys)
        {
            auto name = L"Global\\FOA.M2." + Wide(ToolDigest(key).substr(7));
            Handle mutex(CreateMutexW(nullptr, FALSE, name.c_str()));
            if (!mutex)
            {
                return false;
            }
            DWORD wait = WaitForSingleObject(mutex.Get(), 0);
            if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
            {
                return false;
            }
            locks.m_handles.push_back(std::move(mutex));
        }
        return true;
    }
    std::wstring QuoteArgument(const std::wstring& argument)
    {
        std::wstring result = L"\"";
        size_t slashes = 0;
        for (wchar_t c : argument)
        {
            if (c == L'\\')
            {
                ++slashes;
                continue;
            }
            result.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
            slashes = 0;
            result.push_back(c);
        }
        result.append(slashes * 2, L'\\');
        result.push_back(L'"');
        return result;
    }
    bool BuildCommandLine(const std::wstring& executable, const std::vector<std::wstring>& arguments, std::wstring& result)
    {
        if (executable.empty() || arguments.size() > ToolMaxArguments)
        {
            return false;
        }
        result = QuoteArgument(executable);
        for (const auto& argument : arguments)
        {
            if (argument.find(L'\0') != std::wstring::npos)
            {
                return false;
            }
            result += L" " + QuoteArgument(argument);
            if (result.size() + 1 > 16384)
            {
                return false;
            }
        }
        return result.size() + 1 <= 16384;
    }
    Sandbox::~Sandbox()
    {
        if (m_created)
        {
            Delete();
        }
        if (m_sid)
        {
            FreeSid(m_sid);
        }
    }
    bool Sandbox::Create(const AZStd::string& name)
    {
        if (!ToolSafeId(name))
        {
            return false;
        }
        m_name = name;
        if (!RegistryReadCapabilitySid())
        {
            return false;
        }
        SID_AND_ATTRIBUTES registryRead{ RegistryReadCapabilitySid(), SE_GROUP_ENABLED };
        HRESULT result =
            CreateAppContainerProfile(Wide(name).c_str(), L"FOA tool invocation", L"Isolated batch tool", &registryRead, 1, &m_sid);
        if (FAILED(result))
        {
            return false;
        }
        m_created = true;
        LPWSTR sidText = nullptr;
        if (!ConvertSidToStringSidW(m_sid, &sidText))
        {
            return false;
        }
        PWSTR folder = nullptr;
        result = GetAppContainerFolderPath(sidText, &folder);
        LocalFree(sidText);
        if (FAILED(result))
        {
            return false;
        }
        m_profilePath = folder;
        CoTaskMemFree(folder);
        return true;
    }
    bool Sandbox::Delete()
    {
        if (!m_created)
        {
            return true;
        }
        if (FAILED(DeleteAppContainerProfile(Wide(m_name).c_str())))
        {
            return false;
        }
        m_created = false;
        return true;
    }
} // namespace ExternalToolchain::Windows
