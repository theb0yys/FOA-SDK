#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the TG SDK Core, Framework, Editor, and test build graph."""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path
from typing import Iterable


class BuildGraphContractError(RuntimeError):
    """Raised when build ownership or dependency direction is invalid."""


MANIFEST_ENTRY = re.compile(r"^\s*((?:Source|Tests)/[^\s)]+\.(?:cpp|h))\s*$", re.MULTILINE)


CORE_FORBIDDEN_NAME_PARTS = (
    "Widget",
    "EditorModule",
    "SystemComponent",
    "Persistence",
    "PathPolicy",
    "WorkspaceLoad",
    "WorkspaceSchema",
    "FoundationService",
    "FoundationNotificationBus",
    "SourceImportService",
)
FRAMEWORK_FORBIDDEN_NAME_PARTS = ("Widget", "EditorModule", "SystemComponent")
EDITOR_ALLOWED_CPP_PARTS = ("Widget", "EditorModule", "SystemComponent")


def read_text(path: Path) -> str:
    if not path.is_file():
        raise BuildGraphContractError(f"Required build-graph file is missing: {path}")
    return path.read_text(encoding="utf-8")


def parse_manifest_text(text: str) -> tuple[str, ...]:
    return tuple(MANIFEST_ENTRY.findall(text))


def parse_manifest(path: Path) -> tuple[str, ...]:
    entries = parse_manifest_text(read_text(path))
    if not entries:
        raise BuildGraphContractError(f"Build manifest contains no source entries: {path}")
    return entries


def find_call_block(text: str, command: str, required_fragment: str) -> str:
    marker = f"{command}("
    position = 0
    while True:
        start = text.find(marker, position)
        if start < 0:
            break
        depth = 0
        end = start
        for index in range(start + len(command), len(text)):
            character = text[index]
            if character == "(":
                depth += 1
            elif character == ")":
                depth -= 1
                if depth == 0:
                    end = index + 1
                    break
        block = text[start:end]
        if required_fragment in block:
            return block
        position = max(end, start + len(marker))
    raise BuildGraphContractError(
        f"Unable to find {command} block containing {required_fragment!r}."
    )


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise BuildGraphContractError(f"{label} is missing required fragment {fragment!r}.")


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise BuildGraphContractError(f"{label} contains forbidden fragment {fragment!r}.")


def validate_unique_production_ownership(
    production_cpp: Iterable[str],
    role_entries: dict[str, Iterable[str]],
) -> None:
    production = tuple(sorted(production_cpp))
    owned = [
        entry
        for entries in role_entries.values()
        for entry in entries
        if entry.startswith("Source/") and entry.endswith(".cpp")
    ]
    counts = Counter(owned)
    duplicates = sorted(path for path, count in counts.items() if count != 1)
    missing = sorted(set(production) - set(owned))
    unexpected = sorted(set(owned) - set(production))
    if duplicates or missing or unexpected:
        details = []
        if duplicates:
            details.append("duplicate ownership: " + ", ".join(duplicates))
        if missing:
            details.append("unowned production sources: " + ", ".join(missing))
        if unexpected:
            details.append("unknown production sources: " + ", ".join(unexpected))
        raise BuildGraphContractError("; ".join(details))


def validate_role_names(role: str, entries: Iterable[str], forbidden: Iterable[str]) -> None:
    violations = sorted(
        entry for entry in entries if any(fragment in Path(entry).name for fragment in forbidden)
    )
    if violations:
        raise BuildGraphContractError(
            f"{role} owns files outside its boundary: " + ", ".join(violations)
        )


def validate_test_manifest(entries: Iterable[str], label: str) -> None:
    production_entries = sorted(entry for entry in entries if entry.startswith("Source/"))
    if production_entries:
        raise BuildGraphContractError(
            f"{label} recompiles production sources: " + ", ".join(production_entries)
        )
    non_tests = sorted(entry for entry in entries if not entry.startswith("Tests/"))
    if non_tests:
        raise BuildGraphContractError(
            f"{label} contains non-test entries: " + ", ".join(non_tests)
        )


