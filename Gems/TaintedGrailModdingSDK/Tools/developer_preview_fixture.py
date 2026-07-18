#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Generate and verify the project-owned Developer Preview 0 fixture.

The committed template is the single source of truth. Generation copies only canonical
UTF-8 JSON from that template, produces a SHA-256 manifest, and refuses unsafe overwrite.
The fixture contains no game files, native game identities, private paths, credentials,
runtime code, deployment behavior, save data, or telemetry.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path, PurePosixPath
from typing import Any, Iterable

FIXTURE_SCHEMA_VERSION = 1
FIXTURE_ID = "preview.developer-preview-0"
MANIFEST_NAME = "preview-fixture.manifest.json"

WORKSPACE_PATH = "preview.tgworkspace.json"
PACK_PATH = "Packs/preview.developer-preview-0.tgpack.json"
SOURCE_INPUT_PATH = "Input/preview-source.json"
SOURCE_DOCUMENT_PATH = "Sources/preview.source.synthetic/source.tgsource.json"
EVIDENCE_DOCUMENT_PATH = "Sources/preview.source.synthetic/evidence.tgevidence.json"
CATALOG_PATH = "Catalog/catalog.tgcatalog.json"
EXPECTED_FILE_PATHS = (
    WORKSPACE_PATH,
    PACK_PATH,
    SOURCE_INPUT_PATH,
    SOURCE_DOCUMENT_PATH,
    EVIDENCE_DOCUMENT_PATH,
    CATALOG_PATH,
)

WORKSPACE_ID = "preview.workspace"
PROFILE_ID = "preview.profile.windows-x64"
PACK_ID = "preview.developer-preview-0"
GAME_VERSION = "preview-build-0"
BRANCH = "preview"
RUNTIME_TARGET = "Mono"

ITEM_COMPONENT_ID = "preview.item.component"
ITEM_RESULT_ID = "preview.item.result"
RECIPE_ID = "preview.recipe.synthesis"
STATION_ID = "preview.station.workbench"
LEARN_SOURCE_ID = "preview.learn-source.manual"
RELATIONSHIP_CRAFTED_AT = "preview.relationship.recipe.crafted-at"
RELATIONSHIP_LEARNED_FROM = "preview.relationship.recipe.learned-from"
RELATIONSHIP_UNRESOLVED = "preview.relationship.recipe.learned-from-unresolved"
SUBJECT_UNRESOLVED = "subject:preview:learn-source:unresolved"

EXPECTED_STATE = {
    "allowed_record_ids": [ITEM_COMPONENT_ID, RECIPE_ID],
    "blocked_record_ids": [ITEM_RESULT_ID, STATION_ID],
    "evidence_count": 8,
    "forbidden_record_ids": [ITEM_RESULT_ID, STATION_ID],
    "record_count": 5,
    "relationship_count": 3,
    "stale_record_ids": [STATION_ID],
    "unresolved_relationship_ids": [RELATIONSHIP_UNRESOLVED],
}

PRIVATE_PATH_PATTERN = re.compile(
    r"^(?:[A-Za-z]:[\\/]|/|\\\\)|(?:^|[\\/])(?:Users|home)[\\/]",
    re.IGNORECASE,
)
SECRET_PATTERN = re.compile(
    r"(?:-----BEGIN [A-Z ]*PRIVATE KEY-----|gh[pousr]_[A-Za-z0-9]{20,}|sk-[A-Za-z0-9]{20,})"
)


class FixtureError(RuntimeError):
    """Raised when fixture generation or verification fails closed."""


def template_root_from_script() -> Path:
    return Path(__file__).resolve().parents[1] / "Preview" / "Template"


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False) + "\n").encode("utf-8")


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise FixtureError(message)


def require_string(value: Any, field: str) -> str:
    require(isinstance(value, str) and bool(value), f"{field} must be a non-empty string")
    return value


