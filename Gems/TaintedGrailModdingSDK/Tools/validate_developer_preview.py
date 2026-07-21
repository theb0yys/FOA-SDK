#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the Developer Preview 0 command-layer contract."""

from __future__ import annotations

import sys
from pathlib import Path


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


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    tools_root = repo_root / "Gems/TaintedGrailModdingSDK/Tools"
    script_path = tools_root / "developer_preview.py"
    tests_path = tools_root / "tests/test_developer_preview.py"
    guide_path = repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_0.md"
    workflow_path = repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml"

    try:
        script = require_fragments(
            script_path,
            (
                'PRIMARY_HOST = "Windows x64 Profile"',
                'DEFAULT_CONFIGURE_PRESET = "windows-vs-unity"',
                'DEFAULT_CONFIGURATION = "profile"',
                'PREVIEW_PROJECT_DIRECTORY = "TaintedGrailModdingEditor"',
                'EDITOR_TARGET = "Editor"',
                'CATALOG_TEST_TARGET = "TaintedGrailModdingSDK.Catalog.Tests"',
                "def collect_prerequisite_checks",
                '"git-lfs"',
                '"visual-studio-cpp"',
                "validate_repository_root",
                "validate_build_directory",
                "CMAKE_HOME_DIRECTORY:INTERNAL",
                "--dry-run",
                "def configure_command",
                'f"-DLY_PROJECTS={repo_root / PREVIEW_PROJECT_DIRECTORY}"',
                "def build_command",
                "def validation_plan",
                "def execute_plan",
                "atomic_write_json",
                '"schema_version": PREVIEW_SCHEMA_VERSION',
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
                "test_repository_root_requires_engine_presets_and_gem",
                "test_build_directory_rejects_repository_root_and_unrelated_content",
                "test_build_directory_rejects_cache_for_another_source_tree",
                "test_configure_command_uses_approved_windows_x64_preset",
                "test_build_command_has_fixed_profile_editor_asset_and_catalog_targets",
                "test_validation_plan_is_deterministic_and_ends_with_compiled_tests",
                "test_dry_run_never_invokes_executor",
                "test_execution_stops_and_propagates_original_exit_code",
                "test_atomic_json_result_is_machine_readable",
                "test_unsupported_host_fails_closed",
            ),
        )

        require_fragments(
            guide_path,
            (
                "Developer Preview 0",
                "Windows x64 Profile",
                "developer_preview.py prerequisites",
                "developer_preview.py configure",
                "developer_preview.py build",
                "developer_preview.py validate",
                "not a standalone installer",
                "does not launch FoA",
                "tg-sdk-developer-preview-validation.json",
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

    print("Developer Preview 0 prerequisite/configure/build/validate command contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
