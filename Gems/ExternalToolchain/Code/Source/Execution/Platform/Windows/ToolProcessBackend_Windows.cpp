/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "../../ToolExecutionRedactor.h"
#include "../../ToolProcessBackend.h"
#include "ToolSandbox_Windows.h"
#include <Sddl.h>
#include <UserEnv.h>
#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <thread>

namespace ExternalToolchain
{
    namespace
    {
        using namespace Windows;
        AZ::u64 UtcNow()
        {
            return static_cast<AZ::u64>(
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        }
        bool SameConfiguration(const ToolResolvedConfiguration& a, const ToolResolvedConfiguration& b)
        {
            if (a.m_providerId != b.m_providerId || a.m_providerVersion != b.m_providerVersion || a.m_probeId != b.m_probeId ||
                a.m_toolVersion != b.m_toolVersion || a.m_discoveryStatus != b.m_discoveryStatus ||
                a.m_executablePath != b.m_executablePath || a.m_executableDigest != b.m_executableDigest ||
                a.m_configurationFingerprint != b.m_configurationFingerprint || a.m_roots.size() != b.m_roots.size() ||
                a.m_companions.size() != b.m_companions.size())
            {
                return false;
            }
            for (size_t i = 0; i < a.m_roots.size(); ++i)
            {
                if (a.m_roots[i].m_rootId != b.m_roots[i].m_rootId || a.m_roots[i].m_absolutePath != b.m_roots[i].m_absolutePath)
                {
                    return false;
                }
            }
            for (size_t i = 0; i < a.m_companions.size(); ++i)
            {
                const auto& x = a.m_companions[i];
                const auto& y = b.m_companions[i];
                if (x.m_id != y.m_id || x.m_rootId != y.m_rootId || x.m_relativePath != y.m_relativePath || x.m_sha256 != y.m_sha256 ||
                    x.m_bytes != y.m_bytes)
                {
                    return false;
                }
            }
            return true;
        }
        bool MakeParents(const std::wstring& root, const AZStd::string& relative)
        {
            if (!ToolSafeRelativePath(relative))
            {
                return false;
            }
            auto path = Wide(relative);
            size_t pos = 0;
            while ((pos = path.find(L'/', pos)) != std::wstring::npos)
            {
                auto dir = root + L"\\" + path.substr(0, pos);
                if (GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES && !CreatePrivateDirectory(dir))
                {
                    return false;
                }
                PinnedPath pin;
                if (!pin.Open(Utf8(dir), true))
                {
                    return false;
                }
                ++pos;
            }
            return true;
        }
        bool GrantTree(const std::wstring& root, PSID sid, bool writable, unsigned depth = 0)
        {
            if (depth > 16 || !GrantPath(root, sid, writable, true))
            {
                return false;
            }
            WIN32_FIND_DATAW data{};
            HANDLE find = FindFirstFileW((root + L"\\*").c_str(), &data);
            if (find == INVALID_HANDLE_VALUE)
            {
                return GetLastError() == ERROR_FILE_NOT_FOUND;
            }
            bool ok = true;
            size_t count = 0;
            do
            {
                std::wstring name = data.cFileName;
                if (name == L"." || name == L"..")
                {
                    continue;
                }
                if (++count > 1024 || (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                {
                    ok = false;
                    break;
                }
                auto path = root + L"\\" + name;
                ok = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? GrantTree(path, sid, writable, depth + 1)
                                                                        : GrantPath(path, sid, writable, false);
                if (!ok)
                {
                    break;
                }
            } while (FindNextFileW(find, &data));
            FindClose(find);
            return ok;
        }
        struct AttributeList
        {
            std::vector<unsigned char> m_bytes;
            LPPROC_THREAD_ATTRIBUTE_LIST m_list = nullptr;
            ~AttributeList()
            {
                if (m_list)
                {
                    DeleteProcThreadAttributeList(m_list);
                }
            }
            bool Init()
            {
                SIZE_T size = 0;
                InitializeProcThreadAttributeList(nullptr, 3, 0, &size);
                if (!size || size > 65536)
                {
                    return false;
                }
                m_bytes.resize(size);
                auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(m_bytes.data());
                if (!InitializeProcThreadAttributeList(list, 3, 0, &size))
                {
                    return false;
                }
                m_list = list;
                return true;
            }
            bool Set(DWORD_PTR key, void* data, SIZE_T size)
            {
                return UpdateProcThreadAttribute(m_list, 0, key, data, size, nullptr, nullptr) != FALSE;
            }
        };
        bool Pipe(Handle& read, Handle& write)
        {
            SECURITY_ATTRIBUTES attributes{ sizeof(attributes), nullptr, TRUE };
            HANDLE a = nullptr, b = nullptr;
            if (!CreatePipe(&a, &b, &attributes, 65536))
            {
                return false;
            }
            read.Reset(a);
            write.Reset(b);
            return SetHandleInformation(read.Get(), HANDLE_FLAG_INHERIT, 0) != FALSE;
        }
        bool JobEmpty(HANDLE job, bool& empty)
        {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
            if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr))
            {
                return false;
            }
            empty = info.ActiveProcesses == 0;
            return true;
        }
        bool IsLpac(HANDLE process, PSID expected, const ToolRunCallbacks& callbacks)
        {
            HANDLE raw = nullptr;
            if (!OpenProcessToken(process, TOKEN_QUERY | TOKEN_DUPLICATE, &raw))
            {
                return false;
            }
            Handle token(raw);
            bool verified = VerifyLpacToken(token.Get(), expected);
            if (!verified && callbacks.m_log)
            {
                callbacks.m_log(true, "LPAC identity, capabilities, integrity or effective-access check failed.\n");
            }
            return verified;
        }
        bool Environment(
            const ToolInvocationRequestV2& r,
            const std::wstring& scratch,
            const std::wstring& profile,
            const std::wstring& manifest,
            std::vector<wchar_t>& block)
        {
            wchar_t windows[32768];
            UINT length = GetWindowsDirectoryW(windows, 32768);
            if (!length || length >= 32768)
            {
                return false;
            }
            std::map<std::wstring, std::wstring> values{ { L"SYSTEMROOT", windows },
                                                         { L"WINDIR", windows },
                                                         { L"TEMP", scratch },
                                                         { L"TMP", scratch },
                                                         { L"LOCALAPPDATA", profile },
                                                         { L"FOA_INVOCATION_ID", Wide(r.m_attemptId) },
                                                         { L"FOA_REQUEST_FINGERPRINT", Wide(r.m_fingerprint) },
                                                         { L"FOA_OUTPUT_MANIFEST", manifest } };
            for (const auto& v : r.m_environment)
            {
                auto key = Wide(v.m_name), value = Wide(v.m_value);
                if (key.empty() || (!v.m_value.empty() && value.empty()) || !values.emplace(key, value).second)
                {
                    return false;
                }
            }
            for (const auto& [key, value] : values)
            {
                auto line = key + L"=" + value;
                block.insert(block.end(), line.begin(), line.end());
                block.push_back(L'\0');
                if (block.size() + 1 > 16384)
                {
                    return false;
                }
            }
            block.push_back(L'\0');
            return true;
        }
        bool RemoveSubtree(const std::wstring& root)
        {
            if (GetFileAttributesW(root.c_str()) == INVALID_FILE_ATTRIBUTES)
            {
                return GetLastError() == ERROR_FILE_NOT_FOUND;
            }
            AZStd::string identity;
            {
                PinnedPath pin;
                if (!pin.Open(Utf8(root), true))
                {
                    return false;
                }
                identity = pin.m_identity;
            }
            return RemoveOwnedTree(root, identity);
        }
    } // namespace

