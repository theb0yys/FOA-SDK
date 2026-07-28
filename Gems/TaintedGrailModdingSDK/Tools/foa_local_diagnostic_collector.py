#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Collect bounded, read-only FoA local diagnostics into the SDK intake shape.

The collector intentionally performs only explicit local inspection. It validates the
active workspace profile, checks the configured installation layout, reads a bounded
set of allowlisted file metadata, and folds optional identifier exports from the
workspace ExtractedDataPath into the `foa-local-capture` JSON consumed by the
separate intake tool.

It does not recursively scan game directories, load Unity assemblies, execute
BepInEx/Harmony code, call FoA APIs, copy proprietary payloads, mutate game files,
inspect saves, promote catalog facts, or grant runtime permission.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

TOOL_VERSION = "0.1.0"
TOOL_NAME = "FoA Local Diagnostic Collector"
SOURCE_KIND = "foa-local-diagnostic-capture"
DEFAULT_IDENTIFIER_EXPORT_NAME = "foa-identifiers.json"
MAX_JSON_BYTES = 16 * 1024 * 1024
MAX_FILE_HASH_BYTES = 256 * 1024 * 1024
MAX_OBSERVATIONS = 100_000
MAX_MANAGED_FILES = 256

IDENTIFIER_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
SUBJECT_RE = re.compile(r"^subject:[A-Za-z0-9][A-Za-z0-9:._/-]{1,511}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
DRIVE_PATH_RE = re.compile(r"[A-Za-z]:[\\/]")
UNC_PATH_RE = re.compile(r"^\\\\")
TOKEN_LOCATOR_RE = re.compile(r"^\$(install|managed|plugin|extracted|capture)(/[^\n\r]*)?(\$|:|$)")
JSON_PATH_RE = re.compile(r"^\$([.\[][A-Za-z0-9_\-\]'\".\[\]$]*)?$")
ALLOWED_EXPORT_EVIDENCE_KINDS = {
    "runtime-observation",
    "local-diagnostic-capture",
    "template-diagnostics",
    "native-identifier-observation",
    "addressable-observation",
    "assetbundle-observation",
    "managed-type-observation",
    "unity-guid-observation",
}
ALLOWED_CONFIDENCE = {"unrated", "observed", "documented", "inferred"}
ALLOWED_DOMAINS = {"economy", "population", "world", "quest", "dialogue", "audio", "ui", "runtime"}
ALLOWED_IDENTITY_KINDS = {"native", "synthetic", "composite", "source_scoped"}
ALLOWED_MANAGED_FILE_NAMES = {
    "Assembly-CSharp.dll",
    "UnityEngine.CoreModule.dll",
    "UnityEngine.dll",
    "Unity.TextMeshPro.dll",
    "netstandard.dll",
    "mscorlib.dll",
    "System.dll",
    "System.Core.dll",
}
ALLOWED_INSTALL_MARKERS = {
    "Tainted Grail_Data/globalgamemanagers",
    "Tainted Grail_Data/boot.config",
    "GameAssembly.dll",
    "UnityPlayer.dll",
}


class CollectorError(RuntimeError):
    """Raised when bounded local diagnostic collection cannot proceed."""


@dataclass(frozen=True)
class ActiveProfile:
    workspace_path: Path
    workspace_root: Path
    profile_id: str
    game_version: str
    branch: str
    runtime_target: str
    unity_version: str
    bep_in_ex_version: str
    install_path: Path
    managed_assemblies_path: Path | None
    plugin_path: Path | None
    extracted_data_path: Path | None


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def pretty_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def read_json_file(path: Path, *, maximum_bytes: int = MAX_JSON_BYTES) -> Any:
    try:
        size = path.stat().st_size
    except OSError as exc:
        raise CollectorError(f"Unable to read JSON file {path}: {exc}") from exc
    if size > maximum_bytes:
        raise CollectorError(f"JSON file exceeds {maximum_bytes} bytes: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8", errors="strict"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise CollectorError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc


def require_mapping(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise CollectorError(f"{label} must be a JSON object.")
    return value


def require_string(container: Mapping[str, Any], key: str, *, allow_empty: bool = False, maximum: int = 4096) -> str:
    value = container.get(key)
    if not isinstance(value, str):
        raise CollectorError(f"{key} is required and must be a string.")
    if (not allow_empty and not value) or len(value) > maximum:
        raise CollectorError(f"{key} is empty or exceeds {maximum} characters.")
    if any(ord(ch) < 0x20 or ord(ch) == 0x7F for ch in value):
        raise CollectorError(f"{key} contains a control character.")
    return value


def require_identifier(value: str, label: str) -> str:
    if not IDENTIFIER_RE.match(value):
        raise CollectorError(f"{label} must be a lowercase stable identifier: {value}")
    return value


def require_subject(value: str, label: str) -> str:
    if not SUBJECT_RE.match(value):
        raise CollectorError(f"{label} must be an explicit subject reference: {value}")
    return value


def require_utc(value: str, label: str) -> str:
    if not UTC_RE.match(value):
        raise CollectorError(f"{label} must use whole-second UTC format YYYY-MM-DDTHH:MM:SSZ.")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise CollectorError(f"{label} is not a valid UTC timestamp: {value}") from exc
    return value


def require_false(container: Mapping[str, Any], key: str) -> None:
    if container.get(key, False) is not False:
        raise CollectorError(f"{key} must be false; diagnostics cannot escalate authority.")


def normalize_runtime(value: str) -> str:
    if value not in {"Mono", "IL2CPP"}:
        raise CollectorError("RuntimeTarget must be Mono or IL2CPP.")
    return value


def resolve_document_path(value: str, base: Path) -> Path:
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = base / path
    return path.resolve(strict=False)


def is_relative_to(child: Path, parent: Path) -> bool:
    try:
        child.relative_to(parent)
        return True
    except ValueError:
        return False


def load_active_profile(workspace_path: Path) -> ActiveProfile:
    workspace_path = workspace_path.resolve(strict=False)
    workspace = require_mapping(read_json_file(workspace_path), "workspace")
    if workspace.get("SchemaVersion") != 1:
        raise CollectorError("Workspace must use SchemaVersion 1.")
    workspace_root = resolve_document_path(require_string(workspace, "RootPath", maximum=4096), workspace_path.parent)
    active_profile_id = require_identifier(require_string(workspace, "ActiveGameProfileId", maximum=256), "ActiveGameProfileId")
    profiles = workspace.get("GameProfiles")
    if not isinstance(profiles, list):
        raise CollectorError("Workspace GameProfiles must be an array.")
    matches = [profile for profile in profiles if isinstance(profile, dict) and profile.get("ProfileId") == active_profile_id]
    if len(matches) != 1:
        raise CollectorError("Workspace ActiveGameProfileId must bind to exactly one profile.")
    profile = matches[0]
    runtime_target = normalize_runtime(require_string(profile, "RuntimeTarget", maximum=32))
    install_path = resolve_document_path(require_string(profile, "InstallPath", maximum=4096), workspace_path.parent)
    managed_value = require_string(profile, "ManagedAssembliesPath", allow_empty=runtime_target == "IL2CPP", maximum=4096)
    plugin_value = require_string(profile, "PluginPath", allow_empty=runtime_target == "IL2CPP", maximum=4096)
    extracted_value = require_string(profile, "ExtractedDataPath", allow_empty=True, maximum=4096)
    managed_path = resolve_document_path(managed_value, workspace_path.parent) if managed_value else None
    plugin_path = resolve_document_path(plugin_value, workspace_path.parent) if plugin_value else None
    extracted_path = resolve_document_path(extracted_value, workspace_path.parent) if extracted_value else None
    return ActiveProfile(
        workspace_path=workspace_path,
        workspace_root=workspace_root,
        profile_id=require_identifier(require_string(profile, "ProfileId", maximum=256), "ProfileId"),
        game_version=require_string(profile, "GameVersion", maximum=128),
        branch=require_string(profile, "Branch", maximum=128),
        runtime_target=runtime_target,
        unity_version=require_string(profile, "UnityVersion", allow_empty=True, maximum=128),
        bep_in_ex_version=require_string(profile, "BepInExVersion", allow_empty=runtime_target == "IL2CPP", maximum=128),
        install_path=install_path,
        managed_assemblies_path=managed_path,
        plugin_path=plugin_path,
        extracted_data_path=extracted_path,
    )


def sanitize_segment(value: str) -> str:
    segment = re.sub(r"[^a-z0-9._-]+", "-", value.lower()).strip(".-_")
    return segment[:96] or "unknown"


def subject_suffix(value: str) -> str:
    return sanitize_segment(value).replace("-", ".")


def relative_token(path: Path, root: Path, token: str) -> str:
    try:
        relative = path.resolve(strict=False).relative_to(root.resolve(strict=False))
    except ValueError as exc:
        raise CollectorError(f"Path is outside {token}: {path}") from exc
    if any(part in {"", ".", ".."} for part in relative.parts):
        raise CollectorError(f"Path under {token} contains unsafe segments: {path}")
    return "$" + token + ("/" + relative.as_posix() if relative.parts else "")


def contains_private_or_absolute_path(value: str) -> bool:
    if DRIVE_PATH_RE.search(value) or UNC_PATH_RE.search(value):
        return True
    if value.startswith(("/", "~/")):
        return True
    return False


def assert_no_private_paths(value: Any, *, label: str = "capture") -> None:
    if isinstance(value, str):
        if contains_private_or_absolute_path(value):
            raise CollectorError(f"{label} contains an absolute or private path: {value}")
        return
    if isinstance(value, list):
        for index, child in enumerate(value):
            assert_no_private_paths(child, label=f"{label}[{index}]")
        return
    if isinstance(value, dict):
        for key, child in value.items():
            assert_no_private_paths(child, label=f"{label}.{key}")


def safe_json_locator(value: str) -> str:
    if TOKEN_LOCATOR_RE.match(value) or JSON_PATH_RE.match(value):
        return value
    raise CollectorError(f"Locator must be a JSON path or sanitized token path: {value}")


def hash_file(path: Path) -> str:
    try:
        stat = path.stat()
    except OSError as exc:
        raise CollectorError(f"Unable to stat file for hashing: {path}: {exc}") from exc
    if stat.st_size > MAX_FILE_HASH_BYTES:
        raise CollectorError(f"Refusing to hash oversized file: {path}")
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as exc:
        raise CollectorError(f"Unable to hash file: {path}: {exc}") from exc
    return "sha256:" + digest.hexdigest()


def observation(
    *,
    observation_id: str,
    subject_ref: str,
    claim_id: str,
    claim: str,
    value: str,
    evidence_kind: str,
    confidence: str,
    locator: str,
    record_path: str,
    domain: str = "",
    record_kind: str = "",
    identity_kind: str = "",
    owner_pack_id: str = "",
    record_id: str = "",
    native_ref_exact: str = "",
    display_name: str = "",
) -> dict[str, Any]:
    require_identifier(observation_id, "ObservationId")
    require_subject(subject_ref, "SubjectRef")
    require_identifier(claim_id, "ClaimId")
    if evidence_kind not in ALLOWED_EXPORT_EVIDENCE_KINDS:
        raise CollectorError(f"Unsupported evidence kind: {evidence_kind}")
    if confidence not in ALLOWED_CONFIDENCE:
        raise CollectorError(f"Unsupported confidence: {confidence}")
    if domain and domain not in ALLOWED_DOMAINS:
        raise CollectorError(f"Unsupported domain: {domain}")
    if bool(domain) != bool(record_kind):
        raise CollectorError("Domain and RecordKind must be supplied together.")
    if identity_kind and identity_kind not in ALLOWED_IDENTITY_KINDS:
        raise CollectorError(f"Unsupported identity kind: {identity_kind}")
    result: dict[str, Any] = {
        "ObservationId": observation_id,
        "SubjectRef": subject_ref,
        "ClaimId": claim_id,
        "Claim": claim,
        "Value": value,
        "EvidenceKind": evidence_kind,
        "Confidence": confidence,
        "Locator": safe_json_locator(locator),
        "RecordPath": safe_json_locator(record_path),
        "PromoteAutomatically": False,
        "GrantsRuntimePermission": False,
    }
    optional = {
        "Domain": domain,
        "RecordKind": record_kind,
        "IdentityKind": identity_kind,
        "OwnerPackId": owner_pack_id,
        "RecordId": record_id,
        "NativeRefExact": native_ref_exact,
        "DisplayName": display_name,
    }
    for key, optional_value in optional.items():
        if optional_value:
            result[key] = optional_value
    assert_no_private_paths(result, label=observation_id)
    return result


def add_layout_observation(observations: list[dict[str, Any]], profile: ActiveProfile, claim_id: str, claim: str, value: str, locator: str) -> None:
    index = len(observations)
    observations.append(
        observation(
            observation_id=f"observation.{profile.profile_id}.{claim_id}.{index}",
            subject_ref=f"subject:foa:runtime:profile:{subject_suffix(profile.profile_id)}",
            claim_id=claim_id,
            claim=claim,
            value=value,
            evidence_kind="local-diagnostic-capture",
            confidence="observed",
            locator=locator,
            record_path=f"$.Observations[{index}]",
        )
    )


def validate_profile_paths(profile: ActiveProfile) -> None:
    if not profile.install_path.is_dir():
        raise CollectorError(f"Configured FoA install path does not exist or is not a directory: {profile.install_path}")
    if profile.managed_assemblies_path:
        if not is_relative_to(profile.managed_assemblies_path, profile.install_path):
            raise CollectorError("ManagedAssembliesPath must remain inside the configured install path.")
        if profile.runtime_target == "Mono" and not profile.managed_assemblies_path.is_dir():
            raise CollectorError("Mono profile requires an existing ManagedAssembliesPath directory.")
    if profile.plugin_path:
        if not is_relative_to(profile.plugin_path, profile.install_path):
            raise CollectorError("PluginPath must remain inside the configured install path.")
        if profile.runtime_target == "Mono" and not profile.plugin_path.is_dir():
            raise CollectorError("Mono profile requires an existing PluginPath directory.")
    if profile.extracted_data_path:
        if not is_relative_to(profile.extracted_data_path, profile.workspace_root):
            raise CollectorError("ExtractedDataPath must remain inside the workspace root for bounded local capture.")
        profile.extracted_data_path.mkdir(parents=True, exist_ok=True)


def collect_layout_observations(profile: ActiveProfile, *, include_file_hashes: bool) -> list[dict[str, Any]]:
    validate_profile_paths(profile)
    observations: list[dict[str, Any]] = []
    add_layout_observation(observations, profile, "game_version", "Configured game version was bound before local diagnostic collection.", profile.game_version, "$.GameVersion")
    add_layout_observation(observations, profile, "branch", "Configured branch was bound before local diagnostic collection.", profile.branch, "$.Branch")
    add_layout_observation(observations, profile, "runtime_target", "Configured runtime target was bound before local diagnostic collection.", profile.runtime_target, "$.RuntimeTarget")
    add_layout_observation(observations, profile, "install_path_present", "The configured install root exists; the absolute path was redacted.", "$install", "$.InstallPath")
    data_root = profile.install_path / "Tainted Grail_Data"
    if data_root.is_dir():
        add_layout_observation(observations, profile, "unity_data_root_present", "The Unity data directory exists under the configured install root.", "$install/Tainted Grail_Data", "$install/Tainted Grail_Data")
    if profile.managed_assemblies_path and profile.managed_assemblies_path.is_dir():
        add_layout_observation(observations, profile, "managed_assemblies_path_present", "The configured managed assemblies directory exists under the install root.", relative_token(profile.managed_assemblies_path, profile.install_path, "install"), "$managed")
    if profile.plugin_path and profile.plugin_path.is_dir():
        add_layout_observation(observations, profile, "bepinex_plugin_path_present", "The configured BepInEx plugin directory exists under the install root.", relative_token(profile.plugin_path, profile.install_path, "install"), "$plugin")
    if profile.extracted_data_path and profile.extracted_data_path.is_dir():
        add_layout_observation(observations, profile, "extracted_data_path_present", "The workspace extracted-data directory exists; the absolute path was redacted.", "$extracted", "$extracted")

    if include_file_hashes:
        observations.extend(collect_file_fingerprints(profile, offset=len(observations)))
    return observations


def collect_file_fingerprints(profile: ActiveProfile, *, offset: int) -> list[dict[str, Any]]:
    observations: list[dict[str, Any]] = []
    managed_files: list[Path] = []
    if profile.managed_assemblies_path and profile.managed_assemblies_path.is_dir():
        for name in sorted(ALLOWED_MANAGED_FILE_NAMES):
            candidate = profile.managed_assemblies_path / name
            if candidate.is_file():
                managed_files.append(candidate)
    if len(managed_files) > MAX_MANAGED_FILES:
        raise CollectorError("Managed assembly allowlist produced too many files.")
    for path in managed_files:
        index = offset + len(observations)
        locator = relative_token(path, profile.managed_assemblies_path, "managed")
        observations.append(
            observation(
                observation_id=f"observation.{profile.profile_id}.managed_assembly_fingerprint.{index}",
                subject_ref=f"subject:foa:runtime:managed-assembly:{subject_suffix(path.name)}",
                claim_id="managed_assembly_fingerprint",
                claim="Allowlisted managed assembly fingerprint was observed without copying assembly content.",
                value=hash_file(path),
                evidence_kind="managed-type-observation",
                confidence="observed",
                locator=locator,
                record_path=f"$.Observations[{index}]",
            )
        )
    for relative in sorted(ALLOWED_INSTALL_MARKERS):
        path = profile.install_path / relative
        if not path.is_file():
            continue
        index = offset + len(observations)
        locator = relative_token(path, profile.install_path, "install")
        observations.append(
            observation(
                observation_id=f"observation.{profile.profile_id}.install_marker_fingerprint.{index}",
                subject_ref=f"subject:foa:runtime:install-marker:{subject_suffix(relative)}",
                claim_id="install_marker_fingerprint",
                claim="Allowlisted install marker fingerprint was observed without copying file content.",
                value=hash_file(path),
                evidence_kind="local-diagnostic-capture",
                confidence="observed",
                locator=locator,
                record_path=f"$.Observations[{index}]",
            )
        )
    return observations


def resolve_identifier_exports(profile: ActiveProfile, explicit_exports: Sequence[Path] | None) -> list[Path]:
    exports: list[Path] = []
    if not profile.extracted_data_path:
        if explicit_exports:
            raise CollectorError("Identifier exports require ExtractedDataPath in the active profile.")
        return []
    root = profile.extracted_data_path.resolve(strict=False)
    if explicit_exports:
        candidates = [resolve_document_path(str(path), profile.workspace_path.parent) for path in explicit_exports]
    else:
        default = root / DEFAULT_IDENTIFIER_EXPORT_NAME
        candidates = [default] if default.is_file() else []
    for candidate in candidates:
        candidate = candidate.resolve(strict=False)
        if not is_relative_to(candidate, root):
            raise CollectorError("Identifier export paths must remain inside ExtractedDataPath.")
        if not candidate.is_file():
            raise CollectorError(f"Identifier export does not exist: {candidate}")
        exports.append(candidate)
    return sorted(exports)


def normalize_export_observation(raw: Any, *, export_path: Path, export_root: Path, index: int) -> dict[str, Any]:
    entry = dict(require_mapping(raw, f"export observation {index}"))
    require_false(entry, "PromoteAutomatically")
    require_false(entry, "GrantsRuntimePermission")
    observation_id = require_identifier(require_string(entry, "ObservationId", maximum=192), "ObservationId")
    subject_ref = require_subject(require_string(entry, "SubjectRef", maximum=768), "SubjectRef")
    claim_id = require_identifier(require_string(entry, "ClaimId", maximum=192), "ClaimId")
    evidence_kind = require_string(entry, "EvidenceKind", maximum=128)
    if evidence_kind not in ALLOWED_EXPORT_EVIDENCE_KINDS:
        raise CollectorError(f"Observation {observation_id} has unsupported EvidenceKind {evidence_kind}.")
    confidence = require_string(entry, "Confidence", maximum=64)
    if confidence not in ALLOWED_CONFIDENCE:
        raise CollectorError(f"Observation {observation_id} has unsupported Confidence {confidence}.")
    domain = require_string(entry, "Domain", allow_empty=True, maximum=64) if "Domain" in entry else ""
    record_kind = require_string(entry, "RecordKind", allow_empty=True, maximum=64) if "RecordKind" in entry else ""
    if bool(domain) != bool(record_kind):
        raise CollectorError(f"Observation {observation_id} must provide Domain and RecordKind together.")
    if domain and domain not in ALLOWED_DOMAINS:
        raise CollectorError(f"Observation {observation_id} has unsupported Domain {domain}.")
    identity_kind = require_string(entry, "IdentityKind", allow_empty=True, maximum=64) if "IdentityKind" in entry else ""
    if identity_kind and identity_kind not in ALLOWED_IDENTITY_KINDS:
        raise CollectorError(f"Observation {observation_id} has unsupported IdentityKind {identity_kind}.")
    locator = require_string(entry, "Locator", allow_empty=True, maximum=2048) if "Locator" in entry else ""
    if locator:
        safe_json_locator(locator)
    relative_export = relative_token(export_path, export_root, "extracted")
    combined_locator = relative_export + (":" + locator if locator else "")
    result = observation(
        observation_id=observation_id,
        subject_ref=subject_ref,
        claim_id=claim_id,
        claim=require_string(entry, "Claim", maximum=8192),
        value=require_string(entry, "Value", allow_empty=True, maximum=8192) if "Value" in entry else "",
        evidence_kind=evidence_kind,
        confidence=confidence,
        locator=combined_locator,
        record_path=safe_json_locator(require_string(entry, "RecordPath", maximum=2048)),
        domain=domain,
        record_kind=record_kind,
        identity_kind=identity_kind,
        owner_pack_id=require_string(entry, "OwnerPackId", allow_empty=True, maximum=192) if "OwnerPackId" in entry else "",
        record_id=require_string(entry, "RecordId", allow_empty=True, maximum=192) if "RecordId" in entry else "",
        native_ref_exact=require_string(entry, "NativeRefExact", allow_empty=True, maximum=512) if "NativeRefExact" in entry else "",
        display_name=require_string(entry, "DisplayName", allow_empty=True, maximum=512) if "DisplayName" in entry else "",
    )
    assert_no_private_paths(result, label=observation_id)
    return result


def load_identifier_export(path: Path, profile: ActiveProfile) -> list[dict[str, Any]]:
    document = require_mapping(read_json_file(path), f"identifier export {path}")
    if document.get("SchemaVersion") != 1:
        raise CollectorError("Identifier export must use SchemaVersion 1.")
    if (
        require_string(document, "ProfileId", maximum=256) != profile.profile_id
        or require_string(document, "GameVersion", maximum=128) != profile.game_version
        or require_string(document, "Branch", maximum=128) != profile.branch
        or normalize_runtime(require_string(document, "RuntimeTarget", maximum=32)) != profile.runtime_target
    ):
        raise CollectorError("Identifier export must match the exact active workspace profile.")
    require_false(document, "PromoteAutomatically")
    require_false(document, "GrantsRuntimePermission")
    observations = document.get("Observations")
    if not isinstance(observations, list):
        raise CollectorError("Identifier export Observations must be an array.")
    if not profile.extracted_data_path:
        raise CollectorError("Identifier export requires ExtractedDataPath.")
    return [
        normalize_export_observation(
            raw,
            export_path=path.resolve(strict=False),
            export_root=profile.extracted_data_path.resolve(strict=False),
            index=index,
        )
        for index, raw in enumerate(observations)
    ]


def ensure_unique_observation_ids(observations: Sequence[Mapping[str, Any]]) -> None:
    ids = [str(value["ObservationId"]) for value in observations]
    duplicates = sorted({value for value in ids if ids.count(value) > 1})
    if duplicates:
        raise CollectorError("Duplicate observation IDs are refused: " + ", ".join(duplicates))
    if len(observations) > MAX_OBSERVATIONS:
        raise CollectorError(f"Capture input exceeds {MAX_OBSERVATIONS} observations.")


def build_capture(
    workspace_path: Path,
    *,
    identifier_exports: Sequence[Path] | None = None,
    captured_at: str | None = None,
    include_file_hashes: bool = True,
) -> dict[str, Any]:
    profile = load_active_profile(workspace_path)
    captured_at = require_utc(captured_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "CapturedAt")
    observations = collect_layout_observations(profile, include_file_hashes=include_file_hashes)
    for export_path in resolve_identifier_exports(profile, identifier_exports):
        observations.extend(load_identifier_export(export_path, profile))
    ensure_unique_observation_ids(observations)
    capture_seed = canonical_json_bytes(
        {
            "ProfileId": profile.profile_id,
            "GameVersion": profile.game_version,
            "Branch": profile.branch,
            "RuntimeTarget": profile.runtime_target,
            "CapturedAt": captured_at,
            "Observations": observations,
        }
    )
    capture_id = f"capture.{profile.profile_id}.{hashlib.sha256(capture_seed).hexdigest()[:16]}"
    capture = {
        "SchemaVersion": 1,
        "CaptureId": capture_id,
        "Title": "Bounded FoA local diagnostic capture",
        "SourceKind": SOURCE_KIND,
        "ProfileId": profile.profile_id,
        "GameVersion": profile.game_version,
        "Branch": profile.branch,
        "RuntimeTarget": profile.runtime_target,
        "ToolName": TOOL_NAME,
        "ToolVersion": TOOL_VERSION,
        "CapturedAt": captured_at,
        "Locator": "$capture/foa-local-diagnostic-collector",
        "PromoteAutomatically": False,
        "GrantsRuntimePermission": False,
        "CollectionScope": {
            "InstallRoot": "$install",
            "ManagedAssembliesPath": "$managed" if profile.managed_assemblies_path else "",
            "PluginPath": "$plugin" if profile.plugin_path else "",
            "ExtractedDataPath": "$extracted" if profile.extracted_data_path else "",
            "RecursiveScanAllowed": False,
            "AssemblyLoadAllowed": False,
            "RuntimeInvocationAllowed": False,
            "GameMutationAllowed": False,
            "SaveAccessAllowed": False,
        },
        "Observations": observations,
    }
    assert_no_private_paths(capture)
    return capture


def write_capture(capture: Mapping[str, Any], output: Path, *, replace: bool = False) -> None:
    if output.exists() and not replace:
        raise CollectorError(f"Output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_bytes(pretty_json_bytes(capture))
    os.replace(temporary, output)


def verify_capture(workspace_path: Path, capture_path: Path) -> dict[str, Any]:
    profile = load_active_profile(workspace_path)
    capture = require_mapping(read_json_file(capture_path), "capture")
    if capture.get("SchemaVersion") != 1 or capture.get("SourceKind") != SOURCE_KIND:
        raise CollectorError("Capture is not a FoA local diagnostic capture.")
    require_identifier(require_string(capture, "CaptureId", maximum=256), "CaptureId")
    if (
        require_string(capture, "ProfileId", maximum=256) != profile.profile_id
        or require_string(capture, "GameVersion", maximum=128) != profile.game_version
        or require_string(capture, "Branch", maximum=128) != profile.branch
        or normalize_runtime(require_string(capture, "RuntimeTarget", maximum=32)) != profile.runtime_target
    ):
        raise CollectorError("Capture must match the exact active workspace profile.")
    require_utc(require_string(capture, "CapturedAt", maximum=32), "CapturedAt")
    require_false(capture, "PromoteAutomatically")
    require_false(capture, "GrantsRuntimePermission")
    scope = require_mapping(capture.get("CollectionScope"), "CollectionScope")
    for key in ("RecursiveScanAllowed", "AssemblyLoadAllowed", "RuntimeInvocationAllowed", "GameMutationAllowed", "SaveAccessAllowed"):
        require_false(scope, key)
    observations = capture.get("Observations")
    if not isinstance(observations, list) or not observations:
        raise CollectorError("Capture Observations must be a non-empty array.")
    ensure_unique_observation_ids([require_mapping(value, "observation") for value in observations])
    for index, raw in enumerate(observations):
        entry = require_mapping(raw, f"Observations[{index}]")
        require_identifier(require_string(entry, "ObservationId", maximum=192), "ObservationId")
        require_subject(require_string(entry, "SubjectRef", maximum=768), "SubjectRef")
        require_identifier(require_string(entry, "ClaimId", maximum=192), "ClaimId")
        require_string(entry, "Claim", maximum=8192)
        evidence_kind = require_string(entry, "EvidenceKind", maximum=128)
        if evidence_kind not in ALLOWED_EXPORT_EVIDENCE_KINDS:
            raise CollectorError(f"Observation has unsupported EvidenceKind {evidence_kind}.")
        confidence = require_string(entry, "Confidence", maximum=64)
        if confidence not in ALLOWED_CONFIDENCE:
            raise CollectorError(f"Observation has unsupported Confidence {confidence}.")
        require_string(entry, "Value", allow_empty=True, maximum=8192)
        safe_json_locator(require_string(entry, "Locator", maximum=2048))
        safe_json_locator(require_string(entry, "RecordPath", maximum=2048))
        require_false(entry, "PromoteAutomatically")
        require_false(entry, "GrantsRuntimePermission")
    assert_no_private_paths(capture)
    return dict(capture)


def generate_fixture(output: Path, *, replace: bool = False) -> dict[str, Any]:
    if output.exists():
        if replace:
            if output.is_dir():
                shutil.rmtree(output)
            else:
                output.unlink()
        elif any(output.iterdir() if output.is_dir() else [output]):
            raise CollectorError(f"Fixture output is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)
    workspace_root = output / "workspace"
    install_root = output / "lawful-local-fixture" / "FoA"
    managed = install_root / "Tainted Grail_Data" / "Managed"
    plugins = install_root / "BepInEx" / "plugins"
    extracted = workspace_root / "Extracted"
    managed.mkdir(parents=True, exist_ok=True)
    plugins.mkdir(parents=True, exist_ok=True)
    extracted.mkdir(parents=True, exist_ok=True)
    (install_root / "Tainted Grail_Data" / "globalgamemanagers").write_bytes(b"synthetic-globalgamemanagers")
    (managed / "Assembly-CSharp.dll").write_bytes(b"synthetic-assembly-csharp")
    (managed / "UnityEngine.CoreModule.dll").write_bytes(b"synthetic-unity-core")
    workspace = {
        "SchemaVersion": 1,
        "WorkspaceId": "fixture.workspace",
        "DisplayName": "Fixture Workspace",
        "RootPath": "./workspace",
        "OutputPath": "./workspace/Build",
        "StagingPath": "./workspace/Staging",
        "DeploymentPath": "./workspace/Deployment",
        "ActiveGameProfileId": "foa.mono.fixture",
        "GameProfiles": [
            {
                "ProfileId": "foa.mono.fixture",
                "DisplayName": "FoA Mono Fixture",
                "InstallPath": "./lawful-local-fixture/FoA",
                "GameVersion": "1.23.401",
                "Branch": "mono",
                "RuntimeTarget": "Mono",
                "UnityVersion": "6000.0.64f1",
                "BepInExVersion": "5.4.23.3",
                "ManagedAssembliesPath": "./lawful-local-fixture/FoA/Tainted Grail_Data/Managed",
                "PluginPath": "./lawful-local-fixture/FoA/BepInEx/plugins",
                "DiagnosticsPath": "./workspace/Diagnostics",
                "ExtractedDataPath": "./workspace/Extracted",
                "DlcScopes": ["base-game"],
            }
        ],
    }
    export = {
        "SchemaVersion": 1,
        "ProfileId": "foa.mono.fixture",
        "GameVersion": "1.23.401",
        "Branch": "mono",
        "RuntimeTarget": "Mono",
        "PromoteAutomatically": False,
        "GrantsRuntimePermission": False,
        "Observations": [
            {
                "ObservationId": "observation.fixture.item.native-ref",
                "SubjectRef": "subject:foa:economy:item:iron-ore",
                "ClaimId": "native_ref_exact",
                "Claim": "Native item identifier was observed in a project-owned synthetic local identifier export.",
                "Value": "00000000-0000-0000-0000-000000000001",
                "Domain": "economy",
                "RecordKind": "item",
                "IdentityKind": "native",
                "NativeRefExact": "00000000-0000-0000-0000-000000000001",
                "DisplayName": "Synthetic Iron Ore",
                "EvidenceKind": "native-identifier-observation",
                "Confidence": "observed",
                "Locator": "$.items[0].guid",
                "RecordPath": "$.Observations[0]",
                "PromoteAutomatically": False,
                "GrantsRuntimePermission": False,
            }
        ],
    }
    workspace_path = output / "workspace.tgworkspace.json"
    export_path = extracted / DEFAULT_IDENTIFIER_EXPORT_NAME
    capture_path = output / "capture.foa-local-capture.json"
    workspace_path.write_bytes(pretty_json_bytes(workspace))
    export_path.write_bytes(pretty_json_bytes(export))
    capture = build_capture(workspace_path, captured_at="2026-07-28T00:00:00Z")
    write_capture(capture, capture_path, replace=True)
    verify_capture(workspace_path, capture_path)
    return {"workspace": str(workspace_path), "capture": str(capture_path), "observation_count": len(capture["Observations"])}


def parse_arguments(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Collect bounded read-only FoA local diagnostics into intake JSON.")
    subcommands = parser.add_subparsers(dest="command", required=True)
    collect = subcommands.add_parser("collect", help="Write a sanitized local-capture JSON document from a configured workspace profile.")
    collect.add_argument("--workspace", required=True, type=Path)
    collect.add_argument("--output", required=True, type=Path)
    collect.add_argument("--identifier-export", action="append", type=Path, default=[])
    collect.add_argument("--captured-at", help="Whole-second UTC capture time for deterministic output.")
    collect.add_argument("--no-file-hashes", action="store_true", help="Record bounded path/layout observations without hashing allowlisted files.")
    collect.add_argument("--replace", action="store_true")
    verify = subcommands.add_parser("verify", help="Verify a generated local-capture JSON document against the active workspace profile.")
    verify.add_argument("--workspace", required=True, type=Path)
    verify.add_argument("--input", required=True, type=Path)
    fixture = subcommands.add_parser("fixture", help="Generate a synthetic project-owned collector fixture.")
    fixture.add_argument("--output", required=True, type=Path)
    fixture.add_argument("--replace", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    try:
        if arguments.command == "collect":
            capture = build_capture(
                arguments.workspace,
                identifier_exports=arguments.identifier_export,
                captured_at=arguments.captured_at,
                include_file_hashes=not arguments.no_file_hashes,
            )
            write_capture(capture, arguments.output, replace=arguments.replace)
            print(f"FoA local diagnostic collector wrote {len(capture['Observations'])} observations to {arguments.output}.")
        elif arguments.command == "verify":
            capture = verify_capture(arguments.workspace, arguments.input)
            print(f"FoA local diagnostic capture verified for {capture['CaptureId']}.")
        elif arguments.command == "fixture":
            result = generate_fixture(arguments.output, replace=arguments.replace)
            print(f"FoA local diagnostic fixture wrote {result['observation_count']} observations.")
        else:
            raise CollectorError(f"Unknown command: {arguments.command}")
    except CollectorError as exc:
        print(f"FoA local diagnostic collector failed: {exc}", file=os.sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
