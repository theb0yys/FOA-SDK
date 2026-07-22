#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Prepare and verify Developer Preview 0 Windows manual UI evidence.

The command does not capture the screen, automate UI coordinates, upload files, launch FoA,
or inspect screenshot pixels. A tester must perform the checklist on a real Windows x64
Profile Editor session, review every screenshot, and attest that private or restricted
content is not visible.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
import sys
import tempfile
from datetime import datetime
from pathlib import Path, PurePosixPath
from typing import Any, Iterable

SCHEMA_VERSION = 1
EVIDENCE_ID = "tg-sdk-developer-preview-0-windows-ui"
DOCUMENT_NAME = "ui-evidence.json"
README_NAME = "README.txt"
SCREENSHOTS_DIRECTORY = "screenshots"
PRIMARY_HOST = "Windows x64 Profile"
EDITOR_FILENAME = "Editor.exe"
MAX_SCREENSHOT_BYTES = 12 * 1024 * 1024
MAX_SCREENSHOT_COUNT = 20
MAX_TOTAL_SCREENSHOT_BYTES = 96 * 1024 * 1024
MIN_SCREENSHOT_WIDTH = 800
MIN_SCREENSHOT_HEIGHT = 600

CHECKLIST = (
    {
        "id": "all-panes-open",
        "title": "All Tainted Grail SDK panes open from Tools",
        "screenshot_required": True,
    },
    {
        "id": "normal-scaling-readable",
        "title": "Labels and controls are readable at normal Windows scaling",
        "screenshot_required": True,
    },
    {
        "id": "keyboard-traversal",
        "title": "Keyboard traversal reaches primary controls",
        "screenshot_required": False,
    },
    {
        "id": "preview-data-displayed",
        "title": "Preview workspace, source evidence, and catalog data display correctly",
        "screenshot_required": True,
    },
    {
        "id": "item-recipe-data-displayed",
        "title": "Item and Recipe Editor displays expected typed data",
        "screenshot_required": True,
    },
    {
        "id": "road-atlas-editor-functional",
        "title": "Road Atlas Tool Gem validates, saves, reloads, and reverts an inert snapshot",
        "screenshot_required": True,
    },
    {
        "id": "avalon-ai-editor-functional",
        "title": "Avalon AI Tool Gem validates, plans, saves, reloads, and reverts an inert package",
        "screenshot_required": True,
    },
    {
        "id": "station-learnability-displayed",
        "title": "Station visibility and learnability rows display expected status and reasons",
        "screenshot_required": True,
    },
    {
        "id": "save-close-reopen",
        "title": "Save, close, reopen, and reload preserve the reviewed state",
        "screenshot_required": True,
    },
    {
        "id": "failure-message-actionable",
        "title": "Failure messages identify the affected path or subject",
        "screenshot_required": True,
    },
    {
        "id": "runtime-deployment-absent",
        "title": "No runtime, deployment, injection, or save-mutation action is exposed",
        "screenshot_required": False,
    },
)

CHECK_BY_ID = {entry["id"]: entry for entry in CHECKLIST}
REQUIRED_SCREENSHOT_CHECKS = {
    entry["id"] for entry in CHECKLIST if entry["screenshot_required"]
}

COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
ALIAS_PATTERN = re.compile(r"^[A-Za-z0-9._-]{2,64}$")
PRIVATE_PATH_PATTERN = re.compile(
    r"(?i)(?:[A-Z]:\\Users\\[^\\\r\n\t ]+|\\\\[^\\\r\n\t ]+\\[^\\\r\n\t ]+|/home/[^/\r\n\t ]+|/Users/[^/\r\n\t ]+)"
)
SECRET_PATTERNS = (
    re.compile(r"(?i)\b(?:password|passwd|token|secret|api[_-]?key|authorization)\s*[:=]\s*[^\s,;]+"),
    re.compile(r"(?i)\bBearer\s+[A-Za-z0-9._~+/=-]+"),
    re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|sk-[A-Za-z0-9_-]{20,})\b"),
    re.compile(r"(?i)\bhttps?://[^/\s:@]+:[^@\s/]+@"),
)


class UIEvidenceError(RuntimeError):
    """Raised when manual UI evidence is unsafe or incomplete."""


