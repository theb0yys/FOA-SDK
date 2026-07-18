#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the repository-owned Developer Preview O3DE project integration."""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

PREVIEW_PROJECT_PATH = Path("AutomatedTesting")
PREVIEW_PROJECT_NAME = "AutomatedTesting"
TG_GEM_NAME = "TaintedGrailModdingSDK"
TG_GEM_PATH = "Gems/TaintedGrailModdingSDK"


class PreviewProjectContractError(RuntimeError):
    """Raised when the repository-owned preview project contract is incomplete."""


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


def validate_preview_project(repo_root: Path) -> None:
    engine = load_json_object(repo_root / "engine.json", "engine manifest")
    projects = require_string_list(engine, "projects", "engine manifest")
    external_subdirectories = require_string_list(engine, "external_subdirectories", "engine manifest")

    if projects.count(PREVIEW_PROJECT_PATH.as_posix()) != 1:
        raise PreviewProjectContractError(
            f"engine.json must register {PREVIEW_PROJECT_PATH.as_posix()!r} exactly once."
        )
    if external_subdirectories.count(TG_GEM_PATH) != 1:
        raise PreviewProjectContractError(
            f"engine.json must register {TG_GEM_PATH!r} exactly once."
        )

    project_path = repo_root / PREVIEW_PROJECT_PATH / "project.json"
    project = load_json_object(project_path, "Developer Preview project manifest")
    if project.get("project_name") != PREVIEW_PROJECT_NAME:
        raise PreviewProjectContractError(
            f"{project_path} must keep project_name {PREVIEW_PROJECT_NAME!r}."
        )

    gem_names = require_string_list(project, "gem_names", "Developer Preview project manifest")
    if gem_names.count(TG_GEM_NAME) != 1:
        raise PreviewProjectContractError(
            f"{project_path} must enable {TG_GEM_NAME!r} exactly once."
        )

    quickstart_path = repo_root / "docs/tainted-grail-sdk/OPEN_AND_TEST_EDITOR.md"
    if not quickstart_path.is_file():
        raise PreviewProjectContractError(f"Required Editor quickstart is missing: {quickstart_path}")
    quickstart = quickstart_path.read_text(encoding="utf-8")
    for fragment in (
        "developer_preview_open.py",
        "AutomatedTesting",
        "Tools → Tainted Grail SDK",
        "AutomatedTesting/user/log/Editor.log",
    ):
        if fragment not in quickstart:
            raise PreviewProjectContractError(
                f"{quickstart_path} is missing required runnable-project fragment {fragment!r}."
            )

    opener_path = repo_root / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py"
    if not opener_path.is_file():
        raise PreviewProjectContractError(f"Repository-owned Editor opener is missing: {opener_path}")
    opener = opener_path.read_text(encoding="utf-8")
    for fragment in (
        'PREVIEW_PROJECT = Path("AutomatedTesting")',
        "validate_preview_project",
        "developer_preview_launch.main",
    ):
        if fragment not in opener:
            raise PreviewProjectContractError(
                f"{opener_path} is missing required project opener fragment {fragment!r}."
            )

    launcher_path = repo_root / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py"
    launcher = launcher_path.read_text(encoding="utf-8")
    for fragment in ('"--project"', '"--project-path"', "validate_project_path"):
        if fragment not in launcher:
            raise PreviewProjectContractError(
                f"{launcher_path} is missing required project launch fragment {fragment!r}."
            )


def main() -> int:
    repo_root = repository_root_from_script()
    try:
        validate_preview_project(repo_root)
    except (OSError, PreviewProjectContractError) as exc:
        print(f"Developer Preview project contract validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Developer Preview project contract passed: "
        "AutomatedTesting enables TaintedGrailModdingSDK and is documented as the launch project."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
