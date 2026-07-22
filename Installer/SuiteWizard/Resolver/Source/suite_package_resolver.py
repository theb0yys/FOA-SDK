#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Validate installer manifests and resolve one suite into a canonical dry-run plan."""

from __future__ import annotations

import datetime as dt
import re
import sys
from pathlib import Path
from typing import Iterable, Sequence
from urllib.parse import urlparse

import _resolver_core as core

ResolverError = core.ResolverError
SemVer = core.SemVer
VersionRange = core.VersionRange
Context = core.Context
ResolutionContext = core.ResolutionContext
canonical_json = core.canonical_json
canonical_json_bytes = core.canonical_json_bytes
sha256 = core.sha256

PACKAGE_KINDS = {
    "core-foundation",
    "editor-project",
    "plugin",
    "integration",
    "runtime-adapter",
    "external-tool-provider",
    "documentation",
    "sample",
}
PACKAGE_STATUSES = {
    "planned",
    "experimental",
    "preview",
    "supported",
    "deprecated",
    "blocked",
}
SOURCE_KINDS = {"product", "plugin", "reviewed-external", "generated"}
RUNTIME_TARGETS = {"editor-only", "mono", "il2cpp", "runtime-neutral"}
INSTALL_SCOPES = {"per-user", "per-machine", "portable"}
REDISTRIBUTION_STATES = {"project-owned", "approved", "notice-required", "blocked"}
REVIEW_STATES = {"required", "approved", "blocked", "not-applicable"}
SUITE_CHANNELS = {"development", "alpha", "beta", "release-candidate", "stable"}
SELECTION_STATES = {"required", "default", "optional"}
CAPABILITY_RE = re.compile(r"^[a-z][a-z0-9]*(?:[._-][a-z0-9]+)*$")
GIT_SHA_RE = re.compile(r"^[0-9a-f]{40}$")


def _enum(value: object, label: str, allowed: set[str]) -> str:
    result = core.text(value, label)
    if result not in allowed:
        raise ResolverError(
            f"{label} must be one of: {', '.join(sorted(allowed))}; got {result!r}."
        )
    return result


def _schema_version(document: dict[str, object], label: str) -> None:
    version = core.integer(document.get("schema_version"), f"{label}.schema_version")
    if version != 1:
        raise ResolverError(f"{label}.schema_version must be exactly 1; got {version}.")


def _uri(value: object, label: str) -> str:
    result = core.text(value, label)
    parsed = urlparse(result)
    if parsed.scheme not in {"https", "http"} or not parsed.netloc:
        raise ResolverError(f"{label} must be an absolute HTTP(S) URI.")
    return result


def _git_sha(value: object, label: str) -> str:
    result = core.text(value, label)
    if not GIT_SHA_RE.fullmatch(result):
        raise ResolverError(f"{label} must be an exact lowercase 40-character Git SHA.")
    return result


def _utc(value: object, label: str) -> str:
    result = core.text(value, label)
    if not result.endswith("Z"):
        raise ResolverError(f"{label} must use UTC and end in 'Z'.")
    try:
        parsed = dt.datetime.fromisoformat(result[:-1] + "+00:00")
    except ValueError as exc:
        raise ResolverError(f"{label} is not a valid ISO-8601 UTC timestamp.") from exc
    if parsed.utcoffset() != dt.timedelta(0):
        raise ResolverError(f"{label} must use UTC.")
    return result


def _nonempty_unique_strings(value: object, label: str) -> tuple[str, ...]:
    result = core.strings(value, label)
    if not result:
        raise ResolverError(f"{label} must contain at least one value.")
    return result


