#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Native icon and thumbnail artefact extraction for the visual preview pipeline.

Consumes a `foa-visual-asset-index.json` discovery index and emits local-only
thumbnail artefact evidence. This is the third identity layer only:

FoA native asset reference -> version-bound discovery record -> local preview artefact

It does not invoke Unity, run FoA, parse bundles, invoke O3DE Asset Processor,
create generated O3DE preview products, mutate catalogues, grant runtime
permission, or produce function-complete editor bindings.
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

TOOL_ID = "foa.thumbnail-artifact-extractor"
TOOL_VERSION = "0.1.0"
DOCUMENT_KIND = "foa-thumbnail-artifact-evidence"
INDEX_KIND = "foa-visual-asset-discovery-index"
DEFAULT_INDEX_NAME = "foa-visual-asset-index.json"
DEFAULT_MANIFEST_NAME = "foa-thumbnail-artifacts.json"
SUPPORTED_THUMBNAIL_EXTENSIONS = {".png", ".jpg", ".jpeg", ".webp"}
UNSUPPORTED_TEXTURE_EXTENSIONS = {".dds", ".tga"}
MAX_THUMBNAIL_BYTES = 16 * 1024 * 1024
MAX_ARTIFACTS = 10000

AUTHORITY_FALSE_KEYS = (
    "RuntimeInvocationAllowed",
    "GameMutationAllowed",
    "SaveAccessAllowed",
    "CatalogPromotionAllowed",
    "RuntimePermissionGranted",
    "GeneratedO3dePreviewProduct",
    "O3deAssetProcessorInvoked",
    "UnityInvoked",
    "RepositoryWriteAllowed",
    "FunctionCompleteAllowed",
)

ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
PRIVATE_RE = re.compile(r"(^/|^~/|^[A-Za-z]:[\\/]|^\\\\)")
INSTALL_TOKEN_RE = re.compile(r"^\$install(/[^\\\r\n]*)?$")
PREVIEW_TOKEN_RE = re.compile(r"^\$preview(/[^\\\r\n]*)?$")


class ThumbnailError(RuntimeError):
    """Raised when thumbnail artefact extraction or verification fails."""


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
        raise ThumbnailError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ThumbnailError(f"JSON document must be an object: {path}")
    return value


def require_id(value: Any, label: str) -> str:
    if not isinstance(value, str) or not ID_RE.match(value):
        raise ThumbnailError(f"{label} must be a lowercase stable identifier.")
    return value


def require_utc(value: Any, label: str) -> str:
    if not isinstance(value, str) or not UTC_RE.match(value):
        raise ThumbnailError(f"{label} must use whole-second UTC format.")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise ThumbnailError(f"{label} is not valid UTC.") from exc
    return value


def assert_no_private_paths(value: Any, label: str = "document") -> None:
    if isinstance(value, str):
        if PRIVATE_RE.search(value):
            raise ThumbnailError(f"{label} contains an absolute or private path: {value}")
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
        raise ThumbnailError("Workspace must use SchemaVersion 1.")
    workspace_root = resolve_document_path(str(workspace.get("RootPath", "")), workspace_path.parent)
    active_profile_id = require_id(workspace.get("ActiveGameProfileId"), "ActiveGameProfileId")
    profiles = workspace.get("GameProfiles")
    if not isinstance(profiles, list):
        raise ThumbnailError("Workspace GameProfiles must be an array.")
    matches = [entry for entry in profiles if isinstance(entry, dict) and entry.get("ProfileId") == active_profile_id]
    if len(matches) != 1:
        raise ThumbnailError("Workspace ActiveGameProfileId must bind to exactly one profile.")
    profile = matches[0]
    runtime = profile.get("RuntimeTarget")
    if runtime not in {"Mono", "IL2CPP"}:
        raise ThumbnailError("RuntimeTarget must be Mono or IL2CPP.")
    install = resolve_document_path(str(profile.get("InstallPath", "")), workspace_path.parent)
    extracted = resolve_document_path(str(profile.get("ExtractedDataPath", "")), workspace_path.parent)
    if not install.is_dir():
        raise ThumbnailError("Configured FoA install path does not exist or is not a directory.")
    if not is_relative_to(extracted, workspace_root):
        raise ThumbnailError("ExtractedDataPath must remain inside workspace root.")
    return {
        "ProfileId": require_id(profile.get("ProfileId"), "ProfileId"),
        "GameVersion": str(profile.get("GameVersion", "")),
        "Branch": str(profile.get("Branch", "")),
        "RuntimeTarget": runtime,
        "InstallPath": install,
        "ExtractedDataPath": extracted,
    }