def validate_core_includes(
    code_root: Path,
    core_entries: Iterable[str],
    framework_entries: Iterable[str],
) -> None:
    framework_headers = {
        Path(entry).name for entry in framework_entries if entry.endswith(".h")
    }
    for entry in core_entries:
        if not entry.endswith((".cpp", ".h")):
            continue
        text = read_text(code_root / entry)
        if re.search(r"#include\s*[<\"](?:Q[A-Z]|Qt)", text):
            raise BuildGraphContractError(f"Core source includes Qt: {entry}")
        if "AzToolsFramework" in text:
            raise BuildGraphContractError(f"Core source includes AzToolsFramework: {entry}")
        coupled = sorted(header for header in framework_headers if f'"{header}"' in text)
        if coupled:
            raise BuildGraphContractError(
                f"Core source depends on Framework headers: {entry}: " + ", ".join(coupled)
            )


def validate_build_graph(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    cmake_path = code_root / "CMakeLists.txt"
    core_manifest = code_root / "taintedgrailmoddingsdk_core_files.cmake"
    framework_manifest = code_root / "taintedgrailmoddingsdk_framework_files.cmake"
    editor_manifest = code_root / "taintedgrailmoddingsdk_editor_files.cmake"
    catalog_test_manifest = code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake"
    path_test_manifest = code_root / "taintedgrailmoddingsdk_path_policy_tests_files.cmake"
    stale_manifest = code_root / "taintedgrailmoddingsdk_path_policy_editor_files.cmake"

    cmake = read_text(cmake_path)
    require_fragments(
        cmake,
        ("if(NOT PAL_TRAIT_BUILD_HOST_TOOLS)", "VARIANTS Tools Builders"),
        "TG SDK CMake",
    )

    core_block = find_call_block(cmake, "ly_add_target", "NAME ${gem_name}.Core.Static STATIC")
    framework_block = find_call_block(
        cmake, "ly_add_target", "NAME ${gem_name}.Framework.Static STATIC"
    )
    editor_block = find_call_block(cmake, "ly_add_target", "NAME ${gem_name}.Editor GEM_MODULE")
    test_block = find_call_block(
        cmake, "ly_add_target", "NAME ${gem_name}.Catalog.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}"
    )

    require_fragments(
        core_block,
        ("taintedgrailmoddingsdk_core_files.cmake", "PUBLIC", "AZ::AzCore"),
        "Core target",
    )
    reject_fragments(
        core_block,
        ("Framework.Static", "Editor", "AzToolsFramework", "Qt"),
        "Core target",
    )
    require_fragments(
        framework_block,
        (
            "taintedgrailmoddingsdk_framework_files.cmake",
            "Gem::${gem_name}.Core.Static",
            "AZ::AzToolsFramework",
        ),
        "Framework target",
    )
    reject_fragments(framework_block, (".Editor", "GEM_MODULE"), "Framework target")
    require_fragments(
        editor_block,
        ("taintedgrailmoddingsdk_editor_files.cmake", "Gem::${gem_name}.Framework.Static"),
        "Editor target",
    )
    reject_fragments(
        editor_block,
        ("taintedgrailmoddingsdk_core_files.cmake", "taintedgrailmoddingsdk_framework_files.cmake"),
        "Editor target",
    )
    require_fragments(
        test_block,
        (
            "taintedgrailmoddingsdk_catalog_tests_files.cmake",
            "taintedgrailmoddingsdk_path_policy_tests_files.cmake",
            "Gem::${gem_name}.Framework.Static",
            "AZ::AzTest",
        ),
        "Catalog test target",
    )
    reject_fragments(test_block, ("Core.Static", "Editor"), "Catalog test target")

    for alias in ("${gem_name}.Tools", "${gem_name}.Builders"):
        alias_block = find_call_block(cmake, "ly_create_alias", f"NAME {alias}")
        require_fragments(alias_block, ("Gem::${gem_name}.Editor",), f"{alias} alias")
        reject_fragments(alias_block, ("Core.Static", "Framework.Static"), f"{alias} alias")

    if stale_manifest.exists():
        raise BuildGraphContractError(
            "The obsolete path-policy editor manifest still exists; Framework must own those files."
        )

    core_entries = parse_manifest(core_manifest)
    framework_entries = parse_manifest(framework_manifest)
    execution_manifest = code_root / "taintedgrailmoddingsdk_framework_execution_files.cmake"
    execution_entries = parse_manifest(execution_manifest) if execution_manifest.is_file() else ()
    synthetic_manifest = code_root / "taintedgrailmoddingsdk_framework_synthetic_files.cmake"
    synthetic_entries = parse_manifest(synthetic_manifest) if synthetic_manifest.is_file() else ()
    provider_manifest = code_root / "taintedgrailmoddingsdk_synthetic_provider_files.cmake"
    provider_entries = parse_manifest(provider_manifest) if provider_manifest.is_file() else ()
    if synthetic_entries or provider_entries:
        execution_block = find_call_block(cmake, "ly_add_target", "NAME ${gem_name}.FrameworkExecution.Static")
        require_fragments(execution_block, (synthetic_manifest.name,), "Synthetic transaction owner")
        provider_block = find_call_block(cmake, "ly_add_target", "NAME ${gem_name}.Synthetic.Provider")
        require_fragments(provider_block, ("APPLICATION", provider_manifest.name), "Synthetic native provider owner")
        if provider_entries != ("Source/ExecutionSynthetic/FrameworkSyntheticProvider.cpp",):
            raise BuildGraphContractError("The synthetic application must own exactly its harmless provider source.")

    editor_entries = parse_manifest(editor_manifest)
    catalog_test_entries = parse_manifest(catalog_test_manifest)
    path_test_entries = parse_manifest(path_test_manifest)

    validate_role_names("Core", core_entries, CORE_FORBIDDEN_NAME_PARTS)
    validate_role_names("Framework", framework_entries, FRAMEWORK_FORBIDDEN_NAME_PARTS)
    invalid_editor_cpp = sorted(
        entry
        for entry in editor_entries
        if entry.endswith(".cpp")
        and not any(fragment in Path(entry).name for fragment in EDITOR_ALLOWED_CPP_PARTS)
    )
    if invalid_editor_cpp:
        raise BuildGraphContractError(
            "Editor owns non-composition production sources: " + ", ".join(invalid_editor_cpp)
        )

    validate_test_manifest(catalog_test_entries, "Catalog test manifest")
    validate_test_manifest(path_test_entries, "Path-policy test manifest")

    production_cpp = tuple(
        sorted(path.relative_to(code_root).as_posix() for path in (code_root / "Source").rglob("*.cpp"))
    )
    validate_unique_production_ownership(
        production_cpp,
        {
            "Core": core_entries,
            "Framework": framework_entries,
            "FrameworkExecution": (*execution_entries, *synthetic_entries),
            "SyntheticProvider": provider_entries,
            "Editor": editor_entries,
        },
    )
    validate_core_includes(code_root, core_entries, (*framework_entries, *execution_entries, *synthetic_entries, *provider_entries))

    infrastructure = read_text(gem_root / "Infrastructure/README.md")
    require_fragments(
        infrastructure,
        (
            "Core.Static",
            "Framework.Static",
            "Editor",
            "Core must not depend on Framework",
        ),
        "Infrastructure contract",
    )
    design = read_text(repo_root / "docs/tainted-grail-sdk/CORE_FRAMEWORK_BUILD_GRAPH.md")
    require_fragments(
        design,
        (
            "Core.Static",
            "Framework.Static",
            "Editor",
            "Tests link Framework",
            "No runtime adapter",
        ),
        "Build-graph design contract",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_build_graph(repo_root)
    except (OSError, BuildGraphContractError) as exc:
        print(f"Tainted Grail Core/Framework build-graph validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail build graph passed: Core, Framework, Editor, and tests have unique "
        "source ownership and one-way dependencies."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
