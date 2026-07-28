#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Neutral-to-O3DE preview conversion staging for FOA-SDK.

This consumes a neutral preview handoff and emits local-only O3DE preview source
staging plus evidence. It does not invoke O3DE Asset Processor, generate O3DE
product assets, mutate catalogues, grant runtime permission, deploy, sign, or
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

TOOL_ID = "foa.o3de-preview-conversion"
TOOL_VERSION = "0.1.0"
DOCUMENT_KIND = "foa-o3de-preview-conversion"
HANDOFF_KIND = "foa-neutral-preview-handoff"
DEFAULT_CONVERSION_NAME = "foa-o3de-preview-conversion.json"
MAX_SOURCE_BYTES = 32 * 1024 * 1024

AUTHORITY_FALSE_KEYS = (
    "RuntimeInvocationAllowed",
    "GameMutationAllowed",
    "SaveAccessAllowed",
    "CatalogPromotionAllowed",
    "RuntimePermissionGranted",
    "UnityInvoked",
    "O3deAssetProcessorInvoked",
    "GeneratedO3dePreviewProduct",
    "AssetBrowserEntryCreated",
    "TypedAuthoringBindingCreated",
    "DeploymentAllowed",
    "RepositoryCommitAllowed",
    "RedistributionAllowed",
    "FunctionCompleteAllowed",
)

ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
PRIVATE_RE = re.compile(r"(^/|^~/|^[A-Za-z]:[\\/]|^\\\\)")
HANDOFF_TOKEN_RE = re.compile(r"^\$handoff(/[^\\\r\n]*)?$")
O3DE_TOKEN_RE = re.compile(r"^\$o3depreview(/[^\\\r\n]*)?$")
SHA_RE = re.compile(r"^sha256:[0-9a-f]{64}$")


class O3dePreviewConversionError(RuntimeError):
    pass


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
        raise O3dePreviewConversionError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise O3dePreviewConversionError(f"JSON document must be an object: {path}")
    return value


def require_id(value: Any, label: str) -> str:
    if not isinstance(value, str) or not ID_RE.match(value):
        raise O3dePreviewConversionError(f"{label} must be a lowercase stable identifier.")
    return value


def require_utc(value: Any, label: str) -> str:
    if not isinstance(value, str) or not UTC_RE.match(value):
        raise O3dePreviewConversionError(f"{label} must use whole-second UTC format.")
    datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    return value


def require_sha(value: Any, label: str) -> str:
    if not isinstance(value, str) or not SHA_RE.match(value):
        raise O3dePreviewConversionError(f"{label} must be sha256:<64-hex>.")
    return value