def validate_index(index: Mapping[str, Any], profile: Mapping[str, Any]) -> None:
    if index.get("SchemaVersion") != 1 or index.get("DocumentKind") != INDEX_KIND:
        raise ThumbnailError("Input is not a FoA visual asset discovery index.")
    for key in ("ProfileId", "GameVersion", "Branch", "RuntimeTarget"):
        if index.get(key) != profile[key]:
            raise ThumbnailError("Index must match the exact active workspace profile.")
    gate = index.get("PreviewGateStatus")
    if not isinstance(gate, dict) or gate.get("FunctionCompleteAllowed") is not False:
        raise ThumbnailError("Input index must keep FunctionCompleteAllowed=false.")
    authority = index.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise ThumbnailError("Index OperationalAuthority must be present.")
    for key in (
        "RuntimeInvocationAllowed",
        "GameMutationAllowed",
        "SaveAccessAllowed",
        "CatalogPromotionAllowed",
        "RuntimePermissionGranted",
        "PreviewProductGenerated",
        "O3deAssetProcessorInvoked",
        "UnityInvoked",
        "PayloadCopied",
    ):
        if authority.get(key) is not False:
            raise ThumbnailError(f"Index authority escalation: {key}")
    records = index.get("AssetRecords")
    if not isinstance(records, list):
        raise ThumbnailError("Index AssetRecords must be an array.")
    assert_no_private_paths(index, "index")


def token_to_install_path(token: str, install_root: Path) -> Path:
    if not isinstance(token, str) or not INSTALL_TOKEN_RE.match(token):
        raise ThumbnailError(f"Invalid $install token path: {token}")
    suffix = token[len("$install"):].lstrip("/")
    path = (install_root / suffix).resolve(strict=False)
    if not is_relative_to(path, install_root):
        raise ThumbnailError("Native asset ref escaped install root.")
    return path


def preview_token(path: Path, preview_root: Path) -> str:
    relative = path.resolve(strict=False).relative_to(preview_root.resolve(strict=False))
    if any(part in {"", ".", ".."} for part in relative.parts):
        raise ThumbnailError("Preview artefact path contains unsafe segment.")
    return "$preview/" + relative.as_posix()


def extension_to_artifact_suffix(extension: str) -> str:
    return ".jpg" if extension == ".jpeg" else extension


def copy_local_icon(source: Path, destination: Path) -> dict[str, Any]:
    try:
        stat = source.stat()
    except OSError as exc:
        raise ThumbnailError(f"Unable to stat native icon source: {source}: {exc}") from exc
    if stat.st_size > MAX_THUMBNAIL_BYTES:
        raise ThumbnailError(f"Native icon source exceeds {MAX_THUMBNAIL_BYTES} bytes: {source}")
    payload = source.read_bytes()
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    temporary.write_bytes(payload)
    os.replace(temporary, destination)
    return {"ArtifactSha256": sha256_bytes(payload), "ArtifactByteSize": len(payload)}


def issue(code: str, severity: str, message: str, locator: str) -> dict[str, Any]:
    return {
        "IssueId": "issue.thumbnail." + hashlib.sha256(canonical_json([code, message, locator])).hexdigest()[:16],
        "Code": code,
        "Severity": severity,
        "Message": message,
        "Locator": locator,
    }


def unsupported_artifact(record: Mapping[str, Any], captured_at: str, *, reason: str) -> dict[str, Any]:
    asset_record_id = require_id(record.get("AssetRecordId"), "AssetRecordId")
    return {
        "ThumbnailArtifactId": f"thumbnail.{asset_record_id.removeprefix('visual.asset.')}.unsupported",
        "AssetRecordId": asset_record_id,
        "NativeAssetRef": record.get("NativeAssetRef", ""),
        "SourceSha256": record.get("Sha256", ""),
        "ArtifactPath": "",
        "ArtifactKind": "native-icon-thumbnail",
        "ArtifactExtension": str(record.get("Extension", "")).lower(),
        "GenerationMethod": "unsupported-receipt",
        "Fidelity": "unsupported",
        "Status": "unsupported",
        "LocalOnly": True,
        "RedistributionAllowed": False,
        "RepositoryCommitAllowed": False,
        "PreviewProductGenerated": False,
        "O3deAssetProcessorInvoked": False,
        "UnityInvoked": False,
        "RuntimePermissionGranted": False,
        "CapturedAt": captured_at,
        "Reason": reason,
    }


