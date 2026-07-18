#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the dedicated Developer Preview O3DE project and clickable entry."""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path
from typing import Any

PREVIEW_PROJECT_PATH = Path("TaintedGrailModdingEditor")
PREVIEW_PROJECT_NAME = "TaintedGrailModdingEditor"
PREVIEW_DISPLAY_NAME = "Tainted Grail Modding Editor"
PREVIEW_EXECUTABLE_NAME = "TaintedGrailModdingEditor"
PREVIEW_PNG = "preview.png"
PREVIEW_ICO = "TaintedGrailModdingEditor.ico"
AUTOMATED_TESTING_PATH = Path("AutomatedTesting")
TG_GEM_NAME = "TaintedGrailModdingSDK"
TG_GEM_PATH = "Gems/TaintedGrailModdingSDK"


class PreviewProjectContractError(RuntimeError):
    """Raised when the dedicated preview project contract is incomplete."""


def repository_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def load_json_object(path: Path, label: str) -> dict[str, Any]:
    if not path.is_file():
        raise PreviewProjectContractError(f"{label} is missing: {path}")
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise PreviewProjectContractError(f"{label} is not valid UTF-8 JSON: {path}: {exc}") from exc
    if not isinstance(document, dict):
        raise PreviewProjectContractError(f"{label} must be a JSON object: {path}")
    return document


def require_string_list(document: dict[str, Any], key: str, label: str) -> list[str]:
    value = document.get(key)
    if not isinstance(value, list) or any(not isinstance(entry, str) for entry in value):
        raise PreviewProjectContractError(f"{label} must contain a string array named {key!r}.")
    return value


def validate_png(path: Path) -> None:
    if not path.is_file():
        raise PreviewProjectContractError(f"Project preview icon is missing: {path}")
    header = path.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise PreviewProjectContractError(f"Project preview icon is not a valid PNG: {path}")
    width, height = struct.unpack(">II", header[16:24])
    if width < 128 or height < 128:
        raise PreviewProjectContractError(f"Project preview icon must be at least 128x128: {path}")


def validate_ico(path: Path) -> None:
    if not path.is_file():
        raise PreviewProjectContractError(f"Windows shortcut icon is missing: {path}")
    header = path.read_bytes()[:6]
    if len(header) != 6 or header[:4] != b"\x00\x00\x01\x00":
        raise PreviewProjectContractError(f"Windows shortcut icon is not a valid ICO: {path}")
    image_count = int.from_bytes(header[4:6], "little")
    if image_count < 1:
        raise PreviewProjectContractError(f"Windows shortcut icon has no image entries: {path}")


def require_fragments(path: Path, fragments: tuple[str, ...], label: str) -> str:
    if not path.is_file():
        raise PreviewProjectContractError(f"{label} is missing: {path}")
    content = path.read_text(encoding="utf-8")
    for fragment in fragments:
        if fragment not in content:
            raise PreviewProjectContractError(
                f"{path} is missing required {label} fragment {fragment!r}."
            )
    return content


