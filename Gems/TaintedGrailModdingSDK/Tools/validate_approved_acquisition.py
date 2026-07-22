#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the optional local and exact pinned-GitHub acquisition package."""

from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
PACKAGE_REL = Path("Plugins/Integrations/ApprovedAcquisition")
MODULE_REL = PACKAGE_REL / "Tools/approved_acquisition.py"
MANIFEST_REL = PACKAGE_REL / "approved-sources.json"
PLUGIN_REL = PACKAGE_REL / "plugin.json"
GOLDEN_REL = PACKAGE_REL / "Tests/Fixtures/pinned-github-all.plan.json"
BRIDGE_REL = Path("Gems/TaintedGrailModdingSDK/Tools/tests/test_approved_acquisition.py")
HARDENING_BRIDGE_REL = Path(
    "Gems/TaintedGrailModdingSDK/Tools/tests/test_approved_acquisition_hardening.py"
)
PINNED_COMMIT = "7d3d31371b4327ee3abbaa48b8741b4390bb9e0e"
REQUIRED_PACKAGE_FILES = {
    "README.md",
    "approved-sources.json",
    "plugin.json",
    "Seeds/Assets/action.svg",
    "Seeds/Assets/device.svg",
    "Seeds/Assets/frame.svg",
    "Tests/Fixtures/pinned-github-all.plan.json",
    "Tests/test_approved_acquisition.py",
    "Tests/test_approved_acquisition_hardening.py",
    "Tools/approved_acquisition.py",
}
FORBIDDEN_SUFFIXES = {
    ".dll",
    ".exe",
    ".fbx",
    ".jpg",
    ".meta",
    ".png",
    ".prefab",
    ".tga",
    ".wav",
}
REQUIRED_SOURCE = (
    'GITHUB_RAW_HOST = "raw.githubusercontent.com"',
    "class NoRedirectHandler",
    "Pinned-GitHub acquisition refuses redirects",
    "validate_external_output_path",
    "Acquisition output must remain outside the FOA-SDK checkout",
    "contained_regular_file",
    "path_is_blocked",
    "prohibited by blocked_sources",
    "os.open",
    'path.open("xb")',
    "O_NOFOLLOW",
    "os.fsync",
    "os.replace(temporary, destination)",
    '"candidate_evidence_created": False',
    'provider not in {"local", "pinned-github"}',
    "approved.byte_count + 1",
    "sha256_hex(payload) != approved.sha256",
)
FORBIDDEN_SOURCE = (
    "subprocess",
    "shell=True",
    "os.system",
    "Authorization",
    "GITHUB_TOKEN",
    "password",
    "BepInEx",
    "UnityEngine",
    "Harmony",
    "MerlinsWorkshop",
    "Tainted.Host.Mono",
    "Tainted.Host.IL2CPP",
)


class ApprovedAcquisitionValidationError(RuntimeError):
    """Raised when the reviewed acquisition package drifts."""


def read_bytes(root: Path, relative: Path) -> bytes:
    try:
        return (root / relative).read_bytes()
    except OSError as exc:
        raise ApprovedAcquisitionValidationError(
            f"Unable to read {relative.as_posix()}."
        ) from exc


def read_text(root: Path, relative: Path) -> str:
    try:
        return (root / relative).read_text(encoding="utf-8", errors="strict")
    except (OSError, UnicodeDecodeError) as exc:
        raise ApprovedAcquisitionValidationError(
            f"Unable to read {relative.as_posix()}."
        ) from exc


def load_provider(root: Path):
    path = root / MODULE_REL
    spec = importlib.util.spec_from_file_location(
        "approved_acquisition_validation_target", path
    )
    if spec is None or spec.loader is None:
        raise ApprovedAcquisitionValidationError(
            "Unable to load acquisition provider."
        )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def validate_package_tree(root: Path) -> None:
    package = root / PACKAGE_REL
    actual = {
        path.relative_to(package).as_posix()
        for path in package.rglob("*")
        if path.is_file()
    }
    missing = sorted(REQUIRED_PACKAGE_FILES - actual)
    if missing:
        raise ApprovedAcquisitionValidationError(
            "Approved acquisition package files are missing: " + ", ".join(missing)
        )
    for relative in sorted(actual):
        path = package / relative
        if path.is_symlink():
            raise ApprovedAcquisitionValidationError(
                f"Tracked package path is a symlink: {relative}"
            )
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            raise ApprovedAcquisitionValidationError(
                f"Unapproved payload entered the package: {relative}"
            )
        if path.stat().st_size > 2 * 1024 * 1024:
            raise ApprovedAcquisitionValidationError(
                f"Tracked package file is unexpectedly large: {relative}"
            )


def validate_plugin(root: Path) -> None:
    try:
        manifest = json.loads(read_text(root, PLUGIN_REL))
    except json.JSONDecodeError as exc:
        raise ApprovedAcquisitionValidationError("plugin.json is malformed.") from exc
    expected = {
        "capabilities",
        "category",
        "compatibility",
        "dependencies",
        "entry_points",
        "id",
        "name",
        "optional",
        "provenance",
        "schema_version",
        "status",
        "version",
    }
    if not isinstance(manifest, dict) or set(manifest) != expected:
        raise ApprovedAcquisitionValidationError(
            "plugin.json differs from the governed schema surface."
        )
    checks = (
        manifest.get("schema_version") == 1,
        manifest.get("id") == "extension.approved-acquisition",
        manifest.get("category") == "integration",
        manifest.get("optional") is True,
        manifest.get("entry_points")
        == {
            "python_modules": ["Tools/approved_acquisition.py"],
            "tools": ["Tools/approved_acquisition.py"],
        },
        manifest.get("capabilities")
        == [
            "acquisition.bundle.verify",
            "acquisition.github.pinned.read",
            "acquisition.local.read",
            "acquisition.plan",
        ],
        manifest.get("dependencies") == [],
        manifest.get("compatibility")
        == {
            "game_versions": ["1.23.401"],
            "branches": ["Mono"],
            "runtime_targets": ["editor-only"],
        },
        isinstance(manifest.get("provenance"), dict),
        manifest.get("provenance", {}).get("license") == "Apache-2.0 OR MIT",
        manifest.get("provenance", {}).get("redistribution_reviewed") is True,
    )
    if not all(checks):
        raise ApprovedAcquisitionValidationError(
            "plugin.json identity, authority, or provenance drifted."
        )