def repository_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def resolve_path(value: Path, base: Path) -> Path:
    path = value.expanduser()
    if not path.is_absolute():
        path = base / path
    return path.resolve(strict=False)


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False) + "\n").encode("utf-8")


def atomic_write(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="wb",
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=path.parent,
        delete=False,
    ) as temporary:
        temporary.write(payload)
        temporary.flush()
        temporary_path = Path(temporary.name)
    temporary_path.replace(path)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise UIEvidenceError(message)


def assert_public_text(value: str, label: str) -> None:
    require("\x00" not in value, f"{label} contains a NUL character.")
    require(not PRIVATE_PATH_PATTERN.search(value), f"{label} contains a private absolute path.")
    for pattern in SECRET_PATTERNS:
        require(not pattern.search(value), f"{label} contains secret-like material.")


def validate_source_commit(source_commit: str) -> str:
    normalized = source_commit.strip().lower()
    require(
        COMMIT_PATTERN.fullmatch(normalized) is not None,
        "Source commit must be a full lowercase 40-character Git SHA.",
    )
    return normalized


def validate_tester_alias(tester_alias: str) -> str:
    value = tester_alias.strip()
    require(
        ALIAS_PATTERN.fullmatch(value) is not None,
        "Tester alias must use 2-64 letters, digits, dots, underscores, or hyphens.",
    )
    return value


def validate_display_scale(display_scale_percent: int) -> int:
    require(100 <= display_scale_percent <= 200, "Display scale must be between 100 and 200 percent.")
    return display_scale_percent


def validate_tested_at(value: str) -> str:
    require(value.endswith("Z"), "Tested-at time must be an ISO-8601 UTC timestamp ending in Z.")
    try:
        datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as exc:
        raise UIEvidenceError("Tested-at time is not valid ISO-8601.") from exc
    return value


def validate_output_directory(output_dir: Path, repo_root: Path) -> None:
    require(output_dir != repo_root, "UI evidence output must not be the repository root.")
    require(not is_relative_to(output_dir, repo_root / ".git"), "UI evidence output must not be inside .git.")
    if is_relative_to(output_dir, repo_root):
        build_root = repo_root / "build"
        require(
            is_relative_to(output_dir, build_root),
            "UI evidence inside the repository must be written beneath build/ so it is not tracked accidentally.",
        )


def ensure_no_symlinks(root: Path) -> None:
    require(not root.is_symlink(), f"UI evidence root must not be a symbolic link: {root}")
    if not root.exists():
        return
    for path in root.rglob("*"):
        require(not path.is_symlink(), f"UI evidence must not contain symbolic links: {path}")


def normalize_relative_path(value: str) -> str:
    require(isinstance(value, str) and value, "Evidence paths must be non-empty strings.")
    require("\\" not in value, f"Evidence paths must use forward slashes: {value}")
    pure = PurePosixPath(value)
    require(not pure.is_absolute(), f"Evidence path must be relative: {value}")
    require(".." not in pure.parts and "." not in pure.parts, f"Evidence path must not traverse directories: {value}")
    normalized = pure.as_posix()
    require(
        normalized.startswith(f"{SCREENSHOTS_DIRECTORY}/"),
        f"Screenshot must be stored under {SCREENSHOTS_DIRECTORY}/: {value}",
    )
    require(normalized.lower().endswith(".png"), f"Screenshot must use .png: {value}")
    return normalized


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def png_dimensions(path: Path) -> tuple[int, int]:
    size = path.stat().st_size
    require(0 < size <= MAX_SCREENSHOT_BYTES, f"Screenshot size is outside the allowed range: {path}")
    with path.open("rb") as stream:
        header = stream.read(24)
    require(len(header) == 24, f"Screenshot is too short to be a PNG: {path}")
    require(header[:8] == b"\x89PNG\r\n\x1a\n", f"Screenshot has an invalid PNG signature: {path}")
    require(header[12:16] == b"IHDR", f"Screenshot is missing the PNG IHDR header: {path}")
    width, height = struct.unpack(">II", header[16:24])
    require(width >= MIN_SCREENSHOT_WIDTH, f"Screenshot width must be at least {MIN_SCREENSHOT_WIDTH}: {path}")
    require(height >= MIN_SCREENSHOT_HEIGHT, f"Screenshot height must be at least {MIN_SCREENSHOT_HEIGHT}: {path}")
    return width, height


