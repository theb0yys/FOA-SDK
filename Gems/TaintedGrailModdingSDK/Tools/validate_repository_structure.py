#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Fail closed unless the tracked tree matches the standalone FOA-SDK product layout."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path
from typing import Iterable

ALLOWED_TOP_LEVEL_DIRECTORIES = {
    ".github",
    "Gems",
    "Installer",
    "Plugins",
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
LEGACY_INSTALLER_PREFIX = "Gems/TaintedGrailModdingSDK/Installer/"
ALLOWED_PLUGIN_CATEGORIES = {"Authoring", "Integrations", "RuntimeAdapters"}
ALLOWED_PLUGIN_ROOT_FILES = {
    "Plugins/README.md",
    "Plugins/plugin.schema.json",
}
ALLOWED_PLUGIN_CATEGORY_FILES = {
    f"Plugins/{category}/README.md" for category in ALLOWED_PLUGIN_CATEGORIES
}
PLUGIN_PACKAGE_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9._-]{0,127}$")
AUTOMATIC_STATIC_WORKFLOW = (
    ".github/workflows/tainted-grail-sdk-pr-validation.yml"
)
ALLOWED_INSTALLER_LANES = {
    "Launcher",
    "Packaging",
    "Tests",
}
ALLOWED_INSTALLER_ROOT_FILES = {
    "Installer/README.md",
}
ALLOWED_INSTALLER_LANE_FILES = {
    f"Installer/{lane}/README.md" for lane in ALLOWED_INSTALLER_LANES
}
ALLOWED_GITHUB_FILES = {
    ".github/CODEOWNERS",
    ".github/ISSUE_TEMPLATE/config.yml",
    ".github/ISSUE_TEMPLATE/tg_sdk_bug.yml",
    ".github/ISSUE_TEMPLATE/tg_sdk_feature.yml",
    ".github/ISSUE_TEMPLATE/tg_sdk_research.yml",
    ".github/PULL_REQUEST_TEMPLATE.md",
    AUTOMATIC_STATIC_WORKFLOW,
    ".github/workflows/tainted-grail-editor-entry.yml",
    ".github/workflows/tainted-grail-repository-hygiene.yml",
    ".github/workflows/tainted-grail-sdk-foundation.yml",
    ".github/workflows/tainted-grail-sdk-installer.yml",
}
ALLOWED_DOC_ROOT_FILES = {"docs/README.md"}
ALLOWED_DOC_TREES = {"tainted-grail-sdk", "tainted-grail-modding"}
REQUIRED_PATHS = {
    ".github/CODEOWNERS",
    AUTOMATIC_STATIC_WORKFLOW,
    "Gems/ExternalToolchain/gem.json",
    "Gems/TaintedGrailModdingSDK/gem.json",
    "Installer/README.md",
    "Installer/Launcher/README.md",
    "Installer/Launcher/Windows/FOAInstallerLauncher.csproj",
    "Installer/Launcher/Windows/InstallerWizardForm.cs",
    "Installer/Launcher/Windows/InstalledEditorLauncher.cpp",
    "Installer/Launcher/Windows/InstalledEditorLauncher.rc",
    "Installer/Packaging/README.md",
    "Installer/Packaging/Windows/.config/dotnet-tools.json",
    "Installer/Packaging/Windows/CMakeLists.txt",
    "Installer/Packaging/Windows/README.md",
    "Installer/Tests/README.md",
    "Plugins/README.md",
    "Plugins/plugin.schema.json",
    "Plugins/Authoring/README.md",
    "Plugins/Integrations/README.md",
    "Plugins/RuntimeAdapters/README.md",
    "TaintedGrailModdingEditor/project.json",
    "docs/README.md",
    "docs/tainted-grail-sdk/README.md",
    "docs/tainted-grail-modding/README.md",
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
    legacy_installer_paths: set[str] = set()
    unexpected_gems: set[str] = set()
    unexpected_github: set[str] = set()
    unexpected_plugins: set[str] = set()
    plugin_packages: set[tuple[str, str]] = set()
    unexpected_installer: set[str] = set()

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
            if path.startswith(LEGACY_INSTALLER_PREFIX):
                legacy_installer_paths.add(path)
            if len(parts) < 2 or parts[1] not in ALLOWED_GEMS:
                unexpected_gems.add(parts[1] if len(parts) > 1 else path)
        elif top == "Installer":
            if len(parts) == 2:
                if path not in ALLOWED_INSTALLER_ROOT_FILES:
                    unexpected_installer.add(path)
                continue

            lane = parts[1]
            if lane not in ALLOWED_INSTALLER_LANES:
                unexpected_installer.add(path)
                continue
            if len(parts) == 3:
                if path not in ALLOWED_INSTALLER_LANE_FILES:
                    unexpected_installer.add(path)
                continue

        elif top == "Plugins":
            if len(parts) == 2:
                if path not in ALLOWED_PLUGIN_ROOT_FILES:
                    unexpected_plugins.add(path)
                continue

            category = parts[1]
            if category not in ALLOWED_PLUGIN_CATEGORIES:
                unexpected_plugins.add(path)
                continue
            if len(parts) == 3:
                if path not in ALLOWED_PLUGIN_CATEGORY_FILES:
                    unexpected_plugins.add(path)
                continue

            package = parts[2]
            if package == "README.md" or PLUGIN_PACKAGE_NAME.fullmatch(package) is None:
                unexpected_plugins.add(path)
                continue
            plugin_packages.add((category, package))
        elif top == ".github" and path not in ALLOWED_GITHUB_FILES:
            unexpected_github.add(path)
        elif top == "docs":
            if len(parts) == 2:
                if path not in ALLOWED_DOC_ROOT_FILES:
                    unexpected_top_level_directories.add(path)
            elif parts[1] not in ALLOWED_DOC_TREES:
                unexpected_top_level_directories.add(path)

    missing_plugin_manifests = sorted(
        f"Plugins/{category}/{package}/plugin.json"
        for category, package in plugin_packages
        if f"Plugins/{category}/{package}/plugin.json" not in tracked
    )

    problems: list[str] = []
    if forbidden:
        problems.append("inherited O3DE paths: " + ", ".join(sorted(forbidden)[:20]))
    if legacy_installer_paths:
        problems.append(
            "legacy Gem-owned installer paths: " + ", ".join(sorted(legacy_installer_paths)[:20])
        )
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
    if unexpected_installer:
        problems.append("unexpected installer paths: " + ", ".join(sorted(unexpected_installer)))
    if unexpected_plugins:
        problems.append("unexpected plug-in paths: " + ", ".join(sorted(unexpected_plugins)))
    if missing_plugin_manifests:
        problems.append(
            "plug-in packages missing plugin.json: " + ", ".join(missing_plugin_manifests)
        )
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
    print(
        "FOA-SDK repository structure validation passed: tracked tree is product-only, "
        "plug-in packages are governed, and installer source is isolated."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
