#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Bounded O3DE Asset Processor import proof for FOA-SDK visual previews.

Consumes a local O3DE preview conversion manifest and an external bounded Asset
Processor observation, then emits import-proof evidence. This records observed
Asset Processor results; it does not invoke O3DE Asset Processor, create Asset
Browser entries, mutate catalogs, grant runtime permission, deploy, sign, or
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

TOOL_ID = "foa.o3de-asset-processor-import-proof"
TOOL_VERSION = "0.1.0"
DOCUMENT_KIND = "foa-o3de-asset-processor-import-proof"
CONVERSION_KIND = "foa-o3de-preview-conversion"
OBSERVATION_KIND = "foa-o3de-asset-processor-observation"
DEFAULT_PROOF_NAME = "foa-o3de-asset-processor-import-proof.json"
MAX_LOG_BYTES = 16 * 1024 * 1024
MAX_PRODUCTS = 10000

AUTHORITY_FALSE_KEYS = (
    "RuntimeInvocationAllowed",
    "GameMutationAllowed",
    "SaveAccessAllowed",
    "CatalogPromotionAllowed",
    "RuntimePermissionGranted",
    "UnityInvoked",
    "AssetBrowserEntryCreated",
    "TypedAuthoringBindingCreated",
    "DeploymentAllowed",
    "RepositoryCommitAllowed",
    "RedistributionAllowed",
    "FunctionCompleteAllowed",
    "AssetProcessorInvocationPerformedByThisTool",
)

ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
PRIVATE_RE = re.compile(r"(^/|^~/|^[A-Za-z]:[\\/]|^\\\\)")
SHA_RE = re.compile(r"^sha256:[0-9a-f]{64}$")
O3DE_SOURCE_TOKEN_RE = re.compile(r"^\$o3depreview(/[^\\\r\n]*)?$")
OBSERVATION_TOKEN_RE = re.compile(r"^\$observation(/[^\\\r\n]*)?$")
IMPORT_PROOF_TOKEN_RE = re.compile(r"^\$importproof(/[^\\\r\n]*)?$")
ASSET_CACHE_TOKEN_RE = re.compile(r"^\$assetcache(/[^\\\r\n]*)?$")


class AssetProcessorImportProofError(RuntimeError):
    """Raised when bounded Asset Processor import proof fails."""


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
        raise AssetProcessorImportProofError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise AssetProcessorImportProofError(f"JSON document must be an object: {path}")
    return value


def require_id(value: Any, label: str) -> str:
    if not isinstance(value, str) or not ID_RE.match(value):
        raise AssetProcessorImportProofError(f"{label} must be a lowercase stable identifier.")
    return value


def require_utc(value: Any, label: str) -> str:
    if not isinstance(value, str) or not UTC_RE.match(value):
        raise AssetProcessorImportProofError(f"{label} must use whole-second UTC format.")
    datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    return value


def require_sha(value: Any, label: str) -> str:
    if not isinstance(value, str) or not SHA_RE.match(value):
        raise AssetProcessorImportProofError(f"{label} must be sha256:<64-hex>.")
    return value


def assert_no_private_paths(value: Any, label: str = "document") -> None:
    if isinstance(value, str):
        if PRIVATE_RE.search(value):
            raise AssetProcessorImportProofError(f"{label} contains an absolute or private path: {value}")
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
        raise AssetProcessorImportProofError("Workspace must use SchemaVersion 1.")
    root = resolve_path(str(workspace.get("RootPath", "")), workspace_path.parent)
    active = require_id(workspace.get("ActiveGameProfileId"), "ActiveGameProfileId")
    profiles = workspace.get("GameProfiles")
    if not isinstance(profiles, list):
        raise AssetProcessorImportProofError("Workspace GameProfiles must be an array.")
    matches = [p for p in profiles if isinstance(p, dict) and p.get("ProfileId") == active]
    if len(matches) != 1:
        raise AssetProcessorImportProofError("Workspace ActiveGameProfileId must bind to exactly one profile.")
    profile = matches[0]
    runtime = profile.get("RuntimeTarget")
    if runtime not in {"Mono", "IL2CPP"}:
        raise AssetProcessorImportProofError("RuntimeTarget must be Mono or IL2CPP.")
    install = resolve_path(str(profile.get("InstallPath", "")), workspace_path.parent)
    extracted = resolve_path(str(profile.get("ExtractedDataPath", "")), workspace_path.parent)
    if not install.is_dir():
        raise AssetProcessorImportProofError("Configured FoA install path does not exist or is not a directory.")
    if not inside(extracted, root):
        raise AssetProcessorImportProofError("ExtractedDataPath must remain inside workspace root.")
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
            raise AssetProcessorImportProofError(f"{label} must match the exact active workspace profile.")


def false_authority(authority: Mapping[str, Any], label: str) -> None:
    for key in AUTHORITY_FALSE_KEYS:
        if authority.get(key) is not False:
            raise AssetProcessorImportProofError(f"{label} authority escalation: {key}")


