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

import developer_preview_path_policy
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


def expected_argument_text(
    expected: developer_preview_path_policy.PreviewEntryPaths,
) -> str:
    return (
        f'--project-path "{expected.project}" '
        f'--engine-path "{expected.engine}" '
        f'--project-cache-path "{expected.project_cache}" '
        f'--project-user-path "{expected.project_user}" '
        f'--project-log-path "{expected.project_log}" '
        f'"{expected.startup_level}"'
    )


def legacy_argument_text(project: Path) -> str:
    return f'--project-path "{project.resolve(strict=False)}"'


def parse_icon_location(value: str) -> tuple[Path, int]:
    path_text, separator, index_text = value.rpartition(",")
    if not separator:
        raise EntryVerificationError(f"Shortcut icon location has no image index: {value}")
    try:
        image_index = int(index_text.strip())
    except ValueError as exc:
        raise EntryVerificationError(
            f"Shortcut icon location has an invalid image index: {value}"
        ) from exc
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
        raise EntryVerificationError(
            "Shortcut inspection did not return the required string fields."
        )
    return {key: inspected[key] for key in required}


def _require_manifest_matches_policy(
    payload: dict[str, object],
    expected: developer_preview_path_policy.PreviewEntryPaths,
    *,
    allow_legacy_project_only: bool = False,
) -> None:
    expected_arguments = [
        "--project-path",
        str(expected.project),
        "--engine-path",
        str(expected.engine),
        "--project-cache-path",
        str(expected.project_cache),
        "--project-user-path",
        str(expected.project_user),
        "--project-log-path",
        str(expected.project_log),
        str(expected.startup_level),
    ]
    fields = {
        "target": str(expected.editor),
        "working_directory": str(expected.working_directory),
        "icon": str(expected.icon),
    }
    for key, value in fields.items():
        if payload.get(key) != value:
            raise EntryVerificationError(
                f"Shortcut manifest {key} does not match repository-owned path policy."
            )
    actual_arguments = payload.get("arguments")
    source_project = expected.repo_root / developer_preview_path_policy.PREVIEW_PROJECT
    legacy_arguments = (
        ["--project-path", str(source_project)],
        [
            "--project-path",
            str(source_project),
            str(expected.repo_root / developer_preview_path_policy.PREVIEW_STARTUP_LEVEL),
        ],
    )
    if actual_arguments != expected_arguments and not (
        allow_legacy_project_only and actual_arguments in legacy_arguments
    ):
        raise EntryVerificationError(
            "Shortcut manifest arguments do not match repository-owned path policy."
        )


