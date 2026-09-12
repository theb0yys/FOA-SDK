/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "Execution/Platform/Windows/ToolSandbox_Windows.h"
#include "Execution/ToolExecutionService.h"
#include <AzTest/AzTest.h>
#include <Psapi.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <winioctl.h>

namespace ExternalToolchain
{
    namespace
    {
        bool Same(const ToolAdmissionContext& a, const ToolAdmissionContext& b)
        {
            return a.m_attemptId == b.m_attemptId && a.m_requestFingerprint == b.m_requestFingerprint &&
                a.m_commandFingerprint == b.m_commandFingerprint && a.m_profileFingerprint == b.m_profileFingerprint &&
                a.m_hostInstanceId == b.m_hostInstanceId && a.m_executableDigest == b.m_executableDigest &&
                a.m_configurationFingerprint == b.m_configurationFingerprint && a.m_rootIdentities == b.m_rootIdentities;
        }
        class FixtureLease final : public ToolAdmissionLease
        {
        public:
            ToolAdmissionContext m_context;
            bool m_used = false, m_expired = false;
            std::chrono::steady_clock::time_point m_until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            bool Consume(const ToolAdmissionContext& c, std::chrono::steady_clock::time_point now) override
            {
                if (m_used || m_expired || now > m_until || !Same(c, m_context))
                {
                    return false;
                }
                m_used = true;
                return true;
            }
        };
        class FixtureAdmission final : public ToolExecutionAdmission
        {
        public:
            AZStd::string m_digest;
            AZStd::vector<AZStd::string> m_roots;
            bool m_deny = false, m_expire = false;
            std::unique_ptr<ToolAdmissionLease> Acquire(const ToolAdmissionContext& c) override
            {
                if (m_deny || c.m_executableDigest != m_digest || c.m_rootIdentities != m_roots)
                {
                    return {};
                }
                auto lease = std::make_unique<FixtureLease>();
                lease->m_context = c;
                lease->m_expired = m_expire;
                return lease;
            }
        };
    } // namespace
    class ToolExecutionOperational : public ::testing::Test
    {
    protected:
        std::wstring m_root;
        AZStd::string m_identity;
        ToolExecutionCommandV2 m_command;
        ToolExecutionHostConfiguration m_configuration;
        std::shared_ptr<FixtureAdmission> m_gate;
        std::unique_ptr<ToolExecutionService> m_service;
        void SetUp() override
        {
            wchar_t fixturePath[32768];
            DWORD fixtureLength = GetEnvironmentVariableW(L"FOA_M2_FIXTURE", fixturePath, 32768);
            ASSERT_GT(fixtureLength, 0u);
            ASSERT_LT(fixtureLength, 32768u);
            auto fixtureValue = Windows::Utf8(fixturePath);
            const char* fixture = fixtureValue.c_str();
            ASSERT_NE(fixture, nullptr) << "Native fixture path is required; this lane must not skip.";
            wchar_t temp[32768];
            ASSERT_GT(GetTempPathW(32768, temp), 0u);
            m_root = std::wstring(temp) + L"foa-m2-operation-" + Windows::Wide(Windows::NewId());
            wchar_t crashRoot[32768];
            DWORD crashLength = GetEnvironmentVariableW(L"FOA_M2_CRASH_ROOT", crashRoot, 32768);
            if (crashLength)
            {
                ASSERT_LT(crashLength, 32768u);
                m_root = crashRoot;
            }

            ASSERT_TRUE(Windows::CreatePrivateDirectory(m_root));
            {
                Windows::PinnedPath pin;
                ASSERT_TRUE(pin.Open(Windows::Utf8(m_root), true));
                m_identity = pin.m_identity;
            }
            ASSERT_TRUE(Windows::CreatePrivateDirectory(m_root + L"\\target"));
            ASSERT_TRUE(Windows::CreatePrivateDirectory(m_root + L"\\inputs"));
            ToolResolvedConfiguration resolved;
            resolved.m_providerId = "synthetic.fixture";
            resolved.m_providerVersion = "1.0.0";
            resolved.m_probeId = "native-fixture";
            resolved.m_toolVersion = "1.0.0";
            resolved.m_discoveryStatus = DiscoveryStatus::Installed;
            resolved.m_executablePath = fixture;
            AZ::u64 bytes = 0;
            ASSERT_TRUE(Windows::HashFile(Windows::Wide(fixture), resolved.m_executableDigest, bytes));
            resolved.m_configurationFingerprint = ToolDigest("synthetic-configuration");
            resolved.m_roots = { { "target", Windows::Utf8(m_root + L"\\target") }, { "input", Windows::Utf8(m_root + L"\\inputs") } };
            m_gate = std::make_shared<FixtureAdmission>();
            m_gate->m_digest = resolved.m_executableDigest;
            for (const auto& root : resolved.m_roots)
            {
                Windows::PinnedPath pin;
                ASSERT_TRUE(pin.Open(root.m_absolutePath, true));
                m_gate->m_roots.push_back(root.m_rootId + "/" + pin.m_identity);
            }
            std::sort(m_gate->m_roots.begin(), m_gate->m_roots.end());
            m_configuration.m_enabled = true;
            m_configuration.m_privateStoreRoot = Windows::Utf8(m_root + L"\\store");
            m_configuration.m_admission = m_gate;
            m_configuration.m_resolve = [resolved](const auto&, auto& output)
            {
                output = resolved;
                return true;
            };
            m_command.m_providerId = "synthetic.fixture";
            m_command.m_providerVersion = "1.0.0";
            m_command.m_commandId = "run";
            m_command.m_probeId = "native-fixture";
            m_command.m_inputKinds = { "text/plain" };
            m_command.m_outputKinds = { "text/plain" };
            m_command.m_environmentNames = { "FIXTURE_VALUE", "FIXTURE_HANDLE" };
            StartService();
        }
        void StartService()
        {
            m_service.reset();
            m_service = std::make_unique<ToolExecutionService>(m_configuration);
            ExternalToolProviderDescriptor provider;
            provider.m_providerId = m_command.m_providerId;
            provider.m_providerVersion = m_command.m_providerVersion;
            ExternalToolCommandDescriptor command;
            command.m_commandId = m_command.m_commandId;
            command.m_mode = CommandMode::Batch;
            provider.m_commands.push_back(command);
            ExternalToolDiscoveryProbeDescriptor probe;
            probe.m_probeId = m_command.m_probeId;
            provider.m_discoveryProbes.push_back(probe);
            m_service->ObserveProvider(provider);
            ASSERT_TRUE(m_service->RegisterExecutionCommand(m_command));
            m_service->FinalizeRegistration();
        }
        void TearDown() override
        {
            m_service.reset();
            if (!m_root.empty() && !m_identity.empty())
            {
                EXPECT_TRUE(Windows::RemoveOwnedTree(m_root, m_identity));
            }
        }
        ToolInvocationRequestV2 Request(const char* mode = "success")
        {
            ToolInvocationRequestV2 r;
            r.m_attemptId = "inv." + Windows::NewId();
            r.m_providerId = m_command.m_providerId;
            r.m_commandId = m_command.m_commandId;
            r.m_targetRootId = "target";
            r.m_commandFingerprint = CanonicalToolCommand(m_command).m_fingerprint;
            r.m_arguments = { { ToolArgumentKind::Literal, mode } };
            r.m_outputs = { { "result", "result.txt", "text/plain", 1024 * 1024 } };
            r.m_fingerprint = CanonicalToolRequest(r).m_fingerprint;
            return r;
        }
        bool Submit(ToolInvocationRequestV2& r)
        {
            r.m_fingerprint = CanonicalToolRequest(r).m_fingerprint;
            for (unsigned attempt = 0; attempt < 200; ++attempt)
            {
                auto result = m_service->Submit(r);
                if (result)
                {
                    return true;
                }
                if (result.m_error != ToolError::Busy)
                {
                    ADD_FAILURE() << ToolErrorName(result.m_error);
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            ADD_FAILURE() << "Service did not initialise";
            return false;
        }
        ToolInvocationRecordV2 Wait(const ToolInvocationRequestV2& r)
        {
            for (unsigned attempt = 0; attempt < 1200; ++attempt)
            {
                ToolInvocationStatusV2 status;
                if (m_service->GetStatus(r.m_attemptId, status) && status.m_stage == ToolStage::Terminal)
                {
                    auto page = m_service->EnumerateRecords(0, 64);
                    for (const auto& record : page.m_records)
                    {
                        if (record.m_status.m_attemptId == r.m_attemptId)
                        {
                            return record;
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            ADD_FAILURE() << "Invocation failed to terminate within 12 seconds";
            return {};
        }
        AZStd::string Artifact(const ToolInvocationRecordV2& r)
        {
            if (r.m_outputs.size() != 1)
            {
                ADD_FAILURE() << "Expected one verified output";
                return {};
            }
            const auto& file = r.m_outputs.front();
            auto path =
                m_root + L"\\store\\staging\\" + Windows::Wide(file.m_rootId.substr(8)) + L"\\" + Windows::Wide(file.m_relativePath);
            AZStd::string value;
            if (!Windows::ReadFileBounded(path, 1024 * 1024, value))
            {
                ADD_FAILURE() << "Output was not retained";
            }
            return value;
        }
    };
    TEST_F(ToolExecutionOperational, SuccessRequiresLpacVerifiedArtifactAndDurableRecord)
    {
        auto request = Request();
        ASSERT_TRUE(Submit(request));
        auto record = Wait(request);
        EXPECT_TRUE(ToolSucceeded(record)) << ToolErrorName(record.m_status.m_error) << ":"
                                           << m_service->ReadLog(request.m_attemptId, true, 0).m_text.c_str();
        EXPECT_EQ(record.m_profileFingerprint, ToolDigest(ToolExecutionProfile));
        EXPECT_EQ(record.m_commandFingerprint, request.m_commandFingerprint);
        if (ToolSucceeded(record))
        {
            EXPECT_EQ(Artifact(record), "hello\n");
        }
    }
    TEST_F(ToolExecutionOperational, RapidRootExitDrainsConsoleHostWithoutFalseDescendantFailure)
    {
        const auto start = std::chrono::steady_clock::now();
        for (unsigned invocation = 0; invocation < 16; ++invocation)
        {
            auto request = Request();
            ASSERT_TRUE(Submit(request));
            const auto record = Wait(request);
            ASSERT_TRUE(ToolSucceeded(record)) << invocation << ":" << ToolErrorName(record.m_status.m_error);
            EXPECT_FALSE(record.m_descendantsTerminated);
            EXPECT_EQ(Artifact(record), "hello\n");
        }
        EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(10));
    }
    TEST_F(ToolExecutionOperational, ExplicitDenialAndExpiredLeaseNeverRunTheFixture)
    {
        m_gate->m_deny = true;
        auto denied = Request();
        ASSERT_TRUE(Submit(denied));
        auto first = Wait(denied);
        EXPECT_EQ(first.m_status.m_error, ToolError::AdmissionDenied);
        EXPECT_EQ(first.m_processId, 0);
        m_gate->m_deny = false;
        m_gate->m_expire = true;
        auto stale = Request();
        ASSERT_TRUE(Submit(stale));
        auto second = Wait(stale);
        EXPECT_EQ(second.m_status.m_error, ToolError::StaleAdmission);
        EXPECT_FALSE(second.m_exitCodeObserved);
        EXPECT_FALSE(ToolSucceeded(second));
    }
    TEST_F(ToolExecutionOperational, TimeoutAndCancellationCleanUpTheProcess)
    {
        auto timeout = Request("hang");
        timeout.m_timeoutMilliseconds = 100;
        ASSERT_TRUE(Submit(timeout));
        auto first = Wait(timeout);
        EXPECT_EQ(first.m_status.m_outcome, ToolOutcome::TimedOut);
        EXPECT_EQ(first.m_status.m_cleanup, ToolCleanup::Complete);
        auto cancel = Request("hang");
        ASSERT_TRUE(Submit(cancel));
        for (unsigned i = 0; i < 300; ++i)
        {
            ToolInvocationStatusV2 status;
            m_service->GetStatus(cancel.m_attemptId, status);
            if (status.m_stage == ToolStage::Running)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        ASSERT_TRUE(m_service->Cancel(cancel.m_attemptId));
        auto second = Wait(cancel);
        EXPECT_EQ(second.m_status.m_outcome, ToolOutcome::Cancelled);
        EXPECT_EQ(second.m_status.m_cleanup, ToolCleanup::Complete);
    }
    TEST_F(ToolExecutionOperational, NonzeroCrashAndInvalidOutputsCannotBecomeSuccess)
    {
        for (const char* mode : { "exit", "crash", "malformed", "wrong-hash", "extra" })
        {
            auto request = Request(mode);
            ASSERT_TRUE(Submit(request));
            auto record = Wait(request);
            EXPECT_FALSE(ToolSucceeded(record)) << mode;
            EXPECT_EQ(record.m_status.m_cleanup, ToolCleanup::Complete) << mode << ":" << ToolErrorName(record.m_status.m_error);
            if (AZStd::string(mode) == "exit")
            {
                EXPECT_EQ(record.m_exitCode, 17u);
            }
            else if (AZStd::string(mode) != "crash")
            {
                EXPECT_EQ(record.m_status.m_error, ToolError::OutputInvalid) << mode;
            }
        }
    }
    TEST_F(ToolExecutionOperational, NetworkAndEnvironmentAreIsolated)
    {
        SetEnvironmentVariableW(L"FOA_PARENT_SECRET", L"not-for-child");
        auto environment = Request("environment");
        environment.m_environment = { { "FIXTURE_VALUE", "expected" } };
        ASSERT_TRUE(Submit(environment));
        auto first = Wait(environment);
        EXPECT_TRUE(ToolSucceeded(first)) << ToolErrorName(first.m_status.m_error);
        if (ToolSucceeded(first))
        {
            EXPECT_EQ(Artifact(first), "environment-isolated\n");
        }
        auto log = m_service->ReadLog(environment.m_attemptId, false, 0);
        EXPECT_EQ(log.m_text.find(Windows::Utf8(m_root)), AZStd::string::npos);
        SetEnvironmentVariableW(L"FOA_PARENT_SECRET", nullptr);
        auto network = Request("network");
        ASSERT_TRUE(Submit(network));
        auto second = Wait(network);
        EXPECT_TRUE(ToolSucceeded(second)) << ToolErrorName(second.m_status.m_error) << ":" << second.m_exitCode << ":"
                                           << m_service->ReadLog(network.m_attemptId, true, 0).m_text.c_str();
        if (ToolSucceeded(second))
        {
            EXPECT_EQ(Artifact(second), "network-denied\n");
        }
    }
    TEST_F(ToolExecutionOperational, SurvivingDescendantAndFloodAreContained)
    {
        auto child = Request("child");
        ASSERT_TRUE(Submit(child));
        auto first = Wait(child);
        EXPECT_TRUE(first.m_exitCodeObserved);
        EXPECT_EQ(first.m_exitCode, 0u) << m_service->ReadLog(child.m_attemptId, true, 0).m_text.c_str();
        const auto childLog = m_service->ReadLog(child.m_attemptId, false, 0).m_text;
        EXPECT_EQ(childLog.find("child="), 0u) << "The fixture must actually create its descendant.";
        EXPECT_TRUE(first.m_descendantsTerminated)
            << first.m_exitCode << ":" << m_service->ReadLog(child.m_attemptId, true, 0).m_text.c_str();
        EXPECT_EQ(first.m_status.m_cleanup, ToolCleanup::Complete);
        EXPECT_FALSE(ToolSucceeded(first));
        auto flood = Request("flood");
        ASSERT_TRUE(Submit(flood));
        auto second = Wait(flood);
        EXPECT_EQ(second.m_status.m_error, ToolError::OutputLimitExceeded);
        EXPECT_EQ(second.m_status.m_cleanup, ToolCleanup::Complete);
        EXPECT_LE(m_service->ReadLog(flood.m_attemptId, false, 0).m_text.size(), ToolMaxLogPage);
    }

    TEST_F(ToolExecutionOperational, ArgumentsPreserveEmptyQuotesUnicodeAndTrailingBackslashes)
    {
        auto request = Request("argv");
        AZStd::vector<AZStd::string> values = { "", "two words", "say \"hello\"", "tail\\", "double\\\\", "\xe9\x9b\xaa" };
        AZStd::string expected;
        const char hex[] = "0123456789abcdef";
        for (size_t i = 0; i < values.size(); ++i)
        {
            request.m_arguments.push_back({ ToolArgumentKind::Literal, values[i] });
            expected += AZStd::string::format("%zu:", i);
            for (unsigned char c : values[i])
            {
                expected.push_back(hex[c >> 4]);
                expected.push_back(hex[c & 15]);
            }
            expected += '\n';
        }
        ASSERT_TRUE(Submit(request));
        auto record = Wait(request);
        ASSERT_TRUE(ToolSucceeded(record)) << ToolErrorName(record.m_status.m_error) << ":" << record.m_exitCode;
        EXPECT_EQ(Artifact(record), expected);
    }
    TEST_F(ToolExecutionOperational, OutsideInputAndJournalWritesAreDenied)
    {
        auto outside = m_root + L"\\sentinel.txt";
        ASSERT_TRUE(Windows::WriteFileAtomic(outside, "unchanged"));
        auto source = m_root + L"\\inputs\\paths.txt";
        auto probe = Windows::Utf8(outside) + "\n" + Windows::Utf8(m_root + L"\\store\\records\\escape.txt") + "\n";
        ASSERT_TRUE(Windows::WriteFileAtomic(source, probe));
        auto request = Request("write-probe");
        request.m_inputs = { { "probe", "input", "paths.txt", "text/plain", ToolDigest(probe), probe.size() } };
        request.m_arguments.push_back({ ToolArgumentKind::Input, "probe" });
        ASSERT_TRUE(Submit(request));
        auto record = Wait(request);
        ASSERT_TRUE(ToolSucceeded(record)) << ToolErrorName(record.m_status.m_error) << ":" << record.m_exitCode;
        EXPECT_EQ(Artifact(record), "writes-denied\n");
        AZStd::string value;
        ASSERT_TRUE(Windows::ReadFileBounded(outside, 64, value));
        EXPECT_EQ(value, "unchanged");
        ASSERT_TRUE(Windows::ReadFileBounded(source, 8192, value));
        EXPECT_EQ(value, probe);
        EXPECT_EQ(GetFileAttributesW((m_root + L"\\store\\records\\escape.txt").c_str()), INVALID_FILE_ATTRIBUTES);
    }
    TEST_F(ToolExecutionOperational, UnlistedInheritableHandleIsAbsent)
    {
        auto path = m_root + L"\\canary.txt";
        ASSERT_TRUE(Windows::WriteFileAtomic(path, "private-canary"));
        SECURITY_ATTRIBUTES attributes{ sizeof(attributes), nullptr, TRUE };
        Windows::Handle canary(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, &attributes, OPEN_EXISTING, 0, nullptr));
        ASSERT_TRUE(canary);
        auto request = Request("handle");
        request.m_environment = { { "FIXTURE_HANDLE", AZStd::string::format("%llu", reinterpret_cast<AZ::u64>(canary.Get())) } };
        ASSERT_TRUE(Submit(request));
        auto record = Wait(request);
        ASSERT_TRUE(ToolSucceeded(record)) << ToolErrorName(record.m_status.m_error) << ":" << record.m_exitCode;
        EXPECT_EQ(Artifact(record), "handle-isolated\n");
    }

    TEST_F(ToolExecutionOperational, EffectiveIsolationRejectsOrdinaryAppContainerAndWrongIdentity)
    {
        wchar_t executable[32768];
        DWORD length = GetEnvironmentVariableW(L"FOA_M2_FIXTURE", executable, 32768);
        ASSERT_GT(length, 0u);
        ASSERT_LT(length, 32768u);
        // Validate the reviewed constant against the OS derivation by capability name.
        using Derive = BOOL(WINAPI*)(LPCWSTR, PSID**, DWORD*, PSID**, DWORD*);
        auto module = GetModuleHandleW(L"kernelbase.dll");
        ASSERT_NE(module, nullptr);
        auto derive = reinterpret_cast<Derive>(GetProcAddress(module, "DeriveCapabilitySidsFromName"));
        ASSERT_NE(derive, nullptr);
        struct DerivedCapabilities
        {
            PSID* groups = nullptr;
            PSID* capabilities = nullptr;
            DWORD groupCount = 0, capabilityCount = 0;
            ~DerivedCapabilities()
            {
                for (DWORD i = 0; i < groupCount; ++i)
                {
                    LocalFree(groups[i]);
                }
                for (DWORD i = 0; i < capabilityCount; ++i)
                {
                    LocalFree(capabilities[i]);
                }
                LocalFree(groups);
                LocalFree(capabilities);
            }
        } derived;
        ASSERT_TRUE(derive(L"registryRead", &derived.groups, &derived.groupCount, &derived.capabilities, &derived.capabilityCount));
        ASSERT_EQ(derived.capabilityCount, 1u);
        ASSERT_NE(Windows::RegistryReadCapabilitySid(), nullptr);
        ASSERT_TRUE(EqualSid(derived.capabilities[0], Windows::RegistryReadCapabilitySid()));
        BYTE unknown[SECURITY_MAX_SID_SIZE]{};
        ASSERT_TRUE(CopySid(sizeof(unknown), unknown, Windows::RegistryReadCapabilitySid()));
        *GetSidSubAuthority(unknown, *GetSidSubAuthorityCount(unknown) - 1) ^= 1;
        // Controls remain suspended: no provider code runs with a rejected token.
        for (unsigned control = 0; control < 8; ++control)
        {
            bool lpac = (control & 1) != 0;
            unsigned capabilitySet = control / 2;
            Windows::Sandbox sandbox;
            ASSERT_TRUE(sandbox.Create("foa.m2." + Windows::NewId()));
            SIZE_T bytes = 0;
            DWORD count = lpac ? 2 : 1;
            InitializeProcThreadAttributeList(nullptr, count, 0, &bytes);
            ASSERT_GT(bytes, 0u);
            std::vector<unsigned char> storage(bytes);
            auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
            ASSERT_TRUE(InitializeProcThreadAttributeList(attributes, count, 0, &bytes));
            struct AttributeCleanup
            {
                LPPROC_THREAD_ATTRIBUTE_LIST value;
                ~AttributeCleanup()
                {
                    DeleteProcThreadAttributeList(value);
                }
            } cleanup{ attributes };
            SID_AND_ATTRIBUTES capabilityList[]{ { Windows::RegistryReadCapabilitySid(), SE_GROUP_ENABLED },
                                                 { unknown, SE_GROUP_ENABLED } };
            SECURITY_CAPABILITIES security{ sandbox.Sid(),
                                            capabilitySet == 0 ? nullptr : (capabilitySet == 2 ? capabilityList + 1 : capabilityList),
                                            capabilitySet == 0 ? 0UL : (capabilitySet == 3 ? 2UL : 1UL),
                                            0 };
            ASSERT_TRUE(UpdateProcThreadAttribute(
                attributes, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES, &security, sizeof(security), nullptr, nullptr));
            DWORD policy = PROCESS_CREATION_ALL_APPLICATION_PACKAGES_OPT_OUT;
            if (lpac)
            {
                ASSERT_TRUE(UpdateProcThreadAttribute(
                    attributes, 0, PROC_THREAD_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY, &policy, sizeof(policy), nullptr, nullptr));
            }
            STARTUPINFOEXW startup{};
            startup.StartupInfo.cb = sizeof(startup);
            startup.lpAttributeList = attributes;
            PROCESS_INFORMATION process{};
            auto line = Windows::QuoteArgument(executable) + L" success";
            ASSERT_TRUE(CreateProcessW(
                executable,
                line.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
                nullptr,
                nullptr,
                &startup.StartupInfo,
                &process));
            struct NeverResume
            {
                HANDLE process, thread;
                ~NeverResume()
                {
                    TerminateProcess(process, 1);
                    WaitForSingleObject(process, 5000);
                    CloseHandle(thread);
                    CloseHandle(process);
                }
            } suspended{ process.hProcess, process.hThread };
            HANDLE raw = nullptr;
            ASSERT_TRUE(OpenProcessToken(process.hProcess, TOKEN_QUERY | TOKEN_DUPLICATE, &raw));
            Windows::Handle token(raw);
            EXPECT_EQ(Windows::VerifyLpacToken(token.Get(), sandbox.Sid()), lpac && capabilitySet == 1)
                << "lpac=" << lpac << " capabilitySet=" << capabilitySet;
            BYTE world[SECURITY_MAX_SID_SIZE];
            DWORD size = sizeof(world);
            ASSERT_TRUE(CreateWellKnownSid(WinWorldSid, nullptr, world, &size));
            EXPECT_FALSE(Windows::VerifyLpacToken(token.Get(), world));
            ASSERT_TRUE(TerminateProcess(process.hProcess, 1));
            ASSERT_EQ(WaitForSingleObject(process.hProcess, 5000), WAIT_OBJECT_0);
            token.Reset();
            CloseHandle(suspended.thread);
            suspended.thread = nullptr;
            CloseHandle(suspended.process);
            suspended.process = nullptr;
            EXPECT_TRUE(sandbox.Delete());
        }
    }

    class CrashHarness : public ToolExecutionOperational
    {
    public:
        void TestBody() override
        {
        }
        int Run()
        {
            SetUp();
            if (HasFatalFailure())
            {
                return 91;
            }
            auto request = Request("heartbeat-hang");
            if (!Submit(request))
            {
                return 92;
            }
            for (;;)
            {
                Sleep(100);
            }
        }
    };
    extern "C" __declspec(dllexport) int RunM2HostCrashFixture(int, char**)
    {
        // Separate AzTestRunner entry point; no production target links this admission gate.
        wchar_t root[32768];
        if (!GetEnvironmentVariableW(L"FOA_M2_CRASH_ROOT", root, 32768))
        {
            return 90;
        }
        CrashHarness harness;
        return harness.Run();
    }
    TEST_F(ToolExecutionOperational, HostCrashKillsJobAndRecoveryNeverReplays)
    {
        auto childRoot = m_root + L"\\crash-host";
        wchar_t runner[32768];
        ASSERT_GT(GetModuleFileNameW(nullptr, runner, 32768), 0u);
        HMODULE module = nullptr;
        ASSERT_TRUE(GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&RunM2HostCrashFixture),
            &module));
        wchar_t dll[32768];
        ASSERT_GT(GetModuleFileNameW(module, dll, 32768), 0u);
        auto line = Windows::QuoteArgument(runner) + L" " + Windows::QuoteArgument(dll) + L" RunM2HostCrashFixture";
        ASSERT_TRUE(SetEnvironmentVariableW(L"FOA_M2_CRASH_ROOT", childRoot.c_str()));
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        BOOL created = CreateProcessW(runner, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
        SetEnvironmentVariableW(L"FOA_M2_CRASH_ROOT", nullptr);
        ASSERT_TRUE(created);
        struct ChildGuard
        {
            Windows::Handle process, thread;
            ~ChildGuard()
            {
                if (process)
                {
                    TerminateProcess(process.Get(), 93);
                    WaitForSingleObject(process.Get(), 5000);
                }
            }
        } child{ Windows::Handle(process.hProcess), Windows::Handle(process.hThread) };
        auto store = childRoot + L"\\store";
        ToolInvocationRecordV2 observed;
        bool live = false;
        for (unsigned attempt = 0; attempt < 1000 && !live; ++attempt)
        {
            WIN32_FIND_DATAW data{};
            HANDLE find = FindFirstFileW((store + L"\\records\\*.record.*").c_str(), &data);
            if (find != INVALID_HANDLE_VALUE)
            {
                do
                {
                    AZStd::string frame;
                    ToolInvocationRecordV2 candidate;
                    if (Windows::ReadFileBounded(store + L"\\records\\" + data.cFileName, ToolMaxDocumentBytes + 72, frame) &&
                        frame.size() > 72 && ToolDigest(frame.substr(72)) == frame.substr(0, 71) &&
                        DecodeToolRecord(frame.substr(72), candidate) && candidate.m_processId)
                    {
                        observed = candidate;
                    }
                } while (FindNextFileW(find, &data));
                FindClose(find);
            }
            find = FindFirstFileW((store + L"\\staging\\*").c_str(), &data);
            if (find != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (data.cFileName[0] != L'.' && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        live |= GetFileAttributesW((store + L"\\staging\\" + data.cFileName + L"\\out\\live.txt").c_str()) !=
                            INVALID_FILE_ATTRIBUTES;
                    }
                } while (FindNextFileW(find, &data));
                FindClose(find);
            }
            if (!live)
            {
                Sleep(10);
            }
        }
        ASSERT_TRUE(live);
        ASSERT_NE(observed.m_processId, 0);
        Windows::Handle fixture(
            OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(observed.m_processId)));
        ASSERT_TRUE(fixture);
        FILETIME createdAt{}, ended{}, kernel{}, user{};
        ASSERT_TRUE(GetProcessTimes(fixture.Get(), &createdAt, &ended, &kernel, &user));
        ASSERT_EQ((AZ::u64(createdAt.dwHighDateTime) << 32) | createdAt.dwLowDateTime, observed.m_processCreationTime);
        ASSERT_TRUE(TerminateProcess(child.process.Get(), 94));
        ASSERT_EQ(WaitForSingleObject(child.process.Get(), 5000), WAIT_OBJECT_0);
        ASSERT_EQ(WaitForSingleObject(fixture.Get(), 5000), WAIT_OBJECT_0);
        ToolExecutionHostConfiguration configuration;
        configuration.m_enabled = true;
        configuration.m_privateStoreRoot = Windows::Utf8(store);
        ToolExecutionService recovery(configuration);
        ToolInvocationStatusV2 status;
        bool recovered = false;
        for (unsigned i = 0; i < 1000; ++i)
        {
            if (recovery.GetStatus(observed.m_status.m_attemptId, status) && status.m_stage == ToolStage::Terminal)
            {
                recovered = true;
                break;
            }
            Sleep(10);
        }
        ASSERT_TRUE(recovered);
        EXPECT_EQ(status.m_outcome, ToolOutcome::Interrupted);
        EXPECT_EQ(status.m_cleanup, ToolCleanup::Complete);
        EXPECT_EQ(status.m_persistence, ToolPersistence::Durable);
        EXPECT_EQ(status.m_error, ToolError::HostStopped);
        auto records = recovery.EnumerateRecords(0, 64);
        ASSERT_EQ(records.m_records.size(), 1);
        EXPECT_TRUE(records.m_records[0].m_outputs.empty());
        recovery.Shutdown();
        EXPECT_EQ(
            GetFileAttributesW((store + L"\\records\\" + Windows::Wide(observed.m_status.m_attemptId) + L".intent").c_str()),
            INVALID_FILE_ATTRIBUTES);
    }

    TEST_F(ToolExecutionOperational, QueueLimitsAndBothCancellationOrderingsAreObserved)
    {
        auto running = Request("hang");
        ASSERT_TRUE(Submit(running));
        bool started = false;
        for (unsigned i = 0; i < 500; ++i)
        {
            ToolInvocationStatusV2 status;
            m_service->GetStatus(running.m_attemptId, status);
            if (status.m_stage == ToolStage::Running)
            {
                started = true;
                break;
            }
            Sleep(10);
        }
        ASSERT_TRUE(started);
        AZStd::vector<ToolInvocationRequestV2> queued;
        for (unsigned i = 0; i < 16; ++i)
        {
            queued.push_back(Request());
            ASSERT_TRUE(Submit(queued.back()));
        }
        auto excess = Request();
        EXPECT_EQ(m_service->Submit(excess).m_error, ToolError::QueueFull);
        EXPECT_EQ(m_service->Submit(running).m_error, ToolError::Duplicate);
        for (auto& request : queued)
        {
            ASSERT_TRUE(m_service->Cancel(request.m_attemptId));
        }
        for (auto& request : queued)
        {
            auto record = Wait(request);
            EXPECT_EQ(record.m_status.m_outcome, ToolOutcome::Cancelled);
            EXPECT_EQ(record.m_processId, 0);
            EXPECT_EQ(record.m_status.m_persistence, ToolPersistence::Durable);
        }
        ASSERT_TRUE(m_service->Cancel(running.m_attemptId));
        auto stopped = Wait(running);
        EXPECT_EQ(stopped.m_status.m_outcome, ToolOutcome::Cancelled);
        auto completed = Request();
        ASSERT_TRUE(Submit(completed));
        auto before = Wait(completed);
        ASSERT_TRUE(ToolSucceeded(before));
        ASSERT_TRUE(m_service->Cancel(completed.m_attemptId));
        auto after = Wait(completed);
        EXPECT_EQ(after.m_status.m_outcome, before.m_status.m_outcome);
        EXPECT_EQ(after.m_status.m_sequence, before.m_status.m_sequence);
    }
    TEST_F(ToolExecutionOperational, AdmissionRejectsChangedDiscoveryAndExecutableDigest)
    {
        auto resolve = m_configuration.m_resolve;
        m_configuration.m_resolve = [resolve](const auto& command, auto& resolved)
        {
            resolve(command, resolved);
            resolved.m_discoveryStatus = DiscoveryStatus::NotInstalled;
            return true;
        };
        StartService();
        auto missing = Request();
        ASSERT_TRUE(Submit(missing));
        auto first = Wait(missing);
        EXPECT_EQ(first.m_status.m_error, ToolError::ProviderMismatch);
        EXPECT_EQ(first.m_processId, 0);
        m_configuration.m_resolve = [resolve](const auto& command, auto& resolved)
        {
            resolve(command, resolved);
            resolved.m_executableDigest = ToolDigest("changed");
            return true;
        };
        StartService();
        auto changed = Request();
        ASSERT_TRUE(Submit(changed));
        auto second = Wait(changed);
        EXPECT_EQ(second.m_status.m_error, ToolError::IdentityChanged);
        EXPECT_EQ(second.m_processId, 0);
        auto calls = std::make_shared<std::atomic_uint>(0);
        m_configuration.m_resolve = [resolve, calls](const auto& command, auto& resolved)
        {
            resolve(command, resolved);
            if (calls->fetch_add(1))
            {
                resolved.m_configurationFingerprint = ToolDigest("changed");
            }
            return true;
        };
        StartService();
        auto stale = Request();
        ASSERT_TRUE(Submit(stale));
        auto third = Wait(stale);
        EXPECT_EQ(third.m_status.m_error, ToolError::StaleAdmission);
        EXPECT_FALSE(third.m_exitCodeObserved);
    }
    TEST_F(ToolExecutionOperational, SeparateHostsExcludeSameAndOverlappingTargets)
    {
        auto running = Request("hang");
        ASSERT_TRUE(Submit(running));
        bool started = false;
        for (unsigned i = 0; i < 500; ++i)
        {
            ToolInvocationStatusV2 status;
            m_service->GetStatus(running.m_attemptId, status);
            if (status.m_stage == ToolStage::Running)
            {
                started = true;
                break;
            }
            Sleep(10);
        }
        ASSERT_TRUE(started);
        for (bool overlap : { false, true })
        {
            ToolExecutionHostConfiguration other = m_configuration;
            auto otherRoot = m_root + (overlap ? L"\\other-overlap" : L"\\other-same");
            other.m_privateStoreRoot = Windows::Utf8(otherRoot);
            auto resolve = m_configuration.m_resolve;
            auto otherCommand = m_command;
            if (overlap)
            {
                otherCommand.m_providerId = "synthetic.overlap";
            }
            other.m_resolve = [resolve, this, overlap](const auto& command, auto& resolved)
            {
                resolve(command, resolved);
                resolved.m_providerId = command.m_providerId;
                if (overlap)
                {
                    for (auto& root : resolved.m_roots)
                    {
                        if (root.m_rootId == "target")
                        {
                            root.m_absolutePath = Windows::Utf8(m_root);
                        }
                    }
                }
                return true;
            };
            ToolExecutionService service(other);
            ExternalToolProviderDescriptor provider;
            provider.m_providerId = otherCommand.m_providerId;
            provider.m_providerVersion = m_command.m_providerVersion;
            ExternalToolCommandDescriptor command;
            command.m_commandId = m_command.m_commandId;
            command.m_mode = CommandMode::Batch;
            provider.m_commands.push_back(command);
            ExternalToolDiscoveryProbeDescriptor probe;
            probe.m_probeId = m_command.m_probeId;
            provider.m_discoveryProbes.push_back(probe);
            service.ObserveProvider(provider);
            ASSERT_TRUE(service.RegisterExecutionCommand(otherCommand));
            service.FinalizeRegistration();
            auto request = Request();
            request.m_providerId = otherCommand.m_providerId;
            request.m_commandFingerprint = CanonicalToolCommand(otherCommand).m_fingerprint;
            request.m_fingerprint = CanonicalToolRequest(request).m_fingerprint;
            bool submitted = false;
            for (unsigned i = 0; i < 500; ++i)
            {
                auto result = service.Submit(request);
                if (result)
                {
                    submitted = true;
                    break;
                }
                ASSERT_EQ(result.m_error, ToolError::Busy);
                Sleep(10);
            }
            ASSERT_TRUE(submitted);
            ToolInvocationStatusV2 status;
            bool done = false;
            for (unsigned i = 0; i < 500; ++i)
            {
                service.GetStatus(request.m_attemptId, status);
                if (status.m_stage == ToolStage::Terminal)
                {
                    done = true;
                    break;
                }
                Sleep(10);
            }
            ASSERT_TRUE(done);
            EXPECT_EQ(status.m_error, ToolError::Busy);
            EXPECT_EQ(service.EnumerateRecords(0, 64).m_records[0].m_processId, 0);
        }
        ASSERT_TRUE(m_service->Cancel(running.m_attemptId));
        EXPECT_EQ(Wait(running).m_status.m_outcome, ToolOutcome::Cancelled);
    }

    TEST_F(ToolExecutionOperational, TerminalJournalWriteFailureIsNotSuccessOrReplayAuthority)
    {
        auto request = Request("hang");
        ASSERT_TRUE(Submit(request));
        bool started = false;
        for (unsigned i = 0; i < 500; ++i)
        {
            ToolInvocationStatusV2 status;
            m_service->GetStatus(request.m_attemptId, status);
            if (status.m_stage == ToolStage::Running)
            {
                started = true;
                break;
            }
            Sleep(10);
        }
        ASSERT_TRUE(started);
        Windows::Handle snapshots[2];
        for (unsigned i = 0; i < 2; ++i)
        {
            auto path = m_root + L"\\store\\records\\" + Windows::Wide(request.m_attemptId) + L".record." + std::to_wstring(i);
            snapshots[i].Reset(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
            ASSERT_TRUE(snapshots[i]);
        }
        ASSERT_TRUE(m_service->Cancel(request.m_attemptId));
        auto record = Wait(request);
        EXPECT_EQ(record.m_status.m_outcome, ToolOutcome::Cancelled);
        EXPECT_EQ(record.m_status.m_persistence, ToolPersistence::Failed);
        EXPECT_EQ(record.m_status.m_error, ToolError::JournalFailed);
        EXPECT_FALSE(ToolSucceeded(record));
        auto another = Request();
        EXPECT_EQ(m_service->Submit(another).m_error, ToolError::JournalFailed);
        m_service.reset();
        for (auto& handle : snapshots)
        {
            handle.Reset();
        }
        StartService();
        auto recovered = Wait(request);
        EXPECT_EQ(recovered.m_status.m_outcome, ToolOutcome::Interrupted);
        EXPECT_FALSE(ToolSucceeded(recovered));
    }
    TEST_F(ToolExecutionOperational, UnconfirmedCleanupQuarantinesHostUntilRecovery)
    {
        auto request = Request("hang");
        ASSERT_TRUE(Submit(request));
        bool started = false;
        for (unsigned i = 0; i < 500; ++i)
        {
            ToolInvocationStatusV2 status;
            m_service->GetStatus(request.m_attemptId, status);
            if (status.m_stage == ToolStage::Running)
            {
                started = true;
                break;
            }
            Sleep(10);
        }
        ASSERT_TRUE(started);
        WIN32_FIND_DATAW data{};
        HANDLE find = FindFirstFileW((m_root + L"\\store\\staging\\*").c_str(), &data);
        ASSERT_NE(find, INVALID_HANDLE_VALUE);
        std::wstring stage;
        do
        {
            if (std::wstring(data.cFileName).size() == 32)
            {
                stage = m_root + L"\\store\\staging\\" + data.cFileName;
                break;
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
        ASSERT_FALSE(stage.empty());
        Windows::Handle held(CreateFileW(
            (stage + L"\\scratch").c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr));
        ASSERT_TRUE(held);
        ASSERT_TRUE(m_service->Cancel(request.m_attemptId));
        auto record = Wait(request);
        EXPECT_EQ(record.m_status.m_outcome, ToolOutcome::Cancelled);
        EXPECT_EQ(record.m_status.m_cleanup, ToolCleanup::Failed);
        EXPECT_FALSE(ToolSucceeded(record));
        auto another = Request();
        EXPECT_EQ(m_service->Submit(another).m_error, ToolError::CleanupFailed);
        EXPECT_NE(GetFileAttributesW(stage.c_str()), INVALID_FILE_ATTRIBUTES);
        m_service.reset();
        held.Reset();
        StartService();
        auto recovered = Wait(request);
        EXPECT_EQ(recovered.m_processId, record.m_processId);
        EXPECT_FALSE(ToolSucceeded(recovered));
        EXPECT_EQ(GetFileAttributesW(stage.c_str()), INVALID_FILE_ATTRIBUTES);
        ASSERT_TRUE(Submit(another));
        EXPECT_TRUE(ToolSucceeded(Wait(another)));
    }

    TEST_F(ToolExecutionOperational, ApiLatencyAndShutdownRemainBounded)
    {
        auto request = Request("hang");
        ASSERT_TRUE(Submit(request));
        bool started = false;
        for (unsigned i = 0; i < 500; ++i)
        {
            ToolInvocationStatusV2 status;
            m_service->GetStatus(request.m_attemptId, status);
            if (status.m_stage == ToolStage::Running)
            {
                started = true;
                break;
            }
            Sleep(10);
        }
        ASSERT_TRUE(started);
        std::vector<double> submitTimes, statusTimes, cancelTimes;
        auto measure = [](auto&& operation)
        {
            auto start = std::chrono::steady_clock::now();
            operation();
            return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
        };
        DWORD handlesRunning = 0;
        ASSERT_TRUE(GetProcessHandleCount(GetCurrentProcess(), &handlesRunning));
        PROCESS_MEMORY_COUNTERS memory{};
        memory.cb = sizeof(memory);
        ASSERT_TRUE(K32GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory)));
        for (unsigned i = 0; i < 1000; ++i)
        {
            submitTimes.push_back(measure(
                [&]
                {
                    EXPECT_EQ(m_service->Submit(request).m_error, ToolError::Duplicate);
                }));
            statusTimes.push_back(measure(
                [&]
                {
                    ToolInvocationStatusV2 status;
                    EXPECT_TRUE(m_service->GetStatus(request.m_attemptId, status));
                }));
        }
        auto cancellation = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < 1000; ++i)
        {
            cancelTimes.push_back(measure(
                [&]
                {
                    EXPECT_TRUE(m_service->Cancel(request.m_attemptId));
                }));
        }
        auto record = Wait(request);
        EXPECT_EQ(record.m_status.m_outcome, ToolOutcome::Cancelled);
        double cancelledMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cancellation).count();
        auto p95 = [](auto& values)
        {
            std::sort(values.begin(), values.end());
            return values[values.size() * 95 / 100];
        };
        double submit = p95(submitTimes), status = p95(statusTimes), cancel = p95(cancelTimes);
        EXPECT_LE(submit, 10000);
        EXPECT_LE(status, 10000);
        EXPECT_LE(cancel, 10000);
        EXPECT_LE(cancelledMs, 5000);
        auto shutdown = measure(
            [&]
            {
                m_service->Shutdown();
            });
        EXPECT_LE(shutdown, 5000000);
        DWORD handlesAfter = 0;
        ASSERT_TRUE(GetProcessHandleCount(GetCurrentProcess(), &handlesAfter));
        EXPECT_LT(handlesAfter, handlesRunning);
        std::printf(
            "M2_METRICS submit_p95_us=%.3f status_p95_us=%.3f cancel_p95_us=%.3f cancellation_ms=%.3f shutdown_us=%.3f handles_running=%lu "
            "handles_after=%lu working_set=%llu\n",
            submit,
            status,
            cancel,
            cancelledMs,
            shutdown,
            handlesRunning,
            handlesAfter,
            static_cast<unsigned long long>(memory.WorkingSetSize));
        EXPECT_EQ(m_service->Submit(Request()).m_error, ToolError::HostStopped);
    }