def validate_conversion(conversion: Mapping[str, Any], profile: Mapping[str, Any]) -> tuple[list[Mapping[str, Any]], dict[str, Mapping[str, Any]]]:
    if conversion.get("SchemaVersion") != 1 or conversion.get("DocumentKind") != CONVERSION_KIND:
        raise AssetProcessorImportProofError("Input is not an O3DE preview conversion manifest.")
    if "TransformVerified" in conversion:
        raise AssetProcessorImportProofError("TransformVerified must not appear at top level; use CoordinateConversionEvidence.")
    profile_bound(conversion, profile, "O3DE preview conversion")
    stage = conversion.get("PreviewStageStatus")
    if not isinstance(stage, dict) or stage.get("FunctionCompleteAllowed") is not False:
        raise AssetProcessorImportProofError("Conversion PreviewStageStatus must keep FunctionCompleteAllowed=false.")
    if stage.get("GeneratedO3dePreviewSourcesEmitted") is not True:
        raise AssetProcessorImportProofError("Conversion must emit O3DE preview source staging evidence first.")
    for key in ("O3deAssetProcessorInvoked", "GeneratedO3dePreviewProduct", "AssetBrowserEntryCreated", "TypedAuthoringBindingCreated"):
        if stage.get(key) is not False:
            raise AssetProcessorImportProofError(f"Conversion stage claims a later step: {key}")
    authority = conversion.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise AssetProcessorImportProofError("Conversion OperationalAuthority is required.")
    for key, value in authority.items():
        if value is not False:
            raise AssetProcessorImportProofError(f"Conversion authority escalation: {key}")
    sources = conversion.get("O3dePreviewSources")
    evidence = conversion.get("O3dePreviewProductEvidence")
    if not isinstance(sources, list) or not sources or not isinstance(evidence, list):
        raise AssetProcessorImportProofError("Conversion requires preview sources and placeholder product evidence.")
    source_map: dict[str, Mapping[str, Any]] = {}
    for source in sources:
        if not isinstance(source, dict):
            raise AssetProcessorImportProofError("O3dePreviewSources entries must be objects.")
        source_id = require_id(source.get("O3dePreviewSourceId"), "O3dePreviewSourceId")
        if source_id in source_map:
            raise AssetProcessorImportProofError("Duplicate O3DE preview source identity.")
        token = source.get("PreviewSourcePath")
        if not isinstance(token, str) or not O3DE_SOURCE_TOKEN_RE.match(token):
            raise AssetProcessorImportProofError("PreviewSourcePath must use $o3depreview token.")
        require_sha(source.get("Sha256"), "O3DE preview source Sha256")
        for key in ("RepositoryCommitAllowed", "RedistributionAllowed", "O3deAssetProcessorInvoked", "GeneratedO3dePreviewProduct", "RuntimePermissionGranted"):
            if source.get(key) is not False:
                raise AssetProcessorImportProofError(f"O3DE preview source authority escalation: {key}")
        source_map[source_id] = source
    for placeholder in evidence:
        if not isinstance(placeholder, dict):
            raise AssetProcessorImportProofError("O3dePreviewProductEvidence entries must be objects.")
        if placeholder.get("EvidenceState") != "asset-processor-not-invoked":
            raise AssetProcessorImportProofError("Previous product evidence must be asset-processor-not-invoked.")
    assert_no_private_paths(conversion, "conversion")
    return sources, source_map


def observation_token_path(token: str, observation_root: Path) -> Path:
    if not isinstance(token, str) or not OBSERVATION_TOKEN_RE.match(token):
        raise AssetProcessorImportProofError("Observation payload path must use $observation token.")
    suffix = token[len("$observation"):].lstrip("/")
    path = (observation_root / suffix).resolve(strict=False)
    if not inside(path, observation_root):
        raise AssetProcessorImportProofError("Observation payload path escaped observation root.")
    return path


def importproof_token(path: Path, proof_root: Path) -> str:
    relative = path.resolve(strict=False).relative_to(proof_root.resolve(strict=False))
    if any(part in {"", ".", ".."} for part in relative.parts):
        raise AssetProcessorImportProofError("Import proof payload path contains unsafe segment.")
    return "$importproof/" + relative.as_posix()


def copy_log(token: str, observation_root: Path, proof_root: Path, *, role: str) -> dict[str, Any]:
    source = observation_token_path(token, observation_root)
    if not source.is_file():
        raise AssetProcessorImportProofError(f"Observation log payload is missing: {token}")
    data = source.read_bytes()
    if len(data) > MAX_LOG_BYTES:
        raise AssetProcessorImportProofError(f"Observation log exceeds {MAX_LOG_BYTES} bytes: {token}")
    destination = proof_root / "logs" / source.name
    destination.parent.mkdir(parents=True, exist_ok=True)
    tmp = destination.with_suffix(destination.suffix + ".tmp")
    tmp.write_bytes(data)
    os.replace(tmp, destination)
    return {
        "LogEvidenceId": "o3de.import-log." + hashlib.sha256(canonical_json([role, sha256_bytes(data)])).hexdigest()[:16],
        "Role": role,
        "Path": importproof_token(destination, proof_root),
        "Sha256": sha256_bytes(data),
        "ByteSize": len(data),
        "LocalOnly": True,
        "RepositoryCommitAllowed": False,
        "RedistributionAllowed": False,
    }


