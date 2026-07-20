#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the Developer Preview 0 launch and diagnostics contract."""

from __future__ import annotations

import sys
from pathlib import Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_file(path: Path) -> str:
    if not path.is_file():
        fail(f"Required launch/diagnostics file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(path: Path, fragments: tuple[str, ...]) -> str:
    text = require_file(path)
    for fragment in fragments:
        if fragment not in text:
            fail(f"Missing required fragment {fragment!r} in {path}")
    return text


def require_absent(path: Path, text: str, fragments: tuple[str, ...]) -> None:
    for fragment in fragments:
        if fragment in text:
            fail(f"Prohibited launch/diagnostics fragment {fragment!r} in {path}")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    tools = repo_root / "Gems/TaintedGrailModdingSDK/Tools"
    launch_path = tools / "developer_preview_launch.py"
    diagnostics_path = tools / "developer_preview_diagnostics.py"
    diagnostics_support_path = tools / "developer_preview_diagnostics_support.py"
    launch_tests = tools / "tests/test_developer_preview_launch.py"
    diagnostics_tests = tools / "tests/test_developer_preview_diagnostics.py"
    guide_path = repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_0.md"
    troubleshooting_path = repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_TROUBLESHOOTING.md"
    workflow_path = repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml"

    try:
        launch = require_fragments(
            launch_path,
            (
                'PRIMARY_HOST = "Windows x64 Profile"',
                'EDITOR_FILENAME = "Editor.exe"',
                '"--project-path"',
                '"--log-dir"',
                '"--dry-run"',
                "validate_editor_executable",
                "validate_project_path",
                "launch_command",
                "default_executor",
                "return int(completed.returncode)",
                "Tools -> Tainted Grail SDK",
                "TaintedGrailModdingSDK",
                "does not launch FoA",
            ),
        )
        require_absent(
            launch_path,
            launch,
            (
                "shell=True",
                "os.system(",
                "Popen(",
                "--editor-arg",
                "FoA.exe",
            ),
        )

        diagnostics_main = require_file(diagnostics_path)
        diagnostics_support = require_file(diagnostics_support_path)
        diagnostics = diagnostics_main + "\n" + diagnostics_support
        for fragment in (
                'BUNDLE_ID="tg-sdk-developer-preview-0-diagnostics"',
                'MANIFEST_NAME="diagnostics-manifest.json"',
                "redact_argument_list",
                "redact_text",
                "assert_redacted_text",
                "workspace_inventory",
                "read_log_excerpt",
                "collect_tool_summary",
                "parse_cmake_cache",
                "build_manifest",
                "allowed_bundle_path",
                "verify_bundle",
                "source_artifact_contents_included",
                '"uploaded":False',
                '"review_required_before_sharing":True',
                "Review every generated file before sharing",
                "Nothing was uploaded",
                "MAX_LOG_BYTES_HARD",
                "MAX_BUNDLE_FILE_BYTES",
            ):
            if fragment not in diagnostics:
                fail(f"Missing required fragment {fragment!r} in diagnostics command/support")
        require_absent(
            diagnostics_path,
            diagnostics,
            (
                "requests.",
                "urllib.request",
                "socket.",
                "upload_file",
                "shutil.make_archive",
                "os.environ.items",
                "shell=True",
            ),
        )

        require_fragments(
            launch_tests,
            (
                "test_explicit_editor_must_be_editor_exe",
                "test_launch_command_has_no_arbitrary_or_runtime_arguments",
                "test_dry_run_does_not_invoke_executor_or_create_log_dir",
                "test_real_launch_requires_windows_x64",
                "test_launch_propagates_exit_code_and_writes_result",
                "test_os_error_becomes_127",
                "test_pane_guidance_names_tools_menu_and_activation_log",
            ),
        )
        require_fragments(
            diagnostics_tests,
            (
                "test_redaction_replaces_known_and_generic_private_paths",
                "test_redaction_removes_secret_assignments_tokens_and_url_credentials",
                "test_workspace_inventory_includes_only_durable_documents",
                "test_collect_and_verify_bundle",
                "test_collect_refuses_output_inside_workspace",
                "test_verify_detects_tampering",
                "test_verify_rejects_unexpected_file",
                "test_manifest_path_traversal_is_rejected",
            ),
        )
        require_fragments(
            guide_path,
            (
                "developer_preview_launch.py",
                "--project",
                "--log-dir",
                "developer_preview_diagnostics.py collect",
                "developer_preview_diagnostics.py verify",
                "review every generated file before sharing",
                "does not launch FoA",
            ),
        )
        require_fragments(
            troubleshooting_path,
            (
                "Tools \u2192 Tainted Grail SDK",
                "Editor.log",
                "diagnostics-manifest.json",
                "Nothing is uploaded automatically",
                "redaction",
            ),
        )
        require_fragments(
            workflow_path,
            (
                "Test Developer Preview 0 launch wrapper",
                "Test Developer Preview 0 diagnostics",
                "Validate Developer Preview 0 launch and diagnostics contract",
                'test_developer_preview_launch.py',
                'test_developer_preview_diagnostics.py',
                "validate_developer_preview_launch_diagnostics.py",
            ),
        )
    except (OSError, RuntimeError) as exc:
        print(f"Developer Preview 0 launch/diagnostics contract validation failed: {exc}", file=sys.stderr)
        return 1

    print("Developer Preview 0 Editor launch and redacted diagnostics contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