def require_list(value: Any, field: str) -> list[Any]:
    require(isinstance(value, list), f"{field} must be an array")
    return value


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise FixtureError(f"Unable to read valid UTF-8 JSON from {path}: {exc}") from exc
    require(isinstance(value, dict), f"Expected a JSON object in {path}")
    return value


def normalize_relative_path(value: str) -> str:
    require(isinstance(value, str) and bool(value), "Manifest paths must be non-empty strings")
    require("\\" not in value, f"Manifest path must use forward slashes: {value}")
    path = PurePosixPath(value)
    require(not path.is_absolute(), f"Manifest path must be relative: {value}")
    require(".." not in path.parts, f"Manifest path traversal is prohibited: {value}")
    require(path.as_posix() == value, f"Manifest path is not canonical: {value}")
    return value


def ensure_no_symlinks(root: Path) -> None:
    if root.is_symlink():
        raise FixtureError(f"Fixture directory must not be a symbolic link: {root}")
    if not root.exists():
        return
    for path in root.rglob("*"):
        if path.is_symlink():
            raise FixtureError(f"Fixture directory contains a symbolic link: {path}")


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=str(path.parent))
    temporary_path = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, path)
    finally:
        if temporary_path.exists():
            temporary_path.unlink()


def iter_strings(value: Any) -> Iterable[str]:
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for key, child in value.items():
            yield str(key)
            yield from iter_strings(child)
    elif isinstance(value, list):
        for child in value:
            yield from iter_strings(child)


def assert_public_synthetic_content(value: Any, *, label: str) -> None:
    for text in iter_strings(value):
        require(
            not PRIVATE_PATH_PATTERN.search(text),
            f"{label} contains an absolute or private path: {text}",
        )
        require(not SECRET_PATTERN.search(text), f"{label} contains secret-like material")
        require("\x00" not in text, f"{label} contains a NUL character")


def template_documents(template_root: Path | None = None) -> dict[str, dict[str, Any]]:
    root = (template_root or template_root_from_script()).resolve(strict=False)
    require(root.is_dir(), f"Fixture template directory does not exist: {root}")
    ensure_no_symlinks(root)
    actual = sorted(path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file())
    require(actual == sorted(EXPECTED_FILE_PATHS), "Fixture template file set is incomplete or unexpected")
    documents: dict[str, dict[str, Any]] = {}
    for relative_path in sorted(EXPECTED_FILE_PATHS):
        path = root / relative_path
        document = load_json(path)
        require(path.read_bytes() == canonical_json_bytes(document), f"Template JSON is not canonical: {relative_path}")
        assert_public_synthetic_content(document, label=f"template:{relative_path}")
        documents[relative_path] = document
    return documents


def build_manifest(documents: dict[str, dict[str, Any]]) -> dict[str, Any]:
    files = []
    for relative_path in sorted(documents):
        payload = canonical_json_bytes(documents[relative_path])
        files.append(
            {
                "path": relative_path,
                "sha256": sha256_bytes(payload),
                "size_bytes": len(payload),
            }
        )
    return {
        "expected": EXPECTED_STATE,
        "fixture_id": FIXTURE_ID,
        "files": files,
        "generator": "Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py",
        "schema_version": FIXTURE_SCHEMA_VERSION,
    }


def manifest_paths(manifest: dict[str, Any]) -> list[str]:
    require(manifest.get("schema_version") == FIXTURE_SCHEMA_VERSION, "Unsupported fixture manifest schema")
    require(manifest.get("fixture_id") == FIXTURE_ID, "Unexpected fixture ID")
    require(manifest.get("expected") == EXPECTED_STATE, "Fixture expected-state contract mismatch")
    paths = []
    for entry in require_list(manifest.get("files"), "manifest.files"):
        require(isinstance(entry, dict), "Each manifest file entry must be an object")
        paths.append(normalize_relative_path(entry.get("path")))
    require(paths == sorted(paths), "Manifest files must be sorted deterministically")
    require(len(paths) == len(set(paths)), "Manifest paths must be unique")
    require(paths == sorted(EXPECTED_FILE_PATHS), "Manifest file set is incomplete or unexpected")
    return paths


