/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzCore/JSON/document.h>
#include <AzCore/JSON/stringbuffer.h>
#include <AzCore/JSON/writer.h>
#include <ExternalToolchain/ToolExecutionTypes.h>
#include <algorithm>
#include <cstring>
#include <set>

namespace ExternalToolchain
{
    namespace
    {
        AZ::u32 Rotate(AZ::u32 v, unsigned n)
        {
            return (v >> n) | (v << (32 - n));
        }
        constexpr AZ::u32 K[64] = { 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                                    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                                    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                                    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                                    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                                    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                                    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                                    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
        using Writer = rapidjson::Writer<rapidjson::StringBuffer>;
        void Text(Writer& w, const char* key, const AZStd::string& s)
        {
            w.Key(key);
            w.String(s.data(), static_cast<rapidjson::SizeType>(s.size()));
        }
        void Number(Writer& w, const char* key, AZ::u64 n)
        {
            w.Key(key);
            w.Uint64(n);
        }
        bool Version(const AZStd::string& v)
        {
            if (v.empty() || v.size() > 32)
            {
                return false;
            }
            unsigned dots = 0, digits = 0;
            for (char c : v)
            {
                if (c == '.')
                {
                    if (!digits || ++dots > 2)
                    {
                        return false;
                    }
                    digits = 0;
                }
                else if (c < '0' || c > '9' || ++digits > 9)
                {
                    return false;
                }
            }
            return dots == 2 && digits > 0;
        }
        bool Kind(const AZStd::string& v)
        {
            return !v.empty() && v.size() <= 96 &&
                std::all_of(
                    v.begin(),
                    v.end(),
                    [](char c)
                    {
                        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '/';
                    });
        }
        bool EnvironmentName(const AZStd::string& v)
        {
            if (v.empty() || v.size() > 64)
            {
                return false;
            }
            for (char c : v)
            {
                if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
                {
                    return false;
                }
            }
            for (const char* forbidden : { "PATH",
                                           "COMSPEC",
                                           "PATHEXT",
                                           "TEMP",
                                           "TMP",
                                           "SYSTEMROOT",
                                           "WINDIR",
                                           "LOCALAPPDATA",
                                           "APPDATA",
                                           "USERPROFILE",
                                           "HOME",
                                           "LD_PRELOAD",
                                           "LD_LIBRARY_PATH",
                                           "PYTHONPATH",
                                           "DOTNET_STARTUP_HOOKS",
                                           "COR_ENABLE_PROFILING",
                                           "COR_PROFILER",
                                           "NODE_OPTIONS",
                                           "FOA_INVOCATION_ID",
                                           "FOA_REQUEST_FINGERPRINT",
                                           "FOA_OUTPUT_MANIFEST" })
            {
                if (v == forbidden)
                {
                    return false;
                }
            }
            return v[0] >= 'A' && v[0] <= 'Z';
        }
        template<class V, class F>
        bool Unique(const V& values, F key, size_t bound)
        {
            if (values.size() > bound)
            {
                return false;
            }
            std::set<AZStd::string> seen;
            for (const auto& value : values)
            {
                const auto& candidate = key(value);
                if (candidate.size() > 96 || !seen.insert(candidate).second)
                {
                    return false;
                }
            }
            return true;
        }
        template<class T>
        AZStd::vector<T> Sorted(AZStd::vector<T> values)
        {
            std::sort(
                values.begin(),
                values.end(),
                [](const T& a, const T& b)
                {
                    return a.m_id < b.m_id;
                });
            return values;
        }
        bool Files(const AZStd::vector<ToolFileReference>& files)
        {
            if (!Unique(
                    files,
                    [](const auto& f) -> const AZStd::string&
                    {
                        return f.m_id;
                    },
                    ToolMaxFiles))
            {
                return false;
            }
            AZ::u64 total = 0;
            std::set<AZStd::string> locations;
            for (const auto& f : files)
            {
                if (!ToolSafeId(f.m_id) || !ToolSafeId(f.m_rootId) || !ToolSafeRelativePath(f.m_relativePath) || !Kind(f.m_kind) ||
                    !ToolValidDigest(f.m_sha256) || f.m_bytes > ToolMaxArtifactBytes - total ||
                    !locations.insert(f.m_rootId + "/" + f.m_relativePath).second)
                {
                    return false;
                }
                total += f.m_bytes;
            }
            return true;
        }
        void FileArray(Writer& w, const AZStd::vector<ToolFileReference>& values)
        {
            w.StartArray();
            for (const auto& f : Sorted(values))
            {
                w.StartObject();
                Text(w, "id", f.m_id);
                Text(w, "root", f.m_rootId);
                Text(w, "path", f.m_relativePath);
                Text(w, "kind", f.m_kind);
                Text(w, "sha256", f.m_sha256);
                Number(w, "bytes", f.m_bytes);
                w.EndObject();
            }
            w.EndArray();
        }
        ToolCanonicalResult Complete(const rapidjson::StringBuffer& buffer)
        {
            if (buffer.GetSize() > ToolMaxDocumentBytes)
            {
                return { ToolError::InvalidContract, {}, {} };
            }
            AZStd::string value(buffer.GetString(), buffer.GetSize());
            return { ToolError::None, value, ToolDigest(value) };
        }
        bool Shape(const rapidjson::Value& value, std::initializer_list<const char*> keys)
        {
            if (!value.IsObject() || value.MemberCount() != keys.size())
            {
                return false;
            }
            std::set<AZStd::string> seen;
            for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it)
            {
                AZStd::string name(it->name.GetString(), it->name.GetStringLength());
                bool found = false;
                for (const char* key : keys)
                {
                    found |= name == key;
                }
                if (!found || !seen.insert(name).second)
                {
                    return false;
                }
            }
            return true;
        }
        bool ReadText(const rapidjson::Value& v, const char* key, AZStd::string& target)
        {
            if (!v[key].IsString())
            {
                return false;
            }
            target.assign(v[key].GetString(), v[key].GetStringLength());
            return true;
        }
        bool ReadNumber(const rapidjson::Value& v, const char* key, AZ::u64& target)
        {
            if (!v[key].IsUint64())
            {
                return false;
            }
            target = v[key].GetUint64();
            return true;
        }
        bool Parse(const AZStd::string& bytes, rapidjson::Document& doc)
        {
            if (bytes.empty() || bytes.size() > ToolMaxDocumentBytes)
            {
                return false;
            }
            // Bound nesting before the DOM parser allocates or recurses.
            unsigned depth = 0;
            bool quoted = false, escape = false;
            for (char c : bytes)
            {
                if (!c)
                {
                    return false;
                }
                if (quoted)
                {
                    if (escape)
                    {
                        escape = false;
                    }
                    else if (c == '\\')
                    {
                        escape = true;
                    }
                    else if (c == '"')
                    {
                        quoted = false;
                    }
                }
                else if (c == '"')
                {
                    quoted = true;
                }
                else if (c == '{' || c == '[')
                {
                    if (++depth > 12)
                    {
                        return false;
                    }
                }
                else if (c == '}' || c == ']')
                {
                    if (!depth)
                    {
                        return false;
                    }
                    --depth;
                }
            }
            if (depth || quoted)
            {
                return false;
            }
            doc.Parse<rapidjson::kParseValidateEncodingFlag>(bytes.data(), bytes.size());
            return !doc.HasParseError();
        }
        bool ReadFiles(const rapidjson::Value& values, AZStd::vector<ToolFileReference>& target)
        {
            if (!values.IsArray() || values.Size() > ToolMaxFiles)
            {
                return false;
            }
            for (const auto& v : values.GetArray())
            {
                ToolFileReference f;
                if (!Shape(v, { "id", "root", "path", "kind", "sha256", "bytes" }) || !ReadText(v, "id", f.m_id) ||
                    !ReadText(v, "root", f.m_rootId) || !ReadText(v, "path", f.m_relativePath) || !ReadText(v, "kind", f.m_kind) ||
                    !ReadText(v, "sha256", f.m_sha256) || !ReadNumber(v, "bytes", f.m_bytes))
                {
                    return false;
                }
                target.push_back(AZStd::move(f));
            }
            return Files(target);
        }
        bool StatusValid(const ToolInvocationStatusV2& s)
        {
            return ToolSafeId(s.m_attemptId) && s.m_stage <= ToolStage::Terminal && s.m_outcome <= ToolOutcome::Failed &&
                s.m_verification <= ToolVerification::Failed && s.m_cleanup <= ToolCleanup::Failed &&
                s.m_persistence <= ToolPersistence::Failed && s.m_error <= ToolError::UnsupportedPlatform &&
                !(s.m_stage == ToolStage::Terminal && s.m_outcome == ToolOutcome::Running);
        }
    } // namespace

