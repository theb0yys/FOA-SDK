#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Validate the separate project-owned FoA Mono runtime-adapter package."""

from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
PACKAGE_REL = Path("Plugins/RuntimeAdapters/Mono")
MODULE_REL = PACKAGE_REL / "Tools/mono_runtime_adapter.py"
MANIFEST_REL = PACKAGE_REL / "mono-adapter.json"
PLUGIN_REL = PACKAGE_REL / "plugin.json"
PROJECT_REL = PACKAGE_REL / "Source/FOA.SDK.RuntimeAdapter.Mono.csproj"
SOURCE_REL = PACKAGE_REL / "Source/MonoRuntimeAdapterPlugin.cs"
PLAN_REL = PACKAGE_REL / "Tests/Fixtures/build-plan.json"
RESULT_REL = PACKAGE_REL / "Tests/Fixtures/not-attempted-result.json"
PACKAGE_TEST_REL = PACKAGE_REL / "Tests/test_mono_runtime_adapter.py"
BRIDGE_REL = Path("Gems/TaintedGrailModdingSDK/Tools/tests/test_mono_runtime_adapter.py")
ROUTES_REL = Path("Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp")
DOC_REL = Path("docs/tainted-grail-sdk/MONO_RUNTIME_ADAPTER.md")
INDEX_REL = Path("Plugins/RuntimeAdapters/README.md")
RUNTIME_INDEX_REL = Path("docs/tainted-grail-modding/runtime/README.md")

REQUIRED_PACKAGE_FILES = {
    "README.md",
    "Schemas/execution-gate.schema.json",
    "Schemas/runtime-result.schema.json",
    "Source/FOA.SDK.RuntimeAdapter.Mono.csproj",
    "Source/MonoRuntimeAdapterPlugin.cs",
    "Tests/Fixtures/build-plan.json",
    "Tests/Fixtures/not-attempted-result.json",
    "Tests/test_mono_runtime_adapter.py",
    "Tools/_mono_contract.py",
    "Tools/_mono_gate.py",
    "Tools/_mono_result.py",
    "Tools/mono_runtime_adapter.py",
    "mono-adapter.json",
    "plugin.json",
}
FORBIDDEN_SUFFIXES = {
    ".7z", ".dll", ".exe", ".pdb", ".so", ".dylib", ".zip", ".nupkg",
}
REQUIRED_TESTS = {
    "test_build_plan_is_byte_stable_and_matches_golden",
    "test_exact_gate_is_eligible_but_grants_no_authority",
    "test_il2cpp_profile_is_refused",
    "test_missing_or_overlapping_evidence_is_refused",
    "test_not_attempted_fixture_is_canonical",
    "test_succeeded_result_requires_all_startup_observations",
    "test_unsafe_log_reference_and_impossible_chronology_fail",
    "test_tool_has_no_executor_or_network_surface",
}


class MonoRuntimeAdapterValidationError(RuntimeError):
    """Raised when the reviewed Mono runtime-adapter package drifts."""


def read_text(root: Path, relative: Path) -> str:
    try:
        return (root / relative).read_text(encoding="utf-8", errors="strict")
    except (OSError, UnicodeDecodeError) as exc:
        raise MonoRuntimeAdapterValidationError(
            f"Unable to read {relative.as_posix()}."
        ) from exc


def load_json(root: Path, relative: Path) -> dict[str, object]:
    try:
        value = json.loads(read_text(root, relative))
    except json.JSONDecodeError as exc:
        raise MonoRuntimeAdapterValidationError(
            f"Malformed JSON: {relative.as_posix()}."
        ) from exc
    if not isinstance(value, dict):
        raise MonoRuntimeAdapterValidationError(
            f"Expected a JSON object: {relative.as_posix()}."
        )
    return value


def load_provider(root: Path):
    path = root / MODULE_REL
    spec = importlib.util.spec_from_file_location(
        "mono_runtime_adapter_validation_target", path
    )
    if spec is None or spec.loader is None:
        raise MonoRuntimeAdapterValidationError("Unable to load Mono adapter provider.")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    try:
        spec.loader.exec_module(module)
    except Exception as exc:
        raise MonoRuntimeAdapterValidationError(
            f"Unable to load Mono adapter provider: {exc}"
        ) from exc
    return module


