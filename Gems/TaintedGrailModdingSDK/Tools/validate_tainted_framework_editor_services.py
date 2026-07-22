#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Validate the engine-neutral Tainted Framework editor-service port."""

from __future__ import annotations

import json
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
        raise TaintedFrameworkEditorServicesError(
            f"Unable to read {path}: {exc}"
        ) from exc


def load_json(path: Path) -> object:
    try:
        return json.loads(read(path))
    except json.JSONDecodeError as exc:
        raise TaintedFrameworkEditorServicesError(
            f"Unable to parse {path}: {exc}"
        ) from exc


def ordered(text: str, fragments: tuple[str, ...], message: str) -> None:
    cursor = -1
    for fragment in fragments:
        position = text.find(fragment, cursor + 1)
        require(
            position >= 0,
            f"Missing required service fragment: {fragment}",
        )
        require(position > cursor, message)
        cursor = position


def validate(repo_root: Path) -> None:
    gem = repo_root / "Gems" / "TaintedGrailModdingSDK"
    source = gem / "Code" / "Source"
    tests = gem / "Code" / "Tests"
    tools = gem / "Tools"
    docs = repo_root / "docs" / "tainted-grail-sdk"

    header = read(source / "TaintedFrameworkEditorServices.h")
    implementation = read(source / "TaintedFrameworkEditorServices.cpp")
    foundation_header = read(source / "FoundationService.h")
    bridge = read(source / "FoundationTaintedFrameworkEditorServices.cpp")
    framework_manifest = read(
        gem / "Code" / "taintedgrailmoddingsdk_framework_files.cmake"
    )
    test_manifest = read(
        gem
        / "Code"
        / "taintedgrailmoddingsdk_tainted_framework_tests_files.cmake"
    )
    compiled_tests = read(
        tests / "TaintedFrameworkEditorServicesTests.cpp"
    )
    local_runner = read(tools / "run_local_validation.py")
    fixtures = load_json(
        gem / "Knowledge" / "TaintedFramework" / "golden" / "fixtures.json"
    )
    service_document = read(docs / "TAINTED_FRAMEWORK_EDITOR_SERVICES.md")
    review_document = read(
        docs / "TAINTED_FRAMEWORK_EDITOR_SERVICES_REVIEW.md"
    )

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
        "m_bepInExVersion",
        "ExtensionAPI::ProfileView",
        "m_candidateEvidenceSubmissionEligible",
    ):
        require(
            required in header,
            f"Public editor-service contract missing {required}",
        )

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
        require(
            prohibited not in header,
            f"Public editor service exposes prohibited authority: {prohibited}",
        )
        require(
            prohibited not in implementation,
            "Editor service imports prohibited runtime/file authority: "
            f"{prohibited}",
        )

    require(
        "row.m_runtimeTarget != runtime" in implementation,
        "Compatibility must bind the canonical runtime-target field",
    )
    require(
        "row.m_runtime != runtime" not in implementation,
        "Compatibility uses a nonexistent runtime field",
    )
    require(
        "row.m_bepInExVersion != bepInExVersion" in implementation,
        "Ready compatibility must bind the exact evidence-backed BepInEx version",
    )
    require(
        '"exact_bepinex_version_not_evidence_backed"' in implementation,
        "BepInEx drift reason must be stable",
    )
    require(
        '"ambiguous_exact_compatibility_observations"' in implementation,
        "Conflicting exact observations must fail closed",
    )
    ordered(
        implementation,
        (
            'if (row.m_status == "blocked")',
            'if (row.m_status == "live_load_validated")',
            "if (row.m_gameVersion != gameVersion)",
            "if (row.m_bepInExVersion != bepInExVersion)",
            "++exactMatchCount",
        ),
        "Compatibility matching must check blocked state, exact game version, "
        "and exact BepInEx version before recording readiness",
    )
    require(
        '"upstream_branch_blocked"' in implementation,
        "Blocked branch reason must be stable",
    )
    require(
        '"exact_game_version_not_evidence_backed"' in implementation,
        "Version drift reason must be stable",
    )
    require(
        '"branch_runtime_pair_not_in_canonical_knowledge"' in implementation,
        "Unknown branch/runtime pairs must fail closed",
    )

    for profile_field in (
        "profile.m_gameVersion",
        "profile.m_branch",
        "profile.m_runtimeTarget",
        "profile.m_bepInExVersion",
    ):
        require(
            profile_field in implementation,
            f"Sanitized ProfileView binding missing {profile_field}",
        )

    require(
        'surface.m_status == "candidate"' in implementation,
        "Consumer readiness must require a candidate API-surface status",
    )
    require(
        'surface.m_consumerBinding != "disabled"' in implementation,
        "Consumer readiness must reject disabled bindings",
    )
    require(
        '"surface_status_blocked"' in implementation,
        "Blocked API surfaces need a stable fail-closed reason",
    )

    for false_flag in (
        "plan.m_runtimeInvocationAllowed = false",
        "plan.m_fileWriteAllowed = false",
        "plan.m_catalogMutationAllowed = false",
    ):
        require(
            false_flag in implementation,
            f"Permanent no-authority flag missing: {false_flag}",
        )
    require(
        "&& hasConsumerReadySurface" in implementation,
        "Evidence eligibility must also require one approved consumer-ready surface",
    )
    require(
        "m_candidateEvidenceSubmissionAllowed" not in header + implementation,
        "Editor service must not claim evidence submission authority",
    )

    require(
        "TaintedFrameworkEditorServices::Service "
        "m_taintedFrameworkEditorServices" in foundation_header,
        "Foundation must own one persistent editor-service instance",
    )
    require(
        "GetTaintedFrameworkEditorServices" in foundation_header,
        "Foundation editor-service accessor is missing",
    )
    require(
        "return m_taintedFrameworkEditorServices" in bridge,
        "Foundation accessor must return the persistent service",
    )

    for required in (
        "Source/TaintedFrameworkEditorServices.cpp",
        "Source/TaintedFrameworkEditorServices.h",
        "Source/FoundationTaintedFrameworkEditorServices.cpp",
    ):
        require(
            required in framework_manifest,
            f"Framework build graph missing {required}",
        )
    require(
        "Tests/TaintedFrameworkEditorServicesTests.cpp" in test_manifest,
        "Production-linked editor-service tests are not compiled",
    )

    for test_name in (
        "ExactMonoEvidenceIsReady",
        "Il2CppRemainsBlocked",
        "VersionDriftIsUnsupported",
        "BepInExVersionDriftIsUnsupported",
        "OnlyRuntimeReportIsConsumerReady",
        "BlockedSurfaceHasStableReason",
        "ActivationPlanIsReadOnlyAndDeterministic",
        "BlockedPlanIsNotEvidenceEligible",
        "SanitizedProfileBindingIsExact",
        "SanitizedProfileBepInExDriftFailsClosed",
        "FoundationOwnsPersistentService",
    ):
        require(
            test_name in compiled_tests,
            f"Compiled service coverage missing {test_name}",
        )

    require(isinstance(fixtures, dict), "Golden fixtures must be a JSON object")
    require(
        fixtures.get("branch_compatibility", {}).get("Mono")
        == "supported_exact_1.23.401",
        "Golden fixture must preserve exact Mono game compatibility",
    )
    require(
        fixtures.get("branch_compatibility", {}).get("IL2CPP") == "blocked",
        "Golden fixture must preserve IL2CPP blocking",
    )
    require(
        fixtures.get("configuration")
        == [
            ["General.Enabled", "true"],
            ["Safety.DryRunOnly", "true"],
            ["Safety.ReportOnlyMode", "true"],
        ],
        "Editor-service configuration projection drifted from the golden fixture",
    )
    require(
        fixtures.get("diagnostics")
        == [
            "contract-manifest",
            "host-safety",
            "plugin-registration",
            "runtime-compatibility",
            "runtime-fingerprint",
            "runtime-readiness",
            "runtime-report",
            "service-activation",
            "service-registry",
        ],
        "Editor-service diagnostic projection drifted from the golden fixture",
    )

    require(
        "BepInEx `5.4.23.3`" in service_document,
        "Public service documentation must record the exact ready BepInEx version",
    )
    require(
        "sanitized `ExtensionAPI::ProfileView`" in service_document,
        "Public service documentation must describe the safe profile binding",
    )
    require(
        "acquisition-provider layer" in service_document,
        "Public service documentation must point to the governed next unit",
    )
    require(
        "acquisition-provider layer" in review_document,
        "Review gate must keep acquisition work behind service acceptance",
    )

    require(
        "validate_tainted_framework_editor_services.py" in local_runner,
        "Authoritative local validation does not include the editor-service gate",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate(repo_root)
    except (OSError, TaintedFrameworkEditorServicesError) as exc:
        print(
            f"Tainted Framework editor-service validation failed: {exc}",
            file=sys.stderr,
        )
        return 1
    print(
        "Tainted Framework editor-service validation passed: exact game, branch, "
        "runtime, and BepInEx compatibility; sanitized profile binding; "
        "deterministic golden projections; governed evidence eligibility; "
        "persistent Foundation ownership; production-linked tests; and no "
        "runtime/file/catalog authority."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
