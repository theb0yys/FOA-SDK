#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Stage economy item/recipe catalog-candidate promotions for human review.

This tool consumes the general catalog-promotion candidate document produced by
`foa_game_data_intake.py` and emits a domain-specific review document for
economy records. It prepares item, recipe, and station promotion drafts, but it
does not mutate the live catalog, validate gameplay behavior, grant runtime
permission, or execute adapter work.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

TOOL_ID = "foa.economy-candidate-promotion"
TOOL_VERSION = "0.1.0"
DOCUMENT_KIND = "foa-economy-candidate-promotion"
INPUT_CANDIDATE_KIND = "foa-catalog-promotion-candidates"
MAX_JSON_BYTES = 32 * 1024 * 1024
MAX_RECORDS = 100_000
MAX_ISSUES = 100_000

IDENTIFIER_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
SUBJECT_RE = re.compile(r"^subject:[A-Za-z0-9][A-Za-z0-9:._/-]{1,511}$")
SHA256_RE = re.compile(r"^sha256:[0-9a-f]{64}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
PRIVATE_PATH_RE = re.compile(r"(^/|^~/|^[A-Za-z]:[\\/]|^\\\\)")
ECONOMY_RECORD_KINDS = {"item", "recipe", "station", "crafting_station", "interaction_target"}
STATION_RECORD_KINDS = {"station", "crafting_station", "interaction_target"}
IDENTITY_KINDS = {"native", "synthetic", "composite", "source_scoped"}
CONFIDENCE = {"unrated", "observed", "documented", "inferred"}
RESERVED_FORBIDDEN_USAGE = "no_unvalidated_runtime_use"


class PromotionError(RuntimeError):
    """Raised when economy promotion staging cannot continue."""


def pretty_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def sha256_bytes(payload: bytes) -> str:
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def read_json(path: Path, *, maximum_bytes: int = MAX_JSON_BYTES) -> Any:
    try:
        size = path.stat().st_size
    except OSError as exc:
        raise PromotionError(f"Unable to read JSON file {path}: {exc}") from exc
    if size > maximum_bytes:
        raise PromotionError(f"JSON file exceeds {maximum_bytes} bytes: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8", errors="strict"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise PromotionError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc


def require_mapping(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise PromotionError(f"{label} must be a JSON object.")
    return value


def require_string(source: Mapping[str, Any], key: str, *, allow_empty: bool = False, maximum: int = 4096) -> str:
    value = source.get(key)
    if not isinstance(value, str):
        raise PromotionError(f"{key} is required and must be a string.")
    if (not allow_empty and not value) or len(value) > maximum:
        raise PromotionError(f"{key} is empty or exceeds {maximum} characters.")
    if any(ord(ch) < 0x20 or ord(ch) == 0x7F for ch in value):
        raise PromotionError(f"{key} contains a control character.")
    return value


def require_identifier(value: str, label: str) -> str:
    if not IDENTIFIER_RE.match(value):
        raise PromotionError(f"{label} must be a lowercase stable identifier: {value}")
    return value


def require_subject(value: str, label: str) -> str:
    if not SUBJECT_RE.match(value):
        raise PromotionError(f"{label} must be an explicit subject reference: {value}")
    return value


def require_utc(value: str, label: str) -> str:
    if not UTC_RE.match(value):
        raise PromotionError(f"{label} must use whole-second UTC format YYYY-MM-DDTHH:MM:SSZ.")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise PromotionError(f"{label} is not a valid UTC timestamp: {value}") from exc
    return value


def require_false(source: Mapping[str, Any], key: str) -> None:
    if source.get(key, False) is not False:
        raise PromotionError(f"{key} must be false; economy promotion staging cannot escalate authority.")


def no_private_paths(value: Any, label: str = "promotion") -> None:
    if isinstance(value, str):
        if PRIVATE_PATH_RE.search(value):
            raise PromotionError(f"{label} contains an absolute or private path: {value}")
        return
    if isinstance(value, list):
        for index, child in enumerate(value):
            no_private_paths(child, f"{label}[{index}]")
        return
    if isinstance(value, dict):
        for key, child in value.items():
            no_private_paths(child, f"{label}.{key}")


def resolve_path(value: str, base: Path) -> Path:
    path = Path(value).expanduser()
    return (base / path if not path.is_absolute() else path).resolve(strict=False)


def active_profile(workspace_path: Path) -> dict[str, str]:
    workspace = require_mapping(read_json(workspace_path), "workspace")
    if workspace.get("SchemaVersion") != 1:
        raise PromotionError("Workspace must use SchemaVersion 1.")
    active = require_identifier(require_string(workspace, "ActiveGameProfileId", maximum=256), "ActiveGameProfileId")
    profiles = workspace.get("GameProfiles")
    if not isinstance(profiles, list):
        raise PromotionError("Workspace GameProfiles must be an array.")
    matches = [profile for profile in profiles if isinstance(profile, dict) and profile.get("ProfileId") == active]
    if len(matches) != 1:
        raise PromotionError("Workspace ActiveGameProfileId must bind to exactly one profile.")
    profile = matches[0]
    runtime = require_string(profile, "RuntimeTarget", maximum=32)
    if runtime not in {"Mono", "IL2CPP"}:
        raise PromotionError("RuntimeTarget must be Mono or IL2CPP.")
    return {
        "ProfileId": require_identifier(require_string(profile, "ProfileId", maximum=256), "ProfileId"),
        "GameVersion": require_string(profile, "GameVersion", maximum=128),
        "Branch": require_string(profile, "Branch", maximum=128),
        "RuntimeTarget": runtime,
    }


def validate_binding(header: Mapping[str, Any], profile: Mapping[str, str], *, label: str, runtime: bool = False) -> None:
    if (
        require_string(header, "ProfileId", maximum=256) != profile["ProfileId"]
        or require_string(header, "GameVersion", maximum=128) != profile["GameVersion"]
        or require_string(header, "Branch", maximum=128) != profile["Branch"]
    ):
        raise PromotionError(f"{label} must match the exact active workspace profile.")
    if runtime and require_string(header, "RuntimeTarget", maximum=32) != profile["RuntimeTarget"]:
        raise PromotionError(f"{label} must match the exact active workspace runtime target.")


def issue(issue_id: str, code: str, message: str, *, severity: str = "error", record_id: str = "", locator: str = "", record_path: str = "") -> dict[str, Any]:
    return {
        "IssueId": issue_id,
        "Severity": severity,
        "Code": code,
        "Message": message,
        "RecordId": record_id,
        "Locator": locator,
        "RecordPath": record_path,
        "Line": 0,
    }


def load_inputs(candidate_path: Path, evidence_path: Path, workspace_path: Path) -> tuple[dict[str, Any], dict[str, Any], dict[str, str]]:
    profile = active_profile(workspace_path)
    candidates = dict(require_mapping(read_json(candidate_path), "candidate document"))
    evidence = dict(require_mapping(read_json(evidence_path), "evidence document"))

    if candidates.get("SchemaVersion") != 1 or candidates.get("DocumentKind") != INPUT_CANDIDATE_KIND:
        raise PromotionError("Candidate document must be a schema-1 foa-catalog-promotion-candidates document.")
    if evidence.get("SchemaVersion") != 1:
        raise PromotionError("Evidence document must use SchemaVersion 1.")

    validate_binding(candidates, profile, label="Candidate document", runtime=True)
    validate_binding(evidence, profile, label="Evidence document", runtime=False)

    if candidates.get("SourceId") != evidence.get("SourceId") or candidates.get("SourceFingerprint") != evidence.get("SourceFingerprint"):
        raise PromotionError("Candidate and evidence documents must bind to the same source ID and fingerprint.")

    require_false(candidates, "PromotionAllowed")
    require_false(candidates, "RuntimePermissionGranted")

    records = candidates.get("Records")
    if not isinstance(records, list) or len(records) > MAX_RECORDS:
        raise PromotionError("Candidate Records must be a bounded array.")
    if not records:
        raise PromotionError("Candidate Records must not be empty.")

    evidence_entries = evidence.get("Evidence")
    if not isinstance(evidence_entries, list):
        raise PromotionError("Evidence document Evidence must be an array.")

    return candidates, evidence, profile


def evidence_id_set(evidence: Mapping[str, Any]) -> set[str]:
    values: set[str] = set()
    for entry in evidence.get("Evidence", []):
        if isinstance(entry, dict) and isinstance(entry.get("EvidenceId"), str):
            values.add(entry["EvidenceId"])
    return values


def validate_candidate_record(record: Mapping[str, Any], evidence_ids: set[str], index: int) -> tuple[dict[str, Any] | None, list[dict[str, Any]]]:
    problems: list[dict[str, Any]] = []

    record_id = require_identifier(require_string(record, "RecordId", maximum=192), "RecordId")
    domain = require_string(record, "Domain", maximum=64)
    kind = require_string(record, "RecordKind", maximum=64)
    subject = require_subject(require_string(record, "SubjectRef", maximum=768), "SubjectRef")
    identity = require_string(record, "IdentityKind", maximum=64)
    if identity not in IDENTITY_KINDS:
        problems.append(issue(f"issue.{record_id}.identity-kind", "economy.identity-kind-unsupported", f"Unsupported identity kind: {identity}", record_id=record_id))
    confidence = require_string(record, "Confidence", maximum=64)
    if confidence not in CONFIDENCE:
        problems.append(issue(f"issue.{record_id}.confidence", "economy.confidence-unsupported", f"Unsupported confidence: {confidence}", record_id=record_id))

    evidence = record.get("EvidenceIds")
    if not isinstance(evidence, list) or not evidence or any(not isinstance(value, str) or value not in evidence_ids for value in evidence):
        problems.append(issue(f"issue.{record_id}.missing-evidence", "economy.missing-evidence", "Candidate record must reference evidence present in the bound evidence document.", record_id=record_id))

    if domain != "economy":
        problems.append(issue(f"issue.{record_id}.domain", "economy.non-economy-record", f"Record domain is not economy: {domain}", record_id=record_id))
    if kind not in ECONOMY_RECORD_KINDS:
        problems.append(issue(f"issue.{record_id}.kind", "economy.unsupported-record-kind", f"Unsupported economy record kind: {kind}", record_id=record_id))

    owner = require_string(record, "OwnerPackId", allow_empty=True, maximum=192) if "OwnerPackId" in record else ""
    native = require_string(record, "NativeRefExact", allow_empty=True, maximum=512) if "NativeRefExact" in record else ""
    if identity == "native" and (owner or not native):
        problems.append(issue(f"issue.{record_id}.native-owner", "economy.native-identity-invalid", "Native economy records require NativeRefExact and cannot claim OwnerPackId.", record_id=record_id))
    if identity == "synthetic" and (native or not owner):
        problems.append(issue(f"issue.{record_id}.synthetic-owner", "economy.synthetic-identity-invalid", "Synthetic economy records require OwnerPackId and cannot borrow NativeRefExact.", record_id=record_id))

    normalized = {
        "RecordId": record_id,
        "OwnerPackId": owner,
        "Domain": domain,
        "RecordKind": kind,
        "SubjectRef": subject,
        "NativeRefExact": native,
        "IdentityKind": identity,
        "DisplayName": require_string(record, "DisplayName", allow_empty=True, maximum=512) if "DisplayName" in record else "",
        "Aliases": [value for value in record.get("Aliases", []) if isinstance(value, str)],
        "SourceScopedRefs": [value for value in record.get("SourceScopedRefs", []) if isinstance(value, str)],
        "ResearchStage": require_string(record, "ResearchStage", allow_empty=True, maximum=32) if "ResearchStage" in record else "S2",
        "Confidence": confidence,
        "OperationalRisk": "unknown",
        "ValidationState": "unvalidated",
        "StalenessState": "unknown",
        "AllowedUsages": [],
        "ForbiddenUsages": [RESERVED_FORBIDDEN_USAGE],
        "EvidenceIds": list(evidence) if isinstance(evidence, list) else [],
        "MissingRefs": [],
        "ConflictRefs": [],
        "Tags": sorted({*(value for value in record.get("Tags", []) if isinstance(value, str)), "economy-promotion-candidate"}),
        "CreatedAt": require_string(record, "CreatedAt", allow_empty=True, maximum=32) if "CreatedAt" in record else "",
        "UpdatedAt": require_string(record, "UpdatedAt", allow_empty=True, maximum=32) if "UpdatedAt" in record else "",
        "SupersededByRecordId": "",
        "CandidateIndex": index,
    }
    return normalized, problems


def item_profile(record: Mapping[str, Any]) -> dict[str, Any]:
    return {
        "RecordId": record["RecordId"],
        "Category": "unknown",
        "Subtype": "",
        "StackLimit": 0,
        "Weight": 0.0,
        "BaseValue": 0.0,
        "Rarity": "unknown",
        "Quality": "unknown",
        "Durability": 0.0,
        "QuestItem": False,
        "UniqueItem": False,
        "HiddenItem": False,
        "LocalisationNameRef": "",
        "LocalisationDescriptionRef": "",
        "IconRef": "",
        "AssetRef": "",
        "Tags": sorted(set(record.get("Tags", []))),
        "EvidenceIds": record["EvidenceIds"],
        "ProfileState": "draft",
        "CompletionBlockers": ["economy.item-profile-review-required"],
    }


def recipe_profile(record: Mapping[str, Any]) -> dict[str, Any]:
    identity = record.get("IdentityKind", "")
    persistence = "native_template" if identity == "native" else "custom_template" if identity == "synthetic" else "unknown"
    return {
        "RecordId": record["RecordId"],
        "RecipeType": "unknown",
        "RecipeTab": "",
        "StationRecordIds": [],
        "UnlockMode": "unknown",
        "UnlockSubjectRefs": [],
        "DuplicateKey": record["RecordId"],
        "PersistenceMode": persistence,
        "HiddenRecipe": False,
        "EvidenceIds": record["EvidenceIds"],
        "ProfileState": "incomplete",
        "CompletionBlockers": [
            "economy.recipe-type-review-required",
            "economy.recipe-station-required",
            "economy.recipe-output-required",
        ],
    }


def station_profile(record: Mapping[str, Any]) -> dict[str, Any]:
    return {
        "RecordId": record["RecordId"],
        "StationKind": record["RecordKind"],
        "DisplayName": record["DisplayName"],
        "EvidenceIds": record["EvidenceIds"],
        "ProfileState": "review_required",
    }


def build_promotion_document(
    *,
    candidates: Mapping[str, Any],
    evidence: Mapping[str, Any],
    profile: Mapping[str, str],
    staged_at: str,
    reviewer: str,
) -> dict[str, Any]:
    staged_at = require_utc(staged_at, "StagedAt")
    source_id = require_identifier(require_string(candidates, "SourceId", maximum=256), "SourceId")
    source_fingerprint = require_string(candidates, "SourceFingerprint", maximum=80)
    if not SHA256_RE.match(source_fingerprint):
        raise PromotionError("SourceFingerprint must be lowercase sha256:<64-hex>.")

    known_evidence = evidence_id_set(evidence)
    issues: list[dict[str, Any]] = []
    for existing in candidates.get("Issues", []):
        if isinstance(existing, dict):
            issues.append({
                "IssueId": require_string(existing, "IssueId", allow_empty=True, maximum=256) if "IssueId" in existing else f"issue.input.{len(issues)}",
                "Severity": require_string(existing, "Severity", allow_empty=True, maximum=32) if "Severity" in existing else "error",
                "Code": require_string(existing, "Code", allow_empty=True, maximum=128) if "Code" in existing else "input.candidate-issue",
                "Message": require_string(existing, "Message", allow_empty=True, maximum=8192) if "Message" in existing else "Input candidate issue.",
                "RecordId": require_string(existing, "RecordId", allow_empty=True, maximum=192) if "RecordId" in existing else "",
                "Locator": require_string(existing, "Locator", allow_empty=True, maximum=2048) if "Locator" in existing else "",
                "RecordPath": require_string(existing, "RecordPath", allow_empty=True, maximum=2048) if "RecordPath" in existing else "",
                "Line": int(existing.get("Line", 0)) if isinstance(existing.get("Line", 0), int) else 0,
            })

    records: list[dict[str, Any]] = []
    rejected_records: list[dict[str, Any]] = []
    for index, raw_record in enumerate(candidates.get("Records", [])):
        record, record_issues = validate_candidate_record(require_mapping(raw_record, f"Records[{index}]"), known_evidence, index)
        issues.extend(record_issues)
        if record is None:
            continue
        if any(item.get("RecordId") == record["RecordId"] and item.get("Severity") == "error" for item in record_issues):
            rejected_records.append({"RecordId": record["RecordId"], "Reason": "record-specific-errors"})
        records.append(record)

    record_ids = [record["RecordId"] for record in records]
    for duplicate in sorted({value for value in record_ids if record_ids.count(value) > 1}):
        issues.append(issue(f"issue.{duplicate}.duplicate-record-id", "economy.duplicate-record-id", f"Duplicate candidate RecordId {duplicate}.", record_id=duplicate))
    native_refs = [record["NativeRefExact"].lower() for record in records if record["NativeRefExact"]]
    for duplicate in sorted({value for value in native_refs if native_refs.count(value) > 1}):
        issues.append(issue(f"issue.native-ref.{hashlib.sha256(duplicate.encode()).hexdigest()[:12]}", "economy.duplicate-native-ref", f"Duplicate NativeRefExact {duplicate}.", record_id=""))

    record_error_ids = {item["RecordId"] for item in issues if item.get("Severity") == "error" and item.get("RecordId")}

    record_promotions: list[dict[str, Any]] = []
    item_profiles: list[dict[str, Any]] = []
    recipe_profiles: list[dict[str, Any]] = []
    station_profiles: list[dict[str, Any]] = []

    for record in sorted(records, key=lambda item: item["RecordId"]):
        blocked = record["RecordId"] in record_error_ids
        if record["Domain"] == "economy" and record["RecordKind"] == "item" and not blocked:
            item_profiles.append(item_profile(record))
        elif record["Domain"] == "economy" and record["RecordKind"] == "recipe" and not blocked:
            profile_draft = recipe_profile(record)
            recipe_profiles.append(profile_draft)
            issues.append(issue(
                f"issue.{record['RecordId']}.recipe-incomplete",
                "economy.recipe-profile-incomplete",
                "Recipe candidates require reviewed station and output joins before runtime recipe lanes can be considered.",
                severity="warning",
                record_id=record["RecordId"],
            ))
        elif record["Domain"] == "economy" and record["RecordKind"] in STATION_RECORD_KINDS and not blocked:
            station_profiles.append(station_profile(record))

        record_promotions.append({
            "RecordId": record["RecordId"],
            "SubjectRef": record["SubjectRef"],
            "RecordKind": record["RecordKind"],
            "IdentityKind": record["IdentityKind"],
            "EvidenceIds": record["EvidenceIds"],
            "ReviewState": "blocked" if blocked else "review_required",
            "PromotionRecommended": False if blocked else True,
            "CatalogMutationAllowed": False,
            "RuntimePermissionGranted": False,
            "Blockers": sorted({item["Code"] for item in issues if item.get("RecordId") == record["RecordId"] and item.get("Severity") == "error"}),
        })

    output = {
        "SchemaVersion": 1,
        "DocumentKind": DOCUMENT_KIND,
        "ToolId": TOOL_ID,
        "ToolVersion": TOOL_VERSION,
        "SourceId": source_id,
        "SourceFingerprint": source_fingerprint,
        "ProfileId": profile["ProfileId"],
        "GameVersion": profile["GameVersion"],
        "Branch": profile["Branch"],
        "RuntimeTarget": profile["RuntimeTarget"],
        "StagedAt": staged_at,
        "Reviewer": reviewer,
        "PromotionAllowed": False,
        "CatalogMutationAllowed": False,
        "RuntimePermissionGranted": False,
        "AdapterExecutionAllowed": False,
        "RecordPromotions": sorted(record_promotions, key=lambda item: item["RecordId"]),
        "EconomyItemProfiles": sorted(item_profiles, key=lambda item: item["RecordId"]),
        "EconomyRecipeProfiles": sorted(recipe_profiles, key=lambda item: item["RecordId"]),
        "EconomyStationProfiles": sorted(station_profiles, key=lambda item: item["RecordId"]),
        "RejectedRecords": sorted(rejected_records, key=lambda item: item["RecordId"]),
        "Issues": sorted(issues, key=lambda item: (item["Severity"], item["Code"], item["RecordId"], item["IssueId"])),
    }
    no_private_paths(output)
    return output


def stage(
    *,
    workspace_path: Path,
    candidates_path: Path,
    evidence_path: Path,
    staged_at: str | None = None,
    reviewer: str = "reviewer.local",
) -> dict[str, Any]:
    candidates, evidence, profile = load_inputs(candidates_path, evidence_path, workspace_path)
    return build_promotion_document(
        candidates=candidates,
        evidence=evidence,
        profile=profile,
        staged_at=staged_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        reviewer=reviewer,
    )


def verify_document(document: Mapping[str, Any], profile: Mapping[str, str] | None = None) -> dict[str, Any]:
    if document.get("SchemaVersion") != 1 or document.get("DocumentKind") != DOCUMENT_KIND:
        raise PromotionError("Promotion document must be schema-1 foa-economy-candidate-promotion.")
    require_false(document, "PromotionAllowed")
    require_false(document, "CatalogMutationAllowed")
    require_false(document, "RuntimePermissionGranted")
    require_false(document, "AdapterExecutionAllowed")
    if profile:
        validate_binding(document, profile, label="Promotion document", runtime=True)
    require_utc(require_string(document, "StagedAt", maximum=32), "StagedAt")
    promotions = document.get("RecordPromotions")
    if not isinstance(promotions, list):
        raise PromotionError("RecordPromotions must be an array.")
    if not promotions:
        raise PromotionError("RecordPromotions must not be empty.")
    issues = document.get("Issues")
    if not isinstance(issues, list) or len(issues) > MAX_ISSUES:
        raise PromotionError("Issues must be a bounded array.")
    evidence_bound_ids: set[str] = set()
    for section in ("EconomyItemProfiles", "EconomyRecipeProfiles", "EconomyStationProfiles"):
        values = document.get(section)
        if not isinstance(values, list):
            raise PromotionError(f"{section} must be an array.")
        for entry in values:
            item = require_mapping(entry, section)
            record_id = require_identifier(require_string(item, "RecordId", maximum=192), "RecordId")
            evidence_ids = item.get("EvidenceIds")
            if not isinstance(evidence_ids, list) or not evidence_ids:
                raise PromotionError(f"{section} entry {record_id} must retain evidence IDs.")
            evidence_bound_ids.update(value for value in evidence_ids if isinstance(value, str))
    if not evidence_bound_ids:
        raise PromotionError("Promotion document must retain at least one evidence binding.")
    no_private_paths(document)
    return dict(document)


def write_document(document: Mapping[str, Any], output: Path, *, replace: bool = False) -> None:
    if output.exists() and not replace:
        raise PromotionError(f"Output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_bytes(pretty_json_bytes(document))
    os.replace(temporary, output)


def generate_fixture(output: Path, *, replace: bool = False) -> dict[str, Any]:
    if output.exists():
        if replace:
            shutil.rmtree(output) if output.is_dir() else output.unlink()
        elif any(output.iterdir() if output.is_dir() else [output]):
            raise PromotionError(f"Fixture output is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)

    workspace = {
        "SchemaVersion": 1,
        "WorkspaceId": "fixture.workspace",
        "DisplayName": "Fixture Workspace",
        "RootPath": "./workspace",
        "OutputPath": "./workspace/Build",
        "StagingPath": "./workspace/Staging",
        "DeploymentPath": "./workspace/Deployment",
        "ActiveGameProfileId": "foa.mono.fixture",
        "GameProfiles": [{
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
        }],
    }
    source_fingerprint = "sha256:" + "1" * 64
    source_id = "source.foa.mono.fixture.1111111111111111"
    evidence = {
        "SchemaVersion": 1,
        "SourceId": source_id,
        "SourceFingerprint": source_fingerprint,
        "ProfileId": "foa.mono.fixture",
        "GameVersion": "1.23.401",
        "Branch": "mono",
        "Evidence": [
            {"EvidenceId": "evidence.observation.fixture.item.native-ref", "SourceId": source_id, "SourceFingerprint": source_fingerprint, "ProfileId": "foa.mono.fixture", "GameVersion": "1.23.401", "Branch": "mono", "SubjectRef": "subject:foa:economy:item:iron-ore", "Claim": "native_ref_exact: observed", "EvidenceKind": "native-identifier-observation", "Confidence": "observed", "Locator": "$.items[0].guid", "RecordPath": "$.Observations[0]", "ExtractedAt": "2026-07-28T00:00:00Z"},
            {"EvidenceId": "evidence.observation.fixture.recipe.native-ref", "SourceId": source_id, "SourceFingerprint": source_fingerprint, "ProfileId": "foa.mono.fixture", "GameVersion": "1.23.401", "Branch": "mono", "SubjectRef": "subject:foa:economy:recipe:iron-ingot", "Claim": "native_ref_exact: observed", "EvidenceKind": "native-identifier-observation", "Confidence": "observed", "Locator": "$.recipes[0].guid", "RecordPath": "$.Observations[1]", "ExtractedAt": "2026-07-28T00:00:00Z"},
            {"EvidenceId": "evidence.observation.fixture.station.native-ref", "SourceId": source_id, "SourceFingerprint": source_fingerprint, "ProfileId": "foa.mono.fixture", "GameVersion": "1.23.401", "Branch": "mono", "SubjectRef": "subject:foa:economy:station:forge", "Claim": "native_ref_exact: observed", "EvidenceKind": "native-identifier-observation", "Confidence": "observed", "Locator": "$.stations[0].guid", "RecordPath": "$.Observations[2]", "ExtractedAt": "2026-07-28T00:00:00Z"},
        ],
        "Issues": [],
    }
    candidates = {
        "SchemaVersion": 1,
        "DocumentKind": INPUT_CANDIDATE_KIND,
        "SourceId": source_id,
        "SourceFingerprint": source_fingerprint,
        "ProfileId": "foa.mono.fixture",
        "GameVersion": "1.23.401",
        "Branch": "mono",
        "RuntimeTarget": "Mono",
        "ProviderId": "provider.foa-local-capture",
        "ProviderVersion": "1.0.0",
        "PromotionAllowed": False,
        "RuntimePermissionGranted": False,
        "Records": [
            {"RecordId": "candidate.economy.item.iron.ore", "OwnerPackId": "", "Domain": "economy", "RecordKind": "item", "SubjectRef": "subject:foa:economy:item:iron-ore", "NativeRefExact": "00000000-0000-0000-0000-000000000001", "IdentityKind": "native", "DisplayName": "Synthetic Iron Ore", "Confidence": "observed", "ResearchStage": "S2", "EvidenceIds": ["evidence.observation.fixture.item.native-ref"], "Tags": ["foa-local-capture"], "CreatedAt": "2026-07-28T00:00:00Z", "UpdatedAt": "2026-07-28T00:00:00Z"},
            {"RecordId": "candidate.economy.recipe.iron.ingot", "OwnerPackId": "", "Domain": "economy", "RecordKind": "recipe", "SubjectRef": "subject:foa:economy:recipe:iron-ingot", "NativeRefExact": "00000000-0000-0000-0000-000000000002", "IdentityKind": "native", "DisplayName": "Synthetic Iron Ingot Recipe", "Confidence": "observed", "ResearchStage": "S2", "EvidenceIds": ["evidence.observation.fixture.recipe.native-ref"], "Tags": ["foa-local-capture"], "CreatedAt": "2026-07-28T00:00:00Z", "UpdatedAt": "2026-07-28T00:00:00Z"},
            {"RecordId": "candidate.economy.station.forge", "OwnerPackId": "", "Domain": "economy", "RecordKind": "station", "SubjectRef": "subject:foa:economy:station:forge", "NativeRefExact": "00000000-0000-0000-0000-000000000003", "IdentityKind": "native", "DisplayName": "Synthetic Forge", "Confidence": "observed", "ResearchStage": "S2", "EvidenceIds": ["evidence.observation.fixture.station.native-ref"], "Tags": ["foa-local-capture"], "CreatedAt": "2026-07-28T00:00:00Z", "UpdatedAt": "2026-07-28T00:00:00Z"},
        ],
        "NativeBindings": [],
        "Issues": [],
    }

    workspace_path = output / "workspace.tgworkspace.json"
    evidence_path = output / "evidence.tgevidence.json"
    candidates_path = output / "candidates.tgcatalog-candidates.json"
    promotion_path = output / "economy-promotion.tgeconomy-promotion.json"
    workspace_path.write_bytes(pretty_json_bytes(workspace))
    evidence_path.write_bytes(pretty_json_bytes(evidence))
    candidates_path.write_bytes(pretty_json_bytes(candidates))
    staged = stage(workspace_path=workspace_path, candidates_path=candidates_path, evidence_path=evidence_path, staged_at="2026-07-28T00:00:01Z", reviewer="fixture")
    write_document(staged, promotion_path, replace=True)
    verify_document(staged, active_profile(workspace_path))
    return {
        "SchemaVersion": 1,
        "ManifestKind": "foa-economy-candidate-promotion-fixture",
        "ToolId": TOOL_ID,
        "ToolVersion": TOOL_VERSION,
        "PromotionSha256": sha256_bytes(pretty_json_bytes(staged)),
        "RecordPromotionCount": len(staged["RecordPromotions"]),
        "ItemProfileCount": len(staged["EconomyItemProfiles"]),
        "RecipeProfileCount": len(staged["EconomyRecipeProfiles"]),
        "StationProfileCount": len(staged["EconomyStationProfiles"]),
        "OperationalAuthority": {
            "CatalogMutationAllowed": False,
            "RuntimePermissionGranted": False,
            "AdapterExecutionAllowed": False,
        },
    }


def parse_arguments(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Stage FoA economy catalog candidates for reviewed promotion.")
    sub = parser.add_subparsers(dest="command", required=True)

    stage_parser = sub.add_parser("stage")
    stage_parser.add_argument("--workspace", required=True, type=Path)
    stage_parser.add_argument("--candidates", required=True, type=Path)
    stage_parser.add_argument("--evidence", required=True, type=Path)
    stage_parser.add_argument("--output", required=True, type=Path)
    stage_parser.add_argument("--staged-at")
    stage_parser.add_argument("--reviewer", default="reviewer.local")
    stage_parser.add_argument("--replace", action="store_true")

    verify_parser = sub.add_parser("verify")
    verify_parser.add_argument("--input", required=True, type=Path)
    verify_parser.add_argument("--workspace", type=Path)

    fixture_parser = sub.add_parser("fixture")
    fixture_parser.add_argument("--output", required=True, type=Path)
    fixture_parser.add_argument("--replace", action="store_true")

    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_arguments(argv)
    try:
        if args.command == "fixture":
            manifest = generate_fixture(args.output, replace=args.replace)
            print(f"FoA economy candidate promotion fixture wrote {manifest['RecordPromotionCount']} record promotions.")
        elif args.command == "stage":
            document = stage(
                workspace_path=args.workspace,
                candidates_path=args.candidates,
                evidence_path=args.evidence,
                staged_at=args.staged_at,
                reviewer=args.reviewer,
            )
            write_document(document, args.output, replace=args.replace)
            print(f"FoA economy candidate promotion staged {len(document['RecordPromotions'])} records.")
        elif args.command == "verify":
            profile = active_profile(args.workspace) if args.workspace else None
            document = verify_document(require_mapping(read_json(args.input), "promotion document"), profile)
            print(f"FoA economy candidate promotion verified {len(document['RecordPromotions'])} records.")
        else:
            raise PromotionError(f"Unknown command: {args.command}")
    except PromotionError as exc:
        print(f"FoA economy candidate promotion failed: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
