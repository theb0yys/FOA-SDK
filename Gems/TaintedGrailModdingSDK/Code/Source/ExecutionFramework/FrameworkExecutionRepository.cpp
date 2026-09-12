/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkExecutionRepository.h"
#include <AzCore/JSON/document.h>
#include <AzCore/JSON/stringbuffer.h>
#include <AzCore/JSON/writer.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/set.h>
#include <AzCore/std/sort.h>
#include <filesystem>
#include <thread>
#include <vector>
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
#include <Execution/Platform/Windows/ToolSandbox_Windows.h>
#endif

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
        AZStd::string Quote(const AZStd::string& value)
        {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            writer.String(value.data(), static_cast<rapidjson::SizeType>(value.size()));
            return { buffer.GetString(), buffer.GetSize() };
        }
        AZStd::string Key(const StoredAttempt& a)
        {
            return a.m_executionId + ".a" + AZStd::string::format("%llu", static_cast<unsigned long long>(a.m_attempt));
        }
        AZStd::string Envelope(const char* kind, const AZStd::string& body)
        {
            return "{\"contract_id\":\"foa-framework-execution-store-v1\",\"version\":1,\"kind\":" + Quote(kind) + ",\"body\":" + body +
                "}";
        }
        bool Fields(const rapidjson::Value& j, std::initializer_list<const char*> fields)
        {
            if (!j.IsObject() || j.MemberCount() != fields.size())
            {
                return false;
            }
            AZStd::set<AZStd::string> seen;
            for (auto it = j.MemberBegin(); it != j.MemberEnd(); ++it)
            {
                if (!seen.insert(AZStd::string(it->name.GetString(), it->name.GetStringLength())).second)
                {
                    return false;
                }
            }
            for (auto field : fields)
            {
                if (!j.HasMember(field))
                {
                    return false;
                }
            }
            return true;
        }
        bool Parse(const AZStd::string& bytes, const char* kind, rapidjson::Document& d)
        {
            // Envelopes contain canonical documents as strings, so no recursive DOM is required.
            size_t depth = 0;
            bool quoted = false, escaped = false;
            for (char c : bytes)
            {
                if (quoted)
                {
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (c == '\\')
                    {
                        escaped = true;
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
                    if (++depth > 5)
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
            d.Parse<rapidjson::kParseValidateEncodingFlag>(bytes.data(), bytes.size());
            return !d.HasParseError() && Fields(d, { "contract_id", "version", "kind", "body" }) && d["contract_id"].IsString() &&
                AZStd::string_view(d["contract_id"].GetString()) == StoreContract && d["version"].IsUint() && d["version"].GetUint() == 1 &&
                d["kind"].IsString() && AZStd::string_view(d["kind"].GetString()) == kind;
        }
        AZStd::string Text(const rapidjson::Value& j)
        {
            return j.IsString() ? AZStd::string(j.GetString(), j.GetStringLength()) : AZStd::string{};
        }
        AZStd::string Intent(const StoredAttempt& a)
        {
            return Envelope(
                "intent",
                "{\"execution\":" + Quote(a.m_executionId) + ",\"operation\":" + Quote(a.m_operationKey) +
                    ",\"attempt\":" + AZStd::string::format("%llu", static_cast<unsigned long long>(a.m_attempt)) +
                    ",\"plan\":" + Quote(CE::Canonicalize(a.m_plan).GetValue().m_json) + "}");
        }
        bool ReadIntent(const AZStd::string& bytes, StoredAttempt& a)
        {
            rapidjson::Document d;
            if (!Parse(bytes, "intent", d) || !Fields(d["body"], { "execution", "operation", "attempt", "plan" }))
            {
                return false;
            }
            const auto& b = d["body"];
            if (!b["attempt"].IsUint64())
            {
                return false;
            }
            a.m_executionId = Text(b["execution"]);
            a.m_operationKey = Text(b["operation"]);
            a.m_attempt = b["attempt"].GetUint64();
            return CE::IsStableId(a.m_executionId) && CE::IsDigest(a.m_operationKey) && a.m_attempt > 0 && a.m_attempt <= MaximumAttempts &&
                Decode(Text(b["plan"]), a.m_plan) && a.m_operationKey == a.m_plan.m_fingerprint &&
                Envelope(
                    "intent",
                    "{\"execution\":" + Quote(a.m_executionId) + ",\"operation\":" + Quote(a.m_operationKey) +
                        ",\"attempt\":" + AZStd::string::format("%llu", static_cast<unsigned long long>(a.m_attempt)) +
                        ",\"plan\":" + Quote(Text(b["plan"])) + "}") == bytes;
        }
        AZStd::string Finished(const StoredAttempt& a)
        {
            AZStd::string body = "{\"receipt\":" + Quote(CE::Canonicalize(*a.m_receipt).GetValue().m_json) + ",\"tools\":[";
            bool first = true;
            for (const auto& tool : a.m_tools)
            {
                if (!first)
                {
                    body += ",";
                }
                first = false;
                body += Quote(ET::EncodeToolRecord(tool).m_json);
            }
            return Envelope("finished", body + "],\"quarantined\":" + (a.m_quarantined ? "true" : "false") + "}");
        }
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        Result StorageUsage(
            const AZStd::string& root, AZ::u64 additional, const std::atomic_bool* cancelled, AZ::u64* metadataSize = nullptr)
        {
            if (additional > MaximumStore)
            {
                return { Error::StoreFull };
            }
            AZ::u64 bytes = 0, metadata = 0;
            size_t entries = 0;
            std::error_code ec;
            auto wide = ET::Windows::Wide(root);
            for (std::filesystem::recursive_directory_iterator it(wide, ec), end; it != end && !ec; it.increment(ec))
            {
                if (cancelled && *cancelled)
                {
                    return { Error::Cancelled };
                }
                if (++entries > MaximumLineages * MaximumAttempts * (CE::MaximumPhases * 3 + 8))
                {
                    return { Error::StoreFull };
                }
                const auto path = it->path();
                auto relative = ET::Windows::Utf8(path.lexically_relative(wide).generic_wstring());
                const auto attributes = GetFileAttributesW(path.c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                {
                    return { Error::CorruptStore };
                }
                if (relative == "writer.lock" || relative == "supervisor/host.lock")
                {
                    // These exact zero-byte files are held exclusively by the two repository writers.
                    if (it->file_size(ec) != 0 || ec)
                    {
                        return { Error::CorruptStore };
                    }
                    continue;
                }
                const bool directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                ET::Windows::PinnedPath pin;
                if (!pin.Open(ET::Windows::Utf8(path.wstring()), directory))
                {
                    return { Error::CorruptStore };
                }
                if (directory)
                {
                    continue;
                }
                if (pin.m_bytes > MaximumStore - bytes)
                {
                    return { Error::StoreFull };
                }
                bytes += pin.m_bytes;
                if (relative.find("payloads/") != 0 && relative.find("supervisor/staging/") != 0)
                {
                    if (pin.m_bytes > MaximumMetadata - metadata)
                    {
                        return { Error::StoreFull };
                    }
                    metadata += pin.m_bytes;
                }
            }
            if (ec)
            {
                return { Error::StorageFailed };
            }
            if (metadataSize)
            {
                *metadataSize = metadata;
            }
            return { additional > MaximumStore - bytes ? Error::StoreFull : Error::None };
        }
        bool PublishNew(const AZStd::string& destination, const AZStd::string& bytes)
        {
            const auto path = ET::Windows::Wide(destination);
            ET::Windows::PinnedPath parent;
            if (!parent.Open(ET::Windows::Utf8(std::filesystem::path(path).parent_path().wstring()), true))
            {
                return false;
            }
            auto temporary = path + L"." + ET::Windows::Wide(ET::Windows::NewId()) + L".tmp";
            ET::Windows::Handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!file)
            {
                return false;
            }
            DWORD written = 0;
            const bool complete = WriteFile(file.Get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
                written == bytes.size() && FlushFileBuffers(file.Get());
            file.Reset();
            // No REPLACE_EXISTING: even a concurrent unexpected destination is never overwritten.
            return complete && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH);
        }
        bool CanonicalRequest(const rapidjson::Value& request, const AZStd::string& attempt, const AZStd::string& json)
        {
            if (!request["version"].IsUint() || !request["timeoutMs"].IsUint() || !request["arguments"].IsArray() ||
                request["arguments"].Size() > ET::ToolMaxArguments || !request["inputs"].IsArray() ||
                request["inputs"].Size() > ET::ToolMaxFiles || !request["outputs"].IsArray() ||
                request["outputs"].Size() > ET::ToolMaxFiles || !request["environment"].IsArray() ||
                request["environment"].Size() > ET::ToolMaxEnvironment)
            {
                return false;
            }
            ET::ToolInvocationRequestV2 value;
            value.m_attemptId = attempt;
            value.m_version = request["version"].GetUint();
            value.m_timeoutMilliseconds = request["timeoutMs"].GetUint();
            value.m_providerId = Text(request["provider"]);
            value.m_commandId = Text(request["command"]);
            value.m_targetRootId = Text(request["targetRoot"]);
            value.m_commandFingerprint = Text(request["commandFingerprint"]);
            for (const auto& item : request["arguments"].GetArray())
            {
                if (!Fields(item, { "kind", "value" }) || !item["kind"].IsUint() ||
                    item["kind"].GetUint() > static_cast<unsigned>(ET::ToolArgumentKind::Secret))
                {
                    return false;
                }
                value.m_arguments.push_back({ static_cast<ET::ToolArgumentKind>(item["kind"].GetUint()), Text(item["value"]) });
            }
            for (const auto& item : request["inputs"].GetArray())
            {
                if (!Fields(item, { "id", "root", "path", "kind", "sha256", "bytes" }) || !item["bytes"].IsUint64())
                {
                    return false;
                }
                value.m_inputs.push_back(
                    { Text(item["id"]),
                      Text(item["root"]),
                      Text(item["path"]),
                      Text(item["kind"]),
                      Text(item["sha256"]),
                      item["bytes"].GetUint64() });
            }
            for (const auto& item : request["outputs"].GetArray())
            {
                if (!Fields(item, { "id", "path", "kind", "maxBytes" }) || !item["maxBytes"].IsUint64())
                {
                    return false;
                }
                value.m_outputs.push_back({ Text(item["id"]), Text(item["path"]), Text(item["kind"]), item["maxBytes"].GetUint64() });
            }
            for (const auto& item : request["environment"].GetArray())
            {
                if (!Fields(item, { "name", "value" }))
                {
                    return false;
                }
                value.m_environment.push_back({ Text(item["name"]), Text(item["value"]) });
            }
            auto encoded = ET::CanonicalToolRequest(value);
            return encoded && encoded.m_json == json;
        }
        bool ReadPhaseIntent(const AZStd::string& bytes, const StoredAttempt& attempt, StoredPhaseIntent& output)
        {
            rapidjson::Document d, wrapped;
            if (!Parse(bytes, "phase-intent", d) || !Fields(d["body"], { "attempt", "phase", "request" }))
            {
                return false;
            }
            const auto& b = d["body"];
            const auto json = Text(b["request"]);
            if (json.size() > ET::ToolMaxDocumentBytes || !Parse(Envelope("request", json), "request", wrapped))
            {
                return false;
            }
            const auto& request = wrapped["body"];
            if (!Fields(
                    request,
                    { "contract",
                      "version",
                      "canonicalProfile",
                      "commandFingerprint",
                      "provider",
                      "command",
                      "targetRoot",
                      "timeoutMs",
                      "arguments",
                      "inputs",
                      "outputs",
                      "environment" }))
            {
                return false;
            }
            if (!CanonicalRequest(request, Text(b["attempt"]), json))
            {
                return false;
            }
            StoredPhaseIntent value;
            value.m_toolAttemptId = Text(b["attempt"]);
            value.m_phaseFingerprint = Text(b["phase"]);
            value.m_requestFingerprint = ET::ToolDigest(json);
            value.m_commandFingerprint = Text(request["commandFingerprint"]);
            value.m_providerId = Text(request["provider"]);
            value.m_commandId = Text(request["command"]);
            if (!ET::ToolSafeId(value.m_toolAttemptId) || !CE::IsDigest(value.m_commandFingerprint) ||
                !AZStd::any_of(
                    attempt.m_plan.m_phases.begin(),
                    attempt.m_plan.m_phases.end(),
                    [&](const auto& phase)
                    {
                        return phase.m_fingerprint == value.m_phaseFingerprint && phase.m_providerId == value.m_providerId &&
                            phase.m_commandId == value.m_commandId;
                    }))
            {
                return false;
            }
            const auto canonical = Envelope(
                "phase-intent",
                "{\"attempt\":" + Quote(value.m_toolAttemptId) + ",\"phase\":" + Quote(value.m_phaseFingerprint) +
                    ",\"request\":" + Quote(json) + "}");
            if (canonical != bytes)
            {
                return false;
            }
            output = AZStd::move(value);
            return true;
        }
        bool MatchesIntent(const StoredPhaseIntent& intent, const ET::ToolInvocationRecordV2& tool)
        {
            return intent.m_toolAttemptId == tool.m_status.m_attemptId && intent.m_requestFingerprint == tool.m_requestFingerprint &&
                intent.m_commandFingerprint == tool.m_commandFingerprint &&
                tool.m_profileFingerprint == ET::ToolDigest(ET::ToolExecutionProfile) && tool.m_status.m_stage == ET::ToolStage::Terminal &&
                ET::EncodeToolRecord(tool);
        }
        bool ReadObservations(const AZStd::string& root, StoredAttempt& attempt)
        {
            std::error_code ec;
            const auto folder = ET::Windows::Wide(root + "/transactions/" + Key(attempt));
            for (std::filesystem::directory_iterator it(folder, ec), end; it != end && !ec; it.increment(ec))
            {
                if (it->path().extension() != L".intent")
                {
                    continue;
                }
                AZStd::string bytes;
                StoredPhaseIntent intent;
                if (attempt.m_phaseIntents.size() >= attempt.m_plan.m_phases.size() ||
                    !ET::Windows::ReadFileBounded(it->path().wstring(), 2 * ET::ToolMaxDocumentBytes + 4096, bytes) ||
                    !ReadPhaseIntent(bytes, attempt, intent) || it->path().stem().wstring() != ET::Windows::Wide(intent.m_toolAttemptId) ||
                    AZStd::any_of(
                        attempt.m_phaseIntents.begin(),
                        attempt.m_phaseIntents.end(),
                        [&](const auto& old)
                        {
                            return old.m_toolAttemptId == intent.m_toolAttemptId || old.m_phaseFingerprint == intent.m_phaseFingerprint;
                        }))
                {
                    return false;
                }
                attempt.m_phaseIntents.push_back(AZStd::move(intent));
            }
            if (ec)
            {
                return false;
            }
            for (std::filesystem::directory_iterator it(folder, ec), end; it != end && !ec; it.increment(ec))
            {
                const auto name = ET::Windows::Utf8(it->path().filename().wstring());
                if (name.find("recovered.") != 0)
                {
                    continue;
                }
                AZStd::string bytes;
                rapidjson::Document d;
                ET::ToolInvocationRecordV2 record;
                if (!ET::Windows::ReadFileBounded(it->path().wstring(), 2 * ET::ToolMaxDocumentBytes + 4096, bytes) ||
                    !Parse(bytes, "recovery-observation", d) || !ET::DecodeToolRecord(Text(d["body"]), record) ||
                    name != "recovered." + record.m_status.m_attemptId + ".json" ||
                    !AZStd::any_of(
                        attempt.m_phaseIntents.begin(),
                        attempt.m_phaseIntents.end(),
                        [&](const auto& intent)
                        {
                            return MatchesIntent(intent, record);
                        }) ||
                    bytes != Envelope("recovery-observation", Quote(ET::EncodeToolRecord(record).m_json)))
                {
                    return false;
                }
                attempt.m_recoveredTools.push_back(AZStd::move(record));
            }
            return !ec;
        }
        bool ToolIntents(const AZStd::string& root, const StoredAttempt& attempt)
        {
            size_t toolIndex = 0;
            for (const auto& receipt : attempt.m_receipt->m_phaseReceipts)
            {
                if (!receipt.m_extension)
                {
                    continue;
                }
                if (toolIndex >= attempt.m_tools.size())
                {
                    return false;
                }
                const auto& tool = attempt.m_tools[toolIndex++];
                AZStd::string bytes;
                auto path = root + "/transactions/" + Key(attempt) + "/" + tool.m_status.m_attemptId + ".intent";
                rapidjson::Document document;
                if (!ET::ToolSafeId(tool.m_status.m_attemptId) ||
                    !ET::Windows::ReadFileBounded(ET::Windows::Wide(path), 2 * ET::ToolMaxDocumentBytes + 4096, bytes) ||
                    !Parse(bytes, "phase-intent", document) || !Fields(document["body"], { "attempt", "phase", "request" }))
                {
                    return false;
                }
                const auto& b = document["body"];
                if (Text(b["attempt"]) != tool.m_status.m_attemptId || Text(b["phase"]) != receipt.m_phasePlan.m_fingerprint ||
                    ET::ToolDigest(Text(b["request"])) != tool.m_requestFingerprint)
                {
                    return false;
                }
                rapidjson::Document wrapped;
                const auto json = Text(b["request"]);
                if (json.size() > ET::ToolMaxDocumentBytes)
                {
                    return false;
                }
                // The exact producer-validated bytes are retained as a bounded observation; never replayed.
                if (!Parse(Envelope("request", json), "request", wrapped))
                {
                    return false;
                }
                const auto& request = wrapped["body"];
                if (!Fields(
                        request,
                        { "contract",
                          "version",
                          "canonicalProfile",
                          "commandFingerprint",
                          "provider",
                          "command",
                          "targetRoot",
                          "timeoutMs",
                          "arguments",
                          "inputs",
                          "outputs",
                          "environment" }) ||
                    Text(request["provider"]) != receipt.m_providerId || Text(request["command"]) != receipt.m_commandId ||
                    Text(request["commandFingerprint"]) != tool.m_commandFingerprint)
                {
                    return false;
                }
            }
            return toolIndex == attempt.m_tools.size();
        }
#endif
        bool ValidResult(const StoredAttempt& a)
        {
            if (!a.m_receipt || a.m_receipt->m_id != a.m_executionId || a.m_tools.size() > a.m_plan.m_phases.size())
            {
                return false;
            }
            AZStd::vector<CE::PhaseExtensionReferenceV1> extensions;
            size_t toolIndex = 0;
            for (const auto& phase : a.m_receipt->m_phaseReceipts)
            {
                if (!phase.m_extension)
                {
                    if (phase.m_attempt != 0)
                    {
                        return false;
                    }
                    continue;
                }
                if (toolIndex == a.m_tools.size())
                {
                    return false;
                }
                const auto& tool = a.m_tools[toolIndex++];
                auto encoded = ET::EncodeToolRecord(tool);
                if (!encoded || tool.m_status.m_stage != ET::ToolStage::Terminal ||
                    phase.m_extension->m_extensionContractId != "foa.m2-observation.v2" ||
                    phase.m_extension->m_canonicalJson != encoded.m_json ||
                    phase.m_extension->m_extensionFingerprint != encoded.m_fingerprint ||
                    (phase.m_outcome == CE::Outcome::SUCCEEDED && !ET::ToolSucceeded(tool)))
                {
                    return false;
                }
                extensions.push_back(*phase.m_extension);
            }
            return toolIndex == a.m_tools.size() &&
                CE::Validate(*a.m_receipt, a.m_plan, a.m_receipt->m_authorization, extensions).IsSuccess();
        }
    } // namespace
    bool ValidateStoredPlan(const CE::CapabilityExecutionPlanV1& plan)
    {
        CE::CapabilityDescriptorV1 d;
        CE::CapabilityExecutionRequestV1 r;
        AZStd::vector<CE::CapabilityProviderBindingV1> bindings;
        if (!Decode(plan.m_descriptor.m_canonicalJson, d) || !Decode(plan.m_request.m_canonicalJson, r))
        {
            return false;
        }
        for (const auto& phase : plan.m_phases)
        {
            CE::CapabilityProviderBindingV1 b;
            if (!Decode(phase.m_binding.m_canonicalJson, b))
            {
                return false;
            }
            bindings.push_back(AZStd::move(b));
        }
        return CE::Validate(plan, d, r, bindings).IsSuccess();
    }
    struct FrameworkExecutionRepository::Impl
    {
        mutable std::mutex m_mutex;
        AZStd::string m_root;
        Context m_context;
        AZStd::vector<StoredAttempt> m_attempts;
        AZ::u64 m_bytes = 0;
        bool m_healthy = false;
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        ET::Windows::PinnedPath m_pin;
        ET::Windows::Handle m_writer;
        bool Write(const AZStd::string& path, const AZStd::string& bytes)
        {
            if (!m_healthy || bytes.size() > 8 * CE::MaximumCanonicalBytes || m_bytes + bytes.size() > MaximumMetadata)
            {
                return false;
            }
            if (!PublishNew(m_root + "/" + path, bytes))
            {
                m_healthy = false;
                return false;
            }
            m_bytes += bytes.size();
            return true;
        }
#endif
    };
    FrameworkExecutionRepository::FrameworkExecutionRepository()
        : m_impl(std::make_unique<Impl>())
    {
    }
    FrameworkExecutionRepository::~FrameworkExecutionRepository() = default;
    Result FrameworkExecutionRepository::Open(const AZStd::string& root, const Context& context)
    {
        if (!CE::IsStableId(context.m_workspaceId) || !CE::IsStableId(context.m_packId) || !CE::IsDigest(context.m_profileFingerprint))
        {
            return { Error::Invalid };
        }
        auto next = std::make_unique<Impl>();
        next->m_root = root;
        next->m_context = context;
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        auto wide = ET::Windows::Wide(root);
        if (!next->m_pin.Open(root, true))
        {
            if (!ET::Windows::CreatePrivateDirectory(wide) || !next->m_pin.Open(root, true))
            {
                return { Error::StorageFailed };
            }
        }
        next->m_writer.Reset(CreateFileW(
            (wide + L"\\writer.lock").c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_ALWAYS,
            FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr));
        if (!next->m_writer)
        {
            return { Error::Busy };
        }
        FILE_ATTRIBUTE_TAG_INFO tag{};
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandleEx(next->m_writer.Get(), FileAttributeTagInfo, &tag, sizeof(tag)) ||
            (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || !GetFileInformationByHandle(next->m_writer.Get(), &info) ||
            info.nNumberOfLinks != 1)
        {
            return { Error::CorruptStore };
        }
        const auto header = Envelope(
            "context",
            "{\"workspace\":" + Quote(context.m_workspaceId) + ",\"pack\":" + Quote(context.m_packId) +
                ",\"profile\":" + Quote(context.m_profileFingerprint) + "}");
        AZStd::string existing;
        auto headerPath = wide + L"\\header.json";
        if (GetFileAttributesW(headerPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            // Missing identity in an existing nonempty store is corruption, not a new context.
            std::error_code headerError;
            for (std::filesystem::directory_iterator it(wide, headerError), end; it != end && !headerError; it.increment(headerError))
            {
                if (it->path().filename() != L"writer.lock")
                {
                    return { Error::CorruptStore };
                }
            }
            if (headerError || !PublishNew(root + "/header.json", header))
            {
                return { Error::StorageFailed };
            }
        }
        else if (!ET::Windows::ReadFileBounded(headerPath, 4096, existing) || existing != header)
        {
            return { Error::CorruptStore };
        }
        for (const char* folder : { "transactions", "payloads" })
        {
            ET::Windows::PinnedPath pin;
            auto path = root + "/" + folder;
            if (!pin.Open(path, true) && !ET::Windows::CreatePrivateDirectory(ET::Windows::Wide(path)))
            {
                return { Error::StorageFailed };
            }
        }
        // At most sixteen joined readers validate independent immutable transactions off Editor event paths.
        // Each reader retains the complete strict/contextual checks; only publication remains serial.
        struct Loaded
        {
            StoredAttempt m_attempt;
            AZ::u64 m_bytes = 0;
            Result m_result;
        };
        std::error_code ec;
        std::vector<std::filesystem::path> paths;
        for (std::filesystem::directory_iterator it(wide + L"\\transactions", ec), end; it != end && !ec; it.increment(ec))
        {
            if (paths.size() >= MaximumLineages * MaximumAttempts)
            {
                return { Error::StoreFull };
            }
            paths.push_back(it->path());
        }
        if (ec)
        {
            return { Error::StorageFailed };
        }
        AZ::u64 metadataSize = 0;
        auto usage = StorageUsage(root, 0, nullptr, &metadataSize);
        if (!usage)
        {
            return usage;
        }
        std::vector<Loaded> loaded(paths.size());
        std::atomic_size_t cursor{ 0 };
        auto read = [&](const std::filesystem::path& directory, StoredAttempt& attempt, AZ::u64& metadataBytes) -> Result
        {
            ET::Windows::PinnedPath pin;
            if (!ET::ToolSafeId(ET::Windows::Utf8(directory.filename().wstring())) ||
                !pin.Open(ET::Windows::Utf8(directory.wstring()), true))
            {
                return { Error::CorruptStore };
            }
            auto path = directory.wstring();
            const auto name = ET::Windows::Utf8(directory.filename().wstring());
            AZStd::string bytes;
            if (!ET::Windows::ReadFileBounded(path + L"\\intent.json", 4 * CE::MaximumCanonicalBytes, bytes) ||
                !ReadIntent(bytes, attempt) || Key(attempt) != name)
            {
                return { Error::CorruptStore };
            }
            metadataBytes += bytes.size();
            auto markerPath = path + L"\\committed";
            if (GetFileAttributesW(markerPath.c_str()) == INVALID_FILE_ATTRIBUTES)
            {
                attempt.m_quarantined = true;
            }
            else
            {
                AZStd::string marker;
                if (!ET::Windows::ReadFileBounded(markerPath, 128, marker) ||
                    !ET::Windows::ReadFileBounded(path + L"\\finished.json", 8 * CE::MaximumCanonicalBytes, bytes) ||
                    marker != ET::ToolDigest(bytes))
                {
                    return { Error::CorruptStore };
                }
                metadataBytes += bytes.size();
                rapidjson::Document d;
                if (!Parse(bytes, "finished", d) || !Fields(d["body"], { "receipt", "tools", "quarantined" }) ||
                    !d["body"]["quarantined"].IsBool() || !d["body"]["tools"].IsArray() || d["body"]["tools"].Size() > CE::MaximumPhases)
                {
                    return { Error::CorruptStore };
                }
                CE::CapabilityExecutionReceiptV1 receipt;
                if (!Decode(Text(d["body"]["receipt"]), receipt))
                {
                    return { Error::CorruptStore };
                }
                attempt.m_receipt = AZStd::move(receipt);
                attempt.m_quarantined = d["body"]["quarantined"].GetBool();
                for (const auto& value : d["body"]["tools"].GetArray())
                {
                    ET::ToolInvocationRecordV2 tool;
                    if (!ET::DecodeToolRecord(Text(value), tool))
                    {
                        return { Error::CorruptStore };
                    }
                    attempt.m_tools.push_back(AZStd::move(tool));
                }
                if (!ValidResult(attempt) || !ToolIntents(root, attempt) || Finished(attempt) != bytes)
                {
                    return { Error::CorruptStore };
                }
            }
            if (!ReadObservations(root, attempt))
            {
                return { Error::CorruptStore };
            }
            // Decode(plan) already strictly and contextually validated the complete request.
            // Check the store's extra context binding without redoing every M1 canonical hash.
            rapidjson::Document request;
            request.Parse(attempt.m_plan.m_request.m_canonicalJson.c_str());
            if (request.HasParseError() || !request.IsObject() || Text(request["workspaceId"]) != context.m_workspaceId ||
                Text(request["packId"]) != context.m_packId || Text(request["profileFingerprint"]) != context.m_profileFingerprint)
            {
                return { Error::CorruptStore };
            }
            for (const auto& tool : attempt.m_tools)
            {
                if (tool.m_status.m_cleanup != ET::ToolCleanup::Complete && tool.m_status.m_cleanup != ET::ToolCleanup::NotRequired)
                {
                    attempt.m_quarantined = true;
                }
            }
            return {};
        };
        auto load = [&]
        {
            for (size_t index = cursor.fetch_add(1); index < paths.size(); index = cursor.fetch_add(1))
            {
                loaded[index].m_result = read(paths[index], loaded[index].m_attempt, loaded[index].m_bytes);
            }
        };
        std::vector<std::thread> readers;
        const size_t concurrency = AZStd::min(size_t{ 16 }, paths.size());
        for (size_t index = 1; index < concurrency; ++index)
        {
            readers.emplace_back(load);
        }
        load();
        for (auto& reader : readers)
        {
            reader.join();
        }
        AZStd::set<AZStd::string> lineages, executionIds;
        for (auto& item : loaded)
        {
            if (!item.m_result)
            {
                return item.m_result;
            }
            if (!executionIds.insert(item.m_attempt.m_executionId).second)
            {
                return { Error::CorruptStore };
            }
            next->m_bytes += item.m_bytes;
            lineages.insert(item.m_attempt.m_operationKey);
            if (lineages.size() > MaximumLineages || next->m_bytes > MaximumMetadata)
            {
                return { Error::StoreFull };
            }
            next->m_attempts.push_back(AZStd::move(item.m_attempt));
        }
        AZStd::sort(
            next->m_attempts.begin(),
            next->m_attempts.end(),
            [](const auto& a, const auto& b)
            {
                return a.m_operationKey == b.m_operationKey ? a.m_attempt < b.m_attempt : a.m_operationKey < b.m_operationKey;
            });
        AZStd::string previousOperation;
        AZ::u64 previousAttempt = 0;
        for (const auto& attempt : next->m_attempts)
        {
            if (attempt.m_operationKey != previousOperation)
            {
                previousAttempt = 0;
                previousOperation = attempt.m_operationKey;
            }
            if (attempt.m_attempt != ++previousAttempt)
            {
                return { Error::CorruptStore };
            }
        }
        next->m_bytes = metadataSize;
        next->m_healthy = true;
        m_impl = std::move(next);
        return {};
#else
        return { Error::Unsupported };
#endif
    }
    Result FrameworkExecutionRepository::Begin(const StoredAttempt& a)
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (!m_impl->m_healthy)
        {
            return { Error::StorageFailed };
        }
        if (!ValidateStoredPlan(a.m_plan) || a.m_operationKey != a.m_plan.m_fingerprint || !CE::IsStableId(a.m_executionId) ||
            !a.m_attempt || a.m_attempt > MaximumAttempts || a.m_receipt || !ET::ToolSafeId(Key(a)))
        {
            return { Error::Invalid };
        }
        CE::CapabilityExecutionRequestV1 request;
        if (!Decode(a.m_plan.m_request.m_canonicalJson, request) || !m_impl->m_context.Matches(request))
        {
            return { Error::Invalid };
        }
        AZStd::set<AZStd::string> lineages;
        AZ::u64 last = 0;
        for (const auto& prior : m_impl->m_attempts)
        {
            lineages.insert(prior.m_operationKey);
            if (prior.m_executionId == a.m_executionId)
            {
                return { Error::Collision };
            }
            if (prior.m_operationKey == a.m_operationKey)
            {
                if (!prior.m_receipt || prior.m_quarantined)
                {
                    return { Error::Quarantined };
                }
                last = AZStd::max(last, prior.m_attempt);
            }
        }
        if (a.m_attempt != last + 1)
        {
            return { Error::Invalid };
        }
        if (!lineages.count(a.m_operationKey) && lineages.size() >= MaximumLineages)
        {
            return { Error::StoreFull };
        }
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        auto intentBytes = Intent(a);
        if (intentBytes.size() > MaximumMetadata - m_impl->m_bytes)
        {
            return { Error::StoreFull };
        }
        auto folder = "transactions/" + Key(a);
        if (!ET::Windows::CreatePrivateDirectory(ET::Windows::Wide(m_impl->m_root + "/" + folder)) ||
            !m_impl->Write(folder + "/intent.json", intentBytes))
        {
            m_impl->m_healthy = false;
            return { Error::StorageFailed };
        }
        m_impl->m_attempts.push_back(a);
        return {};
#else
        return { Error::Unsupported };
#endif
    }
    Result FrameworkExecutionRepository::PhaseIntent(
        const AZStd::string& id, AZ::u64 attempt, const AZStd::string& phaseFingerprint, const ET::ToolInvocationRequestV2& invocation)
    {
        std::lock_guard lock(m_impl->m_mutex);
        auto found = AZStd::find_if(
            m_impl->m_attempts.begin(),
            m_impl->m_attempts.end(),
            [&](const auto& a)
            {
                return a.m_executionId == id && a.m_attempt == attempt;
            });
        auto canonical = ET::CanonicalToolRequest(invocation);
        if (found == m_impl->m_attempts.end() || found->m_receipt || !canonical || invocation.m_fingerprint != canonical.m_fingerprint ||
            !ET::ToolSafeId(invocation.m_attemptId))
        {
            return { Error::Invalid };
        }
        auto phase = AZStd::find_if(
            found->m_plan.m_phases.begin(),
            found->m_plan.m_phases.end(),
            [&](const auto& p)
            {
                return p.m_fingerprint == phaseFingerprint && p.m_providerId == invocation.m_providerId &&
                    p.m_commandId == invocation.m_commandId;
            });
        if (phase == found->m_plan.m_phases.end())
        {
            return { Error::Invalid };
        }
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        auto file = "transactions/" + Key(*found) + "/" + invocation.m_attemptId + ".intent";
        if (GetFileAttributesW(ET::Windows::Wide(m_impl->m_root + "/" + file).c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            return { Error::Collision };
        }
        if (AZStd::any_of(
                found->m_phaseIntents.begin(),
                found->m_phaseIntents.end(),
                [&](const auto& old)
                {
                    return old.m_phaseFingerprint == phaseFingerprint;
                }))
        {
            return { Error::Collision };
        }
        const auto bytes = Envelope(
            "phase-intent",
            "{\"attempt\":" + Quote(invocation.m_attemptId) + ",\"phase\":" + Quote(phaseFingerprint) +
                ",\"request\":" + Quote(canonical.m_json) + "}");
        StoredPhaseIntent saved;
        if (!ReadPhaseIntent(bytes, *found, saved))
        {
            return { Error::Invalid };
        }
        if (!m_impl->Write(file, bytes))
        {
            return { Error::StorageFailed };
        }
        found->m_phaseIntents.push_back(AZStd::move(saved));
        return {};
#else
        return { Error::Unsupported };
#endif
    }
    Result FrameworkExecutionRepository::Commit(const StoredAttempt& a)
    {
        std::lock_guard lock(m_impl->m_mutex);
        auto found = AZStd::find_if(
            m_impl->m_attempts.begin(),
            m_impl->m_attempts.end(),
            [&](const auto& old)
            {
                return Key(old) == Key(a);
            });
        if (found == m_impl->m_attempts.end() || found->m_receipt || found->m_plan.m_fingerprint != a.m_plan.m_fingerprint ||
            !ValidResult(a))
        {
            return { Error::Invalid };
        }
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        if (!ToolIntents(m_impl->m_root, a))
        {
            return { Error::Invalid };
        }
        // M2 appended its own journal/record since phase admission. Include those bytes
        // in the shared metadata limit before publishing the Framework result.
        auto usage = StorageUsage(m_impl->m_root, 0, nullptr, &m_impl->m_bytes);
        if (!usage)
        {
            m_impl->m_healthy = false;
            return usage;
        }
        auto bytes = Finished(a);
        auto folder = "transactions/" + Key(a) + "/";
        if (!m_impl->Write(folder + "finished.json", bytes) || !m_impl->Write(folder + "committed", ET::ToolDigest(bytes)))
        {
            return { Error::StorageFailed };
        }
        auto intents = AZStd::move(found->m_phaseIntents);
        auto recovered = AZStd::move(found->m_recoveredTools);
        *found = a;
        found->m_phaseIntents = AZStd::move(intents);
        found->m_recoveredTools = AZStd::move(recovered);
        return {};
#else
        return { Error::Unsupported };
#endif
    }
    Result FrameworkExecutionRepository::Reconcile(const AZStd::vector<ET::ToolInvocationRecordV2>& records)
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (!m_impl->m_healthy)
        {
            return { Error::StorageFailed };
        }
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        for (auto& attempt : m_impl->m_attempts)
        {
            if (attempt.m_receipt && !attempt.m_quarantined)
            {
                continue;
            }
            for (const auto& intent : attempt.m_phaseIntents)
            {
                auto found = AZStd::find_if(
                    records.begin(),
                    records.end(),
                    [&](const auto& tool)
                    {
                        return tool.m_status.m_attemptId == intent.m_toolAttemptId;
                    });
                if (found == records.end())
                {
                    continue;
                } // Absence never establishes cleanup or permits replay.
                if (!MatchesIntent(intent, *found))
                {
                    m_impl->m_healthy = false;
                    return { Error::CorruptStore };
                }
                const auto json = ET::EncodeToolRecord(*found).m_json;
                auto previous = AZStd::find_if(
                    attempt.m_recoveredTools.begin(),
                    attempt.m_recoveredTools.end(),
                    [&](const auto& old)
                    {
                        return old.m_status.m_attemptId == intent.m_toolAttemptId;
                    });
                if (previous != attempt.m_recoveredTools.end())
                {
                    if (ET::EncodeToolRecord(*previous).m_json != json)
                    {
                        m_impl->m_healthy = false;
                        return { Error::CorruptStore };
                    }
                    continue;
                }
                if (!m_impl->Write(
                        "transactions/" + Key(attempt) + "/recovered." + intent.m_toolAttemptId + ".json",
                        Envelope("recovery-observation", Quote(json))))
                {
                    return { Error::StorageFailed };
                }
                attempt.m_recoveredTools.push_back(*found);
                // Actual M2 cleanup is retained, but interrupted Framework custody/ownership remains quarantined.
                attempt.m_quarantined = true;
            }
        }
        return {};
#else
        (void)records;
        return { Error::Unsupported };
#endif
    }
    AZStd::vector<StoredAttempt> FrameworkExecutionRepository::Read(size_t offset, size_t count) const
    {
        std::lock_guard lock(m_impl->m_mutex);
        AZStd::vector<StoredAttempt> result;
        count = AZStd::min(count, size_t{ 64 });
        for (size_t i = offset; i < m_impl->m_attempts.size() && result.size() < count; ++i)
        {
            result.push_back(m_impl->m_attempts[i]);
        }
        return result;
    }
    Result FrameworkExecutionRepository::CheckCapacity(AZ::u64 additional, const std::atomic_bool* cancelled) const
    {
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        AZ::u64 metadataSize = 0;
        auto result = StorageUsage(Root(), additional, cancelled, &metadataSize);
        if (result)
        {
            std::lock_guard lock(m_impl->m_mutex);
            m_impl->m_bytes = metadataSize;
        }
        return result;
#else
        return { Error::Unsupported };
#endif
    }
    bool FrameworkExecutionRepository::Healthy() const
    {
        std::lock_guard lock(m_impl->m_mutex);
        return m_impl->m_healthy;
    }
    AZStd::string FrameworkExecutionRepository::Root() const
    {
        std::lock_guard lock(m_impl->m_mutex);
        return m_impl->m_root;
    }
    AZ::u64 FrameworkExecutionRepository::Bytes() const
    {
        std::lock_guard lock(m_impl->m_mutex);
        return m_impl->m_bytes;
    }
    void FrameworkExecutionRepository::StopWrites()
    {
        std::lock_guard lock(m_impl->m_mutex);
        m_impl->m_healthy = false;
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
