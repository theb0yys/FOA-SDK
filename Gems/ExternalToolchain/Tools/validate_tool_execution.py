#!/usr/bin/env python3
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Static boundaries for M2. Only compiled operational tests prove process isolation."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
class ToolExecutionPolicyError(ValueError):
    """A required execution boundary is missing."""

def validate(root: Path = ROOT) -> None:
    gem = root / "Gems/ExternalToolchain"
    code = gem / "Code"
    def read(path: str) -> str:
        target = code / path
        if not target.is_file():
            raise ToolExecutionPolicyError(f"Missing M2 source: {path}")
        return target.read_text(encoding="utf-8")
    def require(text: str, *tokens: str) -> None:
        for token in tokens:
            if re.sub(r"\s+", "", token) not in re.sub(r"\s+", "", text):
                raise ToolExecutionPolicyError(f"Missing boundary: {token}")
    public = read("Include/ExternalToolchain/ToolExecutionTypes.h")
    require(public, "ExecutionApiVersion{2, 0, 0}", "windows-lpac-registry-read-batch-v1",
            "ToolInvocationRequestV2", "ToolInvocationRecordV2", "ToolOutcome", "ToolVerification",
            "ToolCleanup", "ToolPersistence", "ToolMaxLogBytes", "ToolMaxDocumentBytes")
    require(read("Include/ExternalToolchain/ExternalToolchainTypes.h"), "HostApiVersion{ 1, 1, 0 }")
    pure = read("Source/Execution/ToolExecutionContracts.cpp")
    for token in ("CreateProcess", "QProcess", "ProcessWatcher", "ToolExecutionJournal", "std::filesystem",
                  "TaintedGrailModdingSDK/", "AzFramework/", "AzToolsFramework/"):
        if token in pure:
            raise ToolExecutionPolicyError(f"Execution Core gained forbidden dependency: {token}")
    require(pure, "foa-tool-invocation-v2", "foa-tool-output-manifest-v2", "foa-tool-invocation-record-v2",
            "kParseValidateEncodingFlag", "MemberCount()", "ToolMaxDocumentBytes", "SecretUseUnsupported")
    settings = json.loads((gem / "Registry/external_toolchain.setreg").read_text(encoding="utf-8"))
    host = settings["O3DE"]["ExternalToolchain"]["Host"]
    for key in ("ProcessExecutionEnabled", "AllowShellCommands", "AssetHandoffEnabled"):
        if host[key] is not False:
            raise ToolExecutionPolicyError(f"Default changed: {key}")
    require(read("Source/Execution/ToolExecutionAdmission.h"), "DenyToolExecution", "m_enabled = false",
            "ToolAdmissionLease", "Consume", "m_hostInstanceId")
    service = read("Source/Execution/ToolExecutionService.cpp")
    require(service, "m_workers[2]", "m_queue.size()>=16", "m_entries.size()>=ToolMaxRecords",
            "worker.join()", "m_beginDrain", "ValidateToolRequest", "SaveLogs")
    for name, end in (("Submit(", "GetStatus("), ("GetStatus(", "Cancel("), ("Cancel(", "ReadLog(")):
        region = service.split("ToolExecutionService::" + name, 1)[1].split("ToolExecutionService::" + end, 1)[0]
        for token in ("ReadFile(", "WaitForSingleObject", "sleep_for", "m_journal.", "->Run("):
            if token in region:
                raise ToolExecutionPolicyError(f"Blocking work in {name}: {token}")
    windows = read("Source/Execution/Platform/Windows/ToolProcessBackend_Windows.cpp")
    require(windows, "CREATE_SUSPENDED", "CREATE_NO_WINDOW", "CREATE_UNICODE_ENVIRONMENT",
            "PROC_THREAD_ATTRIBUTE_HANDLE_LIST", "PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES",
            "PROC_THREAD_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY", "AssignProcessToJobObject",
            "JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE", "JOB_OBJECT_LIMIT_ACTIVE_PROCESS", "JOB_OBJECT_LIMIT_JOB_MEMORY",
            "IsLpac", "lease->Consume", "TerminateJobObject", "JobEmpty", "ToolMaxPipeBytes",
            "DecodeToolManifest", "SameConfiguration", "WriteIntent", "ClearIntent")
    for forbidden in ("ShellExecute", "system(", "QProcess", "ProcessWatcher", "CREATE_BREAKAWAY_FROM_JOB"):
        if forbidden in windows:
            raise ToolExecutionPolicyError(f"Forbidden execution route: {forbidden}")
    filesystem = read("Source/Execution/Platform/Windows/ToolSandbox_Windows.cpp")
    require(filesystem, "FILE_FLAG_OPEN_REPARSE_POINT", "GetFinalPathNameByHandleW", "nNumberOfLinks",
            "DRIVE_FIXED", "CreateAppContainerProfile", "DeleteAppContainerProfile", "CreatePrivateDirectory",
            "m_identity!=identity", "FILE_SHARE_READ", "QuoteArgument", "FlushFileBuffers")
    require(filesystem, "RegistryReadCapabilitySid", "capabilities->GroupCount != 1",
            "EqualSid(capabilities->Groups[0].Sid, RegistryReadCapabilitySid())",
            "capabilities->Groups[0].Attributes != SE_GROUP_ENABLED")
    journal = read("Source/Execution/ToolExecutionJournal.cpp")
    require(journal, "ToolMaxRecords", "m_writerLock", "profileCreated", "m_profileName", "Frame(", "m_sequences",
            "ToolMaxLogBytes", "DecodeToolRecord", "m_journalBytes")
    cmake = read("CMakeLists.txt")
    require(cmake, ".Execution.Core.Static", ".Execution.Host.Static", ".Execution.Tests",
            ".Execution.Operational.Tests", ".Execution.Fixture", "FOA_M2_FIXTURE", "NO_UNITY")
    tests_manifest = read("externaltoolchain_execution_tests_files.cmake")
    if "Source/" in tests_manifest:
        raise ToolExecutionPolicyError("Tests must link production objects, not recompile them.")
    require(read("Tests/ToolExecutionOperationalTests.cpp"),
            "SuccessRequiresLpacVerifiedArtifactAndDurableRecord", "ExplicitDenialAndExpiredLeaseNeverRunTheFixture",
            "TimeoutAndCancellationCleanUpTheProcess", "NetworkAndEnvironmentAreIsolated",
            "SurvivingDescendantAndFloodAreContained", "Native fixture path is required")
    require(read("Source/Editor/ExternalToolchainEditorSystemComponent.cpp"),
            "m_execution->Connect()", "m_execution->Shutdown()", "m_execution->FinalizeRegistration()",
            "NotifyQtApplicationAvailable", "QCoreApplication::aboutToQuit", "QObject::disconnect(m_quitConnection)")

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=ROOT)
    args = parser.parse_args()
    try:
        validate(args.repo_root)
    except (ToolExecutionPolicyError, OSError, ValueError, KeyError) as error:
        parser.exit(1, f"FAILED: {error}\n")
    print("PASSED: M2 source boundaries. Native process/isolation proof is a separate compiled operational lane.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
