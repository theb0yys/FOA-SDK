#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Strict import-only implementation for deterministic installer resolution."""

from __future__ import annotations

import argparse
import hashlib
import heapq
import json
import os
import re
import stat
import sys
import tempfile
from dataclasses import dataclass
from functools import total_ordering
from pathlib import Path, PurePosixPath
from typing import Iterable, Sequence

SEMVER_RE = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
    r"(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$"
)
ID_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
DIR_RE = re.compile(r"^[A-Za-z][A-Za-z0-9._-]{0,127}$")
SHA_RE = re.compile(r"^[0-9a-f]{64}$")
CONTROL_RE = re.compile(r"[\x00-\x1f\x7f]")
RANGE_RE = re.compile(r"^(<=|>=|==|=|<|>)?(.+)$")
WINDOWS_BAD = frozenset('<>:"|?*')
WINDOWS_RESERVED = {
    "con",
    "prn",
    "aux",
    "nul",
    *(f"com{i}" for i in range(1, 10)),
    *(f"lpt{i}" for i in range(1, 10)),
}
AUTHORITY = (
    "game_launch",
    "runtime_execution",
    "deployment",
    "save_mutation",
    "signing",
    "publication",
    "catalog_mutation",
    "evidence_promotion",
)


class ResolverError(RuntimeError):
    """Raised when a manifest or resolution request fails closed."""


@total_ordering
@dataclass(frozen=True)
class SemVer:
    major: int
    minor: int
    patch: int
    pre: tuple[str, ...] = ()

    @classmethod
    def parse(cls, value: str, label: str = "version") -> "SemVer":
        match = SEMVER_RE.fullmatch(value)
        if not match:
            raise ResolverError(f"{label} is not valid SemVer: {value!r}")
        prerelease = tuple(match.group(4).split(".")) if match.group(4) else ()
        if any(
            part.isdigit() and len(part) > 1 and part.startswith("0")
            for part in prerelease
        ):
            raise ResolverError(
                f"{label} has a numeric prerelease identifier with a leading zero."
            )
        return cls(
            int(match.group(1)),
            int(match.group(2)),
            int(match.group(3)),
            prerelease,
        )

    def __eq__(self, other: object) -> bool:
        return isinstance(other, SemVer) and (
            self.major,
            self.minor,
            self.patch,
            self.pre,
        ) == (
            other.major,
            other.minor,
            other.patch,
            other.pre,
        )

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, SemVer):
            return NotImplemented
        release = (self.major, self.minor, self.patch)
        other_release = (other.major, other.minor, other.patch)
        if release != other_release:
            return release < other_release
        if not self.pre:
            return False
        if not other.pre:
            return True
        for left, right in zip(self.pre, other.pre):
            if left == right:
                continue
            left_numeric = left.isdigit()
            right_numeric = right.isdigit()
            if left_numeric and right_numeric:
                return int(left) < int(right)
            if left_numeric != right_numeric:
                return left_numeric
            return left < right
        return len(self.pre) < len(other.pre)


@dataclass(frozen=True)
class VersionRange:
    source: str
    clauses: tuple[tuple[str, SemVer], ...]

    @classmethod
    def parse(cls, value: str, label: str) -> "VersionRange":
        value = value.strip()
        if value == "*":
            return cls(value, ())
        if not value or any(token in value for token in ("||", "^", "~")):
            raise ResolverError(
                f"{label} uses unsupported version-range syntax {value!r}; "
                "use '*', exact SemVer, or space-separated comparators."
            )
        clauses: list[tuple[str, SemVer]] = []
        for token in value.replace(",", " ").split():
            match = RANGE_RE.fullmatch(token)
            if not match:
                raise ResolverError(f"{label} contains invalid range token {token!r}.")
            clauses.append(
                (
                    match.group(1) or "==",
                    SemVer.parse(match.group(2), label),
                )
            )
        return cls(value, tuple(clauses))

    def matches(self, version: SemVer) -> bool:
        for operator, expected in self.clauses:
            if operator in ("=", "==") and version != expected:
                return False
            if operator == ">" and not version > expected:
                return False
            if operator == ">=" and not version >= expected:
                return False
            if operator == "<" and not version < expected:
                return False
            if operator == "<=" and not version <= expected:
                return False
        return True


