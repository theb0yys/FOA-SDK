#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Open the repository-owned Tainted Grail Developer Preview O3DE project.

This command selects AutomatedTesting, which is committed with the
TaintedGrailModdingSDK Gem enabled, then delegates process handling to the
reviewed Developer Preview launch wrapper.
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
import validate_developer_preview_project

PREVIEW_PROJECT = Path("AutomatedTesting")
DEFAULT_BUILD_DIRECTORY = Path("build/tg-sdk-developer-preview-0-windows-profile")
DEFAULT_LOG_DIRECTORY = Path("build/tg-sdk-developer-preview-0-launch")


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
        default=DEFAULT_LOG_DIRECTORY,
        help="Wrapper-owned launch log directory.",
    )
    parser.add_argument("--result", type=Path, help="Optional explicit launch-result JSON path.")
    parser.add_argument("--dry-run", action="store_true", help="Print the resolved command without launching.")
    return parser


def launch_arguments(args: argparse.Namespace, repo_root: Path) -> list[str]:
    values = [
        "--repo-root",
        str(repo_root),
        "--project",
        str(repo_root / PREVIEW_PROJECT),
        "--log-dir",
        str(args.log_dir),
    ]
    if args.editor is not None:
        values.extend(("--editor", str(args.editor)))
    else:
        values.extend(("--build-dir", str(args.build_dir)))
    if args.result is not None:
        values.extend(("--result", str(args.result)))
    if args.dry_run:
        values.append("--dry-run")
    return values


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    repo_root = repository_root_from_script()
    try:
        validate_developer_preview_project.validate_preview_project(repo_root)
    except (OSError, validate_developer_preview_project.PreviewProjectContractError) as exc:
        print(f"Developer Preview project integration is invalid: {exc}", file=sys.stderr)
        return 2

    print(
        "Opening repository-owned O3DE project AutomatedTesting with "
        "TaintedGrailModdingSDK enabled."
    )
    return developer_preview_launch.main(launch_arguments(args, repo_root))


if __name__ == "__main__":
    raise SystemExit(main())
