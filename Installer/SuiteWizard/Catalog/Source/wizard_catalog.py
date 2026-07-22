#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Read-only Suite Wizard catalogue discovery and selection interface."""

from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path
from typing import Iterable, Mapping, Sequence

SUITE_WIZARD_ROOT = Path(__file__).resolve().parents[2]
RESOLVER_SOURCE = SUITE_WIZARD_ROOT / "Resolver" / "Source"
VIEW_MODEL_SOURCE = SUITE_WIZARD_ROOT / "ViewModel" / "Source"
sys.path.insert(0, str(RESOLVER_SOURCE))
sys.path.insert(0, str(VIEW_MODEL_SOURCE))

import _resolver_core as core  # noqa: E402
from suite_package_resolver import (  # noqa: E402
    ResolutionContext,
    ResolverError,
    canonical_json_bytes,
    resolve_suite,
    validate_manifest_catalog,
)
from wizard_view_model import WizardContractError, build_view_model  # noqa: E402

AUTHORITY_FIELDS = (
    "acquisition",
    "installation",
    "elevation",
    "game_launch",
    "runtime_execution",
    "deployment",
    "save_mutation",
    "signing",
    "publication",
    "catalog_mutation",
    "evidence_promotion",
)


class CatalogInterfaceError(RuntimeError):
    """Raised when catalogue discovery or a review-only selection fails."""


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def _hash(value: object) -> str:
    return hashlib.sha256(canonical_json_bytes(value)).hexdigest()


def _optional_text(value: object, label: str) -> str:
    if value is None:
        return ""
    return core.text(value, label, True)


def _unique_sorted(values: Iterable[str], label: str) -> list[str]:
    rows = [core.identity(value, label) for value in values]
    if len(set(rows)) != len(rows):
        raise CatalogInterfaceError(f"{label} contains duplicate values.")
    return sorted(rows)


def _validate_authority(value: object, label: str) -> None:
    authority = core.obj(value, label)
    if set(authority) != set(AUTHORITY_FIELDS):
        raise CatalogInterfaceError(f"{label} contains the wrong authority fields.")
    for field in AUTHORITY_FIELDS:
        if core.boolean(authority.get(field), f"{label}.{field}"):
            raise CatalogInterfaceError(f"{label}.{field} must remain false.")


def _suite_directories(installer: Path) -> list[Path]:
    root = core.directory(installer / "Suites", "Installer Suites root")
    result: list[Path] = []
    directory_keys: set[str] = set()
    for child in sorted(root.iterdir(), key=lambda item: (item.name.casefold(), item.name)):
        if child.name == "README.md":
            core.regular(child, "Installer Suites README")
            continue
        directory = core.directory(child, f"Suite directory {child.name}")
        if not core.DIR_RE.fullmatch(directory.name):
            raise CatalogInterfaceError(f"Invalid suite directory name: {directory.name!r}.")
        key = directory.name.casefold()
        if key in directory_keys:
            raise CatalogInterfaceError(
                f"Suite directories contain a Windows case alias: {directory.name!r}."
            )
        directory_keys.add(key)
        result.append(directory)
    if not result:
        raise CatalogInterfaceError("The installer catalogue contains no reviewed suites.")
    return result


