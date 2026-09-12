#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Static M3 ownership guard. Native, persistence and Editor proof run separately."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
CODE = Path("Gems/TaintedGrailModdingSDK/Code")
FAMILY = ('FrameworkArtifactRepository.cpp', 'FrameworkArtifactRepository.h', 'FrameworkExecutionCodec.cpp', 'FrameworkExecutionCodec.h', 'FrameworkExecutionEvidenceProjection.cpp', 'FrameworkExecutionEvidenceProjection.h', 'FrameworkExecutionPolicyService.cpp', 'FrameworkExecutionPolicyService.h', 'FrameworkExecutionRepository.cpp', 'FrameworkExecutionRepository.h', 'FrameworkExecutionService.cpp', 'FrameworkExecutionService.h', 'FrameworkExecutionTypes.h', 'FrameworkProviderService.cpp', 'FrameworkProviderService.h', 'FrameworkTargetOwnershipLedger.cpp', 'FrameworkTargetOwnershipLedger.h', 'FrameworkToolExecutionAdapter.cpp', 'FrameworkToolExecutionAdapter.h')

def validate(root=ROOT):
    errors = []
    try:
        directory = root / CODE / "Source/ExecutionFramework"
        actual = tuple(sorted(p.name for p in directory.iterdir() if p.is_file()))
        if actual != FAMILY:
            errors.append("M3 production inventory differs from the accepted family.")
        manifest = (root / CODE / "taintedgrailmoddingsdk_framework_execution_files.cmake").read_text(encoding="utf-8")
        entries = re.findall(r"(?m)^\s*Source/ExecutionFramework/(\S+)\s*$", manifest)
        if tuple(sorted(entries)) != FAMILY:
            errors.append("M3 files require unique ownership in their dedicated target.")
        for name in actual:
            text = (directory / name).read_text(encoding="utf-8")
            text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
            if re.search(r"\b(CreateProcess\w*|ShellExecute\w*|system|popen)\s*\(", text):
                errors.append(f"Framework bypasses M2 process ownership: {name}")
            if re.search(r"(?:->|\.)\s*Connect\s*\(", text):
                errors.append(f"Framework must not connect another public M2 handler: {name}")
            if re.search(r"RegisterCandidateEvidence|PromoteCandidate|RegisterEvidence\s*\(", text):
                errors.append(f"Candidate projection must not publish or promote evidence: {name}")
        cmake = (root / CODE / "CMakeLists.txt").read_text(encoding="utf-8")
        for fragment in ("NAME ${gem_name}.FrameworkExecution.Static STATIC", "FrameworkExecution.${tg_m3_lane}",
                         "taintedgrailmoddingsdk_framework_execution_operational_tests_files.cmake", "ExternalToolchain.Execution.Fixture"):
            if fragment not in cmake:
                errors.append("Missing M3 build/native lane: " + fragment)
        for suffix in ("ProviderPolicy", "Repository", "Orchestrator", "Operational"):
            text = (root / CODE / f"Tests/FrameworkExecution{suffix}Tests.cpp").read_text(encoding="utf-8")
            if not re.search(r"\bTEST(?:_F)?\(", text) or "GTEST_SKIP" in text:
                errors.append("Missing or skipped M3 acceptance lane: " + suffix)
        workflow = (root / ".github/workflows/tainted-grail-sdk-pr-validation.yml").read_text(encoding="utf-8")
        for target in ("TaintedGrailModdingSDK.FrameworkExecution.Tests", "TaintedGrailModdingSDK.FrameworkExecution.Operational.Tests"):
            if target not in workflow:
                errors.append("Missing read-only native CI target: " + target)
    except (OSError, UnicodeError) as error:
        errors.append(str(error))
    return errors

if __name__ == "__main__":
    errors = validate()
    for error in errors:
        print("ERROR: " + error)
    print("FAILED" if errors else "PASSED: M3 static ownership and required lane registration.")
    raise SystemExit(bool(errors))
