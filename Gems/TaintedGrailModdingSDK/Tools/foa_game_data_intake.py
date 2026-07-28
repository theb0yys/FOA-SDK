#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Create reviewed read-only FoA game-data intake candidates from sanitized local observations.

This tool is intentionally narrow. It does not scan arbitrary game directories, load
Unity assemblies, execute BepInEx/Harmony code, deploy files, mutate saves, promote
catalog facts, or grant runtime permission. It converts an explicitly supplied,
sanitized FoA local-capture JSON document into source/evidence documents and
catalog-promotion candidates that the existing SDK review path can consume.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

TOOL_VERSION = "0.1.0"
PROVIDER_ID = "provider.foa-local-capture"
PROVIDER_VERSION = "1.0.0"
IMPORTER_ID = "foa.local-game-data-intake"
SOURCE_KIND = "foa-local-diagnostic-capture"
SOURCE_MEDIA_TYPE = "application/vnd.foa.local-capture+json"
MANIFEST_NAME = "foa-game-data-intake.manifest.json"
SOURCE_INPUT_NAME = "source-input.foa-local-capture.json"
SOURCE_DOCUMENT_PATH = "Sources/{source_id}/source.tgsource.json"
EVIDENCE_DOCUMENT_PATH = "Sources/{source_id}/evidence.tgevidence.json"
CATALOG_CANDIDATE_PATH = "Catalog/Candidates/{source_id}.tgcatalog-candidates.json"
MAX_CAPTURE_BYTES = 16 * 1024 * 1024
MAX_OBSERVATIONS = 100_000
RESERVED_FORBIDDEN_USAGE = "no_unvalidated_runtime_use"

