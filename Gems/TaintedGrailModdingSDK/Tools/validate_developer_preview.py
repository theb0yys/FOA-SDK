#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the Developer Preview command layer and exact-head coordinator contract."""

from __future__ import annotations

import sys
from pathlib import Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_file(path: Path) -> str:
    if not path.is_file():
        fail(f"Required Developer Preview file is missing: {path}")
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


def reject_fragments(text: str, fragments: tuple[str, ...], path: Path) -> None:
    for fragment in fragments:
        if fragment in text:
            fail(f"Developer Preview tooling contains unsafe behavior {fragment!r}: {path}")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    tools_root = repo_root / "Gems/TaintedGrailModdingSDK/Tools"
    script_path = tools_root / "developer_preview.py"
    tests_path = tools_root / "tests/test_developer_preview.py"
    coordinator_path = tools_root / "developer_preview_verification.py"
    coordinator_tests_path = tools_root / "tests/test_developer_preview_verification.py"
    guide_path = repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_0.md"
    verification_guide_path = (
        repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_EXACT_HEAD_VERIFICATION.md"
    )
    documentation_hub_path = repo_root / "docs/tainted-grail-sdk/README.md"
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
        reject_fragments(
            script,
            (
                "shell=True",
                "os.system(",
                "Popen(",
                'subprocess.run(["FoA',
                'subprocess.run(("FoA',
            ),
            script_path,
        )
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
                "test_build_command_has_fixed_profile_editor_and_catalog_targets",
                "test_validation_plan_is_deterministic_and_ends_with_compiled_tests",
                "test_dry_run_never_invokes_executor",
                "test_execution_stops_and_propagates_original_exit_code",
                "test_atomic_json_result_is_machine_readable",
                "test_unsupported_host_fails_closed",
            ),
        )

        coordinator = require_fragments(
            coordinator_path,
            (
                'DEFAULT_BASE_REF = "origin/main"',
                'DEFAULT_SYNC_REFS = ("origin/main", "origin/foa-development")',
                "def resolve_review_ancestry(",
                "merge-base",
                "--is-ancestor",
                "def prerequisites_step(",
                "prerequisites_path",
                "Prerequisite output must not make an unconfigured O3DE build directory non-empty.",
                "def automated_gate_commands(",
                '"git-diff-check": ("git", "diff", "--check", base_commit, "HEAD")',
                "def probe_receipt(",
                "def probe_ui_evidence(",
                "validation_receipt.py",
                "developer_preview_ui_evidence.py",
                "--require-merge-ready",
                "Authoritative receipt verification failed before execution",
                "receipt_finalized",
                "verify-ui-evidence",
                "replace the invalid or tampered finalized receipt",
                "The authoritative validators remain",
            ),
        )
        reject_fragments(
            coordinator,
            (
                "shell=True",
                "os.system(",
                "subprocess.Popen(",
                "pyautogui",
                "ImageGrab",
                "win32gui",
                "SendKeys",
                'return "complete" if receipt.get("finalized_at_utc")',
            ),
            coordinator_path,
        )
        require_order(
            coordinator,
            (
                '"git-diff-check"',
                '"local-validation"',
                '"o3de-configure"',
                '"o3de-build"',
                '"compiled-tests"',
            ),
            coordinator_path,
        )

        require_fragments(
            coordinator_tests_path,
            (
                "test_prerequisite_output_does_not_poison_build_directory",
                "test_prerequisite_output_inside_build_directory_is_rejected",
                "test_committed_diff_gate_uses_explicit_base_commit",
                "test_sync_ref_must_be_ancestor_of_head",
                "test_failed_receipt_probe_prevents_complete_status",
                "test_complete_requires_authoritative_receipt_and_ui_verifiers",
                "test_finalized_receipt_is_reverified_instead_of_no_op",
                "test_execution_stops_on_failure_and_dry_run_executes_nothing",
                "test_atomic_state_write_is_machine_readable",
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
            verification_guide_path,
            (
                "Developer Preview Exact-Head Verification",
                "git fetch --prune origin",
                "origin/main",
                "origin/foa-development",
                "committed review range",
                "prerequisite result",
                "authoritative receipt verifier",
                "authoritative Windows UI evidence verifier",
                "already-finalized receipt",
                "All twenty-two TG SDK panes",
            ),
        )
        documentation_hub = require_file(documentation_hub_path)
        if "DEVELOPER_PREVIEW_EXACT_HEAD_VERIFICATION.md" not in documentation_hub:
            fail("The documentation hub does not link the exact-head verification runbook.")

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
        print(
            f"Developer Preview command and exact-head coordinator validation failed: {exc}",
            file=sys.stderr,
        )
        return 1

    print(
        "Developer Preview prerequisite/configure/build/validate and exact-head "
        "verification coordinator contracts passed."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
