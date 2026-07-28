#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Neutral preview handoff manifest for FOA-SDK visual previews.

This is the fourth visual-preview identity layer only:

FoA native asset reference -> version-bound discovery record -> local preview
artefact -> neutral preview handoff.

It does not invoke Unity, run FoA, invoke O3DE Asset Processor, generate O3DE
preview products, mutate catalogues, grant runtime permission, deploy, sign, or
claim function-complete editor binding.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

TOOL_ID = "foa.neutral-preview-handoff"
TOOL_VERSION = "0.1.0"
DOCUMENT_KIND = "foa-neutral-preview-handoff"
INDEX_KIND = "foa-visual-asset-discovery-index"
THUMBNAIL_KIND = "foa-thumbnail-artifact-evidence"
DEFAULT_HANDOFF_NAME = "foa-preview-handoff.json"
MAX_PAYLOAD_BYTES = 32 * 1024 * 1024

AUTHORITY_FALSE_KEYS = (
    "RuntimeInvocationAllowed",
    "GameMutationAllowed",
    "SaveAccessAllowed",
    "CatalogPromotionAllowed",
    "RuntimePermissionGranted",
    "O3deAssetProcessorInvoked",
    "UnityInvoked",
    "DeploymentAllowed",
    "RepositoryCommitAllowed",
    "RedistributionAllowed",
    "GeneratedO3dePreviewProduct",
    "TypedAuthoringBindingCreated",
    "FunctionCompleteAllowed",
)

ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
PRIVATE_RE = re.compile(r"(^/|^~/|^[A-Za-z]:[\\/]|^\\\\)")
PREVIEW_TOKEN_RE = re.compile(r"^\$preview(/[^\\\r\n]*)?$")
HANDOFF_TOKEN_RE = re.compile(r"^\$handoff(/[^\\\r\n]*)?$")
INSTALL_TOKEN_RE = re.compile(r"^\$install(/[^\\\r\n]*)?$")
SHA_RE = re.compile(r"^sha256:[0-9a-f]{64}$")
MEDIA_TYPES = {
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".webp": "image/webp",
    ".json": "application/json",
}


class HandoffError(RuntimeError):
    """Raised when neutral preview handoff creation or verification fails."""


