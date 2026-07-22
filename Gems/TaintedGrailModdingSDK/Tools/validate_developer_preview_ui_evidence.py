#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the Developer Preview 0 Windows manual UI evidence contract."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_file(path: Path) -> str:
    if not path.is_file():
        fail(f"Required manual UI evidence file is missing: {path}")
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
            fail(f"Prohibited manual UI evidence fragment {fragment!r} in {path}")


def load_module(path: Path):
    spec = importlib.util.spec_from_file_location("tg_ui_evidence_contract", path)
    if not spec or not spec.loader:
        fail(f"Unable to import manual UI evidence command: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    tools = repo_root / "Gems/TaintedGrailModdingSDK/Tools"
    script_path = tools / "developer_preview_ui_evidence.py"
    tests_path = tools / "tests/test_developer_preview_ui_evidence.py"
    checklist_path = repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md"
    guide_path = repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_0.md"
    docs_index_path = repo_root / "docs/tainted-grail-sdk/README.md"
    release_path = repo_root / "docs/tainted-grail-sdk/RELEASE_PROCESS.md"
    maintainer_path = repo_root / "docs/tainted-grail-sdk/MAINTAINER_CHECKLIST.md"
    workflow_path = repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml"
    changelog_path = repo_root / "CHANGELOG.md"

    try:
        script = require_fragments(
            script_path,
            (
                'EVIDENCE_ID = "tg-sdk-developer-preview-0-windows-ui"',
                'PRIMARY_HOST = "Windows x64 Profile"',
                'EDITOR_FILENAME = "Editor.exe"',
                'DOCUMENT_NAME = "ui-evidence.json"',
                'SCREENSHOTS_DIRECTORY = "screenshots"',
                "MAX_SCREENSHOT_BYTES",
                "MAX_SCREENSHOT_COUNT",
                "REQUIRED_SCREENSHOT_CHECKS",
                "all-panes-open",
                "normal-scaling-readable",
                "keyboard-traversal",
                "preview-data-displayed",
                "item-recipe-data-displayed",
                "road-atlas-editor-functional",
                "avalon-ai-editor-functional",
                "station-learnability-displayed",
                "save-close-reopen",
                "failure-message-actionable",
                "runtime-deployment-absent",
                "validate_output_directory",
                "png_dimensions",
                "attach_screenshot",
                "record_check",
                "attest_evidence",
                "verify_evidence",
                '"init"',
                '"record"',
                '"attach"',
                '"attest"',
                '"verify"',
                "--expected-commit",
                "does not capture the screen",
                "does not inspect screenshot pixels",
                "Nothing is uploaded automatically",
                "Do not commit screenshots",
            ),
        )
        require_absent(
            script_path,
            script,
            (
                "pyautogui",
                "ImageGrab",
                "mss.",
                "PIL.",
                "pytesseract",
                "ocr",
                "requests.",
                "urllib.request",
                "socket.",
                "upload_file",
                "shell=True",
                "os.system(",
                "subprocess.Popen",
                "FoA.exe",
            ),
        )

        module = load_module(script_path)
        checklist_ids = [entry["id"] for entry in module.CHECKLIST]
        expected_ids = [
            "all-panes-open",
            "normal-scaling-readable",
            "keyboard-traversal",
            "preview-data-displayed",
            "item-recipe-data-displayed",
            "road-atlas-editor-functional",
            "avalon-ai-editor-functional",
            "station-learnability-displayed",
            "save-close-reopen",
            "failure-message-actionable",
            "runtime-deployment-absent",
        ]
        if checklist_ids != expected_ids:
            fail("Manual UI checklist IDs or order changed without contract review.")
        if module.REQUIRED_SCREENSHOT_CHECKS != {
            "all-panes-open",
            "normal-scaling-readable",
            "preview-data-displayed",
            "item-recipe-data-displayed",
            "road-atlas-editor-functional",
            "avalon-ai-editor-functional",
            "station-learnability-displayed",
            "save-close-reopen",
            "failure-message-actionable",
        }:
            fail("Required screenshot coverage changed without contract review.")

        require_fragments(
            tests_path,
            (
                "test_init_creates_pending_evidence_without_claiming_a_pass",
                "test_init_rejects_tracked_source_output",
                "test_record_requires_public_notes",
                "test_attach_rejects_non_png_content",
                "test_attach_records_hash_dimensions_and_coverage",
                "test_attest_requires_every_confirmation",
                "test_complete_evidence_verifies",
                "test_verify_rejects_pending_check",
                "test_verify_rejects_missing_required_screenshot_coverage",
                "test_verify_detects_screenshot_tampering",
                "test_verify_rejects_unexpected_file",
                "test_verify_rejects_symlink",
                "test_verify_rejects_commit_mismatch",
            ),
        )

        require_fragments(
            checklist_path,
            (
                "Windows Manual UI Smoke and Screenshot Evidence",
                "real Windows x64 Profile",
                "Tools \u2192 Tainted Grail SDK",
                "All ten TG SDK panes",
                "Tainted Grail Adapter Work-Order Plans",
                "100\u2013200%",
                "keyboard traversal",
                "save, close, reopen",
                "failure message",
                "work-order execution, deployment, injection, or save-mutation",
                "developer_preview_ui_evidence.py init",
                "developer_preview_ui_evidence.py record",
                "developer_preview_ui_evidence.py attach",
                "developer_preview_ui_evidence.py attest",
                "developer_preview_ui_evidence.py verify",
                "Do not commit screenshots",
                "does not inspect screenshot pixels",
                "exact source commit",
            ),
        )
        require_fragments(
            guide_path,
            (
                "Windows manual UI smoke",
                "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md",
                "developer_preview_ui_evidence.py",
                "manual screenshot capture",
                "actual Windows pass remains pending",
            ),
        )
        require_fragments(
            docs_index_path,
            (
                "Windows Manual UI Smoke",
                "screenshot-evidence",
                "actual Windows screenshot pass remains pending",
            ),
        )
        require_fragments(
            release_path,
            (
                "Windows manual UI evidence",
                "exact reviewed `main` commit",
                "screenshot hashes",
                "privacy attestation",
                "must not be committed",
            ),
        )
        require_fragments(
            maintainer_path,
            (
                "manual UI evidence",
                "exact accepted commit",
                "screenshot verifier",
                "privacy review",
                "not committed",
            ),
        )
        require_fragments(
            workflow_path,
            (
                "Test Developer Preview 0 manual UI evidence tooling",
                "Validate Developer Preview 0 manual UI evidence contract",
                'test_developer_preview_ui_evidence.py',
                "validate_developer_preview_ui_evidence.py",
            ),
        )
        require_fragments(
            changelog_path,
            (
                "Windows manual UI checklist",
                "Screenshot-evidence initializer",
                "PNG integrity",
                "actual Windows screenshot pass remains pending",
            ),
        )
    except (ImportError, OSError, RuntimeError) as exc:
        print(f"Developer Preview 0 manual UI evidence contract validation failed: {exc}", file=sys.stderr)
        return 1

    print("Developer Preview 0 Windows manual UI evidence contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
