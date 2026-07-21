#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Create and hash a Windows shortcut under the canonical TG SDK path policy."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Callable, Mapping, Sequence

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import developer_preview_path_policy
import developer_preview_assets
import developer_preview_workspace
import validate_developer_preview_project

SCHEMA_VERSION = 1
SHORTCUT_NAME = "Tainted Grail Modding Editor.lnk"
MANIFEST_NAME = "Tainted Grail Modding Editor.shortcut.json"
PREVIEW_PROJECT = developer_preview_path_policy.PREVIEW_PROJECT
ICON_PATH = developer_preview_path_policy.PREVIEW_ICON
DEFAULT_BUILD_DIRECTORY = developer_preview_path_policy.APPROVED_BUILD_DIRECTORY
DEFAULT_OUTPUT = developer_preview_path_policy.DEFAULT_SHORTCUT_OUTPUT

PowerShellRunner = Callable[[Sequence[str], Mapping[str, str]], tuple[int, str]]

POWERSHELL_CREATE_SCRIPT = r"""
$ErrorActionPreference = 'Stop'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($env:TG_SHORTCUT_OUTPUT)
$shortcut.TargetPath = $env:TG_EDITOR_EXE
$shortcut.Arguments = '--project-path "' + $env:TG_PROJECT_PATH + '"' +
    ' --engine-path "' + $env:TG_ENGINE_PATH + '"' +
    ' --project-cache-path "' + $env:TG_PROJECT_CACHE_PATH + '"' +
    ' --project-user-path "' + $env:TG_PROJECT_USER_PATH + '"' +
    ' --project-log-path "' + $env:TG_PROJECT_LOG_PATH + '"' +
    ' "' + $env:TG_STARTUP_LEVEL + '"'
$shortcut.WorkingDirectory = $env:TG_EDITOR_WORKING_DIRECTORY
$shortcut.IconLocation = $env:TG_SHORTCUT_ICON + ',0'
$shortcut.Description = 'Tainted Grail Modding Editor'
$shortcut.Save()
"""


class ShortcutError(RuntimeError):
    """Raised when a shortcut cannot be created or hash-verified safely."""


def repository_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def resolve_path(value: Path, base: Path) -> Path:
    return developer_preview_path_policy.canonical_path(value, base)


def require_windows_x64() -> None:
    if platform.system() != "Windows" or platform.machine().lower() not in {"amd64", "x86_64"}:
        raise ShortcutError(
            "Shortcut creation is supported only on Windows x64; "
            f"detected {platform.system()} {platform.machine()}."
        )


def resolve_powershell() -> str:
    executable = shutil.which("pwsh") or shutil.which("powershell")
    if not executable:
        raise ShortcutError("PowerShell was not found on PATH.")
    return executable


def validate_output_path(output: Path) -> None:
    if output.suffix.casefold() != ".lnk":
        raise ShortcutError("Shortcut output must use the .lnk extension.")
    if output.is_symlink():
        raise ShortcutError(f"Shortcut output must not be a symbolic link: {output}")
    if output.exists() and not output.is_file():
        raise ShortcutError(f"Shortcut output is not a regular file: {output}")


def default_runner(command: Sequence[str], environment: Mapping[str, str]) -> tuple[int, str]:
    completed = subprocess.run(
        list(command),
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=dict(environment),
    )
    return int(completed.returncode), completed.stdout.strip()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def atomic_write_json(path: Path, payload: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        newline="\n",
        dir=path.parent,
        prefix=f".{path.name}.",
        suffix=".tmp",
        delete=False,
    ) as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
        stream.write("\n")
        temporary = Path(stream.name)
    temporary.replace(path)


def manifest_path_for(shortcut: Path) -> Path:
    return shortcut.with_name(f"{shortcut.stem}.shortcut.json")


