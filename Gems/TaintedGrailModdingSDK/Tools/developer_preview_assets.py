#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Prepare the bounded Developer Preview asset cache before opening the Editor."""

from __future__ import annotations

import os
import shlex
import subprocess
from pathlib import Path
from typing import Callable, Sequence, TextIO

import developer_preview_workspace

ASSET_PROCESSOR_BATCH_FILENAME = "AssetProcessorBatch.exe"
ASSET_PLATFORM = "pc"
STDOUT_FILENAME = "asset-processor-batch.stdout.log"
STDERR_FILENAME = "asset-processor-batch.stderr.log"

ProcessExecutor = Callable[[Sequence[str], Path, TextIO, TextIO], int]


class AssetPreparationError(RuntimeError):
    """Raised when the clean-first-run asset preparation cannot complete."""


def command_text(command: Sequence[str]) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(list(command))
    return shlex.join(command)


def resolve_asset_processor_batch(editor: Path, *, require_exists: bool) -> Path:
    batch = editor.parent / ASSET_PROCESSOR_BATCH_FILENAME
    if batch.name.casefold() != ASSET_PROCESSOR_BATCH_FILENAME.casefold():
        raise AssetPreparationError("The Asset Processor Batch executable name is invalid.")
    if require_exists and not batch.is_file():
        raise AssetPreparationError(
            "The Developer Preview asset preflight is not built. Expected "
            f"{batch}. Run the preview build command first."
        )
    return batch


def asset_processor_command(
    batch: Path,
    repo_root: Path,
    workspace: developer_preview_workspace.PreviewWorkspacePaths,
) -> tuple[str, ...]:
    return (
        str(batch),
        "--project-path",
        str(workspace.project),
        "--engine-path",
        str(repo_root),
        "--project-cache-path",
        str(workspace.cache),
        "--project-user-path",
        str(workspace.user),
        "--project-log-path",
        str(workspace.log),
        f"--platforms={ASSET_PLATFORM}",
    )


def default_executor(
    command: Sequence[str],
    cwd: Path,
    stdout: TextIO,
    stderr: TextIO,
) -> int:
    completed = subprocess.run(
        list(command),
        cwd=str(cwd),
        check=False,
        stdout=stdout,
        stderr=stderr,
        env=os.environ.copy(),
    )
    return int(completed.returncode)


def prepare_assets(
    *,
    editor: Path,
    repo_root: Path,
    workspace: developer_preview_workspace.PreviewWorkspacePaths,
    dry_run: bool,
    executor: ProcessExecutor = default_executor,
) -> tuple[str, ...]:
    """Process the local project assets synchronously before Editor startup."""

    batch = resolve_asset_processor_batch(editor, require_exists=not dry_run)
    command = asset_processor_command(batch, repo_root, workspace)
    print(f"Preparing the bounded Developer Preview asset cache:\n+ {command_text(command)}")
    if dry_run:
        return command

    if not (repo_root / "engine.json").is_file():
        raise AssetPreparationError(f"The O3DE engine path is invalid: {repo_root}")
    verified_workspace = developer_preview_workspace.verify_preview_workspace(repo_root)
    if verified_workspace != workspace:
        raise AssetPreparationError(
            "Asset preparation paths do not match the verified bounded Developer Preview workspace."
        )
    stdout_path = workspace.launcher_log / STDOUT_FILENAME
    stderr_path = workspace.launcher_log / STDERR_FILENAME
    for output_path in (stdout_path, stderr_path):
        if output_path.is_symlink() or (output_path.exists() and not output_path.is_file()):
            raise AssetPreparationError(
                f"Asset-preparation log output is unsafe: {output_path}"
            )
    with stdout_path.open("w", encoding="utf-8", newline="\n", errors="replace") as stdout, (
        stderr_path.open("w", encoding="utf-8", newline="\n", errors="replace")
    ) as stderr:
        try:
            exit_code = executor(command, batch.parent, stdout, stderr)
        except OSError as exc:
            raise AssetPreparationError(f"Unable to run {batch}: {exc}") from exc
    if exit_code != 0:
        raise AssetPreparationError(
            f"Asset preparation failed with exit code {exit_code}. "
            f"Inspect {stdout_path} and {stderr_path}."
        )
    print(f"Developer Preview assets are ready. Logs: {workspace.launcher_log}")
    return command