def validate_observation(observation: Mapping[str, Any], profile: Mapping[str, Any], conversion: Mapping[str, Any], source_map: Mapping[str, Mapping[str, Any]]) -> None:
    if observation.get("SchemaVersion") != 1 or observation.get("DocumentKind") != OBSERVATION_KIND:
        raise AssetProcessorImportProofError("Input is not an O3DE Asset Processor observation.")
    profile_bound(observation, profile, "Asset Processor observation")
    if observation.get("SourceConversionId") != conversion.get("ConversionId"):
        raise AssetProcessorImportProofError("Observation SourceConversionId must match conversion input.")
    require_utc(observation.get("ObservedAt"), "ObservedAt")
    authority = observation.get("OperationalAuthority", {})
    if not isinstance(authority, dict):
        raise AssetProcessorImportProofError("Observation OperationalAuthority must be an object.")
    for key in ("RuntimePermissionGranted", "CatalogPromotionAllowed", "GameMutationAllowed", "SaveAccessAllowed", "UnityInvoked", "DeploymentAllowed", "RepositoryCommitAllowed", "RedistributionAllowed", "FunctionCompleteAllowed"):
        if authority.get(key) is not False:
            raise AssetProcessorImportProofError(f"Observation authority escalation: {key}")
    run = observation.get("AssetProcessorRun")
    if not isinstance(run, dict):
        raise AssetProcessorImportProofError("Observation AssetProcessorRun is required.")
    if run.get("InvocationObserved") is not True:
        raise AssetProcessorImportProofError("Bounded import proof requires an observed Asset Processor invocation.")
    require_utc(run.get("StartedAt"), "AssetProcessorRun.StartedAt")
    require_utc(run.get("CompletedAt"), "AssetProcessorRun.CompletedAt")
    if not isinstance(run.get("ExitCode"), int):
        raise AssetProcessorImportProofError("AssetProcessorRun.ExitCode must be an integer.")
    if run.get("TimedOut") not in {True, False}:
        raise AssetProcessorImportProofError("AssetProcessorRun.TimedOut must be boolean.")
    command = run.get("CommandLineRedacted")
    if not isinstance(command, list) or not all(isinstance(part, str) for part in command):
        raise AssetProcessorImportProofError("AssetProcessorRun.CommandLineRedacted must be an array of strings.")
    logs = observation.get("ImportLogs")
    if not isinstance(logs, list) or not logs:
        raise AssetProcessorImportProofError("Observation requires at least one import log entry.")
    products = observation.get("ImportedProducts")
    if not isinstance(products, list):
        raise AssetProcessorImportProofError("Observation ImportedProducts must be an array.")
    if len(products) > MAX_PRODUCTS:
        raise AssetProcessorImportProofError(f"ImportedProducts exceeds {MAX_PRODUCTS}.")
    source_ids = set(source_map.keys())
    for product in products:
        if not isinstance(product, dict):
            raise AssetProcessorImportProofError("ImportedProducts entries must be objects.")
        source_id = require_id(product.get("O3dePreviewSourceId"), "ImportedProduct.O3dePreviewSourceId")
        if source_id not in source_ids:
            raise AssetProcessorImportProofError("Imported product references unknown O3DE preview source.")
        require_id(product.get("PreviewProductId"), "PreviewProductId")
        require_sha(product.get("ProductSha256"), "ProductSha256")
        cache = product.get("ProductCachePath")
        if not isinstance(cache, str) or not ASSET_CACHE_TOKEN_RE.match(cache):
            raise AssetProcessorImportProofError("ProductCachePath must use $assetcache token.")
        if not isinstance(product.get("ProductByteSize"), int) or product["ProductByteSize"] < 0:
            raise AssetProcessorImportProofError("ProductByteSize must be a non-negative integer.")
        if not isinstance(product.get("AssetId"), str) or not product["AssetId"]:
            raise AssetProcessorImportProofError("Imported product AssetId is required.")
    failures = observation.get("ImportFailures")
    if not isinstance(failures, list):
        raise AssetProcessorImportProofError("Observation ImportFailures must be an array.")
    for failure in failures:
        if not isinstance(failure, dict):
            raise AssetProcessorImportProofError("ImportFailures entries must be objects.")
        require_id(failure.get("FailureId"), "ImportFailure.FailureId")
        if failure.get("O3dePreviewSourceId") and failure["O3dePreviewSourceId"] not in source_ids:
            raise AssetProcessorImportProofError("Import failure references unknown O3DE preview source.")
    evidence_ids = observation.get("VerificationEvidenceIds", [])
    if not isinstance(evidence_ids, list) or not all(isinstance(item, str) and item for item in evidence_ids):
        raise AssetProcessorImportProofError("VerificationEvidenceIds must be an array of non-empty strings.")
    assert_no_private_paths(observation, "observation")


