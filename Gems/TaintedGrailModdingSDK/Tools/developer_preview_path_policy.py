#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Canonical product, engine, build, and executable trust policy for FOA-SDK tooling."""

from __future__ import annotations

import platform
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import developer_preview
import developer_preview_workspace

# Compatibility defaults used by existing command parsers. Resolution is dynamic through
# approved_build_directory(), so FOA_BUILD_ROOT remains authoritative.
APPROVED_BUILD_DIRECTORY = Path("../foa-build/tg-sdk-developer-preview-0-windows-profile")
PREVIEW_PROJECT = Path("TaintedGrailModdingEditor")
PREVIEW_ICON = PREVIEW_PROJECT / "TaintedGrailModdingEditor.ico"
PREVIEW_STARTUP_LEVEL = PREVIEW_PROJECT / "Levels/DefaultLevel/DefaultLevel.prefab"
DEFAULT_SHORTCUT_OUTPUT = Path("../foa-build/Tainted Grail Modding Editor.lnk")
DIAGNOSTIC_OUTPUT_DIRECTORY = Path("../foa-build/diagnostic-entries")
EDITOR_CANDIDATES = (
    Path("bin/profile/Editor.exe"),
    Path("bin/Profile/Editor.exe"),
)
TRUST_MODE_SOURCE_BUILD = "source-built"
TRUST_MODE_DIAGNOSTIC_OVERRIDE = "diagnostic-override"
MINIMUM_EDITOR_BINARY_SIZE = 4096
PE_MACHINE_AMD64 = 0x8664
PE32_PLUS_MAGIC = 0x20B
PE_SUBSYSTEM_WINDOWS_GUI = 2


class PathPolicyError(RuntimeError):
    """Raised when a path violates the canonical FOA-SDK path policy."""


@dataclass(frozen=True)
class PreviewEntryPaths:
    trust_mode: str
    repo_root: Path
    build_directory: Path | None
    editor: Path
    engine: Path
    project: Path
    startup_level: Path
    project_cache: Path
    project_user: Path
    project_log: Path
    icon: Path
    working_directory: Path


def canonical_path(value: Path, base: Path | None = None, *, require_exists: bool = False) -> Path:
    path = value.expanduser()
    if not path.is_absolute():
        path = (base or Path.cwd()) / path
    try:
        return path.resolve(strict=require_exists)
    except (OSError, RuntimeError) as exc:
        raise PathPolicyError(f"Unable to resolve canonical path {path}: {exc}") from exc


def _case_insensitive_default() -> bool:
    return platform.system() == "Windows"


def same_path(left: Path, right: Path, *, case_insensitive: bool | None = None) -> bool:
    insensitive = _case_insensitive_default() if case_insensitive is None else case_insensitive
    left_parts = left.parts
    right_parts = right.parts
    if len(left_parts) != len(right_parts):
        return False
    if insensitive:
        return all(a.casefold() == b.casefold() for a, b in zip(left_parts, right_parts))
    return left_parts == right_parts


def is_contained(root: Path, candidate: Path, *, case_insensitive: bool | None = None) -> bool:
    insensitive = _case_insensitive_default() if case_insensitive is None else case_insensitive
    root_parts = root.parts
    candidate_parts = candidate.parts
    if len(candidate_parts) < len(root_parts):
        return False
    if insensitive:
        return all(a.casefold() == b.casefold() for a, b in zip(root_parts, candidate_parts))
    return candidate_parts[: len(root_parts)] == root_parts


def require_contained(
    root: Path,
    candidate: Path,
    *,
    label: str,
    case_insensitive: bool | None = None,
    root_must_exist: bool = True,
) -> Path:
    canonical_root = canonical_path(root, require_exists=root_must_exist)
    canonical_candidate = canonical_path(candidate, require_exists=False)
    if not is_contained(canonical_root, canonical_candidate, case_insensitive=case_insensitive):
        raise PathPolicyError(f"{label} must remain inside {canonical_root}: {canonical_candidate}")
    return canonical_candidate