def validate_package_tree(root: Path) -> None:
    package = root / PACKAGE_REL
    actual = {
        path.relative_to(package).as_posix()
        for path in package.rglob("*")
        if path.is_file()
        and "__pycache__" not in path.parts
        and path.suffix != ".pyc"
    }
    if actual != REQUIRED_PACKAGE_FILES:
        missing = sorted(REQUIRED_PACKAGE_FILES - actual)
        extra = sorted(actual - REQUIRED_PACKAGE_FILES)
        raise MonoRuntimeAdapterValidationError(
            "Mono package file set drifted; missing="
            + ",".join(missing)
            + "; extra="
            + ",".join(extra)
        )
    for relative in sorted(actual):
        path = package / relative
        if path.is_symlink():
            raise MonoRuntimeAdapterValidationError(
                f"Tracked Mono package path is a symlink: {relative}."
            )
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            raise MonoRuntimeAdapterValidationError(
                f"Generated or runtime binary entered FOA-SDK: {relative}."
            )
        if path.stat().st_size > 2 * 1024 * 1024:
            raise MonoRuntimeAdapterValidationError(
                f"Mono package source is unexpectedly large: {relative}."
            )


def validate_plugin(root: Path) -> None:
    plugin = load_json(root, PLUGIN_REL)
    expected = {
        "capabilities", "category", "compatibility", "dependencies",
        "entry_points", "id", "name", "optional", "provenance",
        "schema_version", "status", "version",
    }
    if set(plugin) != expected:
        raise MonoRuntimeAdapterValidationError("Mono plugin.json field set drifted.")
    if plugin != {
        "capabilities": [
            "runtime.mono.build-plan",
            "runtime.mono.execution-gate",
            "runtime.mono.package-verify",
            "runtime.mono.result-verify",
        ],
        "category": "runtime-adapter",
        "compatibility": {
            "branches": ["mono"],
            "game_versions": ["1.23.401"],
            "runtime_targets": ["mono"],
        },
        "dependencies": [],
        "entry_points": {
            "python_modules": ["Tools/mono_runtime_adapter.py"],
            "tools": ["Tools/mono_runtime_adapter.py"],
        },
        "id": "extension.foa-mono-runtime-adapter",
        "name": "FOA Mono Runtime Adapter",
        "optional": True,
        "provenance": {
            "license": "Apache-2.0 OR MIT",
            "origin": "theb0yys/FOA-SDK",
            "redistribution_reviewed": True,
            "revision": "project-owned",
        },
        "schema_version": 1,
        "status": "experimental",
        "version": "0.1.0",
    }:
        raise MonoRuntimeAdapterValidationError(
            "Mono plug-in identity, compatibility, or provenance drifted."
        )


def validate_provider_contract(root: Path) -> None:
    provider = load_provider(root)
    try:
        manifest = provider.validate_package(root / PACKAGE_REL)
        plan = provider.validate_plan(load_json(root, PLAN_REL), root / PACKAGE_REL)
        result = load_json(root, RESULT_REL)
        gate = result.get("gate")
        if not isinstance(gate, dict):
            raise MonoRuntimeAdapterValidationError(
                "The not-attempted fixture must embed its exact gate."
            )
        provider.validate_result(result["result"], gate, root / PACKAGE_REL)
    except (KeyError, TypeError, provider.MonoAdapterError) as exc:
        raise MonoRuntimeAdapterValidationError(
            f"Mono provider contract validation failed: {exc}"
        ) from exc
    if manifest.get("adapter_id") != "adapter.foa.mono":
        raise MonoRuntimeAdapterValidationError("Mono manifest adapter ID drifted.")
    if plan.get("plan_kind") != "foa-mono-adapter-build-plan":
        raise MonoRuntimeAdapterValidationError("Mono build-plan kind drifted.")


def validate_source_boundary(root: Path) -> None:
    project = read_text(root, PROJECT_REL)
    source = read_text(root, SOURCE_REL)
    tool = "\n".join(
        read_text(root, PACKAGE_REL / relative)
        for relative in (
            "Tools/_mono_contract.py",
            "Tools/_mono_gate.py",
    "Tools/_mono_result.py",
            "Tools/mono_runtime_adapter.py",
        )
    )
    required_project = (
        "<TargetFramework>netstandard2.1</TargetFramework>",
        "<Deterministic>true</Deterministic>",
        "<ContinuousIntegrationBuild>true</ContinuousIntegrationBuild>",
        "$(FoAGameRoot)",
        "BepInEx.dll",
        "UnityEngine.CoreModule.dll",
        "<Private>false</Private>",
    )
    if any(value not in project for value in required_project):
        raise MonoRuntimeAdapterValidationError(
            "Mono project lost deterministic or local-reference requirements."
        )
    required_source = (
        '[BepInPlugin(PluginGuid, PluginName, PluginVersion)]',
        'PluginGuid = "foa.sdk.runtime-adapter.mono"',
        'PluginVersion = "0.1.0"',
        '[BepInDependency(FrameworkGuid, FrameworkVersion)]',
        '"foa-sdk.mono-adapter.ready"',
        'Logger.LogInfo',
    )
    if any(value not in source for value in required_source):
        raise MonoRuntimeAdapterValidationError(
            "Mono BepInEx source identity or startup marker drifted."
        )
    forbidden_source = (
        "HarmonyPatch", "Harmony(", "File.", "Directory.", "Process.Start",
        "System.Net", "HttpClient", "Socket", "Il2Cpp", "IL2CPP",
    )
    if any(value in source for value in forbidden_source):
        raise MonoRuntimeAdapterValidationError(
            "Mono source gained mutation, filesystem, network, process, or IL2CPP scope."
        )
    forbidden_tool = (
        "import requests", "import socket", "import subprocess",
        "import urllib.request", "os.system", "shell=True", "Popen(",
    )
    if any(value in tool for value in forbidden_tool):
        raise MonoRuntimeAdapterValidationError(
            "Mono package gained an executor or network client."
        )
    for marker in (
        'AUTHORITY_FIELDS = (',
        'def authority() -> dict[str, bool]:',
        'return {field: False for field in AUTHORITY_FIELDS}',
        '"execution_allowed": False',
        '"candidate_evidence_promoted": False',
    ):
        if marker not in tool:
            raise MonoRuntimeAdapterValidationError(
                f"Mono authority boundary marker is missing: {marker}"
            )