def _package_rows(installer: Path) -> list[dict[str, object]]:
    root = core.directory(installer / "Packages", "Installer Packages root")
    rows: list[dict[str, object]] = []
    identities: set[str] = set()
    directory_keys: set[str] = set()
    for child in sorted(root.iterdir(), key=lambda item: (item.name.casefold(), item.name)):
        if child.name == "README.md":
            core.regular(child, "Installer Packages README")
            continue
        directory = core.directory(child, f"Package directory {child.name}")
        if not core.DIR_RE.fullmatch(directory.name):
            raise CatalogInterfaceError(f"Invalid package directory name: {directory.name!r}.")
        directory_key = directory.name.casefold()
        if directory_key in directory_keys:
            raise CatalogInterfaceError(
                f"Package directories contain a Windows case alias: {directory.name!r}."
            )
        directory_keys.add(directory_key)
        document, fingerprint = core.load_json(
            directory / "package.json", f"package {directory.name}"
        )
        package_id = core.identity(document.get("package_id"), f"{directory.name}.package_id")
        if package_id in identities:
            raise CatalogInterfaceError(f"Duplicate package identity {package_id!r}.")
        identities.add(package_id)

        source = core.obj(document.get("source"), f"{package_id}.source")
        compatibility = core.obj(document.get("compatibility"), f"{package_id}.compatibility")
        lifecycle = core.obj(document.get("lifecycle"), f"{package_id}.lifecycle")
        legal = core.obj(document.get("legal"), f"{package_id}.legal")
        dependencies = []
        for index, raw in enumerate(core.array(document.get("dependencies"), f"{package_id}.dependencies")):
            dependency = core.obj(raw, f"{package_id}.dependencies[{index}]")
            dependencies.append({
                "package_id": core.identity(
                    dependency.get("package_id"), f"{package_id}.dependencies[{index}].package_id"
                ),
                "version_range": core.text(
                    dependency.get("version_range"),
                    f"{package_id}.dependencies[{index}].version_range",
                ),
                "required": core.boolean(
                    dependency.get("required"), f"{package_id}.dependencies[{index}].required"
                ),
            })
        payload = core.array(document.get("payload"), f"{package_id}.payload")
        payload_size = 0
        for index, raw in enumerate(payload):
            item = core.obj(raw, f"{package_id}.payload[{index}]")
            payload_size += core.integer(
                item.get("size_bytes"), f"{package_id}.payload[{index}].size_bytes"
            )

        source_row = {
            "kind": core.text(source.get("kind"), f"{package_id}.source.kind"),
            "repository": core.text(source.get("repository"), f"{package_id}.source.repository"),
            "commit": core.text(source.get("commit"), f"{package_id}.source.commit"),
            "path": core.relative_path(source.get("path"), f"{package_id}.source.path"),
        }
        if source.get("fingerprint_sha256") is not None:
            source_row["fingerprint_sha256"] = core.text(
                source.get("fingerprint_sha256"), f"{package_id}.source.fingerprint_sha256"
            )

        rows.append({
            "package_directory": directory.name,
            "package_id": package_id,
            "display_name": core.text(document.get("display_name"), f"{package_id}.display_name"),
            "description": _optional_text(document.get("description"), f"{package_id}.description"),
            "version": core.text(document.get("version"), f"{package_id}.version"),
            "kind": core.text(document.get("kind"), f"{package_id}.kind"),
            "status": core.text(document.get("status"), f"{package_id}.status"),
            "manifest_sha256": fingerprint,
            "source": source_row,
            "compatibility": {
                field: sorted(core.strings(compatibility.get(field), f"{package_id}.compatibility.{field}"))
                for field in (
                    "platforms",
                    "architectures",
                    "game_versions",
                    "branches",
                    "runtime_targets",
                )
            },
            "dependencies": sorted(dependencies, key=lambda item: item["package_id"]),
            "conflicts": sorted(
                core.identity(value, f"{package_id}.conflict")
                for value in core.array(document.get("conflicts", []), f"{package_id}.conflicts")
            ),
            "capabilities": sorted(
                core.strings(document.get("capabilities", []), f"{package_id}.capabilities")
            ),
            "payload_summary": {
                "file_count": len(payload),
                "size_bytes": payload_size,
            },
            "lifecycle": {
                field: lifecycle[field]
                for field in (
                    "install_scope",
                    "elevation_required",
                    "repair_supported",
                    "upgrade_supported",
                    "uninstall_supported",
                    "rollback_required",
                    "preserve_external_workspaces",
                )
            },
            "legal": {
                "license_expression": core.text(
                    legal.get("license_expression"), f"{package_id}.legal.license_expression"
                ),
                "redistribution_review": core.text(
                    legal.get("redistribution_review"), f"{package_id}.legal.redistribution_review"
                ),
                "notice_files": sorted(
                    core.relative_path(value, f"{package_id}.legal.notice_file")
                    for value in core.array(legal.get("notice_files"), f"{package_id}.legal.notice_files")
                ),
            },
        })
    return sorted(rows, key=lambda item: item["package_id"])