def build_artifacts(workspace_path: Path, index_path: Path, *, preview_root: Path | None = None, captured_at: str | None = None) -> dict[str, Any]:
    profile = load_profile(workspace_path)
    index = read_json(index_path)
    validate_index(index, profile)
    if not is_relative_to(index_path.resolve(strict=False), profile["ExtractedDataPath"]):
        raise ThumbnailError("Index file must remain inside ExtractedDataPath.")
    captured_at = require_utc(captured_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "CapturedAt")
    preview_root = (preview_root.resolve(strict=False) if preview_root is not None else (profile["ExtractedDataPath"] / "PreviewArtifacts" / "Thumbnails").resolve(strict=False))
    if not is_relative_to(preview_root, profile["ExtractedDataPath"]):
        raise ThumbnailError("Preview output root must remain inside ExtractedDataPath.")
    preview_root.mkdir(parents=True, exist_ok=True)
    artifacts: list[dict[str, Any]] = []
    issues: list[dict[str, Any]] = []
    for record in index.get("AssetRecords", []):
        if not isinstance(record, dict):
            issues.append(issue("malformed-asset-record", "error", "AssetRecords entry is not an object.", ""))
            continue
        asset_record_id = require_id(record.get("AssetRecordId"), "AssetRecordId")
        native_ref = record.get("NativeAssetRef")
        extension = str(record.get("Extension", "")).lower()
        eligibility = record.get("PreviewEligibility", {})
        thumbnail_candidate = isinstance(eligibility, dict) and eligibility.get("ThumbnailCandidate") is True
        if not thumbnail_candidate:
            continue
        if extension not in SUPPORTED_THUMBNAIL_EXTENSIONS | UNSUPPORTED_TEXTURE_EXTENSIONS:
            issues.append(issue("unsupported-thumbnail-extension", "warning", f"{asset_record_id} uses unsupported extension {extension}.", str(native_ref)))
            continue
        source = token_to_install_path(str(native_ref), profile["InstallPath"])
        if not source.is_file():
            issues.append(issue("missing-thumbnail-source", "error", f"{asset_record_id} source file is missing.", str(native_ref)))
            continue
        if extension in UNSUPPORTED_TEXTURE_EXTENSIONS:
            artifacts.append(unsupported_artifact(record, captured_at, reason=f"{extension} thumbnail decode is not implemented in this Alpha slice."))
            continue
        suffix = extension_to_artifact_suffix(extension)
        artifact_id = f"thumbnail.{asset_record_id.removeprefix('visual.asset.')}"
        destination = preview_root / f"{artifact_id}{suffix}"
        written = copy_local_icon(source, destination)
        artifacts.append({
            "ThumbnailArtifactId": artifact_id,
            "AssetRecordId": asset_record_id,
            "NativeAssetRef": native_ref,
            "SourceIndexId": index["IndexId"],
            "SourceSha256": record.get("Sha256", ""),
            "ArtifactPath": preview_token(destination, preview_root),
            "ArtifactKind": "native-icon-thumbnail",
            "ArtifactExtension": suffix,
            "GenerationMethod": "local-only-loose-icon-copy",
            "Fidelity": "native-icon-byte-preserved",
            "Status": "generated",
            "LocalOnly": True,
            "RedistributionAllowed": False,
            "RepositoryCommitAllowed": False,
            "PreviewProductGenerated": False,
            "O3deAssetProcessorInvoked": False,
            "UnityInvoked": False,
            "RuntimePermissionGranted": False,
            "CapturedAt": captured_at,
            **written,
        })
    if len(artifacts) > MAX_ARTIFACTS:
        raise ThumbnailError(f"Thumbnail artefact count exceeds {MAX_ARTIFACTS}.")
    artifacts.sort(key=lambda item: item["ThumbnailArtifactId"])
    manifest_id_seed = canonical_json({"IndexId": index["IndexId"], "Artifacts": [(item["ThumbnailArtifactId"], item.get("ArtifactSha256", "")) for item in artifacts], "CapturedAt": captured_at})
    manifest = {
        "SchemaVersion": 1,
        "DocumentKind": DOCUMENT_KIND,
        "ManifestId": "thumbnail.manifest." + hashlib.sha256(manifest_id_seed).hexdigest()[:16],
        "SourceIndexId": index["IndexId"],
        "ProfileId": profile["ProfileId"],
        "GameVersion": profile["GameVersion"],
        "Branch": profile["Branch"],
        "RuntimeTarget": profile["RuntimeTarget"],
        "ToolId": TOOL_ID,
        "ToolVersion": TOOL_VERSION,
        "CapturedAt": captured_at,
        "PreviewRoot": "$preview",
        "PreviewStageStatus": {"DiscoveryIndexConsumed": True, "LocalPreviewArtifactsEmitted": bool(artifacts), "GeneratedO3dePreviewProduct": False, "TypedAuthoringBindingCreated": False, "FunctionCompleteAllowed": False, "NextRequiredStages": ["unity-to-neutral-preview-handoff", "neutral-to-o3de-preview-conversion", "asset-browser-pane", "item-recipe-visual-selectors"]},
        "ThumbnailArtifacts": artifacts,
        "Issues": issues,
        "OperationalAuthority": {key: False for key in AUTHORITY_FALSE_KEYS},
    }
    assert_no_private_paths(manifest)
    return manifest