    ToolSha256::ToolSha256()
        : m_state{ 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 }
    {
    }
    void ToolSha256::Block(const unsigned char* p)
    {
        AZ::u32 w[64];
        for (unsigned i = 0; i < 16; ++i)
        {
            w[i] = (AZ::u32(p[i * 4]) << 24) | (AZ::u32(p[i * 4 + 1]) << 16) | (AZ::u32(p[i * 4 + 2]) << 8) | p[i * 4 + 3];
        }
        for (unsigned i = 16; i < 64; ++i)
        {
            AZ::u32 a = w[i - 15], b = w[i - 2];
            w[i] = w[i - 16] + (Rotate(a, 7) ^ Rotate(a, 18) ^ (a >> 3)) + w[i - 7] + (Rotate(b, 17) ^ Rotate(b, 19) ^ (b >> 10));
        }
        AZ::u32 a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3], e = m_state[4], f = m_state[5], g = m_state[6],
                h = m_state[7];
        for (unsigned i = 0; i < 64; ++i)
        {
            AZ::u32 t1 = h + (Rotate(e, 6) ^ Rotate(e, 11) ^ Rotate(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
            AZ::u32 t2 = (Rotate(a, 2) ^ Rotate(a, 13) ^ Rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
        m_state[5] += f;
        m_state[6] += g;
        m_state[7] += h;
    }
    void ToolSha256::Update(const void* data, size_t size)
    {
        const auto* bytes = static_cast<const unsigned char*>(data);
        m_bytes += size;
        while (size)
        {
            size_t take = std::min(size, 64 - m_used);
            std::memcpy(m_buffer + m_used, bytes, take);
            m_used += take;
            bytes += take;
            size -= take;
            if (m_used == 64)
            {
                Block(m_buffer);
                m_used = 0;
            }
        }
    }
    AZStd::string ToolSha256::Finish()
    {
        AZ::u64 bits = m_bytes * 8;
        unsigned char pad[128] = { 0x80 };
        size_t count = m_used < 56 ? 56 - m_used : 120 - m_used;
        Update(pad, count);
        for (unsigned i = 0; i < 8; ++i)
        {
            pad[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
        }
        Update(pad, 8);
        AZStd::string result = "sha256:";
        constexpr char hex[] = "0123456789abcdef";
        for (AZ::u32 word : m_state)
        {
            for (int n = 28; n >= 0; n -= 4)
            {
                result.push_back(hex[(word >> n) & 15]);
            }
        }
        return result;
    }
    AZStd::string ToolDigest(const AZStd::string& value)
    {
        ToolSha256 sha;
        sha.Update(value.data(), value.size());
        return sha.Finish();
    }
    bool ToolValidDigest(const AZStd::string& s)
    {
        return s.size() == 71 && s.compare(0, 7, "sha256:") == 0 &&
            std::all_of(
                   s.begin() + 7,
                   s.end(),
                   [](char c)
                   {
                       return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                   });
    }
    bool ToolSafeId(const AZStd::string& s)
    {
        return !s.empty() && s.size() <= 96 && s.front() != '.' && s.back() != '.' && s.find("..") == AZStd::string::npos &&
            std::all_of(
                s.begin(),
                s.end(),
                [](char c)
                {
                    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
                });
    }
    bool ToolSafeText(const AZStd::string& s, size_t maximum)
    {
        if (s.size() > maximum || s.find("://") != AZStd::string::npos)
        {
            return false;
        }
        for (size_t i = 0; i < s.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c >= 128)
            {
                unsigned remaining = 0;
                AZ::u32 value = 0, minimum = 0;
                if (c >= 0xc2 && c <= 0xdf)
                {
                    remaining = 1;
                    value = c & 31;
                    minimum = 0x80;
                }
                else if (c >= 0xe0 && c <= 0xef)
                {
                    remaining = 2;
                    value = c & 15;
                    minimum = 0x800;
                }
                else if (c >= 0xf0 && c <= 0xf4)
                {
                    remaining = 3;
                    value = c & 7;
                    minimum = 0x10000;
                }
                else
                {
                    return false;
                }
                if (remaining >= s.size() - i)
                {
                    return false;
                }
                while (remaining--)
                {
                    auto next = static_cast<unsigned char>(s[++i]);
                    if ((next & 0xc0) != 0x80)
                    {
                        return false;
                    }
                    value = (value << 6) | (next & 63);
                }
                if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                {
                    return false;
                }
                continue;
            }
            if ((c < 32 && c != '\t') || c == 127 ||
                (c == ':' && i && ((s[i - 1] >= 'a' && s[i - 1] <= 'z') || (s[i - 1] >= 'A' && s[i - 1] <= 'Z'))))
            {
                return false;
            }
        }
        return true;
    }
    bool ToolSafeRelativePath(const AZStd::string& s)
    {
        if (s.empty() || s.size() > 240 || s.front() == '/' || s.back() == '/' || s.find('\\') != AZStd::string::npos)
        {
            return false;
        }
        size_t start = 0;
        while (start < s.size())
        {
            size_t end = s.find('/', start);
            if (end == AZStd::string::npos)
            {
                end = s.size();
            }
            auto part = s.substr(start, end - start);
            if (!ToolSafeId(part))
            {
                return false;
            }
            auto dot = part.find('.');
            auto base = part.substr(0, dot);
            if (base == "con" || base == "prn" || base == "aux" || base == "nul" ||
                (base.size() == 4 && (base.substr(0, 3) == "com" || base.substr(0, 3) == "lpt") && base[3] >= '0' && base[3] <= '9'))
            {
                return false;
            }
            start = end + 1;
        }
        return true;
    }
    bool ToolSucceeded(const ToolInvocationRecordV2& r)
    {
        return r.m_status.m_stage == ToolStage::Terminal && r.m_status.m_outcome == ToolOutcome::ExitedZero &&
            r.m_status.m_verification == ToolVerification::Passed && r.m_status.m_cleanup == ToolCleanup::Complete &&
            r.m_status.m_persistence == ToolPersistence::Durable && r.m_status.m_error == ToolError::None && r.m_exitCodeObserved &&
            r.m_exitCode == 0;
    }
    const char* ToolErrorName(ToolError error)
    {
        constexpr const char* names[] = { "None",
                                          "InvalidContract",
                                          "UnsupportedVersion",
                                          "Disabled",
                                          "AdmissionDenied",
                                          "StaleAdmission",
                                          "SecretUseUnsupported",
                                          "ProviderMismatch",
                                          "RegistrationClosed",
                                          "Duplicate",
                                          "QueueFull",
                                          "NotFound",
                                          "Busy",
                                          "UnsafePath",
                                          "IdentityChanged",
                                          "InputMismatch",
                                          "IsolationUnavailable",
                                          "ContainmentFailed",
                                          "LaunchFailed",
                                          "OutputLimitExceeded",
                                          "OutputInvalid",
                                          "CleanupFailed",
                                          "JournalFailed",
                                          "StoreFull",
                                          "HostStopped",
                                          "UnsupportedPlatform" };
        auto index = static_cast<size_t>(error);
        return index < sizeof(names) / sizeof(names[0]) ? names[index] : "InvalidError";
    }
    ToolCanonicalResult CanonicalToolCommand(const ToolExecutionCommandV2& c)
    {
        if (c.m_version != 2)
        {
            return { ToolError::UnsupportedVersion, {}, {} };
        }
        if (!ToolSafeId(c.m_providerId) || !Version(c.m_providerVersion) || !ToolSafeId(c.m_commandId) || !ToolSafeId(c.m_probeId) ||
            c.m_profile != ToolExecutionProfile || c.m_argumentConvention != ToolArgumentConvention || !c.m_timeoutMilliseconds ||
            c.m_timeoutMilliseconds > 1800000 || !c.m_maxProcesses || c.m_maxProcesses > 8 || !c.m_memoryBytes ||
            c.m_memoryBytes > ToolMaxArtifactBytes)
        {
            return { ToolError::InvalidContract, {}, {} };
        }
        for (const auto* list : { &c.m_inputKinds, &c.m_outputKinds, &c.m_environmentNames })
        {
            if (!Unique(
                    *list,
                    [](const auto& v) -> const AZStd::string&
                    {
                        return v;
                    },
                    32))
            {
                return { ToolError::InvalidContract, {}, {} };
            }
            for (const auto& v : *list)
            {
                if (!(list == &c.m_environmentNames ? EnvironmentName(v) : Kind(v)))
                {
                    return { ToolError::InvalidContract, {}, {} };
                }
            }
        }
        rapidjson::StringBuffer b;
        Writer w(b);
        w.StartObject();
        Text(w, "contract", "foa-tool-command-v2");
        Number(w, "version", 2);
        Text(w, "provider", c.m_providerId);
        Text(w, "providerVersion", c.m_providerVersion);
        Text(w, "command", c.m_commandId);
        Text(w, "probe", c.m_probeId);
        Text(w, "profile", c.m_profile);
        Text(w, "arguments", c.m_argumentConvention);
        const char* keys[] = { "inputKinds", "outputKinds", "environmentNames" };
        size_t i = 0;
        for (auto list : { c.m_inputKinds, c.m_outputKinds, c.m_environmentNames })
        {
            std::sort(list.begin(), list.end());
            w.Key(keys[i++]);
            w.StartArray();
            for (const auto& v : list)
            {
                w.String(v.c_str());
            }
            w.EndArray();
        }
        Number(w, "timeoutMs", c.m_timeoutMilliseconds);
        Number(w, "processes", c.m_maxProcesses);
        Number(w, "memoryBytes", c.m_memoryBytes);
        w.EndObject();
        return Complete(b);
    }
    ToolCanonicalResult CanonicalToolRequest(const ToolInvocationRequestV2& r)
    {
        if (r.m_version != 2)
        {
            return { ToolError::UnsupportedVersion, {}, {} };
        }
        if (!ToolSafeId(r.m_attemptId) || !ToolSafeId(r.m_providerId) || !ToolSafeId(r.m_commandId) || !ToolSafeId(r.m_targetRootId) ||
            !ToolValidDigest(r.m_commandFingerprint) || !Files(r.m_inputs) || r.m_arguments.size() > ToolMaxArguments ||
            !r.m_timeoutMilliseconds || r.m_timeoutMilliseconds > 1800000 ||
            !Unique(
                r.m_outputs,
                [](const auto& v) -> const AZStd::string&
                {
                    return v.m_id;
                },
                ToolMaxFiles) ||
            !Unique(
                r.m_environment,
                [](const auto& v) -> const AZStd::string&
                {
                    return v.m_name;
                },
                ToolMaxEnvironment))
        {
            return { ToolError::InvalidContract, {}, {} };
        }
        std::set<AZStd::string> paths;
        for (const auto& o : r.m_outputs)
        {
            if (!ToolSafeId(o.m_id) || !ToolSafeRelativePath(o.m_relativePath) || !Kind(o.m_kind) || !o.m_maxBytes ||
                o.m_relativePath == "manifest.v2.json" || o.m_maxBytes > ToolMaxArtifactBytes || !paths.insert(o.m_relativePath).second)
            {
                return { ToolError::InvalidContract, {}, {} };
            }
        }
        size_t aggregate = 0;
        for (const auto& a : r.m_arguments)
        {
            if (a.m_kind == ToolArgumentKind::Secret)
            {
                return { ToolError::SecretUseUnsupported, {}, {} };
            }
            if (a.m_kind > ToolArgumentKind::Secret || !ToolSafeText(a.m_value) ||
                (a.m_kind == ToolArgumentKind::Literal && !a.m_value.empty() && (a.m_value.front() == '/' || a.m_value.front() == '\\')))
            {
                return { ToolError::InvalidContract, {}, {} };
            }
            if (a.m_kind == ToolArgumentKind::Input &&
                std::none_of(
                    r.m_inputs.begin(),
                    r.m_inputs.end(),
                    [&](const auto& f)
                    {
                        return f.m_id == a.m_value;
                    }))
            {
                return { ToolError::InvalidContract, {}, {} };
            }
            if (a.m_kind == ToolArgumentKind::Output &&
                std::none_of(
                    r.m_outputs.begin(),
                    r.m_outputs.end(),
                    [&](const auto& f)
                    {
                        return f.m_id == a.m_value;
                    }))
            {
                return { ToolError::InvalidContract, {}, {} };
            }
            if (a.m_kind == ToolArgumentKind::Scratch && !ToolSafeRelativePath(a.m_value))
            {
                return { ToolError::InvalidContract, {}, {} };
            }
            aggregate += a.m_value.size() + 1;
        }
        if (aggregate > 16384)
        {
            return { ToolError::InvalidContract, {}, {} };
        }
        aggregate = 0;
        for (const auto& v : r.m_environment)
        {
            if (!EnvironmentName(v.m_name) || !ToolSafeText(v.m_value))
            {
                return { ToolError::InvalidContract, {}, {} };
            }
            aggregate += v.m_name.size() + v.m_value.size() + 2;
        }
        if (aggregate > 16384)
        {
            return { ToolError::InvalidContract, {}, {} };
        }
        rapidjson::StringBuffer b;
        Writer w(b);
        w.StartObject();
        Text(w, "contract", "foa-tool-invocation-v2");
        Number(w, "version", 2);
        Text(w, "canonicalProfile", "foa-tool-invocation-canonical-json-v2");
        // Attempt identity intentionally stays outside reusable semantic request identity.
        Text(w, "commandFingerprint", r.m_commandFingerprint);
        Text(w, "provider", r.m_providerId);
        Text(w, "command", r.m_commandId);
        Text(w, "targetRoot", r.m_targetRootId);
        Number(w, "timeoutMs", r.m_timeoutMilliseconds);
        w.Key("arguments");
        w.StartArray();
        for (const auto& a : r.m_arguments)
        {
            w.StartObject();
            Number(w, "kind", static_cast<AZ::u8>(a.m_kind));
            Text(w, "value", a.m_value);
            w.EndObject();
        }
        w.EndArray();
        w.Key("inputs");
        FileArray(w, r.m_inputs);
        w.Key("outputs");
        w.StartArray();
        for (const auto& o : Sorted(r.m_outputs))
        {
            w.StartObject();
            Text(w, "id", o.m_id);
            Text(w, "path", o.m_relativePath);
            Text(w, "kind", o.m_kind);
            Number(w, "maxBytes", o.m_maxBytes);
            w.EndObject();
        }
        w.EndArray();
        auto env = r.m_environment;
        std::sort(
            env.begin(),
            env.end(),
            [](const auto& a, const auto& b)
            {
                return a.m_name < b.m_name;
            });
        w.Key("environment");
        w.StartArray();
        for (const auto& v : env)
        {
            w.StartObject();
            Text(w, "name", v.m_name);
            Text(w, "value", v.m_value);
            w.EndObject();
        }
        w.EndArray();
        w.EndObject();
        return Complete(b);
    }
    ToolResult ValidateToolRequest(const ToolInvocationRequestV2& r, const ToolExecutionCommandV2& c)
    {
        auto command = CanonicalToolCommand(c);
        if (!command)
        {
            return { command.m_error };
        }
        auto request = CanonicalToolRequest(r);
        if (!request)
        {
            return { request.m_error };
        }
        if (r.m_fingerprint != request.m_fingerprint || r.m_commandFingerprint != command.m_fingerprint ||
            r.m_providerId != c.m_providerId || r.m_commandId != c.m_commandId || r.m_timeoutMilliseconds > c.m_timeoutMilliseconds)
        {
            return { ToolError::InvalidContract };
        }
        auto contains = [](const auto& list, const auto& value)
        {
            return std::find(list.begin(), list.end(), value) != list.end();
        };
        for (const auto& f : r.m_inputs)
        {
            if (!contains(c.m_inputKinds, f.m_kind))
            {
                return { ToolError::InvalidContract };
            }
        }
        for (const auto& f : r.m_outputs)
        {
            if (!contains(c.m_outputKinds, f.m_kind))
            {
                return { ToolError::InvalidContract };
            }
        }
        for (const auto& v : r.m_environment)
        {
            if (!contains(c.m_environmentNames, v.m_name))
            {
                return { ToolError::InvalidContract };
            }
        }
        return {};
    }
    ToolCanonicalResult EncodeToolManifest(const ToolOutputManifestV2& m)
    {
        if (m.m_version != 2)
        {
            return { ToolError::UnsupportedVersion, {}, {} };
        }
        if (!ToolSafeId(m.m_attemptId) || !ToolValidDigest(m.m_requestFingerprint) || !Files(m.m_outputs))
        {
            return { ToolError::InvalidContract, {}, {} };
        }
        rapidjson::StringBuffer b;
        Writer w(b);
        w.StartObject();
        Text(w, "contract", "foa-tool-output-manifest-v2");
        Number(w, "version", 2);
        Text(w, "attempt", m.m_attemptId);
        Text(w, "requestFingerprint", m.m_requestFingerprint);
        w.Key("outputs");
        FileArray(w, m.m_outputs);
        w.EndObject();
        return Complete(b);
    }
    ToolResult DecodeToolManifest(const AZStd::string& json, ToolOutputManifestV2& output)
    {
        rapidjson::Document d;
        ToolOutputManifestV2 m;
        if (!Parse(json, d) || !Shape(d, { "contract", "version", "attempt", "requestFingerprint", "outputs" }) ||
            !d["contract"].IsString() ||
            AZStd::string(d["contract"].GetString(), d["contract"].GetStringLength()) != "foa-tool-output-manifest-v2" ||
            !d["version"].IsUint() || d["version"].GetUint() != 2 || !ReadText(d, "attempt", m.m_attemptId) ||
            !ReadText(d, "requestFingerprint", m.m_requestFingerprint) || !ReadFiles(d["outputs"], m.m_outputs) || !EncodeToolManifest(m))
        {
            return { ToolError::InvalidContract };
        }
        output = AZStd::move(m);
        return {};
    }
    ToolCanonicalResult EncodeToolRecord(const ToolInvocationRecordV2& r)
    {
        if (r.m_version != 2)
        {
            return { ToolError::UnsupportedVersion, {}, {} };
        }
        if (!StatusValid(r.m_status) || !ToolValidDigest(r.m_requestFingerprint) || !ToolValidDigest(r.m_commandFingerprint) ||
            !ToolValidDigest(r.m_profileFingerprint) || (!r.m_executableDigest.empty() && !ToolValidDigest(r.m_executableDigest)) ||
            !Files(r.m_outputs) || (r.m_exitCodeObserved && (!r.m_processId || !r.m_processCreationTime)) ||
            (r.m_status.m_verification == ToolVerification::Passed && (!r.m_exitCodeObserved || r.m_exitCode != 0)) ||
            (r.m_status.m_outcome == ToolOutcome::ExitedZero && (!r.m_exitCodeObserved || r.m_exitCode != 0)) ||
            (r.m_status.m_outcome == ToolOutcome::ExitedNonzero && (!r.m_exitCodeObserved || r.m_exitCode == 0)))
        {
            return { ToolError::InvalidContract, {}, {} };
        }
        rapidjson::StringBuffer b;
        Writer w(b);
        w.StartObject();
        Text(w, "contract", "foa-tool-invocation-record-v2");
        Number(w, "version", 2);
        Text(w, "attempt", r.m_status.m_attemptId);
        Text(w, "requestFingerprint", r.m_requestFingerprint);
        Text(w, "commandFingerprint", r.m_commandFingerprint);
        Text(w, "profileFingerprint", r.m_profileFingerprint);
        Text(w, "executableDigest", r.m_executableDigest);
        Number(w, "sequence", r.m_status.m_sequence);
        Number(w, "stage", static_cast<AZ::u8>(r.m_status.m_stage));
        Number(w, "outcome", static_cast<AZ::u8>(r.m_status.m_outcome));
        Number(w, "verification", static_cast<AZ::u8>(r.m_status.m_verification));
        Number(w, "cleanup", static_cast<AZ::u8>(r.m_status.m_cleanup));
        Number(w, "persistence", static_cast<AZ::u8>(r.m_status.m_persistence));
        Number(w, "error", static_cast<AZ::u8>(r.m_status.m_error));
        Number(w, "startedUtcMs", r.m_startedUtcMilliseconds);
        Number(w, "finishedUtcMs", r.m_finishedUtcMilliseconds);
        Number(w, "processId", r.m_processId);
        Number(w, "processCreationTime", r.m_processCreationTime);
        Number(w, "exitCode", r.m_exitCode);
        w.Key("exitObserved");
        w.Bool(r.m_exitCodeObserved);
        w.Key("descendantsTerminated");
        w.Bool(r.m_descendantsTerminated);
        w.Key("stdoutTruncated");
        w.Bool(r.m_stdoutTruncated);
        w.Key("stderrTruncated");
        w.Bool(r.m_stderrTruncated);
        w.Key("outputs");
        FileArray(w, r.m_outputs);
        w.EndObject();
        return Complete(b);
    }
    ToolResult DecodeToolRecord(const AZStd::string& json, ToolInvocationRecordV2& output)
    {
        rapidjson::Document d;
        ToolInvocationRecordV2 r;
        if (!Parse(json, d) ||
            !Shape(
                d,
                { "contract",
                  "version",
                  "attempt",
                  "requestFingerprint",
                  "commandFingerprint",
                  "profileFingerprint",
                  "executableDigest",
                  "sequence",
                  "stage",
                  "outcome",
                  "verification",
                  "cleanup",
                  "persistence",
                  "error",
                  "startedUtcMs",
                  "finishedUtcMs",
                  "processId",
                  "processCreationTime",
                  "exitCode",
                  "exitObserved",
                  "descendantsTerminated",
                  "stdoutTruncated",
                  "stderrTruncated",
                  "outputs" }) ||
            !d["contract"].IsString() ||
            AZStd::string(d["contract"].GetString(), d["contract"].GetStringLength()) != "foa-tool-invocation-record-v2" ||
            !d["version"].IsUint() || d["version"].GetUint() != 2)
        {
            return { ToolError::InvalidContract };
        }
        if (!ReadText(d, "attempt", r.m_status.m_attemptId) || !ReadText(d, "requestFingerprint", r.m_requestFingerprint) ||
            !ReadText(d, "commandFingerprint", r.m_commandFingerprint) || !ReadText(d, "profileFingerprint", r.m_profileFingerprint) ||
            !ReadText(d, "executableDigest", r.m_executableDigest) || !ReadNumber(d, "sequence", r.m_status.m_sequence) ||
            !ReadNumber(d, "startedUtcMs", r.m_startedUtcMilliseconds) || !ReadNumber(d, "finishedUtcMs", r.m_finishedUtcMilliseconds) ||
            !ReadNumber(d, "processId", r.m_processId) || !ReadNumber(d, "processCreationTime", r.m_processCreationTime) ||
            !ReadFiles(d["outputs"], r.m_outputs))
        {
            return { ToolError::InvalidContract };
        }
        for (const char* key : { "stage", "outcome", "verification", "cleanup", "persistence", "error" })
        {
            if (!d[key].IsUint() || d[key].GetUint() > 255)
            {
                return { ToolError::InvalidContract };
            }
        }
        r.m_status.m_stage = static_cast<ToolStage>(d["stage"].GetUint());
        r.m_status.m_outcome = static_cast<ToolOutcome>(d["outcome"].GetUint());
        r.m_status.m_verification = static_cast<ToolVerification>(d["verification"].GetUint());
        r.m_status.m_cleanup = static_cast<ToolCleanup>(d["cleanup"].GetUint());
        r.m_status.m_persistence = static_cast<ToolPersistence>(d["persistence"].GetUint());
        r.m_status.m_error = static_cast<ToolError>(d["error"].GetUint());
        if (!d["exitCode"].IsUint())
        {
            return { ToolError::InvalidContract };
        }
        r.m_exitCode = d["exitCode"].GetUint();
        for (const char* key : { "exitObserved", "descendantsTerminated", "stdoutTruncated", "stderrTruncated" })
        {
            if (!d[key].IsBool())
            {
                return { ToolError::InvalidContract };
            }
        }
        r.m_exitCodeObserved = d["exitObserved"].GetBool();
        r.m_descendantsTerminated = d["descendantsTerminated"].GetBool();
        r.m_stdoutTruncated = d["stdoutTruncated"].GetBool();
        r.m_stderrTruncated = d["stderrTruncated"].GetBool();
        if (!EncodeToolRecord(r))
        {
            return { ToolError::InvalidContract };
        }
        output = AZStd::move(r);
        return {};
    }
} // namespace ExternalToolchain