def prepare_output_directory(output_dir: Path, *, replace: bool) -> None:
    require(not output_dir.is_symlink(), f"Fixture output must not be a symbolic link: {output_dir}")
    if output_dir.exists() and not output_dir.is_dir():
        raise FixtureError(f"Fixture output is not a directory: {output_dir}")
    if not output_dir.exists():
        output_dir.mkdir(parents=True)
        return
    ensure_no_symlinks(output_dir)
    if not any(path.is_file() for path in output_dir.rglob("*")):
        return
    if not replace:
        raise FixtureError(
            f"Fixture output is not empty: {output_dir}. "
            "Use --replace only for an existing fixture that passes full verification."
        )
    verify_fixture(output_dir)
    shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)


def generate_fixture(
    output_dir: Path,
    *,
    replace: bool = False,
    template_root: Path | None = None,
) -> dict[str, Any]:
    output_dir = output_dir.expanduser().resolve(strict=False)
    prepare_output_directory(output_dir, replace=replace)
    documents = template_documents(template_root)
    for relative_path in sorted(documents):
        atomic_write(output_dir / relative_path, canonical_json_bytes(documents[relative_path]))
    manifest = build_manifest(documents)
    atomic_write(output_dir / MANIFEST_NAME, canonical_json_bytes(manifest))
    verify_fixture(output_dir, template_root=template_root)
    return manifest


def validate_workspace(workspace: dict[str, Any]) -> dict[str, Any]:
    require(workspace.get("WorkspaceId") == WORKSPACE_ID, "Workspace ID mismatch")
    require(workspace.get("RootPath") == ".", "Preview workspace root must remain relative")
    profiles = require_list(workspace.get("GameProfiles"), "workspace.GameProfiles")
    require(len(profiles) == 1, "Preview workspace must contain exactly one game profile")
    profile = profiles[0]
    require(isinstance(profile, dict), "Workspace profile must be an object")
    require(workspace.get("ActiveGameProfileId") == PROFILE_ID, "Active profile mismatch")
    require(profile.get("ProfileId") == PROFILE_ID, "Profile ID mismatch")
    require(profile.get("GameVersion") == GAME_VERSION, "Profile game version mismatch")
    require(profile.get("Branch") == BRANCH, "Profile branch mismatch")
    require(profile.get("RuntimeTarget") == RUNTIME_TARGET, "Profile runtime target mismatch")
    for field in ("InstallPath", "ManagedAssembliesPath", "PluginPath", "DiagnosticsPath", "ExtractedDataPath"):
        value = require_string(profile.get(field), f"profile.{field}")
        require(not PRIVATE_PATH_PATTERN.search(value), f"profile.{field} must be portable")
    return profile


def validate_pack(pack: dict[str, Any]) -> None:
    require(pack.get("SchemaVersion") == 1, "Unsupported pack schema")
    require(pack.get("PackId") == PACK_ID, "Pack ID mismatch")
    require(pack.get("OwnerId") == "preview", "Pack owner mismatch")
    require(pack.get("TargetGameVersion") == GAME_VERSION, "Pack game version mismatch")
    require(pack.get("TargetBranch") == BRANCH, "Pack branch mismatch")
    require(pack.get("RuntimeActionsEnabled") is False, "Preview pack runtime actions must remain disabled")
    pattern = r"[a-z0-9][a-z0-9._-]*\.[a-z0-9][a-z0-9._-]*"
    require(re.fullmatch(pattern, PACK_ID) is not None, "Invalid pack ID")


