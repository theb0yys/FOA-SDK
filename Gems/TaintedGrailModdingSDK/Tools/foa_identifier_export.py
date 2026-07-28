#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate and normalize FoA identifier exports for bounded local intake.

`foa-identifiers.json` is the explicit, sanitized identifier handoff consumed
from `ExtractedDataPath` by the bounded local diagnostic collector. This tool
validates and canonicalizes that handoff. It does not scan game directories,
load Unity assemblies, execute BepInEx/Harmony, call FoA APIs, copy game
payloads, promote catalog facts, or grant runtime permission.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

TOOL_ID = "foa.identifier-export-contract"
TOOL_VERSION = "0.1.0"
DOCUMENT_KIND = "foa-identifier-export"
DEFAULT_EXPORT_NAME = "foa-identifiers.json"
MAX_JSON_BYTES = 16 * 1024 * 1024
MAX_OBSERVATIONS = 100_000
IDENTIFIER_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
SUBJECT_RE = re.compile(r"^subject:[A-Za-z0-9][A-Za-z0-9:._/-]{1,511}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
GUID_RE = re.compile(r"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")
PRIVATE_PATH_RE = re.compile(r"(^/|^~/|^[A-Za-z]:[\\/]|^\\\\)")
LOCATOR_RE = re.compile(r"(^\$([.\[]|$)|^\$(install|managed|plugin|extracted|capture)(/|:|$))")
DOMAINS = {"economy", "population", "world", "quest", "dialogue", "audio", "ui", "runtime"}
IDENTITY_KINDS = {"native", "synthetic", "composite", "source_scoped"}
EVIDENCE_KINDS = {"runtime-observation", "local-diagnostic-capture", "template-diagnostics", "native-identifier-observation", "addressable-observation", "assetbundle-observation", "managed-type-observation", "unity-guid-observation"}
CONFIDENCE = {"unrated", "observed", "documented", "inferred"}
ALLOWED_CLAIM_IDS = {"native_ref_exact", "unity_guid", "addressable_key", "assetbundle_name", "managed_type_name", "template_key", "recipe_key", "localization_key", "asset_path_token", "diagnostic_fact"}
CLAIMS = ALLOWED_CLAIM_IDS
VALUE_REQUIRED = CLAIMS - {"diagnostic_fact"}


class ExportError(RuntimeError):
    pass


def pretty(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def pretty_json_bytes(value: Any) -> bytes:
    return pretty(value)


def digest(value: bytes) -> str:
    return "sha256:" + hashlib.sha256(value).hexdigest()


def read_json(path: Path) -> Any:
    try:
        if path.stat().st_size > MAX_JSON_BYTES:
            raise ExportError(f"JSON file exceeds {MAX_JSON_BYTES} bytes: {path}")
        return json.loads(path.read_text(encoding="utf-8", errors="strict"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ExportError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc


def obj(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise ExportError(f"{label} must be a JSON object.")
    return value


def text(source: Mapping[str, Any], key: str, *, empty: bool = False, limit: int = 4096) -> str:
    value = source.get(key)
    if not isinstance(value, str) or (not empty and not value) or len(value) > limit:
        raise ExportError(f"{key} is required as bounded text.")
    if any(ord(ch) < 0x20 or ord(ch) == 0x7f for ch in value):
        raise ExportError(f"{key} contains a control character.")
    return value


def stable(value: str, label: str) -> str:
    if not IDENTIFIER_RE.match(value):
        raise ExportError(f"{label} must be a lowercase stable identifier: {value}")
    return value


def utc(value: str, label: str) -> str:
    if not UTC_RE.match(value):
        raise ExportError(f"{label} must use whole-second UTC format YYYY-MM-DDTHH:MM:SSZ.")
    datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    return value


def must_false(source: Mapping[str, Any], key: str) -> None:
    if source.get(key, False) is not False:
        raise ExportError(f"{key} must be false; identifier export cannot escalate authority.")


def no_private(value: Any, label: str = "export") -> None:
    if isinstance(value, str):
        if PRIVATE_PATH_RE.search(value):
            raise ExportError(f"{label} contains an absolute or private path: {value}")
    elif isinstance(value, list):
        for index, item in enumerate(value):
            no_private(item, f"{label}[{index}]")
    elif isinstance(value, dict):
        for key, item in value.items():
            no_private(item, f"{label}.{key}")


def locator(value: str, label: str) -> str:
    if not LOCATOR_RE.match(value):
        raise ExportError(f"{label} must be a JSON path or sanitized token locator: {value}")
    return value


def resolve(value: str, base: Path) -> Path:
    path = Path(value).expanduser()
    return (base / path if not path.is_absolute() else path).resolve(strict=False)


def inside(path: Path, root: Path) -> bool:
    try:
        path.resolve(strict=False).relative_to(root.resolve(strict=False))
        return True
    except ValueError:
        return False


def active_profile(workspace_path: Path) -> dict[str, Any]:
    workspace = obj(read_json(workspace_path), "workspace")
    if workspace.get("SchemaVersion") != 1:
        raise ExportError("Workspace must use SchemaVersion 1.")
    root = resolve(text(workspace, "RootPath"), workspace_path.parent)
    active = stable(text(workspace, "ActiveGameProfileId", limit=256), "ActiveGameProfileId")
    profiles = workspace.get("GameProfiles")
    if not isinstance(profiles, list):
        raise ExportError("Workspace GameProfiles must be an array.")
    matches = [entry for entry in profiles if isinstance(entry, dict) and entry.get("ProfileId") == active]
    if len(matches) != 1:
        raise ExportError("Workspace ActiveGameProfileId must bind to exactly one profile.")
    profile = matches[0]
    extracted_raw = text(profile, "ExtractedDataPath", empty=True)
    extracted = resolve(extracted_raw, workspace_path.parent) if extracted_raw else None
    if extracted and not inside(extracted, root):
        raise ExportError("ExtractedDataPath must remain inside the workspace root.")
    runtime = text(profile, "RuntimeTarget", limit=32)
    if runtime not in {"Mono", "IL2CPP"}:
        raise ExportError("RuntimeTarget must be Mono or IL2CPP.")
    return {
        "ProfileId": stable(text(profile, "ProfileId", limit=256), "ProfileId"),
        "GameVersion": text(profile, "GameVersion", limit=128),
        "Branch": text(profile, "Branch", limit=128),
        "RuntimeTarget": runtime,
        "ExtractedDataPath": extracted,
    }


def normalize_observation(raw: Any, index: int) -> dict[str, Any]:
    entry = dict(obj(raw, f"Observations[{index}]"))
    must_false(entry, "PromoteAutomatically")
    must_false(entry, "GrantsRuntimePermission")
    observation_id = stable(text(entry, "ObservationId", limit=192), "ObservationId")
    subject = text(entry, "SubjectRef", limit=768)
    if not SUBJECT_RE.match(subject):
        raise ExportError(f"SubjectRef must be explicit: {subject}")
    claim_id = stable(text(entry, "ClaimId", limit=192), "ClaimId")
    if claim_id not in CLAIMS:
        raise ExportError(f"Observation {observation_id} has unsupported ClaimId {claim_id}.")
    value = text(entry, "Value", empty=True, limit=8192) if "Value" in entry else ""
    if claim_id in VALUE_REQUIRED and not value:
        raise ExportError(f"Observation {observation_id} requires Value for {claim_id}.")
    evidence = text(entry, "EvidenceKind", limit=128)
    confidence = text(entry, "Confidence", limit=64)
    if evidence not in EVIDENCE_KINDS or confidence not in CONFIDENCE:
        raise ExportError(f"Observation {observation_id} has unsupported evidence or confidence.")
    domain = text(entry, "Domain", empty=True, limit=64) if "Domain" in entry else ""
    kind = text(entry, "RecordKind", empty=True, limit=64) if "RecordKind" in entry else ""
    if bool(domain) != bool(kind) or (domain and domain not in DOMAINS):
        raise ExportError(f"Observation {observation_id} has invalid domain/record kind.")
    identity = text(entry, "IdentityKind", empty=True, limit=64) if "IdentityKind" in entry else ""
    if identity and identity not in IDENTITY_KINDS:
        raise ExportError(f"Observation {observation_id} has unsupported IdentityKind {identity}.")
    owner = text(entry, "OwnerPackId", empty=True, limit=192) if "OwnerPackId" in entry else ""
    if owner:
        stable(owner, "OwnerPackId")
    native = text(entry, "NativeRefExact", empty=True, limit=512) if "NativeRefExact" in entry else ""
    if claim_id in {"native_ref_exact", "unity_guid"}:
        native = native or value
        if not GUID_RE.match(native):
            raise ExportError(f"Observation {observation_id} has malformed native GUID.")
    if identity == "synthetic" and (native or not owner):
        raise ExportError(f"Observation {observation_id} synthetic records require owner pack and no NativeRefExact.")
    if identity == "native" and owner:
        raise ExportError(f"Observation {observation_id} native records cannot claim OwnerPackId.")
    result = {
        "ObservationId": observation_id,
        "SubjectRef": subject,
        "ClaimId": claim_id,
        "Claim": text(entry, "Claim", limit=8192),
        "Value": value,
        "EvidenceKind": evidence,
        "Confidence": confidence,
        "Locator": locator(text(entry, "Locator", limit=2048), "Locator"),
        "RecordPath": locator(text(entry, "RecordPath", limit=2048), "RecordPath"),
        "PromoteAutomatically": False,
        "GrantsRuntimePermission": False,
    }
    for key, val in (("Domain", domain), ("RecordKind", kind), ("IdentityKind", identity), ("OwnerPackId", owner), ("NativeRefExact", native), ("DisplayName", text(entry, "DisplayName", empty=True, limit=512) if "DisplayName" in entry else "")):
        if val:
            result[key] = val
    no_private(result, observation_id)
    return result


def normalize(document: Mapping[str, Any], profile: Mapping[str, Any] | None = None) -> dict[str, Any]:
    if document.get("SchemaVersion") != 1 or document.get("DocumentKind", DOCUMENT_KIND) != DOCUMENT_KIND:
        raise ExportError("Identifier export must use SchemaVersion 1 and DocumentKind foa-identifier-export.")
    for key in ("PromoteAutomatically", "GrantsRuntimePermission"):
        must_false(document, key)
    header = {
        "ProfileId": stable(text(document, "ProfileId", limit=256), "ProfileId"),
        "GameVersion": text(document, "GameVersion", limit=128),
        "Branch": text(document, "Branch", limit=128),
        "RuntimeTarget": text(document, "RuntimeTarget", limit=32),
    }
    if header["RuntimeTarget"] not in {"Mono", "IL2CPP"}:
        raise ExportError("RuntimeTarget must be Mono or IL2CPP.")
    if profile and any(header[key] != profile[key] for key in header):
        raise ExportError("Identifier export must match the exact active workspace profile.")
    observations_raw = document.get("Observations")
    if not isinstance(observations_raw, list) or not observations_raw or len(observations_raw) > MAX_OBSERVATIONS:
        raise ExportError("Identifier export Observations must be non-empty and bounded.")
    observations = [normalize_observation(item, index) for index, item in enumerate(observations_raw)]
    ids = [item["ObservationId"] for item in observations]
    duplicates = sorted({item for item in ids if ids.count(item) > 1})
    if duplicates:
        raise ExportError("Duplicate observation IDs are refused: " + ", ".join(duplicates))
    native_refs = [item.get("NativeRefExact", "").lower() for item in observations if item.get("NativeRefExact")]
    native_duplicates = sorted({item for item in native_refs if native_refs.count(item) > 1})
    if native_duplicates:
        raise ExportError("Duplicate NativeRefExact values are refused: " + ", ".join(native_duplicates))
    output = {
        "SchemaVersion": 1,
        "DocumentKind": DOCUMENT_KIND,
        "ExportId": stable(text(document, "ExportId", limit=256), "ExportId"),
        **header,
        "ToolName": text(document, "ToolName", limit=256),
        "ToolVersion": text(document, "ToolVersion", limit=128),
        "CapturedAt": utc(text(document, "CapturedAt", limit=32), "CapturedAt"),
        "PromoteAutomatically": False,
        "GrantsRuntimePermission": False,
        "Observations": sorted(observations, key=lambda item: item["ObservationId"]),
    }
    no_private(output)
    return output


def load_export(input_path: Path, workspace_path: Path | None = None) -> dict[str, Any]:
    profile = active_profile(workspace_path) if workspace_path else None
    if profile:
        root = profile["ExtractedDataPath"]
        if root is None or not inside(input_path.resolve(strict=False), root):
            raise ExportError("Identifier export file must remain inside ExtractedDataPath.")
    return normalize(obj(read_json(input_path), "identifier export"), profile)


def write_document(document: Mapping[str, Any], output: Path, replace: bool) -> None:
    if output.exists() and not replace:
        raise ExportError(f"Output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    tmp = output.with_suffix(output.suffix + ".tmp")
    tmp.write_bytes(pretty(document))
    os.replace(tmp, output)


def fixture(output: Path, replace: bool = False) -> dict[str, Any]:
    if output.exists():
        if replace:
            shutil.rmtree(output) if output.is_dir() else output.unlink()
        elif any(output.iterdir() if output.is_dir() else [output]):
            raise ExportError(f"Fixture output is not empty: {output}")
    extracted = output / "workspace" / "Extracted"
    extracted.mkdir(parents=True, exist_ok=True)
    workspace = {"SchemaVersion": 1, "WorkspaceId": "fixture.workspace", "DisplayName": "Fixture Workspace", "RootPath": "./workspace", "OutputPath": "./workspace/Build", "StagingPath": "./workspace/Staging", "DeploymentPath": "./workspace/Deployment", "ActiveGameProfileId": "foa.mono.fixture", "GameProfiles": [{"ProfileId": "foa.mono.fixture", "DisplayName": "FoA Mono Fixture", "InstallPath": "./lawful-local-fixture/FoA", "GameVersion": "1.23.401", "Branch": "mono", "RuntimeTarget": "Mono", "UnityVersion": "6000.0.64f1", "BepInExVersion": "5.4.23.3", "ManagedAssembliesPath": "./lawful-local-fixture/FoA/Tainted Grail_Data/Managed", "PluginPath": "./lawful-local-fixture/FoA/BepInEx/plugins", "DiagnosticsPath": "./workspace/Diagnostics", "ExtractedDataPath": "./workspace/Extracted", "DlcScopes": ["base-game"]}]}
    document = {"SchemaVersion": 1, "DocumentKind": DOCUMENT_KIND, "ExportId": "export.foa.fixture.identifiers", "ProfileId": "foa.mono.fixture", "GameVersion": "1.23.401", "Branch": "mono", "RuntimeTarget": "Mono", "ToolName": "FoA Synthetic Identifier Exporter", "ToolVersion": "1.0.0", "CapturedAt": "2026-07-28T00:00:00Z", "PromoteAutomatically": False, "GrantsRuntimePermission": False, "Observations": [{"ObservationId": "observation.fixture.item.native-ref", "SubjectRef": "subject:foa:economy:item:iron-ore", "ClaimId": "native_ref_exact", "Claim": "Native item identifier was observed in a synthetic identifier export.", "Value": "00000000-0000-0000-0000-000000000001", "Domain": "economy", "RecordKind": "item", "IdentityKind": "native", "NativeRefExact": "00000000-0000-0000-0000-000000000001", "DisplayName": "Synthetic Iron Ore", "EvidenceKind": "native-identifier-observation", "Confidence": "observed", "Locator": "$.items[0].guid", "RecordPath": "$.Observations[0]", "PromoteAutomatically": False, "GrantsRuntimePermission": False}, {"ObservationId": "observation.fixture.template.addressable", "SubjectRef": "subject:foa:population:template:bandit", "ClaimId": "addressable_key", "Claim": "Population template addressable key was observed in a synthetic identifier export.", "Value": "Characters/Templates/Bandit", "Domain": "population", "RecordKind": "template", "IdentityKind": "source_scoped", "DisplayName": "Synthetic Bandit Template", "EvidenceKind": "addressable-observation", "Confidence": "observed", "Locator": "$.templates[0].addressableKey", "RecordPath": "$.Observations[1]", "PromoteAutomatically": False, "GrantsRuntimePermission": False}, {"ObservationId": "observation.fixture.managed.type", "SubjectRef": "subject:foa:runtime:managed-type:inventory-service", "ClaimId": "managed_type_name", "Claim": "Managed type name was observed in a synthetic identifier export without loading an assembly.", "Value": "Game.Inventory.InventoryService", "EvidenceKind": "managed-type-observation", "Confidence": "observed", "Locator": "$.managedTypes[0].name", "RecordPath": "$.Observations[2]", "PromoteAutomatically": False, "GrantsRuntimePermission": False}]}
    workspace_path = output / "workspace.tgworkspace.json"
    export_path = extracted / DEFAULT_EXPORT_NAME
    workspace_path.write_bytes(pretty(workspace))
    normalized = normalize(document, active_profile(workspace_path))
    export_path.write_bytes(pretty(normalized))
    return {"SchemaVersion": 1, "ManifestKind": "foa-identifier-export-contract", "ToolId": TOOL_ID, "ToolVersion": TOOL_VERSION, "ExportId": normalized["ExportId"], "Sha256": digest(pretty(normalized)), "ObservationCount": len(normalized["Observations"]), "OperationalAuthority": {"RecursiveScanAllowed": False, "AssemblyLoadAllowed": False, "RuntimeInvocationAllowed": False, "GameMutationAllowed": False, "SaveAccessAllowed": False, "CatalogPromotionAllowed": False, "RuntimePermissionGranted": False}}


def load_and_validate_export(input_path: Path, workspace_path: Path | None = None) -> dict[str, Any]:
    return load_export(input_path, workspace_path)


def generate_fixture(output: Path, *, replace: bool = False) -> dict[str, Any]:
    return fixture(output, replace=replace)


def parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate and normalize FoA identifier exports.")
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("verify", "normalize"):
        cmd = sub.add_parser(name)
        cmd.add_argument("--input", required=True, type=Path)
        cmd.add_argument("--workspace", type=Path)
        if name == "normalize":
            cmd.add_argument("--output", required=True, type=Path)
            cmd.add_argument("--replace", action="store_true")
    fix = sub.add_parser("fixture")
    fix.add_argument("--output", required=True, type=Path)
    fix.add_argument("--replace", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.command == "fixture":
            manifest = fixture(args.output, replace=args.replace)
            print(f"FoA identifier export fixture wrote {manifest['ObservationCount']} observations.")
        else:
            document = load_export(args.input, args.workspace)
            if args.command == "normalize":
                write_document(document, args.output, args.replace)
                print(f"FoA identifier export normalized: {document['ExportId']} -> {args.output}")
            else:
                print(f"FoA identifier export verified: {document['ExportId']} with {len(document['Observations'])} observations.")
    except ExportError as exc:
        print(f"FoA identifier export failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