def create_shortcut(
    *,
    repo_root: Path,
    build_dir: Path,
    explicit_editor: Path | None,
    output: Path,
    replace: bool,
    dry_run: bool,
    diagnostic_override: bool = False,
    runner: PowerShellRunner = default_runner,
) -> dict[str, object]:
    validate_developer_preview_project.validate_preview_project(repo_root)
    try:
        workspace = developer_preview_workspace.materialize_preview_workspace(
            repo_root,
            dry_run=dry_run,
        )
        if explicit_editor is not None:
            if not diagnostic_override:
                raise ShortcutError(
                    "An explicit --editor is a diagnostic override; pass --diagnostic-override "
                    "and choose a diagnostic output path."
                )
            paths = developer_preview_path_policy.resolve_diagnostic_entry(
                repo_root,
                explicit_editor,
            )
        else:
            if diagnostic_override:
                raise ShortcutError("--diagnostic-override requires an explicit --editor.")
            paths = developer_preview_path_policy.resolve_source_built_entry(
                repo_root,
                build_dir,
                require_editor=not dry_run,
                require_configured=not dry_run,
            )
        output = developer_preview_path_policy.resolve_shortcut_output(
            repo_root,
            output,
            diagnostic_override=diagnostic_override,
        )
        developer_preview_assets.prepare_assets(
            editor=paths.editor,
            repo_root=repo_root,
            workspace=workspace,
            dry_run=dry_run,
        )
    except (
        developer_preview_assets.AssetPreparationError,
        developer_preview_path_policy.PathPolicyError,
        developer_preview_workspace.WorkspaceError,
    ) as exc:
        raise ShortcutError(str(exc)) from exc
    validate_output_path(output)

    payload: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "status": "planned" if dry_run else "created",
        "trust_mode": paths.trust_mode,
        "shortcut": str(output),
        "target": str(paths.editor),
        "arguments": [
            "--project-path",
            str(paths.project),
            "--engine-path",
            str(paths.engine),
            "--project-cache-path",
            str(paths.project_cache),
            "--project-user-path",
            str(paths.project_user),
            "--project-log-path",
            str(paths.project_log),
            str(paths.startup_level),
        ],
        "working_directory": str(paths.working_directory),
        "icon": str(paths.icon),
    }
    if paths.build_directory is not None:
        payload["approved_build_directory"] = str(paths.build_directory)

    print(f"Shortcut: {output}")
    print(f"Trust mode: {paths.trust_mode}")
    print(f"Target: {paths.editor}")
    print(
        f'Arguments: --project-path "{paths.project}" --engine-path "{paths.engine}" '
        f'--project-cache-path "{paths.project_cache}" '
        f'--project-user-path "{paths.project_user}" '
        f'--project-log-path "{paths.project_log}" "{paths.startup_level}"'
    )
    print(f"Icon: {paths.icon}")

    if dry_run:
        return payload

    require_windows_x64()
    if output.exists():
        if not replace:
            raise ShortcutError(f"Shortcut already exists; pass --replace to overwrite it: {output}")
        output.unlink()

    output.parent.mkdir(parents=True, exist_ok=True)
    powershell = resolve_powershell()
    environment = os.environ.copy()
    environment.update(
        {
            "TG_SHORTCUT_OUTPUT": str(output),
            "TG_EDITOR_EXE": str(paths.editor),
            "TG_PROJECT_PATH": str(paths.project),
            "TG_ENGINE_PATH": str(paths.engine),
            "TG_PROJECT_CACHE_PATH": str(paths.project_cache),
            "TG_PROJECT_USER_PATH": str(paths.project_user),
            "TG_PROJECT_LOG_PATH": str(paths.project_log),
            "TG_STARTUP_LEVEL": str(paths.startup_level),
            "TG_EDITOR_WORKING_DIRECTORY": str(paths.working_directory),
            "TG_SHORTCUT_ICON": str(paths.icon),
        }
    )
    command = (
        powershell,
        "-NoLogo",
        "-NoProfile",
        "-NonInteractive",
        "-ExecutionPolicy",
        "Bypass",
        "-Command",
        POWERSHELL_CREATE_SCRIPT,
    )
    exit_code, output_text = runner(command, environment)
    if exit_code != 0:
        raise ShortcutError(
            f"PowerShell failed to create the shortcut with exit code {exit_code}: {output_text}"
        )
    if not output.is_file() or output.stat().st_size == 0:
        raise ShortcutError(f"PowerShell did not create a usable shortcut: {output}")

    payload["size_bytes"] = output.stat().st_size
    payload["sha256"] = sha256_file(output)
    manifest = manifest_path_for(output)
    atomic_write_json(manifest, payload)
    print(f"Created clickable shortcut: {output}")
    print(f"Wrote shortcut manifest: {manifest}")
    return payload