def _validate_suite_document(document: dict[str, object], label: str) -> None:
    _schema_version(document, label)
    suite_id = core.identity(document.get("suite_id"), f"{label}.suite_id")
    core.text(document.get("display_name"), f"{suite_id}.display_name")
    core.SemVer.parse(core.text(document.get("version"), f"{suite_id}.version"), f"{suite_id}.version")
    _enum(document.get("channel"), f"{suite_id}.channel", SUITE_CHANNELS)

    compatibility = core.obj(document.get("compatibility"), f"{suite_id}.compatibility")
    platforms = _nonempty_unique_strings(
        compatibility.get("platforms"), f"{suite_id}.compatibility.platforms"
    )
    architectures = _nonempty_unique_strings(
        compatibility.get("architectures"), f"{suite_id}.compatibility.architectures"
    )
    if any(value != "windows" for value in platforms):
        raise ResolverError(f"{suite_id} currently supports only the Windows host platform.")
    if any(value != "x86_64" for value in architectures):
        raise ResolverError(f"{suite_id} currently supports only the x86_64 host architecture.")

    entries = core.array(document.get("packages"), f"{suite_id}.packages")
    if not entries:
        raise ResolverError(f"{suite_id}.packages must contain at least one package entry.")
    package_ids: set[str] = set()
    orders: set[int] = set()
    entry_feature_ids: dict[str, tuple[str, ...]] = {}
    for index, raw in enumerate(entries):
        entry = core.obj(raw, f"{suite_id}.packages[{index}]")
        package_id = core.identity(
            entry.get("package_id"), f"{suite_id}.packages[{index}].package_id"
        )
        if package_id in package_ids:
            raise ResolverError(f"{suite_id} contains duplicate package entry {package_id}.")
        package_ids.add(package_id)
        _enum(entry.get("selection"), f"{suite_id}.{package_id}.selection", SELECTION_STATES)
        order = core.integer(entry.get("order"), f"{suite_id}.{package_id}.order")
        if order < 0 or order in orders:
            raise ResolverError(f"{suite_id} contains a negative or duplicate package order {order}.")
        orders.add(order)
        entry_feature_ids[package_id] = tuple(
            core.identity(value, f"{suite_id}.{package_id}.feature_id")
            for value in core.array(entry.get("feature_ids", []), f"{suite_id}.{package_id}.feature_ids")
        )
        if len(set(entry_feature_ids[package_id])) != len(entry_feature_ids[package_id]):
            raise ResolverError(f"{suite_id}.{package_id}.feature_ids contains duplicates.")

    features = core.array(document.get("features", []), f"{suite_id}.features")
    feature_ids: set[str] = set()
    for index, raw in enumerate(features):
        feature = core.obj(raw, f"{suite_id}.features[{index}]")
        feature_id = core.identity(
            feature.get("feature_id"), f"{suite_id}.features[{index}].feature_id"
        )
        if feature_id in feature_ids:
            raise ResolverError(f"{suite_id} contains duplicate feature {feature_id}.")
        feature_ids.add(feature_id)
        core.text(feature.get("display_name"), f"{feature_id}.display_name")
        referenced = tuple(
            core.identity(value, f"{feature_id}.package_id")
            for value in core.array(feature.get("package_ids"), f"{feature_id}.package_ids")
        )
        if not referenced or len(set(referenced)) != len(referenced):
            raise ResolverError(f"{feature_id}.package_ids must be non-empty and unique.")
        unknown = sorted(set(referenced) - package_ids)
        if unknown:
            raise ResolverError(
                f"Feature {feature_id} references packages outside the suite: {', '.join(unknown)}."
            )

    for package_id, declared_features in sorted(entry_feature_ids.items()):
        unknown = sorted(set(declared_features) - feature_ids)
        if unknown:
            raise ResolverError(
                f"Suite package {package_id} references unknown features: {', '.join(unknown)}."
            )
        for feature_id in declared_features:
            matching = next(
                core.obj(raw, f"{suite_id}.feature")
                for raw in features
                if core.identity(core.obj(raw, f"{suite_id}.feature").get("feature_id"), "feature_id")
                == feature_id
            )
            referenced = {
                core.identity(value, f"{feature_id}.package_id")
                for value in core.array(matching.get("package_ids"), f"{feature_id}.package_ids")
            }
            if package_id not in referenced:
                raise ResolverError(
                    f"Suite package {package_id} declares feature {feature_id}, but the feature does not include it."
                )

    policies = core.obj(document.get("policies"), f"{suite_id}.policies")
    for field in (
        "network_allowed",
        "elevation_allowed",
        "unreviewed_packages_allowed",
        "silent_install_allowed",
    ):
        core.boolean(policies.get(field), f"{suite_id}.policies.{field}")
    if policies.get("unreviewed_packages_allowed") is not False:
        raise ResolverError(f"{suite_id} must keep unreviewed_packages_allowed false.")

    provenance = core.obj(document.get("provenance"), f"{suite_id}.provenance")
    _uri(provenance.get("source_repository"), f"{suite_id}.provenance.source_repository")
    _git_sha(provenance.get("source_commit"), f"{suite_id}.provenance.source_commit")
    core.text(provenance.get("reviewed_by"), f"{suite_id}.provenance.reviewed_by")
    _utc(provenance.get("reviewed_at_utc"), f"{suite_id}.provenance.reviewed_at_utc")
    core.strings(provenance.get("evidence", []), f"{suite_id}.provenance.evidence")