def write_manifest(manifest: Mapping[str, Any], manifest_path: Path, *, replace: bool = False) -> None:
    if manifest_path.exists() and not replace:
        raise ThumbnailError(f"Manifest already exists: {manifest_path}")
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = manifest_path.with_suffix(manifest_path.suffix + ".tmp")
    temporary.write_bytes(pretty_json(manifest))
    os.replace(temporary, manifest_path)


def verify_manifest(manifest_path: Path, *, workspace_path: Path | None = None, index_path: Path | None = None, preview_root: Path | None = None) -> dict[str, Any]:
    manifest = read_json(manifest_path)
    if manifest.get("SchemaVersion") != 1 or manifest.get("DocumentKind") != DOCUMENT_KIND:
        raise ThumbnailError("Input is not a thumbnail artefact evidence manifest.")
    require_utc(manifest.get("CapturedAt"), "CapturedAt")
    require_id(manifest.get("ManifestId"), "ManifestId")
    authority = manifest.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise ThumbnailError("OperationalAuthority is required.")
    for key in AUTHORITY_FALSE_KEYS:
        if authority.get(key) is not False:
            raise ThumbnailError(f"Manifest authority escalation: {key}")
    stage = manifest.get("PreviewStageStatus")
    if not isinstance(stage, dict) or stage.get("FunctionCompleteAllowed") is not False:
        raise ThumbnailError("PreviewStageStatus must keep FunctionCompleteAllowed=false.")
    artifacts = manifest.get("ThumbnailArtifacts")
    if not isinstance(artifacts, list):
        raise ThumbnailError("ThumbnailArtifacts must be an array.")
    seen: set[str] = set()
    root = preview_root.resolve(strict=False) if preview_root is not None else manifest_path.parent.resolve(strict=False)
    for artifact in artifacts:
        if not isinstance(artifact, dict):
            raise ThumbnailError("ThumbnailArtifacts entries must be objects.")
        artifact_id = require_id(artifact.get("ThumbnailArtifactId"), "ThumbnailArtifactId")
        if artifact_id in seen:
            raise ThumbnailError("Duplicate thumbnail artefact identity.")
        seen.add(artifact_id)
        for key in ("RedistributionAllowed", "RepositoryCommitAllowed", "PreviewProductGenerated", "O3deAssetProcessorInvoked", "UnityInvoked", "RuntimePermissionGranted"):
            if artifact.get(key) is not False:
                raise ThumbnailError(f"Artifact authority escalation: {key}")
        path_token = artifact.get("ArtifactPath", "")
        if artifact.get("Status") == "generated":
            if not isinstance(path_token, str) or not PREVIEW_TOKEN_RE.match(path_token):
                raise ThumbnailError("Generated artefacts require $preview ArtifactPath.")
            artifact_file = root / path_token[len("$preview/"):]
            if not artifact_file.is_file():
                raise ThumbnailError(f"Thumbnail artefact payload missing: {path_token}")
            payload = artifact_file.read_bytes()
            if artifact.get("ArtifactByteSize") != len(payload):
                raise ThumbnailError(f"Thumbnail artefact size mismatch: {path_token}")
            if artifact.get("ArtifactSha256") != sha256_bytes(payload):
                raise ThumbnailError(f"Thumbnail artefact SHA-256 mismatch: {path_token}")
        elif artifact.get("Status") == "unsupported":
            if path_token:
                raise ThumbnailError("Unsupported artefacts must not claim an ArtifactPath.")
        else:
            raise ThumbnailError("Thumbnail artefact has unsupported Status.")
    if workspace_path is not None:
        profile = load_profile(workspace_path)
        for key in ("ProfileId", "GameVersion", "Branch", "RuntimeTarget"):
            if manifest.get(key) != profile[key]:
                raise ThumbnailError("Manifest must match the exact active workspace profile.")
    if index_path is not None:
        index = read_json(index_path)
        if manifest.get("SourceIndexId") != index.get("IndexId"):
            raise ThumbnailError("Manifest SourceIndexId must match input index.")
    assert_no_private_paths(manifest)
    return manifest