    TEST_F(ToolExecutionOperational, JobCommittedMemoryCeilingIsEnforced)
    {
        m_command.m_memoryBytes = 64ULL * 1024 * 1024;
        StartService();
        auto request = Request("memory-limit");
        ASSERT_TRUE(Submit(request));
        auto record = Wait(request);
        ASSERT_TRUE(ToolSucceeded(record)) << ToolErrorName(record.m_status.m_error) << ":" << record.m_exitCode;
        auto payload = Artifact(record);
        ASSERT_EQ(payload.substr(0, 10), "allocated=");
        auto allocated = _strtoui64(payload.c_str() + 10, nullptr, 10);
        EXPECT_GT(allocated, 0);
        EXPECT_LT(allocated, 64ULL * 1024 * 1024);
    }
    TEST_F(ToolExecutionOperational, HardLinkedInputIsRejectedBeforeLaunch)
    {
        auto file = m_root + L"\\inputs\\source.txt";
        auto alias = m_root + L"\\inputs\\alias.txt";
        ASSERT_TRUE(Windows::WriteFileAtomic(file, "source"));
        ASSERT_TRUE(CreateHardLinkW(alias.c_str(), file.c_str(), nullptr));
        auto request = Request();
        request.m_inputs = { { "source", "input", "source.txt", "text/plain", ToolDigest("source"), 6 } };
        ASSERT_TRUE(Submit(request));
        auto record = Wait(request);
        EXPECT_EQ(record.m_status.m_error, ToolError::InputMismatch);
        EXPECT_EQ(record.m_processId, 0);
        ASSERT_TRUE(DeleteFileW(alias.c_str()));
        AZStd::string value;
        ASSERT_TRUE(Windows::ReadFileBounded(file, 32, value));
        EXPECT_EQ(value, "source");
    }