def _validate_package_document(document: dict[str, object], label: str) -> None:
    _schema_version(document, label)
    package_id = core.identity(document.get("package_id"), f"{label}.package_id")
    core.text(document.get("display_name"), f"{package_id}.display_name")
    core.SemVer.parse(
        core.text(document.get("version"), f"{package_id}.version"), f"{package_id}.version"
    )
    _enum(document.get("kind"), f"{package_id}.kind", PACKAGE_KINDS)
    _enum(document.get("status"), f"{package_id}.status", PACKAGE_STATUSES)

    source = core.obj(document.get("source"), f"{package_id}.source")
    _enum(source.get("kind"), f"{package_id}.source.kind", SOURCE_KINDS)
    _uri(source.get("repository"), f"{package_id}.source.repository")
    _git_sha(source.get("commit"), f"{package_id}.source.commit")
    core.relative_path(source.get("path"), f"{package_id}.source.path")
    fingerprint = source.get("fingerprint_sha256")
    if fingerprint is not None:
        value = core.text(fingerprint, f"{package_id}.source.fingerprint_sha256")
        if not core.SHA_RE.fullmatch(value):
            raise ResolverError(f"{package_id}.source.fingerprint_sha256 must be lowercase SHA-256.")

    compatibility = core.obj(document.get("compatibility"), f"{package_id}.compatibility")
    platforms = _nonempty_unique_strings(
        compatibility.get("platforms"), f"{package_id}.compatibility.platforms"
    )
    architectures = _nonempty_unique_strings(
        compatibility.get("architectures"), f"{package_id}.compatibility.architectures"
    )
    if any(value != "windows" for value in platforms):
        raise ResolverError(f"{package_id} currently supports only the Windows host platform.")
    if any(value != "x86_64" for value in architectures):
        raise ResolverError(f"{package_id} currently supports only the x86_64 host architecture.")
    core.strings(compatibility.get("game_versions"), f"{package_id}.compatibility.game_versions")
    core.strings(compatibility.get("branches"), f"{package_id}.compatibility.branches")
    runtime_targets = core.strings(
        compatibility.get("runtime_targets"), f"{package_id}.compatibility.runtime_targets"
    )
    unknown_runtime_targets = sorted(set(runtime_targets) - RUNTIME_TARGETS)
    if unknown_runtime_targets:
        raise ResolverError(
            f"{package_id} contains unknown runtime targets: {', '.join(unknown_runtime_targets)}."
        )

    dependency_ids: set[str] = set()
    for index, raw in enumerate(
        core.array(document.get("dependencies"), f"{package_id}.dependencies")
    ):
        dependency = core.obj(raw, f"{package_id}.dependencies[{index}]")
        dependency_id = core.identity(
            dependency.get("package_id"), f"{package_id}.dependencies[{index}].package_id"
        )
        if dependency_id == package_id or dependency_id in dependency_ids:
            raise ResolverError(
                f"{package_id} contains a self or duplicate dependency {dependency_id}."
            )
        dependency_ids.add(dependency_id)
        core.VersionRange.parse(
            core.text(
                dependency.get("version_range"),
                f"{package_id}.dependencies[{index}].version_range",
            ),
            f"{package_id} dependency {dependency_id}",
        )
        core.boolean(
            dependency.get("required"), f"{package_id}.dependencies[{index}].required"
        )

    conflicts = tuple(
        core.identity(value, f"{package_id}.conflict")
        for value in core.array(document.get("conflicts", []), f"{package_id}.conflicts")
    )
    if package_id in conflicts or len(set(conflicts)) != len(conflicts):
        raise ResolverError(f"{package_id}.conflicts contains a self or duplicate conflict.")

    capabilities = core.strings(
        document.get("capabilities", []), f"{package_id}.capabilities"
    )
    invalid_capabilities = sorted(value for value in capabilities if not CAPABILITY_RE.fullmatch(value))
    if invalid_capabilities:
        raise ResolverError(
            f"{package_id} contains invalid capabilities: {', '.join(invalid_capabilities)}."
        )

    payload_entries = core.array(document.get("payload"), f"{package_id}.payload")
    if not payload_entries:
        raise ResolverError(f"{package_id}.payload must contain at least one file.")
    for index, raw in enumerate(payload_entries):
        payload = core.obj(raw, f"{package_id}.payload[{index}]")
        core.relative_path(payload.get("source"), f"{package_id}.payload[{index}].source")
        core.relative_path(
            payload.get("destination"), f"{package_id}.payload[{index}].destination"
        )
        checksum = core.text(payload.get("sha256"), f"{package_id}.payload[{index}].sha256")
        if not core.SHA_RE.fullmatch(checksum):
            raise ResolverError(f"{package_id}.payload[{index}].sha256 must be lowercase SHA-256.")
        size = core.integer(payload.get("size_bytes"), f"{package_id}.payload[{index}].size_bytes")
        if size < 0:
            raise ResolverError(f"{package_id}.payload[{index}].size_bytes must be non-negative.")
        _enum(
            payload.get("redistribution"),
            f"{package_id}.payload[{index}].redistribution",
            REDISTRIBUTION_STATES,
        )

    lifecycle = core.obj(document.get("lifecycle"), f"{package_id}.lifecycle")
    _enum(lifecycle.get("install_scope"), f"{package_id}.lifecycle.install_scope", INSTALL_SCOPES)
    for field in (
        "elevation_required",
        "repair_supported",
        "upgrade_supported",
        "uninstall_supported",
        "rollback_required",
        "preserve_external_workspaces",
    ):
        core.boolean(lifecycle.get(field), f"{package_id}.lifecycle.{field}")
    if lifecycle.get("preserve_external_workspaces") is not True:
        raise ResolverError(f"{package_id} must preserve external workspaces.")

    legal = core.obj(document.get("legal"), f"{package_id}.legal")
    core.text(legal.get("license_expression"), f"{package_id}.legal.license_expression")
    _enum(
        legal.get("redistribution_review"),
        f"{package_id}.legal.redistribution_review",
        REVIEW_STATES,
    )
    for index, value in enumerate(
        core.array(legal.get("notice_files"), f"{package_id}.legal.notice_files")
    ):
        core.relative_path(value, f"{package_id}.legal.notice_files[{index}]")

    authority = core.obj(document.get("authority"), f"{package_id}.authority")
    for field in core.AUTHORITY:
        if core.boolean(authority.get(field), f"{package_id}.authority.{field}"):
            raise ResolverError(f"{package_id} illegally grants authority {field}.")


