# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""M5 owner/profile guard; actual native and Editor proof is a separate lane."""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[3]
CODE = Path("Gems/TaintedGrailModdingSDK/Code")
FILES = {"FrameworkSyntheticTarget.h", "FrameworkSyntheticTarget.cpp", "FrameworkSyntheticWorkflow.h",
         "FrameworkSyntheticWorkflow.cpp", "FrameworkSyntheticProvider.cpp"}
def validate(root=ROOT):
    errors = []
    try:
        source = root / CODE / "Source/ExecutionSynthetic"
        if {p.name for p in source.iterdir() if p.is_file()} != FILES:
            errors.append("Synthetic provider inventory differs from its accepted owner.")
        for name in FILES:
            text = (source / name).read_text(encoding="utf-8")
            if re.search(r"\b(?:CreateProcess\w*|ShellExecute\w*|system|popen)\s*\(", text):
                errors.append("Synthetic code bypasses M2 process supervision: " + name)
            if re.search(r"RegisterCandidateEvidence|PromoteCandidate|RegisterEvidence\s*\(",text):
                errors.append("Synthetic observations must not publish game evidence: " + name)
        provider = (root / CODE / "Source/ExecutionFramework/FrameworkProviderService.cpp").read_text()
        provider = " ".join(provider.split())
        for marker in ("if (!m_synthetic) return Supported(d)", "FrameworkSyntheticTarget::Capability", "FrameworkSyntheticTarget::Profile()"):
            if marker not in provider:
                errors.append("Missing default-staging or exact synthetic profile gate: " + marker)
        target = (source / "FrameworkSyntheticTarget.cpp").read_text()
        for marker in ("foa-synthetic-target-v1", "GENERIC_READ | GENERIC_WRITE, 0", "FlushFileBuffers", "Error::Drifted"):
            if marker not in target:
                errors.append("Missing target transaction guard: " + marker)
        tests = (root / CODE / "Tests/FrameworkExecutionOperationalTests.cpp").read_text()
        for case in ("SixActualProcessesVerifyAndRestoreTargetThenReopenWithoutReplay", "NonzeroLaunchRestoresBytesAndPreservesOriginalFailure",
                     "CancelledLaunchIsJoinedAndRolledBack", "InterruptedOwnedPostimageNeedsFreshConfirmationForRecovery",
                     "CorruptBackupOrForeignPostimageIsNeverOverwrittenByRollback"):
            if case not in tests:
                errors.append("Missing synthetic native acceptance: " + case)
        cmake = (root / CODE / "CMakeLists.txt").read_text()
        if "FOA_M5_PROVIDER=$<TARGET_FILE:TaintedGrailModdingSDK.Synthetic.Provider>" not in cmake:
            errors.append("Native tests lack the actual supervised M5 provider.")
        for name in ("taintedgrailmoddingsdk_framework_synthetic_files.cmake", "taintedgrailmoddingsdk_synthetic_provider_files.cmake"):
            if name not in cmake:
                errors.append("Missing owned M5 source target: " + name)
    except (OSError, UnicodeError) as error:
        errors.append(str(error))
    return errors
if __name__ == "__main__":
    errors = validate()
    for error in errors: print("ERROR: " + error)
    print("FAILED" if errors else "PASSED: M5 static owner and synthetic profile guards.")
    raise SystemExit(bool(errors))
