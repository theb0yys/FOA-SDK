/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace ExternalToolchain
{
    struct ToolExecutionApiVersion
    {
        AZ::u32 m_major = 2, m_minor = 0, m_patch = 0;
    };
    inline constexpr ToolExecutionApiVersion ExecutionApiVersion{ 2, 0, 0 };
    inline constexpr const char* ToolExecutionProfile = "windows-lpac-registry-read-batch-v1";
    inline constexpr const char* ToolArgumentConvention = "windows-msvc-argv-v1";
    inline constexpr size_t ToolMaxFiles = 64, ToolMaxArguments = 128, ToolMaxEnvironment = 32;
    inline constexpr size_t ToolMaxDocumentBytes = 256 * 1024, ToolMaxLogBytes = 256 * 1024;
    inline constexpr size_t ToolMaxLogPage = 16 * 1024, ToolMaxRecords = 1024;
    inline constexpr AZ::u64 ToolMaxArtifactBytes = 1024ULL * 1024 * 1024;
    inline constexpr AZ::u64 ToolMaxPipeBytes = 64ULL * 1024 * 1024;

    enum class ToolStage : AZ::u8
    {
        Queued,
        Preparing,
        Running,
        Draining,
        Verifying,
        Finalizing,
        Terminal
    };
    enum class ToolOutcome : AZ::u8
    {
        NotAttempted,
        Running,
        ExitedZero,
        ExitedNonzero,
        Cancelled,
        TimedOut,
        Interrupted,
        Failed
    };
    enum class ToolVerification : AZ::u8
    {
        NotRun,
        Passed,
        Failed
    };
    enum class ToolCleanup : AZ::u8
    {
        NotRequired,
        Pending,
        Complete,
        Failed
    };
    enum class ToolPersistence : AZ::u8
    {
        NotWritten,
        Durable,
        Failed
    };
    enum class ToolArgumentKind : AZ::u8
    {
        Literal,
        Input,
        Output,
        Scratch,
        Secret
    };
    enum class ToolError : AZ::u8
    {
        None,
        InvalidContract,
        UnsupportedVersion,
        Disabled,
        AdmissionDenied,
        StaleAdmission,
        SecretUseUnsupported,
        ProviderMismatch,
        RegistrationClosed,
        Duplicate,
        QueueFull,
        NotFound,
        Busy,
        UnsafePath,
        IdentityChanged,
        InputMismatch,
        IsolationUnavailable,
        ContainmentFailed,
        LaunchFailed,
        OutputLimitExceeded,
        OutputInvalid,
        CleanupFailed,
        JournalFailed,
        StoreFull,
        HostStopped,
        UnsupportedPlatform
    };

    struct ToolResult
    {
        ToolError m_error = ToolError::None;
        explicit operator bool() const
        {
            return m_error == ToolError::None;
        }
    };

    struct ToolFileReference
    {
        AZStd::string m_id, m_rootId, m_relativePath, m_kind, m_sha256;
        AZ::u64 m_bytes = 0;
    };
    struct ToolExpectedOutput
    {
        AZStd::string m_id, m_relativePath, m_kind;
        AZ::u64 m_maxBytes = ToolMaxArtifactBytes;
    };
    struct ToolArgument
    {
        ToolArgumentKind m_kind = ToolArgumentKind::Literal;
        AZStd::string m_value;
    };
    struct ToolEnvironmentValue
    {
        AZStd::string m_name, m_value;
    };

    struct ToolExecutionCommandV2
    {
        AZ::u32 m_version = 2;
        AZStd::string m_providerId, m_providerVersion, m_commandId, m_probeId;
        AZStd::string m_profile = ToolExecutionProfile, m_argumentConvention = ToolArgumentConvention;
        AZStd::vector<AZStd::string> m_inputKinds, m_outputKinds, m_environmentNames;
        AZ::u32 m_timeoutMilliseconds = 30000, m_maxProcesses = 8;
        AZ::u64 m_memoryBytes = ToolMaxArtifactBytes;
    };

    struct ToolInvocationRequestV2
    {
        AZ::u32 m_version = 2;
        AZStd::string m_attemptId, m_commandFingerprint, m_fingerprint;
        AZStd::string m_providerId, m_commandId, m_targetRootId;
        AZStd::vector<ToolArgument> m_arguments;
        AZStd::vector<ToolFileReference> m_inputs;
        AZStd::vector<ToolExpectedOutput> m_outputs;
        AZStd::vector<ToolEnvironmentValue> m_environment;
        AZ::u32 m_timeoutMilliseconds = 30000;
    };

    struct ToolInvocationStatusV2
    {
        AZStd::string m_attemptId;
        AZ::u64 m_sequence = 0;
        ToolStage m_stage = ToolStage::Queued;
        ToolOutcome m_outcome = ToolOutcome::NotAttempted;
        ToolVerification m_verification = ToolVerification::NotRun;
        ToolCleanup m_cleanup = ToolCleanup::NotRequired;
        ToolPersistence m_persistence = ToolPersistence::NotWritten;
        ToolError m_error = ToolError::None;
    };
    struct ToolOutputManifestV2
    {
        AZ::u32 m_version = 2;
        AZStd::string m_attemptId, m_requestFingerprint;
        AZStd::vector<ToolFileReference> m_outputs;
    };
    struct ToolInvocationRecordV2
    {
        AZ::u32 m_version = 2;
        ToolInvocationStatusV2 m_status;
        AZStd::string m_requestFingerprint, m_commandFingerprint, m_profileFingerprint, m_executableDigest;
        AZ::u64 m_startedUtcMilliseconds = 0, m_finishedUtcMilliseconds = 0;
        AZ::u64 m_processId = 0, m_processCreationTime = 0;
        AZ::u32 m_exitCode = 0;
        bool m_exitCodeObserved = false, m_descendantsTerminated = false;
        bool m_stdoutTruncated = false, m_stderrTruncated = false;
        AZStd::vector<ToolFileReference> m_outputs;
    };
    struct ToolLogPage
    {
        ToolError m_error = ToolError::None;
        AZStd::string m_text;
        AZ::u64 m_nextCursor = 0;
        bool m_truncated = false;
    };
    struct ToolRecordPage
    {
        ToolError m_error = ToolError::None;
        AZStd::vector<ToolInvocationRecordV2> m_records;
        size_t m_nextOffset = 0;
    };
    struct ToolCanonicalResult
    {
        ToolError m_error = ToolError::None;
        AZStd::string m_json, m_fingerprint;
        explicit operator bool() const
        {
            return m_error == ToolError::None;
        }
    };

    // Streaming SHA-256 over exact bytes, with no OS or filesystem dependencies.
    class ToolSha256
    {
    public:
        ToolSha256();
        void Update(const void* bytes, size_t size);
        AZStd::string Finish();

    private:
        void Block(const unsigned char* bytes);
        AZ::u32 m_state[8];
        unsigned char m_buffer[64]{};
        AZ::u64 m_bytes = 0;
        size_t m_used = 0;
    };

    AZStd::string ToolDigest(const AZStd::string& bytes);
    bool ToolSafeId(const AZStd::string& value);
    bool ToolSafeRelativePath(const AZStd::string& value);
    bool ToolSafeText(const AZStd::string& value, size_t maximum = 4096);
    bool ToolValidDigest(const AZStd::string& value);
    bool ToolSucceeded(const ToolInvocationRecordV2& record);
    const char* ToolErrorName(ToolError error);
    ToolCanonicalResult CanonicalToolCommand(const ToolExecutionCommandV2& command);
    ToolCanonicalResult CanonicalToolRequest(const ToolInvocationRequestV2& request);
    ToolResult ValidateToolRequest(const ToolInvocationRequestV2& request, const ToolExecutionCommandV2& command);
    ToolCanonicalResult EncodeToolManifest(const ToolOutputManifestV2& manifest);
    ToolResult DecodeToolManifest(const AZStd::string& json, ToolOutputManifestV2& manifest);
    ToolCanonicalResult EncodeToolRecord(const ToolInvocationRecordV2& record);
    ToolResult DecodeToolRecord(const AZStd::string& json, ToolInvocationRecordV2& record);
} // namespace ExternalToolchain
