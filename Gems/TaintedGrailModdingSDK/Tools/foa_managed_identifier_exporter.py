#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Produce bounded FoA identifier exports from local managed metadata observations.

This exporter is the first producer for the `foa-identifiers.json` contract. It
reads only the active workspace profile, an allowlisted managed assembly name,
and optional project-owned seed observations beneath ExtractedDataPath. It never
loads assemblies as code, decompiles methods, executes Unity/FoA/BepInEx/Harmony,
copies proprietary payloads, mutates game files, promotes catalog facts, or
grants runtime permission.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence

TOOLS_ROOT = Path(__file__).resolve().parent
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import foa_identifier_export

TOOL_NAME = "FoA Managed Identifier Exporter"
TOOL_VERSION = "0.1.0"
DOCUMENT_KIND = foa_identifier_export.DOCUMENT_KIND
DEFAULT_OUTPUT_NAME = foa_identifier_export.DEFAULT_EXPORT_NAME
DEFAULT_SEED_NAME = "foa-managed-identifier-seeds.json"
MAX_JSON_BYTES = 16 * 1024 * 1024
MAX_ASSEMBLY_BYTES = 256 * 1024 * 1024
MAX_STRING_CANDIDATES = 4096
MAX_OBSERVATIONS = 100_000

IDENTIFIER_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
PRIVATE_PATH_RE = re.compile(r"(^/|^~/|^[A-Za-z]:[\\/]|^\\\\)")
MANAGED_TYPE_RE = re.compile(
    r"^(?:[A-Za-z_][A-Za-z0-9_]*\.)+[A-Za-z_][A-Za-z0-9_]*(?:\+[A-Za-z_][A-Za-z0-9_]*)?$"
)
TYPE_HINT_RE = re.compile(
    r"^[A-Za-z_][A-Za-z0-9_]*(?:Manager|Service|Definition|Database|Template|Recipe|Item|Inventory|Controller|Component)$"
)
ALLOWED_MANAGED_ASSEMBLIES = (
    "Assembly-CSharp.dll",
)
SEED_COLLECTIONS = {
    "ManagedTypes": "managed_type_name",
    "TemplateKeys": "template_key",
    "RecipeKeys": "recipe_key",
    "AddressableKeys": "addressable_key",
    "NativeRefs": "native_ref_exact",
}
CLAIM_DEFAULTS = {
    "managed_type_name": {
        "domain": "",
        "record_kind": "",
        "identity_kind": "",
        "evidence": "managed-type-observation",
        "confidence": "inferred",
        "subject_prefix": "subject:foa:runtime:managed-type:",
        "claim": "Managed type-like identifier was observed by bounded local metadata export without loading an assembly.",
    },
    "template_key": {
        "domain": "population",
        "record_kind": "template",
        "identity_kind": "source_scoped",
        "evidence": "template-diagnostics",
        "confidence": "observed",
        "subject_prefix": "subject:foa:population:template:",
        "claim": "Template key was supplied by a bounded managed identifier seed under ExtractedDataPath.",
    },
    "recipe_key": {
        "domain": "economy",
        "record_kind": "recipe",
        "identity_kind": "source_scoped",
        "evidence": "template-diagnostics",
        "confidence": "observed",
        "subject_prefix": "subject:foa:economy:recipe:",
        "claim": "Recipe key was supplied by a bounded managed identifier seed under ExtractedDataPath.",
    },
    "addressable_key": {
        "domain": "",
        "record_kind": "",
        "identity_kind": "source_scoped",
        "evidence": "addressable-observation",
        "confidence": "observed",
        "subject_prefix": "subject:foa:addressable:",
        "claim": "Addressable key was supplied by a bounded managed identifier seed under ExtractedDataPath.",
    },
    "native_ref_exact": {
        "domain": "",
        "record_kind": "",
        "identity_kind": "native",
        "evidence": "native-identifier-observation",
        "confidence": "observed",
        "subject_prefix": "subject:foa:native-ref:",
        "claim": "Native reference was supplied by a bounded managed identifier seed under ExtractedDataPath.",
    },
}