    class WindowsToolProcess final : public ToolProcessBackend
    {
    public:
        ToolInvocationRecordV2 Run(
            const ToolInvocationRequestV2& request,
            const ToolExecutionCommandV2& command,
            const ToolExecutionHostConfiguration& configuration,
            const AZStd::string& host,
            ToolExecutionJournal& journal,
            std::atomic_bool& cancelled,
            const ToolRunCallbacks& callbacks) override
        {
            ToolInvocationRecordV2 record;
            record.m_status.m_attemptId = request.m_attemptId;
            record.m_requestFingerprint = request.m_fingerprint;
            record.m_commandFingerprint = request.m_commandFingerprint;
            record.m_profileFingerprint = ToolDigest(ToolExecutionProfile);
            record.m_startedUtcMilliseconds = UtcNow();
            record.m_status.m_stage = ToolStage::Preparing;
            record.m_status.m_sequence = 1;
            auto observe = [&](ToolStage stage)
            {
                record.m_status.m_stage = stage;
                ++record.m_status.m_sequence;
                if (callbacks.m_observe)
                {
                    callbacks.m_observe(record);
                }
            };
            auto save = [&]
            {
                ++record.m_status.m_sequence;
                record.m_status.m_persistence = ToolPersistence::Durable;
                auto result = journal.Save(record);
                if (!result)
                {
                    record.m_status.m_persistence = ToolPersistence::Failed;
                }
                return bool(result);
            };
            ToolRecoveryIntent intent{ request.m_attemptId, NewId(), {}, {}, false };
            intent.m_profileName = "foa.m2." + intent.m_stageName;
            std::wstring stage = Wide(journal.Root()) + L"\\staging\\" + Wide(intent.m_stageName);
            bool intentWritten = false, stageCreated = false, jobConfirmedStopped = true, keepOutputs = false;
            Sandbox sandbox;
            ToolLocks locks;
            std::map<AZStd::string, PinnedPath> roots;
            auto execute = [&]() -> ToolError
            {
                if (!ValidateToolRequest(request, command))
                {
                    return ToolError::InvalidContract;
                }
                if (cancelled)
                {
                    record.m_status.m_outcome = ToolOutcome::Cancelled;
                    return ToolError::None;
                }
                ToolResolvedConfiguration resolved;
                if (!configuration.m_resolve || !configuration.m_resolve(command, resolved) || !configuration.m_admission)
                {
                    return ToolError::AdmissionDenied;
                }
                if (resolved.m_providerId != command.m_providerId || resolved.m_providerVersion != command.m_providerVersion ||
                    resolved.m_probeId != command.m_probeId || resolved.m_discoveryStatus != DiscoveryStatus::Installed ||
                    resolved.m_toolVersion.empty() || !ToolSafeText(resolved.m_toolVersion, 64))
                {
                    return ToolError::ProviderMismatch;
                }
                if (!ToolValidDigest(resolved.m_executableDigest) || !ToolValidDigest(resolved.m_configurationFingerprint) ||
                    resolved.m_roots.empty() || resolved.m_roots.size() > 64 || resolved.m_companions.size() > ToolMaxFiles)
                {
                    return ToolError::InvalidContract;
                }
                PinnedPath executable;
                if (!executable.Open(resolved.m_executablePath, false, GENERIC_READ, FILE_SHARE_READ) ||
                    std::filesystem::path(executable.m_path).extension() != L".exe")
                {
                    return ToolError::UnsafePath;
                }
                AZ::u64 executableBytes = 0;
                AZStd::string executableHash;
                if (!HashFile(executable.m_path, executableHash, executableBytes) || executableHash != resolved.m_executableDigest)
                {
                    return ToolError::IdentityChanged;
                }
                record.m_executableDigest = executableHash;
                AZStd::vector<AZStd::string> markers{ resolved.m_executablePath, journal.Root() };
                ToolAdmissionContext context{ request.m_attemptId,
                                              request.m_fingerprint,
                                              request.m_commandFingerprint,
                                              record.m_profileFingerprint,
                                              host,
                                              executableHash,
                                              resolved.m_configurationFingerprint,
                                              {} };
                for (const auto& root : resolved.m_roots)
                {
                    if (!ToolSafeId(root.m_rootId) || roots.count(root.m_rootId))
                    {
                        return ToolError::InvalidContract;
                    }
                    PinnedPath pin;
                    if (!pin.Open(root.m_absolutePath, true))
                    {
                        return ToolError::UnsafePath;
                    }
                    context.m_rootIdentities.push_back(root.m_rootId + "/" + pin.m_identity);
                    markers.push_back(root.m_absolutePath);
                    roots.emplace(root.m_rootId, std::move(pin));
                }
                std::sort(context.m_rootIdentities.begin(), context.m_rootIdentities.end());
                auto target = roots.find(request.m_targetRootId);
                if (target == roots.end())
                {
                    return ToolError::UnsafePath;
                }
                if (!AcquireLocks(target->second, request.m_providerId, locks))
                {
                    return ToolError::Busy;
                }
                auto lease = configuration.m_admission->Acquire(context);
                if (!lease)
                {
                    return ToolError::AdmissionDenied;
                }
                if (!save() || !journal.WriteIntent(intent))
                {
                    return ToolError::JournalFailed;
                }
                intentWritten = true;
                if (!CreatePrivateDirectory(stage))
                {
                    return ToolError::UnsafePath;
                }
                stageCreated = true;
                {
                    PinnedPath pin;
                    if (!pin.Open(Utf8(stage), true))
                    {
                        return ToolError::UnsafePath;
                    }
                    intent.m_stageIdentity = pin.m_identity;
                }
                record.m_status.m_cleanup = ToolCleanup::Pending;
                if (!journal.WriteIntent(intent))
                {
                    return ToolError::JournalFailed;
                }
                const std::wstring tool = stage + L"\\tool", inputs = stage + L"\\inputs", out = stage + L"\\out",
                                   scratch = stage + L"\\scratch";
                for (const auto& path : { tool, inputs, out, scratch })
                {
                    if (!CreatePrivateDirectory(path))
                    {
                        return ToolError::UnsafePath;
                    }
                }
                auto program = tool + L"\\tool.exe";
                AZ::u64 aggregate = executableBytes;
                if (!CopyCheckedFile(executable.m_path, program, executableHash, executableBytes))
                {
                    return ToolError::InputMismatch;
                }
                auto copy = [&](const ToolFileReference& f, const std::wstring& directory) -> bool
                {
                    if (!ToolSafeId(f.m_id) || !ToolSafeRelativePath(f.m_relativePath) || !ToolValidDigest(f.m_sha256) ||
                        f.m_bytes > ToolMaxArtifactBytes - aggregate)
                    {
                        return false;
                    }
                    auto root = roots.find(f.m_rootId);
                    if (root == roots.end() || !MakeParents(directory, f.m_relativePath))
                    {
                        return false;
                    }
                    if (!CopyCheckedFile(
                            root->second.m_path + L"\\" + Wide(f.m_relativePath),
                            directory + L"\\" + Wide(f.m_relativePath),
                            f.m_sha256,
                            f.m_bytes))
                    {
                        return false;
                    }
                    aggregate += f.m_bytes;
                    return true;
                };
                for (const auto& companion : resolved.m_companions)
                {
                    if (!copy(companion, tool))
                    {
                        return ToolError::InputMismatch;
                    }
                }
                for (const auto& input : request.m_inputs)
                {
                    auto destination = inputs + L"\\" + Wide(input.m_id);
                    if (!CreatePrivateDirectory(destination) || !copy(input, destination))
                    {
                        return ToolError::InputMismatch;
                    }
                }
                for (const auto& output : request.m_outputs)
                {
                    if (output.m_relativePath == "manifest.v2.json" || !MakeParents(out, output.m_relativePath))
                    {
                        return ToolError::InvalidContract;
                    }
                }
                if (!sandbox.Create(intent.m_profileName))
                {
                    intent.m_profileCreated = sandbox.Created();
                    if (intent.m_profileCreated)
                    {
                        journal.WriteIntent(intent);
                    }
                    return ToolError::IsolationUnavailable;
                }
                intent.m_profileCreated = true;
                if (!journal.WriteIntent(intent))
                {
                    return ToolError::JournalFailed;
                }
                if (!GrantTree(tool, sandbox.Sid(), false) || !GrantTree(inputs, sandbox.Sid(), false) ||
                    !GrantTree(out, sandbox.Sid(), true) || !GrantTree(scratch, sandbox.Sid(), true))
                {
                    return ToolError::IsolationUnavailable;
                }
                markers.push_back(Utf8(stage));
                markers.push_back(Utf8(sandbox.ProfilePath()));
                ToolExecutionRedactor stdoutRedactor(markers), stderrRedactor(markers);
                std::vector<std::wstring> arguments;
                for (const auto& argument : request.m_arguments)
                {
                    std::wstring value;
                    switch (argument.m_kind)
                    {
                    case ToolArgumentKind::Literal:
                        value = Wide(argument.m_value);
                        if (!argument.m_value.empty() && value.empty())
                        {
                            return ToolError::InvalidContract;
                        }
                        break;
                    case ToolArgumentKind::Input:
                        for (const auto& f : request.m_inputs)
                        {
                            if (f.m_id == argument.m_value)
                            {
                                value = inputs + L"\\" + Wide(f.m_id) + L"\\" + Wide(f.m_relativePath);
                            }
                        }
                        break;
                    case ToolArgumentKind::Output:
                        for (const auto& f : request.m_outputs)
                        {
                            if (f.m_id == argument.m_value)
                            {
                                value = out + L"\\" + Wide(f.m_relativePath);
                            }
                        }
                        break;
                    case ToolArgumentKind::Scratch:
                        if (!MakeParents(scratch, argument.m_value))
                        {
                            return ToolError::UnsafePath;
                        }
                        value = scratch + L"\\" + Wide(argument.m_value);
                        break;
                    default:
                        return ToolError::SecretUseUnsupported;
                    }
                    arguments.push_back(std::move(value));
                }
                std::wstring commandLine;
                std::vector<wchar_t> environment;
                if (!BuildCommandLine(program, arguments, commandLine) ||
                    !Environment(request, scratch, sandbox.ProfilePath(), out + L"\\manifest.v2.json", environment))
                {
                    return ToolError::InvalidContract;
                }
                PinnedPath stagedExecutable;
                if (!stagedExecutable.Open(Utf8(program), false, GENERIC_READ, FILE_SHARE_READ))
                {
                    return ToolError::UnsafePath;
                }
                Handle stdoutRead, stdoutWrite, stderrRead, stderrWrite;
                if (!Pipe(stdoutRead, stdoutWrite) || !Pipe(stderrRead, stderrWrite))
                {
                    return ToolError::LaunchFailed;
                }
                Handle job(CreateJobObjectW(nullptr, nullptr));
                if (!job)
                {
                    return ToolError::ContainmentFailed;
                }
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
                limits.BasicLimitInformation.LimitFlags =
                    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_JOB_MEMORY;
                limits.BasicLimitInformation.ActiveProcessLimit = command.m_maxProcesses;
                limits.JobMemoryLimit = static_cast<SIZE_T>(command.m_memoryBytes);
                if (!SetInformationJobObject(job.Get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
                {
                    return ToolError::ContainmentFailed;
                }
                AttributeList attributes;
                if (!RegistryReadCapabilitySid())
                {
                    return ToolError::IsolationUnavailable;
                }
                SID_AND_ATTRIBUTES registryRead{ RegistryReadCapabilitySid(), SE_GROUP_ENABLED };
                SECURITY_CAPABILITIES security{ sandbox.Sid(), &registryRead, 1, 0 };
                DWORD policy = PROCESS_CREATION_ALL_APPLICATION_PACKAGES_OPT_OUT;
                HANDLE inherited[] = { stdoutWrite.Get(), stderrWrite.Get() };
                if (!attributes.Init() || !attributes.Set(PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES, &security, sizeof(security)) ||
                    !attributes.Set(PROC_THREAD_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY, &policy, sizeof(policy)) ||
                    !attributes.Set(PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited)))
                {
                    return ToolError::IsolationUnavailable;
                }
                STARTUPINFOEXW startup{};
                startup.StartupInfo.cb = sizeof(startup);
                startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
                startup.StartupInfo.wShowWindow = SW_HIDE;
                startup.StartupInfo.hStdInput = nullptr;
                startup.StartupInfo.hStdOutput = stdoutWrite.Get();
                startup.StartupInfo.hStdError = stderrWrite.Get();
                startup.lpAttributeList = attributes.m_list;
                PROCESS_INFORMATION process{};
                if (!CreateProcessW(
                        program.c_str(),
                        commandLine.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
                        environment.data(),
                        out.c_str(),
                        &startup.StartupInfo,
                        &process))
                {
                    return ToolError::LaunchFailed;
                }
                Handle processHandle(process.hProcess), threadHandle(process.hThread);
                record.m_processId = process.dwProcessId;
                FILETIME creation{}, exit{}, kernel{}, user{};
                if (!GetProcessTimes(processHandle.Get(), &creation, &exit, &kernel, &user))
                {
                    TerminateProcess(processHandle.Get(), 1);
                    jobConfirmedStopped = WaitForSingleObject(processHandle.Get(), 5000) == WAIT_OBJECT_0;
                    return ToolError::ContainmentFailed;
                }
                record.m_processCreationTime = (AZ::u64(creation.dwHighDateTime) << 32) | creation.dwLowDateTime;
                BOOL assigned = AssignProcessToJobObject(job.Get(), processHandle.Get());
                if (!assigned && callbacks.m_log)
                {
                    callbacks.m_log(true, AZStd::string::format("Job assignment failed: %lu\n", GetLastError()));
                }
                if (!assigned || !IsLpac(processHandle.Get(), sandbox.Sid(), callbacks))
                {
                    TerminateProcess(processHandle.Get(), 1);
                    jobConfirmedStopped = WaitForSingleObject(processHandle.Get(), 5000) == WAIT_OBJECT_0;
                    return ToolError::ContainmentFailed;
                }
                jobConfirmedStopped = false;
                ToolResolvedConfiguration current;
                if (!configuration.m_resolve(command, current) || !SameConfiguration(resolved, current) || cancelled ||
                    !lease->Consume(context, std::chrono::steady_clock::now()))
                {
                    TerminateJobObject(job.Get(), 1);
                    jobConfirmedStopped = WaitForSingleObject(processHandle.Get(), 5000) == WAIT_OBJECT_0;
                    return cancelled ? ToolError::None : ToolError::StaleAdmission;
                }
                lease.reset();
                record.m_status.m_outcome = ToolOutcome::Running;
                if (!save())
                {
                    TerminateJobObject(job.Get(), 1);
                    jobConfirmedStopped = WaitForSingleObject(processHandle.Get(), 5000) == WAIT_OBJECT_0;
                    return ToolError::JournalFailed;
                }
                if (ResumeThread(threadHandle.Get()) == static_cast<DWORD>(-1))
                {
                    TerminateJobObject(job.Get(), 1);
                    jobConfirmedStopped = WaitForSingleObject(processHandle.Get(), 5000) == WAIT_OBJECT_0;
                    return ToolError::LaunchFailed;
                }
                stdoutWrite.Reset();
                stderrWrite.Reset();
                observe(ToolStage::Running);
                auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(request.m_timeoutMilliseconds);
                auto cleanupDeadline = deadline + std::chrono::seconds(5), nextDiskCheck = std::chrono::steady_clock::now();
                AZ::u64 drained = 0;
                bool terminating = false, terminationIssued = false;
                auto forceStopAt = deadline;
                ToolError processError = ToolError::None;
                auto drain = [&](Handle& pipe, ToolExecutionRedactor& redactor, bool err) -> bool
                {
                    for (unsigned chunk = 0; chunk < 16; ++chunk)
                    {
                        DWORD available = 0;
                        if (!PeekNamedPipe(pipe.Get(), nullptr, 0, nullptr, &available, nullptr))
                        {
                            return GetLastError() == ERROR_BROKEN_PIPE;
                        }
                        if (!available)
                        {
                            return true;
                        }
                        char buffer[8192];
                        DWORD read = 0;
                        if (!ReadFile(pipe.Get(), buffer, std::min<DWORD>(available, sizeof(buffer)), &read, nullptr))
                        {
                            return GetLastError() == ERROR_BROKEN_PIPE;
                        }
                        drained += read;
                        if (drained > ToolMaxPipeBytes)
                        {
                            return false;
                        }
                        auto text = redactor.Push(buffer, read);
                        if (callbacks.m_log && !text.empty())
                        {
                            callbacks.m_log(err, text);
                        }
                    }
                    return true;
                };
                for (;;)
                {
                    auto now = std::chrono::steady_clock::now();
                    auto drainedBefore = drained;
                    if (!drain(stdoutRead, stdoutRedactor, false) || !drain(stderrRead, stderrRedactor, true))
                    {
                        processError = ToolError::OutputLimitExceeded;
                    }
                    if (!terminating && now >= nextDiskCheck)
                    {
                        AZStd::vector<AZStd::string> files;
                        AZ::u64 size = 0;
                        ULARGE_INTEGER free{};
                        if (!EnumerateTree(stage, files, size) || !GetDiskFreeSpaceExW(stage.c_str(), &free, nullptr, nullptr) ||
                            free.QuadPart < 16 * 1024 * 1024)
                        {
                            processError = ToolError::OutputLimitExceeded;
                        }
                        nextDiskCheck = now + std::chrono::milliseconds(500);
                    }
                    DWORD rootWait = WaitForSingleObject(processHandle.Get(), 0);
                    if (rootWait == WAIT_FAILED)
                    {
                        processError = ToolError::LaunchFailed;
                    }
                    if (!terminating && (cancelled || now >= deadline || processError != ToolError::None || rootWait == WAIT_OBJECT_0))
                    {
                        bool cancelWon = callbacks.m_beginDrain ? callbacks.m_beginDrain() : cancelled.load();
                        if (cancelWon)
                        {
                            record.m_status.m_outcome = ToolOutcome::Cancelled;
                        }
                        else if (now >= deadline)
                        {
                            record.m_status.m_outcome = ToolOutcome::TimedOut;
                        }
                        else if (processError != ToolError::None)
                        {
                            record.m_status.m_outcome = ToolOutcome::Failed;
                        }
                        else
                        {
                            DWORD code = 0;
                            if (!GetExitCodeProcess(processHandle.Get(), &code))
                            {
                                processError = ToolError::LaunchFailed;
                                record.m_status.m_outcome = ToolOutcome::Failed;
                            }
                            else
                            {
                                record.m_exitCode = code;
                                record.m_exitCodeObserved = true;
                                record.m_status.m_outcome = code ? ToolOutcome::ExitedNonzero : ToolOutcome::ExitedZero;
                            }
                        }
                        terminating = true;
                        cleanupDeadline = now + std::chrono::seconds(5);
                        observe(ToolStage::Draining);
                        // Windows console hosts can outlive the root briefly. Allow the whole
                        // job to drain naturally within the existing cleanup deadline. Errors,
                        // cancellation and timeout still request termination immediately.
                        forceStopAt =
                            record.m_exitCodeObserved && processError == ToolError::None ? now + std::chrono::milliseconds(100) : now;
                    }
                    bool empty = false;
                    if (!JobEmpty(job.Get(), empty))
                    {
                        return ToolError::CleanupFailed;
                    }
                    if (terminating && !empty && !terminationIssued && (now >= forceStopAt || processError != ToolError::None))
                    {
                        if (record.m_exitCodeObserved)
                        {
                            record.m_descendantsTerminated = true;
                            processError = ToolError::CleanupFailed;
                        }
                        if (!TerminateJobObject(job.Get(), 1))
                        {
                            return ToolError::CleanupFailed;
                        }
                        terminationIssued = true;
                    }
                    if (terminating && empty)
                    {
                        jobConfirmedStopped = true;
                        if (!drain(stdoutRead, stdoutRedactor, false) || !drain(stderrRead, stderrRedactor, true))
                        {
                            processError = ToolError::OutputLimitExceeded;
                        }
                        auto a = stdoutRedactor.Push("", 0, true), b = stderrRedactor.Push("", 0, true);
                        if (callbacks.m_log)
                        {
                            callbacks.m_log(false, a);
                            callbacks.m_log(true, b);
                        }
                        break;
                    }
                    if (terminating && now >= cleanupDeadline)
                    {
                        return ToolError::CleanupFailed;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(drained == drainedBefore ? 10 : 1));
                }
                if (processError != ToolError::None)
                {
                    return processError;
                }
                if (record.m_status.m_outcome != ToolOutcome::ExitedZero)
                {
                    return ToolError::None;
                }
                observe(ToolStage::Verifying);
                record.m_status.m_verification = ToolVerification::Failed;
                AZStd::string manifestBytes;
                ToolOutputManifestV2 manifest;
                if (!ReadFileBounded(out + L"\\manifest.v2.json", ToolMaxDocumentBytes, manifestBytes) ||
                    !DecodeToolManifest(manifestBytes, manifest) || manifest.m_attemptId != request.m_attemptId ||
                    manifest.m_requestFingerprint != request.m_fingerprint || manifest.m_outputs.size() != request.m_outputs.size())
                {
                    return ToolError::OutputInvalid;
                }
                AZStd::vector<AZStd::string> outputFiles;
                AZ::u64 outputBytes = 0;
                if (!EnumerateTree(out, outputFiles, outputBytes) || outputBytes > ToolMaxArtifactBytes - aggregate ||
                    outputFiles.size() != request.m_outputs.size() + 1)
                {
                    return ToolError::OutputInvalid;
                }
                std::set<AZStd::string> actualPaths(outputFiles.begin(), outputFiles.end());
                actualPaths.erase("manifest.v2.json");
                for (const auto& expected : request.m_outputs)
                {
                    auto found = std::find_if(
                        manifest.m_outputs.begin(),
                        manifest.m_outputs.end(),
                        [&](const auto& f)
                        {
                            return f.m_id == expected.m_id;
                        });
                    if (found == manifest.m_outputs.end() || found->m_rootId != "output" ||
                        found->m_relativePath != expected.m_relativePath || found->m_kind != expected.m_kind ||
                        found->m_bytes > expected.m_maxBytes || !actualPaths.erase(found->m_relativePath))
                    {
                        return ToolError::OutputInvalid;
                    }
                    AZStd::string digest;
                    AZ::u64 bytes = 0;
                    if (!HashFile(out + L"\\" + Wide(found->m_relativePath), digest, bytes, expected.m_maxBytes) ||
                        digest != found->m_sha256 || bytes != found->m_bytes)
                    {
                        return ToolError::OutputInvalid;
                    }
                    auto observed = *found;
                    observed.m_rootId = "staging." + intent.m_stageName;
                    observed.m_relativePath = "out/" + found->m_relativePath;
                    record.m_outputs.push_back(AZStd::move(observed));
                }
                if (!actualPaths.empty())
                {
                    return ToolError::OutputInvalid;
                }
                record.m_status.m_verification = ToolVerification::Passed;
                keepOutputs = true;
                return ToolError::None;
            };
            record.m_status.m_error = execute();
            if (record.m_status.m_outcome == ToolOutcome::Running)
            {
                record.m_status.m_outcome = cancelled ? ToolOutcome::Cancelled : ToolOutcome::Failed;
            }
            if (cancelled && record.m_status.m_outcome == ToolOutcome::NotAttempted)
            {
                record.m_status.m_outcome = ToolOutcome::Cancelled;
            }
            observe(ToolStage::Finalizing);
            bool cleaned = jobConfirmedStopped;
            if (cleaned && sandbox.Created())
            {
                cleaned = sandbox.Delete();
            }
            if (cleaned && stageCreated)
            {
                cleaned = keepOutputs
                    ? (RemoveSubtree(stage + L"\\tool") && RemoveSubtree(stage + L"\\inputs") && RemoveSubtree(stage + L"\\scratch"))
                    : RemoveOwnedTree(stage, intent.m_stageIdentity);
            }
            record.m_status.m_cleanup = cleaned ? (stageCreated ? ToolCleanup::Complete : ToolCleanup::NotRequired) : ToolCleanup::Failed;
            if (!cleaned && record.m_status.m_error == ToolError::None)
            {
                record.m_status.m_error = ToolError::CleanupFailed;
            }
            if (intentWritten && cleaned)
            {
                if (!save() || !journal.ClearIntent(request.m_attemptId))
                {
                    record.m_status.m_error = ToolError::JournalFailed;
                    record.m_status.m_persistence = ToolPersistence::Failed;
                }
            }
            return record;
        }

        ToolResult Recover(ToolExecutionJournal& journal) override
        {
            AZStd::vector<ToolInvocationRecordV2> records;
            auto loaded = journal.Load(records);
            if (!loaded)
            {
                return loaded;
            }
            AZStd::vector<ToolRecoveryIntent> intents;
            auto read = journal.ReadIntents(intents);
            if (!read)
            {
                return read;
            }
            std::set<AZStd::string> recoveredAttempts;
            for (const auto& intent : intents)
            {
                auto stage = Wide(journal.Root()) + L"\\staging\\" + Wide(intent.m_stageName);
                if (intent.m_profileCreated)
                {
                    if (FAILED(DeleteAppContainerProfile(Wide(intent.m_profileName).c_str())))
                    {
                        return { ToolError::CleanupFailed };
                    }
                }
                else
                {
                    PSID sid = nullptr;
                    if (FAILED(DeriveAppContainerSidFromAppContainerName(Wide(intent.m_profileName).c_str(), &sid)) || !sid)
                    {
                        return { ToolError::CleanupFailed };
                    }
                    LPWSTR text = nullptr;
                    PWSTR folder = nullptr;
                    bool absent = false;
                    if (ConvertSidToStringSidW(sid, &text))
                    {
                        HRESULT located = GetAppContainerFolderPath(text, &folder);
                        if (SUCCEEDED(located) && folder)
                        {
                            DWORD attributes = GetFileAttributesW(folder);
                            DWORD error = GetLastError();
                            absent =
                                attributes == INVALID_FILE_ATTRIBUTES && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND);
                            CoTaskMemFree(folder);
                        }
                        else
                        {
                            absent =
                                located == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || located == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
                        }
                        LocalFree(text);
                    }
                    FreeSid(sid);
                    // An API error is not proof that an unconfirmed profile is absent.
                    if (!absent)
                    {
                        return { ToolError::CleanupFailed };
                    }
                }
                if (GetFileAttributesW(stage.c_str()) != INVALID_FILE_ATTRIBUTES &&
                    (intent.m_stageIdentity.empty() || !RemoveOwnedTree(stage, intent.m_stageIdentity)))
                {
                    return { ToolError::CleanupFailed };
                }
                if (GetFileAttributesW(stage.c_str()) == INVALID_FILE_ATTRIBUTES && GetLastError() != ERROR_FILE_NOT_FOUND &&
                    GetLastError() != ERROR_PATH_NOT_FOUND)
                {
                    return { ToolError::CleanupFailed };
                }
                recoveredAttempts.insert(intent.m_attemptId);
                if (!journal.ClearIntent(intent.m_attemptId))
                {
                    return { ToolError::JournalFailed };
                }
            }
            for (auto& record : records)
            {
                if (record.m_status.m_stage == ToolStage::Terminal)
                {
                    continue;
                }
                if (record.m_status.m_cleanup == ToolCleanup::Pending && !recoveredAttempts.count(record.m_status.m_attemptId))
                {
                    return { ToolError::CleanupFailed };
                }
                record.m_status.m_stage = ToolStage::Terminal;
                record.m_status.m_outcome = ToolOutcome::Interrupted;
                record.m_status.m_verification = ToolVerification::NotRun;
                record.m_outputs.clear();
                record.m_status.m_cleanup = ToolCleanup::Complete;
                record.m_status.m_persistence = ToolPersistence::Durable;
                record.m_status.m_error = ToolError::HostStopped;
                record.m_finishedUtcMilliseconds = UtcNow();
                ++record.m_status.m_sequence;
                if (!journal.Save(record))
                {
                    return { ToolError::JournalFailed };
                }
            }
            return {};
        }
    };
    std::unique_ptr<ToolProcessBackend> MakeToolProcessBackend()
    {
        return std::make_unique<WindowsToolProcess>();
    }
} // namespace ExternalToolchain
