#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate governed optional Tool Gem packages and their ExtensionAPI bindings."""

from __future__ import annotations

import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path


ID = re.compile(r"^extension\.[a-z0-9]+(?:[.-][a-z0-9]+)*$")
TOKEN = re.compile(r"^[a-z0-9]+(?:[.-][a-z0-9]+)*$")
BRANCH_TOKEN = re.compile(r"^[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*$")
SEMVER = re.compile(
    r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    r"(?:-[0-9A-Za-z.-]+)?$"
)
PACKAGE_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9._-]{0,127}$")
RELATIVE_PATH = re.compile(r"^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*$")
GIT_COMMIT = re.compile(r"^[0-9a-f]{40}$")

CATEGORY_BY_DIRECTORY = {
    "Authoring": "authoring",
    "Integrations": "integration",
    "RuntimeAdapters": "runtime-adapter",
}

# Capabilities listed here are accepted only after syntax, uniqueness, and size
# validation. Capability ownership is separately enforced below so adding a
# project-specific contract never turns it into a repository-wide wildcard.
KNOWN_CAPABILITIES = {
    "acquisition.bundle.verify",
    "acquisition.github.pinned.read",
    "acquisition.local.read",
    "acquisition.merlin.checkout.verify",
    "acquisition.merlin.license.accept",
    "acquisition.merlin.plan",
    "acquisition.merlin.receipt.verify",
    "acquisition.plan",
    "query-catalog",
    "read-active-profile",
    "runtime.mono.build-plan",
    "runtime.mono.execution-gate",
    "runtime.mono.package-verify",
    "runtime.mono.result-verify",
    "submit-candidate-evidence",
}
CAPABILITY_OWNERS = {
    "runtime.mono.build-plan": "extension.foa-mono-runtime-adapter",
    "runtime.mono.execution-gate": "extension.foa-mono-runtime-adapter",
    "runtime.mono.package-verify": "extension.foa-mono-runtime-adapter",
    "runtime.mono.result-verify": "extension.foa-mono-runtime-adapter",
}
KNOWN_RUNTIME_TARGETS = {"editor-only", "mono", "il2cpp"}
KNOWN_STATUSES = {"experimental", "available", "deprecated"}
EXPECTED_KEYS = {
    "schema_version",
    "id",
    "name",
    "version",
    "category",
    "status",
    "optional",
    "entry_points",
    "capabilities",
    "dependencies",
    "compatibility",
    "provenance",
}


class PluginPackageError(RuntimeError):
    """Raised when an optional package can bypass the governed contract."""


@dataclass(frozen=True)
class Package:
    directory: Path
    relative_directory: str
    manifest: dict[str, object]
    extension_id: str
    gem_names: tuple[str, ...]
    dependencies: tuple[str, ...]


def fail(message: str) -> None:
    raise PluginPackageError(message)


def load_object(path: Path, label: str) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"Unable to load {label} {path}: {exc}")
    if not isinstance(value, dict):
        fail(f"{label} must be a JSON object: {path}")
    return value


def require_string_list(
    value: object,
    label: str,
    *,
    pattern: re.Pattern[str] | None = None,
    allow_empty: bool = False,
    maximum: int = 64,
) -> list[str]:
    if not isinstance(value, list) or any(not isinstance(item, str) for item in value):
        fail(f"{label} must be a string array")
    if (
        (not allow_empty and not value)
        or len(value) > maximum
        or len(set(value)) != len(value)
    ):
        fail(f"{label} must be non-empty, unique, and bounded")
    if any(
        not item
        or len(item) > 256
        or (pattern is not None and pattern.fullmatch(item) is None)
        for item in value
    ):
        fail(f"{label} contains an invalid value")
    return value


def validate_capabilities(extension_id: str, value: object) -> list[str]:
    capabilities = require_string_list(
        value,
        f"{extension_id} capabilities",
        pattern=TOKEN,
    )
    unknown = set(capabilities) - KNOWN_CAPABILITIES
    if unknown:
        fail(
            f"{extension_id} declares unknown governed capabilities: "
            f"{sorted(unknown)}"
        )
    wrong_owner = {
        capability: owner
        for capability in capabilities
        if (owner := CAPABILITY_OWNERS.get(capability)) is not None
        and owner != extension_id
    }
    if wrong_owner:
        fail(
            f"{extension_id} declares capabilities owned by another extension: "
            f"{sorted(wrong_owner.items())}"
        )
    return capabilities