def _suite_row(directory: Path) -> dict[str, object]:
    document, fingerprint = core.load_json(directory / "suite.json", f"suite {directory.name}")
    suite_id = core.identity(document.get("suite_id"), f"{directory.name}.suite_id")
    entries = []
    for index, raw in enumerate(core.array(document.get("packages"), f"{suite_id}.packages")):
        entry = core.obj(raw, f"{suite_id}.packages[{index}]")
        entries.append({
            "package_id": core.identity(
                entry.get("package_id"), f"{suite_id}.packages[{index}].package_id"
            ),
            "selection": core.text(
                entry.get("selection"), f"{suite_id}.packages[{index}].selection"
            ),
            "order": core.integer(entry.get("order"), f"{suite_id}.packages[{index}].order"),
            "feature_ids": sorted(
                core.identity(value, f"{suite_id}.packages[{index}].feature_id")
                for value in core.array(
                    entry.get("feature_ids", []), f"{suite_id}.packages[{index}].feature_ids"
                )
            ),
        })
    features = []
    for index, raw in enumerate(core.array(document.get("features", []), f"{suite_id}.features")):
        feature = core.obj(raw, f"{suite_id}.features[{index}]")
        feature_id = core.identity(
            feature.get("feature_id"), f"{suite_id}.features[{index}].feature_id"
        )
        features.append({
            "feature_id": feature_id,
            "display_name": core.text(feature.get("display_name"), f"{feature_id}.display_name"),
            "description": _optional_text(feature.get("description"), f"{feature_id}.description"),
            "package_ids": sorted(
                core.identity(value, f"{feature_id}.package_id")
                for value in core.array(feature.get("package_ids"), f"{feature_id}.package_ids")
            ),
        })
    compatibility = core.obj(document.get("compatibility"), f"{suite_id}.compatibility")
    policies = core.obj(document.get("policies"), f"{suite_id}.policies")
    provenance = core.obj(document.get("provenance"), f"{suite_id}.provenance")
    ordered_entries = sorted(entries, key=lambda item: (item["order"], item["package_id"]))
    return {
        "suite_directory": directory.name,
        "suite_id": suite_id,
        "display_name": core.text(document.get("display_name"), f"{suite_id}.display_name"),
        "description": _optional_text(document.get("description"), f"{suite_id}.description"),
        "version": core.text(document.get("version"), f"{suite_id}.version"),
        "channel": core.text(document.get("channel"), f"{suite_id}.channel"),
        "manifest_sha256": fingerprint,
        "compatibility": {
            "platforms": sorted(core.strings(compatibility.get("platforms"), f"{suite_id}.platforms")),
            "architectures": sorted(
                core.strings(compatibility.get("architectures"), f"{suite_id}.architectures")
            ),
        },
        "policies": {
            field: core.boolean(policies.get(field), f"{suite_id}.policies.{field}")
            for field in (
                "network_allowed",
                "elevation_allowed",
                "unreviewed_packages_allowed",
                "silent_install_allowed",
            )
        },
        "packages": ordered_entries,
        "features": sorted(features, key=lambda item: item["feature_id"]),
        "required_package_ids": sorted(
            item["package_id"] for item in ordered_entries if item["selection"] == "required"
        ),
        "default_package_ids": sorted(
            item["package_id"]
            for item in ordered_entries
            if item["selection"] in {"required", "default"}
        ),
        "optional_package_ids": sorted(
            item["package_id"] for item in ordered_entries if item["selection"] == "optional"
        ),
        "provenance": {
            "source_repository": core.text(
                provenance.get("source_repository"), f"{suite_id}.provenance.source_repository"
            ),
            "source_commit": core.text(
                provenance.get("source_commit"), f"{suite_id}.provenance.source_commit"
            ),
            "reviewed_by": core.text(
                provenance.get("reviewed_by"), f"{suite_id}.provenance.reviewed_by"
            ),
            "reviewed_at_utc": core.text(
                provenance.get("reviewed_at_utc"), f"{suite_id}.provenance.reviewed_at_utc"
            ),
            "evidence": sorted(
                core.relative_path(value, f"{suite_id}.provenance.evidence")
                for value in core.array(
                    provenance.get("evidence", []), f"{suite_id}.provenance.evidence"
                )
            ),
        },
    }


def discover_catalog(installer_root: Path) -> dict[str, object]:
    installer = core.directory(installer_root, "Installer root")
    suite_directories = _suite_directories(installer)
    for directory in suite_directories:
        validate_manifest_catalog(installer, directory.name)

    packages = _package_rows(installer)
    package_ids = {row["package_id"] for row in packages}
    suites = []
    suite_ids: set[str] = set()
    for directory in suite_directories:
        row = _suite_row(directory)
        if row["suite_id"] in suite_ids:
            raise CatalogInterfaceError(f"Duplicate suite identity {row['suite_id']!r}.")
        suite_ids.add(row["suite_id"])
        referenced = {entry["package_id"] for entry in row["packages"]}
        unknown = sorted(referenced - package_ids)
        if unknown:
            raise CatalogInterfaceError(
                f"Suite {row['suite_id']} references packages outside the catalogue: "
                + ", ".join(unknown)
            )
        suites.append(row)

    suites.sort(key=lambda item: item["suite_id"])
    base = {
        "schema_version": 1,
        "catalog_scope": "review-only",
        "suites": suites,
        "packages": packages,
        "summary": {
            "suite_count": len(suites),
            "package_count": len(packages),
            "feature_count": sum(len(row["features"]) for row in suites),
        },
        "authority": _authority(),
    }
    return {**base, "catalog_sha256": _hash(base)}