def validate_manifest_and_golden(root: Path) -> None:
    module = load_provider(root)
    try:
        manifest = module.load_manifest(root / MANIFEST_REL)
    except Exception as exc:
        raise ApprovedAcquisitionValidationError(
            f"Approved source manifest is invalid: {exc}"
        ) from exc
    if manifest.repository != "theb0yys/FOA-SDK" or manifest.commit != PINNED_COMMIT:
        raise ApprovedAcquisitionValidationError(
            "Pinned-GitHub source identity drifted."
        )
    for approved in manifest.files:
        try:
            module.validate_payload(
                read_bytes(root, Path(approved.source_path)), approved
            )
        except Exception as exc:
            raise ApprovedAcquisitionValidationError(
                f"Approved source bytes drifted: {approved.source_path}"
            ) from exc
    generated = module.canonical_json_bytes(
        module.build_plan(manifest, provider="pinned-github")
    )
    if generated != read_bytes(root, GOLDEN_REL):
        raise ApprovedAcquisitionValidationError(
            "Pinned-GitHub golden plan is stale or non-deterministic."
        )


def validate_provider(root: Path) -> None:
    source = read_text(root, MODULE_REL)
    for fragment in REQUIRED_SOURCE:
        if fragment not in source:
            raise ApprovedAcquisitionValidationError(
                f"Provider is missing required fail-closed behavior: {fragment}"
            )
    for fragment in FORBIDDEN_SOURCE:
        if fragment in source:
            raise ApprovedAcquisitionValidationError(
                f"Provider contains forbidden authority or dependency: {fragment}"
            )
    for fragment in ("/main/", "/master/", "refs/heads/", "api.github.com/repos/"):
        if fragment in source:
            raise ApprovedAcquisitionValidationError(
                f"Provider contains an unpinned GitHub route: {fragment}"
            )


def validate_tests_and_docs(root: Path) -> None:
    bridge = read_text(root, BRIDGE_REL)
    for fragment in (
        "ApprovedAcquisition/Tests/test_approved_acquisition.py",
        "load_tests",
        "ApprovedAcquisitionTests",
    ):
        if fragment not in bridge:
            raise ApprovedAcquisitionValidationError(
                f"Mandatory test bridge is missing {fragment!r}."
            )
    hardening_bridge = read_text(root, HARDENING_BRIDGE_REL)
    for fragment in (
        "test_approved_acquisition_hardening.py",
        "ApprovedAcquisitionHardeningTests",
    ):
        if fragment not in hardening_bridge:
            raise ApprovedAcquisitionValidationError(
                f"Hardening test bridge is missing {fragment!r}."
            )
    tests = read_text(root, PACKAGE_REL / "Tests/test_approved_acquisition.py")
    for fragment in (
        "test_local_acquisition_is_atomic_and_verifiable",
        "test_pinned_github_route_uses_injected_verified_bytes",
        "test_local_symlink_source_fails",
        "test_tampered_bundle_fails",
        "test_production_manifest_matches_golden_plan_and_seed_hashes",
    ):
        if fragment not in tests:
            raise ApprovedAcquisitionValidationError(
                f"Provider tests are missing {fragment}."
            )
    hardening_tests = read_text(
        root, PACKAGE_REL / "Tests/test_approved_acquisition_hardening.py"
    )
    for fragment in (
        "test_blocked_source_pattern_is_enforced_by_provider",
        "test_blocked_bundle_pattern_is_enforced_by_provider",
        "test_local_source_root_with_symlinked_ancestor_fails",
    ):
        if fragment not in hardening_tests:
            raise ApprovedAcquisitionValidationError(
                f"Provider hardening tests are missing {fragment}."
            )
    public = read_text(
        root, Path("docs/tainted-grail-sdk/APPROVED_ACQUISITION_PROVIDERS.md")
    )
    for fragment in (
        "local",
        "pinned-GitHub",
        PINNED_COMMIT,
        "NO upstream PNG",
        "outside the FOA-SDK checkout",
        "candidate evidence",
        "Merlin",
        "Mono",
        "IL2CPP",
    ):
        if fragment not in public:
            raise ApprovedAcquisitionValidationError(
                f"Public acquisition contract is missing {fragment!r}."
            )
    if "ApprovedAcquisition" not in read_text(
        root, Path("Plugins/Integrations/README.md")
    ):
        raise ApprovedAcquisitionValidationError(
            "Integration package index does not list ApprovedAcquisition."
        )


def validate_approved_acquisition(repo_root: Path = REPO_ROOT) -> None:
    validate_package_tree(repo_root)
    validate_plugin(repo_root)
    validate_manifest_and_golden(repo_root)
    validate_provider(repo_root)
    validate_tests_and_docs(repo_root)


def main() -> int:
    try:
        validate_approved_acquisition()
    except ApprovedAcquisitionValidationError as exc:
        print(f"Approved acquisition validation failed: {exc}", file=sys.stderr)
        return 1
    print("Approved local and pinned-GitHub acquisition validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