def validate_entry_points(
    package_root: Path,
    value: object,
    label: str,
) -> tuple[str, ...]:
    if not isinstance(value, dict) or not value:
        fail(f"{label} must be a non-empty object")
    unknown = set(value) - {"o3de_gems", "python_modules", "tools"}
    if unknown:
        fail(f"{label} contains unknown entry-point kinds: {sorted(unknown)}")

    gem_names: list[str] = []
    for kind, paths_value in value.items():
        paths = require_string_list(
            paths_value,
            f"{label}.{kind}",
            pattern=RELATIVE_PATH,
        )
        for relative in paths:
            path = package_root / relative
            if not path.exists():
                fail(f"{label}.{kind} target does not exist: {path}")
            if kind == "o3de_gems":
                descriptor = path / "gem.json"
                cmake = path / "CMakeLists.txt"
                if not descriptor.is_file() or not cmake.is_file():
                    fail(
                        "O3DE Gem entry point requires gem.json and CMakeLists.txt: "
                        f"{path}"
                    )
                gem = load_object(descriptor, "plug-in Gem descriptor")
                gem_name = gem.get("gem_name")
                if not isinstance(gem_name, str) or not PACKAGE_NAME.fullmatch(gem_name):
                    fail(f"Plug-in Gem has invalid gem_name: {descriptor}")
                if gem.get("type") != "Tool":
                    fail(f"Plug-in O3DE entry point must be a Tool Gem: {descriptor}")
                dependencies = gem.get("dependencies")
                if (
                    not isinstance(dependencies, list)
                    or "TaintedGrailModdingSDK" not in dependencies
                ):
                    fail(
                        "Plug-in Tool Gem must depend on TaintedGrailModdingSDK: "
                        f"{descriptor}"
                    )
                gem_names.append(gem_name)
            elif not path.is_file():
                fail(f"{label}.{kind} entry point must be a file: {path}")
    return tuple(gem_names)


def validate_manifest(package_root: Path, plugins_root: Path) -> Package:
    manifest_path = package_root / "plugin.json"
    manifest = load_object(manifest_path, "plug-in manifest")
    unknown = set(manifest) - EXPECTED_KEYS
    missing = EXPECTED_KEYS - set(manifest)
    if unknown or missing:
        fail(
            f"Plug-in manifest keys mismatch at {manifest_path}; "
            f"missing={sorted(missing)}, unknown={sorted(unknown)}"
        )
    if manifest.get("schema_version") != 1 or manifest.get("optional") is not True:
        fail(
            "Plug-in manifest must be schema 1 and explicitly optional: "
            f"{manifest_path}"
        )

    extension_id = manifest.get("id")
    name = manifest.get("name")
    version = manifest.get("version")
    status = manifest.get("status")
    if not isinstance(extension_id, str) or ID.fullmatch(extension_id) is None:
        fail(f"Invalid plug-in extension ID: {manifest_path}")
    if not isinstance(name, str) or not name or len(name) > 128:
        fail(f"Invalid plug-in display name: {manifest_path}")
    if not isinstance(version, str) or SEMVER.fullmatch(version) is None:
        fail(f"Invalid plug-in semantic version: {manifest_path}")
    if status not in KNOWN_STATUSES:
        fail(f"Invalid plug-in status: {manifest_path}")

    category_directory = package_root.parent.name
    expected_category = CATEGORY_BY_DIRECTORY.get(category_directory)
    if expected_category is None or manifest.get("category") != expected_category:
        fail(f"Plug-in category does not match its governed directory: {manifest_path}")

    validate_capabilities(extension_id, manifest.get("capabilities"))
    dependencies = require_string_list(
        manifest.get("dependencies"),
        f"{extension_id} dependencies",
        pattern=ID,
        allow_empty=True,
    )
    if extension_id in dependencies:
        fail(f"{extension_id} cannot depend on itself")

    compatibility = manifest.get("compatibility")
    if not isinstance(compatibility, dict) or set(compatibility) != {
        "game_versions",
        "branches",
        "runtime_targets",
    }:
        fail(f"{extension_id} compatibility has missing or unknown fields")
    compatibility_may_be_unbound = expected_category == "integration"
    require_string_list(
        compatibility.get("game_versions"),
        f"{extension_id} game versions",
        allow_empty=compatibility_may_be_unbound,
    )
    require_string_list(
        compatibility.get("branches"),
        f"{extension_id} branches",
        pattern=BRANCH_TOKEN,
        allow_empty=compatibility_may_be_unbound,
    )
    targets = require_string_list(
        compatibility.get("runtime_targets"),
        f"{extension_id} runtime targets",
        pattern=TOKEN,
        maximum=3,
    )
    if set(targets) - KNOWN_RUNTIME_TARGETS:
        fail(f"{extension_id} declares an unknown runtime target")
    if expected_category == "authoring" and targets != ["editor-only"]:
        fail(f"Authoring plug-in {extension_id} must remain editor-only")
    if expected_category == "runtime-adapter" and (
        "editor-only" in targets or len(targets) != 1
    ):
        fail(
            f"Runtime adapter {extension_id} must declare exactly one Mono or "
            "IL2CPP target"
        )

    provenance = manifest.get("provenance")
    if not isinstance(provenance, dict) or set(provenance) != {
        "origin",
        "revision",
        "license",
        "redistribution_reviewed",
    }:
        fail(f"{extension_id} provenance has missing or unknown fields")
    for key in ("origin", "revision", "license"):
        value = provenance.get(key)
        if not isinstance(value, str) or not value or len(value) > 512:
            fail(f"{extension_id} provenance field {key} is invalid")
    revision = str(provenance.get("revision"))
    origin = str(provenance.get("origin", ""))
    if revision != "project-owned" and (
        "github" in origin.lower() or "/" in origin
    ):
        if GIT_COMMIT.fullmatch(revision) is None:
            fail(f"{extension_id} Git provenance requires one exact lowercase commit")
    if not isinstance(provenance.get("redistribution_reviewed"), bool):
        fail(f"{extension_id} redistribution review state must be boolean")
    if (
        provenance.get("license") == "NOASSERTION"
        and provenance.get("redistribution_reviewed") is not False
    ):
        fail(
            f"{extension_id} cannot mark unlicensed upstream payloads "
            "redistribution-reviewed"
        )

    gem_names = validate_entry_points(
        package_root,
        manifest.get("entry_points"),
        f"{extension_id} entry_points",
    )
    relative = package_root.relative_to(plugins_root.parent).as_posix()
    return Package(
        package_root,
        relative,
        manifest,
        extension_id,
        gem_names,
        tuple(dependencies),
    )