def default_paths(workspace_path: Path) -> tuple[Path, Path, Path]:
    profile = load_profile(workspace_path)
    preview_root = profile["ExtractedDataPath"] / "PreviewArtifacts" / "Thumbnails"
    return (profile["ExtractedDataPath"] / DEFAULT_INDEX_NAME, preview_root, preview_root / DEFAULT_MANIFEST_NAME)


def synthetic_index(workspace_path: Path) -> dict[str, Any]:
    profile = load_profile(workspace_path)
    records = []
    for ordinal, relative in enumerate(("Tainted Grail_Data/LooseIcons/iron.png", "Tainted Grail_Data/LooseIcons/ore.tga")):
        source = profile["InstallPath"] / relative
        payload = source.read_bytes()
        extension = source.suffix.lower()
        record_id = f"visual.asset.{profile['ProfileId']}.{hashlib.sha256(canonical_json([relative, sha256_bytes(payload)])).hexdigest()[:16]}"
        records.append({"AssetRecordId": record_id, "NativeAssetRef": "$install/" + relative, "ProfileId": profile["ProfileId"], "GameVersion": profile["GameVersion"], "Branch": profile["Branch"], "RuntimeTarget": profile["RuntimeTarget"], "Locator": "$install/" + relative, "FileName": source.name, "Extension": extension, "FileKind": "loose-texture", "ByteSize": len(payload), "Sha256": sha256_bytes(payload), "FingerprintStatus": "hashed", "PreviewEligibility": {"ThumbnailCandidate": True, "StaticPreviewCandidate": False, "RequiresExtraction": False, "Reason": "thumbnail candidate"}, "EvidenceKind": "visual-asset-discovery", "Confidence": "observed", "DiscoveryOrdinal": ordinal, "CatalogPromotionAllowed": False, "RuntimePermissionGranted": False, "PreviewProductGenerated": False})
    return {"SchemaVersion": 1, "DocumentKind": INDEX_KIND, "IndexId": "visual.index.foa.mono.fixture.synthetic", "ProfileId": profile["ProfileId"], "GameVersion": profile["GameVersion"], "Branch": profile["Branch"], "RuntimeTarget": profile["RuntimeTarget"], "ToolId": "foa.visual-asset-discovery-index", "ToolVersion": "0.1.0", "CapturedAt": "2026-07-28T00:00:00Z", "InstallRoot": "$install", "OutputRoot": "$extracted", "DiscoveryScope": {"ConfiguredInstallRootOnly": True, "FileContentCopyAllowed": False, "AssemblyLoadAllowed": False, "RuntimeInvocationAllowed": False}, "PreviewGateStatus": {"VisualPreviewGateRequired": True, "FunctionCompleteAllowed": False, "Stage": "alpha.discovery-index"}, "AssetRecords": records, "Issues": [], "OperationalAuthority": {"RuntimeInvocationAllowed": False, "GameMutationAllowed": False, "SaveAccessAllowed": False, "CatalogPromotionAllowed": False, "RuntimePermissionGranted": False, "PreviewProductGenerated": False, "O3deAssetProcessorInvoked": False, "UnityInvoked": False, "PayloadCopied": False}}