def validate_preview_project(repo_root: Path) -> None:
    engine = load_json_object(repo_root / "engine.json", "engine manifest")
    projects = require_string_list(engine, "projects", "engine manifest")
    external_subdirectories = require_string_list(engine, "external_subdirectories", "engine manifest")

    project_key = PREVIEW_PROJECT_PATH.as_posix()
    if projects.count(project_key) != 1:
        raise PreviewProjectContractError(f"engine.json must register {project_key!r} exactly once.")
    if external_subdirectories.count(TG_GEM_PATH) != 1:
        raise PreviewProjectContractError(f"engine.json must register {TG_GEM_PATH!r} exactly once.")

    project_root = repo_root / PREVIEW_PROJECT_PATH
    project_path = project_root / "project.json"
    project = load_json_object(project_path, "Developer Preview project manifest")
    expected_fields = {
        "project_name": PREVIEW_PROJECT_NAME,
        "display_name": PREVIEW_DISPLAY_NAME,
        "product_name": PREVIEW_DISPLAY_NAME,
        "executable_name": PREVIEW_EXECUTABLE_NAME,
        "icon_path": PREVIEW_PNG,
        "engine": "o3de",
    }
    for key, expected in expected_fields.items():
        if project.get(key) != expected:
            raise PreviewProjectContractError(f"{project_path} must keep {key}={expected!r}.")

    gem_names = require_string_list(project, "gem_names", "Developer Preview project manifest")
    if gem_names.count(TG_GEM_NAME) != 1:
        raise PreviewProjectContractError(f"{project_path} must enable {TG_GEM_NAME!r} exactly once.")

    for required in ("CMakeLists.txt", "cmake/EngineFinder.cmake"):
        path = project_root / required
        if not path.is_file():
            raise PreviewProjectContractError(f"Dedicated project build file is missing: {path}")
    validate_png(project_root / PREVIEW_PNG)
    validate_ico(project_root / PREVIEW_ICO)

    automated = load_json_object(
        repo_root / AUTOMATED_TESTING_PATH / "project.json",
        "AutomatedTesting project manifest",
    )
    automated_gems = require_string_list(automated, "gem_names", "AutomatedTesting project manifest")
    if TG_GEM_NAME in automated_gems:
        raise PreviewProjectContractError(
            "AutomatedTesting must not host the TG editor after the dedicated project is registered."
        )

    quickstart_path = repo_root / "docs/tainted-grail-sdk/OPEN_AND_TEST_EDITOR.md"
    quickstart = require_fragments(
        quickstart_path,
        (
            "TaintedGrailModdingEditor",
            "developer_preview_entry.py create",
            "developer_preview_entry.py verify",
            "Tainted Grail Modding Editor.lnk",
            "Tools → Tainted Grail SDK",
            "TaintedGrailModdingEditor/user/log/Editor.log",
        ),
        "dedicated-entry quickstart",
    )
    if "developer_preview_shortcut.py create" in quickstart:
        raise PreviewProjectContractError(
            "The quickstart must use the hardened developer_preview_entry.py command."
        )

    opener_path = repo_root / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py"
    opener = require_fragments(
        opener_path,
        (
            'PREVIEW_PROJECT = Path("TaintedGrailModdingEditor")',
            "validate_preview_project",
            "developer_preview_launch.main",
        ),
        "project opener",
    )

    shortcut_path = repo_root / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_shortcut.py"
    shortcut = require_fragments(
        shortcut_path,
        (
            "WScript.Shell",
            "TG_SHORTCUT_OUTPUT",
            "TaintedGrailModdingEditor.ico",
            "'--project-path \"'",
            "developer_preview_launch.resolve_editor_executable",
            "validate_preview_project",
        ),
        "low-level shortcut generator",
    )

    entry_path = repo_root / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py"
    entry = require_fragments(
        entry_path,
        (
            "developer_preview_shortcut.verify_shortcut",
            "developer_preview_shortcut.create_shortcut",
            "inspect_shortcut",
            "verify_entry",
            "if output.exists() and replace",
            "WScript.Shell",
        ),
        "hardened clickable entry",
    )

    for prohibited in (
        "shell=True",
        "AutomatedTesting/user/log",
        'PREVIEW_PROJECT = Path("AutomatedTesting")',
    ):
        if prohibited in shortcut or prohibited in entry or prohibited in opener:
            raise PreviewProjectContractError(
                f"Dedicated entry tooling still contains prohibited fragment {prohibited!r}."
            )

    launcher_path = repo_root / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py"
    require_fragments(
        launcher_path,
        ('"--project"', '"--project-path"', "validate_project_path"),
        "project launcher",
    )


def main() -> int:
    repo_root = repository_root_from_script()
    try:
        validate_preview_project(repo_root)
    except (OSError, PreviewProjectContractError) as exc:
        print(f"Developer Preview project contract validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Developer Preview project contract passed: dedicated "
        "TaintedGrailModdingEditor project, icons, opener, and hardened clickable entry are complete."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