@dataclass(frozen=True)
class Context:
    platform: str
    architecture: str
    runtime_target: str
    game_version: str = ""
    branch: str = ""


ResolutionContext = Context


def canonical_json(value: object) -> bytes:
    return (
        json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        + "\n"
    ).encode()


canonical_json_bytes = canonical_json


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def is_reparse(path: Path) -> bool:
    try:
        return bool(path.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT)
    except AttributeError:
        return False
    except OSError as exc:
        raise ResolverError(f"Unable to inspect {path}: {exc}") from exc


def directory(path: Path, label: str) -> Path:
    if path.is_symlink() or is_reparse(path) or not path.is_dir():
        raise ResolverError(f"{label} is missing, not a directory, or unsafe: {path}")
    return path.resolve(strict=True)


def regular(path: Path, label: str) -> Path:
    if path.is_symlink() or is_reparse(path) or not path.is_file():
        raise ResolverError(f"{label} is missing, not a regular file, or unsafe: {path}")
    return path


def load_json(path: Path, label: str) -> tuple[dict[str, object], str]:
    raw = regular(path, label).read_bytes()

    def reject_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise ResolverError(f"Duplicate JSON key {key!r} in {label}.")
            result[key] = value
        return result

    try:
        document = json.loads(
            raw.decode("utf-8"),
            object_pairs_hook=reject_duplicates,
        )
    except ResolverError:
        raise
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ResolverError(f"Unable to parse {label}: {exc}") from exc
    if not isinstance(document, dict):
        raise ResolverError(f"{label} must be a JSON object.")
    return document, sha256(raw)


def text(value: object, label: str, empty: bool = False) -> str:
    if not isinstance(value, str):
        raise ResolverError(f"{label} must be a string.")
    result = value.strip()
    if (not empty and not result) or CONTROL_RE.search(result):
        raise ResolverError(f"{label} is empty or contains control characters.")
    return result


def boolean(value: object, label: str) -> bool:
    if not isinstance(value, bool):
        raise ResolverError(f"{label} must be a boolean.")
    return value