def verify_shortcut(shortcut: Path) -> dict[str, object]:
    shortcut = shortcut.resolve(strict=False)
    validate_output_path(shortcut)
    manifest = manifest_path_for(shortcut)
    if not shortcut.is_file():
        raise ShortcutError(f"Shortcut is missing: {shortcut}")
    if not manifest.is_file():
        raise ShortcutError(f"Shortcut manifest is missing: {manifest}")
    try:
        payload = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ShortcutError(f"Shortcut manifest is invalid: {manifest}: {exc}") from exc
    if not isinstance(payload, dict):
        raise ShortcutError("Shortcut manifest must be a JSON object.")
    if payload.get("schema_version") != SCHEMA_VERSION:
        raise ShortcutError("Shortcut manifest schema version is unsupported.")
    if payload.get("trust_mode") not in {
        developer_preview_path_policy.TRUST_MODE_SOURCE_BUILD,
        developer_preview_path_policy.TRUST_MODE_DIAGNOSTIC_OVERRIDE,
    }:
        raise ShortcutError("Shortcut manifest trust mode is invalid.")
    if payload.get("shortcut") != str(shortcut):
        raise ShortcutError("Shortcut manifest path does not match the selected shortcut.")
    if payload.get("size_bytes") != shortcut.stat().st_size:
        raise ShortcutError("Shortcut size does not match its manifest.")
    if payload.get("sha256") != sha256_file(shortcut):
        raise ShortcutError("Shortcut SHA-256 does not match its manifest.")
    print(f"Verified clickable shortcut hash: {shortcut}")
    return payload


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    create = subparsers.add_parser("create", help="Create the Windows .lnk entry.")
    create.add_argument("--repo-root", type=Path)
    source = create.add_mutually_exclusive_group()
    source.add_argument("--editor", type=Path)
    source.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD_DIRECTORY)
    create.add_argument(
        "--diagnostic-override",
        action="store_true",
        help="Label an explicit external Editor.exe as diagnostic-only, never verified release entry.",
    )
    create.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    create.add_argument("--replace", action="store_true")
    create.add_argument("--dry-run", action="store_true")

    verify = subparsers.add_parser("verify", help="Verify the generated shortcut hash manifest.")
    verify.add_argument("--shortcut", type=Path, default=DEFAULT_OUTPUT)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    repo_root = resolve_path(
        args.repo_root if getattr(args, "repo_root", None) else repository_root_from_script(),
        Path.cwd(),
    )
    try:
        if args.command == "verify":
            verify_shortcut(resolve_path(args.shortcut, repo_root))
            return 0

        create_shortcut(
            repo_root=repo_root,
            build_dir=resolve_path(args.build_dir, repo_root),
            explicit_editor=resolve_path(args.editor, repo_root) if args.editor else None,
            output=resolve_path(args.output, repo_root),
            replace=args.replace,
            dry_run=args.dry_run,
            diagnostic_override=args.diagnostic_override,
        )
        return 0
    except (
        OSError,
        RuntimeError,
        ValueError,
        validate_developer_preview_project.PreviewProjectContractError,
    ) as exc:
        print(f"Developer Preview shortcut failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