def validate_route_separation(root: Path) -> None:
    routes = read_text(root, ROUTES_REL)
    mono_required = (
        'mono.m_adapterId = "adapter.foa.mono";',
        'mono.m_frameworkVersion = "0.1.33";',
        'mono.m_gameVersion = "1.23.401";',
        'mono.m_branch = "mono";',
        'mono.m_runtimeTarget = "Mono";',
        'mono.m_unityVersion = "6000.0.64f1";',
        'mono.m_bepInExVersion = "5.4.23.3";',
        'mono.m_evidenceState = EvidenceState::HostLiveLoadValidated;',
    )
    il2cpp_required = (
        'il2Cpp.m_adapterId = "adapter.foa.il2cpp";',
        'il2Cpp.m_branch = "il2cpp";',
        'il2Cpp.m_runtimeTarget = "IL2CPP";',
        'il2Cpp.m_bepInExVersion = "6.0.0-be.735";',
    )
    if any(value not in routes for value in mono_required):
        raise MonoRuntimeAdapterValidationError(
            "Canonical Mono route no longer matches the package profile."
        )
    if any(value not in routes for value in il2cpp_required):
        raise MonoRuntimeAdapterValidationError(
            "Mono work weakened the separate IL2CPP route boundary."
        )


def validate_tests_and_docs(root: Path) -> None:
    tests = read_text(root, PACKAGE_TEST_REL)
    bridge = read_text(root, BRIDGE_REL)
    docs = read_text(root, DOC_REL)
    index = read_text(root, INDEX_REL)
    runtime_index = read_text(root, RUNTIME_INDEX_REL)
    missing_tests = sorted(name for name in REQUIRED_TESTS if f"def {name}(" not in tests)
    if missing_tests:
        raise MonoRuntimeAdapterValidationError(
            "Mono package test coverage drifted: " + ",".join(missing_tests)
        )
    if "MonoRuntimeAdapterTests = MODULE.MonoRuntimeAdapterTests" not in bridge:
        raise MonoRuntimeAdapterValidationError(
            "Mono tests are not exposed through mandatory Gem discovery."
        )
    for phrase in (
        "Mono/IL2CPP separation",
        "No executor is included",
        "HostLiveLoadValidated",
        "Candidate evidence remains unpromoted",
        "BepInEx: 5.4.23.3",
    ):
        if phrase not in docs:
            raise MonoRuntimeAdapterValidationError(
                f"Mono public contract is missing: {phrase}"
            )
    if "Mono" not in index or "IL2CPP" not in index or "separate" not in index:
        raise MonoRuntimeAdapterValidationError(
            "Runtime-adapter index lost the route separation statement."
        )
    if "MONO_RUNTIME_ADAPTER.md" not in runtime_index or "IL2CPP remains a separate later package" not in runtime_index:
        raise MonoRuntimeAdapterValidationError(
            "Runtime handbook did not register the Mono package or preserve IL2CPP separation."
        )


def validate_mono_runtime_adapter(root: Path = REPO_ROOT) -> None:
    validate_package_tree(root)
    validate_plugin(root)
    validate_source_boundary(root)
    validate_provider_contract(root)
    validate_route_separation(root)
    validate_tests_and_docs(root)


def main() -> int:
    try:
        validate_mono_runtime_adapter(REPO_ROOT)
    except MonoRuntimeAdapterValidationError as exc:
        print(f"Mono runtime-adapter validation failed: {exc}", file=sys.stderr)
        return 1
    print("Mono runtime-adapter validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