def validate_catalog(catalog: Mapping[str, object]) -> dict[str, object]:
    document = dict(catalog)
    if core.integer(document.get("schema_version"), "catalog.schema_version") != 1:
        raise CatalogInterfaceError("catalog.schema_version must be exactly 1.")
    if document.get("catalog_scope") != "review-only":
        raise CatalogInterfaceError("catalog scope must remain review-only.")
    declared = core.text(document.get("catalog_sha256"), "catalog.catalog_sha256")
    if not core.SHA_RE.fullmatch(declared):
        raise CatalogInterfaceError("catalog.catalog_sha256 must be lowercase SHA-256.")
    unsigned = {key: value for key, value in document.items() if key != "catalog_sha256"}
    if _hash(unsigned) != declared:
        raise CatalogInterfaceError("catalog fingerprint does not match its content.")
    _validate_authority(document.get("authority"), "catalog.authority")

    suites = core.array(document.get("suites"), "catalog.suites")
    packages = core.array(document.get("packages"), "catalog.packages")
    suite_ids = [
        core.identity(core.obj(row, "catalog.suite").get("suite_id"), "catalog.suite.suite_id")
        for row in suites
    ]
    package_ids = [
        core.identity(core.obj(row, "catalog.package").get("package_id"), "catalog.package.package_id")
        for row in packages
    ]
    if suite_ids != sorted(suite_ids) or len(set(suite_ids)) != len(suite_ids):
        raise CatalogInterfaceError("catalog suites must have unique sorted identities.")
    if package_ids != sorted(package_ids) or len(set(package_ids)) != len(package_ids):
        raise CatalogInterfaceError("catalog packages must have unique sorted identities.")
    package_id_set = set(package_ids)
    suite_directories: set[str] = set()
    for index, raw_suite in enumerate(suites):
        suite = core.obj(raw_suite, f"catalog.suites[{index}]")
        directory = core.text(suite.get("suite_directory"), f"catalog.suites[{index}].suite_directory")
        if not core.DIR_RE.fullmatch(directory) or directory.casefold() in suite_directories:
            raise CatalogInterfaceError("catalog suite directories must be portable and case-unique.")
        suite_directories.add(directory.casefold())
        entries = core.array(suite.get("packages"), f"catalog.suites[{index}].packages")
        entry_ids = [
            core.identity(
                core.obj(entry, "catalog.suite.package").get("package_id"),
                "catalog.suite.package.package_id",
            )
            for entry in entries
        ]
        if len(set(entry_ids)) != len(entry_ids):
            raise CatalogInterfaceError("catalog suite package entries must be unique.")
        unknown = sorted(set(entry_ids) - package_id_set)
        if unknown:
            raise CatalogInterfaceError(
                "catalog suite references unknown packages: " + ", ".join(unknown)
            )
        for raw_feature in core.array(
            suite.get("features"), f"catalog.suites[{index}].features"
        ):
            feature = core.obj(raw_feature, "catalog.suite.feature")
            feature_packages = {
                core.identity(value, "catalog.suite.feature.package_id")
                for value in core.array(
                    feature.get("package_ids"), "catalog.suite.feature.package_ids"
                )
            }
            if not feature_packages <= set(entry_ids):
                raise CatalogInterfaceError(
                    "catalog feature references packages outside its suite."
                )
    summary = core.obj(document.get("summary"), "catalog.summary")
    expected = (
        len(suites),
        len(packages),
        sum(len(core.array(core.obj(row, "catalog.suite").get("features"), "catalog.suite.features"))
            for row in suites),
    )
    actual = (
        core.integer(summary.get("suite_count"), "catalog.summary.suite_count"),
        core.integer(summary.get("package_count"), "catalog.summary.package_count"),
        core.integer(summary.get("feature_count"), "catalog.summary.feature_count"),
    )
    if actual != expected:
        raise CatalogInterfaceError("catalog summary does not match discovered content.")
    return document


