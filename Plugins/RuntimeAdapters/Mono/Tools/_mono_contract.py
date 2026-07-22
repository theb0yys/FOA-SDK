# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Shared immutable package and build-plan contracts for the Mono adapter."""
from __future__ import annotations

import hashlib
import json
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Mapping

PACKAGE_ROOT = Path(__file__).resolve().parents[1]
ADAPTER_ID = "adapter.foa.mono"
PACKAGE_ID = "extension.foa-mono-runtime-adapter"
PACKAGE_VERSION = "0.1.0"
STARTUP_MARKER = "foa-sdk.mono-adapter.ready"
PROFILE = {
    "profile_id": "foa-1.23.401-mono-bepinex5-tf-0.1.33",
    "game_version": "1.23.401",
    "branch": "mono",
    "runtime_target": "Mono",
    "unity_version": "6000.0.64f1",
    "bepinex_version": "5.4.23.3",
    "framework_version": "0.1.33",
    "evidence_state": "HostLiveLoadValidated",
}
AUTHORITY_FIELDS = (
    "build", "deployment", "game_launch", "process_execution",
    "runtime_execution", "runtime_mutation", "save_access", "signing",
    "publication", "catalog_mutation", "evidence_promotion",
)
MANIFEST_SHA256 = "sha256:05cfc4f58fa25857d5c89683d535f163debee3c817c346c9ee9fe14ad3e9d756"
SHA256_RE = re.compile(r"^sha256:[0-9a-f]{64}$")
ID_RE = re.compile(r"^[a-z0-9]+(?:[._-][a-z0-9]+)*$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
SAFE_PATH_RE = re.compile(r"^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*$")


class MonoAdapterError(RuntimeError):
    pass


def canonical_json(document: Mapping[str, object]) -> bytes:
    return (json.dumps(document, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def sha256_bytes(data: bytes) -> str:
    return "sha256:" + hashlib.sha256(data).hexdigest()


def fingerprint(document: Mapping[str, object]) -> str:
    return sha256_bytes(canonical_json(document))


def authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def object_value(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise MonoAdapterError(f"{label} must be an object.")
    return dict(value)


def text(value: object, label: str, maximum: int, allow_empty: bool = False) -> str:
    if not isinstance(value, str) or (not allow_empty and not value) or len(value) > maximum:
        raise MonoAdapterError(f"{label} is invalid or unbounded.")
    if any(ord(char) < 0x20 or ord(char) == 0x7F for char in value):
        raise MonoAdapterError(f"{label} contains control characters.")
    return value


def sha256(value: object, label: str) -> str:
    checked = text(value, label, 71)
    if not SHA256_RE.fullmatch(checked):
        raise MonoAdapterError(f"{label} must be a lowercase SHA-256 fingerprint.")
    return checked


def stable_id(value: object, label: str) -> str:
    checked = text(value, label, 128)
    if not ID_RE.fullmatch(checked):
        raise MonoAdapterError(f"{label} must be a stable lowercase identifier.")
    return checked


def utc(value: object, label: str) -> datetime:
    checked = text(value, label, 20)
    if not UTC_RE.fullmatch(checked):
        raise MonoAdapterError(f"{label} must be whole-second UTC.")
    return datetime.strptime(checked, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)


def safe_reference(value: object, label: str, allow_empty: bool = False) -> str:
    checked = text(value, label, 512, allow_empty)
    if checked or not allow_empty:
        if not SAFE_PATH_RE.fullmatch(checked) or ".." in checked.split("/") or ":" in checked or "\\" in checked:
            raise MonoAdapterError(f"{label} must be a safe relative logical reference.")
    return checked


def exact_authority(value: object, label: str) -> None:
    record = object_value(value, label)
    if set(record) != set(AUTHORITY_FIELDS) or any(record[field] is not False for field in AUTHORITY_FIELDS):
        raise MonoAdapterError(f"{label} must contain the exact all-false authority record.")


def unique_ids(value: object, label: str) -> list[str]:
    if not isinstance(value, list) or not 1 <= len(value) <= 64:
        raise MonoAdapterError(f"{label} must contain between 1 and 64 identifiers.")
    checked = [stable_id(row, f"{label}[{index}]") for index, row in enumerate(value)]
    if len(set(checked)) != len(checked):
        raise MonoAdapterError(f"{label} contains duplicate identifiers.")
    return sorted(checked)


def load_json(path: Path) -> dict[str, object]:
    try:
        raw = path.read_bytes()
        if len(raw) > 512 * 1024:
            raise MonoAdapterError(f"{path} exceeds the JSON byte limit.")
        return object_value(json.loads(raw.decode("utf-8", errors="strict")), str(path))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise MonoAdapterError(f"Unable to load strict JSON from {path}.") from exc


def validate_manifest(document: Mapping[str, object]) -> dict[str, object]:
    manifest = dict(document)
    if fingerprint(manifest) != MANIFEST_SHA256:
        raise MonoAdapterError("mono-adapter.json differs from the reviewed canonical manifest.")
    exact_authority(manifest.get("authority"), "manifest.authority")
    return manifest


def validate_package(package_root: Path = PACKAGE_ROOT) -> dict[str, object]:
    manifest = validate_manifest(load_json(package_root / "mono-adapter.json"))
    source_project = object_value(manifest["source_project"], "source_project")
    for raw in source_project["source_files"]:
        row = object_value(raw, "source file")
        relative = safe_reference(row["path"], "source file path")
        path = package_root / relative
        if path.is_symlink() or not path.is_file() or path.stat().st_size > 256 * 1024:
            raise MonoAdapterError(f"Required Mono source file is missing, linked, or unbounded: {relative}")
        if sha256_bytes(path.read_bytes()) != row["sha256"]:
            raise MonoAdapterError(f"Mono source fingerprint mismatch: {relative}")
    forbidden = {".dll", ".exe", ".pdb", ".zip", ".7z", ".nupkg"}
    for path in package_root.rglob("*"):
        if path.is_file() and path.suffix.lower() in forbidden:
            raise MonoAdapterError(f"Generated or binary payload is forbidden: {path.name}")
    return manifest


def build_plan(package_root: Path = PACKAGE_ROOT) -> dict[str, object]:
    manifest = validate_package(package_root)
    source = object_value(manifest["source_project"], "source_project")
    base = {
        "schema_version": 1,
        "plan_kind": "foa-mono-adapter-build-plan",
        "adapter_id": ADAPTER_ID,
        "package_id": PACKAGE_ID,
        "package_version": PACKAGE_VERSION,
        "profile": PROFILE,
        "route_contract_version": "1.0.0",
        "source_files": source["source_files"],
        "dependencies": manifest["dependencies"],
        "expected_binaries": manifest["expected_binaries"],
        "steps": [{
            "step_id": "validate-exact-local-references",
            "tool": "dotnet",
            "arguments": ["build", source["project"], "-c", "Release", "-p:FoAGameRoot=${FOA_GAME_ROOT}", "-p:ContinuousIntegrationBuild=true", "-p:UseSharedCompilation=false"],
            "execution_allowed": False,
        }],
        "authority": authority(),
    }
    return {**base, "build_plan_sha256": fingerprint(base)}


def validate_plan(plan: Mapping[str, object], package_root: Path = PACKAGE_ROOT) -> dict[str, object]:
    document = dict(plan)
    if document != build_plan(package_root):
        raise MonoAdapterError("Mono build plan is stale, altered, or noncanonical.")
    return document
