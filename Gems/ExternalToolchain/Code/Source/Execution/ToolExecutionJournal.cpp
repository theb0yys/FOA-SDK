/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ToolExecutionJournal.h"
#include <AzCore/JSON/document.h>
#include <AzCore/JSON/stringbuffer.h>
#include <AzCore/JSON/writer.h>
#include <AzCore/PlatformDef.h>
#include <map>
#include <set>
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
#include "Platform/Windows/ToolSandbox_Windows.h"
#endif

namespace ExternalToolchain
{
    namespace
    {
        bool FileIdentity(const AZStd::string& text)
        {
            if (text.size() != 25 || text[8] != ':')
            {
                return false;
            }
            for (size_t i = 0; i < text.size(); ++i)
            {
                if (i != 8 && !((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f')))
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace
    struct ToolExecutionJournal::Impl
    {
        AZStd::string m_root;
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
        Windows::PinnedPath m_pin;
        Windows::Handle m_writerLock;
        std::wstring m_records;
        std::map<AZStd::string, AZ::u64> m_sequences;
        std::map<AZStd::string, AZ::u64> m_sizes;
        AZ::u64 m_journalBytes = 0, m_logBytes = 0;

        bool Put(const AZStd::string& name, const AZStd::string& contents, bool log = false)
        {
            AZ::u64& total = log ? m_logBytes : m_journalBytes;
            AZ::u64 ceiling = log ? 512ULL * 1024 * 1024 : 256ULL * 1024 * 1024;
            AZ::u64 old = m_sizes[name];
            if (contents.size() > ceiling - (total - old))
            {
                return false;
            }
            if (!Windows::WriteFileAtomic(m_records + L"\\" + Windows::Wide(name), contents))
            {
                return false;
            }
            total = total - old + contents.size();
            m_sizes[name] = contents.size();
            return true;
        }
        bool Remove(const AZStd::string& name)
        {
            auto path = m_records + L"\\" + Windows::Wide(name);
            if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
            {
                return GetLastError() == ERROR_FILE_NOT_FOUND;
            }
            {
                Windows::PinnedPath pin;
                if (!pin.Open(Windows::Utf8(path), false))
                {
                    return false;
                }
            }
            if (!DeleteFileW(path.c_str()))
            {
                return false;
            }
            auto it = m_sizes.find(name);
            if (it != m_sizes.end())
            {
                m_journalBytes -= it->second;
                m_sizes.erase(it);
            }
            return true;
        }
        static AZStd::string Frame(const AZStd::string& body)
        {
            return ToolDigest(body) + "\n" + body;
        }
        bool Read(const AZStd::string& name, AZStd::string& body)
        {
            AZStd::string frame;
            if (!Windows::ReadFileBounded(m_records + L"\\" + Windows::Wide(name), ToolMaxDocumentBytes + 72, frame) || frame.size() < 72 ||
                frame[71] != '\n')
            {
                return false;
            }
            body = frame.substr(72);
            return ToolDigest(body) == frame.substr(0, 71);
        }
#endif
    };
    ToolExecutionJournal::ToolExecutionJournal()
        : m_impl(std::make_unique<Impl>())
    {
    }
    ToolExecutionJournal::~ToolExecutionJournal() = default;
    AZStd::string ToolExecutionJournal::Root() const
    {
        return m_impl->m_root;
    }
    ToolResult ToolExecutionJournal::Open(const AZStd::string& root)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
        if (m_impl->m_writerLock)
        {
            return { ToolError::Busy };
        }
        auto path = Windows::Wide(root);
        if (path.empty())
        {
            return { ToolError::UnsafePath };
        }
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES && !Windows::CreatePrivateDirectory(path))
        {
            return { ToolError::UnsafePath };
        }
        if (!m_impl->m_pin.Open(root, true))
        {
            return { ToolError::UnsafePath };
        }
        // An exclusive regular lock file also excludes other host sessions for this store.
        auto lockPath = m_impl->m_pin.m_path + L"\\host.lock";
        if (GetFileAttributesW(lockPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            Windows::PinnedPath pin;
            if (!pin.Open(Windows::Utf8(lockPath), false))
            {
                return { ToolError::UnsafePath };
            }
        }
        m_impl->m_writerLock.Reset(CreateFileW(
            lockPath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr));
        if (!m_impl->m_writerLock)
        {
            return { ToolError::Busy };
        }
        m_impl->m_root = Windows::Utf8(m_impl->m_pin.m_path);
        m_impl->m_records = m_impl->m_pin.m_path + L"\\records";
        for (const auto& folder : { m_impl->m_records, m_impl->m_pin.m_path + L"\\staging" })
        {
            if (GetFileAttributesW(folder.c_str()) == INVALID_FILE_ATTRIBUTES && !Windows::CreatePrivateDirectory(folder))
            {
                return { ToolError::JournalFailed };
            }
            Windows::PinnedPath pin;
            if (!pin.Open(Windows::Utf8(folder), true))
            {
                return { ToolError::UnsafePath };
            }
        }
        WIN32_FIND_DATAW data{};
        HANDLE find = FindFirstFileW((m_impl->m_records + L"\\*").c_str(), &data);
        if (find != INVALID_HANDLE_VALUE)
        {
            bool valid = true;
            size_t entries = 0;
            do
            {
                std::wstring name = data.cFileName;
                if (name == L"." || name == L"..")
                {
                    continue;
                }
                if (++entries > 8192 || (data.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)))
                {
                    valid = false;
                    break;
                }
                Windows::PinnedPath pin;
                if (!pin.Open(Windows::Utf8(m_impl->m_records + L"\\" + name), false))
                {
                    valid = false;
                    break;
                }
                auto key = Windows::Utf8(name);
                m_impl->m_sizes[key] = pin.m_bytes;
                bool logFile = key.size() > 4 && key.substr(key.size() - 4) == ".log";
                if (logFile)
                {
                    m_impl->m_logBytes += pin.m_bytes;
                }
                else
                {
                    m_impl->m_journalBytes += pin.m_bytes;
                }
            } while (FindNextFileW(find, &data));
            FindClose(find);
            if (!valid)
            {
                return { ToolError::UnsafePath };
            }
        }
        if (m_impl->m_logBytes > 512ULL * 1024 * 1024 || m_impl->m_journalBytes > 256ULL * 1024 * 1024)
        {
            return { ToolError::StoreFull };
        }
        return {};
#else
        (void)root;
        return { ToolError::UnsupportedPlatform };
#endif
    }
    ToolResult ToolExecutionJournal::Save(const ToolInvocationRecordV2& record)
    {
        auto encoded = EncodeToolRecord(record);
        if (!encoded)
        {
            return { encoded.m_error };
        }
        std::lock_guard<std::mutex> lock(m_mutex);
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
        if (!m_impl->m_writerLock)
        {
            return { ToolError::JournalFailed };
        }
        const auto& id = record.m_status.m_attemptId;
        auto found = m_impl->m_sequences.find(id);
        if (found != m_impl->m_sequences.end() && record.m_status.m_sequence <= found->second)
        {
            return { ToolError::InvalidContract };
        }
        if (found == m_impl->m_sequences.end() && m_impl->m_sequences.size() >= ToolMaxRecords)
        {
            return { ToolError::StoreFull };
        }
        auto slot = AZStd::string::format(".record.%u", static_cast<unsigned>(record.m_status.m_sequence % 2));
        if (!m_impl->Put(id + slot, Impl::Frame(encoded.m_json)))
        {
            return { ToolError::JournalFailed };
        }
        m_impl->m_sequences[id] = record.m_status.m_sequence;
        return {};
#else
        return { ToolError::UnsupportedPlatform };
#endif
    }
    ToolResult ToolExecutionJournal::SaveLogs(const AZStd::string& id, const AZStd::string& out, const AZStd::string& err)
    {
        if (!ToolSafeId(id) || out.size() > ToolMaxLogBytes || err.size() > ToolMaxLogBytes)
        {
            return { ToolError::InvalidContract };
        }
        std::lock_guard<std::mutex> lock(m_mutex);
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
        if (!m_impl->m_writerLock || !m_impl->Put(id + ".stdout.log", out, true) || !m_impl->Put(id + ".stderr.log", err, true))
        {
            return { ToolError::JournalFailed };
        }
        return {};
#else
        return { ToolError::UnsupportedPlatform };
#endif
    }
    ToolResult ToolExecutionJournal::ReadLogs(const AZStd::string& id, AZStd::string& out, AZStd::string& err)
    {
        if (!ToolSafeId(id))
        {
            return { ToolError::InvalidContract };
        }
        std::lock_guard<std::mutex> lock(m_mutex);
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
        auto read = [&](const char* suffix, AZStd::string& text)
        {
            auto path = m_impl->m_records + L"\\" + Windows::Wide(id + suffix);
            if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
            {
                text.clear();
                return GetLastError() == ERROR_FILE_NOT_FOUND;
            }
            return Windows::ReadFileBounded(path, ToolMaxLogBytes, text);
        };
        return { read(".stdout.log", out) && read(".stderr.log", err) ? ToolError::None : ToolError::JournalFailed };
#else
        (void)out;
        (void)err;
        return { ToolError::UnsupportedPlatform };
#endif
    }
    ToolResult ToolExecutionJournal::Load(AZStd::vector<ToolInvocationRecordV2>& records)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
        std::set<AZStd::string> ids;
        for (const auto& [name, size] : m_impl->m_sizes)
        {
            (void)size;
            auto pos = name.rfind(".record.");
            if (pos != AZStd::string::npos)
            {
                auto id = name.substr(0, pos);
                if (!ToolSafeId(id) || name.size() != pos + 9 || (name.back() != '0' && name.back() != '1'))
                {
                    return { ToolError::JournalFailed };
                }
                ids.insert(id);
            }
        }
        if (ids.size() > ToolMaxRecords)
        {
            return { ToolError::StoreFull };
        }
        AZStd::vector<ToolInvocationRecordV2> loaded;
        for (const auto& id : ids)
        {
            bool found = false;
            ToolInvocationRecordV2 newest;
            for (unsigned slot = 0; slot < 2; ++slot)
            {
                AZStd::string body;
                ToolInvocationRecordV2 r;
                if (m_impl->Read(id + AZStd::string::format(".record.%u", slot), body) && DecodeToolRecord(body, r) &&
                    r.m_status.m_attemptId == id)
                {
                    if (!found || r.m_status.m_sequence > newest.m_status.m_sequence)
                    {
                        newest = AZStd::move(r);
                        found = true;
                    }
                }
            }
            // Corrupt attempts remain quarantined in place; no path from them is followed.
            if (!found)
            {
                return { ToolError::JournalFailed };
            }
            m_impl->m_sequences[id] = newest.m_status.m_sequence;
            loaded.push_back(AZStd::move(newest));
        }
        records = AZStd::move(loaded);
        return {};
#else
        (void)records;
        return { ToolError::UnsupportedPlatform };
#endif
    }
    ToolResult ToolExecutionJournal::WriteIntent(const ToolRecoveryIntent& i)
    {
        if (!ToolSafeId(i.m_attemptId) || !ToolSafeId(i.m_stageName) || i.m_profileName != "foa.m2." + i.m_stageName ||
            (!i.m_stageIdentity.empty() && !FileIdentity(i.m_stageIdentity)))
        {
            return { ToolError::InvalidContract };
        }
        std::lock_guard<std::mutex> lock(m_mutex);
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
        rapidjson::StringBuffer b;
        rapidjson::Writer<rapidjson::StringBuffer> w(b);
        w.StartObject();
        w.Key("version");
        w.Uint(2);
        for (const auto& pair : { std::make_pair("attempt", i.m_attemptId),
                                  std::make_pair("stage", i.m_stageName),
                                  std::make_pair("profile", i.m_profileName),
                                  std::make_pair("identity", i.m_stageIdentity) })
        {
            w.Key(pair.first);
            w.String(pair.second.c_str());
        }
        w.Key("profileCreated");
        w.Bool(i.m_profileCreated);
        w.EndObject();
        return { m_impl->m_writerLock && m_impl->Put(i.m_attemptId + ".intent", Impl::Frame(AZStd::string(b.GetString(), b.GetSize())))
                     ? ToolError::None
                     : ToolError::JournalFailed };
#else
        return { ToolError::UnsupportedPlatform };
#endif
    }
    ToolResult ToolExecutionJournal::ReadIntents(AZStd::vector<ToolRecoveryIntent>& intents)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
        for (const auto& [name, size] : m_impl->m_sizes)
        {
            (void)size;
            if (name.size() < 7 || name.substr(name.size() - 7) != ".intent")
            {
                continue;
            }
            if (intents.size() >= ToolMaxRecords)
            {
                return { ToolError::StoreFull };
            }
            AZStd::string body;
            if (!m_impl->Read(name, body) || body.size() > 2048)
            {
                return { ToolError::JournalFailed };
            }
            rapidjson::Document d;
            d.Parse<rapidjson::kParseValidateEncodingFlag>(body.data(), body.size());
            if (d.HasParseError() || !d.IsObject() || d.MemberCount() != 6)
            {
                return { ToolError::JournalFailed };
            }
            std::set<AZStd::string> seen;
            for (auto it = d.MemberBegin(); it != d.MemberEnd(); ++it)
            {
                AZStd::string key(it->name.GetString(), it->name.GetStringLength());
                if (!seen.insert(key).second)
                {
                    return { ToolError::JournalFailed };
                }
            }
            if (seen != std::set<AZStd::string>{ "version", "attempt", "stage", "profile", "identity", "profileCreated" } ||
                !d["version"].IsUint() || d["version"].GetUint() != 2 || !d["profileCreated"].IsBool())
            {
                return { ToolError::JournalFailed };
            }
            for (const char* key : { "attempt", "stage", "profile", "identity" })
            {
                if (!d[key].IsString())
                {
                    return { ToolError::JournalFailed };
                }
            }
            auto text = [&](const char* key)
            {
                return AZStd::string(d[key].GetString(), d[key].GetStringLength());
            };
            ToolRecoveryIntent intent{ text("attempt"), text("stage"), text("profile"), text("identity"), d["profileCreated"].GetBool() };
            if (!ToolSafeId(intent.m_attemptId) || name != intent.m_attemptId + ".intent" || !ToolSafeId(intent.m_stageName) ||
                intent.m_stageName.size() != 32 || intent.m_profileName != "foa.m2." + intent.m_stageName ||
                (!intent.m_stageIdentity.empty() && !FileIdentity(intent.m_stageIdentity)))
            {
                return { ToolError::JournalFailed };
            }
            intents.push_back(AZStd::move(intent));
        }
        return {};
#else
        (void)intents;
        return { ToolError::UnsupportedPlatform };
#endif
    }
    ToolResult ToolExecutionJournal::ClearIntent(const AZStd::string& attempt)
    {
        if (!ToolSafeId(attempt))
        {
            return { ToolError::InvalidContract };
        }
        std::lock_guard<std::mutex> lock(m_mutex);
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
        return { m_impl->Remove(attempt + ".intent") ? ToolError::None : ToolError::JournalFailed };
#else
        return { ToolError::UnsupportedPlatform };
#endif
    }
} // namespace ExternalToolchain