def create_selection(
    catalog: Mapping[str, object],
    *,
    suite_id: str,
    platform: str,
    architecture: str,
    runtime_target: str,
    game_version: str = "",
    branch: str = "",
    selected_package_ids: Iterable[str] = (),
    excluded_package_ids: Iterable[str] = (),
    selected_feature_ids: Iterable[str] = (),
) -> dict[str, object]:
    checked = validate_catalog(catalog)
    selected_suite_id = core.identity(suite_id, "suite_id")
    suites = [
        core.obj(row, "catalog.suite")
        for row in core.array(checked.get("suites"), "catalog.suites")
        if core.obj(row, "catalog.suite").get("suite_id") == selected_suite_id
    ]
    if len(suites) != 1:
        raise CatalogInterfaceError(f"Unknown suite ID {selected_suite_id!r}.")
    suite = suites[0]
    selected = _unique_sorted(selected_package_ids, "selected package")
    excluded = _unique_sorted(excluded_package_ids, "excluded package")
    features = _unique_sorted(selected_feature_ids, "selected feature")
    if set(selected) & set(excluded):
        raise CatalogInterfaceError("A package cannot be both selected and excluded.")

    entry_ids = {
        core.identity(core.obj(row, "suite.package").get("package_id"), "suite.package.package_id")
        for row in core.array(suite.get("packages"), "suite.packages")
    }
    feature_ids = {
        core.identity(core.obj(row, "suite.feature").get("feature_id"), "suite.feature.feature_id")
        for row in core.array(suite.get("features"), "suite.features")
    }
    unknown_packages = sorted((set(selected) | set(excluded)) - entry_ids)
    if unknown_packages:
        raise CatalogInterfaceError(
            "Selection references packages outside the suite: " + ", ".join(unknown_packages)
        )
    unknown_features = sorted(set(features) - feature_ids)
    if unknown_features:
        raise CatalogInterfaceError("Selection references unknown features: " + ", ".join(unknown_features))

    base = {
        "schema_version": 1,
        "selection_scope": "review-only",
        "catalog_sha256": checked["catalog_sha256"],
        "suite_id": selected_suite_id,
        "suite_directory": core.text(suite.get("suite_directory"), "suite.suite_directory"),
        "context": {
            "platform": core.text(platform, "platform"),
            "architecture": core.text(architecture, "architecture"),
            "runtime_target": core.text(runtime_target, "runtime_target"),
            "game_version": core.text(game_version, "game_version", True),
            "branch": core.text(branch, "branch", True),
        },
        "selected_package_ids": selected,
        "excluded_package_ids": excluded,
        "selected_feature_ids": features,
        "authority": _authority(),
    }
    return {**base, "selection_sha256": _hash(base)}


def validate_selection(selection: Mapping[str, object]) -> dict[str, object]:
    document = dict(selection)
    if core.integer(document.get("schema_version"), "selection.schema_version") != 1:
        raise CatalogInterfaceError("selection.schema_version must be exactly 1.")
    if document.get("selection_scope") != "review-only":
        raise CatalogInterfaceError("selection scope must remain review-only.")
    declared = core.text(document.get("selection_sha256"), "selection.selection_sha256")
    if not core.SHA_RE.fullmatch(declared):
        raise CatalogInterfaceError("selection.selection_sha256 must be lowercase SHA-256.")
    unsigned = {key: value for key, value in document.items() if key != "selection_sha256"}
    if _hash(unsigned) != declared:
        raise CatalogInterfaceError("selection fingerprint does not match its content.")
    catalog_hash = core.text(document.get("catalog_sha256"), "selection.catalog_sha256")
    if not core.SHA_RE.fullmatch(catalog_hash):
        raise CatalogInterfaceError("selection.catalog_sha256 must be lowercase SHA-256.")
    core.identity(document.get("suite_id"), "selection.suite_id")
    suite_directory = core.text(document.get("suite_directory"), "selection.suite_directory")
    if not core.DIR_RE.fullmatch(suite_directory):
        raise CatalogInterfaceError("selection.suite_directory is not a portable directory name.")
    context = core.obj(document.get("context"), "selection.context")
    core.text(context.get("platform"), "selection.context.platform")
    core.text(context.get("architecture"), "selection.context.architecture")
    core.text(context.get("runtime_target"), "selection.context.runtime_target")
    core.text(context.get("game_version"), "selection.context.game_version", True)
    core.text(context.get("branch"), "selection.context.branch", True)
    selected = _unique_sorted(
        core.strings(document.get("selected_package_ids"), "selection.selected_package_ids"),
        "selection.selected_package_id",
    )
    excluded = _unique_sorted(
        core.strings(document.get("excluded_package_ids"), "selection.excluded_package_ids"),
        "selection.excluded_package_id",
    )
    features = _unique_sorted(
        core.strings(document.get("selected_feature_ids"), "selection.selected_feature_ids"),
        "selection.selected_feature_id",
    )
    if selected != document["selected_package_ids"]:
        raise CatalogInterfaceError("selected package IDs must be uniquely sorted.")
    if excluded != document["excluded_package_ids"]:
        raise CatalogInterfaceError("excluded package IDs must be uniquely sorted.")
    if features != document["selected_feature_ids"]:
        raise CatalogInterfaceError("selected feature IDs must be uniquely sorted.")
    if set(selected) & set(excluded):
        raise CatalogInterfaceError("A package cannot be both selected and excluded.")
    _validate_authority(document.get("authority"), "selection.authority")
    return document