def assert_no_private_paths(value: Any, label: str = "document") -> None:
    if isinstance(value, str):
        if PRIVATE_RE.search(value):
            raise O3dePreviewConversionError(f"{label} contains an absolute or private path: {value}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            assert_no_private_paths(child, f"{label}[{index}]")
    elif isinstance(value, dict):
        for key, child in value.items():
            assert_no_private_paths(child, f"{label}.{key}")


def resolve_path(raw: str, base: Path) -> Path:
    path = Path(raw).expanduser()
    return (base / path if not path.is_absolute() else path).resolve(strict=False)


def inside(path: Path, root: Path) -> bool:
    try:
        path.resolve(strict=False).relative_to(root.resolve(strict=False))
        return True
    except ValueError:
        return False


def load_profile(workspace_path: Path) -> dict[str, Any]:
    workspace = read_json(workspace_path)
    if workspace.get("SchemaVersion") != 1:
        raise O3dePreviewConversionError("Workspace must use SchemaVersion 1.")
    root = resolve_path(str(workspace.get("RootPath", "")), workspace_path.parent)
    active = require_id(workspace.get("ActiveGameProfileId"), "ActiveGameProfileId")
    matches = [p for p in workspace.get("GameProfiles", []) if isinstance(p, dict) and p.get("ProfileId") == active]
    if len(matches) != 1:
        raise O3dePreviewConversionError("Workspace ActiveGameProfileId must bind to exactly one profile.")
    profile = matches[0]
    runtime = profile.get("RuntimeTarget")
    if runtime not in {"Mono", "IL2CPP"}:
        raise O3dePreviewConversionError("RuntimeTarget must be Mono or IL2CPP.")
    install = resolve_path(str(profile.get("InstallPath", "")), workspace_path.parent)
    extracted = resolve_path(str(profile.get("ExtractedDataPath", "")), workspace_path.parent)
    if not install.is_dir():
        raise O3dePreviewConversionError("Configured FoA install path does not exist or is not a directory.")
    if not inside(extracted, root):
        raise O3dePreviewConversionError("ExtractedDataPath must remain inside workspace root.")
    return {
        "ProfileId": require_id(profile.get("ProfileId"), "ProfileId"),
        "GameVersion": str(profile.get("GameVersion", "")),
        "Branch": str(profile.get("Branch", "")),
        "RuntimeTarget": runtime,
        "InstallPath": install,
        "ExtractedDataPath": extracted,
    }


def profile_bound(document: Mapping[str, Any], profile: Mapping[str, Any], label: str) -> None:
    for key in ("ProfileId", "GameVersion", "Branch", "RuntimeTarget"):
        if document.get(key) != profile[key]:
            raise O3dePreviewConversionError(f"{label} must match the exact active workspace profile.")


def false_authority(authority: Mapping[str, Any], label: str) -> None:
    for key in AUTHORITY_FALSE_KEYS:
        if key in authority and authority.get(key) is not False:
            raise O3dePreviewConversionError(f"{label} authority escalation: {key}")


def validate_handoff(handoff: Mapping[str, Any], profile: Mapping[str, Any]) -> tuple[list[Mapping[str, Any]], dict[str, Mapping[str, Any]]]:
    if handoff.get("SchemaVersion") != 1 or handoff.get("DocumentKind") != HANDOFF_KIND:
        raise O3dePreviewConversionError("Input is not a neutral preview handoff.")
    if "TransformVerified" in handoff:
        raise O3dePreviewConversionError("Top-level TransformVerified is forbidden; use CoordinateConversionEvidence.")
    profile_bound(handoff, profile, "Neutral handoff")
    stage = handoff.get("PreviewStageStatus")
    if not isinstance(stage, dict) or stage.get("FunctionCompleteAllowed") is not False:
        raise O3dePreviewConversionError("Neutral handoff must keep FunctionCompleteAllowed=false.")
    if stage.get("NeutralPreviewHandoffEmitted") is not True:
        raise O3dePreviewConversionError("Neutral handoff must indicate NeutralPreviewHandoffEmitted=true.")
    authority = handoff.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise O3dePreviewConversionError("Neutral handoff OperationalAuthority is required.")
    false_authority(authority, "Neutral handoff")
    if not isinstance(handoff.get("CoordinateDeclaration"), dict) or not isinstance(handoff.get("CoordinateConversionEvidence"), dict):
        raise O3dePreviewConversionError("CoordinateDeclaration and CoordinateConversionEvidence are required.")
    payloads = handoff.get("Payloads")
    entries = handoff.get("PreviewEntries")
    if not isinstance(payloads, list) or not isinstance(entries, list) or not payloads or not entries:
        raise O3dePreviewConversionError("Neutral handoff requires non-empty Payloads and PreviewEntries.")
    payload_map: dict[str, Mapping[str, Any]] = {}
    for payload in payloads:
        if not isinstance(payload, dict):
            raise O3dePreviewConversionError("Payload entries must be objects.")
        payload_id = require_id(payload.get("PayloadId"), "PayloadId")
        if payload_id in payload_map:
            raise O3dePreviewConversionError("Duplicate PayloadId in handoff.")
        if not isinstance(payload.get("Path"), str) or not HANDOFF_TOKEN_RE.match(payload["Path"]):
            raise O3dePreviewConversionError("Payload Path must use $handoff token.")
        require_sha(payload.get("Sha256"), "Payload Sha256")
        if payload.get("RepositoryCommitAllowed") is not False or payload.get("RedistributionAllowed") is not False:
            raise O3dePreviewConversionError("Handoff payload must remain local-only.")
        payload_map[payload_id] = payload
    for entry in entries:
        if not isinstance(entry, dict):
            raise O3dePreviewConversionError("PreviewEntries must be objects.")
        require_id(entry.get("PreviewEntryId"), "PreviewEntryId")
        require_id(entry.get("PrimarySourceAssetRecordId"), "PrimarySourceAssetRecordId")
        if not isinstance(entry.get("SourceDependencies"), list) or not entry.get("SourceDependencies"):
            raise O3dePreviewConversionError("PreviewEntries require SourceDependencies.")
        for ref in entry.get("PayloadRefs", []):
            require_id(ref, "PreviewEntry PayloadRef")
            if ref not in payload_map:
                raise O3dePreviewConversionError("PreviewEntry references missing handoff payload.")
        if entry.get("GeneratedO3dePreviewProduct") is not False:
            raise O3dePreviewConversionError("Handoff entry must not claim generated O3DE product.")
    assert_no_private_paths(handoff, "handoff")
    return entries, payload_map


def token_path(token: str, root: Path, token_name: str) -> Path:
    prefix = token_name + "/"
    suffix = token[len(prefix):]
    path = (root / suffix).resolve(strict=False)
    if not inside(path, root):
        raise O3dePreviewConversionError(f"{token_name} path escaped root.")
    return path


def o3de_token(path: Path, root: Path) -> str:
    rel = path.resolve(strict=False).relative_to(root.resolve(strict=False))
    if any(part in {"", ".", ".."} for part in rel.parts):
        raise O3dePreviewConversionError("O3DE preview source path contains unsafe segment.")
    return "$o3depreview/" + rel.as_posix()


def source_subdir(payload: Mapping[str, Any]) -> str:
    if payload.get("Role") == "thumbnail":
        return "SourceAssets/Textures"
    if payload.get("MediaType") == "application/json":
        return "SourceAssets/Metadata"
    return "SourceAssets/Other"


def source_kind(payload: Mapping[str, Any]) -> str:
    media = str(payload.get("MediaType", ""))
    if media.startswith("image/"):
        return "texture-preview-source"
    if media == "application/json":
        return "metadata-preview-source"
    return "generic-preview-source"


def copy_payload(source: Path, destination: Path) -> dict[str, Any]:
    if not source.is_file():
        raise O3dePreviewConversionError(f"Handoff payload is missing: {source}")
    data = source.read_bytes()
    if len(data) > MAX_SOURCE_BYTES:
        raise O3dePreviewConversionError(f"Handoff payload exceeds {MAX_SOURCE_BYTES} bytes: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    tmp = destination.with_suffix(destination.suffix + ".tmp")
    tmp.write_bytes(data)
    os.replace(tmp, destination)
    return {"Sha256": sha256_bytes(data), "ByteSize": len(data)}


def coordinate_evidence(handoff: Mapping[str, Any]) -> dict[str, Any]:
    return {
        "SourceCoordinateDeclaration": handoff.get("CoordinateDeclaration", {}),
        "PriorCoordinateConversionEvidence": handoff.get("CoordinateConversionEvidence", {}),
        "ConversionToolId": TOOL_ID,
        "ConversionToolVersion": TOOL_VERSION,
        "TransformPolicyId": "neutral-payload-to-o3de-preview-source-staging",
        "CoordinateTransformApplied": False,
        "PreviewSourceStagingPerformed": True,
        "ConversionMatrix": [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]],
        "VerificationState": "not-verified",
        "VerificationEvidenceIds": [],
        "VerificationEvidenceRequired": True,
    }


def build_conversion(workspace_path: Path, handoff_path: Path, *, output_root: Path | None = None, captured_at: str | None = None, replace: bool = False) -> tuple[dict[str, Any], Path]:
    profile = load_profile(workspace_path)
    handoff = read_json(handoff_path)
    entries, payload_map = validate_handoff(handoff, profile)
    if not inside(handoff_path.resolve(strict=False), profile["ExtractedDataPath"]):
        raise O3dePreviewConversionError("Neutral handoff must remain inside ExtractedDataPath.")
    captured_at = require_utc(captured_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "CapturedAt")
    seed = canonical_json({"ProfileId": profile["ProfileId"], "HandoffId": handoff["HandoffId"], "Payloads": [(p["PayloadId"], p["Sha256"]) for p in payload_map.values()], "CapturedAt": captured_at})
    conversion_id = "o3de.preview." + profile["ProfileId"] + "." + hashlib.sha256(seed).hexdigest()[:16]
    root = (output_root.resolve(strict=False) if output_root else (profile["ExtractedDataPath"] / "PreviewArtifacts" / "O3DE" / conversion_id).resolve(strict=False))
    if not inside(root, profile["ExtractedDataPath"]):
        raise O3dePreviewConversionError("O3DE preview output root must remain inside ExtractedDataPath.")
    if root.exists() and any(root.iterdir()):
        if replace:
            shutil.rmtree(root)
        else:
            raise O3dePreviewConversionError(f"O3DE preview output root is not empty: {root}")
    root.mkdir(parents=True, exist_ok=True)
    sources: list[dict[str, Any]] = []
    evidence: list[dict[str, Any]] = []
    for entry in entries:
        for ref in entry.get("PayloadRefs", []):
            payload = payload_map[ref]
            source_file = token_path(payload["Path"], handoff_path.parent, "$handoff")
            destination = root / source_subdir(payload) / source_file.name
            written = copy_payload(source_file, destination)
            if written["Sha256"] != payload["Sha256"]:
                raise O3dePreviewConversionError("Copied O3DE preview source does not match handoff payload hash.")
            source_id = "o3de.source." + hashlib.sha256(canonical_json([conversion_id, entry["PreviewEntryId"], ref, written["Sha256"]])).hexdigest()[:16]
            evidence_id = "o3de.product-evidence." + hashlib.sha256(canonical_json([source_id, "not-invoked"])).hexdigest()[:16]
            sources.append({
                "O3dePreviewSourceId": source_id,
                "PreviewEntryId": entry["PreviewEntryId"],
                "PrimarySourceAssetRecordId": entry["PrimarySourceAssetRecordId"],
                "SourceDependencies": entry.get("SourceDependencies", []),
                "SourceHandoffPayloadId": ref,
                "SourceHandoffPayloadSha256": payload["Sha256"],
                "PreviewSourcePath": o3de_token(destination, root),
                "PreviewSourceKind": source_kind(payload),
                "MediaType": payload.get("MediaType", "application/octet-stream"),
                "Sha256": written["Sha256"],
                "ByteSize": written["ByteSize"],
                "GeneratedFromNeutralHandoff": True,
                "LocalOnly": True,
                "RepositoryCommitAllowed": False,
                "RedistributionAllowed": False,
                "O3deAssetProcessorInvoked": False,
                "GeneratedO3dePreviewProduct": False,
                "RuntimePermissionGranted": False,
                "ProductEvidenceId": evidence_id,
            })
            evidence.append({
                "ProductEvidenceId": evidence_id,
                "O3dePreviewSourceId": source_id,
                "EvidenceState": "asset-processor-not-invoked",
                "O3deAssetProcessorInvoked": False,
                "GeneratedO3dePreviewProduct": False,
                "ProductAssetIds": [],
                "ProductCachePaths": [],
                "ImportLogPath": "",
                "PreviewRenderVerified": False,
                "VerificationEvidenceIds": [],
                "NextRequiredAction": "run-bounded-o3de-asset-processor-import-proof",
            })
    if not sources:
        raise O3dePreviewConversionError("At least one O3DE preview source must be staged.")
    manifest = {
        "SchemaVersion": 1,
        "DocumentKind": DOCUMENT_KIND,
        "ConversionId": conversion_id,
        "ProfileId": profile["ProfileId"],
        "GameVersion": profile["GameVersion"],
        "Branch": profile["Branch"],
        "RuntimeTarget": profile["RuntimeTarget"],
        "ToolId": TOOL_ID,
        "ToolVersion": TOOL_VERSION,
        "CapturedAt": captured_at,
        "PreviewIntent": "editor-preview-only",
        "SourceHandoffId": handoff["HandoffId"],
        "SourceIndexId": handoff.get("SourceIndexId", ""),
        "SourceThumbnailManifestId": handoff.get("SourceThumbnailManifestId", ""),
        "PrimarySourceAssetRecordId": handoff.get("PrimarySourceAssetRecordId", ""),
        "SourceAssetRecordIds": handoff.get("SourceAssetRecordIds", []),
        "SourceDependencies": handoff.get("SourceDependencies", []),
        "PreviewStageStatus": {
            "NeutralPreviewHandoffConsumed": True,
            "GeneratedO3dePreviewSourcesEmitted": True,
            "O3deAssetProcessorInvoked": False,
            "GeneratedO3dePreviewProduct": False,
            "AssetBrowserEntryCreated": False,
            "TypedAuthoringBindingCreated": False,
            "FunctionCompleteAllowed": False,
            "NextRequiredStages": ["o3de-asset-processor-import-proof", "asset-browser-pane", "item-recipe-visual-selectors"],
        },
        "GeneratedRoot": "$o3depreview",
        "CoordinateDeclaration": handoff.get("CoordinateDeclaration", {}),
        "CoordinateConversionEvidence": coordinate_evidence(handoff),
        "PreviewEntryIds": sorted(entry["PreviewEntryId"] for entry in entries),
        "O3dePreviewSources": sorted(sources, key=lambda item: item["O3dePreviewSourceId"]),
        "O3dePreviewProductEvidence": sorted(evidence, key=lambda item: item["ProductEvidenceId"]),
        "Issues": [],
        "OperationalAuthority": {key: False for key in AUTHORITY_FALSE_KEYS},
    }
    assert_no_private_paths(manifest)
    manifest_path = root / DEFAULT_CONVERSION_NAME
    manifest_path.write_bytes(pretty_json(manifest))
    return manifest, manifest_path


def verify_conversion(manifest_path: Path, *, workspace_path: Path | None = None, handoff_path: Path | None = None, output_root: Path | None = None) -> dict[str, Any]:
    manifest = read_json(manifest_path)
    if manifest.get("SchemaVersion") != 1 or manifest.get("DocumentKind") != DOCUMENT_KIND:
        raise O3dePreviewConversionError("Input is not an O3DE preview conversion manifest.")
    if "TransformVerified" in manifest:
        raise O3dePreviewConversionError("TransformVerified must not appear at top level; use CoordinateConversionEvidence.")
    require_id(manifest.get("ConversionId"), "ConversionId")
    require_utc(manifest.get("CapturedAt"), "CapturedAt")
    stage = manifest.get("PreviewStageStatus")
    if not isinstance(stage, dict) or stage.get("FunctionCompleteAllowed") is not False:
        raise O3dePreviewConversionError("PreviewStageStatus must keep FunctionCompleteAllowed=false.")
    for key in ("O3deAssetProcessorInvoked", "GeneratedO3dePreviewProduct", "TypedAuthoringBindingCreated"):
        if stage.get(key) is not False:
            raise O3dePreviewConversionError(f"PreviewStageStatus authority escalation: {key}")
    authority = manifest.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise O3dePreviewConversionError("OperationalAuthority is required.")
    false_authority(authority, "Conversion manifest")
    coord = manifest.get("CoordinateConversionEvidence")
    if not isinstance(coord, dict) or coord.get("CoordinateTransformApplied") is not False or coord.get("VerificationState") == "verified":
        raise O3dePreviewConversionError("CoordinateConversionEvidence must remain unverified for this staging slice.")
    sources = manifest.get("O3dePreviewSources")
    product_evidence = manifest.get("O3dePreviewProductEvidence")
    if not isinstance(sources, list) or not sources or not isinstance(product_evidence, list):
        raise O3dePreviewConversionError("O3DE preview sources and product evidence are required.")
    root = output_root.resolve(strict=False) if output_root else manifest_path.parent.resolve(strict=False)
    seen: set[str] = set()
    evidence_ids = set()
    for source in sources:
        source_id = require_id(source.get("O3dePreviewSourceId"), "O3dePreviewSourceId")
        if source_id in seen:
            raise O3dePreviewConversionError("Duplicate O3DE preview source identity.")
        seen.add(source_id)
        for key in ("RepositoryCommitAllowed", "RedistributionAllowed", "O3deAssetProcessorInvoked", "GeneratedO3dePreviewProduct", "RuntimePermissionGranted"):
            if source.get(key) is not False:
                raise O3dePreviewConversionError(f"O3DE preview source authority escalation: {key}")
        token = source.get("PreviewSourcePath")
        if not isinstance(token, str) or not O3DE_TOKEN_RE.match(token):
            raise O3dePreviewConversionError("PreviewSourcePath must use $o3depreview token.")
        payload_file = root / token[len("$o3depreview/"):]
        if not payload_file.is_file():
            raise O3dePreviewConversionError(f"O3DE preview source payload missing: {token}")
        payload = payload_file.read_bytes()
        if source.get("ByteSize") != len(payload) or source.get("Sha256") != sha256_bytes(payload):
            raise O3dePreviewConversionError(f"O3DE preview source payload mismatch: {token}")
        evidence_ids.add(source.get("ProductEvidenceId"))
    for evidence in product_evidence:
        if evidence.get("ProductEvidenceId") not in evidence_ids:
            raise O3dePreviewConversionError("Product evidence must bind to staged source evidence.")
        if evidence.get("O3deAssetProcessorInvoked") is not False or evidence.get("GeneratedO3dePreviewProduct") is not False:
            raise O3dePreviewConversionError("Product evidence must not claim Asset Processor execution.")
    if workspace_path:
        profile = load_profile(workspace_path)
        profile_bound(manifest, profile, "Conversion manifest")
        if not inside(manifest_path.resolve(strict=False), profile["ExtractedDataPath"]):
            raise O3dePreviewConversionError("Conversion manifest must remain inside ExtractedDataPath.")
    if handoff_path:
        handoff = read_json(handoff_path)
        if manifest.get("SourceHandoffId") != handoff.get("HandoffId"):
            raise O3dePreviewConversionError("Conversion SourceHandoffId must match handoff input.")
    assert_no_private_paths(manifest)
    return manifest


def write_workspace(output: Path) -> Path:
    (output / "game" / "FoA").mkdir(parents=True, exist_ok=True)
    extracted = output / "workspace" / "Extracted"
    extracted.mkdir(parents=True, exist_ok=True)
    workspace = {"SchemaVersion": 1, "WorkspaceId": "fixture.workspace", "DisplayName": "O3DE Preview Fixture", "RootPath": "./workspace", "OutputPath": "./workspace/Build", "StagingPath": "./workspace/Staging", "DeploymentPath": "./workspace/Deploy", "ActiveGameProfileId": "foa.mono.fixture", "GameProfiles": [{"ProfileId": "foa.mono.fixture", "DisplayName": "FoA Mono Fixture", "InstallPath": "./game/FoA", "GameVersion": "1.23.401", "Branch": "mono", "RuntimeTarget": "Mono", "UnityVersion": "6000.0.64f1", "BepInExVersion": "5.4.23.3", "ManagedAssembliesPath": "", "PluginPath": "", "DiagnosticsPath": "./workspace/Diagnostics", "ExtractedDataPath": "./workspace/Extracted", "DlcScopes": ["base-game"]}]}
    path = output / "workspace.tgworkspace.json"
    path.write_bytes(pretty_json(workspace))
    return path


def synthetic_handoff(output: Path) -> tuple[Path, Path]:
    workspace_path = write_workspace(output)
    profile = load_profile(workspace_path)
    handoff_id = "preview.handoff.foa.mono.fixture.synthetic"
    handoff_root = profile["ExtractedDataPath"] / "PreviewArtifacts" / "Handoffs" / handoff_id
    payload_path = handoff_root / "payloads" / "thumbnails" / "thumbnail.synthetic.png"
    payload_path.parent.mkdir(parents=True, exist_ok=True)
    payload_path.write_bytes(b"synthetic-thumbnail")
    payload_hash = sha256_bytes(payload_path.read_bytes())
    asset_id = "visual.asset.foa.mono.fixture.synthetic"
    dependency = {"SourceAssetRecordId": asset_id, "NativeAssetRef": "$install/Tainted Grail_Data/LooseIcons/iron.png", "SourceIndexId": "visual.index.foa.mono.fixture.synthetic", "SourceFingerprint": sha256_bytes(b"source"), "DependencyRole": "primary", "DependencyKind": "visual-asset-discovery-record", "RequiredForPreview": True}
    payload_id = "payload.synthetic.thumbnail"
    handoff = {"SchemaVersion": 1, "DocumentKind": HANDOFF_KIND, "HandoffId": handoff_id, "ProfileId": profile["ProfileId"], "GameVersion": profile["GameVersion"], "Branch": profile["Branch"], "RuntimeTarget": profile["RuntimeTarget"], "ToolId": "foa.neutral-preview-handoff", "ToolVersion": "0.1.0", "CapturedAt": "2026-07-28T00:00:00Z", "PreviewIntent": "editor-preview-only", "PreviewStageStatus": {"NeutralPreviewHandoffEmitted": True, "GeneratedO3dePreviewProduct": False, "TypedAuthoringBindingCreated": False, "FunctionCompleteAllowed": False}, "SourceIndexId": "visual.index.foa.mono.fixture.synthetic", "SourceThumbnailManifestId": "thumbnail.manifest.synthetic", "PrimarySourceAssetRecordId": asset_id, "SourceAssetRecordIds": [asset_id], "SourceDependencies": [dependency], "CoordinateDeclaration": {"DeclaredSourceCoordinateSystem": {"System": "unity-declared"}, "DeclaredTargetCoordinateSystem": {"System": "o3de-preview-declared"}}, "CoordinateConversionEvidence": {"VerificationState": "not-verified", "VerificationEvidenceIds": []}, "PreviewEntries": [{"PreviewEntryId": "preview.entry.synthetic", "PrimarySourceAssetRecordId": asset_id, "SourceDependencies": [dependency], "NativeAssetRef": "$install/Tainted Grail_Data/LooseIcons/iron.png", "SourceIndexId": "visual.index.foa.mono.fixture.synthetic", "SourceThumbnailArtifactId": "thumbnail.synthetic", "PreviewClass": "icon", "PayloadRefs": [payload_id], "Fidelity": {"Geometry": "none", "Materials": "none", "Textures": "exact-copy", "Skeleton": "unsupported", "Animation": "unsupported"}, "Losses": [], "Warnings": [], "GeneratedO3dePreviewProduct": False, "TypedAuthoringBindingCreated": False}], "Payloads": [{"PayloadId": payload_id, "Role": "thumbnail", "Path": "$handoff/payloads/thumbnails/thumbnail.synthetic.png", "MediaType": "image/png", "Sha256": payload_hash, "ByteSize": payload_path.stat().st_size, "Generated": True, "LocalOnly": True, "RepositoryCommitAllowed": False, "RedistributionAllowed": False, "SourceAssetRecordIds": [asset_id], "SourceThumbnailArtifactId": "thumbnail.synthetic"}], "Losses": [], "Warnings": [], "Issues": [], "OperationalAuthority": {key: False for key in AUTHORITY_FALSE_KEYS}}
    handoff_path = handoff_root / "foa-preview-handoff.json"
    handoff_path.write_bytes(pretty_json(handoff))
    return workspace_path, handoff_path


def generate_fixture(output: Path, *, replace: bool = False) -> dict[str, Any]:
    if output.exists():
        if replace:
            shutil.rmtree(output) if output.is_dir() else output.unlink()
        else:
            raise O3dePreviewConversionError(f"Fixture output is not empty: {output}")
    workspace_path, handoff_path = synthetic_handoff(output)
    manifest, manifest_path = build_conversion(workspace_path, handoff_path, captured_at="2026-07-28T00:00:01Z")
    verify_conversion(manifest_path, workspace_path=workspace_path, handoff_path=handoff_path)
    return {"ConversionId": manifest["ConversionId"], "PreviewSourceCount": len(manifest["O3dePreviewSources"]), "ManifestPath": str(manifest_path), "OperationalAuthority": {key: False for key in AUTHORITY_FALSE_KEYS}}


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Stage neutral preview handoffs as local-only O3DE preview sources.")
    sub = parser.add_subparsers(dest="command", required=True)
    convert = sub.add_parser("convert")
    convert.add_argument("--workspace", required=True, type=Path)
    convert.add_argument("--handoff", required=True, type=Path)
    convert.add_argument("--output-root", type=Path)
    convert.add_argument("--captured-at")
    convert.add_argument("--replace", action="store_true")
    verify = sub.add_parser("verify")
    verify.add_argument("--input", required=True, type=Path)
    verify.add_argument("--workspace", type=Path)
    verify.add_argument("--handoff", type=Path)
    verify.add_argument("--output-root", type=Path)
    fixture = sub.add_parser("fixture")
    fixture.add_argument("--output", required=True, type=Path)
    fixture.add_argument("--replace", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.command == "fixture":
            manifest = generate_fixture(args.output, replace=args.replace)
            print(f"FoA O3DE preview conversion fixture wrote {manifest['PreviewSourceCount']} preview sources.")
        elif args.command == "convert":
            manifest, manifest_path = build_conversion(args.workspace, args.handoff, output_root=args.output_root, captured_at=args.captured_at, replace=args.replace)
            print(f"FoA O3DE preview conversion staged {len(manifest['O3dePreviewSources'])} sources to {manifest_path}.")
        else:
            manifest = verify_conversion(args.input, workspace_path=args.workspace, handoff_path=args.handoff, output_root=args.output_root)
            print(f"FoA O3DE preview conversion verified: {manifest['ConversionId']} with {len(manifest['O3dePreviewSources'])} sources.")
    except O3dePreviewConversionError as exc:
        print(f"FoA O3DE preview conversion failed: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
