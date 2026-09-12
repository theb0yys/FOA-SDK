/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "../../ToolExecutionAdmission.h"
#include "../../ToolExecutionJournal.h"
#include <AzCore/PlatformDef.h>
#include <Windows.h>
#include <memory>
#include <string>
#include <vector>

namespace ExternalToolchain::Windows
{
    class Handle
    {
    public:
        explicit Handle(HANDLE value = nullptr)
            : m_value(value)
        {
        }
        ~Handle()
        {
            Reset();
        }
        Handle(Handle&& other) noexcept
            : m_value(other.Release())
        {
        }
        Handle& operator=(Handle&& other) noexcept
        {
            if (this != &other)
            {
                Reset(other.Release());
            }
            return *this;
        }
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        HANDLE Get() const
        {
            return m_value;
        }
        explicit operator bool() const
        {
            return m_value && m_value != INVALID_HANDLE_VALUE;
        }
        HANDLE Release()
        {
            auto value = m_value;
            m_value = nullptr;
            return value;
        }
        void Reset(HANDLE value = nullptr)
        {
            if (*this)
            {
                CloseHandle(m_value);
            }
            m_value = value;
        }

    private:
        HANDLE m_value;
    };
    std::wstring Wide(const AZStd::string& value);
    AZStd::string Utf8(const std::wstring& value);
    AZStd::string NewId();
    struct PinnedPath
    {
        std::wstring m_path;
        std::vector<Handle> m_handles;
        AZStd::string m_identity;
        AZ::u64 m_bytes = 0;
        bool Open(
            const AZStd::string& path,
            bool directory,
            DWORD access = FILE_READ_ATTRIBUTES,
            DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE);
        HANDLE Leaf() const
        {
            return m_handles.empty() ? nullptr : m_handles.back().Get();
        }
    };
    bool CreatePrivateDirectory(const std::wstring& path);
    bool GrantPath(const std::wstring& path, PSID sid, bool writable, bool directory);
    bool ReadFileBounded(const std::wstring& path, size_t limit, AZStd::string& contents);
    bool WriteFileAtomic(const std::wstring& path, const AZStd::string& contents);
    bool HashFile(const std::wstring& path, AZStd::string& digest, AZ::u64& bytes, AZ::u64 maximum = ToolMaxArtifactBytes);
    bool CopyCheckedFile(const std::wstring& source, const std::wstring& destination, const AZStd::string& digest, AZ::u64 bytes);
    bool RemoveOwnedTree(const std::wstring& path, const AZStd::string& identity);
    bool EnumerateTree(const std::wstring& root, AZStd::vector<AZStd::string>& files, AZ::u64& bytes);
    struct ToolLocks
    {
        std::vector<Handle> m_handles;
        ~ToolLocks()
        {
            for (auto& mutex : m_handles)
            {
                ReleaseMutex(mutex.Get());
            }
        }
    };
    PSID RegistryReadCapabilitySid();
    bool VerifyLpacToken(HANDLE token, PSID expectedPackage);
    bool AcquireLocks(const PinnedPath& target, const AZStd::string& provider, ToolLocks& locks);
    std::wstring QuoteArgument(const std::wstring& argument);
    bool BuildCommandLine(const std::wstring& executable, const std::vector<std::wstring>& arguments, std::wstring& result);
    class Sandbox
    {
    public:
        Sandbox() = default;
        ~Sandbox();
        bool Create(const AZStd::string& name);
        bool Delete();
        PSID Sid() const
        {
            return m_sid;
        }
        const std::wstring& ProfilePath() const
        {
            return m_profilePath;
        }
        bool Created() const
        {
            return m_created;
        }

    private:
        PSID m_sid = nullptr;
        AZStd::string m_name;
        std::wstring m_profilePath;
        bool m_created = false;
    };
} // namespace ExternalToolchain::Windows