def validate_manifest_catalog(installer_root: Path, suite_name: str) -> None:
    installer = core.directory(installer_root, "Installer root")
    if not core.DIR_RE.fullmatch(suite_name):
        raise ResolverError(f"Invalid suite directory name: {suite_name!r}")
    suite_document, _ = core.load_json(
        core.directory(installer / "Suites" / suite_name, f"Suite {suite_name}") / "suite.json",
        f"suite {suite_name}",
    )
    _validate_suite_document(suite_document, f"suite {suite_name}")

    packages_root = core.directory(installer / "Packages", "Installer Packages root")
    package_ids: set[str] = set()
    for child in sorted(packages_root.iterdir(), key=lambda item: item.name.casefold()):
        if child.name == "README.md":
            core.regular(child, "Installer Packages README")
            continue
        package_directory = core.directory(child, f"Package directory {child.name}")
        if not core.DIR_RE.fullmatch(package_directory.name):
            raise ResolverError(f"Invalid package directory name: {package_directory.name!r}")
        document, _ = core.load_json(
            package_directory / "package.json", f"package {package_directory.name}"
        )
        _validate_package_document(document, f"package {package_directory.name}")
        package_id = core.identity(
            document.get("package_id"), f"{package_directory.name}.package_id"
        )
        if package_id in package_ids:
            raise ResolverError(f"Duplicate package_id {package_id!r}.")
        package_ids.add(package_id)


def resolve(
    installer: Path,
    suite_name: str,
    context: Context,
    *,
    selected: Iterable[str] = (),
    excluded: Iterable[str] = (),
    features: Iterable[str] = (),
) -> dict[str, object]:
    validate_manifest_catalog(installer, suite_name)
    return core.resolve(
        installer,
        suite_name,
        context,
        selected=selected,
        excluded=excluded,
        features=features,
    )


def resolve_suite(
    installer_root: Path,
    suite_name: str,
    context: Context,
    *,
    selected_package_ids: Iterable[str] = (),
    excluded_package_ids: Iterable[str] = (),
    selected_feature_ids: Iterable[str] = (),
) -> dict[str, object]:
    return resolve(
        installer_root,
        suite_name,
        context,
        selected=selected_package_ids,
        excluded=excluded_package_ids,
        features=selected_feature_ids,
    )


def main(argv: Sequence[str] | None = None) -> int:
    arguments = core.parse_args(argv)
    try:
        plan = resolve(
            arguments.installer_root,
            arguments.suite,
            Context(
                core.text(arguments.platform, "platform"),
                core.text(arguments.architecture, "architecture"),
                core.text(arguments.runtime_target, "runtime target"),
                core.text(arguments.game_version, "game version", True),
                core.text(arguments.branch, "branch", True),
            ),
            selected=arguments.select,
            excluded=arguments.exclude,
            features=arguments.feature,
        )
        data = canonical_json(plan)
        if arguments.output:
            core.atomic_write(arguments.output, data)
        else:
            sys.stdout.buffer.write(data)
        return 0
    except (OSError, ResolverError) as exc:
        print(f"Installer suite resolution failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
