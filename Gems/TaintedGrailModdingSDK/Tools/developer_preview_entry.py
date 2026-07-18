#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Create and verify the safe clickable entry for the dedicated TG editor."""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Callable, Mapping, Sequence

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import developer_preview_shortcut

SHORTCUT_DESCRIPTION = "Tainted Grail Modding Editor"

PowerShellRunner = Callable[[Sequence[str], Mapping[str, str]], tuple[int, str]]

POWERSHELL_INSPECT_SCRIPT = r"""
$ErrorActionPreference = 'Stop'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($env:TG_SHORTCUT_OUTPUT)
[ordered]@{
    target = $shortcut.TargetPath
    arguments = $shortcut.Arguments
    working_directory = $shortcut.WorkingDirectory
    icon = $shortcut.IconLocation
    description = $shortcut.Description
} | ConvertTo-Json -Compress
"""


class EntryVerificationError(RuntimeError):
    """Raised when a generated Windows shortcut is unsafe or inconsistent."""


def require_windows_x64() -> None:
    if platform.system() != "Windows" or platform.machine().lower() not in {"amd64", "x86_64"}:
        raise EntryVerificationError(
            "Clickable-entry verification is supported only on Windows x64; "
            f"detected {platform.system()} {platform.machine()}."
        )


def resolve_powershell() -> str:
    executable = shutil.which("pwsh") or shutil.which("powershell")
    if not executable:
        raise EntryVerificationError("PowerShell was not found on PATH.")
    return executable


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


def expected_argument_text(project: Path) -> str:
    return f'--project-path "{project.resolve(strict=False)}"'


def parse_icon_location(value: str) -> tuple[Path, int]:
    path_text, separator, index_text = value.rpartition(",")
    if not separator:
        raise EntryVerificationError(f"Shortcut icon location has no image index: {value}")
    try:
        image_index = int(index_text.strip())
    except ValueError as exc:
        raise EntryVerificationError(f"Shortcut icon location has an invalid image index: {value}") from exc
    return Path(path_text.strip().strip('"')).resolve(strict=False), image_index


def inspect_shortcut(
    shortcut: Path,
    *,
    runner: PowerShellRunner = default_runner,
) -> dict[str, str]:
    require_windows_x64()
    environment = os.environ.copy()
    environment["TG_SHORTCUT_OUTPUT"] = str(shortcut.resolve(strict=False))
    command = (
        resolve_powershell(),
        "-NoLogo",
        "-NoProfile",
        "-NonInteractive",
        "-ExecutionPolicy",
        "Bypass",
        "-Command",
        POWERSHELL_INSPECT_SCRIPT,
    )
    exit_code, output_text = runner(command, environment)
    if exit_code != 0:
        raise EntryVerificationError(
            f"PowerShell failed to inspect the shortcut with exit code {exit_code}: {output_text}"
        )
    try:
        inspected = json.loads(output_text)
    except json.JSONDecodeError as exc:
        raise EntryVerificationError(
            f"PowerShell returned invalid shortcut inspection JSON: {output_text}"
        ) from exc
    required = ("target", "arguments", "working_directory", "icon", "description")
    if not isinstance(inspected, dict) or any(
        not isinstance(inspected.get(key), str) for key in required
    ):
        raise EntryVerificationError("Shortcut inspection did not return the required string fields.")
    return {key: inspected[key] for key in required}