def pretty_json(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def canonical_json(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def sha256_bytes(payload: bytes) -> str:
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8", errors="strict"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise HandoffError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise HandoffError(f"JSON document must be an object: {path}")
    return value


def require_id(value: Any, label: str) -> str:
    if not isinstance(value, str) or not ID_RE.match(value):
        raise HandoffError(f"{label} must be a lowercase stable identifier.")
    return value


def require_utc(value: Any, label: str) -> str:
    if not isinstance(value, str) or not UTC_RE.match(value):
        raise HandoffError(f"{label} must use whole-second UTC format.")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise HandoffError(f"{label} is not valid UTC.") from exc
    return value


def require_sha(value: Any, label: str) -> str:
    if not isinstance(value, str) or not SHA_RE.match(value):
        raise HandoffError(f"{label} must be sha256:<64-hex>.")
    return value


def assert_no_private_paths(value: Any, label: str = "document") -> None:
    if isinstance(value, str):
        if PRIVATE_RE.search(value):
            raise HandoffError(f"{label} contains an absolute or private path: {value}")
        return
    if isinstance(value, list):
        for index, child in enumerate(value):
            assert_no_private_paths(child, f"{label}[{index}]")
        return
    if isinstance(value, dict):
        for key, child in value.items():
            assert_no_private_paths(child, f"{label}.{key}")


def resolve_document_path(raw: str, base: Path) -> Path:
    path = Path(raw).expanduser()
    return (base / path if not path.is_absolute() else path).resolve(strict=False)


def is_relative_to(path: Path, root: Path) -> bool:
    try:
        path.resolve(strict=False).relative_to(root.resolve(strict=False))
        return True
    except ValueError:
        return False


def load_profile(workspace_path: Path) -> dict[str, Any]:
    workspace = read_json(workspace_path)
    if workspace.get("SchemaVersion") != 1:
        raise HandoffError("Workspace must use SchemaVersion 1.")
    workspace_root = resolve_document_path(str(workspace.get("RootPath", "")), workspace_path.parent)
    active_profile_id = require_id(workspace.get("ActiveGameProfileId"), "ActiveGameProfileId")
    profiles = workspace.get("GameProfiles")
    if not isinstance(profiles, list):
        raise HandoffError("Workspace GameProfiles must be an array.")
    matches = [entry for entry in profiles if isinstance(entry, dict) and entry.get("ProfileId") == active_profile_id]
    if len(matches) != 1:
        raise HandoffError("Workspace ActiveGameProfileId must bind to exactly one profile.")
    profile = matches[0]
    runtime = profile.get("RuntimeTarget")
    if runtime not in {"Mono", "IL2CPP"}:
        raise HandoffError("RuntimeTarget must be Mono or IL2CPP.")
    install = resolve_document_path(str(profile.get("InstallPath", "")), workspace_path.parent)
    extracted = resolve_document_path(str(profile.get("ExtractedDataPath", "")), workspace_path.parent)
    if not install.is_dir():
        raise HandoffError("Configured FoA install path does not exist or is not a directory.")
    if not is_relative_to(extracted, workspace_root):
        raise HandoffError("ExtractedDataPath must remain inside workspace root.")
    return {
        "ProfileId": require_id(profile.get("ProfileId"), "ProfileId"),
        "GameVersion": str(profile.get("GameVersion", "")),
        "Branch": str(profile.get("Branch", "")),
        "RuntimeTarget": runtime,
        "InstallPath": install,
        "ExtractedDataPath": extracted,
    }


def validate_profile_bound(document: Mapping[str, Any], profile: Mapping[str, Any], label: str) -> None:
    for key in ("ProfileId", "GameVersion", "Branch", "RuntimeTarget"):
        if document.get(key) != profile[key]:
            raise HandoffError(f"{label} must match the exact active workspace profile.")


def validate_index(index: Mapping[str, Any], profile: Mapping[str, Any]) -> dict[str, Mapping[str, Any]]:
    if index.get("SchemaVersion") != 1 or index.get("DocumentKind") != INDEX_KIND:
        raise HandoffError("Input is not a FoA visual asset discovery index.")
    validate_profile_bound(index, profile, "Index")
    gate = index.get("PreviewGateStatus")
    if not isinstance(gate, dict) or gate.get("FunctionCompleteAllowed") is not False:
        raise HandoffError("Input index must keep FunctionCompleteAllowed=false.")
    records = index.get("AssetRecords")
    if not isinstance(records, list):
        raise HandoffError("Index AssetRecords must be an array.")
    mapping: dict[str, Mapping[str, Any]] = {}
    for raw in records:
        if not isinstance(raw, dict):
            raise HandoffError("AssetRecords entries must be objects.")
        asset_id = require_id(raw.get("AssetRecordId"), "AssetRecordId")
        if asset_id in mapping:
            raise HandoffError("Duplicate AssetRecordId in visual asset index.")
        native_ref = raw.get("NativeAssetRef")
        if not isinstance(native_ref, str) or not INSTALL_TOKEN_RE.match(native_ref):
            raise HandoffError("AssetRecord NativeAssetRef must be a $install token path.")
        require_sha(raw.get("Sha256"), "AssetRecord Sha256")
        mapping[asset_id] = raw
    assert_no_private_paths(index, "index")
    return mapping


def validate_thumbnail_manifest(manifest: Mapping[str, Any], profile: Mapping[str, Any], index: Mapping[str, Any]) -> list[Mapping[str, Any]]:
    if manifest.get("SchemaVersion") != 1 or manifest.get("DocumentKind") != THUMBNAIL_KIND:
        raise HandoffError("Input is not a thumbnail artefact evidence manifest.")
    validate_profile_bound(manifest, profile, "Thumbnail manifest")
    if manifest.get("SourceIndexId") != index.get("IndexId"):
        raise HandoffError("Thumbnail manifest SourceIndexId must match visual asset index.")
    stage = manifest.get("PreviewStageStatus")
    if not isinstance(stage, dict) or stage.get("FunctionCompleteAllowed") is not False:
        raise HandoffError("Thumbnail manifest must keep FunctionCompleteAllowed=false.")
    artifacts = manifest.get("ThumbnailArtifacts")
    if not isinstance(artifacts, list):
        raise HandoffError("ThumbnailArtifacts must be an array.")
    seen: set[str] = set()
    for raw in artifacts:
        if not isinstance(raw, dict):
            raise HandoffError("ThumbnailArtifacts entries must be objects.")
        artifact_id = require_id(raw.get("ThumbnailArtifactId"), "ThumbnailArtifactId")
        if artifact_id in seen:
            raise HandoffError("Duplicate ThumbnailArtifactId.")
        seen.add(artifact_id)
        for key in ("RedistributionAllowed", "RepositoryCommitAllowed", "PreviewProductGenerated", "O3deAssetProcessorInvoked", "UnityInvoked", "RuntimePermissionGranted"):
            if raw.get(key) is not False:
                raise HandoffError(f"Thumbnail artifact authority escalation: {key}")
        status = raw.get("Status")
        if status not in {"generated", "unsupported"}:
            raise HandoffError("Thumbnail artifact Status must be generated or unsupported.")
        if status == "generated":
            if not isinstance(raw.get("ArtifactPath"), str) or not PREVIEW_TOKEN_RE.match(raw["ArtifactPath"]):
                raise HandoffError("Generated thumbnail artifacts require $preview ArtifactPath.")
            require_sha(raw.get("ArtifactSha256"), "Thumbnail ArtifactSha256")
    assert_no_private_paths(manifest, "thumbnail manifest")
    return artifacts


def thumbnail_payload_path(artifact: Mapping[str, Any], thumbnail_manifest_path: Path) -> Path:
    token = str(artifact.get("ArtifactPath", ""))
    if not PREVIEW_TOKEN_RE.match(token):
        raise HandoffError("ArtifactPath must be a $preview token path.")
    suffix = token[len("$preview/"):]
    path = (thumbnail_manifest_path.parent / suffix).resolve(strict=False)
    if not is_relative_to(path, thumbnail_manifest_path.parent):
        raise HandoffError("Thumbnail artifact path escaped preview root.")
    return path


def handoff_token(path: Path, handoff_root: Path) -> str:
    relative = path.resolve(strict=False).relative_to(handoff_root.resolve(strict=False))
    if any(part in {"", ".", ".."} for part in relative.parts):
        raise HandoffError("Handoff payload path contains unsafe segment.")
    return "$handoff/" + relative.as_posix()


def media_type_for(path: Path) -> str:
    return MEDIA_TYPES.get(path.suffix.lower(), "application/octet-stream")


def copy_payload(source: Path, destination: Path) -> dict[str, Any]:
    if not source.is_file():
        raise HandoffError(f"Handoff source payload is missing: {source}")
    payload = source.read_bytes()
    if len(payload) > MAX_PAYLOAD_BYTES:
        raise HandoffError(f"Handoff payload exceeds {MAX_PAYLOAD_BYTES} bytes: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    temporary.write_bytes(payload)
    os.replace(temporary, destination)
    return {"Sha256": sha256_bytes(payload), "ByteSize": len(payload), "MediaType": media_type_for(destination)}


def dependency_for(record: Mapping[str, Any], source_index_id: str, role: str) -> dict[str, Any]:
    return {
        "SourceAssetRecordId": require_id(record.get("AssetRecordId"), "AssetRecordId"),
        "NativeAssetRef": str(record.get("NativeAssetRef", "")),
        "SourceIndexId": source_index_id,
        "SourceFingerprint": require_sha(record.get("Sha256"), "AssetRecord Sha256"),
        "DependencyRole": role,
        "DependencyKind": "visual-asset-discovery-record",
        "RequiredForPreview": True,
    }


def merge_dependencies(dependencies: Sequence[Mapping[str, Any]]) -> list[dict[str, Any]]:
    merged: dict[str, dict[str, Any]] = {}
    for dependency in dependencies:
        key = str(dependency["SourceAssetRecordId"])
        current = dict(dependency)
        if key in merged and merged[key].get("DependencyRole") != "primary":
            if current.get("DependencyRole") == "primary":
                merged[key] = current
        else:
            merged[key] = current
    return sorted(merged.values(), key=lambda item: item["SourceAssetRecordId"])


def coordinate_declaration() -> dict[str, Any]:
    return {
        "DeclaredSourceCoordinateSystem": {
            "System": "unity-declared",
            "UpAxis": "Y",
            "ForwardAxis": "Z",
            "Scale": 1.0,
            "EvidenceState": "declared-not-verified",
        },
        "DeclaredTargetCoordinateSystem": {
            "System": "o3de-preview-declared",
            "UpAxis": "Z",
            "ForwardAxis": "Y",
            "Scale": 1.0,
            "EvidenceState": "declared-not-verified",
        },
    }


def coordinate_conversion_evidence() -> dict[str, Any]:
    return {
        "ConversionToolId": "none.alpha-neutral-handoff-only",
        "ConversionToolVersion": "0.0.0",
        "TransformPolicyId": "declared-no-transform-applied",
        "ConversionOperationPerformed": False,
        "ConversionMatrix": [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
        "VerificationState": "not-verified",
        "VerificationEvidenceIds": [],
        "VerificationEvidenceRequired": True,
    }


def build_handoff(
    workspace_path: Path,
    index_path: Path,
    thumbnail_manifest_path: Path,
    *,
    output_root: Path | None = None,
    captured_at: str | None = None,
    replace: bool = False,
) -> tuple[dict[str, Any], Path]:
    profile = load_profile(workspace_path)
    index = read_json(index_path)
    index_records = validate_index(index, profile)
    thumbnails = read_json(thumbnail_manifest_path)
    artifacts = validate_thumbnail_manifest(thumbnails, profile, index)
    if not is_relative_to(index_path.resolve(strict=False), profile["ExtractedDataPath"]):
        raise HandoffError("Visual asset index must remain inside ExtractedDataPath.")
    if not is_relative_to(thumbnail_manifest_path.resolve(strict=False), profile["ExtractedDataPath"]):
        raise HandoffError("Thumbnail manifest must remain inside ExtractedDataPath.")
    captured_at = require_utc(captured_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "CapturedAt")

    candidate_artifacts = [artifact for artifact in artifacts if artifact.get("Status") in {"generated", "unsupported"}]
    if not candidate_artifacts:
        raise HandoffError("At least one generated or unsupported thumbnail artifact is required for a handoff.")
    seed = canonical_json({
        "ProfileId": profile["ProfileId"],
        "SourceIndexId": index["IndexId"],
        "ThumbnailManifestId": thumbnails.get("ManifestId", ""),
        "Artifacts": [(item.get("ThumbnailArtifactId"), item.get("ArtifactSha256", ""), item.get("Status")) for item in candidate_artifacts],
        "CapturedAt": captured_at,
    })
    handoff_id = "preview.handoff." + profile["ProfileId"] + "." + hashlib.sha256(seed).hexdigest()[:16]
    handoff_root = (output_root.resolve(strict=False) if output_root is not None else (profile["ExtractedDataPath"] / "PreviewArtifacts" / "Handoffs" / handoff_id).resolve(strict=False))
    if not is_relative_to(handoff_root, profile["ExtractedDataPath"]):
        raise HandoffError("Handoff output root must remain inside ExtractedDataPath.")
    if handoff_root.exists() and any(handoff_root.iterdir()):
        if replace:
            shutil.rmtree(handoff_root)
        else:
            raise HandoffError(f"Handoff output root is not empty: {handoff_root}")
    handoff_root.mkdir(parents=True, exist_ok=True)

    dependencies: list[dict[str, Any]] = []
    payloads: list[dict[str, Any]] = []
    entries: list[dict[str, Any]] = []
    issues: list[dict[str, Any]] = []

    for ordinal, artifact in enumerate(candidate_artifacts):
        asset_record_id = require_id(artifact.get("AssetRecordId"), "AssetRecordId")
        if asset_record_id not in index_records:
            raise HandoffError(f"Thumbnail artifact references missing asset record: {asset_record_id}")
        record = index_records[asset_record_id]
        dependency = dependency_for(record, str(index["IndexId"]), "primary" if ordinal == 0 else "source")
        dependencies.append(dependency)
        entry_id = "preview.entry." + hashlib.sha256(canonical_json([handoff_id, artifact.get("ThumbnailArtifactId"), asset_record_id])).hexdigest()[:16]
        entry_payload_refs: list[str] = []
        entry_warnings: list[str] = []
        entry_losses: list[str] = []
        preview_class = "icon" if artifact.get("Status") == "generated" else "unsupported"
        fidelity = {
            "Geometry": "none",
            "Materials": "none",
            "Textures": "exact-copy" if artifact.get("Status") == "generated" else "unsupported",
            "Skeleton": "unsupported",
            "Animation": "unsupported",
        }
        if artifact.get("Status") == "generated":
            source_payload = thumbnail_payload_path(artifact, thumbnail_manifest_path)
            suffix = source_payload.suffix.lower() or str(artifact.get("ArtifactExtension", ".bin"))
            payload_path = handoff_root / "payloads" / "thumbnails" / f"{artifact['ThumbnailArtifactId']}{suffix}"
            written = copy_payload(source_payload, payload_path)
            if artifact.get("ArtifactSha256") and artifact.get("ArtifactSha256") != written["Sha256"]:
                raise HandoffError("Copied handoff payload does not match source thumbnail artifact hash.")
            payload_id = "payload." + hashlib.sha256(canonical_json([handoff_id, artifact["ThumbnailArtifactId"], written["Sha256"]])).hexdigest()[:16]
            entry_payload_refs.append(payload_id)
            payloads.append({
                "PayloadId": payload_id,
                "Role": "thumbnail",
                "Path": handoff_token(payload_path, handoff_root),
                "MediaType": written["MediaType"],
                "Sha256": written["Sha256"],
                "ByteSize": written["ByteSize"],
                "Generated": True,
                "LocalOnly": True,
                "RepositoryCommitAllowed": False,
                "RedistributionAllowed": False,
                "SourceAssetRecordIds": [asset_record_id],
                "SourceThumbnailArtifactId": artifact["ThumbnailArtifactId"],
            })
        else:
            receipt = {
                "SchemaVersion": 1,
                "DocumentKind": "foa-neutral-preview-unsupported-receipt",
                "HandoffId": handoff_id,
                "ThumbnailArtifactId": artifact.get("ThumbnailArtifactId", ""),
                "AssetRecordId": asset_record_id,
                "NativeAssetRef": artifact.get("NativeAssetRef", ""),
                "Reason": artifact.get("Reason", "unsupported thumbnail artifact"),
                "GeneratedO3dePreviewProduct": False,
                "RuntimePermissionGranted": False,
            }
            receipt_path = handoff_root / "payloads" / "metadata" / f"{artifact['ThumbnailArtifactId']}.unsupported.json"
            receipt_path.parent.mkdir(parents=True, exist_ok=True)
            receipt_path.write_bytes(pretty_json(receipt))
            payload_bytes = receipt_path.read_bytes()
            payload_id = "payload." + hashlib.sha256(canonical_json([handoff_id, artifact["ThumbnailArtifactId"], sha256_bytes(payload_bytes)])).hexdigest()[:16]
            entry_payload_refs.append(payload_id)
            entry_warnings.append("Unsupported source emitted as receipt only; no preview conversion performed.")
            entry_losses.append("No texture payload generated for unsupported source.")
            payloads.append({
                "PayloadId": payload_id,
                "Role": "metadata",
                "Path": handoff_token(receipt_path, handoff_root),
                "MediaType": "application/json",
                "Sha256": sha256_bytes(payload_bytes),
                "ByteSize": len(payload_bytes),
                "Generated": True,
                "LocalOnly": True,
                "RepositoryCommitAllowed": False,
                "RedistributionAllowed": False,
                "SourceAssetRecordIds": [asset_record_id],
                "SourceThumbnailArtifactId": artifact["ThumbnailArtifactId"],
            })
        entries.append({
            "PreviewEntryId": entry_id,
            "PrimarySourceAssetRecordId": asset_record_id,
            "SourceDependencies": [dependency],
            "NativeAssetRef": artifact.get("NativeAssetRef", record.get("NativeAssetRef", "")),
            "SourceIndexId": index["IndexId"],
            "SourceThumbnailArtifactId": artifact.get("ThumbnailArtifactId", ""),
            "PreviewClass": preview_class,
            "PayloadRefs": entry_payload_refs,
            "Fidelity": fidelity,
            "Losses": entry_losses,
            "Warnings": entry_warnings,
            "GeneratedO3dePreviewProduct": False,
            "TypedAuthoringBindingCreated": False,
        })

    dependencies = merge_dependencies(dependencies)
    source_ids = [dependency["SourceAssetRecordId"] for dependency in dependencies]
    primary_source_id = source_ids[0]
    manifest = {
        "SchemaVersion": 1,
        "DocumentKind": DOCUMENT_KIND,
        "HandoffId": handoff_id,
        "ProfileId": profile["ProfileId"],
        "GameVersion": profile["GameVersion"],
        "Branch": profile["Branch"],
        "RuntimeTarget": profile["RuntimeTarget"],
        "ToolId": TOOL_ID,
        "ToolVersion": TOOL_VERSION,
        "CapturedAt": captured_at,
        "PreviewIntent": "editor-preview-only",
        "PreviewStageStatus": {
            "DiscoveryIndexConsumed": True,
            "LocalPreviewArtifactsConsumed": True,
            "NeutralPreviewHandoffEmitted": True,
            "GeneratedO3dePreviewProduct": False,
            "TypedAuthoringBindingCreated": False,
            "FunctionCompleteAllowed": False,
            "NextRequiredStages": ["neutral-to-o3de-preview-conversion", "asset-browser-pane", "item-recipe-visual-selectors"],
        },
        "SourceIndexId": index["IndexId"],
        "SourceThumbnailManifestId": thumbnails.get("ManifestId", ""),
        "PrimarySourceAssetRecordId": primary_source_id,
        "SourceAssetRecordIds": source_ids,
        "SourceDependencies": dependencies,
        "CoordinateDeclaration": coordinate_declaration(),
        "CoordinateConversionEvidence": coordinate_conversion_evidence(),
        "PreviewEntries": sorted(entries, key=lambda item: item["PreviewEntryId"]),
        "Payloads": sorted(payloads, key=lambda item: item["PayloadId"]),
        "Losses": sorted({loss for entry in entries for loss in entry.get("Losses", [])}),
        "Warnings": sorted({warning for entry in entries for warning in entry.get("Warnings", [])}),
        "Issues": issues,
        "OperationalAuthority": {key: False for key in AUTHORITY_FALSE_KEYS},
    }
    assert_no_private_paths(manifest)
    manifest_path = handoff_root / DEFAULT_HANDOFF_NAME
    manifest_path.write_bytes(pretty_json(manifest))
    return manifest, manifest_path


def validate_coordinate_blocks(document: Mapping[str, Any]) -> None:
    if "TransformVerified" in document:
        raise HandoffError("TransformVerified must not appear at top level; use CoordinateConversionEvidence.")
    declaration = document.get("CoordinateDeclaration")
    evidence = document.get("CoordinateConversionEvidence")
    if not isinstance(declaration, dict) or not isinstance(evidence, dict):
        raise HandoffError("Coordinate declaration and conversion evidence blocks are required.")
    for key in ("DeclaredSourceCoordinateSystem", "DeclaredTargetCoordinateSystem"):
        if not isinstance(declaration.get(key), dict):
            raise HandoffError(f"CoordinateDeclaration requires {key}.")
        if declaration[key].get("EvidenceState") != "declared-not-verified":
            raise HandoffError("Coordinate declaration must remain declared-not-verified in this Alpha slice.")
    if evidence.get("ConversionOperationPerformed") is not False:
        raise HandoffError("Neutral handoff must not claim coordinate conversion was performed.")
    if evidence.get("VerificationState") != "not-verified":
        raise HandoffError("Coordinate conversion verification must remain not-verified.")
    matrix = evidence.get("ConversionMatrix")
    if not (isinstance(matrix, list) and len(matrix) == 4 and all(isinstance(row, list) and len(row) == 4 for row in matrix)):
        raise HandoffError("CoordinateConversionEvidence requires a 4x4 ConversionMatrix.")
    if not isinstance(evidence.get("VerificationEvidenceIds"), list):
        raise HandoffError("CoordinateConversionEvidence requires VerificationEvidenceIds array.")


def verify_handoff(manifest_path: Path, *, workspace_path: Path | None = None, index_path: Path | None = None, thumbnail_manifest_path: Path | None = None) -> dict[str, Any]:
    document = read_json(manifest_path)
    if document.get("SchemaVersion") != 1 or document.get("DocumentKind") != DOCUMENT_KIND:
        raise HandoffError("Input is not a FoA neutral preview handoff manifest.")
    require_id(document.get("HandoffId"), "HandoffId")
    require_utc(document.get("CapturedAt"), "CapturedAt")
    authority = document.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise HandoffError("OperationalAuthority is required.")
    for key in AUTHORITY_FALSE_KEYS:
        if authority.get(key) is not False:
            raise HandoffError(f"Handoff authority escalation: {key}")
    stage = document.get("PreviewStageStatus")
    if not isinstance(stage, dict) or stage.get("FunctionCompleteAllowed") is not False:
        raise HandoffError("PreviewStageStatus must keep FunctionCompleteAllowed=false.")
    validate_coordinate_blocks(document)
    dependencies = document.get("SourceDependencies")
    if not isinstance(dependencies, list) or not dependencies:
        raise HandoffError("SourceDependencies must be a non-empty collection.")
    dependency_ids: set[str] = set()
    for dependency in dependencies:
        if not isinstance(dependency, dict):
            raise HandoffError("SourceDependencies entries must be objects.")
        dependency_ids.add(require_id(dependency.get("SourceAssetRecordId"), "SourceAssetRecordId"))
        if not isinstance(dependency.get("NativeAssetRef"), str) or not INSTALL_TOKEN_RE.match(dependency["NativeAssetRef"]):
            raise HandoffError("Source dependency NativeAssetRef must be tokenized.")
        require_sha(dependency.get("SourceFingerprint"), "SourceFingerprint")
    primary = require_id(document.get("PrimarySourceAssetRecordId"), "PrimarySourceAssetRecordId")
    if primary not in dependency_ids:
        raise HandoffError("PrimarySourceAssetRecordId must appear in SourceDependencies.")
    if sorted(document.get("SourceAssetRecordIds", [])) != sorted(dependency_ids):
        raise HandoffError("SourceAssetRecordIds must mirror SourceDependencies.")
    payloads = document.get("Payloads")
    if not isinstance(payloads, list):
        raise HandoffError("Payloads must be an array.")
    payload_ids: set[str] = set()
    root = manifest_path.parent.resolve(strict=False)
    for payload in payloads:
        if not isinstance(payload, dict):
            raise HandoffError("Payload entries must be objects.")
        payload_id = require_id(payload.get("PayloadId"), "PayloadId")
        if payload_id in payload_ids:
            raise HandoffError("Duplicate PayloadId.")
        payload_ids.add(payload_id)
        if payload.get("RepositoryCommitAllowed") is not False or payload.get("RedistributionAllowed") is not False:
            raise HandoffError("Payload must remain local-only and non-redistributable.")
        token = payload.get("Path")
        if not isinstance(token, str) or not HANDOFF_TOKEN_RE.match(token):
            raise HandoffError("Payload Path must be a $handoff token path.")
        payload_file = root / token[len("$handoff/"):]
        if not payload_file.is_file():
            raise HandoffError(f"Handoff payload is missing: {token}")
        data = payload_file.read_bytes()
        if payload.get("ByteSize") != len(data):
            raise HandoffError(f"Payload size mismatch: {token}")
        if payload.get("Sha256") != sha256_bytes(data):
            raise HandoffError(f"Payload SHA-256 mismatch: {token}")
        source_ids = payload.get("SourceAssetRecordIds")
        if not isinstance(source_ids, list) or not source_ids or any(item not in dependency_ids for item in source_ids):
            raise HandoffError("Payload SourceAssetRecordIds must reference SourceDependencies.")
    entries = document.get("PreviewEntries")
    if not isinstance(entries, list) or not entries:
        raise HandoffError("PreviewEntries must be a non-empty array.")
    for entry in entries:
        if not isinstance(entry, dict):
            raise HandoffError("PreviewEntry entries must be objects.")
        require_id(entry.get("PreviewEntryId"), "PreviewEntryId")
        if require_id(entry.get("PrimarySourceAssetRecordId"), "PrimarySourceAssetRecordId") not in dependency_ids:
            raise HandoffError("PreviewEntry primary source must be in top-level dependencies.")
        if entry.get("GeneratedO3dePreviewProduct") is not False or entry.get("TypedAuthoringBindingCreated") is not False:
            raise HandoffError("PreviewEntry cannot claim later-stage products or bindings.")
        refs = entry.get("PayloadRefs")
        if not isinstance(refs, list) or any(ref not in payload_ids for ref in refs):
            raise HandoffError("PreviewEntry PayloadRefs must reference Payloads.")
        deps = entry.get("SourceDependencies")
        if not isinstance(deps, list) or not deps:
            raise HandoffError("PreviewEntry requires local SourceDependencies.")
    if workspace_path is not None:
        profile = load_profile(workspace_path)
        validate_profile_bound(document, profile, "Handoff")
        if not is_relative_to(manifest_path.resolve(strict=False), profile["ExtractedDataPath"]):
            raise HandoffError("Handoff manifest must remain inside ExtractedDataPath.")
    if index_path is not None:
        index = read_json(index_path)
        if document.get("SourceIndexId") != index.get("IndexId"):
            raise HandoffError("Handoff SourceIndexId must match visual asset index.")
    if thumbnail_manifest_path is not None:
        thumbnails = read_json(thumbnail_manifest_path)
        if document.get("SourceThumbnailManifestId") != thumbnails.get("ManifestId"):
            raise HandoffError("Handoff SourceThumbnailManifestId must match thumbnail manifest.")
    assert_no_private_paths(document)
    return document


def generate_fixture(output: Path, *, replace: bool = False) -> dict[str, Any]:
    if output.exists():
        if replace:
            shutil.rmtree(output) if output.is_dir() else output.unlink()
        else:
            raise HandoffError(f"Fixture output is not empty: {output}")
    install = output / "game" / "FoA"
    icons = install / "Tainted Grail_Data" / "LooseIcons"
    thumbnails_root = output / "workspace" / "Extracted" / "PreviewArtifacts" / "Thumbnails"
    icons.mkdir(parents=True, exist_ok=True)
    thumbnails_root.mkdir(parents=True, exist_ok=True)
    (icons / "iron.png").write_bytes(b"synthetic-png")
    workspace = {"SchemaVersion": 1, "WorkspaceId": "fixture.workspace", "DisplayName": "Neutral Handoff Fixture", "RootPath": "./workspace", "OutputPath": "./workspace/Build", "StagingPath": "./workspace/Staging", "DeploymentPath": "./workspace/Deploy", "ActiveGameProfileId": "foa.mono.fixture", "GameProfiles": [{"ProfileId": "foa.mono.fixture", "DisplayName": "FoA Mono Fixture", "InstallPath": "./game/FoA", "GameVersion": "1.23.401", "Branch": "mono", "RuntimeTarget": "Mono", "UnityVersion": "6000.0.64f1", "BepInExVersion": "5.4.23.3", "ManagedAssembliesPath": "", "PluginPath": "", "DiagnosticsPath": "./workspace/Diagnostics", "ExtractedDataPath": "./workspace/Extracted", "DlcScopes": ["base-game"]}]}
    workspace_path = output / "workspace.tgworkspace.json"
    workspace_path.write_bytes(pretty_json(workspace))
    profile = load_profile(workspace_path)
    source_payload = icons / "iron.png"
    source_sha = sha256_bytes(source_payload.read_bytes())
    asset_record_id = "visual.asset.foa.mono.fixture." + hashlib.sha256(canonical_json(["iron.png", source_sha])).hexdigest()[:16]
    index = {"SchemaVersion": 1, "DocumentKind": INDEX_KIND, "IndexId": "visual.index.foa.mono.fixture.synthetic", "ProfileId": profile["ProfileId"], "GameVersion": profile["GameVersion"], "Branch": profile["Branch"], "RuntimeTarget": profile["RuntimeTarget"], "ToolId": "foa.visual-asset-discovery-index", "ToolVersion": "0.1.0", "CapturedAt": "2026-07-28T00:00:00Z", "PreviewGateStatus": {"FunctionCompleteAllowed": False}, "AssetRecords": [{"AssetRecordId": asset_record_id, "NativeAssetRef": "$install/Tainted Grail_Data/LooseIcons/iron.png", "ProfileId": profile["ProfileId"], "GameVersion": profile["GameVersion"], "Branch": profile["Branch"], "RuntimeTarget": profile["RuntimeTarget"], "Sha256": source_sha}], "OperationalAuthority": {"RuntimeInvocationAllowed": False, "GameMutationAllowed": False, "SaveAccessAllowed": False, "CatalogPromotionAllowed": False, "RuntimePermissionGranted": False, "PreviewProductGenerated": False, "O3deAssetProcessorInvoked": False, "UnityInvoked": False, "PayloadCopied": False}}
    index_path = profile["ExtractedDataPath"] / "foa-visual-asset-index.json"
    index_path.parent.mkdir(parents=True, exist_ok=True)
    index_path.write_bytes(pretty_json(index))
    thumbnail_id = "thumbnail." + asset_record_id.removeprefix("visual.asset.")
    thumbnail_payload_path = thumbnails_root / f"{thumbnail_id}.png"
    thumbnail_payload_path.write_bytes(source_payload.read_bytes())
    thumbnail_manifest = {"SchemaVersion": 1, "DocumentKind": THUMBNAIL_KIND, "ManifestId": "thumbnail.manifest.foa.mono.fixture.synthetic", "SourceIndexId": index["IndexId"], "ProfileId": profile["ProfileId"], "GameVersion": profile["GameVersion"], "Branch": profile["Branch"], "RuntimeTarget": profile["RuntimeTarget"], "ToolId": "foa.thumbnail-artifact-extractor", "ToolVersion": "0.1.0", "CapturedAt": "2026-07-28T00:00:01Z", "PreviewStageStatus": {"FunctionCompleteAllowed": False}, "ThumbnailArtifacts": [{"ThumbnailArtifactId": thumbnail_id, "AssetRecordId": asset_record_id, "NativeAssetRef": "$install/Tainted Grail_Data/LooseIcons/iron.png", "SourceIndexId": index["IndexId"], "SourceSha256": source_sha, "ArtifactPath": "$preview/" + thumbnail_payload_path.name, "ArtifactKind": "native-icon-thumbnail", "ArtifactExtension": ".png", "GenerationMethod": "local-only-loose-icon-copy", "Fidelity": "native-icon-byte-preserved", "Status": "generated", "LocalOnly": True, "RedistributionAllowed": False, "RepositoryCommitAllowed": False, "PreviewProductGenerated": False, "O3deAssetProcessorInvoked": False, "UnityInvoked": False, "RuntimePermissionGranted": False, "CapturedAt": "2026-07-28T00:00:01Z", "ArtifactSha256": source_sha, "ArtifactByteSize": len(source_payload.read_bytes())}], "Issues": [], "OperationalAuthority": {"RuntimeInvocationAllowed": False, "GameMutationAllowed": False, "SaveAccessAllowed": False, "CatalogPromotionAllowed": False, "RuntimePermissionGranted": False, "GeneratedO3dePreviewProduct": False, "O3deAssetProcessorInvoked": False, "UnityInvoked": False, "RepositoryWriteAllowed": False, "FunctionCompleteAllowed": False}}
    thumbnail_manifest_path = thumbnails_root / "foa-thumbnail-artifacts.json"
    thumbnail_manifest_path.write_bytes(pretty_json(thumbnail_manifest))
    handoff, handoff_path = build_handoff(workspace_path, index_path, thumbnail_manifest_path, captured_at="2026-07-28T00:00:02Z")
    verify_handoff(handoff_path, workspace_path=workspace_path, index_path=index_path, thumbnail_manifest_path=thumbnail_manifest_path)
    return {"HandoffId": handoff["HandoffId"], "PreviewEntryCount": len(handoff["PreviewEntries"]), "PayloadCount": len(handoff["Payloads"]), "HandoffPath": str(handoff_path)}


def parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create neutral FOA preview handoff manifests.")
    sub = parser.add_subparsers(dest="command", required=True)
    handoff = sub.add_parser("handoff", help="Create a neutral preview handoff from discovery and thumbnail evidence.")
    handoff.add_argument("--workspace", required=True, type=Path)
    handoff.add_argument("--index", required=True, type=Path)
    handoff.add_argument("--thumbnails", required=True, type=Path)
    handoff.add_argument("--output-root", type=Path)
    handoff.add_argument("--captured-at")
    handoff.add_argument("--replace", action="store_true")
    verify = sub.add_parser("verify", help="Verify a neutral preview handoff manifest.")
    verify.add_argument("--input", required=True, type=Path)
    verify.add_argument("--workspace", type=Path)
    verify.add_argument("--index", type=Path)
    verify.add_argument("--thumbnails", type=Path)
    fixture = sub.add_parser("fixture", help="Generate a project-owned synthetic fixture.")
    fixture.add_argument("--output", required=True, type=Path)
    fixture.add_argument("--replace", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.command == "fixture":
            manifest = generate_fixture(args.output, replace=args.replace)
            print(f"FoA neutral preview handoff fixture wrote {manifest['PreviewEntryCount']} entries and {manifest['PayloadCount']} payloads.")
        elif args.command == "handoff":
            document, path = build_handoff(args.workspace, args.index, args.thumbnails, output_root=args.output_root, captured_at=args.captured_at, replace=args.replace)
            print(f"FoA neutral preview handoff wrote {len(document['PreviewEntries'])} entries to {path}.")
        else:
            document = verify_handoff(args.input, workspace_path=args.workspace, index_path=args.index, thumbnail_manifest_path=args.thumbnails)
            print(f"FoA neutral preview handoff verified: {document['HandoffId']} with {len(document['PreviewEntries'])} entries.")
    except HandoffError as exc:
        print(f"FoA neutral preview handoff failed: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())