def resolve_selection(installer_root: Path, selection: Mapping[str, object]) -> dict[str, object]:
    checked = validate_selection(selection)
    catalog = discover_catalog(installer_root)
    if checked["catalog_sha256"] != catalog["catalog_sha256"]:
        raise CatalogInterfaceError(
            "The reviewed catalogue changed after selection; rediscover it before resolving."
        )
    suites = [
        core.obj(row, "catalog.suite")
        for row in core.array(catalog.get("suites"), "catalog.suites")
        if core.obj(row, "catalog.suite").get("suite_id") == checked["suite_id"]
    ]
    if len(suites) != 1 or suites[0].get("suite_directory") != checked["suite_directory"]:
        raise CatalogInterfaceError("The selected suite is missing or its directory identity changed.")
    context = core.obj(checked.get("context"), "selection.context")
    plan = resolve_suite(
        Path(installer_root),
        checked["suite_directory"],
        ResolutionContext(
            context["platform"],
            context["architecture"],
            context["runtime_target"],
            context["game_version"],
            context["branch"],
        ),
        selected_package_ids=checked["selected_package_ids"],
        excluded_package_ids=checked["excluded_package_ids"],
        selected_feature_ids=checked["selected_feature_ids"],
    )
    view_model = build_view_model(plan)
    base = {
        "schema_version": 1,
        "result_scope": "review-only",
        "catalog_sha256": catalog["catalog_sha256"],
        "selection_sha256": checked["selection_sha256"],
        "plan": plan,
        "view_model": view_model,
        "authority": _authority(),
    }
    return {**base, "result_sha256": _hash(base)}


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Discover reviewed FOA-SDK installer choices or resolve one review-only selection."
    )
    commands = parser.add_subparsers(dest="command", required=True)
    discover = commands.add_parser("discover", help="Emit the canonical reviewed catalogue.")
    discover.add_argument("--installer-root", type=Path, required=True)

    resolve = commands.add_parser("resolve", help="Resolve explicit choices against an exact catalogue.")
    resolve.add_argument("--installer-root", type=Path, required=True)
    resolve.add_argument("--expected-catalog-sha256", required=True)
    resolve.add_argument("--suite-id", required=True)
    resolve.add_argument("--platform", required=True)
    resolve.add_argument("--architecture", required=True)
    resolve.add_argument("--runtime-target", required=True)
    resolve.add_argument("--game-version", default="")
    resolve.add_argument("--branch", default="")
    resolve.add_argument("--select", action="append", default=[])
    resolve.add_argument("--exclude", action="append", default=[])
    resolve.add_argument("--feature", action="append", default=[])
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        catalog = discover_catalog(arguments.installer_root)
        if arguments.command == "discover":
            sys.stdout.buffer.write(canonical_json_bytes(catalog))
            return 0
        if arguments.expected_catalog_sha256 != catalog["catalog_sha256"]:
            raise CatalogInterfaceError(
                "Expected catalogue fingerprint does not match the current reviewed catalogue."
            )
        selection = create_selection(
            catalog,
            suite_id=arguments.suite_id,
            platform=arguments.platform,
            architecture=arguments.architecture,
            runtime_target=arguments.runtime_target,
            game_version=arguments.game_version,
            branch=arguments.branch,
            selected_package_ids=arguments.select,
            excluded_package_ids=arguments.exclude,
            selected_feature_ids=arguments.feature,
        )
        sys.stdout.buffer.write(canonical_json_bytes(resolve_selection(arguments.installer_root, selection)))
        return 0
    except (OSError, ResolverError, WizardContractError, CatalogInterfaceError) as exc:
        print(f"Suite Wizard catalogue interface failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