def verify_entry(
    shortcut: Path,
    *,
    runner: PowerShellRunner = default_runner,
) -> dict[str, object]:
    shortcut = shortcut.resolve(strict=False)
    payload = developer_preview_shortcut.verify_shortcut(shortcut)

    target_value = payload.get("target")
    arguments_value = payload.get("arguments")
    working_value = payload.get("working_directory")
    icon_value = payload.get("icon")
    if not isinstance(target_value, str) or not target_value:
        raise EntryVerificationError("Shortcut manifest target is invalid.")
    if (
        not isinstance(arguments_value, list)
        or len(arguments_value) != 2
        or arguments_value[0] != "--project-path"
        or not isinstance(arguments_value[1], str)
    ):
        raise EntryVerificationError("Shortcut manifest project arguments are invalid.")
    if not isinstance(working_value, str) or not working_value:
        raise EntryVerificationError("Shortcut manifest working directory is invalid.")
    if not isinstance(icon_value, str) or not icon_value:
        raise EntryVerificationError("Shortcut manifest icon is invalid.")

    expected_target = Path(target_value).resolve(strict=False)
    expected_project = Path(arguments_value[1]).resolve(strict=False)
    expected_working = Path(working_value).resolve(strict=False)
    expected_icon = Path(icon_value).resolve(strict=False)
    for label, path in (
        ("target", expected_target),
        ("project", expected_project),
        ("working directory", expected_working),
        ("icon", expected_icon),
    ):
        if not path.exists():
            raise EntryVerificationError(f"Shortcut {label} no longer exists: {path}")

    inspected = inspect_shortcut(shortcut, runner=runner)
    actual_target = Path(inspected["target"]).resolve(strict=False)
    actual_working = Path(inspected["working_directory"]).resolve(strict=False)
    actual_icon, actual_icon_index = parse_icon_location(inspected["icon"])
    if actual_target != expected_target:
        raise EntryVerificationError(f"Shortcut target mismatch: {actual_target}")
    if inspected["arguments"] != expected_argument_text(expected_project):
        raise EntryVerificationError(f"Shortcut argument mismatch: {inspected['arguments']}")
    if actual_working != expected_working:
        raise EntryVerificationError(f"Shortcut working-directory mismatch: {actual_working}")
    if actual_icon != expected_icon or actual_icon_index != 0:
        raise EntryVerificationError(f"Shortcut icon mismatch: {inspected['icon']}")
    if inspected["description"] != SHORTCUT_DESCRIPTION:
        raise EntryVerificationError(f"Shortcut description mismatch: {inspected['description']}")

    print(f"Verified clickable entry target, arguments, working directory, icon, and hash: {shortcut}")
    return payload


def create_entry(
    *,
    repo_root: Path,
    build_dir: Path,
    explicit_editor: Path | None,
    output: Path,
    replace: bool,
    dry_run: bool,
    runner: PowerShellRunner = default_runner,
) -> dict[str, object]:
    output = output.resolve(strict=False)
    if output.exists() and replace:
        verify_entry(output, runner=runner)

    payload = developer_preview_shortcut.create_shortcut(
        repo_root=repo_root,
        build_dir=build_dir,
        explicit_editor=explicit_editor,
        output=output,
        replace=replace,
        dry_run=dry_run,
        runner=runner,
    )
    if not dry_run:
        verify_entry(output, runner=runner)
    return payload


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    create = subparsers.add_parser("create", help="Create the verified Windows clickable entry.")
    create.add_argument("--repo-root", type=Path)
    source = create.add_mutually_exclusive_group()
    source.add_argument("--editor", type=Path)
    source.add_argument(
        "--build-dir",
        type=Path,
        default=developer_preview_shortcut.DEFAULT_BUILD_DIRECTORY,
    )
    create.add_argument("--output", type=Path, default=developer_preview_shortcut.DEFAULT_OUTPUT)
    create.add_argument("--replace", action="store_true")
    create.add_argument("--dry-run", action="store_true")

    verify = subparsers.add_parser("verify", help="Verify the clickable entry and its manifest.")
    verify.add_argument("--shortcut", type=Path, default=developer_preview_shortcut.DEFAULT_OUTPUT)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    repo_root = developer_preview_shortcut.resolve_path(
        args.repo_root
        if getattr(args, "repo_root", None)
        else developer_preview_shortcut.repository_root_from_script(),
        Path.cwd(),
    )
    try:
        if args.command == "verify":
            verify_entry(developer_preview_shortcut.resolve_path(args.shortcut, repo_root))
            return 0

        create_entry(
            repo_root=repo_root,
            build_dir=developer_preview_shortcut.resolve_path(args.build_dir, repo_root),
            explicit_editor=(
                developer_preview_shortcut.resolve_path(args.editor, repo_root)
                if args.editor
                else None
            ),
            output=developer_preview_shortcut.resolve_path(args.output, repo_root),
            replace=args.replace,
            dry_run=args.dry_run,
        )
        return 0
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"Developer Preview clickable entry failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