def validate_source_and_evidence(
    root: Path,
    source_document: dict[str, Any],
    evidence_document: dict[str, Any],
) -> dict[str, dict[str, Any]]:
    require(source_document.get("SchemaVersion") == 1, "Unsupported source schema")
    require(evidence_document.get("SchemaVersion") == 1, "Unsupported evidence schema")
    source = source_document.get("Source")
    require(isinstance(source, dict), "Source document must contain Source object")
    source_input = (root / SOURCE_INPUT_PATH).read_bytes()
    expected_fingerprint = f"sha256:{sha256_bytes(source_input)}"
    require(source.get("Fingerprint") == expected_fingerprint, "Source fingerprint does not match source input")
    require(source.get("ByteSize") == len(source_input), "Source byte size does not match source input")
    for field in ("SourceId", "ProfileId", "GameVersion", "Branch"):
        require(source.get(field) == evidence_document.get(field), f"Source/evidence binding mismatch for {field}")
    require(source.get("ProfileId") == PROFILE_ID, "Source profile mismatch")
    require(source.get("GameVersion") == GAME_VERSION, "Source game version mismatch")
    require(source.get("Branch") == BRANCH, "Source branch mismatch")
    require(source.get("RuntimeTarget") == RUNTIME_TARGET, "Source runtime target mismatch")
    require(evidence_document.get("SourceFingerprint") == expected_fingerprint, "Evidence fingerprint mismatch")

    evidence_by_id: dict[str, dict[str, Any]] = {}
    for record in require_list(evidence_document.get("Evidence"), "evidence.Evidence"):
        require(isinstance(record, dict), "Evidence entries must be objects")
        evidence_id = require_string(record.get("EvidenceId"), "EvidenceId")
        require(evidence_id.startswith("preview.evidence."), f"Evidence outside preview namespace: {evidence_id}")
        require(evidence_id not in evidence_by_id, f"Duplicate evidence ID: {evidence_id}")
        require(record.get("SourceId") == source.get("SourceId"), f"Evidence source mismatch: {evidence_id}")
        require(
            record.get("SourceFingerprint") == expected_fingerprint,
            f"Evidence fingerprint mismatch: {evidence_id}",
        )
        require(record.get("ProfileId") == PROFILE_ID, f"Evidence profile mismatch: {evidence_id}")
        require(record.get("GameVersion") == GAME_VERSION, f"Evidence game version mismatch: {evidence_id}")
        require(record.get("Branch") == BRANCH, f"Evidence branch mismatch: {evidence_id}")
        subject = require_string(record.get("SubjectRef"), f"{evidence_id}.SubjectRef")
        require(subject.startswith("subject:preview:"), f"Evidence subject outside preview namespace: {subject}")
        evidence_by_id[evidence_id] = record
    require(len(evidence_by_id) == EXPECTED_STATE["evidence_count"], "Unexpected evidence count")
    return evidence_by_id


def require_evidence(values: Any, evidence_by_id: dict[str, Any], label: str) -> None:
    evidence_ids = require_list(values, f"{label}.EvidenceIds")
    require(bool(evidence_ids), f"{label} requires evidence")
    for evidence_id in evidence_ids:
        require(evidence_id in evidence_by_id, f"{label} references unknown evidence: {evidence_id}")