class ManagedExportError(RuntimeError):
    """Raised when the managed identifier exporter cannot safely produce a contract."""


@dataclass(frozen=True)
class ActiveProfile:
    workspace_path: Path
    workspace_root: Path
    profile_id: str
    game_version: str
    branch: str
    runtime_target: str
    install_path: Path
    managed_assemblies_path: Path
    extracted_data_path: Path


def pretty_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def sha256_bytes(value: bytes) -> str:
    return "sha256:" + hashlib.sha256(value).hexdigest()


def read_json(path: Path, *, maximum_bytes: int = MAX_JSON_BYTES) -> Any:
    try:
        size = path.stat().st_size
    except OSError as exc:
        raise ManagedExportError(f"Unable to read JSON file {path}: {exc}") from exc
    if size > maximum_bytes:
        raise ManagedExportError(f"JSON file exceeds {maximum_bytes} bytes: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8", errors="strict"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ManagedExportError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc


def require_mapping(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise ManagedExportError(f"{label} must be a JSON object.")
    return value


def require_string(source: Mapping[str, Any], key: str, *, allow_empty: bool = False, maximum: int = 4096) -> str:
    value = source.get(key)
    if not isinstance(value, str):
        raise ManagedExportError(f"{key} is required and must be a string.")
    if (not allow_empty and not value) or len(value) > maximum:
        raise ManagedExportError(f"{key} is empty or exceeds {maximum} characters.")
    if any(ord(character) < 0x20 or ord(character) == 0x7f for character in value):
        raise ManagedExportError(f"{key} contains a control character.")
    return value


def require_false(source: Mapping[str, Any], key: str) -> None:
    if source.get(key, False) is not False:
        raise ManagedExportError(f"{key} must be false; managed identifier export cannot escalate authority.")


def require_identifier(value: str, label: str) -> str:
    if not IDENTIFIER_RE.match(value):
        raise ManagedExportError(f"{label} must be a lowercase stable identifier: {value}")
    return value


def require_utc(value: str, label: str) -> str:
    if not UTC_RE.match(value):
        raise ManagedExportError(f"{label} must use whole-second UTC format YYYY-MM-DDTHH:MM:SSZ.")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise ManagedExportError(f"{label} is not a valid UTC timestamp: {value}") from exc
    return value


def resolve_document_path(value: str, base: Path) -> Path:
    path = Path(value).expanduser()
    return (base / path if not path.is_absolute() else path).resolve(strict=False)


def is_relative_to(path: Path, root: Path) -> bool:
    try:
        path.resolve(strict=False).relative_to(root.resolve(strict=False))
        return True
    except ValueError:
        return False


def contains_private_path(value: str) -> bool:
    return PRIVATE_PATH_RE.search(value) is not None


def assert_no_private_paths(value: Any, label: str = "export") -> None:
    if isinstance(value, str):
        if contains_private_path(value):
            raise ManagedExportError(f"{label} contains an absolute or private path: {value}")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            assert_no_private_paths(item, f"{label}[{index}]")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            assert_no_private_paths(item, f"{label}.{key}")


def sanitize_suffix(value: str) -> str:
    suffix = re.sub(r"[^a-z0-9._-]+", "-", value.lower()).strip(".-_")
    suffix = re.sub(r"[._-]{2,}", ".", suffix)
    return suffix[:160] or "unknown"


def observation_id(profile_id: str, claim_id: str, value: str) -> str:
    checksum = hashlib.sha256(value.encode("utf-8")).hexdigest()[:16]
    return f"observation.{profile_id}.{claim_id}.{checksum}"


def load_active_profile(workspace_path: Path) -> ActiveProfile:
    workspace = require_mapping(read_json(workspace_path), "workspace")
    if workspace.get("SchemaVersion") != 1:
        raise ManagedExportError("Workspace must use SchemaVersion 1.")
    workspace_root = resolve_document_path(require_string(workspace, "RootPath"), workspace_path.parent)
    active_profile_id = require_identifier(require_string(workspace, "ActiveGameProfileId", maximum=256), "ActiveGameProfileId")
    profiles = workspace.get("GameProfiles")
    if not isinstance(profiles, list):
        raise ManagedExportError("Workspace GameProfiles must be an array.")
    matches = [entry for entry in profiles if isinstance(entry, dict) and entry.get("ProfileId") == active_profile_id]
    if len(matches) != 1:
        raise ManagedExportError("Workspace ActiveGameProfileId must bind to exactly one profile.")

    profile = matches[0]
    runtime_target = require_string(profile, "RuntimeTarget", maximum=32)
    if runtime_target not in {"Mono", "IL2CPP"}:
        raise ManagedExportError("RuntimeTarget must be Mono or IL2CPP.")
    install_path = resolve_document_path(require_string(profile, "InstallPath"), workspace_path.parent)
    managed_path = resolve_document_path(require_string(profile, "ManagedAssembliesPath", allow_empty=runtime_target == "IL2CPP"), workspace_path.parent)
    extracted_path = resolve_document_path(require_string(profile, "ExtractedDataPath"), workspace_path.parent)

    if not install_path.is_dir():
        raise ManagedExportError(f"Configured FoA install path does not exist: {install_path}")
    if not is_relative_to(managed_path, install_path):
        raise ManagedExportError("ManagedAssembliesPath must remain inside the configured install path.")
    if not managed_path.is_dir():
        raise ManagedExportError("ManagedAssembliesPath must exist for managed identifier export.")
    if not is_relative_to(extracted_path, workspace_root):
        raise ManagedExportError("ExtractedDataPath must remain inside the workspace root.")
    extracted_path.mkdir(parents=True, exist_ok=True)

    return ActiveProfile(
        workspace_path=workspace_path,
        workspace_root=workspace_root,
        profile_id=require_identifier(require_string(profile, "ProfileId", maximum=256), "ProfileId"),
        game_version=require_string(profile, "GameVersion", maximum=128),
        branch=require_string(profile, "Branch", maximum=128),
        runtime_target=runtime_target,
        install_path=install_path,
        managed_assemblies_path=managed_path,
        extracted_data_path=extracted_path,
    )


def managed_locator(path: Path, profile: ActiveProfile) -> str:
    try:
        relative = path.resolve(strict=False).relative_to(profile.managed_assemblies_path.resolve(strict=False))
    except ValueError as exc:
        raise ManagedExportError(f"Managed assembly path escaped ManagedAssembliesPath: {path}") from exc
    if any(part in {"", ".", ".."} for part in relative.parts):
        raise ManagedExportError(f"Managed assembly path contains unsafe segment: {path}")
    return "$managed/" + relative.as_posix()


def read_bounded_file(path: Path) -> bytes:
    try:
        stat = path.stat()
    except OSError as exc:
        raise ManagedExportError(f"Unable to stat file: {path}: {exc}") from exc
    if stat.st_size > MAX_ASSEMBLY_BYTES:
        raise ManagedExportError(f"Refusing to inspect oversized managed assembly: {path}")
    try:
        return path.read_bytes()
    except OSError as exc:
        raise ManagedExportError(f"Unable to read managed assembly: {path}: {exc}") from exc


def printable_ascii_strings(payload: bytes) -> Iterable[str]:
    for match in re.finditer(rb"[ -~]{4,}", payload):
        try:
            yield match.group(0).decode("ascii")
        except UnicodeDecodeError:
            continue


def printable_utf16le_strings(payload: bytes) -> Iterable[str]:
    pattern = re.compile(rb"(?:[ -~]\x00){4,}")
    for match in pattern.finditer(payload):
        try:
            yield match.group(0).decode("utf-16le")
        except UnicodeDecodeError:
            continue


def is_candidate_managed_type(value: str) -> bool:
    if len(value) > 256 or contains_private_path(value):
        return False
    if "/" in value or "\\" in value or " " in value:
        return False
    if MANAGED_TYPE_RE.match(value):
        return True
    if TYPE_HINT_RE.match(value):
        return True
    return False


def extract_managed_type_candidates(payload: bytes) -> list[str]:
    candidates: set[str] = set()
    for value in list(printable_ascii_strings(payload)) + list(printable_utf16le_strings(payload)):
        value = value.strip("\x00 \t\r\n")
        if is_candidate_managed_type(value):
            candidates.add(value)
        if len(candidates) >= MAX_STRING_CANDIDATES:
            break
    return sorted(candidates)


def base_observation(
    *,
    profile: ActiveProfile,
    claim_id: str,
    value: str,
    locator: str,
    record_path: str,
    claim: str,
    evidence_kind: str,
    confidence: str,
    domain: str = "",
    record_kind: str = "",
    identity_kind: str = "",
    native_ref_exact: str = "",
    display_name: str = "",
    subject_ref: str | None = None,
) -> dict[str, Any]:
    defaults = CLAIM_DEFAULTS[claim_id]
    subject = subject_ref or defaults["subject_prefix"] + sanitize_suffix(native_ref_exact or value)
    result: dict[str, Any] = {
        "ObservationId": observation_id(profile.profile_id, claim_id, native_ref_exact or value),
        "SubjectRef": subject,
        "ClaimId": claim_id,
        "Claim": claim,
        "Value": value,
        "EvidenceKind": evidence_kind,
        "Confidence": confidence,
        "Locator": locator,
        "RecordPath": record_path,
        "PromoteAutomatically": False,
        "GrantsRuntimePermission": False,
    }
    resolved_domain = domain or defaults["domain"]
    resolved_kind = record_kind or defaults["record_kind"]
    resolved_identity = identity_kind or defaults["identity_kind"]
    optional = {
        "Domain": resolved_domain,
        "RecordKind": resolved_kind,
        "IdentityKind": resolved_identity,
        "NativeRefExact": native_ref_exact,
        "DisplayName": display_name,
    }
    for key, optional_value in optional.items():
        if optional_value:
            result[key] = optional_value
    assert_no_private_paths(result, label=result["ObservationId"])
    return result


def collect_managed_assembly_observations(profile: ActiveProfile) -> list[dict[str, Any]]:
    observations: list[dict[str, Any]] = []
    for assembly_name in ALLOWED_MANAGED_ASSEMBLIES:
        assembly_path = profile.managed_assemblies_path / assembly_name
        if not assembly_path.is_file():
            continue
        payload = read_bounded_file(assembly_path)
        for index, type_name in enumerate(extract_managed_type_candidates(payload)):
            observations.append(
                base_observation(
                    profile=profile,
                    claim_id="managed_type_name",
                    value=type_name,
                    locator=f"{managed_locator(assembly_path, profile)}:strings[{index}]",
                    record_path=f"$.Observations[{len(observations)}]",
                    claim=CLAIM_DEFAULTS["managed_type_name"]["claim"],
                    evidence_kind=CLAIM_DEFAULTS["managed_type_name"]["evidence"],
                    confidence=CLAIM_DEFAULTS["managed_type_name"]["confidence"],
                    display_name=type_name,
                )
            )
    return observations


def resolve_seed_paths(profile: ActiveProfile, explicit_seeds: Sequence[Path] | None) -> list[Path]:
    if explicit_seeds:
        candidates = [resolve_document_path(str(seed), profile.workspace_path.parent) for seed in explicit_seeds]
    else:
        default = profile.extracted_data_path / DEFAULT_SEED_NAME
        candidates = [default] if default.is_file() else []
    paths: list[Path] = []
    for candidate in candidates:
        candidate = candidate.resolve(strict=False)
        if not is_relative_to(candidate, profile.extracted_data_path):
            raise ManagedExportError("Managed identifier seed files must remain inside ExtractedDataPath.")
        if not candidate.is_file():
            raise ManagedExportError(f"Managed identifier seed file does not exist: {candidate}")
        paths.append(candidate)
    return sorted(paths)


def token_locator(seed_path: Path, profile: ActiveProfile) -> str:
    relative = seed_path.resolve(strict=False).relative_to(profile.extracted_data_path.resolve(strict=False))
    if any(part in {"", ".", ".."} for part in relative.parts):
        raise ManagedExportError("Managed identifier seed path contains unsafe segments.")
    return "$extracted/" + relative.as_posix()


def seed_value_to_mapping(value: Any, claim_id: str, index: int) -> Mapping[str, Any]:
    if isinstance(value, str):
        return {"Value": value}
    if isinstance(value, dict):
        return value
    raise ManagedExportError(f"Seed {claim_id}[{index}] must be a string or object.")


def seed_observation(profile: ActiveProfile, claim_id: str, collection_name: str, entry: Mapping[str, Any], locator_prefix: str, index: int) -> dict[str, Any]:
    defaults = CLAIM_DEFAULTS[claim_id]
    value = require_string(entry, "Value", maximum=8192)
    native_ref = require_string(entry, "NativeRefExact", allow_empty=True, maximum=512) if "NativeRefExact" in entry else ""
    if claim_id == "native_ref_exact":
        native_ref = native_ref or value
    subject = require_string(entry, "SubjectRef", allow_empty=True, maximum=768) if "SubjectRef" in entry else ""
    domain = require_string(entry, "Domain", allow_empty=True, maximum=64) if "Domain" in entry else defaults["domain"]
    kind = require_string(entry, "RecordKind", allow_empty=True, maximum=64) if "RecordKind" in entry else defaults["record_kind"]
    identity = require_string(entry, "IdentityKind", allow_empty=True, maximum=64) if "IdentityKind" in entry else defaults["identity_kind"]
    display_name = require_string(entry, "DisplayName", allow_empty=True, maximum=512) if "DisplayName" in entry else value
    claim_text = require_string(entry, "Claim", allow_empty=True, maximum=8192) if "Claim" in entry else defaults["claim"]
    return base_observation(
        profile=profile,
        claim_id=claim_id,
        value=value,
        locator=f"{locator_prefix}:$.{collection_name}[{index}]",
        record_path=f"$.{collection_name}[{index}]",
        claim=claim_text or defaults["claim"],
        evidence_kind=require_string(entry, "EvidenceKind", allow_empty=True, maximum=128) if "EvidenceKind" in entry else defaults["evidence"],
        confidence=require_string(entry, "Confidence", allow_empty=True, maximum=64) if "Confidence" in entry else defaults["confidence"],
        domain=domain,
        record_kind=kind,
        identity_kind=identity,
        native_ref_exact=native_ref,
        display_name=display_name,
        subject_ref=subject or None,
    )


def passthrough_observation(raw: Any, profile: ActiveProfile, locator_prefix: str, index: int) -> dict[str, Any]:
    document = {
        "SchemaVersion": 1,
        "DocumentKind": DOCUMENT_KIND,
        "ExportId": f"export.{profile.profile_id}.seed-probe",
        "ProfileId": profile.profile_id,
        "GameVersion": profile.game_version,
        "Branch": profile.branch,
        "RuntimeTarget": profile.runtime_target,
        "ToolName": TOOL_NAME,
        "ToolVersion": TOOL_VERSION,
        "CapturedAt": "2026-07-28T00:00:00Z",
        "PromoteAutomatically": False,
        "GrantsRuntimePermission": False,
        "Observations": [raw],
    }
    try:
        normalized = foa_identifier_export.normalize(document)
    except foa_identifier_export.ExportError as exc:
        raise ManagedExportError(str(exc)) from exc
    result = dict(normalized["Observations"][0])
    result["Locator"] = f"{locator_prefix}:{result['Locator']}"
    assert_no_private_paths(result, label=result["ObservationId"])
    return result


def load_seed_observations(profile: ActiveProfile, seed_path: Path) -> list[dict[str, Any]]:
    document = require_mapping(read_json(seed_path), f"managed identifier seed {seed_path}")
    if document.get("SchemaVersion") != 1:
        raise ManagedExportError("Managed identifier seed must use SchemaVersion 1.")
    if (
        require_string(document, "ProfileId", maximum=256) != profile.profile_id
        or require_string(document, "GameVersion", maximum=128) != profile.game_version
        or require_string(document, "Branch", maximum=128) != profile.branch
        or require_string(document, "RuntimeTarget", maximum=32) != profile.runtime_target
    ):
        raise ManagedExportError("Managed identifier seed must match the exact active workspace profile.")
    require_false(document, "PromoteAutomatically")
    require_false(document, "GrantsRuntimePermission")
    locator_prefix = token_locator(seed_path, profile)
    observations: list[dict[str, Any]] = []
    if "Observations" in document:
        raw_observations = document["Observations"]
        if not isinstance(raw_observations, list):
            raise ManagedExportError("Seed Observations must be an array.")
        for index, raw in enumerate(raw_observations):
            observations.append(passthrough_observation(raw, profile, locator_prefix, index))
    for collection_name, claim_id in SEED_COLLECTIONS.items():
        values = document.get(collection_name, [])
        if values is None:
            values = []
        if not isinstance(values, list):
            raise ManagedExportError(f"{collection_name} must be an array.")
        for index, raw in enumerate(values):
            observations.append(
                seed_observation(
                    profile,
                    claim_id,
                    collection_name,
                    seed_value_to_mapping(raw, claim_id, index),
                    locator_prefix,
                    index,
                )
            )
    return observations


def build_export(
    workspace_path: Path,
    *,
    seed_paths: Sequence[Path] | None = None,
    captured_at: str | None = None,
    include_assembly_strings: bool = True,
) -> dict[str, Any]:
    profile = load_active_profile(workspace_path)
    captured_at = require_utc(captured_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "CapturedAt")
    observations: list[dict[str, Any]] = []
    if include_assembly_strings:
        observations.extend(collect_managed_assembly_observations(profile))
    for seed_path in resolve_seed_paths(profile, seed_paths):
        observations.extend(load_seed_observations(profile, seed_path))
    if len(observations) > MAX_OBSERVATIONS:
        raise ManagedExportError(f"Managed identifier export exceeds {MAX_OBSERVATIONS} observations.")
    if not observations:
        raise ManagedExportError("No managed identifier observations were produced.")

    raw_export = {
        "SchemaVersion": 1,
        "DocumentKind": DOCUMENT_KIND,
        "ExportId": f"export.{profile.profile_id}.managed.{hashlib.sha256(canonical_json_bytes(observations)).hexdigest()[:16]}",
        "ProfileId": profile.profile_id,
        "GameVersion": profile.game_version,
        "Branch": profile.branch,
        "RuntimeTarget": profile.runtime_target,
        "ToolName": TOOL_NAME,
        "ToolVersion": TOOL_VERSION,
        "CapturedAt": captured_at,
        "PromoteAutomatically": False,
        "GrantsRuntimePermission": False,
        "Observations": observations,
    }
    normalized = foa_identifier_export.normalize(raw_export)
    assert_no_private_paths(normalized)
    return normalized


def write_export(document: Mapping[str, Any], output: Path, *, replace: bool = False) -> None:
    if output.exists() and not replace:
        raise ManagedExportError(f"Output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_bytes(pretty_json_bytes(document))
    os.replace(temporary, output)


def verify_export(input_path: Path, workspace_path: Path | None = None) -> dict[str, Any]:
    try:
        return foa_identifier_export.load_export(input_path, workspace_path)
    except foa_identifier_export.ExportError as exc:
        raise ManagedExportError(str(exc)) from exc


def default_output_path(profile: ActiveProfile) -> Path:
    return profile.extracted_data_path / DEFAULT_OUTPUT_NAME


def generate_fixture(output: Path, *, replace: bool = False) -> dict[str, Any]:
    if output.exists():
        if replace:
            shutil.rmtree(output) if output.is_dir() else output.unlink()
        elif any(output.iterdir() if output.is_dir() else [output]):
            raise ManagedExportError(f"Fixture output is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)
    workspace_root = output / "workspace"
    install_root = output / "lawful-local-fixture" / "FoA"
    managed = install_root / "Tainted Grail_Data" / "Managed"
    extracted = workspace_root / "Extracted"
    managed.mkdir(parents=True, exist_ok=True)
    extracted.mkdir(parents=True, exist_ok=True)
    (managed / "Assembly-CSharp.dll").write_bytes(
        b"\x00Game.Inventory.InventoryService\x00"
        b"\x00Game.Crafting.RecipeDatabase\x00"
        b"\x00FoA.Population.BanditTemplateDefinition\x00"
    )
    seed = {
        "SchemaVersion": 1,
        "ProfileId": "foa.mono.fixture",
        "GameVersion": "1.23.401",
        "Branch": "mono",
        "RuntimeTarget": "Mono",
        "PromoteAutomatically": False,
        "GrantsRuntimePermission": False,
        "TemplateKeys": [
            {
                "Value": "Characters/Templates/Bandit",
                "SubjectRef": "subject:foa:population:template:bandit",
                "DisplayName": "Synthetic Bandit Template",
            }
        ],
        "RecipeKeys": [
            {
                "Value": "Crafting/Recipes/IronIngot",
                "SubjectRef": "subject:foa:economy:recipe:iron-ingot",
                "DisplayName": "Synthetic Iron Ingot Recipe",
            }
        ],
    }
    (extracted / DEFAULT_SEED_NAME).write_bytes(pretty_json_bytes(seed))
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
    workspace_path = output / "workspace.tgworkspace.json"
    workspace_path.write_bytes(pretty_json_bytes(workspace))
    document = build_export(workspace_path, captured_at="2026-07-28T00:00:00Z")
    export_path = extracted / DEFAULT_OUTPUT_NAME
    write_export(document, export_path, replace=True)
    verify_export(export_path, workspace_path)
    return {
        "SchemaVersion": 1,
        "ManifestKind": "foa-managed-identifier-exporter-fixture",
        "ToolName": TOOL_NAME,
        "ToolVersion": TOOL_VERSION,
        "ExportPath": "$extracted/" + DEFAULT_OUTPUT_NAME,
        "ExportId": document["ExportId"],
        "ObservationCount": len(document["Observations"]),
        "Sha256": sha256_bytes(pretty_json_bytes(document)),
        "OperationalAuthority": {
            "RecursiveScanAllowed": False,
            "AssemblyLoadAllowed": False,
            "RuntimeInvocationAllowed": False,
            "GameMutationAllowed": False,
            "SaveAccessAllowed": False,
            "CatalogPromotionAllowed": False,
            "RuntimePermissionGranted": False,
        },
    }


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export bounded FoA managed identifier observations.")
    subcommands = parser.add_subparsers(dest="command", required=True)

    export = subcommands.add_parser("export")
    export.add_argument("--workspace", required=True, type=Path)
    export.add_argument("--output", type=Path)
    export.add_argument("--seed", action="append", type=Path, default=[])
    export.add_argument("--captured-at")
    export.add_argument("--replace", action="store_true")
    export.add_argument("--no-assembly-strings", action="store_true")

    verify = subcommands.add_parser("verify")
    verify.add_argument("--input", required=True, type=Path)
    verify.add_argument("--workspace", type=Path)

    fixture_cmd = subcommands.add_parser("fixture")
    fixture_cmd.add_argument("--output", required=True, type=Path)
    fixture_cmd.add_argument("--replace", action="store_true")

    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.command == "fixture":
            manifest = generate_fixture(args.output, replace=args.replace)
            print(f"FoA managed identifier exporter fixture wrote {manifest['ObservationCount']} observations.")
            return 0
        if args.command == "verify":
            document = verify_export(args.input, args.workspace)
            print(f"FoA managed identifier export verified: {document['ExportId']} with {len(document['Observations'])} observations.")
            return 0

        profile = load_active_profile(args.workspace)
        output = args.output or default_output_path(profile)
        if not is_relative_to(output.resolve(strict=False), profile.extracted_data_path):
            raise ManagedExportError("Managed identifier export output must remain inside ExtractedDataPath.")
        document = build_export(
            args.workspace,
            seed_paths=args.seed,
            captured_at=args.captured_at,
            include_assembly_strings=not args.no_assembly_strings,
        )
        write_export(document, output, replace=args.replace)
        verify_export(output, args.workspace)
        print(f"FoA managed identifier export wrote {len(document['Observations'])} observations to {output}.")
    except (ManagedExportError, foa_identifier_export.ExportError) as exc:
        print(f"FoA managed identifier exporter failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