def generate_fixture(output: Path, *, replace: bool = False) -> dict[str, Any]:
    if output.exists():
        if replace:
            shutil.rmtree(output) if output.is_dir() else output.unlink()
        else:
            raise ThumbnailError(f"Fixture output is not empty: {output}")
    install = output / "game" / "FoA"
    icons = install / "Tainted Grail_Data" / "LooseIcons"
    extracted = output / "workspace" / "Extracted"
    icons.mkdir(parents=True, exist_ok=True)
    extracted.mkdir(parents=True, exist_ok=True)
    (icons / "iron.png").write_bytes(b"synthetic-png")
    (icons / "ore.tga").write_bytes(b"synthetic-tga")
    workspace = {"SchemaVersion": 1, "WorkspaceId": "fixture.workspace", "DisplayName": "Thumbnail Fixture", "RootPath": "./workspace", "OutputPath": "./workspace/Build", "StagingPath": "./workspace/Staging", "DeploymentPath": "./workspace/Deploy", "ActiveGameProfileId": "foa.mono.fixture", "GameProfiles": [{"ProfileId": "foa.mono.fixture", "DisplayName": "FoA Mono Fixture", "InstallPath": "./game/FoA", "GameVersion": "1.23.401", "Branch": "mono", "RuntimeTarget": "Mono", "UnityVersion": "6000.0.64f1", "BepInExVersion": "5.4.23.3", "ManagedAssembliesPath": "", "PluginPath": "", "DiagnosticsPath": "./workspace/Diagnostics", "ExtractedDataPath": "./workspace/Extracted", "DlcScopes": ["base-game"]}]}
    workspace_path = output / "workspace.tgworkspace.json"
    workspace_path.write_bytes(pretty_json(workspace))
    index_path = extracted / DEFAULT_INDEX_NAME
    index_path.write_bytes(pretty_json(synthetic_index(workspace_path)))
    preview_root = extracted / "PreviewArtifacts" / "Thumbnails"
    manifest = build_artifacts(workspace_path, index_path, preview_root=preview_root, captured_at="2026-07-28T00:00:01Z")
    manifest_path = preview_root / DEFAULT_MANIFEST_NAME
    write_manifest(manifest, manifest_path, replace=True)
    verify_manifest(manifest_path, workspace_path=workspace_path, index_path=index_path, preview_root=preview_root)
    return {"ManifestId": manifest["ManifestId"], "GeneratedCount": len([item for item in manifest["ThumbnailArtifacts"] if item["Status"] == "generated"]), "UnsupportedCount": len([item for item in manifest["ThumbnailArtifacts"] if item["Status"] == "unsupported"]), "ManifestPath": str(manifest_path)}


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Extract local-only native icon thumbnail artefacts from a visual asset index.")
    sub = parser.add_subparsers(dest="command", required=True)
    extract = sub.add_parser("extract")
    extract.add_argument("--workspace", required=True, type=Path)
    extract.add_argument("--index", type=Path)
    extract.add_argument("--preview-root", type=Path)
    extract.add_argument("--manifest", type=Path)
    extract.add_argument("--captured-at")
    extract.add_argument("--replace", action="store_true")
    verify = sub.add_parser("verify")
    verify.add_argument("--manifest", required=True, type=Path)
    verify.add_argument("--workspace", type=Path)
    verify.add_argument("--index", type=Path)
    verify.add_argument("--preview-root", type=Path)
    fixture = sub.add_parser("fixture")
    fixture.add_argument("--output", required=True, type=Path)
    fixture.add_argument("--replace", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.command == "fixture":
            manifest = generate_fixture(args.output, replace=args.replace)
            print(f"FoA thumbnail artefact fixture wrote {manifest['GeneratedCount']} generated and {manifest['UnsupportedCount']} unsupported artefacts.")
        elif args.command == "extract":
            default_index, default_preview_root, default_manifest = default_paths(args.workspace)
            index_path = args.index or default_index
            preview_root = args.preview_root or default_preview_root
            manifest_path = args.manifest or default_manifest
            manifest = build_artifacts(args.workspace, index_path, preview_root=preview_root, captured_at=args.captured_at)
            write_manifest(manifest, manifest_path, replace=args.replace)
            print(f"FoA thumbnail artefact manifest wrote {len(manifest['ThumbnailArtifacts'])} artefacts to {manifest_path}.")
        elif args.command == "verify":
            manifest = verify_manifest(args.manifest, workspace_path=args.workspace, index_path=args.index, preview_root=args.preview_root)
            print(f"FoA thumbnail artefact manifest verified: {manifest['ManifestId']} with {len(manifest['ThumbnailArtifacts'])} artefacts.")
        else:
            raise ThumbnailError(f"Unknown command: {args.command}")
    except ThumbnailError as exc:
        print(f"FoA thumbnail artefact extraction failed: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
