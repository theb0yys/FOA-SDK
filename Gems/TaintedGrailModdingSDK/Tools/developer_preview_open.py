#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Open the dedicated Tainted Grail Modding Editor O3DE project.

This command selects TaintedGrailModdingEditor, validates the committed project
contract, and delegates process handling to the restricted Developer Preview
Editor launch wrapper.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Sequence

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import developer_preview_launch
import developer_preview_assets
import developer_preview_workspace
import validate_developer_preview_project

DEFAULT_BUILD_DIRECTORY = Path("build/tg-sdk-developer-preview-0-windows-profile")


def repository_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--editor", type=Path, help="Explicit source-built Editor.exe.")
    source.add_argument(
        "--build-dir",
        type=Path,
        default=DEFAULT_BUILD_DIRECTORY,
        help="Configured Developer Preview build directory.",
    )
    parser.add_argument(
        "--log-dir",
        type=Path,
        help=(
            "Optional wrapper-owned launch log directory beneath the bounded launcher root; "
            "defaults to that root."
        ),
    )
    parser.add_argument(
        "--result",
        type=Path,
        help="Optional launch-result JSON path beneath the bounded launcher root.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print the resolved command without launching.")
    return parser


def launch_arguments(
    args: argparse.Namespace,
    repo_root: Path,
    workspace: developer_preview_workspace.PreviewWorkspacePaths,
) -> list[str]:
    log_dir = bounded_launcher_path(
        args.log_dir,
        workspace.launcher_log,
        default=workspace.launcher_log,
        label="launch log directory",
    )
    values = [
        "--repo-root",
        str(repo_root),
        "--project",
        str(workspace.project),
        "--engine",
        str(repo_root),
        "--project-cache",
        str(workspace.cache),
        "--project-user",
        str(workspace.user),
        "--project-log",
        str(workspace.log),
        "--level",
        str(workspace.startup_level),
        "--log-dir",
        str(log_dir),
    ]
    if args.editor is not None:
        values.extend(("--editor", str(args.editor)))
    else:
        values.extend(("--build-dir", str(args.build_dir)))
    if args.result is not None:
        result = bounded_launcher_path(
            args.result,
            workspace.launcher_log,
            default=None,
            label="launch result",
        )
        assert result is not None
        values.extend(("--result", str(result)))
    if args.dry_run:
        values.append("--dry-run")
    return values


def bounded_launcher_path(
    value: Path | None,
    launcher_root: Path,
    *,
    default: Path | None,
    label: str,
) -> Path | None:
    if value is None:
        return default
    candidate = value.expanduser()
    if not candidate.is_absolute():
        candidate = launcher_root / candidate
    candidate = candidate.resolve(strict=False)
    root = launcher_root.resolve(strict=False)
    if not developer_preview_launch.is_relative_to(candidate, root):
        raise developer_preview_workspace.WorkspaceError(
            f"The Developer Preview {label} must remain inside {root}: {candidate}"
        )
    if candidate.is_symlink():
        raise developer_preview_workspace.WorkspaceError(
            f"The Developer Preview {label} must not be a symbolic link: {candidate}"
        )
    return candidate


def resolve_preflight_editor(args: argparse.Namespace, repo_root: Path) -> Path:
    if args.dry_run:
        if args.editor is not None:
            return developer_preview_launch.resolve_path(args.editor, repo_root)
        build_dir = developer_preview_launch.resolve_path(args.build_dir, repo_root)
        return developer_preview_launch.editor_candidates(build_dir)[0]
    editor, _ = developer_preview_launch.resolve_editor_executable(
        repo_root,
        explicit_editor=args.editor,
        build_dir=None if args.editor is not None else args.build_dir,
    )
    return editor


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    repo_root = repository_root_from_script()
    try:
        validate_developer_preview_project.validate_preview_project(repo_root)
        workspace = developer_preview_workspace.materialize_preview_workspace(
            repo_root,
            dry_run=args.dry_run,
        )
        editor = resolve_preflight_editor(args, repo_root)
        developer_preview_assets.prepare_assets(
            editor=editor,
            repo_root=repo_root,
            workspace=workspace,
            dry_run=args.dry_run,
        )
        arguments = launch_arguments(args, repo_root, workspace)
    except (
        OSError,
        developer_preview_assets.AssetPreparationError,
        developer_preview_workspace.WorkspaceError,
        validate_developer_preview_project.PreviewProjectContractError,
    ) as exc:
        print(f"Developer Preview project integration is invalid: {exc}", file=sys.stderr)
        return 2

    print(
        "Opening dedicated O3DE project TaintedGrailModdingEditor with "
        "TaintedGrailModdingSDK enabled."
    )
    return developer_preview_launch.main(arguments)


if __name__ == "__main__":
    raise SystemExit(main())