def validate_catalog(catalog: dict[str, Any], evidence_by_id: dict[str, dict[str, Any]]) -> None:
    require(catalog.get("SchemaVersion") == 1, "Unsupported catalog schema")
    require(catalog.get("WorkspaceId") == WORKSPACE_ID, "Catalog workspace binding mismatch")
    require(catalog.get("ProfileId") == PROFILE_ID, "Catalog profile binding mismatch")
    require(catalog.get("GameVersion") == GAME_VERSION, "Catalog game version binding mismatch")
    require(catalog.get("Branch") == BRANCH, "Catalog branch binding mismatch")

    records: dict[str, dict[str, Any]] = {}
    subjects = set()
    for record in require_list(catalog.get("Records"), "catalog.Records"):
        require(isinstance(record, dict), "Catalog records must be objects")
        record_id = require_string(record.get("RecordId"), "RecordId")
        require(record_id.startswith("preview."), f"Record outside preview namespace: {record_id}")
        require(record_id not in records, f"Duplicate record ID: {record_id}")
        require(record.get("IdentityKind") == "synthetic", f"Record must be synthetic: {record_id}")
        require(record.get("OwnerPackId") == PACK_ID, f"Synthetic record has wrong owner: {record_id}")
        require(record.get("NativeRefExact") == "", f"Synthetic record claims a native identity: {record_id}")
        subject = require_string(record.get("SubjectRef"), f"{record_id}.SubjectRef")
        require(subject.startswith("subject:preview:"), f"Record subject outside preview namespace: {record_id}")
        require(subject not in subjects, f"Duplicate subject ref: {subject}")
        subjects.add(subject)
        require_evidence(record.get("EvidenceIds"), evidence_by_id, record_id)
        allowed = require_list(record.get("AllowedUsages"), f"{record_id}.AllowedUsages")
        forbidden = require_list(record.get("ForbiddenUsages"), f"{record_id}.ForbiddenUsages")
        require(not set(allowed).intersection(forbidden), f"Record has conflicting permissions: {record_id}")
        records[record_id] = record
    require(len(records) == EXPECTED_STATE["record_count"], "Unexpected catalog record count")

    relationships: dict[str, dict[str, Any]] = {}
    for relationship in require_list(catalog.get("Relationships"), "catalog.Relationships"):
        require(isinstance(relationship, dict), "Relationships must be objects")
        relationship_id = require_string(relationship.get("RelationshipId"), "RelationshipId")
        require(relationship_id.startswith("preview.relationship."), "Relationship outside preview namespace")
        require(relationship_id not in relationships, f"Duplicate relationship ID: {relationship_id}")
        require(relationship.get("FromRecordId") in records, f"Unknown relationship source: {relationship_id}")
        to_record = relationship.get("ToRecordId", "")
        target_subject = relationship.get("TargetSubjectRef", "")
        require(
            bool(to_record) != bool(target_subject),
            f"Relationship requires exactly one target: {relationship_id}",
        )
        if to_record:
            require(to_record in records, f"Unknown relationship target: {relationship_id}")
        else:
            require(
                isinstance(target_subject, str) and target_subject.startswith("subject:preview:"),
                f"Unresolved target outside preview namespace: {relationship_id}",
            )
            missing = require_list(relationship.get("MissingRefs"), f"{relationship_id}.MissingRefs")
            require(
                target_subject in missing,
                f"Unresolved relationship must preserve its missing reference: {relationship_id}",
            )
        require_evidence(relationship.get("EvidenceIds"), evidence_by_id, relationship_id)
        relationships[relationship_id] = relationship
    require(len(relationships) == EXPECTED_STATE["relationship_count"], "Unexpected relationship count")

    validations: dict[str, dict[str, Any]] = {}
    for validation in require_list(catalog.get("ValidationHistory"), "catalog.ValidationHistory"):
        require(isinstance(validation, dict), "Validation entries must be objects")
        validation_id = require_string(validation.get("ValidationId"), "ValidationId")
        require(validation_id.startswith("preview.validation."), "Validation outside preview namespace")
        require(validation_id not in validations, f"Duplicate validation ID: {validation_id}")
        subject_kind = validation.get("SubjectKind")
        subject_id = validation.get("SubjectId")
        known = records if subject_kind == "record" else relationships
        require(subject_kind in ("record", "relationship"), f"Unsupported validation kind: {validation_id}")
        require(subject_id in known, f"Validation references unknown subject: {validation_id}")
        require(validation.get("ProfileId") == PROFILE_ID, f"Validation profile mismatch: {validation_id}")
        require(validation.get("GameVersion") == GAME_VERSION, f"Validation version mismatch: {validation_id}")
        require(validation.get("Branch") == BRANCH, f"Validation branch mismatch: {validation_id}")
        require_evidence(validation.get("EvidenceIds"), evidence_by_id, validation_id)
        validations[validation_id] = validation

    governance_ids = set()
    for event in require_list(catalog.get("GovernanceHistory"), "catalog.GovernanceHistory"):
        require(isinstance(event, dict), "Governance entries must be objects")
        event_id = require_string(event.get("EventId"), "EventId")
        require(event_id.startswith("preview.governance."), "Governance outside preview namespace")
        require(event_id not in governance_ids, f"Duplicate governance ID: {event_id}")
        governance_ids.add(event_id)
        subject_kind = event.get("SubjectKind")
        subject_id = event.get("SubjectId")
        known = records if subject_kind == "record" else relationships
        require(
            subject_kind in ("record", "relationship") and subject_id in known,
            f"Unknown governance subject: {event_id}",
        )
        require(event.get("Reviewer") == "preview.maintainer", f"Unexpected reviewer: {event_id}")
        require_evidence(event.get("EvidenceIds"), evidence_by_id, event_id)
        for validation_id in require_list(event.get("ValidationIds"), f"{event_id}.ValidationIds"):
            require(validation_id in validations, f"Governance references unknown validation: {event_id}")

    item_profiles = require_list(catalog.get("EconomyItems"), "catalog.EconomyItems")
    require(len(item_profiles) == 2, "Expected two economy item profiles")
    for profile in item_profiles:
        record_id = profile.get("RecordId")
        require(record_id in records and records[record_id].get("RecordKind") == "item", "Bad item profile")
        require_evidence(profile.get("EvidenceIds"), evidence_by_id, f"economy item {record_id}")
        require(profile.get("StackLimit", 0) > 0, f"Invalid stack limit: {record_id}")
        for field in ("Weight", "BaseValue", "Durability"):
            require(profile.get(field, -1) >= 0, f"Negative {field}: {record_id}")

    recipes = require_list(catalog.get("EconomyRecipes"), "catalog.EconomyRecipes")
    require(len(recipes) == 1 and recipes[0].get("RecordId") == RECIPE_ID, "Bad recipe profile")
    recipe = recipes[0]
    require(recipe.get("PersistenceMode") == "custom_template", "Synthetic recipe must use custom_template")
    require(recipe.get("StationRecordIds") == [STATION_ID], "Recipe station set mismatch")
    require(
        records[STATION_ID].get("RecordKind") in {"station", "crafting_station", "interaction_target"},
        "Bad station",
    )
    require_evidence(recipe.get("EvidenceIds"), evidence_by_id, "economy recipe")

    ingredients = require_list(catalog.get("RecipeIngredients"), "catalog.RecipeIngredients")
    outputs = require_list(catalog.get("RecipeOutputs"), "catalog.RecipeOutputs")
    require(len(ingredients) == 1 and len(outputs) == 1, "Expected one ingredient and one output")
    for join, label in ((ingredients[0], "ingredient"), (outputs[0], "output")):
        require(join.get("RecipeRecordId") == RECIPE_ID, f"{label} recipe mismatch")
        item_id = join.get("ItemRecordId", "")
        item_subject = join.get("ItemSubjectRef", "")
        require(bool(item_id) != bool(item_subject), f"{label} requires exactly one item identity")
        require(item_id in records and records[item_id].get("RecordKind") == "item", f"Bad {label} item")
        require(join.get("Quantity", 0) > 0, f"{label} quantity must be positive")
        require_evidence(join.get("EvidenceIds"), evidence_by_id, label)
    require(0 < outputs[0].get("Chance", 0) <= 1, "Output chance is outside (0, 1]")

    for record_id in EXPECTED_STATE["allowed_record_ids"]:
        require(bool(records[record_id].get("AllowedUsages")), f"Allowed state missing: {record_id}")
    for record_id in EXPECTED_STATE["forbidden_record_ids"]:
        require(bool(records[record_id].get("ForbiddenUsages")), f"Forbidden state missing: {record_id}")
    for record_id in EXPECTED_STATE["blocked_record_ids"]:
        blocked = bool(records[record_id].get("ForbiddenUsages") or records[record_id].get("MissingRefs"))
        require(blocked, f"Blocked state missing: {record_id}")
    for record_id in EXPECTED_STATE["stale_record_ids"]:
        require(records[record_id].get("StalenessState") == "stale", f"Stale state missing: {record_id}")
    unresolved = relationships[RELATIONSHIP_UNRESOLVED]
    require(unresolved.get("TargetSubjectRef") == SUBJECT_UNRESOLVED, "Unresolved state missing")


