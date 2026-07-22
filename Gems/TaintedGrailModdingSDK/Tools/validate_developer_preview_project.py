#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the extracted FOA-SDK product tree and dedicated Editor project."""

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
PREVIEW_SCENE_SRG = "ShaderLib/scenesrg.srgi"
PREVIEW_VIEW_SRG = "ShaderLib/viewsrg.srgi"
PREVIEW_STARTUP_LEVEL = "Levels/DefaultLevel/DefaultLevel.prefab"
REQUIRED_PRODUCT_GEMS = ("ExternalToolchain", "TaintedGrailModdingSDK")
REQUIRED_PREVIEW_GEMS = (
    "Atom",
    "DiffuseProbeGrid",
    "PhysX5",
    *REQUIRED_PRODUCT_GEMS,
)
EXPECTED_EXTERNAL_SUBDIRECTORIES = (
    "../Gems/ExternalToolchain",
    "../Gems/TaintedGrailModdingSDK",
)
FORBIDDEN_ENGINE_ROOT_PATHS = (
    "Assets",
    "AutomatedTesting",
    "Code",
    "Docker",
    "Registry",
    "Templates",
    "Tools",
    "cmake",
    "python",
    "scripts",
    ".automatedtesting.json",
    "CMakeLists.txt",
    "CMakePresets.json",
    "engine.json",
)


class PreviewProjectContractError(RuntimeError):
    """Raised when the extracted product/project contract is incomplete."""


def repository_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def load_json_object(path: Path, label: str) -> dict[str, Any]:
    if not path.is_file():
        raise PreviewProjectContractError(f"{label} is missing: {path}")
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise PreviewProjectContractError(
            f"{label} is not valid UTF-8 JSON: {path}: {exc}"
        ) from exc
    if not isinstance(document, dict):
        raise PreviewProjectContractError(f"{label} must be a JSON object: {path}")
    return document


def require_string_list(document: dict[str, Any], key: str, label: str) -> list[str]:
    value = document.get(key)
    if not isinstance(value, list) or any(not isinstance(entry, str) for entry in value):
        raise PreviewProjectContractError(
            f"{label} must contain a string array named {key!r}."
        )
    return value


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
    if int.from_bytes(header[4:6], "little") < 1:
        raise PreviewProjectContractError(f"Windows shortcut icon has no image entries: {path}")


def validate_extracted_product_tree(product_root: Path) -> None:
    for relative_path in FORBIDDEN_ENGINE_ROOT_PATHS:
        path = product_root / relative_path
        if path.exists():
            raise PreviewProjectContractError(
                f"Inherited O3DE path must not exist in the FOA-SDK product tree: {relative_path}"
            )
    lock = load_json_object(product_root / "o3de.lock.json", "O3DE dependency lock")
    if lock.get("schema_version") != 1 or lock.get("engine_name") != "o3de":
        raise PreviewProjectContractError("O3DE dependency lock identity is invalid.")
    commit = lock.get("commit")
    if not isinstance(commit, str) or len(commit) != 40:
        raise PreviewProjectContractError("O3DE dependency lock must contain a full commit SHA.")

    gems_root = product_root / "Gems"
    if not gems_root.is_dir():
        raise PreviewProjectContractError(f"Product Gems root is missing: {gems_root}")
    actual_gems = sorted(path.name for path in gems_root.iterdir() if path.is_dir())
    if actual_gems != sorted(REQUIRED_PRODUCT_GEMS):
        raise PreviewProjectContractError(
            "FOA-SDK must contain exactly the two product-owned Gems; "
            f"found {actual_gems}."
        )
    for gem in REQUIRED_PRODUCT_GEMS:
        load_json_object(gems_root / gem / "gem.json", f"{gem} Gem descriptor")