def build_proof(workspace_path: Path, conversion_path: Path, observation_path: Path, *, output_root: Path | None = None, captured_at: str | None = None, replace: bool = False) -> tuple[dict[str, Any], Path]:
    profile = load_profile(workspace_path)
    conversion = read_json(conversion_path)
    conversion_sources, source_map = validate_conversion(conversion, profile)
    observation = read_json(observation_path)
    validate_observation(observation, profile, conversion, source_map)
    if not inside(conversion_path.resolve(strict=False), profile["ExtractedDataPath"]):
        raise AssetProcessorImportProofError("Conversion manifest must remain inside ExtractedDataPath.")
    if not inside(observation_path.resolve(strict=False), profile["ExtractedDataPath"]):
        raise AssetProcessorImportProofError("Observation manifest must remain inside ExtractedDataPath.")
    captured_at = require_utc(captured_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "CapturedAt")
    seed = canonical_json({
        "ProfileId": profile["ProfileId"],
        "ConversionId": conversion["ConversionId"],
        "ObservationId": observation.get("ObservationId", ""),
        "Products": [(item.get("PreviewProductId"), item.get("ProductSha256")) for item in observation.get("ImportedProducts", [])],
        "CapturedAt": captured_at,
    })
    proof_id = "o3de.import-proof." + profile["ProfileId"] + "." + hashlib.sha256(seed).hexdigest()[:16]
    root = (output_root.resolve(strict=False) if output_root else (profile["ExtractedDataPath"] / "PreviewArtifacts" / "O3DE" / str(conversion["ConversionId"]) / "ImportProofs" / proof_id).resolve(strict=False))
    if not inside(root, profile["ExtractedDataPath"]):
        raise AssetProcessorImportProofError("Import proof output root must remain inside ExtractedDataPath.")
    if root.exists() and any(root.iterdir()):
        if replace:
            shutil.rmtree(root)
        else:
            raise AssetProcessorImportProofError(f"Import proof output root is not empty: {root}")
    root.mkdir(parents=True, exist_ok=True)

    observation_root = observation_path.parent.resolve(strict=False)
    logs = []
    for log in observation.get("ImportLogs", []):
        if not isinstance(log, dict):
            raise AssetProcessorImportProofError("ImportLogs entries must be objects.")
        role = str(log.get("Role", "asset-processor-log"))
        copied = copy_log(str(log.get("Path", "")), observation_root, root, role=role)
        if log.get("Sha256") and log.get("Sha256") != copied["Sha256"]:
            raise AssetProcessorImportProofError("Copied import log does not match observation hash.")
        logs.append(copied)

    imported_products = []
    for product in observation.get("ImportedProducts", []):
        source = source_map[product["O3dePreviewSourceId"]]
        product_id = require_id(product["PreviewProductId"], "PreviewProductId")
        imported_products.append({
            "PreviewProductEvidenceId": "o3de.preview-product." + hashlib.sha256(canonical_json([proof_id, product_id, product["ProductSha256"]])).hexdigest()[:16],
            "PreviewProductId": product_id,
            "O3dePreviewSourceId": product["O3dePreviewSourceId"],
            "PreviewEntryId": source.get("PreviewEntryId", ""),
            "PrimarySourceAssetRecordId": source.get("PrimarySourceAssetRecordId", ""),
            "AssetId": product["AssetId"],
            "ProductKind": str(product.get("ProductKind", "o3de-preview-product")),
            "BuilderId": str(product.get("BuilderId", "")),
            "ProductCachePath": product["ProductCachePath"],
            "ProductSha256": product["ProductSha256"],
            "ProductByteSize": product["ProductByteSize"],
            "GeneratedByO3deAssetProcessor": True,
            "LocalOnly": True,
            "RepositoryCommitAllowed": False,
            "RedistributionAllowed": False,
            "RuntimePermissionGranted": False,
            "TypedAuthoringBindingCreated": False,
        })

    run = observation["AssetProcessorRun"]
    import_state = "observed-success" if run.get("ExitCode") == 0 and imported_products and not observation.get("ImportFailures") else "observed-failure"
    manifest = {
        "SchemaVersion": 1,
        "DocumentKind": DOCUMENT_KIND,
        "ImportProofId": proof_id,
        "ProfileId": profile["ProfileId"],
        "GameVersion": profile["GameVersion"],
        "Branch": profile["Branch"],
        "RuntimeTarget": profile["RuntimeTarget"],
        "ToolId": TOOL_ID,
        "ToolVersion": TOOL_VERSION,
        "CapturedAt": captured_at,
        "PreviewIntent": "editor-preview-only",
        "SourceConversionId": conversion["ConversionId"],
        "SourceHandoffId": conversion.get("SourceHandoffId", ""),
        "SourceIndexId": conversion.get("SourceIndexId", ""),
        "SourceThumbnailManifestId": conversion.get("SourceThumbnailManifestId", ""),
        "PrimarySourceAssetRecordId": conversion.get("PrimarySourceAssetRecordId", ""),
        "SourceAssetRecordIds": conversion.get("SourceAssetRecordIds", []),
        "SourceDependencies": conversion.get("SourceDependencies", []),
        "PreviewStageStatus": {
            "O3dePreviewConversionConsumed": True,
            "O3deAssetProcessorImportProofRecorded": True,
            "O3deAssetProcessorInvocationObserved": True,
            "GeneratedO3dePreviewProductEvidence": bool(imported_products),
            "AssetBrowserEntryCreated": False,
            "PreviewRenderVerified": False,
            "TypedAuthoringBindingCreated": False,
            "FunctionCompleteAllowed": False,
            "NextRequiredStages": ["asset-browser-pane", "3d-preview-viewport", "item-recipe-visual-selectors"],
        },
        "AssetProcessorImportRun": {
            "ImportState": import_state,
            "InvocationObserved": True,
            "InvocationMode": str(run.get("InvocationMode", "bounded-external")),
            "ExecutableId": str(run.get("ExecutableId", "AssetProcessorBatch")),
            "ExecutableSha256": str(run.get("ExecutableSha256", "")),
            "CommandLineRedacted": run["CommandLineRedacted"],
            "StartedAt": run["StartedAt"],
            "CompletedAt": run["CompletedAt"],
            "ExitCode": run["ExitCode"],
            "TimedOut": run["TimedOut"],
            "ObservedBy": str(run.get("ObservedBy", "external-bounded-observation")),
        },
        "O3dePreviewSources": conversion_sources,
        "ImportedProducts": sorted(imported_products, key=lambda item: item["PreviewProductEvidenceId"]),
        "ImportLogs": sorted(logs, key=lambda item: item["LogEvidenceId"]),
        "ImportFailures": observation.get("ImportFailures", []),
        "VerificationEvidenceIds": observation.get("VerificationEvidenceIds", []),
        "Issues": observation.get("Issues", []),
        "OperationalAuthority": {key: False for key in AUTHORITY_FALSE_KEYS},
    }
    assert_no_private_paths(manifest)
    manifest_path = root / DEFAULT_PROOF_NAME
    manifest_path.write_bytes(pretty_json(manifest))
    return manifest, manifest_path