def validate_dependency_graph(packages: list[Package]) -> None:
    by_id = {package.extension_id: package for package in packages}
    if len(by_id) != len(packages):
        fail("Plug-in extension IDs must be globally unique")
    for package in packages:
        missing = set(package.dependencies) - set(by_id)
        if missing:
            fail(f"{package.extension_id} has missing dependencies: {sorted(missing)}")

    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(extension_id: str) -> None:
        if extension_id in visiting:
            fail(f"Plug-in dependency cycle reaches {extension_id}")
        if extension_id in visited:
            return
        visiting.add(extension_id)
        for dependency in by_id[extension_id].dependencies:
            visit(dependency)
        visiting.remove(extension_id)
        visited.add(extension_id)

    for extension_id in sorted(by_id):
        visit(extension_id)


def validate_project_registration(repo_root: Path, packages: list[Package]) -> None:
    project_path = repo_root / "TaintedGrailModdingEditor/project.json"
    project = load_object(project_path, "Editor project manifest")
    external = project.get("external_subdirectories")
    gem_names = project.get("gem_names")
    if not isinstance(external, list) or any(
        not isinstance(item, str) for item in external
    ):
        fail("Editor project external_subdirectories must be a string array")
    if not isinstance(gem_names, list) or any(
        not isinstance(item, str) for item in gem_names
    ):
        fail("Editor project gem_names must be a string array")

    expected_external: list[str] = []
    expected_gems: list[str] = []
    for package in sorted(packages, key=lambda item: item.relative_directory):
        entry_points = package.manifest["entry_points"]
        assert isinstance(entry_points, dict)
        gem_paths = entry_points.get("o3de_gems", [])
        assert isinstance(gem_paths, list)
        for relative, gem_name in zip(gem_paths, package.gem_names, strict=True):
            expected_external.append(
                "../" + package.relative_directory + "/" + relative
            )
            expected_gems.append(gem_name)

    missing_external = [
        item for item in expected_external if external.count(item) != 1
    ]
    missing_gems = [item for item in expected_gems if gem_names.count(item) != 1]
    if missing_external or missing_gems:
        fail(
            "Editor project does not deterministically select every available "
            "plug-in Tool Gem; "
            f"external={missing_external}, gems={missing_gems}"
        )
    selected_plugin_directories = [
        item
        for item in external
        if isinstance(item, str) and item.startswith("../Plugins/")
    ]
    selected_plugin_gems = [item for item in gem_names if item in set(expected_gems)]
    if (
        selected_plugin_directories != expected_external
        or selected_plugin_gems != expected_gems
    ):
        fail(
            "Editor project plug-in Tool Gems must be selected once in "
            "deterministic package order"
        )


def discover_packages(repo_root: Path) -> list[Package]:
    plugins_root = repo_root / "Plugins"
    packages: list[Package] = []
    for directory in CATEGORY_BY_DIRECTORY:
        category_root = plugins_root / directory
        if not category_root.is_dir():
            fail(f"Missing governed plug-in category: {category_root}")
        for child in sorted(category_root.iterdir(), key=lambda path: path.name):
            if child.name == "README.md":
                continue
            if not child.is_dir() or PACKAGE_NAME.fullmatch(child.name) is None:
                fail(f"Unexpected plug-in category entry: {child}")
            packages.append(validate_manifest(child, plugins_root))
    return packages


def validate_plugin_packages(repo_root: Path) -> list[Package]:
    packages = discover_packages(repo_root)
    if not packages:
        fail("The governed plug-in root contains no functional packages")
    validate_dependency_graph(packages)
    validate_project_registration(repo_root, packages)
    return packages


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        packages = validate_plugin_packages(repo_root)
    except PluginPackageError as exc:
        print(f"FOA-SDK plug-in package validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "FOA-SDK plug-in package validation passed: "
        f"{len(packages)} optional Tool Gem package(s), exact manifests, entry points, "
        "dependencies, compatibility, provenance, and project selection are governed."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
