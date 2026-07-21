#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Validate the engine-neutral Tainted Framework editor-service port."""

from __future__ import annotations

import sys
from pathlib import Path


class TaintedFrameworkEditorServicesError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise TaintedFrameworkEditorServicesError(message)


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="strict")
    except OSError as exc:
        raise TaintedFrameworkEditorServicesError(f"Unable to read {path}: {exc}") from exc


def ordered(text: str, fragments: tuple[str, ...], message: str) -> None:
    cursor = -1
    for fragment in fragments:
        position = text.find(fragment, cursor + 1)
        require(position >= 0, f"Missing required service fragment: {fragment}")
        require(position > cursor, message)
        cursor = position


def validate(repo_root: Path) -> None:
    gem = repo_root / "Gems" / "TaintedGrailModdingSDK"
    source = gem / "Code" / "Source"
    tests = gem / "Code" / "Tests"
    tools = gem / "Tools"

    header = read(source / "TaintedFrameworkEditorServices.h")
    implementation = read(source / "TaintedFrameworkEditorServices.cpp")
    foundation_header = read(source / "FoundationService.h")
    bridge = read(source / "FoundationTaintedFrameworkEditorServices.cpp")
    framework_manifest = read(gem / "Code" / "taintedgrailmoddingsdk_framework_files.cmake")
    test_manifest = read(gem / "Code" / "taintedgrailmoddingsdk_tainted_framework_tests_files.cmake")
    compiled_tests = read(tests / "TaintedFrameworkEditorServicesTests.cpp")
    local_runner = read(tools / "run_local_validation.py")

    for required in (
        "CompatibilityDecision",
        "ApiSurfaceDecision",
        "ConfigurationDefault",
        "ActivationPlan",
        "EvaluateCompatibility",
        "GetApiSurfaceDecisions",
        "GetConfigurationDefaults",
        "GetDiagnosticVocabulary",
        "BuildActivationPlan",
        "m_candidateEvidenceSubmissionEligible",
    ):
        require(required in header, f"Public editor-service contract missing {required}")

    for prohibited in (
        "CatalogDatabase&",
        "SourceEvidenceRegistry&",
        "WorkspaceModel&",
        "GameProfile&",
        "BepInEx",
        "UnityEngine",
        "Harmony",
        "QFile",
        "std::filesystem",
    ):
        require(prohibited not in header, f"Public editor service exposes prohibited authority: {prohibited}")
        require(prohibited not in implementation, f"Editor service imports prohibited runtime/file authority: {prohibited}")

    ordered(
        implementation,
        (
            'if (row.m_status == "live_load_validated")',
            "if (row.m_gameVersion == gameVersion)",
            "result.m_status = ReadinessStatus::Ready",
        ),
        "Compatibility readiness must require exact evidence-backed game-version matching",
    )
    require('row.m_status == "blocked"' in implementation, "Blocked upstream branches must remain blocked")
    require('"upstream_branch_blocked"' in implementation, "Blocked branch reason must be stable")
    require('"exact_game_version_not_evidence_backed"' in implementation, "Version drift reason must be stable")
    require('"branch_runtime_pair_not_in_canonical_knowledge"' in implementation,
            "Unknown branch/runtime pairs must fail closed")

    for false_flag in (
        "plan.m_runtimeInvocationAllowed = false",
        "plan.m_fileWriteAllowed = false",
        "plan.m_catalogMutationAllowed = false",
    ):
        require(false_flag in implementation, f"Permanent no-authority flag missing: {false_flag}")
    require("plan.m_candidateEvidenceSubmissionEligible" in implementation,
            "Governed candidate-evidence eligibility must be explicit")
    require("m_candidateEvidenceSubmissionAllowed" not in header + implementation,
            "Editor service must not claim evidence submission authority")

    require("TaintedFrameworkEditorServices::Service m_taintedFrameworkEditorServices" in foundation_header,
            "Foundation must own one persistent editor-service instance")
    require("GetTaintedFrameworkEditorServices" in foundation_header,
            "Foundation editor-service accessor is missing")
    require("return m_taintedFrameworkEditorServices" in bridge,
            "Foundation accessor must return the persistent service")

    for required in (
        "Source/TaintedFrameworkEditorServices.cpp",
        "Source/TaintedFrameworkEditorServices.h",
        "Source/FoundationTaintedFrameworkEditorServices.cpp",
    ):
        require(required in framework_manifest, f"Framework build graph missing {required}")
    require("Tests/TaintedFrameworkEditorServicesTests.cpp" in test_manifest,
            "Production-linked editor-service tests are not compiled")

    for test_name in (
        "ExactMonoEvidenceIsReady",
        "Il2CppRemainsBlocked",
        "VersionDriftIsUnsupported",
        "OnlyRuntimeReportIsConsumerReady",
        "ActivationPlanIsReadOnlyAndDeterministic",
        "BlockedPlanIsNotEvidenceEligible",
        "FoundationOwnsPersistentService",
    ):
        require(test_name in compiled_tests, f"Compiled service coverage missing {test_name}")

    require("validate_tainted_framework_editor_services.py" in local_runner,
            "Authoritative local validation does not include the editor-service gate")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate(repo_root)
    except (OSError, TaintedFrameworkEditorServicesError) as exc:
        print(f"Tainted Framework editor-service validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Framework editor-service validation passed: exact compatibility, "
        "deterministic read-only projections, governed evidence eligibility, "
        "persistent Foundation ownership, production-linked tests, and no "
        "runtime/file/catalog authority."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