def verify_proof(manifest_path: Path, *, workspace_path: Path | None = None, conversion_path: Path | None = None, output_root: Path | None = None) -> dict[str, Any]:
    manifest = read_json(manifest_path)
    if manifest.get("SchemaVersion") != 1 or manifest.get("DocumentKind") != DOCUMENT_KIND:
        raise AssetProcessorImportProofError("Input is not an O3DE Asset Processor import proof manifest.")
    if "TransformVerified" in manifest:
        raise AssetProcessorImportProofError("TransformVerified must not appear at top level.")
    require_id(manifest.get("ImportProofId"), "ImportProofId")
    require_utc(manifest.get("CapturedAt"), "CapturedAt")
    stage = manifest.get("PreviewStageStatus")
    if not isinstance(stage, dict) or stage.get("FunctionCompleteAllowed") is not False:
        raise AssetProcessorImportProofError("PreviewStageStatus must keep FunctionCompleteAllowed=false.")
    for key in ("AssetBrowserEntryCreated", "TypedAuthoringBindingCreated"):
        if stage.get(key) is not False:
            raise AssetProcessorImportProofError(f"PreviewStageStatus authority escalation: {key}")
    authority = manifest.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise AssetProcessorImportProofError("OperationalAuthority is required.")
    false_authority(authority, "Import proof")
    run = manifest.get("AssetProcessorImportRun")
    if not isinstance(run, dict) or run.get("InvocationObserved") is not True:
        raise AssetProcessorImportProofError("AssetProcessorImportRun must record an observed invocation.")
    require_utc(run.get("StartedAt"), "AssetProcessorImportRun.StartedAt")
    require_utc(run.get("CompletedAt"), "AssetProcessorImportRun.CompletedAt")
    if not isinstance(run.get("ExitCode"), int) or run.get("TimedOut") not in {True, False}:
        raise AssetProcessorImportProofError("AssetProcessorImportRun must include ExitCode and TimedOut.")
    products = manifest.get("ImportedProducts")
    if not isinstance(products, list):
        raise AssetProcessorImportProofError("ImportedProducts must be an array.")
    seen_products: set[str] = set()
    source_ids = {source.get("O3dePreviewSourceId") for source in manifest.get("O3dePreviewSources", []) if isinstance(source, dict)}
    for product in products:
        if not isinstance(product, dict):
            raise AssetProcessorImportProofError("ImportedProducts entries must be objects.")
        evidence_id = require_id(product.get("PreviewProductEvidenceId"), "PreviewProductEvidenceId")
        if evidence_id in seen_products:
            raise AssetProcessorImportProofError("Duplicate imported product evidence identity.")
        seen_products.add(evidence_id)
        if product.get("O3dePreviewSourceId") not in source_ids:
            raise AssetProcessorImportProofError("Imported product references missing O3DE preview source.")
        require_id(product.get("PreviewProductId"), "PreviewProductId")
        require_sha(product.get("ProductSha256"), "ProductSha256")
        cache = product.get("ProductCachePath")
        if not isinstance(cache, str) or not ASSET_CACHE_TOKEN_RE.match(cache):
            raise AssetProcessorImportProofError("ProductCachePath must use $assetcache token.")
        for key in ("RepositoryCommitAllowed", "RedistributionAllowed", "RuntimePermissionGranted", "TypedAuthoringBindingCreated"):
            if product.get(key) is not False:
                raise AssetProcessorImportProofError(f"Imported product authority escalation: {key}")
    root = output_root.resolve(strict=False) if output_root else manifest_path.parent.resolve(strict=False)
    logs = manifest.get("ImportLogs")
    if not isinstance(logs, list) or not logs:
        raise AssetProcessorImportProofError("ImportLogs must contain at least one copied log evidence entry.")
    seen_logs: set[str] = set()
    for log in logs:
        if not isinstance(log, dict):
            raise AssetProcessorImportProofError("ImportLogs entries must be objects.")
        log_id = require_id(log.get("LogEvidenceId"), "LogEvidenceId")
        if log_id in seen_logs:
            raise AssetProcessorImportProofError("Duplicate import log evidence identity.")
        seen_logs.add(log_id)
        for key in ("RepositoryCommitAllowed", "RedistributionAllowed"):
            if log.get(key) is not False:
                raise AssetProcessorImportProofError(f"Import log authority escalation: {key}")
        token = log.get("Path")
        if not isinstance(token, str) or not IMPORT_PROOF_TOKEN_RE.match(token):
            raise AssetProcessorImportProofError("Import log Path must use $importproof token.")
        path = root / token[len("$importproof/"):]
        if not path.is_file():
            raise AssetProcessorImportProofError(f"Import proof log missing: {token}")
        payload = path.read_bytes()
        if log.get("ByteSize") != len(payload) or log.get("Sha256") != sha256_bytes(payload):
            raise AssetProcessorImportProofError(f"Import proof log payload mismatch: {token}")
    failures = manifest.get("ImportFailures")
    if not isinstance(failures, list):
        raise AssetProcessorImportProofError("ImportFailures must be an array.")
    if run.get("ExitCode") == 0 and products and failures:
        raise AssetProcessorImportProofError("Successful import proof cannot also contain failures.")
    if workspace_path:
        profile = load_profile(workspace_path)
        profile_bound(manifest, profile, "Import proof")
        if not inside(manifest_path.resolve(strict=False), profile["ExtractedDataPath"]):
            raise AssetProcessorImportProofError("Import proof manifest must remain inside ExtractedDataPath.")
    if conversion_path:
        conversion = read_json(conversion_path)
        if manifest.get("SourceConversionId") != conversion.get("ConversionId"):
            raise AssetProcessorImportProofError("Import proof SourceConversionId must match conversion input.")
    assert_no_private_paths(manifest)
    return manifest


