#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Reject tracked repository paths that collide on case-insensitive filesystems."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import unicodedata
from collections import defaultdict
from pathlib import Path
from typing import Iterable, Sequence


class TrackedPathCollisionError(RuntimeError):
    """Raised when tracked paths cannot be read or are case-colliding."""


def repository_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def collision_key(path: str) -> str:
    normalized = unicodedata.normalize("NFC", path.replace("\\", "/"))
    return "/".join(part.casefold() for part in normalized.split("/"))


def find_collisions(paths: Iterable[str]) -> tuple[tuple[str, ...], ...]:
    grouped: dict[str, set[str]] = defaultdict(set)
    for path in paths:
        if not path:
            continue
        grouped[collision_key(path)].add(path.replace("\\", "/"))

    collisions = [
        tuple(sorted(values))
        for values in grouped.values()
        if len(values) > 1
    ]
    return tuple(sorted(collisions))


def read_tracked_paths(repo_root: Path) -> tuple[str, ...]:
    completed = subprocess.run(
        ("git", "-C", str(repo_root), "ls-files", "-z"),
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        error = os.fsdecode(completed.stderr).strip()
        raise TrackedPathCollisionError(
            f"git ls-files failed with exit code {completed.returncode}: {error}"
        )

    return tuple(
        os.fsdecode(value)
        for value in completed.stdout.split(b"\0")
        if value
    )


def validate_repository(repo_root: Path) -> None:
    collisions = find_collisions(read_tracked_paths(repo_root))
    if not collisions:
        return

    details = "\n".join(
        "  - " + " <> ".join(paths)
        for paths in collisions
    )
    raise TrackedPathCollisionError(
        "Tracked paths collide after Unicode normalization and case folding:\n"
        f"{details}"
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=repository_root_from_script())
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    repo_root = args.repo_root.expanduser().resolve(strict=False)
    try:
        validate_repository(repo_root)
    except (OSError, TrackedPathCollisionError) as exc:
        print(f"Tracked-path collision validation failed: {exc}", file=sys.stderr)
        return 1

    print("Tracked-path collision validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
