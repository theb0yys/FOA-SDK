#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Fail closed unless the tracked tree matches the standalone FOA-SDK product layout."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from typing import Iterable

ALLOWED_TOP_LEVEL_DIRECTORIES = {
    ".github",
    "Gems",
    "Research",
    "TaintedGrailModdingEditor",
    "docs",
}
ALLOWED_ROOT_FILES = {
    ".clang-format",
    ".editorconfig",
    ".gitattributes",
    ".gitignore",
    "CHANGELOG.md",
    "CODE_OF_CONDUCT.md",
    "CONTRIBUTING.md",
    "GOVERNANCE.md",
    "LICENSE.txt",
    "LICENSE_APACHE2.TXT",
    "LICENSE_MIT.TXT",
    "README.md",
    "ROADMAP.md",
    "SECURITY.md",
    "SUPPORT.md",
    "o3de.lock.json",
}
ALLOWED_GEMS = {"ExternalToolchain", "TaintedGrailModdingSDK"}
ALLOWED_GITHUB_FILES = {
    ".github/CODEOWNERS",
    ".github/ISSUE_TEMPLATE/config.yml",
    ".github/ISSUE_TEMPLATE/tg_sdk_bug.yml",
    ".github/ISSUE_TEMPLATE/tg_sdk_feature.yml",
    ".github/ISSUE_TEMPLATE/tg_sdk_research.yml",
    ".github/PULL_REQUEST_TEMPLATE.md",
    ".github/workflows/tainted-grail-editor-entry.yml",
    ".github/workflows/tainted-grail-repository-hygiene.yml",
    ".github/workflows/tainted-grail-sdk-foundation.yml",
    ".github/workflows/tainted-grail-sdk-installer.yml",
}
REQUIRED_PATHS = {
    ".github/CODEOWNERS",
    "Gems/ExternalToolchain/gem.json",
    "Gems/TaintedGrailModdingSDK/gem.json",
    "TaintedGrailModdingEditor/project.json",
    "docs/tainted-grail-sdk/README.md",
    "Research/o3de-to-unity-conversion-and-runtime-bridge/README.md",
    "o3de.lock.json",
    "README.md",
}
FORBIDDEN_ENGINE_ROOTS = {
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
}
FORBIDDEN_ENGINE_FILES = {
    ".automatedtesting.json",
    ".lfsconfig",
    "CMakeLists.txt",
    "CMakePresets.json",
    "Doxyfile_ScriptBinds",
    "RETIRED_CODE.md",
    "engine.json",
    "pytest.ini",
}


class RepositoryStructureError(RuntimeError):
    """Raised when tracked paths escape the reviewed product allow-list."""


def normalize_paths(paths: Iterable[str]) -> set[str]:
    normalized: set[str] = set()
    for raw in paths:
        path = raw.strip().replace("\\", "/")
        if not path:
            continue
        if path.startswith("./"):
            path = path[2:]
        if path.startswith("/") or "//" in path or path in {".", ".."}:
            raise RepositoryStructureError(f"Invalid tracked path: {raw!r}")
        if any(part in {"", ".", ".."} for part in path.split("/")):
            raise RepositoryStructureError(f"Non-canonical tracked path: {raw!r}")
        normalized.add(path)
    return normalized


def validate_paths(paths: Iterable[str]) -> None:
    tracked = normalize_paths(paths)
    missing = sorted(REQUIRED_PATHS - tracked)
    if missing:
        raise RepositoryStructureError(
            "Standalone FOA-SDK tree is missing required paths: " + ", ".join(missing)
        )

    unexpected_root_files: set[str] = set()
    unexpected_top_level_directories: set[str] = set()
    forbidden: set[str] = set()
    unexpected_gems: set[str] = set()
    unexpected_github: set[str] = set()

    for path in tracked:
        parts = path.split("/")
        top = parts[0]
        if len(parts) == 1:
            if top in FORBIDDEN_ENGINE_FILES:
                forbidden.add(path)
            elif top not in ALLOWED_ROOT_FILES:
                unexpected_root_files.add(path)
            continue

        if top in FORBIDDEN_ENGINE_ROOTS:
            forbidden.add(path)
            continue
        if top not in ALLOWED_TOP_LEVEL_DIRECTORIES:
            unexpected_top_level_directories.add(top)
            continue
        if top == "Gems":
            if len(parts) < 2 or parts[1] not in ALLOWED_GEMS:
                unexpected_gems.add(parts[1] if len(parts) > 1 else path)
        elif top == ".github" and path not in ALLOWED_GITHUB_FILES:
            unexpected_github.add(path)
        elif top == "docs" and (len(parts) < 2 or parts[1] != "tainted-grail-sdk"):
            unexpected_top_level_directories.add(path)

    problems: list[str] = []
    if forbidden:
        problems.append("inherited O3DE paths: " + ", ".join(sorted(forbidden)[:20]))
    if unexpected_root_files:
        problems.append("unexpected root files: " + ", ".join(sorted(unexpected_root_files)))
    if unexpected_top_level_directories:
        problems.append(
            "unexpected top-level product paths: "
            + ", ".join(sorted(unexpected_top_level_directories))
        )
    if unexpected_gems:
        problems.append("unexpected root Gems: " + ", ".join(sorted(unexpected_gems)))
    if unexpected_github:
        problems.append("unexpected .github files: " + ", ".join(sorted(unexpected_github)))
    if problems:
        raise RepositoryStructureError("; ".join(problems))


def tracked_paths(repo_root: Path) -> list[str]:
    completed = subprocess.run(
        ("git", "-C", str(repo_root), "ls-files", "-z"),
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", errors="replace").strip()
        raise RepositoryStructureError(f"Unable to enumerate tracked paths: {detail}")
    return [
        entry.decode("utf-8", errors="strict")
        for entry in completed.stdout.split(b"\0")
        if entry
    ]


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_paths(tracked_paths(repo_root))
    except (OSError, UnicodeDecodeError, RepositoryStructureError) as exc:
        print(f"FOA-SDK repository structure validation failed: {exc}", file=sys.stderr)
        return 1
    print("FOA-SDK repository structure validation passed: tracked tree is product-only.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