def write_workspace(output: Path) -> Path:
    (output / "game" / "FoA").mkdir(parents=True, exist_ok=True)
    extracted = output / "workspace" / "Extracted"
    extracted.mkdir(parents=True, exist_ok=True)
    workspace = {
        "SchemaVersion": 1,
        "WorkspaceId": "fixture.workspace",
        "DisplayName": "O3DE AP Proof Fixture",
        "RootPath": "./workspace",
        "OutputPath": "./workspace/Build",
        "StagingPath": "./workspace/Staging",
        "DeploymentPath": "./workspace/Deploy",
        "ActiveGameProfileId": "foa.mono.fixture",
        "GameProfiles": [{
            "ProfileId": "foa.mono.fixture",
            "DisplayName": "FoA Mono Fixture",
            "InstallPath": "./game/FoA",
            "GameVersion": "1.23.401",
            "Branch": "mono",
            "RuntimeTarget": "Mono",
            "UnityVersion": "6000.0.64f1",
            "BepInExVersion": "5.4.23.3",
            "ManagedAssembliesPath": "",
            "PluginPath": "",
            "DiagnosticsPath": "./workspace/Diagnostics",
            "ExtractedDataPath": "./workspace/Extracted",
            "DlcScopes": ["base-game"],
        }],
    }
    path = output / "workspace.tgworkspace.json"
    path.write_bytes(pretty_json(workspace))
    return path