IDENTIFIER_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
SUBJECT_RE = re.compile(r"^subject:[A-Za-z0-9][A-Za-z0-9:._/-]{1,511}$")
SHA256_RE = re.compile(r"^sha256:[0-9a-f]{64}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
NATIVE_GUID_RE = re.compile(
    r"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"
)
ALLOWED_DOMAINS = {"economy", "population", "world", "quest", "dialogue", "audio", "ui", "runtime"}
ALLOWED_IDENTITY_KINDS = {"native", "synthetic", "composite", "source_scoped"}
ALLOWED_EVIDENCE_KINDS = {
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


class IntakeError(RuntimeError):
    """Raised when a capture cannot be accepted as candidate evidence."""


@dataclass(frozen=True)
class ActiveProfile:
    profile_id: str
    game_version: str
    branch: str
    runtime_target: str
    unity_version: str
    bep_in_ex_version: str
    install_path: str
    managed_assemblies_path: str
    plugin_path: str
    extracted_data_path: str


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def pretty_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def sha256_bytes(payload: bytes) -> str:
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def read_json_file(path: Path, *, maximum_bytes: int = MAX_CAPTURE_BYTES) -> Any:
    try:
        stat = path.stat()
    except OSError as exc:
        raise IntakeError(f"Unable to read JSON file {path}: {exc}") from exc
    if stat.st_size > maximum_bytes:
        raise IntakeError(f"JSON file exceeds {maximum_bytes} bytes: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8", errors="strict"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise IntakeError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc


def require_mapping(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise IntakeError(f"{label} must be a JSON object.")
    return value


def require_string(container: Mapping[str, Any], key: str, *, allow_empty: bool = False, maximum: int = 4096) -> str:
    value = container.get(key)
    if not isinstance(value, str):
        raise IntakeError(f"{key} is required and must be a string.")
    if (not allow_empty and not value) or len(value) > maximum:
        raise IntakeError(f"{key} is empty or exceeds {maximum} characters.")
    if any(ord(ch) < 0x20 or ord(ch) == 0x7F for ch in value):
        raise IntakeError(f"{key} contains a control character.")
    return value


def require_bool_false(container: Mapping[str, Any], key: str) -> None:
    value = container.get(key, False)
    if value is not False:
        raise IntakeError(f"{key} must be false; intake cannot escalate authority.")


def require_identifier(value: str, label: str) -> str:
    if not IDENTIFIER_RE.match(value):
        raise IntakeError(f"{label} must be a lowercase stable identifier: {value}")
    return value


def require_subject(value: str, label: str) -> str:
    if not SUBJECT_RE.match(value):
        raise IntakeError(f"{label} must be an explicit subject reference: {value}")
    return value


def require_utc(value: str, label: str) -> str:
    if not UTC_RE.match(value):
        raise IntakeError(f"{label} must use whole-second UTC format YYYY-MM-DDTHH:MM:SSZ.")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise IntakeError(f"{label} is not a valid UTC timestamp: {value}") from exc
    return value


def require_sha256(value: str, label: str) -> str:
    if not SHA256_RE.match(value):
        raise IntakeError(f"{label} must be lowercase sha256:<64-hex>.")
    return value


def normalize_runtime(value: str) -> str:
    if value not in {"Mono", "IL2CPP"}:
        raise IntakeError("RuntimeTarget must be Mono or IL2CPP.")
    return value


def load_active_profile(workspace_path: Path) -> ActiveProfile:
    workspace = require_mapping(read_json_file(workspace_path), "workspace")
    if workspace.get("SchemaVersion") != 1:
        raise IntakeError("Workspace must use SchemaVersion 1.")
    active_profile_id = require_identifier(require_string(workspace, "ActiveGameProfileId", maximum=256), "ActiveGameProfileId")
    profiles = workspace.get("GameProfiles")
    if not isinstance(profiles, list):
        raise IntakeError("Workspace GameProfiles must be an array.")
    matches = [profile for profile in profiles if isinstance(profile, dict) and profile.get("ProfileId") == active_profile_id]
    if len(matches) != 1:
        raise IntakeError("Workspace ActiveGameProfileId must bind to exactly one profile.")
    profile = matches[0]
    runtime_target = normalize_runtime(require_string(profile, "RuntimeTarget", maximum=32))
    return ActiveProfile(
        profile_id=require_identifier(require_string(profile, "ProfileId", maximum=256), "ProfileId"),
        game_version=require_string(profile, "GameVersion", maximum=128),
        branch=require_string(profile, "Branch", maximum=128),
        runtime_target=runtime_target,
        unity_version=require_string(profile, "UnityVersion", allow_empty=True, maximum=128),
        bep_in_ex_version=require_string(profile, "BepInExVersion", allow_empty=runtime_target == "IL2CPP", maximum=128),
        install_path=require_string(profile, "InstallPath", maximum=4096),
        managed_assemblies_path=require_string(profile, "ManagedAssembliesPath", allow_empty=runtime_target == "IL2CPP", maximum=4096),
        plugin_path=require_string(profile, "PluginPath", allow_empty=runtime_target == "IL2CPP", maximum=4096),
        extracted_data_path=require_string(profile, "ExtractedDataPath", allow_empty=True, maximum=4096),
    )


def ensure_capture_matches_profile(capture: Mapping[str, Any], profile: ActiveProfile) -> None:
    if capture.get("SchemaVersion") != 1:
        raise IntakeError("Capture input must use SchemaVersion 1.")
    require_identifier(require_string(capture, "CaptureId", maximum=256), "CaptureId")
    if (
        require_string(capture, "ProfileId", maximum=256) != profile.profile_id
        or require_string(capture, "GameVersion", maximum=128) != profile.game_version
        or require_string(capture, "Branch", maximum=128) != profile.branch
        or normalize_runtime(require_string(capture, "RuntimeTarget", maximum=32)) != profile.runtime_target
    ):
        raise IntakeError("Capture input must match the exact active workspace profile.")
    require_string(capture, "ToolName", maximum=256)
    require_string(capture, "ToolVersion", maximum=128)
    require_utc(require_string(capture, "CapturedAt", maximum=32), "CapturedAt")
    if capture.get("SourceKind", SOURCE_KIND) != SOURCE_KIND:
        raise IntakeError(f"SourceKind must be {SOURCE_KIND}.")
    require_bool_false(capture, "PromoteAutomatically")
    require_bool_false(capture, "GrantsRuntimePermission")


def slug_from_subject(subject_ref: str) -> str:
    slug = re.sub(r"[^a-z0-9._-]+", ".", subject_ref.lower().removeprefix("subject:"))
    slug = re.sub(r"[.]{2,}", ".", slug).strip(".")
    return slug[:160] or "unresolved"


def value_fingerprint(observation: Mapping[str, Any]) -> str:
    stable = {
        "SubjectRef": observation.get("SubjectRef", ""),
        "ClaimId": observation.get("ClaimId", ""),
        "Value": observation.get("Value", ""),
        "NativeRefExact": observation.get("NativeRefExact", ""),
        "AssetGuid": observation.get("AssetGuid", ""),
        "AddressableKey": observation.get("AddressableKey", ""),
    }
    return sha256_bytes(canonical_json_bytes(stable))


def validate_observation(raw: Any, *, index: int, profile: ActiveProfile, source_revision: str) -> dict[str, Any]:
    observation = dict(require_mapping(raw, f"Observations[{index}]"))
    observation_id = require_identifier(require_string(observation, "ObservationId", maximum=192), "ObservationId")
    subject_ref = require_subject(require_string(observation, "SubjectRef", maximum=768), "SubjectRef")
    claim_id = require_identifier(require_string(observation, "ClaimId", maximum=192), "ClaimId")
    claim = require_string(observation, "Claim", maximum=8192)
    evidence_kind = require_string(observation, "EvidenceKind", maximum=128)
    if evidence_kind not in ALLOWED_EVIDENCE_KINDS:
        raise IntakeError(f"Observation {observation_id} has unsupported EvidenceKind {evidence_kind}.")
    confidence = require_string(observation, "Confidence", maximum=64)
    if confidence not in ALLOWED_CONFIDENCE:
        raise IntakeError(f"Observation {observation_id} has unsupported Confidence {confidence}.")
    locator = require_string(observation, "Locator", maximum=2048)
    record_path = require_string(observation, "RecordPath", maximum=2048)
    require_bool_false(observation, "PromoteAutomatically")
    require_bool_false(observation, "GrantsRuntimePermission")

    domain = require_string(observation, "Domain", allow_empty=True, maximum=64) if "Domain" in observation else ""
    record_kind = require_string(observation, "RecordKind", allow_empty=True, maximum=64) if "RecordKind" in observation else ""
    if bool(domain) != bool(record_kind):
        raise IntakeError(f"Observation {observation_id} must provide Domain and RecordKind together.")
    if domain and domain not in ALLOWED_DOMAINS:
        raise IntakeError(f"Observation {observation_id} has unsupported Domain {domain}.")
    identity_kind = observation.get("IdentityKind", "")
    if identity_kind:
        identity_kind = require_string(observation, "IdentityKind", maximum=64)
        if identity_kind not in ALLOWED_IDENTITY_KINDS:
            raise IntakeError(f"Observation {observation_id} has unsupported IdentityKind {identity_kind}.")

    native_ref = observation.get("NativeRefExact", "")
    if native_ref:
        native_ref = require_string(observation, "NativeRefExact", maximum=512)
        if identity_kind == "synthetic":
            raise IntakeError(f"Observation {observation_id} cannot assign a native ref to a synthetic record.")
        if claim_id in {"native_ref_exact", "unity_guid"} and not NATIVE_GUID_RE.match(native_ref):
            raise IntakeError(f"Observation {observation_id} has malformed NativeRefExact GUID.")
    owner_pack_id = observation.get("OwnerPackId", "")
    if owner_pack_id:
        owner_pack_id = require_identifier(require_string(observation, "OwnerPackId", maximum=192), "OwnerPackId")
    if identity_kind == "synthetic" and not owner_pack_id:
        raise IntakeError(f"Observation {observation_id} synthetic records require OwnerPackId.")
    if identity_kind == "native" and owner_pack_id:
        raise IntakeError(f"Observation {observation_id} native records cannot claim OwnerPackId.")

    record_id = observation.get("RecordId", "")
    if record_id:
        record_id = require_identifier(require_string(observation, "RecordId", maximum=192), "RecordId")
    elif domain and record_kind:
        record_id = f"candidate.{domain}.{record_kind}.{slug_from_subject(subject_ref)}"
    display_name = require_string(observation, "DisplayName", allow_empty=True, maximum=512) if "DisplayName" in observation else ""

    return {
        "ObservationId": observation_id,
        "ProviderId": PROVIDER_ID,
        "ProviderVersion": PROVIDER_VERSION,
        "SourceRevision": source_revision,
        "ProfileId": profile.profile_id,
        "GameVersion": profile.game_version,
        "Branch": profile.branch,
        "RuntimeTarget": profile.runtime_target,
        "SubjectRef": subject_ref,
        "ClaimId": claim_id,
        "Claim": claim,
        "Value": require_string(observation, "Value", allow_empty=True, maximum=8192) if "Value" in observation else "",
        "EvidenceKind": evidence_kind,
        "Confidence": confidence,
        "Locator": locator,
        "RecordPath": record_path,
        "Domain": domain,
        "RecordKind": record_kind,
        "IdentityKind": identity_kind,
        "OwnerPackId": owner_pack_id,
        "RecordId": record_id,
        "NativeRefExact": native_ref,
        "DisplayName": display_name,
        "ValueFingerprint": value_fingerprint(observation),
        "PromoteAutomatically": False,
        "GrantsRuntimePermission": False,
    }


def source_revision_for_capture(capture_bytes: bytes) -> str:
    return hashlib.sha256(capture_bytes).hexdigest()[:40]


def build_documents(workspace_path: Path, capture_path: Path, *, imported_at: str | None = None) -> dict[str, Any]:
    profile = load_active_profile(workspace_path)
    capture = require_mapping(read_json_file(capture_path), "capture input")
    ensure_capture_matches_profile(capture, profile)
    imported_at = require_utc(imported_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "ImportedAt")
    raw_observations = capture.get("Observations")
    if not isinstance(raw_observations, list) or not raw_observations:
        raise IntakeError("Capture input must contain at least one observation.")
    if len(raw_observations) > MAX_OBSERVATIONS:
        raise IntakeError(f"Capture input exceeds {MAX_OBSERVATIONS} observations.")

    canonical_capture_bytes = canonical_json_bytes(capture)
    source_fingerprint = sha256_bytes(canonical_capture_bytes)
    source_id = f"source.{profile.profile_id}.{hashlib.sha256(canonical_capture_bytes).hexdigest()[:16]}"
    source_revision = source_revision_for_capture(canonical_capture_bytes)
    captured_at = require_utc(require_string(capture, "CapturedAt", maximum=32), "CapturedAt")
    observations = [
        validate_observation(raw, index=index, profile=profile, source_revision=source_revision)
        for index, raw in enumerate(raw_observations)
    ]
    observation_ids = [value["ObservationId"] for value in observations]
    duplicate_observation_ids = sorted({value for value in observation_ids if observation_ids.count(value) > 1})
    if duplicate_observation_ids:
        raise IntakeError("Duplicate observation IDs are refused: " + ", ".join(duplicate_observation_ids))

    source_document = {
        "SchemaVersion": 1,
        "Source": {
            "SourceId": source_id,
            "Title": require_string(capture, "Title", allow_empty=True, maximum=512) if "Title" in capture else "FoA local game data capture",
            "SourceKind": SOURCE_KIND,
            "Locator": require_string(capture, "Locator", maximum=2048) if "Locator" in capture else SOURCE_INPUT_NAME,
            "Fingerprint": source_fingerprint,
            "ProfileId": profile.profile_id,
            "GameVersion": profile.game_version,
            "Branch": profile.branch,
            "RuntimeTarget": profile.runtime_target,
            "ToolName": require_string(capture, "ToolName", maximum=256),
            "ToolVersion": require_string(capture, "ToolVersion", maximum=128),
            "ImporterId": IMPORTER_ID,
            "ImporterVersion": TOOL_VERSION,
            "CapturedAt": captured_at,
            "ImportedAt": imported_at,
            "Limitations": "Read-only sanitized local capture; no game mutation, runtime invocation, asset copy, save access, automatic catalog promotion, or permission grant.",
            "MediaType": SOURCE_MEDIA_TYPE,
            "ByteSize": len(canonical_capture_bytes),
            "ImportStatus": "imported",
        },
        "Issues": [],
    }
    evidence_records = [
        {
            "EvidenceId": f"evidence.{observation['ObservationId']}",
            "SourceId": source_id,
            "SourceFingerprint": source_fingerprint,
            "ProfileId": profile.profile_id,
            "GameVersion": profile.game_version,
            "Branch": profile.branch,
            "SubjectRef": observation["SubjectRef"],
            "Claim": f"{observation['ClaimId']}: {observation['Claim']}",
            "EvidenceKind": observation["EvidenceKind"],
            "Confidence": observation["Confidence"],
            "Locator": observation["Locator"],
            "RecordPath": observation["RecordPath"],
            "ExtractedAt": captured_at,
        }
        for observation in observations
    ]
    return {
        "source_id": source_id,
        "source_input": capture,
        "source_document": source_document,
        "evidence_document": {
            "SchemaVersion": 1,
            "SourceId": source_id,
            "SourceFingerprint": source_fingerprint,
            "ProfileId": profile.profile_id,
            "GameVersion": profile.game_version,
            "Branch": profile.branch,
            "Evidence": evidence_records,
            "Issues": [],
        },
        "candidate_document": build_catalog_candidates(
            observations=observations,
            source_id=source_id,
            source_fingerprint=source_fingerprint,
            profile=profile,
            captured_at=captured_at,
        ),
    }


def build_catalog_candidates(*, observations: Sequence[Mapping[str, Any]], source_id: str, source_fingerprint: str, profile: ActiveProfile, captured_at: str) -> dict[str, Any]:
    records: list[dict[str, Any]] = []
    bindings: list[dict[str, Any]] = []
    issues: list[dict[str, Any]] = []
    seen_records: dict[str, str] = {}
    seen_native_refs: dict[str, str] = {}
    for observation in observations:
        evidence_id = f"evidence.{observation['ObservationId']}"
        bindings.append(
            {
                "BindingId": f"binding.{observation['ObservationId']}",
                "ObservationId": observation["ObservationId"],
                "SubjectRef": observation["SubjectRef"],
                "ClaimId": observation["ClaimId"],
                "Value": observation["Value"],
                "NativeRefExact": observation["NativeRefExact"],
                "ValueFingerprint": observation["ValueFingerprint"],
                "EvidenceIds": [evidence_id],
            }
        )
        if not observation["Domain"] or not observation["RecordKind"]:
            continue
        record_id = observation["RecordId"]
        native_ref = observation["NativeRefExact"]
        if record_id in seen_records:
            issues.append({"IssueId": f"issue.{observation['ObservationId']}.duplicate-record-id", "Severity": "error", "Code": "catalog-candidate.duplicate-record-id", "Message": f"RecordId {record_id} is produced by multiple observations.", "Locator": observation["Locator"], "RecordPath": observation["RecordPath"], "Line": 0})
            continue
        if native_ref and native_ref.lower() in seen_native_refs:
            issues.append({"IssueId": f"issue.{observation['ObservationId']}.duplicate-native-ref", "Severity": "error", "Code": "catalog-candidate.duplicate-native-ref", "Message": f"NativeRefExact {native_ref} is produced by multiple candidate records.", "Locator": observation["Locator"], "RecordPath": observation["RecordPath"], "Line": 0})
            continue
        seen_records[record_id] = observation["ObservationId"]
        if native_ref:
            seen_native_refs[native_ref.lower()] = observation["ObservationId"]
        identity_kind = observation["IdentityKind"] or ("native" if native_ref else "source_scoped")
        records.append(
            {
                "RecordId": record_id,
                "OwnerPackId": observation["OwnerPackId"],
                "Domain": observation["Domain"],
                "RecordKind": observation["RecordKind"],
                "SubjectRef": observation["SubjectRef"],
                "NativeRefExact": native_ref,
                "IdentityKind": identity_kind,
                "DisplayName": observation["DisplayName"],
                "Aliases": [],
                "SourceScopedRefs": [source_id],
                "ResearchStage": "S2",
                "Confidence": observation["Confidence"],
                "OperationalRisk": "unknown",
                "ValidationState": "unvalidated",
                "StalenessState": "unknown",
                "AllowedUsages": [],
                "ForbiddenUsages": [RESERVED_FORBIDDEN_USAGE],
                "EvidenceIds": [evidence_id],
                "MissingRefs": [],
                "ConflictRefs": [],
                "Tags": ["foa-local-capture", observation["ClaimId"]],
                "CreatedAt": captured_at,
                "UpdatedAt": captured_at,
                "SupersededByRecordId": "",
                "PromotionState": "candidate",
                "PromotionBlocked": bool(issues),
            }
        )
    records.sort(key=lambda value: value["RecordId"])
    bindings.sort(key=lambda value: value["BindingId"])
    return {
        "SchemaVersion": 1,
        "DocumentKind": "foa-catalog-promotion-candidates",
        "SourceId": source_id,
        "SourceFingerprint": source_fingerprint,
        "ProfileId": profile.profile_id,
        "GameVersion": profile.game_version,
        "Branch": profile.branch,
        "RuntimeTarget": profile.runtime_target,
        "ProviderId": PROVIDER_ID,
        "ProviderVersion": PROVIDER_VERSION,
        "PromotionAllowed": False,
        "RuntimePermissionGranted": False,
        "Records": records,
        "NativeBindings": bindings,
        "Issues": issues,
    }


def expected_paths(source_id: str) -> list[str]:
    return sorted([SOURCE_INPUT_NAME, SOURCE_DOCUMENT_PATH.format(source_id=source_id), EVIDENCE_DOCUMENT_PATH.format(source_id=source_id), CATALOG_CANDIDATE_PATH.format(source_id=source_id)])


def write_documents(documents: Mapping[str, Any], output: Path, *, replace: bool = False) -> dict[str, Any]:
    source_id = str(documents["source_id"])
    if output.exists():
        if replace:
            if output.is_dir():
                shutil.rmtree(output)
            else:
                output.unlink()
        elif any(output.iterdir() if output.is_dir() else [output]):
            raise IntakeError(f"Output directory is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)
    payloads = {
        SOURCE_INPUT_NAME: canonical_json_bytes(documents["source_input"]),
        SOURCE_DOCUMENT_PATH.format(source_id=source_id): pretty_json_bytes(documents["source_document"]),
        EVIDENCE_DOCUMENT_PATH.format(source_id=source_id): pretty_json_bytes(documents["evidence_document"]),
        CATALOG_CANDIDATE_PATH.format(source_id=source_id): pretty_json_bytes(documents["candidate_document"]),
    }
    entries = []
    for relative, payload in sorted(payloads.items()):
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(payload)
        entries.append({"path": relative, "sha256": sha256_bytes(payload), "size_bytes": len(payload)})
    manifest = {
        "SchemaVersion": 1,
        "ManifestKind": "foa-game-data-intake-output",
        "ToolId": IMPORTER_ID,
        "ToolVersion": TOOL_VERSION,
        "SourceId": source_id,
        "Files": entries,
        "OperationalAuthority": {
            "GameFileReadBoundedToInput": True,
            "GameMutationAllowed": False,
            "RuntimeInvocationAllowed": False,
            "SaveAccessAllowed": False,
            "CatalogPromotionAllowed": False,
            "RuntimePermissionGranted": False,
        },
    }
    (output / MANIFEST_NAME).write_bytes(pretty_json_bytes(manifest))
    return manifest


def verify_output(output: Path) -> dict[str, Any]:
    if not output.is_dir():
        raise IntakeError(f"Output directory does not exist: {output}")
    manifest = require_mapping(read_json_file(output / MANIFEST_NAME), "manifest")
    if manifest.get("SchemaVersion") != 1 or manifest.get("ManifestKind") != "foa-game-data-intake-output":
        raise IntakeError("Output manifest is not a FoA game-data intake manifest.")
    authority = require_mapping(manifest.get("OperationalAuthority"), "OperationalAuthority")
    for key in ("GameMutationAllowed", "RuntimeInvocationAllowed", "SaveAccessAllowed", "CatalogPromotionAllowed", "RuntimePermissionGranted"):
        if authority.get(key) is not False:
            raise IntakeError(f"Output authority must keep {key}=false.")
    files = manifest.get("Files")
    if not isinstance(files, list) or not files:
        raise IntakeError("Manifest Files must be a non-empty array.")
    seen = set()
    for entry_raw in files:
        entry = require_mapping(entry_raw, "manifest file entry")
        relative = require_string(entry, "path", maximum=4096)
        if relative.startswith("/") or ".." in Path(relative).parts:
            raise IntakeError(f"Manifest path is unsafe: {relative}")
        if relative in seen:
            raise IntakeError(f"Manifest path is duplicated: {relative}")
        seen.add(relative)
        payload_path = output / relative
        if not payload_path.is_file():
            raise IntakeError(f"Manifest payload is missing: {relative}")
        payload = payload_path.read_bytes()
        if entry.get("size_bytes") != len(payload):
            raise IntakeError(f"Manifest size mismatch: {relative}")
        if require_sha256(str(entry.get("sha256", "")), "sha256") != sha256_bytes(payload):
            raise IntakeError(f"Manifest SHA-256 mismatch: {relative}")
    source_id = require_identifier(require_string(manifest, "SourceId", maximum=256), "SourceId")
    required = set(expected_paths(source_id))
    if seen != required:
        raise IntakeError(f"Manifest file set mismatch: expected {sorted(required)}, got {sorted(seen)}")
    evidence = require_mapping(read_json_file(output / EVIDENCE_DOCUMENT_PATH.format(source_id=source_id)), "evidence document")
    candidates = require_mapping(read_json_file(output / CATALOG_CANDIDATE_PATH.format(source_id=source_id)), "candidate document")
    if evidence.get("SourceId") != source_id or candidates.get("SourceId") != source_id:
        raise IntakeError("Evidence and candidate documents must bind to the manifest SourceId.")
    if candidates.get("PromotionAllowed") is not False or candidates.get("RuntimePermissionGranted") is not False:
        raise IntakeError("Candidate document must not grant promotion or runtime permission.")
    evidence_ids = {entry["EvidenceId"] for entry in evidence.get("Evidence", []) if isinstance(entry, dict)}
    for record in candidates.get("Records", []):
        if not isinstance(record, dict):
            raise IntakeError("Candidate records must be objects.")
        for evidence_id in record.get("EvidenceIds", []):
            if evidence_id not in evidence_ids:
                raise IntakeError(f"Candidate record references missing evidence: {evidence_id}")
    return dict(manifest)


def generate_fixture(output: Path, *, replace: bool = False) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="foa-game-data-intake-fixture-") as temporary:
        temp = Path(temporary)
        workspace = {"SchemaVersion": 1, "WorkspaceId": "fixture.workspace", "DisplayName": "Fixture Workspace", "RootPath": "./fixture-workspace", "OutputPath": "./fixture-workspace/Build", "StagingPath": "./fixture-workspace/Staging", "DeploymentPath": "./fixture-workspace/Deployment", "ActiveGameProfileId": "foa.mono.fixture", "GameProfiles": [{"ProfileId": "foa.mono.fixture", "DisplayName": "FoA Mono Fixture", "InstallPath": "./lawful-local-fixture/FoA", "GameVersion": "1.23.401", "Branch": "mono", "RuntimeTarget": "Mono", "UnityVersion": "6000.0.64f1", "BepInExVersion": "5.4.23.3", "ManagedAssembliesPath": "./lawful-local-fixture/FoA/Tainted Grail_Data/Managed", "PluginPath": "./lawful-local-fixture/FoA/BepInEx/plugins", "DiagnosticsPath": "./fixture-workspace/Diagnostics", "ExtractedDataPath": "./fixture-workspace/Extracted", "DlcScopes": ["base-game"]}]}
        capture = {"SchemaVersion": 1, "CaptureId": "capture.foa.fixture", "Title": "Synthetic FoA local identifier capture", "SourceKind": SOURCE_KIND, "ProfileId": "foa.mono.fixture", "GameVersion": "1.23.401", "Branch": "mono", "RuntimeTarget": "Mono", "ToolName": "FoA Synthetic Diagnostic Capture", "ToolVersion": "1.0.0", "CapturedAt": "2026-07-28T00:00:00Z", "Locator": "fixture.foa-local-capture.json", "PromoteAutomatically": False, "GrantsRuntimePermission": False, "Observations": [{"ObservationId": "observation.fixture.item.native-ref", "SubjectRef": "subject:foa:economy:item:iron-ore", "ClaimId": "native_ref_exact", "Claim": "Native item identifier was observed in the sanitized local diagnostic capture.", "Value": "00000000-0000-0000-0000-000000000001", "Domain": "economy", "RecordKind": "item", "IdentityKind": "native", "NativeRefExact": "00000000-0000-0000-0000-000000000001", "DisplayName": "Synthetic Iron Ore", "EvidenceKind": "native-identifier-observation", "Confidence": "observed", "Locator": "$.items[0].guid", "RecordPath": "$.Observations[0]", "PromoteAutomatically": False, "GrantsRuntimePermission": False}, {"ObservationId": "observation.fixture.recipe.native-ref", "SubjectRef": "subject:foa:economy:recipe:iron-ingot", "ClaimId": "native_ref_exact", "Claim": "Native recipe identifier was observed in the sanitized local diagnostic capture.", "Value": "00000000-0000-0000-0000-000000000002", "Domain": "economy", "RecordKind": "recipe", "IdentityKind": "native", "NativeRefExact": "00000000-0000-0000-0000-000000000002", "DisplayName": "Synthetic Iron Ingot Recipe", "EvidenceKind": "native-identifier-observation", "Confidence": "observed", "Locator": "$.recipes[0].guid", "RecordPath": "$.Observations[1]", "PromoteAutomatically": False, "GrantsRuntimePermission": False}, {"ObservationId": "observation.fixture.template.addressable", "SubjectRef": "subject:foa:population:template:bandit", "ClaimId": "addressable_key", "Claim": "Population template addressable key was observed in the sanitized local diagnostic capture.", "Value": "Characters/Templates/Bandit", "Domain": "population", "RecordKind": "template", "IdentityKind": "source_scoped", "DisplayName": "Synthetic Bandit Template", "EvidenceKind": "addressable-observation", "Confidence": "observed", "Locator": "$.templates[0].addressableKey", "RecordPath": "$.Observations[2]", "PromoteAutomatically": False, "GrantsRuntimePermission": False}]}
        workspace_path = temp / "workspace.tgworkspace.json"
        capture_path = temp / "capture.foa-local-capture.json"
        workspace_path.write_bytes(pretty_json_bytes(workspace))
        capture_path.write_bytes(pretty_json_bytes(capture))
        documents = build_documents(workspace_path, capture_path, imported_at="2026-07-28T00:00:01Z")
        manifest = write_documents(documents, output, replace=replace)
        verify_output(output)
        return manifest


def parse_arguments(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create read-only FoA local game-data intake evidence.")
    subcommands = parser.add_subparsers(dest="command", required=True)
    capture = subcommands.add_parser("capture", help="Convert a sanitized local-capture JSON file into intake output.")
    capture.add_argument("--workspace", required=True, type=Path)
    capture.add_argument("--input", required=True, type=Path)
    capture.add_argument("--output", required=True, type=Path)
    capture.add_argument("--imported-at", help="Whole-second UTC import time for deterministic evidence output.")
    capture.add_argument("--replace", action="store_true")
    fixture = subcommands.add_parser("fixture", help="Generate a synthetic project-owned fixture output.")
    fixture.add_argument("--output", required=True, type=Path)
    fixture.add_argument("--replace", action="store_true")
    verify = subcommands.add_parser("verify", help="Verify generated intake output.")
    verify.add_argument("--output", required=True, type=Path)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    try:
        if arguments.command == "capture":
            documents = build_documents(arguments.workspace, arguments.input, imported_at=arguments.imported_at)
            manifest = write_documents(documents, arguments.output, replace=arguments.replace)
            print(f"FoA game-data intake wrote {len(manifest['Files'])} files for {manifest['SourceId']}.")
        elif arguments.command == "fixture":
            manifest = generate_fixture(arguments.output, replace=arguments.replace)
            print(f"FoA game-data intake fixture wrote {len(manifest['Files'])} files for {manifest['SourceId']}.")
        elif arguments.command == "verify":
            manifest = verify_output(arguments.output)
            print(f"FoA game-data intake output verified for {manifest['SourceId']}.")
        else:
            raise IntakeError(f"Unknown command: {arguments.command}")
    except IntakeError as exc:
        print(f"FoA game-data intake failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