def verify_entry(
    shortcut: Path,
    *,
    repo_root: Path,
    allow_diagnostic_override: bool = False,
    allow_legacy_project_only: bool = False,
    runner: PowerShellRunner = default_runner,
) -> dict[str, object]:
    shortcut = shortcut.resolve(strict=False)
    payload = developer_preview_shortcut.verify_shortcut(shortcut)
    trust_mode = payload.get("trust_mode")
    try:
        if trust_mode == developer_preview_path_policy.TRUST_MODE_SOURCE_BUILD:
            expected = developer_preview_path_policy.resolve_source_built_entry(
                repo_root,
                developer_preview_path_policy.APPROVED_BUILD_DIRECTORY,
                require_editor=True,
                require_configured=True,
            )
        elif trust_mode == developer_preview_path_policy.TRUST_MODE_DIAGNOSTIC_OVERRIDE:
            if not allow_diagnostic_override:
                raise EntryVerificationError(
                    "Diagnostic override shortcuts are not verified source-built entries. "
                    "Pass --allow-diagnostic-override only for deliberate diagnostics."
                )
            target = payload.get("target")
            if not isinstance(target, str) or not target:
                raise EntryVerificationError("Diagnostic shortcut manifest target is invalid.")
            expected = developer_preview_path_policy.resolve_diagnostic_entry(
                repo_root,
                Path(target),
            )
        else:
            raise EntryVerificationError("Shortcut manifest trust mode is unsupported.")
    except developer_preview_path_policy.PathPolicyError as exc:
        raise EntryVerificationError(str(exc)) from exc

    _require_manifest_matches_policy(
        payload,
        expected,
        allow_legacy_project_only=allow_legacy_project_only,
    )
    inspected = inspect_shortcut(shortcut, runner=runner)
    actual_target = Path(inspected["target"]).resolve(strict=False)
    actual_working = Path(inspected["working_directory"]).resolve(strict=False)
    actual_icon, actual_icon_index = parse_icon_location(inspected["icon"])
    if actual_target != expected.editor:
        raise EntryVerificationError(f"Shortcut target mismatch: {actual_target}")
    expected_arguments = expected_argument_text(expected)
    actual_arguments = inspected["arguments"]
    source_project = expected.repo_root / developer_preview_path_policy.PREVIEW_PROJECT
    legacy_argument_texts = (
        legacy_argument_text(source_project),
        (
            f'{legacy_argument_text(source_project)} '
            f'"{expected.repo_root / developer_preview_path_policy.PREVIEW_STARTUP_LEVEL}"'
        ),
    )
    if actual_arguments != expected_arguments and not (
        allow_legacy_project_only
        and actual_arguments in legacy_argument_texts
    ):
        raise EntryVerificationError(f"Shortcut argument mismatch: {inspected['arguments']}")
    if actual_working != expected.working_directory:
        raise EntryVerificationError(f"Shortcut working-directory mismatch: {actual_working}")
    if actual_icon != expected.icon or actual_icon_index != 0:
        raise EntryVerificationError(f"Shortcut icon mismatch: {inspected['icon']}")
    if inspected["description"] != SHORTCUT_DESCRIPTION:
        raise EntryVerificationError(
            f"Shortcut description mismatch: {inspected['description']}"
        )

    if trust_mode == developer_preview_path_policy.TRUST_MODE_SOURCE_BUILD:
        print(
            "Verified source-built clickable entry from repository-owned policy: "
            f"{shortcut}"
        )
    else:
        print(
            "Validated diagnostic-only clickable entry; not a verified release entry: "
            f"{shortcut}"
        )
    return payload


def create_entry(
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
    output = output.resolve(strict=False)
    if output.exists() and replace:
        verify_entry(
            output,
            repo_root=repo_root,
            allow_diagnostic_override=diagnostic_override,
            allow_legacy_project_only=True,
            runner=runner,
        )

    payload = developer_preview_shortcut.create_shortcut(
        repo_root=repo_root,
        build_dir=build_dir,
        explicit_editor=explicit_editor,
        output=output,
        replace=replace,
        dry_run=dry_run,
        diagnostic_override=diagnostic_override,
        runner=runner,
    )
    if not dry_run:
        verify_entry(
            output,
            repo_root=repo_root,
            allow_diagnostic_override=diagnostic_override,
            runner=runner,
        )
    return payload


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    create = subparsers.add_parser(
        "create",
        help="Create the verified Windows clickable entry.",
    )
    create.add_argument("--repo-root", type=Path)
    source = create.add_mutually_exclusive_group()
    source.add_argument("--editor", type=Path)
    source.add_argument(
        "--build-dir",
        type=Path,
        default=developer_preview_shortcut.DEFAULT_BUILD_DIRECTORY,
    )
    create.add_argument("--diagnostic-override", action="store_true")
    create.add_argument(
        "--output",
        type=Path,
        default=developer_preview_shortcut.DEFAULT_OUTPUT,
    )
    create.add_argument("--replace", action="store_true")
    create.add_argument("--dry-run", action="store_true")

    verify = subparsers.add_parser(
        "verify",
        help="Verify the clickable entry and its manifest.",
    )
    verify.add_argument("--repo-root", type=Path)
    verify.add_argument(
        "--shortcut",
        type=Path,
        default=developer_preview_shortcut.DEFAULT_OUTPUT,
    )
    verify.add_argument("--allow-diagnostic-override", action="store_true")
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
            verify_entry(
                developer_preview_shortcut.resolve_path(args.shortcut, repo_root),
                repo_root=repo_root,
                allow_diagnostic_override=args.allow_diagnostic_override,
            )
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
            diagnostic_override=args.diagnostic_override,
        )
        return 0
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"Developer Preview clickable entry failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
