#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Materialize the dedicated Editor project in bounded per-user storage."""

from __future__ import annotations

import hashlib
import json
import os
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping

SCHEMA_VERSION = 1
PREVIEW_PROJECT = Path("TaintedGrailModdingEditor")
PREVIEW_STARTUP_LEVEL = Path("Levels/DefaultLevel/DefaultLevel.prefab")
WORKSPACE_RELATIVE_ROOT = Path("O3DE/TGEditor")
PROJECT_DIRECTORY = Path("project")
CACHE_DIRECTORY = PROJECT_DIRECTORY / "Cache"
USER_DIRECTORY = PROJECT_DIRECTORY / "user"
LOG_DIRECTORY = USER_DIRECTORY / "log"
LAUNCHER_LOG_DIRECTORY = Path("launcher")
MANIFEST_NAME = ".tg-preview-materialization.json"

MANAGED_PROJECT_FILES = (
    Path("CMakeLists.txt"),
    Path("ShaderLib/scenesrg.srgi"),
    Path("ShaderLib/viewsrg.srgi"),
    Path("TaintedGrailModdingEditor.ico"),
    Path("cmake/EngineFinder.cmake"),
    Path("preview.png"),
    Path("project.json"),
)
PRESERVED_PROJECT_FILES = (PREVIEW_STARTUP_LEVEL,)


class WorkspaceError(RuntimeError):
    """Raised when the per-user project materialization is unsafe or invalid."""


@dataclass(frozen=True)
class PreviewWorkspacePaths:
    root: Path
    project: Path
    startup_level: Path
    cache: Path
    user: Path
    log: Path
    launcher_log: Path
    manifest: Path


def _is_contained(root: Path, candidate: Path) -> bool:
    root_parts = root.parts
    candidate_parts = candidate.parts
    if len(candidate_parts) < len(root_parts):
        return False
    if os.name == "nt":
        return all(a.casefold() == b.casefold() for a, b in zip(root_parts, candidate_parts))
    return candidate_parts[: len(root_parts)] == root_parts


def default_local_app_data(environment: Mapping[str, str] | None = None) -> Path:
    values = os.environ if environment is None else environment
    raw_value = values.get("LOCALAPPDATA", "").strip()
    if not raw_value:
        raise WorkspaceError(
            "LOCALAPPDATA is unavailable; the Developer Preview cannot select bounded per-user storage."
        )
    local_app_data = Path(raw_value)
    if not local_app_data.is_absolute():
        raise WorkspaceError(f"LOCALAPPDATA must be an absolute path: {local_app_data}")
    try:
        local_app_data = local_app_data.resolve(strict=True)
    except OSError as exc:
        raise WorkspaceError(f"Unable to resolve LOCALAPPDATA {local_app_data}: {exc}") from exc
    if not local_app_data.is_dir():
        raise WorkspaceError(f"LOCALAPPDATA is not a directory: {local_app_data}")
    return local_app_data


def resolve_workspace_paths(
    *,
    environment: Mapping[str, str] | None = None,
) -> PreviewWorkspacePaths:
    local_app_data = default_local_app_data(environment)
    root = (local_app_data / WORKSPACE_RELATIVE_ROOT).resolve(strict=False)
    if not _is_contained(local_app_data, root):
        raise WorkspaceError(f"Developer Preview storage escaped LOCALAPPDATA: {root}")
    project = root / PROJECT_DIRECTORY
    return PreviewWorkspacePaths(
        root=root,
        project=project,
        startup_level=project / PREVIEW_STARTUP_LEVEL,
        cache=root / CACHE_DIRECTORY,
        user=root / USER_DIRECTORY,
        log=root / LOG_DIRECTORY,
        launcher_log=root / LAUNCHER_LOG_DIRECTORY,
        manifest=root / MANIFEST_NAME,
    )


def _require_regular_source(source_project: Path, relative: Path) -> Path:
    source = source_project / relative
    if source.is_symlink() or not source.is_file():
        raise WorkspaceError(f"Managed Developer Preview source is not a regular file: {source}")
    resolved_project = source_project.resolve(strict=True)
    resolved_source = source.resolve(strict=True)
    if not _is_contained(resolved_project, resolved_source):
        raise WorkspaceError(f"Managed Developer Preview source escaped the project: {resolved_source}")
    return resolved_source


def _require_safe_destination(paths: PreviewWorkspacePaths, destination: Path) -> None:
    resolved_root = paths.root.resolve(strict=True)
    current = destination
    while _is_contained(paths.root, current):
        if current.is_symlink():
            raise WorkspaceError(
                f"Developer Preview destination must not contain a symbolic link: {current}"
            )
        if current == paths.root:
            break
        current = current.parent
    resolved_destination = destination.resolve(strict=False)
    if not _is_contained(resolved_root, resolved_destination):
        raise WorkspaceError(f"Developer Preview destination escaped per-user storage: {resolved_destination}")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _load_manifest(path: Path) -> dict[str, object] | None:
    if not path.exists():
        return None
    if path.is_symlink() or not path.is_file():
        raise WorkspaceError(f"Developer Preview materialization manifest is unsafe: {path}")
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise WorkspaceError(f"Developer Preview materialization manifest is invalid: {path}: {exc}") from exc
    if not isinstance(payload, dict) or payload.get("schema_version") != SCHEMA_VERSION:
        raise WorkspaceError(f"Developer Preview materialization manifest schema is unsupported: {path}")
    managed = payload.get("managed_files")
    if not isinstance(managed, dict) or any(
        not isinstance(key, str) or not isinstance(value, str)
        for key, value in managed.items()
    ):
        raise WorkspaceError(f"Developer Preview materialization manifest files are invalid: {path}")
    return payload