def synthetic_conversion(output: Path, workspace_path: Path) -> Path:
    profile = load_profile(workspace_path)
    conversion_id = "o3de.preview.foa.mono.fixture.synthetic"
    root = profile["ExtractedDataPath"] / "PreviewArtifacts" / "O3DE" / conversion_id
    source_path = root / "SourceAssets" / "Textures" / "iron.png"
    source_path.parent.mkdir(parents=True, exist_ok=True)
    source_payload = b"synthetic-o3de-preview-source"
    source_path.write_bytes(source_payload)
    source_id = "o3de.source.fixture.iron"
    manifest = {
        "SchemaVersion": 1,
        "DocumentKind": CONVERSION_KIND,
        "ConversionId": conversion_id,
        "ProfileId": profile["ProfileId"],
        "GameVersion": profile["GameVersion"],
        "Branch": profile["Branch"],
        "RuntimeTarget": profile["RuntimeTarget"],
        "ToolId": "foa.o3de-preview-conversion",
        "ToolVersion": "0.1.0",
        "CapturedAt": "2026-07-28T00:00:00Z",
        "PreviewIntent": "editor-preview-only",
        "SourceHandoffId": "preview.handoff.foa.mono.fixture.synthetic",
        "SourceIndexId": "visual.index.foa.mono.fixture.synthetic",
        "SourceThumbnailManifestId": "thumbnail.manifest.synthetic",
        "PrimarySourceAssetRecordId": "visual.asset.foa.mono.fixture.iron",
        "SourceAssetRecordIds": ["visual.asset.foa.mono.fixture.iron"],
        "SourceDependencies": [{"SourceAssetRecordId": "visual.asset.foa.mono.fixture.iron", "NativeAssetRef": "$install/Tainted Grail_Data/LooseIcons/iron.png", "SourceIndexId": "visual.index.foa.mono.fixture.synthetic", "SourceFingerprint": sha256_bytes(b"synthetic-native-source"), "DependencyRole": "primary", "DependencyKind": "visual-asset-discovery-record", "RequiredForPreview": True}],
        "PreviewStageStatus": {"NeutralPreviewHandoffConsumed": True, "GeneratedO3dePreviewSourcesEmitted": True, "O3deAssetProcessorInvoked": False, "GeneratedO3dePreviewProduct": False, "AssetBrowserEntryCreated": False, "TypedAuthoringBindingCreated": False, "FunctionCompleteAllowed": False, "NextRequiredStages": ["o3de-asset-processor-import-proof", "asset-browser-pane", "item-recipe-visual-selectors"]},
        "GeneratedRoot": "$o3depreview",
        "CoordinateDeclaration": {},
        "CoordinateConversionEvidence": {"VerificationState": "not-verified"},
        "PreviewEntryIds": ["preview.entry.fixture.iron"],
        "O3dePreviewSources": [{"O3dePreviewSourceId": source_id, "PreviewEntryId": "preview.entry.fixture.iron", "PrimarySourceAssetRecordId": "visual.asset.foa.mono.fixture.iron", "SourceDependencies": [], "SourceHandoffPayloadId": "payload.fixture.iron", "SourceHandoffPayloadSha256": sha256_bytes(source_payload), "PreviewSourcePath": "$o3depreview/SourceAssets/Textures/iron.png", "PreviewSourceKind": "texture-preview-source", "MediaType": "image/png", "Sha256": sha256_bytes(source_payload), "ByteSize": len(source_payload), "GeneratedFromNeutralHandoff": True, "LocalOnly": True, "RepositoryCommitAllowed": False, "RedistributionAllowed": False, "O3deAssetProcessorInvoked": False, "GeneratedO3dePreviewProduct": False, "RuntimePermissionGranted": False, "ProductEvidenceId": "o3de.product-evidence.fixture.notinvoked"}],
        "O3dePreviewProductEvidence": [{"ProductEvidenceId": "o3de.product-evidence.fixture.notinvoked", "O3dePreviewSourceId": source_id, "EvidenceState": "asset-processor-not-invoked", "O3deAssetProcessorInvoked": False, "GeneratedO3dePreviewProduct": False, "ProductAssetIds": [], "ProductCachePaths": [], "ImportLogPath": "", "PreviewRenderVerified": False, "VerificationEvidenceIds": [], "NextRequiredAction": "run-bounded-o3de-asset-processor-import-proof"}],
        "Issues": [],
        "OperationalAuthority": {"RuntimeInvocationAllowed": False, "GameMutationAllowed": False, "SaveAccessAllowed": False, "CatalogPromotionAllowed": False, "RuntimePermissionGranted": False, "UnityInvoked": False, "O3deAssetProcessorInvoked": False, "GeneratedO3dePreviewProduct": False, "AssetBrowserEntryCreated": False, "TypedAuthoringBindingCreated": False, "DeploymentAllowed": False, "RepositoryCommitAllowed": False, "RedistributionAllowed": False, "FunctionCompleteAllowed": False},
    }
    path = root / "foa-o3de-preview-conversion.json"
    path.write_bytes(pretty_json(manifest))
    return path