def validate_source_editor_binary(editor: Path) -> None:
    try:
        size = editor.stat().st_size
        with editor.open("rb") as stream:
            header = stream.read(MINIMUM_EDITOR_BINARY_SIZE)
    except OSError as exc:
        raise PathPolicyError(f"Unable to inspect the source-built Editor.exe: {editor}: {exc}") from exc
    if size < MINIMUM_EDITOR_BINARY_SIZE or len(header) < MINIMUM_EDITOR_BINARY_SIZE:
        raise PathPolicyError(f"The approved Editor.exe is too small to be an O3DE Windows binary: {editor}")
    if header[:2] != b"MZ":
        raise PathPolicyError(f"The approved Editor.exe has no Windows PE header: {editor}")
    pe_offset = struct.unpack_from("<I", header, 0x3C)[0]
    optional_offset = pe_offset + 24
    if optional_offset + 70 > len(header) or header[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise PathPolicyError(f"The approved Editor.exe has an invalid PE signature: {editor}")
    machine = struct.unpack_from("<H", header, pe_offset + 4)[0]
    optional_magic = struct.unpack_from("<H", header, optional_offset)[0]
    subsystem = struct.unpack_from("<H", header, optional_offset + 68)[0]
    if machine != PE_MACHINE_AMD64:
        raise PathPolicyError(f"The approved Editor.exe is not an x64 Windows executable: {editor}")
    if optional_magic != PE32_PLUS_MAGIC or subsystem != PE_SUBSYSTEM_WINDOWS_GUI:
        raise PathPolicyError(f"The approved Editor.exe is not a 64-bit Windows GUI executable: {editor}")


def _product_owned_path(
    product_root: Path,
    value: Path,
    *,
    label: str,
    require_exists: bool,
) -> Path:
    canonical = canonical_path(value, require_exists=require_exists)
    if not is_contained(product_root, canonical):
        raise PathPolicyError(
            f"{label} must resolve inside the FOA-SDK product checkout; "
            f"symlink or junction escape rejected: {canonical}"
        )
    return canonical


def _product_paths(product_root: Path) -> tuple[Path, Path, Path, Path]:
    product = canonical_path(product_root, require_exists=True)
    try:
        developer_preview.validate_product_root(product)
    except RuntimeError as exc:
        raise PathPolicyError(str(exc)) from exc
    project = _product_owned_path(
        product,
        product / PREVIEW_PROJECT,
        label="Dedicated editor project",
        require_exists=True,
    )
    icon = _product_owned_path(
        product,
        product / PREVIEW_ICON,
        label="Dedicated editor icon",
        require_exists=True,
    )
    startup_level = _product_owned_path(
        product,
        product / PREVIEW_STARTUP_LEVEL,
        label="Dedicated editor startup level",
        require_exists=True,
    )
    if not project.is_dir():
        raise PathPolicyError(f"Dedicated editor project is not a directory: {project}")
    if not icon.is_file():
        raise PathPolicyError(f"Dedicated editor icon is not a file: {icon}")
    if not startup_level.is_file() or startup_level.suffix.casefold() != ".prefab":
        raise PathPolicyError(f"Dedicated editor startup level is not a prefab file: {startup_level}")
    levels_root = canonical_path(project / "Levels", require_exists=True)
    if not is_contained(levels_root, startup_level):
        raise PathPolicyError(
            f"Dedicated editor startup level must remain inside {levels_root}: {startup_level}"
        )
    return product, project, startup_level, icon


def _engine_paths(product_root: Path) -> tuple[Path, developer_preview.EngineLock]:
    try:
        lock = developer_preview.load_engine_lock(product_root)
        engine = developer_preview.default_engine_root(product_root, lock)
        developer_preview.validate_engine_root(engine, lock)
        developer_preview.validate_engine_pin(engine, lock)
    except RuntimeError as exc:
        raise PathPolicyError(str(exc)) from exc
    if same_path(product_root, engine) or is_contained(product_root, engine) or is_contained(engine, product_root):
        raise PathPolicyError("FOA-SDK product_root and O3DE engine_root must be separate checkouts.")
    return canonical_path(engine, require_exists=True), lock


def _workspace_paths(
    product: Path,
    *,
    require_materialized: bool,
) -> developer_preview_workspace.PreviewWorkspacePaths:
    try:
        if require_materialized:
            return developer_preview_workspace.verify_preview_workspace(product)
        return developer_preview_workspace.resolve_workspace_paths()
    except developer_preview_workspace.WorkspaceError as exc:
        raise PathPolicyError(str(exc)) from exc


def approved_build_directory(product_root: Path) -> Path:
    product = canonical_path(product_root, require_exists=True)
    return canonical_path(developer_preview.default_build_directory(product), require_exists=False)


def _require_cmake_source_binding(
    engine_root: Path,
    product_root: Path,
    build_directory: Path,
) -> None:
    cache_path = build_directory / "CMakeCache.txt"
    try:
        cache = cache_path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise PathPolicyError(f"Unable to read configured build provenance: {cache_path}: {exc}") from exc
    source_match = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$", cache, re.MULTILINE)
    if not source_match:
        raise PathPolicyError(
            "The configured Developer Preview build has no CMAKE_HOME_DIRECTORY source binding. "
            "Reconfigure the approved build directory before creating a verified entry."
        )
    configured_source = canonical_path(Path(source_match.group(1).strip()), require_exists=True)
    if not same_path(engine_root, configured_source):
        raise PathPolicyError(
            f"The approved build directory belongs to another engine source tree: {configured_source}"
        )
    projects_match = re.search(r"^LY_PROJECTS:STRING=(.*)$", cache, re.MULTILINE)
    configured_projects = {
        canonical_path(Path(value.strip()), engine_root)
        for value in (projects_match.group(1).split(";") if projects_match else ())
        if value.strip()
    }
    required_project = canonical_path(product_root / PREVIEW_PROJECT)
    if required_project not in configured_projects:
        raise PathPolicyError(
            f"The approved build is not configured for the FOA-SDK project: {required_project}"
        )


def resolve_source_built_entry(
    repo_root: Path,
    build_dir: Path | None,
    *,
    require_editor: bool,
    require_configured: bool,
) -> PreviewEntryPaths:
    product, _, _, icon = _product_paths(repo_root)
    engine, _ = _engine_paths(product)
    workspace = _workspace_paths(product, require_materialized=require_editor or require_configured)
    approved = approved_build_directory(product)
    compatibility_default = canonical_path(product / APPROVED_BUILD_DIRECTORY)
    requested = canonical_path(build_dir or approved, product)
    if same_path(requested, compatibility_default) and not same_path(approved, compatibility_default):
        requested = approved
    if not same_path(approved, requested):
        raise PathPolicyError(
            "Verified clickable entries must use the approved external Developer Preview build directory: "
            f"{approved}"
        )
    try:
        developer_preview.validate_build_directory(
            product,
            engine,
            approved,
            require_configured=require_configured,
        )
    except RuntimeError as exc:
        raise PathPolicyError(str(exc)) from exc
    if require_configured:
        _require_cmake_source_binding(engine, product, approved)

    candidates = tuple(
        require_contained(
            approved,
            approved / relative,
            label="Source-built Editor executable",
            root_must_exist=require_configured,
        )
        for relative in EDITOR_CANDIDATES
    )
    editor = next((candidate for candidate in candidates if candidate.is_file()), candidates[0])
    if require_editor and not editor.is_file():
        expected = ", ".join(str(candidate) for candidate in candidates)
        raise PathPolicyError(f"The approved source-built Editor.exe is missing. Expected one of: {expected}")
    if require_editor:
        validate_source_editor_binary(editor)
    return PreviewEntryPaths(
        trust_mode=TRUST_MODE_SOURCE_BUILD,
        repo_root=product,
        build_directory=approved,
        editor=editor,
        engine=engine,
        project=workspace.project,
        startup_level=workspace.startup_level,
        project_cache=workspace.cache,
        project_user=workspace.user,
        project_log=workspace.log,
        icon=icon,
        working_directory=editor.parent,
    )


def resolve_diagnostic_entry(repo_root: Path, explicit_editor: Path) -> PreviewEntryPaths:
    product, _, _, icon = _product_paths(repo_root)
    engine, _ = _engine_paths(product)
    workspace = _workspace_paths(product, require_materialized=True)
    editor = canonical_path(explicit_editor, product, require_exists=True)
    if not editor.is_file() or editor.name.casefold() != "editor.exe":
        raise PathPolicyError(f"Diagnostic override must identify an existing Editor.exe: {editor}")
    return PreviewEntryPaths(
        trust_mode=TRUST_MODE_DIAGNOSTIC_OVERRIDE,
        repo_root=product,
        build_directory=None,
        editor=editor,
        engine=engine,
        project=workspace.project,
        startup_level=workspace.startup_level,
        project_cache=workspace.cache,
        project_user=workspace.user,
        project_log=workspace.log,
        icon=icon,
        working_directory=editor.parent,
    )


def resolve_shortcut_output(repo_root: Path, output: Path, *, diagnostic_override: bool) -> Path:
    product = canonical_path(repo_root, require_exists=True)
    build_directory = approved_build_directory(product)
    build_root = build_directory.parent
    requested = canonical_path(output, product)
    default_output = canonical_path(product / DEFAULT_SHORTCUT_OUTPUT)
    if diagnostic_override and same_path(requested, default_output):
        raise PathPolicyError(
            "Diagnostic overrides must not replace the standard verified clickable entry; "
            f"choose an output beneath {build_root / 'diagnostic-entries'}."
        )
    root = build_root
    if diagnostic_override:
        root = build_root / "diagnostic-entries"
    return require_contained(
        root,
        requested,
        label="Shortcut output",
        root_must_exist=False,
    )
