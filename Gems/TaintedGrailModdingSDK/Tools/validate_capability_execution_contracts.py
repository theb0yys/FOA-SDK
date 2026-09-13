#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Check the M0 Core-only execution contract boundary; this is static evidence."""
from __future__ import annotations
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CODE = Path("Gems/TaintedGrailModdingSDK/Code")
TOOLS = Path("Gems/TaintedGrailModdingSDK/Tools")
FAMILY = tuple(f"CapabilityExecution{name}.{ext}" for name in ("Contracts", "Canonical", "Validation") for ext in ("h", "cpp"))
TESTS = tuple(f"Tests/CapabilityExecution{name}Tests.cpp" for name in ("Contract", "Canonical", "Validation"))
TARGET = "CapabilityExecution.Tests"
MANIFEST = "taintedgrailmoddingsdk_capability_execution_tests_files.cmake"
WORKFLOW = Path(".github/workflows/tainted-grail-sdk-pr-validation.yml")
KINDS = (
    "CapabilityDescriptorV1", "CapabilityProviderBindingV1", "ArtifactReferenceV1", "ExpectedArtifactV1",
    "ArtifactRecordV1", "OptionV1", "CapabilityExecutionRequestV1", "CapabilitySupportDecisionV1",
    "CapabilityQualificationDecisionV1", "CapabilityEnvironmentDecisionV1", "CapabilityPolicyDecisionV1",
    "CapabilityAuthorizationReceiptV1", "TargetMutationClaimV1", "RollbackStepV1", "RollbackPlanV1",
    "CapabilityPhasePlanV1", "CapabilityExecutionPlanV1", "FailureRecordV1", "DiagnosticReferenceV1",
    "TargetObservationV1", "PhaseExtensionReferenceV1", "CapabilityPhaseReceiptV1", "RollbackStepReceiptV1",
    "RollbackReceiptV1", "CapabilityExecutionReceiptV1",
)
FORBIDDEN = (
    r"\bQ[A-Z][a-z]\w*", r"AzToolsFramework", r"AzQtComponents", r"FrameworkBus",
    r"\b(?:EBus|SerializeContext|Reflect|QProcess|SystemFile|FileIOBase)\b",
    r"\b(?:fopen|fwrite|system|popen|CreateProcess|ShellExecute|WinHttp|socket|getenv)\s*\(",
    r"#\s*include\s*[<\"][^>\"]*(?:filesystem|fstream|Windows[.]h|Persistence|Registry|ExternalToolchain)",
)
def read(root: Path, path: Path) -> str:
    return (root / path).read_text(encoding="utf-8")

def code_only(text: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)