def verify_fixture(
    output_dir: Path,
    *,
    template_root: Path | None = None,
) -> dict[str, Any]:
    output_dir = output_dir.expanduser().resolve(strict=False)
    require(output_dir.is_dir(), f"Fixture output directory does not exist: {output_dir}")
    ensure_no_symlinks(output_dir)
    manifest_path = output_dir / MANIFEST_NAME
    require(manifest_path.is_file(), f"Fixture manifest is missing: {manifest_path}")
    manifest = load_json(manifest_path)
    paths = manifest_paths(manifest)

    actual = sorted(path.relative_to(output_dir).as_posix() for path in output_dir.rglob("*") if path.is_file())
    require(actual == sorted([*paths, MANIFEST_NAME]), "Fixture contains missing or unexpected files")
    entries = {entry["path"]: entry for entry in manifest["files"]}
    for relative_path in paths:
        payload = (output_dir / relative_path).read_bytes()
        require(entries[relative_path].get("sha256") == sha256_bytes(payload), f"SHA-256 mismatch: {relative_path}")
        require(entries[relative_path].get("size_bytes") == len(payload), f"Size mismatch: {relative_path}")
        require(
            payload == canonical_json_bytes(load_json(output_dir / relative_path)),
            f"Non-canonical JSON: {relative_path}",
        )
        assert_public_synthetic_content(load_json(output_dir / relative_path), label=relative_path)

    workspace = load_json(output_dir / WORKSPACE_PATH)
    pack = load_json(output_dir / PACK_PATH)
    source = load_json(output_dir / SOURCE_DOCUMENT_PATH)
    evidence = load_json(output_dir / EVIDENCE_DOCUMENT_PATH)
    catalog = load_json(output_dir / CATALOG_PATH)
    validate_workspace(workspace)
    validate_pack(pack)
    evidence_by_id = validate_source_and_evidence(output_dir, source, evidence)
    validate_catalog(catalog, evidence_by_id)

    expected_manifest = build_manifest(template_documents(template_root))
    require(manifest == expected_manifest, "Manifest does not match deterministic template definition")
    for relative_path in paths:
        template_payload = canonical_json_bytes(template_documents(template_root)[relative_path])
        require(
            (output_dir / relative_path).read_bytes() == template_payload,
            f"Output differs from template: {relative_path}",
        )
    return manifest


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    generate_parser = subparsers.add_parser("generate", help="Generate the deterministic synthetic fixture")
    generate_parser.add_argument("--output", type=Path, required=True, help="Explicit output directory")
    generate_parser.add_argument(
        "--replace",
        action="store_true",
        help="Replace only an existing fixture that first passes full verification",
    )

    verify_parser = subparsers.add_parser("verify", help="Verify hashes, schemas, bindings, and safety")
    verify_parser.add_argument("--output", type=Path, required=True, help="Existing fixture directory")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    try:
        if args.command == "generate":
            manifest = generate_fixture(args.output, replace=args.replace)
            action = "Generated"
        else:
            manifest = verify_fixture(args.output)
            action = "Verified"
        resolved = args.output.expanduser().resolve(strict=False)
        print(f"{action} {manifest['fixture_id']} with {len(manifest['files'])} files at {resolved}")
    except (FixtureError, OSError) as exc:
        print(f"Developer Preview fixture {args.command} failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
