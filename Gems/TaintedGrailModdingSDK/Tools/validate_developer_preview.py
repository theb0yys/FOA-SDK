#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the Developer Preview 0 external-engine command contract."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


PINNED_O3DE_COMMIT = "68683f23fb747380d3efa2424bd5f30242e9c5a2"
EXPECTED_PROJECT_EXTERNAL_SUBDIRECTORIES = (
    "../Gems/ExternalToolchain",
    "../Gems/TaintedGrailModdingSDK",
    "../Plugins/Authoring/AvalonAI/Gem",
    "../Plugins/Authoring/RoadAtlas/Gem",
)


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_file(path: Path) -> str:
    if not path.is_file():
        fail(f"Required Developer Preview 0 file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(path: Path, fragments: tuple[str, ...]) -> str:
    text = require_file(path)
    for fragment in fragments:
        if fragment not in text:
            fail(f"Missing required fragment {fragment!r} in {path}")
    return text


def require_order(text: str, fragments: tuple[str, ...], path: Path) -> None:
    position = -1
    for fragment in fragments:
        current = text.find(fragment, position + 1)
        if current < 0:
            fail(f"Missing ordered fragment {fragment!r} in {path}")
        if current <= position:
            fail(f"Out-of-order fragment {fragment!r} in {path}")
        position = current


def validate_engine_lock(path: Path) -> None:
    try:
        payload = json.loads(require_file(path))
    except json.JSONDecodeError as exc:
        fail(f"Invalid O3DE lock JSON in {path}: {exc}")

    expected = {
        "schema_version": 1,
        "repository": "https://github.com/o3de/o3de.git",
        "commit": PINNED_O3DE_COMMIT,
        "engine_name": "o3de",
        "engine_version": "2.7.0",
        "checkout_directory": "o3de",
    }
    if payload != expected:
        fail(f"O3DE lock does not match the reviewed exact dependency contract: {path}")
    if re.fullmatch(r"[0-9a-f]{40}", payload["commit"]) is None:
        fail("Pinned O3DE commit must be a full lowercase SHA.")


def validate_project_external_boundary(product_root: Path) -> None:
    project_path = product_root / "TaintedGrailModdingEditor/project.json"
    try:
        project = json.loads(require_file(project_path))
    except json.JSONDecodeError as exc:
        fail(f"Invalid Developer Preview project JSON in {project_path}: {exc}")
    if not isinstance(project, dict):
        fail(f"Developer Preview project manifest must be a JSON object: {project_path}")

    external_subdirectories = project.get("external_subdirectories")
    if external_subdirectories != list(EXPECTED_PROJECT_EXTERNAL_SUBDIRECTORIES):
        fail(
            "Developer Preview project must register the complete product-owned Gem "
            f"directories in deterministic order: {EXPECTED_PROJECT_EXTERNAL_SUBDIRECTORIES}"
        )

    gem_names = project.get("gem_names")
    if not isinstance(gem_names, list):
        fail(f"Developer Preview project must declare gem_names: {project_path}")
    for gem_name in (
        "ExternalToolchain",
        "TaintedGrailModdingSDK",
        "AvalonAIAuthoring",
        "RoadAtlasAuthoring",
    ):
        if gem_names.count(gem_name) != 1:
            fail(f"Developer Preview project must enable {gem_name!r} exactly once.")

    require_fragments(
        product_root / "TaintedGrailModdingEditor/cmake/EngineFinder.cmake",
        (
            "FOA_O3DE_ROOT",
            "o3de.lock.json",
            "checkout_directory",
            "tg_editor_product_root",
            "tg_editor_engine_root",
            "tg_editor_expected_engine_commit",
            "rev-parse HEAD",
            "External O3DE commit mismatch",
            "o3deConfigVersion.cmake",
            "place the checkout beside FOA-SDK",
        ),
    )


def main() -> int:
    product_root = Path(__file__).resolve().parents[3]
    tools_root = product_root / "Gems/TaintedGrailModdingSDK/Tools"
    script_path = tools_root / "developer_preview.py"
    tests_path = tools_root / "tests/test_developer_preview.py"
    guide_path = product_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_0.md"
    boundary_path = product_root / "docs/tainted-grail-sdk/EXTERNAL_O3DE_DEPENDENCY.md"
    workflow_path = product_root / ".github/workflows/tainted-grail-sdk-foundation.yml"
    lock_path = product_root / "o3de.lock.json"

    try:
        validate_engine_lock(lock_path)
        validate_project_external_boundary(product_root)
        script = require_fragments(
            script_path,
            (
                'PRIMARY_HOST = "Windows x64 Profile"',
                'DEFAULT_CONFIGURE_PRESET = "windows-vs-unity"',
                'DEFAULT_CONFIGURATION = "profile"',
                'PREVIEW_PROJECT_DIRECTORY = "TaintedGrailModdingEditor"',
                'ENGINE_LOCK_FILENAME = "o3de.lock.json"',
                'ENGINE_ROOT_ENVIRONMENT_VARIABLE = "FOA_O3DE_ROOT"',
                'BUILD_ROOT_ENVIRONMENT_VARIABLE = "FOA_BUILD_ROOT"',
                "def validate_product_root",
                "def load_engine_lock",
                "def validate_engine_root",
                "def validate_engine_pin",
                "def validate_build_directory",
                "CMAKE_HOME_DIRECTORY:INTERNAL",
                "def configure_command",
                'str(engine_root)',
                'f"-DLY_PROJECTS={product_root / PREVIEW_PROJECT_DIRECTORY}"',
                "def build_command",
                "def validation_plan",
                'engine_root / "scripts" / "commit_validation"',
                "def execute_plan",
                "atomic_write_json",
                '"product_root": str(product_root)',
                '"engine_root": str(engine_root)',
                '"engine_commit": lock.commit',
                '"build_root": str(build_dir)',
                "--product-root",
                "--engine-root",
                "--build-dir",
                "Developer Preview 0 command failed",
                "does not launch FoA, deploy files, modify saves",
            ),
        )
        for forbidden in (
            "shell=True",
            "os.system(",
            "Popen(",
            'subprocess.run(["FoA',
            'subprocess.run(("FoA',
        ):
            if forbidden in script:
                fail(f"Developer Preview command must not use unsafe or runtime behavior: {forbidden}")

        require_order(
            script,
            (
                '"developer-preview-command-tests"',
                '"developer-preview-contract"',
                '"foundation"',
                '"governance-hardening"',
                '"catalog-contract"',
                '"o3de-source-policy"',
                '"compiled-catalog-tests"',
            ),
            script_path,
        )

        require_fragments(
            tests_path,
            (
                "test_product_root_requires_lock_project_and_custom_gems",
                "test_engine_lock_requires_full_commit_and_expected_schema",
                "test_engine_root_requires_descriptor_presets_validator_and_identity",
                "test_default_engine_root_prefers_environment_then_sibling",
                "test_build_directory_rejects_product_engine_and_unrelated_content",
                "test_build_directory_rejects_cache_for_another_engine_tree",
                "test_engine_pin_fails_closed_for_wrong_checkout",
                "test_configure_command_uses_external_engine_and_product_project",
                "test_validation_plan_separates_product_and_engine_tools",
                "test_dry_run_never_invokes_executor",
                "test_execution_stops_and_propagates_original_exit_code",
                "test_atomic_json_result_is_machine_readable",
                "test_payload_records_all_three_roots_and_pinned_commit",
                "test_unsupported_host_fails_closed",
            ),
        )

        require_fragments(
            guide_path,
            (
                "Developer Preview 0",
                "Windows x64 Profile",
                "FOA-SDK",
                "o3de.lock.json",
                "FOA_O3DE_ROOT",
                "developer_preview.py prerequisites",
                "developer_preview.py configure",
                "developer_preview.py build",
                "developer_preview.py validate",
                "--engine-root",
                "not a standalone installer",
                "does not launch FoA",
                "tg-sdk-developer-preview-validation.json",
            ),
        )

        require_fragments(
            boundary_path,
            (
                "product_root",
                "engine_root",
                "build_root",
                PINNED_O3DE_COMMIT,
                "Product-owned Gem discovery",
                "history extraction",
                "FOA-O3DE",
            ),
        )

        require_fragments(
            workflow_path,
            (
                "Test Developer Preview 0 command layer",
                "Validate Developer Preview 0 command contract",
                "python -m unittest discover -s Gems/TaintedGrailModdingSDK/Tools/tests",
                "python Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview.py",
            ),
        )
    except (OSError, RuntimeError) as exc:
        print(f"Developer Preview 0 command contract validation failed: {exc}", file=sys.stderr)
        return 1

    print("Developer Preview 0 external O3DE dependency contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