def read_document(output_dir: Path) -> dict[str, Any]:
    document_path = output_dir / DOCUMENT_NAME
    require(document_path.is_file(), f"UI evidence document is missing: {document_path}")
    try:
        document = json.loads(document_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise UIEvidenceError(f"Unable to read UI evidence document: {exc}") from exc
    require(isinstance(document, dict), "UI evidence document must be a JSON object.")
    require(document_path.read_bytes() == canonical_json_bytes(document), "UI evidence JSON must remain canonical.")
    return document


def write_document(output_dir: Path, document: dict[str, Any]) -> None:
    text = canonical_json_bytes(document).decode("utf-8")
    assert_public_text(text, DOCUMENT_NAME)
    atomic_write(output_dir / DOCUMENT_NAME, text.encode("utf-8"))


def initial_document(
    *,
    source_commit: str,
    tester_alias: str,
    windows_version: str,
    display_scale_percent: int,
) -> dict[str, Any]:
    source_commit = validate_source_commit(source_commit)
    tester_alias = validate_tester_alias(tester_alias)
    windows_version = windows_version.strip()
    require(windows_version, "Windows version is required.")
    assert_public_text(windows_version, "Windows version")
    display_scale_percent = validate_display_scale(display_scale_percent)
    return {
        "schema_version": SCHEMA_VERSION,
        "evidence_id": EVIDENCE_ID,
        "source_commit": source_commit,
        "status": "pending",
        "tested_at_utc": "",
        "tester_alias": tester_alias,
        "host": {
            "operating_system": "Windows",
            "architecture": "x64",
            "configuration": "profile",
            "windows_version": windows_version,
            "display_scale_percent": display_scale_percent,
            "editor_executable": EDITOR_FILENAME,
            "launch_exit_code": None,
            "tg_sdk_activation_log_confirmed": False,
        },
        "checklist": [
            {
                "id": entry["id"],
                "title": entry["title"],
                "status": "pending",
                "notes": "",
                "screenshot_required": entry["screenshot_required"],
            }
            for entry in CHECKLIST
        ],
        "screenshots": [],
        "attestation": {
            "reviewed_for_private_data": False,
            "contains_only_project_owned_or_approved_content": False,
            "no_game_files_or_saves_visible": False,
            "no_credentials_or_private_paths_visible": False,
            "no_runtime_or_deployment_action_exposed": False,
            "not_uploaded_by_tool": True,
            "tester_confirmed": False,
        },
    }


def readme_text(source_commit: str) -> str:
    return f"""Developer Preview 0 Windows manual UI evidence
================================================

Source commit: {source_commit}

This directory is release evidence for a real Windows x64 Profile O3DE Editor session.
The tool does not capture screenshots or automate UI coordinates. Use Windows screenshot
tools manually, review each image, then attach it with developer_preview_ui_evidence.py.

Do not include:
- proprietary game content, game files, or saves;
- credentials, tokens, user names, private paths, or unrelated desktop content;
- FoA, BepInEx, Harmony, Avalon Core, deployment, injection, or save-mutation screens.

Nothing is uploaded automatically. Evidence is incomplete until every required checklist
item is recorded as pass, required screenshot coverage is attached, all attestations are
confirmed, and the verify command succeeds for the exact source commit.

Keep the directory under build/ or outside the repository. Do not commit screenshots.
"""


def initialize_evidence(
    output_dir: Path,
    *,
    repo_root: Path,
    source_commit: str,
    tester_alias: str,
    windows_version: str,
    display_scale_percent: int,
) -> dict[str, Any]:
    output_dir = output_dir.resolve(strict=False)
    repo_root = repo_root.resolve(strict=False)
    validate_output_directory(output_dir, repo_root)
    require(not output_dir.is_symlink(), f"UI evidence output must not be a symbolic link: {output_dir}")
    if output_dir.exists():
        require(output_dir.is_dir(), f"UI evidence output is not a directory: {output_dir}")
        require(not any(output_dir.iterdir()), f"UI evidence output must be empty: {output_dir}")
    else:
        output_dir.mkdir(parents=True)
    (output_dir / SCREENSHOTS_DIRECTORY).mkdir()
    document = initial_document(
        source_commit=source_commit,
        tester_alias=tester_alias,
        windows_version=windows_version,
        display_scale_percent=display_scale_percent,
    )
    write_document(output_dir, document)
    atomic_write(output_dir / README_NAME, readme_text(document["source_commit"]).encode("utf-8"))
    return document


def find_check(document: dict[str, Any], check_id: str) -> dict[str, Any]:
    require(check_id in CHECK_BY_ID, f"Unknown manual UI checklist ID: {check_id}")
    checklist = document.get("checklist")
    require(isinstance(checklist, list), "UI evidence checklist must be an array.")
    for check in checklist:
        if isinstance(check, dict) and check.get("id") == check_id:
            return check
    raise UIEvidenceError(f"UI evidence checklist is missing: {check_id}")


def record_check(
    output_dir: Path,
    *,
    check_id: str,
    status: str,
    notes: str,
) -> dict[str, Any]:
    document = read_document(output_dir)
    require(status in {"pass", "fail", "blocked"}, "Checklist status must be pass, fail, or blocked.")
    notes = notes.strip()
    require(notes, "Checklist notes are required for every recorded result.")
    assert_public_text(notes, f"Checklist notes for {check_id}")
    check = find_check(document, check_id)
    check["status"] = status
    check["notes"] = notes
    document["status"] = "candidate"
    write_document(output_dir, document)
    return document


def next_screenshot_path(document: dict[str, Any], check_ids: list[str]) -> str:
    sequence = len(document.get("screenshots", [])) + 1
    base = "-".join(check_ids[:2])
    base = re.sub(r"[^a-z0-9-]+", "-", base.lower()).strip("-") or "ui"
    return f"{SCREENSHOTS_DIRECTORY}/{sequence:02d}-{base}.png"


def attach_screenshot(
    output_dir: Path,
    *,
    screenshot: Path,
    check_ids: Iterable[str],
    title: str,
    description: str,
    privacy_reviewed: bool,
    project_owned_only: bool,
) -> dict[str, Any]:
    require(privacy_reviewed, "Screenshot attachment requires explicit private-data review.")
    require(project_owned_only, "Screenshot attachment requires project-owned/approved-content confirmation.")
    document = read_document(output_dir)
    checks = sorted(set(check_ids))
    require(checks, "At least one checklist ID must be attached to a screenshot.")
    for check_id in checks:
        require(check_id in CHECK_BY_ID, f"Unknown screenshot checklist ID: {check_id}")
    title = title.strip()
    description = description.strip()
    require(title and description, "Screenshot title and description are required.")
    assert_public_text(title, "Screenshot title")
    assert_public_text(description, "Screenshot description")
    screenshot = screenshot.resolve(strict=True)
    require(screenshot.is_file(), f"Screenshot is not a file: {screenshot}")
    require(not screenshot.is_symlink(), f"Screenshot source must not be a symbolic link: {screenshot}")
    width, height = png_dimensions(screenshot)
    entries = document.get("screenshots")
    require(isinstance(entries, list), "UI evidence screenshots must be an array.")
    require(len(entries) < MAX_SCREENSHOT_COUNT, f"No more than {MAX_SCREENSHOT_COUNT} screenshots are allowed.")
    relative_path = next_screenshot_path(document, checks)
    target = output_dir / relative_path
    require(not target.exists(), f"Screenshot target already exists: {target}")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(screenshot, target)
    entry = {
        "path": relative_path,
        "sha256": sha256_file(target),
        "size_bytes": target.stat().st_size,
        "width": width,
        "height": height,
        "checks": checks,
        "title": title,
        "description": description,
        "privacy_reviewed": True,
        "project_owned_or_approved_content": True,
    }
    entries.append(entry)
    document["status"] = "candidate"
    write_document(output_dir, document)
    return document


def attest_evidence(
    output_dir: Path,
    *,
    tested_at_utc: str,
    launch_exit_code: int,
    activation_log_confirmed: bool,
    reviewed_for_private_data: bool,
    contains_only_project_owned_or_approved_content: bool,
    no_game_files_or_saves_visible: bool,
    no_credentials_or_private_paths_visible: bool,
    no_runtime_or_deployment_action_exposed: bool,
    tester_confirmed: bool,
) -> dict[str, Any]:
    require(launch_exit_code == 0, "The accepted manual UI run requires an Editor launch exit code of 0.")
    flags = {
        "tg_sdk_activation_log_confirmed": activation_log_confirmed,
        "reviewed_for_private_data": reviewed_for_private_data,
        "contains_only_project_owned_or_approved_content": contains_only_project_owned_or_approved_content,
        "no_game_files_or_saves_visible": no_game_files_or_saves_visible,
        "no_credentials_or_private_paths_visible": no_credentials_or_private_paths_visible,
        "no_runtime_or_deployment_action_exposed": no_runtime_or_deployment_action_exposed,
        "tester_confirmed": tester_confirmed,
    }
    missing = [name for name, value in flags.items() if not value]
    require(not missing, "All manual UI evidence attestations are required: " + ", ".join(missing))
    document = read_document(output_dir)
    document["tested_at_utc"] = validate_tested_at(tested_at_utc)
    document["host"]["launch_exit_code"] = launch_exit_code
    document["host"]["tg_sdk_activation_log_confirmed"] = True
    attestation = document["attestation"]
    for name in (
        "reviewed_for_private_data",
        "contains_only_project_owned_or_approved_content",
        "no_game_files_or_saves_visible",
        "no_credentials_or_private_paths_visible",
        "no_runtime_or_deployment_action_exposed",
        "tester_confirmed",
    ):
        attestation[name] = True
    attestation["not_uploaded_by_tool"] = True
    document["status"] = "candidate"
    write_document(output_dir, document)
    return document


def actual_files(output_dir: Path) -> set[str]:
    files: set[str] = set()
    for path in output_dir.rglob("*"):
        if path.is_file():
            files.add(path.relative_to(output_dir).as_posix())
    return files


def verify_evidence(output_dir: Path, *, expected_commit: str) -> dict[str, Any]:
    output_dir = output_dir.resolve(strict=True)
    require(output_dir.is_dir(), f"UI evidence output is not a directory: {output_dir}")
    ensure_no_symlinks(output_dir)
    expected_commit = validate_source_commit(expected_commit)
    document = read_document(output_dir)
    require(document.get("schema_version") == SCHEMA_VERSION, "Unsupported UI evidence schema version.")
    require(document.get("evidence_id") == EVIDENCE_ID, "Unexpected UI evidence ID.")
    require(
        document.get("source_commit") == expected_commit,
        "UI evidence source commit does not match the expected commit.",
    )
    require(document.get("status") == "candidate", "UI evidence must be attested as a candidate before verification.")
    validate_tester_alias(str(document.get("tester_alias", "")))
    validate_tested_at(str(document.get("tested_at_utc", "")))
    host = document.get("host")
    require(isinstance(host, dict), "UI evidence host metadata must be an object.")
    require(host.get("operating_system") == "Windows", "Manual UI evidence must come from Windows.")
    require(host.get("architecture") == "x64", "Manual UI evidence must come from x64.")
    require(host.get("configuration") == "profile", "Manual UI evidence must use the Profile configuration.")
    require(host.get("editor_executable") == EDITOR_FILENAME, "Manual UI evidence must use Editor.exe.")
    require(host.get("launch_exit_code") == 0, "Manual UI evidence requires a successful Editor launch.")
    require(host.get("tg_sdk_activation_log_confirmed") is True, "TG SDK activation log confirmation is required.")
    validate_display_scale(int(host.get("display_scale_percent", 0)))
    windows_version = str(host.get("windows_version", "")).strip()
    require(windows_version, "Windows version is required.")
    assert_public_text(windows_version, "Windows version")

    checklist = document.get("checklist")
    require(isinstance(checklist, list), "UI evidence checklist must be an array.")
    require(len(checklist) == len(CHECKLIST), "UI evidence checklist count is incorrect.")
    for index, expected in enumerate(CHECKLIST):
        check = checklist[index]
        require(isinstance(check, dict), "Each UI evidence checklist entry must be an object.")
        require(check.get("id") == expected["id"], f"Checklist order or ID mismatch at {expected['id']}.")
        require(check.get("title") == expected["title"], f"Checklist title mismatch at {expected['id']}.")
        require(
            check.get("screenshot_required") is expected["screenshot_required"],
            f"Checklist screenshot requirement mismatch at {expected['id']}.",
        )
        require(check.get("status") == "pass", f"Checklist item did not pass: {expected['id']}.")
        notes = str(check.get("notes", "")).strip()
        require(notes, f"Checklist notes are required: {expected['id']}.")
        assert_public_text(notes, f"Checklist notes for {expected['id']}")

    screenshots = document.get("screenshots")
    require(isinstance(screenshots, list), "UI evidence screenshots must be an array.")
    require(1 <= len(screenshots) <= MAX_SCREENSHOT_COUNT, "UI evidence screenshot count is outside the allowed range.")
    expected_files = {DOCUMENT_NAME, README_NAME}
    seen_paths: set[str] = set()
    covered_checks: set[str] = set()
    total_size = 0
    for entry in screenshots:
        require(isinstance(entry, dict), "Each screenshot entry must be an object.")
        relative_path = normalize_relative_path(entry.get("path"))
        require(relative_path not in seen_paths, f"Duplicate screenshot path: {relative_path}")
        seen_paths.add(relative_path)
        expected_files.add(relative_path)
        screenshot_path = output_dir / relative_path
        require(screenshot_path.is_file(), f"Screenshot is missing: {relative_path}")
        width, height = png_dimensions(screenshot_path)
        size = screenshot_path.stat().st_size
        total_size += size
        require(entry.get("sha256") == sha256_file(screenshot_path), f"Screenshot hash mismatch: {relative_path}")
        require(entry.get("size_bytes") == size, f"Screenshot size mismatch: {relative_path}")
        require(
            entry.get("width") == width and entry.get("height") == height,
            f"Screenshot dimensions mismatch: {relative_path}",
        )
        checks = entry.get("checks")
        require(
            isinstance(checks, list) and checks == sorted(set(checks)),
            f"Screenshot checks must be sorted and unique: {relative_path}",
        )
        require(checks, f"Screenshot must cover at least one checklist item: {relative_path}")
        for check_id in checks:
            require(check_id in CHECK_BY_ID, f"Screenshot references an unknown checklist item: {check_id}")
            covered_checks.add(check_id)
        require(entry.get("privacy_reviewed") is True, f"Screenshot privacy review is missing: {relative_path}")
        require(
            entry.get("project_owned_or_approved_content") is True,
            f"Screenshot content attestation is missing: {relative_path}",
        )
        for field in ("title", "description"):
            value = str(entry.get(field, "")).strip()
            require(value, f"Screenshot {field} is required: {relative_path}")
            assert_public_text(value, f"Screenshot {field} for {relative_path}")
    require(total_size <= MAX_TOTAL_SCREENSHOT_BYTES, "Total screenshot size exceeds the evidence limit.")
    missing_coverage = sorted(REQUIRED_SCREENSHOT_CHECKS - covered_checks)
    require(not missing_coverage, "Required screenshot coverage is missing: " + ", ".join(missing_coverage))

    attestation = document.get("attestation")
    require(isinstance(attestation, dict), "UI evidence attestation must be an object.")
    for name in (
        "reviewed_for_private_data",
        "contains_only_project_owned_or_approved_content",
        "no_game_files_or_saves_visible",
        "no_credentials_or_private_paths_visible",
        "no_runtime_or_deployment_action_exposed",
        "not_uploaded_by_tool",
        "tester_confirmed",
    ):
        require(attestation.get(name) is True, f"Required UI evidence attestation is missing: {name}")

    readme_path = output_dir / README_NAME
    require(readme_path.is_file(), f"UI evidence README is missing: {readme_path}")
    readme = readme_path.read_text(encoding="utf-8")
    require("Nothing is uploaded automatically" in readme, "UI evidence README must state the no-upload boundary.")
    require("Do not commit screenshots" in readme, "UI evidence README must state the no-commit boundary.")
    assert_public_text(readme, README_NAME)

    found_files = actual_files(output_dir)
    require(found_files == expected_files, "UI evidence contains missing or unexpected files.")
    document_text = canonical_json_bytes(document).decode("utf-8")
    assert_public_text(document_text, DOCUMENT_NAME)
    return document


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    initialize = subparsers.add_parser("init", help="Create a pending Windows manual UI evidence directory.")
    initialize.add_argument("--output", type=Path, required=True)
    initialize.add_argument("--repo-root", type=Path)
    initialize.add_argument("--source-commit", required=True)
    initialize.add_argument("--tester-alias", required=True)
    initialize.add_argument("--windows-version", required=True)
    initialize.add_argument("--display-scale", type=int, default=100)

    record = subparsers.add_parser("record", help="Record one manual checklist result.")
    record.add_argument("--output", type=Path, required=True)
    record.add_argument("--check", required=True, choices=tuple(CHECK_BY_ID))
    record.add_argument("--status", required=True, choices=("pass", "fail", "blocked"))
    record.add_argument("--notes", required=True)

    attach = subparsers.add_parser("attach", help="Copy and register one reviewed PNG screenshot.")
    attach.add_argument("--output", type=Path, required=True)
    attach.add_argument("--screenshot", type=Path, required=True)
    attach.add_argument("--check", action="append", required=True, choices=tuple(CHECK_BY_ID))
    attach.add_argument("--title", required=True)
    attach.add_argument("--description", required=True)
    attach.add_argument("--privacy-reviewed", action="store_true")
    attach.add_argument("--project-owned-only", action="store_true")

    attest = subparsers.add_parser("attest", help="Record the final Windows-run and disclosure attestations.")
    attest.add_argument("--output", type=Path, required=True)
    attest.add_argument("--tested-at", required=True)
    attest.add_argument("--launch-exit-code", type=int, required=True)
    attest.add_argument("--activation-log-confirmed", action="store_true")
    attest.add_argument("--reviewed-for-private-data", action="store_true")
    attest.add_argument("--project-owned-only", action="store_true")
    attest.add_argument("--no-game-files-or-saves", action="store_true")
    attest.add_argument("--no-credentials-or-private-paths", action="store_true")
    attest.add_argument("--no-runtime-or-deployment", action="store_true")
    attest.add_argument("--tester-confirmed", action="store_true")

    verify = subparsers.add_parser("verify", help="Verify completed checklist, PNG hashes, coverage, and attestations.")
    verify.add_argument("--output", type=Path, required=True)
    verify.add_argument("--expected-commit", required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        output = resolve_path(args.output, Path.cwd())
        if args.command == "init":
            repo_root = resolve_path(args.repo_root or repository_root_from_script(), Path.cwd())
            initialize_evidence(
                output,
                repo_root=repo_root,
                source_commit=args.source_commit,
                tester_alias=args.tester_alias,
                windows_version=args.windows_version,
                display_scale_percent=args.display_scale,
            )
            print(f"Pending Windows manual UI evidence initialized: {output}")
            print("No screenshot was captured and no UI pass is claimed.")
            return 0
        if args.command == "record":
            record_check(output, check_id=args.check, status=args.status, notes=args.notes)
            print(f"Recorded {args.check}: {args.status}")
            return 0
        if args.command == "attach":
            screenshot = resolve_path(args.screenshot, Path.cwd())
            attach_screenshot(
                output,
                screenshot=screenshot,
                check_ids=args.check,
                title=args.title,
                description=args.description,
                privacy_reviewed=args.privacy_reviewed,
                project_owned_only=args.project_owned_only,
            )
            print(f"Reviewed screenshot attached beneath: {output / SCREENSHOTS_DIRECTORY}")
            return 0
        if args.command == "attest":
            attest_evidence(
                output,
                tested_at_utc=args.tested_at,
                launch_exit_code=args.launch_exit_code,
                activation_log_confirmed=args.activation_log_confirmed,
                reviewed_for_private_data=args.reviewed_for_private_data,
                contains_only_project_owned_or_approved_content=args.project_owned_only,
                no_game_files_or_saves_visible=args.no_game_files_or_saves,
                no_credentials_or_private_paths_visible=args.no_credentials_or_private_paths,
                no_runtime_or_deployment_action_exposed=args.no_runtime_or_deployment,
                tester_confirmed=args.tester_confirmed,
            )
            print(f"Windows manual UI evidence attested: {output}")
            return 0
        verify_evidence(output, expected_commit=args.expected_commit)
        print(f"Developer Preview 0 Windows manual UI evidence verified: {output}")
        print("Verification checks metadata and file integrity; it does not inspect screenshot pixels.")
        return 0
    except (OSError, UIEvidenceError) as exc:
        print(f"Developer Preview 0 UI evidence command failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