def validate(root: Path = ROOT) -> list[str]:
    errors: list[str] = []
    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)
    try:
        source = root / CODE / "Source"
        actual = {p.name for p in source.glob("CapabilityExecution*") if p.is_file()}
        require(actual == set(FAMILY), "Production family must be exactly the six M0 files.")
        texts = {name: read(root, CODE / "Source" / name) for name in FAMILY}
        for name, text in texts.items():
            clean = code_only(text)
            for pattern in FORBIDDEN:
                require(not re.search(pattern, clean), f"Forbidden dependency or side effect in {name}: {pattern}")
            for include in re.findall(r'#include\s+"([^"]+)"', clean):
                require(include in FAMILY or include in ("CanonicalFingerprint.h", "DeterministicContractJson.h"),
                        f"Undeclared local dependency in {name}: {include}")
        for name in FAMILY:
            owners = []
            for manifest in (root / CODE).glob("*files.cmake"):
                entries = re.findall(r"(?m)^\s*(Source/\S+)\s*$", read(root, manifest))
                if f"Source/{name}" in entries:
                    owners.append(manifest.name)
                    require(entries.count(f"Source/{name}") == 1, f"Duplicate inventory: {name}")
            require(owners == ["taintedgrailmoddingsdk_core_files.cmake"], f"Unique Core ownership required: {name}")
        for path in source.rglob("*"):
            if path.is_file() and path.name not in FAMILY and path.suffix in (".h", ".cpp") and path.relative_to(source).as_posix() not in ("ExecutionFramework/FrameworkExecutionCodec.h", "ExecutionPlanning/FrameworkPlannerService.h"):
                require(not re.search(r'#include\s+[<"][^>"]*CapabilityExecution', code_only(path.read_text(encoding="utf-8"))),
                        f"Unauthorised production consumer: {path.name}")
        manifest = read(root, CODE / MANIFEST)
        require(tuple(re.findall(r"(?m)^\s*(Tests/\S+)\s*$", manifest)) == TESTS, "Exact three-file test manifest required.")
        require("Source/" not in manifest, "Tests must link Core instead of recompiling production.")
        for test in TESTS:
            body = read(root, CODE / test)
            require("TEST(" in body and "CapabilityExecution" in body, f"Compiled tests absent: {test}")
            require("QCoreApplication" not in body and "AzToolsFramework" not in body, f"Test must remain Core-only: {test}")
        cmake = read(root, CODE / "CMakeLists.txt")
        blocks = re.findall(r"^    ly_add_target\((.*?)^    \)", cmake, re.S | re.M)
        target = [b for b in blocks if "NAME ${gem_name}." + TARGET + " " in b]
        require(len(target) == 1, "Exactly one dedicated compiled target required.")
        if target:
            b = target[0]
            require("NO_UNITY" in b and MANIFEST in b, "Dedicated target must use NO_UNITY and exact manifest.")
            dependencies = b.split("BUILD_DEPENDENCIES", 1)[-1].split()
            require(dependencies == ["PRIVATE", "Gem::${gem_name}.Core.Static", "AZ::AzTest"], "Dedicated target must link only Core and AzTest.")
        require("NAME Gem::${gem_name}.CapabilityExecution.Tests" in cmake and
                "TEST_COMMAND ${tg_capability_execution_ctest_command}" in cmake, "Dedicated CTest registration required.")
        header = texts["CapabilityExecutionContracts.h"]
        for fragment in ("foa-capability-execution-v1", "foa-capability-execution-canonical-json-v1",
                         "ContractVersion = 1", "SizeBudget", "MaximumCanonicalBytes", "MaximumCollection", "MaximumNesting"):
            require(fragment in header, f"Required contract fragment missing: {fragment}")
        for kind in KINDS:
            require(f"struct {kind}" in header, f"Missing contract type: {kind}")
            require(f"Canonicalize(const {kind}&" in texts["CapabilityExecutionCanonical.h"], f"Missing canonical projection: {kind}")
            require(f"Validate(const {kind}&" in texts["CapabilityExecutionValidation.h"], f"Missing validation: {kind}")
        workflow = read(root, WORKFLOW)
        require("contents: read" in workflow and not re.search(r"\w+: write", workflow), "CI must be read-only.")
        jobs = re.findall(r"(?ms)^  capability-execution-compiled:\n(.*?)(?=^  [\w-]+:|\Z)", workflow)
        require(len(jobs) == 1, "Dedicated automatic compiled CI job required.")
        if jobs:
            for fragment in ("runs-on: windows-2022", "needs: static-validation", "persist-credentials: false",
                             "github.event.pull_request.head.sha || github.sha", "O3DE_COMMIT", "TEST_TARGET: TaintedGrailModdingSDK.CapabilityExecution.Tests",
                             "--parallel 2", "--no-tests=error", "cmake --build", "ctest --test-dir"):
                require(fragment in jobs[0], f"Compiled CI missing: {fragment}")
        runner = read(root, TOOLS / "run_local_validation.py")
        require('"validate_capability_execution_contracts.py"' in runner and "CapabilityExecution" in runner,
                "M1 static and compiled gates must enter local validation.")
    except (OSError, UnicodeError) as exc:
        errors.append(f"Missing or unreadable boundary input: {exc}")
    return errors

def main() -> int:
    errors = validate()
    for error in errors:
        print(f"ERROR: {error}")
    print("FAILED" if errors else "PASSED: M1 execution contracts remain bounded and Core-only.")
    return int(bool(errors))

if __name__ == "__main__":
    raise SystemExit(main())