    TEST_F(ToolExecutionOperational, ReparseTargetIsRejectedWithoutFollowingIt)
    {
        auto link = m_root + L"\\target-link";
        auto target = m_root + L"\\target";
        ASSERT_TRUE(Windows::CreatePrivateDirectory(link));
        struct Junction
        {
            DWORD tag;
            WORD length, reserved, subOffset, subLength, printOffset, printLength;
            wchar_t paths[2048];
        } data{};
        auto substitute = L"\\??\\" + target;
        ASSERT_LT(substitute.size() + target.size() + 2, 2048);
        data.tag = IO_REPARSE_TAG_MOUNT_POINT;
        data.subLength = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
        data.printOffset = data.subLength + sizeof(wchar_t);
        data.printLength = static_cast<WORD>(target.size() * sizeof(wchar_t));
        data.length = 8 + data.printOffset + data.printLength + sizeof(wchar_t);
        std::copy(substitute.begin(), substitute.end(), data.paths);
        std::copy(target.begin(), target.end(), data.paths + substitute.size() + 1);
        {
            Windows::Handle directory(CreateFileW(
                link.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
                nullptr));
            ASSERT_TRUE(directory);
            DWORD returned = 0;
            ASSERT_TRUE(DeviceIoControl(directory.Get(), FSCTL_SET_REPARSE_POINT, &data, data.length + 8, nullptr, 0, &returned, nullptr));
        }
        auto resolve = m_configuration.m_resolve;
        m_configuration.m_resolve = [resolve, link](const auto& command, auto& resolved)
        {
            resolve(command, resolved);
            for (auto& root : resolved.m_roots)
            {
                if (root.m_rootId == "target")
                {
                    root.m_absolutePath = Windows::Utf8(link);
                }
            }
            return true;
        };
        StartService();
        auto request = Request();
        ASSERT_TRUE(Submit(request));
        auto record = Wait(request);
        EXPECT_EQ(record.m_status.m_error, ToolError::UnsafePath);
        EXPECT_EQ(record.m_processId, 0);
        ASSERT_TRUE(RemoveDirectoryW(link.c_str()));
        EXPECT_NE(GetFileAttributesW(target.c_str()), INVALID_FILE_ATTRIBUTES);
    }

} // namespace ExternalToolchain