def _atomic_copy(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{destination.name}.",
        suffix=".tmp",
        dir=destination.parent,
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as output, source.open("rb") as input_stream:
            for chunk in iter(lambda: input_stream.read(1024 * 1024), b""):
                output.write(chunk)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, destination)
    finally:
        if temporary.exists():
            temporary.unlink()


def _atomic_write_json(path: Path, payload: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=path.parent,
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(payload, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def materialize_preview_workspace(
    repo_root: Path,
    *,
    environment: Mapping[str, str] | None = None,
    dry_run: bool = False,
) -> PreviewWorkspacePaths:
    repo = repo_root.resolve(strict=True)
    source_project = (repo / PREVIEW_PROJECT).resolve(strict=True)
    if not _is_contained(repo, source_project) or not source_project.is_dir():
        raise WorkspaceError(f"Dedicated Developer Preview source project is invalid: {source_project}")

    sources = {
        relative.as_posix(): _require_regular_source(source_project, relative)
        for relative in (*MANAGED_PROJECT_FILES, *PRESERVED_PROJECT_FILES)
    }
    paths = resolve_workspace_paths(environment=environment)
    if dry_run:
        return paths

    paths.root.mkdir(parents=True, exist_ok=True)
    if not _is_contained(default_local_app_data(environment), paths.root.resolve(strict=True)):
        raise WorkspaceError(f"Developer Preview storage escaped LOCALAPPDATA: {paths.root}")
    for directory in (paths.project, paths.cache, paths.user, paths.log, paths.launcher_log):
        _require_safe_destination(paths, directory)
        directory.mkdir(parents=True, exist_ok=True)
        _require_safe_destination(paths, directory)

    previous = _load_manifest(paths.manifest)
    previous_hashes = previous.get("managed_files", {}) if previous else {}
    assert isinstance(previous_hashes, dict)
    current_hashes: dict[str, str] = {}

    for relative in MANAGED_PROJECT_FILES:
        key = relative.as_posix()
        source = sources[key]
        destination = paths.project / relative
        _require_safe_destination(paths, destination)
        source_hash = sha256_file(source)
        if destination.exists():
            if destination.is_symlink() or not destination.is_file():
                raise WorkspaceError(f"Managed Developer Preview destination is unsafe: {destination}")
            destination_hash = sha256_file(destination)
            previous_hash = previous_hashes.get(key)
            if destination_hash != source_hash and destination_hash != previous_hash:
                raise WorkspaceError(
                    "Managed Developer Preview file was modified outside the materializer; "
                    f"refusing to overwrite it: {destination}"
                )
        if not destination.exists() or sha256_file(destination) != source_hash:
            _atomic_copy(source, destination)
        current_hashes[key] = source_hash

    for relative in PRESERVED_PROJECT_FILES:
        key = relative.as_posix()
        source = sources[key]
        destination = paths.project / relative
        _require_safe_destination(paths, destination)
        if destination.exists():
            if destination.is_symlink() or not destination.is_file():
                raise WorkspaceError(f"Preserved Developer Preview destination is unsafe: {destination}")
        else:
            _atomic_copy(source, destination)

    _require_safe_destination(paths, paths.manifest)
    _atomic_write_json(
        paths.manifest,
        {
            "schema_version": SCHEMA_VERSION,
            "managed_files": current_hashes,
            "preserved_files": [relative.as_posix() for relative in PRESERVED_PROJECT_FILES],
        },
    )
    return paths


def verify_preview_workspace(
    repo_root: Path,
    *,
    environment: Mapping[str, str] | None = None,
) -> PreviewWorkspacePaths:
    repo = repo_root.resolve(strict=True)
    source_project = (repo / PREVIEW_PROJECT).resolve(strict=True)
    paths = resolve_workspace_paths(environment=environment)
    if not paths.root.is_dir() or paths.root.is_symlink():
        raise WorkspaceError(f"Developer Preview per-user storage is missing or unsafe: {paths.root}")
    manifest = _load_manifest(paths.manifest)
    if manifest is None:
        raise WorkspaceError(f"Developer Preview materialization manifest is missing: {paths.manifest}")
    managed_hashes = manifest["managed_files"]
    assert isinstance(managed_hashes, dict)
    expected_keys = {relative.as_posix() for relative in MANAGED_PROJECT_FILES}
    if set(managed_hashes) != expected_keys:
        raise WorkspaceError("Developer Preview materialization manifest has an unexpected managed-file set.")

    for directory in (paths.project, paths.cache, paths.user, paths.log, paths.launcher_log):
        if not directory.is_dir() or directory.is_symlink():
            raise WorkspaceError(f"Developer Preview writable directory is missing or unsafe: {directory}")
        _require_safe_destination(paths, directory)
    for relative in MANAGED_PROJECT_FILES:
        key = relative.as_posix()
        source = _require_regular_source(source_project, relative)
        destination = paths.project / relative
        _require_safe_destination(paths, destination)
        if destination.is_symlink() or not destination.is_file():
            raise WorkspaceError(f"Managed Developer Preview file is missing or unsafe: {destination}")
        expected_hash = sha256_file(source)
        if managed_hashes.get(key) != expected_hash or sha256_file(destination) != expected_hash:
            raise WorkspaceError(f"Managed Developer Preview file is stale or modified: {destination}")
    if paths.startup_level.is_symlink() or not paths.startup_level.is_file():
        raise WorkspaceError(f"Developer Preview startup level is missing or unsafe: {paths.startup_level}")
    _require_safe_destination(paths, paths.startup_level)
    return paths