def validate_preview_project(product_root: Path) -> None:
    validate_extracted_product_tree(product_root)
    project_root = product_root / PREVIEW_PROJECT_PATH
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

    external_subdirectories = require_string_list(
        project, "external_subdirectories", "Developer Preview project manifest"
    )
    if external_subdirectories != list(EXPECTED_EXTERNAL_SUBDIRECTORIES):
        raise PreviewProjectContractError(
            "Developer Preview project must register exactly the two product-owned Gem directories "
            "in deterministic order."
        )
    gem_names = require_string_list(project, "gem_names", "Developer Preview project manifest")
    for required_gem in REQUIRED_PREVIEW_GEMS:
        if gem_names.count(required_gem) != 1:
            raise PreviewProjectContractError(
                f"{project_path} must enable {required_gem!r} exactly once."
            )

    for required in ("CMakeLists.txt", "cmake/EngineFinder.cmake"):
        path = project_root / required
        if not path.is_file():
            raise PreviewProjectContractError(f"Dedicated project build file is missing: {path}")
    engine_finder = require_fragments(
        project_root / "cmake/EngineFinder.cmake",
        (
            "FOA_O3DE_ROOT",
            "o3de.lock.json",
            "External O3DE commit mismatch",
            "tg_editor_product_root",
            "tg_editor_engine_root",
        ),
        "external engine resolver",
    )
    if "tg_editor_engine_root)" not in engine_finder:
        raise PreviewProjectContractError("External engine resolver does not publish its engine root.")

    startup_level_path = project_root / PREVIEW_STARTUP_LEVEL
    startup_level = load_json_object(startup_level_path, "dedicated project startup level")
    for key in ("ContainerEntity", "Entities"):
        if key not in startup_level:
            raise PreviewProjectContractError(
                f"Dedicated project startup level is missing canonical top-level key {key!r}."
            )
    validate_png(project_root / PREVIEW_PNG)
    validate_ico(project_root / PREVIEW_ICO)
    require_fragments(
        project_root / PREVIEW_SCENE_SRG,
        ("#pragma once", "SrgSemantics.azsli", "partial ShaderResourceGroup SceneSrg : SRG_PerScene"),
        "project scene SRG extension",
    )
    require_fragments(
        project_root / PREVIEW_VIEW_SRG,
        ("#pragma once", "SrgSemantics.azsli", "partial ShaderResourceGroup ViewSrg : SRG_PerView"),
        "project view SRG extension",
    )

    quickstart = require_fragments(
        product_root / "docs/tainted-grail-sdk/OPEN_AND_TEST_EDITOR.md",
        (
            "TaintedGrailModdingEditor",
            "developer_preview_entry.py create",
            "developer_preview_entry.py verify",
            "DefaultLevel.prefab",
            "validate_path_policy.py",
            "bounded per-user storage",
            "Tainted Grail Modding Editor.lnk",
            "Tools \u2192 Tainted Grail SDK",
            "LOCALAPPDATA",
        ),
        "dedicated-entry quickstart",
    )
    if "developer_preview_shortcut.py create" in quickstart:
        raise PreviewProjectContractError(
            "The quickstart must use the trusted developer_preview_entry.py command."
        )

    tools = product_root / "Gems/TaintedGrailModdingSDK/Tools"
    require_fragments(
        tools / "developer_preview_path_policy.py",
        (
            "APPROVED_BUILD_DIRECTORY",
            "resolve_source_built_entry",
            "resolve_diagnostic_entry",
            "validate_source_editor_binary",
            "PREVIEW_STARTUP_LEVEL",
            "verify_preview_workspace",
            "developer_preview.validate_build_directory",
            "FOA-SDK product_root and O3DE engine_root must be separate checkouts",
            "Diagnostic overrides must not replace",
        ),
        "canonical path and executable policy",
    )
    require_fragments(
        tools / "developer_preview_shortcut.py",
        (
            "TG_ENGINE_PATH",
            "TG_PROJECT_CACHE_PATH",
            "TG_PROJECT_USER_PATH",
            "TG_PROJECT_LOG_PATH",
            "TG_STARTUP_LEVEL",
            "resolve_source_built_entry",
            "resolve_diagnostic_entry",
            '"trust_mode": paths.trust_mode',
        ),
        "shortcut generator",
    )
    require_fragments(
        tools / "developer_preview_entry.py",
        (
            "verify_shortcut",
            "create_shortcut",
            "resolve_source_built_entry",
            "_require_manifest_matches_policy",
            "Diagnostic override shortcuts are not verified source-built entries",
        ),
        "trusted clickable entry",
    )
    require_fragments(
        tools / "developer_preview_workspace.py",
        (
            "LOCALAPPDATA",
            "MANAGED_PROJECT_FILES",
            "PRESERVED_PROJECT_FILES",
            "materialize_preview_workspace",
            "verify_preview_workspace",
            "refusing to overwrite",
        ),
        "bounded per-user project materializer",
    )
    require_fragments(
        tools / "developer_preview_assets.py",
        (
            "AssetProcessorBatch.exe",
            "--project-cache-path",
            "--project-user-path",
            "--project-log-path",
            "verify_preview_workspace",
            "Asset preparation failed",
        ),
        "clean-first-run asset preflight",
    )


def main() -> int:
    try:
        validate_preview_project(repository_root_from_script())
    except (OSError, PreviewProjectContractError) as exc:
        print(f"Developer Preview project validation failed: {exc}", file=sys.stderr)
        return 1
    print("Extracted FOA-SDK product and Developer Preview project contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