def integer(value: object, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ResolverError(f"{label} must be an integer.")
    return value


def array(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise ResolverError(f"{label} must be an array.")
    return value


def obj(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ResolverError(f"{label} must be an object.")
    return value


def identity(value: object, label: str) -> str:
    result = text(value, label)
    if not ID_RE.fullmatch(result):
        raise ResolverError(f"{label} is not a namespaced ID: {result!r}")
    return result


def strings(value: object, label: str) -> tuple[str, ...]:
    result = tuple(
        text(item, f"{label}[{index}]")
        for index, item in enumerate(array(value, label))
    )
    if len(set(result)) != len(result):
        raise ResolverError(f"{label} contains duplicates.")
    return result


def relative_path(value: object, label: str) -> str:
    result = text(value, label)
    if "\\" in result:
        raise ResolverError(f"{label} must use forward slashes.")
    path = PurePosixPath(result)
    if path.is_absolute() or not path.parts or any(
        part in ("", ".", "..") for part in path.parts
    ):
        raise ResolverError(f"{label} is not a canonical relative path: {result!r}")
    for part in path.parts:
        if any(character in WINDOWS_BAD for character in part) or part.endswith((" ", ".")):
            raise ResolverError(
                f"{label} contains a Windows-unsafe component: {result!r}"
            )
        if part.split(".", 1)[0].casefold() in WINDOWS_RESERVED:
            raise ResolverError(
                f"{label} uses a reserved Windows device name: {result!r}"
            )
    return path.as_posix()


def path_key(value: str) -> str:
    return "/".join(
        part.rstrip(" .").casefold() for part in PurePosixPath(value).parts
    )


def compatibility(
    document: dict[str, object],
    label: str,
) -> dict[str, tuple[str, ...]]:
    return {
        key: strings(document.get(key), f"{label}.{key}")
        for key in (
            "platforms",
            "architectures",
            "game_versions",
            "branches",
            "runtime_targets",
        )
    }


def match_context(
    label: str,
    allowed: dict[str, tuple[str, ...]],
    context: Context,
) -> None:
    actual = {
        "platforms": context.platform,
        "architectures": context.architecture,
        "game_versions": context.game_version,
        "branches": context.branch,
        "runtime_targets": context.runtime_target,
    }
    for key, values in allowed.items():
        if not values:
            continue
        selected = actual[key]
        readable = key[:-1].replace("_", " ")
        if not selected:
            raise ResolverError(
                f"{label} requires explicit {readable} context; "
                f"allowed: {', '.join(values)}."
            )
        if selected not in values:
            raise ResolverError(
                f"{label} rejects {readable} {selected!r}; "
                f"allowed: {', '.join(values)}."
            )


def load_packages(installer: Path) -> dict[str, dict[str, object]]:
    packages_root = directory(installer / "Packages", "Installer Packages root")
    packages: dict[str, dict[str, object]] = {}
    for child in sorted(packages_root.iterdir(), key=lambda item: item.name.casefold()):
        if child.name == "README.md":
            regular(child, "Installer Packages README")
            continue
        folder = directory(child, f"Package directory {child.name}")
        document, fingerprint = load_json(
            folder / "package.json",
            f"package {folder.name}",
        )
        package_id = identity(document.get("package_id"), f"{folder.name}.package_id")
        if package_id in packages:
            raise ResolverError(f"Duplicate package_id {package_id!r}.")
        version_text = text(document.get("version"), f"{package_id}.version")
        version = SemVer.parse(version_text, f"{package_id}.version")
        source = obj(document.get("source"), f"{package_id}.source")

        dependencies: list[dict[str, object]] = []
        for index, raw in enumerate(
            array(document.get("dependencies"), f"{package_id}.dependencies")
        ):
            dependency = obj(raw, f"{package_id}.dependencies[{index}]")
            dependency_id = identity(
                dependency.get("package_id"),
                f"{package_id}.dependencies[{index}].package_id",
            )
            dependencies.append(
                {
                    "package_id": dependency_id,
                    "range": VersionRange.parse(
                        text(
                            dependency.get("version_range"),
                            f"{package_id}.{dependency_id}.range",
                        ),
                        f"{package_id} dependency {dependency_id}",
                    ),
                    "required": boolean(
                        dependency.get("required"),
                        f"{package_id}.{dependency_id}.required",
                    ),
                }
            )

        payload: list[dict[str, object]] = []
        for index, raw in enumerate(
            array(document.get("payload"), f"{package_id}.payload")
        ):
            item = obj(raw, f"{package_id}.payload[{index}]")
            payload.append(
                {
                    "source": relative_path(
                        item.get("source"),
                        f"{package_id}.payload[{index}].source",
                    ),
                    "destination": relative_path(
                        item.get("destination"),
                        f"{package_id}.payload[{index}].destination",
                    ),
                    "sha256": text(
                        item.get("sha256"),
                        f"{package_id}.payload[{index}].sha256",
                    ),
                    "size_bytes": integer(
                        item.get("size_bytes"),
                        f"{package_id}.payload[{index}].size_bytes",
                    ),
                    "redistribution": text(
                        item.get("redistribution"),
                        f"{package_id}.payload[{index}].redistribution",
                    ),
                }
            )

        lifecycle = obj(document.get("lifecycle"), f"{package_id}.lifecycle")
        legal = obj(document.get("legal"), f"{package_id}.legal")
        source_fingerprint = source.get("fingerprint_sha256")
        source_view: dict[str, object] = {
            "kind": text(source.get("kind"), f"{package_id}.source.kind"),
            "repository": text(
                source.get("repository"),
                f"{package_id}.source.repository",
            ),
            "commit": text(source.get("commit"), f"{package_id}.source.commit"),
            "path": relative_path(
                source.get("path"),
                f"{package_id}.source.path",
            ),
        }
        if source_fingerprint is not None:
            source_view["fingerprint_sha256"] = text(
                source_fingerprint,
                f"{package_id}.source.fingerprint_sha256",
            )

        packages[package_id] = {
            "id": package_id,
            "display": text(
                document.get("display_name"),
                f"{package_id}.display_name",
            ),
            "version_text": version_text,
            "version": version,
            "kind": text(document.get("kind"), f"{package_id}.kind"),
            "status": text(document.get("status"), f"{package_id}.status"),
            "manifest_sha256": fingerprint,
            "source": source_view,
            "compatibility": compatibility(
                obj(
                    document.get("compatibility"),
                    f"{package_id}.compatibility",
                ),
                f"{package_id}.compatibility",
            ),
            "dependencies": tuple(
                sorted(dependencies, key=lambda item: str(item["package_id"]))
            ),
            "conflicts": tuple(
                sorted(
                    identity(value, f"{package_id}.conflict")
                    for value in array(
                        document.get("conflicts", []),
                        f"{package_id}.conflicts",
                    )
                )
            ),
            "capabilities": tuple(
                sorted(
                    strings(
                        document.get("capabilities", []),
                        f"{package_id}.capabilities",
                    )
                )
            ),
            "payload": tuple(
                sorted(payload, key=lambda item: path_key(str(item["destination"])))
            ),
            "lifecycle": {
                key: lifecycle[key]
                for key in (
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
                "license_expression": text(
                    legal.get("license_expression"),
                    f"{package_id}.legal.license_expression",
                ),
                "redistribution_review": text(
                    legal.get("redistribution_review"),
                    f"{package_id}.legal.redistribution_review",
                ),
                "notice_files": sorted(
                    relative_path(value, f"{package_id}.legal.notice")
                    for value in array(
                        legal.get("notice_files"),
                        f"{package_id}.legal.notice_files",
                    )
                ),
            },
        }
    return packages


def load_suite(installer: Path, name: str) -> dict[str, object]:
    if not DIR_RE.fullmatch(name):
        raise ResolverError(f"Invalid suite directory name: {name!r}")
    document, fingerprint = load_json(
        directory(installer / "Suites" / name, f"Suite {name}") / "suite.json",
        f"suite {name}",
    )
    suite_id = identity(document.get("suite_id"), f"{name}.suite_id")
    entries: list[dict[str, object]] = []
    for index, raw in enumerate(
        array(document.get("packages"), f"{suite_id}.packages")
    ):
        item = obj(raw, f"{suite_id}.packages[{index}]")
        entries.append(
            {
                "package_id": identity(
                    item.get("package_id"),
                    f"{suite_id}.packages[{index}].package_id",
                ),
                "selection": text(
                    item.get("selection"),
                    f"{suite_id}.packages[{index}].selection",
                ),
                "order": integer(
                    item.get("order"),
                    f"{suite_id}.packages[{index}].order",
                ),
            }
        )
    features: dict[str, tuple[str, ...]] = {}
    for index, raw in enumerate(
        array(document.get("features", []), f"{suite_id}.features")
    ):
        feature = obj(raw, f"{suite_id}.features[{index}]")
        feature_id = identity(
            feature.get("feature_id"),
            f"{suite_id}.features[{index}].feature_id",
        )
        features[feature_id] = tuple(
            sorted(
                identity(value, f"{feature_id}.package_id")
                for value in array(
                    feature.get("package_ids"),
                    f"{feature_id}.package_ids",
                )
            )
        )
    policies = obj(document.get("policies"), f"{suite_id}.policies")
    compatibility_document = obj(
        document.get("compatibility"),
        f"{suite_id}.compatibility",
    )
    return {
        "id": suite_id,
        "display": text(document.get("display_name"), f"{suite_id}.display_name"),
        "version": text(document.get("version"), f"{suite_id}.version"),
        "channel": text(document.get("channel"), f"{suite_id}.channel"),
        "manifest_sha256": fingerprint,
        "entries": tuple(
            sorted(
                entries,
                key=lambda item: (int(item["order"]), str(item["package_id"])),
            )
        ),
        "features": features,
        "policies": {
            key: boolean(policies.get(key), f"{suite_id}.policies.{key}")
            for key in (
                "network_allowed",
                "elevation_allowed",
                "unreviewed_packages_allowed",
                "silent_install_allowed",
            )
        },
        "compatibility": {
            "platforms": strings(
                compatibility_document.get("platforms"),
                f"{suite_id}.compatibility.platforms",
            ),
            "architectures": strings(
                compatibility_document.get("architectures"),
                f"{suite_id}.compatibility.architectures",
            ),
        },
    }


def find_cycle(
    chosen: set[str],
    packages: dict[str, dict[str, object]],
) -> tuple[str, ...]:
    state: dict[str, int] = {}
    stack: list[str] = []
    positions: dict[str, int] = {}

    def visit(package_id: str) -> tuple[str, ...] | None:
        state[package_id] = 1
        positions[package_id] = len(stack)
        stack.append(package_id)
        for dependency in packages[package_id]["dependencies"]:
            dependency_id = str(dependency["package_id"])
            if dependency_id not in chosen:
                continue
            if state.get(dependency_id, 0) == 0:
                result = visit(dependency_id)
                if result:
                    return result
            elif state.get(dependency_id) == 1:
                body = stack[positions[dependency_id] :]
                start = min(range(len(body)), key=lambda index: body[index])
                body = body[start:] + body[:start]
                return tuple(body + [body[0]])
        stack.pop()
        positions.pop(package_id)
        state[package_id] = 2
        return None

    for package_id in sorted(chosen):
        if state.get(package_id, 0) == 0:
            result = visit(package_id)
            if result:
                return result
    return ()


def resolve(
    installer: Path,
    suite_name: str,
    context: Context,
    *,
    selected: Iterable[str] = (),
    excluded: Iterable[str] = (),
    features: Iterable[str] = (),
) -> dict[str, object]:
    installer = directory(installer, "Installer root")
    suite = load_suite(installer, suite_name)
    packages = load_packages(installer)
    match_context(f"Suite {suite['id']}", suite["compatibility"], context)

    entries = {
        str(entry["package_id"]): entry
        for entry in suite["entries"]
    }
    missing_manifests = sorted(set(entries) - set(packages))
    if missing_manifests:
        raise ResolverError(
            "Suite references package entries without package.json: "
            + ", ".join(missing_manifests)
        )

    selected_ids = {identity(value, "selected package") for value in selected}
    excluded_ids = {identity(value, "excluded package") for value in excluded}
    selected_features = {identity(value, "selected feature") for value in features}
    if selected_ids & excluded_ids:
        raise ResolverError("A package cannot be selected and excluded.")
    unknown = sorted((selected_ids | excluded_ids) - set(entries))
    if unknown:
        raise ResolverError(
            "User selection references packages outside the suite: "
            + ", ".join(unknown)
        )
    unknown_features = sorted(selected_features - set(suite["features"]))
    if unknown_features:
        raise ResolverError(
            "Unknown suite features: " + ", ".join(unknown_features)
        )

    chosen: set[str] = set()
    reasons: dict[str, set[str]] = {}
    for entry in suite["entries"]:
        package_id = str(entry["package_id"])
        selection = str(entry["selection"])
        if selection == "required" and package_id in excluded_ids:
            raise ResolverError(f"Required package {package_id} cannot be excluded.")
        if (
            selection == "required"
            or (selection == "default" and package_id not in excluded_ids)
            or package_id in selected_ids
        ):
            chosen.add(package_id)
            reasons.setdefault(package_id, set()).add(f"suite:{selection}")
    for feature_id in sorted(selected_features):
        for package_id in suite["features"][feature_id]:
            if package_id in excluded_ids:
                raise ResolverError(
                    f"Feature {feature_id} requires excluded package {package_id}."
                )
            chosen.add(package_id)
            reasons.setdefault(package_id, set()).add(f"feature:{feature_id}")
    for package_id in selected_ids:
        reasons.setdefault(package_id, set()).add("user:selected")

    pending = list(chosen)
    heapq.heapify(pending)
    validated: set[str] = set()
    warnings: set[str] = set()
    while pending:
        package_id = heapq.heappop(pending)
        if package_id in validated:
            continue
        package = packages[package_id]
        if package["status"] in {"planned", "blocked"}:
            raise ResolverError(
                f"Package {package_id} is not installable while status is {package['status']!r}."
            )
        if package["legal"]["redistribution_review"] not in {
            "approved",
            "not-applicable",
        }:
            raise ResolverError(
                f"Package {package_id} lacks approved redistribution review."
            )
        match_context(
            f"Package {package_id}",
            package["compatibility"],
            context,
        )
        if package["status"] in {"experimental", "deprecated"}:
            warnings.add(
                f"Package {package_id} has lifecycle status {package['status']}."
            )
        validated.add(package_id)
        for dependency in package["dependencies"]:
            dependency_id = str(dependency["package_id"])
            if bool(dependency["required"]) and dependency_id not in packages:
                raise ResolverError(
                    f"Package {package_id} requires missing package {dependency_id}."
                )
            if bool(dependency["required"]):
                if dependency_id in excluded_ids:
                    raise ResolverError(
                        f"Package {package_id} requires excluded package {dependency_id}."
                    )
                reasons.setdefault(dependency_id, set()).add(
                    f"dependency:{package_id}"
                )
                if dependency_id not in chosen:
                    chosen.add(dependency_id)
                    heapq.heappush(pending, dependency_id)

    for package_id in sorted(chosen):
        for dependency in packages[package_id]["dependencies"]:
            dependency_id = str(dependency["package_id"])
            version_range = dependency["range"]
            if dependency_id in chosen and not version_range.matches(
                packages[dependency_id]["version"]
            ):
                raise ResolverError(
                    f"Package {package_id} requires {dependency_id} "
                    f"{version_range.source}, but selected version is "
                    f"{packages[dependency_id]['version_text']}."
                )

    conflict_pairs = {
        tuple(sorted((package_id, conflict)))
        for package_id in chosen
        for conflict in packages[package_id]["conflicts"]
        if conflict in chosen
    }
    if conflict_pairs:
        raise ResolverError(
            "Selected packages conflict: "
            + ", ".join(
                f"{left} <-> {right}"
                for left, right in sorted(conflict_pairs)
            )
        )

    elevated = sorted(
        package_id
        for package_id in chosen
        if bool(packages[package_id]["lifecycle"]["elevation_required"])
    )
    if elevated and not bool(suite["policies"]["elevation_allowed"]):
        raise ResolverError(
            "suite forbids elevation required by: " + ", ".join(elevated)
        )

    dependents = {package_id: set() for package_id in chosen}
    indegree = {package_id: 0 for package_id in chosen}
    for package_id in chosen:
        for dependency in packages[package_id]["dependencies"]:
            dependency_id = str(dependency["package_id"])
            if dependency_id in chosen and package_id not in dependents[dependency_id]:
                dependents[dependency_id].add(package_id)
                indegree[package_id] += 1
    ready = [
        package_id for package_id, degree in indegree.items() if degree == 0
    ]
    heapq.heapify(ready)
    order: list[str] = []
    while ready:
        package_id = heapq.heappop(ready)
        order.append(package_id)
        for dependent in sorted(dependents[package_id]):
            indegree[dependent] -= 1
            if indegree[dependent] == 0:
                heapq.heappush(ready, dependent)
    if len(order) != len(chosen):
        cycle = find_cycle(chosen, packages)
        raise ResolverError("Dependency cycle detected: " + " -> ".join(cycle))

    destinations: dict[str, str] = {}
    rows: list[dict[str, object]] = []
    file_count = 0
    total_size = 0
    for package_id in order:
        package = packages[package_id]
        payload: list[dict[str, object]] = []
        for raw in package["payload"]:
            item = dict(raw)
            destination = str(item["destination"])
            key = path_key(destination)
            if key in destinations:
                raise ResolverError(
                    f"destination collision at {destination!r}: "
                    f"{destinations[key]} and {package_id}."
                )
            destinations[key] = package_id
            payload.append(item)
            file_count += 1
            total_size += int(item["size_bytes"])
        rows.append(
            {
                "package_id": package_id,
                "display_name": package["display"],
                "version": package["version_text"],
                "kind": package["kind"],
                "status": package["status"],
                "selection_reasons": sorted(
                    reasons.get(package_id, {"dependency:implicit"})
                ),
                "dependency_ids": sorted(
                    str(dependency["package_id"])
                    for dependency in package["dependencies"]
                    if str(dependency["package_id"]) in chosen
                ),
                "capabilities": list(package["capabilities"]),
                "manifest_sha256": package["manifest_sha256"],
                "source": package["source"],
                "lifecycle": package["lifecycle"],
                "legal": package["legal"],
                "payload": payload,
            }
        )

    plan_without_hash: dict[str, object] = {
        "schema_version": 1,
        "suite": {
            "suite_id": suite["id"],
            "display_name": suite["display"],
            "version": suite["version"],
            "channel": suite["channel"],
            "manifest_sha256": suite["manifest_sha256"],
        },
        "context": {
            "platform": context.platform,
            "architecture": context.architecture,
            "runtime_target": context.runtime_target,
            "game_version": context.game_version,
            "branch": context.branch,
        },
        "selection": {
            "selected_package_ids": sorted(selected_ids),
            "excluded_package_ids": sorted(excluded_ids),
            "selected_feature_ids": sorted(selected_features),
        },
        "policies": dict(sorted(suite["policies"].items())),
        "requires_elevation": bool(elevated),
        "package_order": order,
        "packages": rows,
        "summary": {
            "package_count": len(rows),
            "payload_file_count": file_count,
            "payload_size_bytes": total_size,
        },
        "warnings": sorted(warnings),
    }
    return {
        **plan_without_hash,
        "plan_sha256": sha256(canonical_json(plan_without_hash)),
    }


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


def atomic_write(path: Path, data: bytes) -> None:
    destination = path.resolve(strict=False)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and (
        destination.is_symlink() or is_reparse(destination)
    ):
        raise ResolverError(f"Output is an unsafe link: {destination}")
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "wb",
            dir=destination.parent,
            prefix=f".{destination.name}.",
            delete=False,
        ) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, destination)
    except OSError as exc:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        raise ResolverError(f"Unable to publish {destination}: {exc}") from exc


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Resolve a reviewed installer suite into a canonical plan."
    )
    parser.add_argument("--installer-root", type=Path, required=True)
    parser.add_argument("--suite", required=True)
    parser.add_argument("--platform", default="windows")
    parser.add_argument("--architecture", default="x86_64")
    parser.add_argument("--runtime-target", default="editor-only")
    parser.add_argument("--game-version", default="")
    parser.add_argument("--branch", default="")
    parser.add_argument("--select", action="append", default=[])
    parser.add_argument("--exclude", action="append", default=[])
    parser.add_argument("--feature", action="append", default=[])
    parser.add_argument("--output", type=Path)
    return parser.parse_args(argv)


if __name__ == "__main__":
    print(
        "_resolver_core.py is an internal import-only module; "
        "use suite_package_resolver.py.",
        file=sys.stderr,
    )
    raise SystemExit(2)