def synthetic_observation(workspace_path: Path, conversion_path: Path) -> Path:
    profile = load_profile(workspace_path)
    conversion = read_json(conversion_path)
    observation_root = profile["ExtractedDataPath"] / "PreviewArtifacts" / "O3DE" / str(conversion["ConversionId"]) / "AssetProcessorObservation"
    logs = observation_root / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    log_payload = b"AssetProcessorBatch synthetic import succeeded\n"
    (logs / "assetprocessor.log").write_bytes(log_payload)
    product_payload = b"synthetic-product"
    observation = {
        "SchemaVersion": 1,
        "DocumentKind": OBSERVATION_KIND,
        "ObservationId": "o3de.ap.observation.fixture",
        "SourceConversionId": conversion["ConversionId"],
        "ProfileId": profile["ProfileId"],
        "GameVersion": profile["GameVersion"],
        "Branch": profile["Branch"],
        "RuntimeTarget": profile["RuntimeTarget"],
        "ObservedAt": "2026-07-28T00:01:00Z",
        "AssetProcessorRun": {"InvocationObserved": True, "InvocationMode": "bounded-local-fixture", "ExecutableId": "AssetProcessorBatch", "ExecutableSha256": sha256_bytes(b"synthetic-ap-exe"), "CommandLineRedacted": ["AssetProcessorBatch", "--project-path", "$project", "--platform", "pc"], "StartedAt": "2026-07-28T00:00:01Z", "CompletedAt": "2026-07-28T00:00:03Z", "ExitCode": 0, "TimedOut": False, "ObservedBy": "fixture"},
        "ImportedProducts": [{"O3dePreviewSourceId": conversion["O3dePreviewSources"][0]["O3dePreviewSourceId"], "PreviewProductId": "o3de.product.fixture.iron", "AssetId": "{fixture-asset}:0", "ProductKind": "texture-preview-product", "BuilderId": "synthetic.o3de.asset-builder", "ProductCachePath": "$assetcache/pc/textures/iron.dds", "ProductSha256": sha256_bytes(product_payload), "ProductByteSize": len(product_payload)}],
        "ImportLogs": [{"Role": "asset-processor-log", "Path": "$observation/logs/assetprocessor.log", "Sha256": sha256_bytes(log_payload), "ByteSize": len(log_payload)}],
        "ImportFailures": [],
        "VerificationEvidenceIds": ["fixture.visual-import-proof"],
        "Issues": [],
        "OperationalAuthority": {"RuntimePermissionGranted": False, "CatalogPromotionAllowed": False, "GameMutationAllowed": False, "SaveAccessAllowed": False, "UnityInvoked": False, "DeploymentAllowed": False, "RepositoryCommitAllowed": False, "RedistributionAllowed": False, "FunctionCompleteAllowed": False},
    }
    path = observation_root / "foa-o3de-asset-processor-observation.json"
    path.write_bytes(pretty_json(observation))
    return path


def generate_fixture(output: Path, *, replace: bool = False) -> dict[str, Any]:
    if output.exists():
        if replace:
            shutil.rmtree(output) if output.is_dir() else output.unlink()
        else:
            raise AssetProcessorImportProofError(f"Fixture output is not empty: {output}")
    workspace = write_workspace(output)
    conversion = synthetic_conversion(output, workspace)
    observation = synthetic_observation(workspace, conversion)
    manifest, proof = build_proof(workspace, conversion, observation, captured_at="2026-07-28T00:02:00Z", replace=True)
    verify_proof(proof, workspace_path=workspace, conversion_path=conversion)
    return {"ImportProofId": manifest["ImportProofId"], "ImportedProducts": len(manifest["ImportedProducts"]), "ProofPath": str(proof)}


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Record bounded O3DE Asset Processor import proof.")
    sub = parser.add_subparsers(dest="command", required=True)

    proof = sub.add_parser("proof")
    proof.add_argument("--workspace", required=True, type=Path)
    proof.add_argument("--conversion", required=True, type=Path)
    proof.add_argument("--observation", required=True, type=Path)
    proof.add_argument("--output-root", type=Path)
    proof.add_argument("--captured-at")
    proof.add_argument("--replace", action="store_true")

    verify = sub.add_parser("verify")
    verify.add_argument("--input", required=True, type=Path)
    verify.add_argument("--workspace", type=Path)
    verify.add_argument("--conversion", type=Path)
    verify.add_argument("--output-root", type=Path)

    fixture = sub.add_parser("fixture")
    fixture.add_argument("--output", required=True, type=Path)
    fixture.add_argument("--replace", action="store_true")

    args = parser.parse_args(argv)
    try:
        if args.command == "fixture":
            result = generate_fixture(args.output, replace=args.replace)
            print(f"FoA O3DE Asset Processor import proof fixture wrote {result['ImportedProducts']} product records.")
        elif args.command == "proof":
            manifest, path = build_proof(args.workspace, args.conversion, args.observation, output_root=args.output_root, captured_at=args.captured_at, replace=args.replace)
            print(f"FoA O3DE Asset Processor import proof wrote {len(manifest['ImportedProducts'])} product records to {path}.")
        elif args.command == "verify":
            manifest = verify_proof(args.input, workspace_path=args.workspace, conversion_path=args.conversion, output_root=args.output_root)
            print(f"FoA O3DE Asset Processor import proof verified: {manifest['ImportProofId']}.")
    except AssetProcessorImportProofError as exc:
        print(f"FoA O3DE Asset Processor import proof failed: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
