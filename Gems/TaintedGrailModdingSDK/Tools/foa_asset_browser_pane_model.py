#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Build a bounded Asset Browser pane model from O3DE import-proof evidence.

This tool consumes import-proof evidence only. It normalizes the original fixture
field names and the canonical import-proof field names without granting any
catalog, runtime, deployment, redistribution, or authoring-binding authority.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

TOOL_ID = "foa.asset-browser-pane-model"
TOOL_VERSION = "0.2.0"
DOCUMENT_KIND = "foa-asset-browser-pane-model"
PROOF_KIND = "foa-o3de-asset-processor-import-proof"
DEFAULT_NAME = "foa-asset-browser-pane-model.json"
MAX_ENTRIES = 10000

FALSE_KEYS = (
    "RuntimeInvocationAllowed",
    "GameMutationAllowed",
    "SaveAccessAllowed",
    "CatalogPromotionAllowed",
    "RuntimePermissionGranted",
    "UnityInvoked",
    "O3deAssetProcessorInvokedByThisTool",
    "O3deAssetBrowserMutated",
    "AssetBrowserEntryCreated",
    "TypedAuthoringBindingCreated",
    "DeploymentAllowed",
    "RepositoryCommitAllowed",
    "RedistributionAllowed",
    "FunctionCompleteAllowed",
)

ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
SHA_RE = re.compile(r"^(?:sha256:)?[0-9a-f]{64}$")
ASSET_ID_RE = re.compile(
    r"^(?:\{?[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
    r"[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\}?)(?::[0-9a-fA-F]{1,8})?$"
)


class AssetBrowserPaneError(RuntimeError):
    """Raised when the pane-model contract is invalid."""


def pretty(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def canonical(value: Any) -> bytes:
    return (
        json.dumps(
            value,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        )
        + "\n"
    ).encode("utf-8")


def sha256_bytes(payload: bytes) -> str:
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8", errors="strict"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise AssetBrowserPaneError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise AssetBrowserPaneError(f"JSON document must be an object: {path}")
    return value


def require_id(value: Any, label: str) -> str:
    if not isinstance(value, str) or not ID_RE.fullmatch(value):
        raise AssetBrowserPaneError(f"{label} must be a stable lowercase identifier.")
    return value


def require_utc(value: Any, label: str) -> str:
    if not isinstance(value, str) or not UTC_RE.fullmatch(value):
        raise AssetBrowserPaneError(f"{label} must use whole-second UTC format.")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise AssetBrowserPaneError(f"{label} is not valid UTC.") from exc
    return value


def require_asset_id(value: Any, label: str) -> str:
    if not isinstance(value, str) or not ASSET_ID_RE.fullmatch(value):
        raise AssetBrowserPaneError(
            f"{label} must be an O3DE AssetId string in GUID[:subId] form."
        )
    return value


def no_private_paths(value: Any, label: str = "document") -> None:
    if isinstance(value, str):
        if (
            value.startswith("/")
            or value.startswith("~/")
            or value.startswith("\\\\")
            or (len(value) > 1 and value[1] == ":")
        ):
            raise AssetBrowserPaneError(f"{label} contains an absolute or private path.")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            no_private_paths(child, f"{label}[{index}]")
    elif isinstance(value, dict):
        for key, child in value.items():
            no_private_paths(child, f"{label}.{key}")


def resolve(raw: str, base: Path) -> Path:
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
        raise AssetBrowserPaneError("Workspace must use SchemaVersion 1.")
    workspace_root = resolve(str(workspace.get("RootPath", "")), workspace_path.parent)
    active_id = require_id(
        workspace.get("ActiveGameProfileId"),
        "ActiveGameProfileId",
    )
    profiles = workspace.get("GameProfiles")
    if not isinstance(profiles, list):
        raise AssetBrowserPaneError("Workspace GameProfiles must be an array.")
    matches = [
        entry
        for entry in profiles
        if isinstance(entry, dict) and entry.get("ProfileId") == active_id
    ]
    if len(matches) != 1:
        raise AssetBrowserPaneError(
            "Workspace ActiveGameProfileId must bind to exactly one profile."
        )
    profile = matches[0]
    runtime = profile.get("RuntimeTarget")
    if runtime not in {"Mono", "IL2CPP"}:
        raise AssetBrowserPaneError("RuntimeTarget must be Mono or IL2CPP.")
    install = resolve(str(profile.get("InstallPath", "")), workspace_path.parent)
    extracted = resolve(str(profile.get("ExtractedDataPath", "")), workspace_path.parent)
    if not install.is_dir():
        raise AssetBrowserPaneError(
            "Configured FoA install path does not exist or is not a directory."
        )
    if not inside(extracted, workspace_root):
        raise AssetBrowserPaneError(
            "ExtractedDataPath must remain inside the workspace root."
        )
    return {
        "ProfileId": require_id(profile.get("ProfileId"), "ProfileId"),
        "GameVersion": str(profile.get("GameVersion", "")),
        "Branch": str(profile.get("Branch", "")),
        "RuntimeTarget": runtime,
        "InstallPath": install,
        "ExtractedDataPath": extracted,
    }


def profile_bound(
    document: Mapping[str, Any],
    profile: Mapping[str, Any],
    label: str,
) -> None:
    for key in ("ProfileId", "GameVersion", "Branch", "RuntimeTarget"):
        if document.get(key) != profile[key]:
            raise AssetBrowserPaneError(
                f"{label} must match the exact active workspace profile."
            )


def object_list(document: Mapping[str, Any], key: str) -> list[dict[str, Any]]:
    value = document.get(key)
    if not isinstance(value, list):
        return []
    return [entry for entry in value if isinstance(entry, dict)]


def imported_products(proof: Mapping[str, Any]) -> list[dict[str, Any]]:
    direct = object_list(proof, "ImportedProducts")
    if direct:
        return direct
    normalized: list[dict[str, Any]] = []
    for evidence in object_list(proof, "O3dePreviewProductEvidence"):
        for product in evidence.get("ImportedProducts", []) or []:
            if not isinstance(product, dict):
                continue
            candidate = dict(product)
            candidate.setdefault(
                "PreviewProductEvidenceId",
                evidence.get("PreviewProductEvidenceId")
                or evidence.get("ProductEvidenceId", ""),
            )
            candidate.setdefault(
                "O3dePreviewSourceId",
                evidence.get("O3dePreviewSourceId", ""),
            )
            normalized.append(candidate)
    return normalized


def import_failures(proof: Mapping[str, Any]) -> list[dict[str, Any]]:
    direct = object_list(proof, "ImportFailures")
    if direct:
        return direct
    normalized: list[dict[str, Any]] = []
    for evidence in object_list(proof, "O3dePreviewProductEvidence"):
        for failure in evidence.get("Failures", []) or []:
            if not isinstance(failure, dict):
                continue
            candidate = dict(failure)
            candidate.setdefault(
                "PreviewProductEvidenceId",
                evidence.get("PreviewProductEvidenceId")
                or evidence.get("ProductEvidenceId", ""),
            )
            candidate.setdefault(
                "O3dePreviewSourceId",
                evidence.get("O3dePreviewSourceId", ""),
            )
            normalized.append(candidate)
    return normalized


def import_logs(proof: Mapping[str, Any]) -> list[dict[str, Any]]:
    for key in ("ImportLogs", "ImportLogEvidence", "LogEvidence"):
        value = object_list(proof, key)
        if value:
            return value
    return []


def product_evidence_id(product: Mapping[str, Any]) -> str:
    return require_id(
        product.get("PreviewProductEvidenceId")
        or product.get("ProductEvidenceId"),
        "PreviewProductEvidenceId",
    )


def product_asset_id(product: Mapping[str, Any]) -> str:
    direct = product.get("AssetId") or product.get("ProductAssetId")
    if isinstance(direct, str) and direct:
        return require_asset_id(direct, "ImportedProduct.AssetId")
    values = product.get("ProductAssetIds")
    if isinstance(values, list) and values:
        return require_asset_id(values[0], "ImportedProduct.AssetId")
    raise AssetBrowserPaneError("Imported product AssetId is required.")


def product_cache_path(product: Mapping[str, Any]) -> str:
    direct = product.get("ProductCachePath")
    if isinstance(direct, str) and direct:
        value = direct
    else:
        values = product.get("ProductCachePaths")
        value = values[0] if isinstance(values, list) and values else ""
    if not isinstance(value, str) or not value.startswith("$assetcache/"):
        raise AssetBrowserPaneError(
            "Imported product cache path must use the $assetcache token."
        )
    return value


def validate_proof(
    proof: Mapping[str, Any],
    profile: Mapping[str, Any],
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    if proof.get("SchemaVersion") != 1 or proof.get("DocumentKind") != PROOF_KIND:
        raise AssetBrowserPaneError(
            "Input must be import-proof evidence, not raw conversion data."
        )
    profile_bound(proof, profile, "Import proof")
    stage = proof.get("PreviewStageStatus")
    if not isinstance(stage, dict) or stage.get("FunctionCompleteAllowed") is not False:
        raise AssetBrowserPaneError(
            "Import proof must keep FunctionCompleteAllowed=false."
        )
    for key in ("AssetBrowserEntryCreated", "TypedAuthoringBindingCreated"):
        if stage.get(key) is not False:
            raise AssetBrowserPaneError(f"Import proof stage escalation: {key}")
    authority = proof.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise AssetBrowserPaneError("Import proof OperationalAuthority is required.")
    for key, value in authority.items():
        if key == "O3deAssetProcessorInvocationObserved":
            if value not in {True, False}:
                raise AssetBrowserPaneError(
                    "O3deAssetProcessorInvocationObserved must be boolean."
                )
        elif value is not False:
            raise AssetBrowserPaneError(f"Import proof authority escalation: {key}")

    products = imported_products(proof)
    failures = import_failures(proof)
    logs = import_logs(proof)
    if not products and not failures:
        raise AssetBrowserPaneError(
            "Import proof must contain imported products or explicit failures."
        )
    if len(products) + len(failures) > MAX_ENTRIES:
        raise AssetBrowserPaneError(
            f"Import proof exceeds the maximum pane-entry count of {MAX_ENTRIES}."
        )

    seen_product_evidence: set[str] = set()
    for product in products:
        require_id(
            product.get("O3dePreviewSourceId"),
            "ImportedProduct.O3dePreviewSourceId",
        )
        evidence_id = product_evidence_id(product)
        if evidence_id in seen_product_evidence:
            raise AssetBrowserPaneError("Duplicate imported product evidence identity.")
        seen_product_evidence.add(evidence_id)
        product_asset_id(product)
        product_cache_path(product)
        product_sha = product.get("ProductSha256", "")
        if product_sha and (
            not isinstance(product_sha, str) or not SHA_RE.fullmatch(product_sha)
        ):
            raise AssetBrowserPaneError(
                "Imported product ProductSha256 must be SHA-256 when present."
            )

    seen_failures: set[str] = set()
    for failure in failures:
        failure_id = require_id(failure.get("FailureId"), "ImportFailure.FailureId")
        if failure_id in seen_failures:
            raise AssetBrowserPaneError("Duplicate import failure identity.")
        seen_failures.add(failure_id)
        if failure.get("O3dePreviewSourceId"):
            require_id(
                failure.get("O3dePreviewSourceId"),
                "ImportFailure.O3dePreviewSourceId",
            )

    for log in logs:
        path = log.get("Path")
        if path and (
            not isinstance(path, str) or not path.startswith("$importproof/")
        ):
            raise AssetBrowserPaneError(
                "Import log path must use the $importproof token."
            )

    no_private_paths(proof, "import proof")
    return products, failures, logs


def selection_policy() -> dict[str, bool]:
    return {
        "SelectableInPane": True,
        "CanCreateTypedAuthoringBinding": False,
        "RequiresExplicitBindingStep": True,
        "CatalogPromotionAllowed": False,
        "RuntimePermissionGranted": False,
        "RepositoryCommitAllowed": False,
        "RedistributionAllowed": False,
    }


def product_entry(
    model_id: str,
    proof: Mapping[str, Any],
    product: Mapping[str, Any],
) -> dict[str, Any]:
    asset_id = product_asset_id(product)
    cache_path = product_cache_path(product)
    evidence_id = product_evidence_id(product)
    preview_product_id = str(product.get("PreviewProductId", ""))
    display_name = product.get("DisplayName") or preview_product_id or asset_id
    return {
        "PaneEntryId": "assetbrowser.entry."
        + hashlib.sha256(
            canonical([model_id, asset_id, cache_path, evidence_id])
        ).hexdigest()[:16],
        "EntryKind": "o3de-preview-product",
        "DisplayName": str(display_name),
        "PreviewAvailability": "product-imported",
        "O3dePreviewSourceId": product["O3dePreviewSourceId"],
        "ProductEvidenceId": evidence_id,
        "PreviewProductId": preview_product_id,
        "ProductKind": str(product.get("ProductKind", "o3de-preview-product")),
        "BuilderId": str(product.get("BuilderId", "")),
        "ProductAssetIds": [asset_id],
        "ProductCachePaths": [cache_path],
        "ProductSha256": str(product.get("ProductSha256", "")),
        "ProductByteSize": int(product.get("ProductByteSize", 0) or 0),
        "PreviewRenderVerified": bool(product.get("PreviewRenderVerified", False)),
        "SourceImportProofId": proof.get("ImportProofId", ""),
        "SourceConversionId": proof.get("SourceConversionId", ""),
        "PrimarySourceAssetRecordId": product.get(
            "PrimarySourceAssetRecordId",
            proof.get("PrimarySourceAssetRecordId", ""),
        ),
        "SourceDependencies": product.get(
            "SourceDependencies",
            proof.get("SourceDependencies", []),
        ),
        "EvidenceRefs": [evidence_id],
        "IssueSeverity": "none",
        "Issues": [],
        "SelectionPolicy": selection_policy(),
    }


def failure_entry(
    model_id: str,
    proof: Mapping[str, Any],
    failure: Mapping[str, Any],
) -> dict[str, Any]:
    failure_id = require_id(failure.get("FailureId"), "ImportFailure.FailureId")
    return {
        "PaneEntryId": "assetbrowser.entry."
        + hashlib.sha256(
            canonical(
                [
                    model_id,
                    failure_id,
                    failure.get("O3dePreviewSourceId", ""),
                ]
            )
        ).hexdigest()[:16],
        "EntryKind": "o3de-import-failure",
        "DisplayName": str(failure.get("DisplayName") or failure_id),
        "PreviewAvailability": "import-failed",
        "O3dePreviewSourceId": str(failure.get("O3dePreviewSourceId", "")),
        "ProductEvidenceId": str(
            failure.get("PreviewProductEvidenceId")
            or failure.get("ProductEvidenceId", "")
        ),
        "ProductAssetIds": [],
        "ProductCachePaths": [],
        "SourceImportProofId": proof.get("ImportProofId", ""),
        "SourceConversionId": proof.get("SourceConversionId", ""),
        "PrimarySourceAssetRecordId": failure.get(
            "PrimarySourceAssetRecordId",
            proof.get("PrimarySourceAssetRecordId", ""),
        ),
        "SourceDependencies": failure.get(
            "SourceDependencies",
            proof.get("SourceDependencies", []),
        ),
        "EvidenceRefs": [failure_id],
        "IssueSeverity": "error",
        "Issues": [
            {
                "Code": str(failure.get("Code", "o3de-import-failed")),
                "Message": str(
                    failure.get(
                        "Message",
                        "O3DE import failed for this preview source.",
                    )
                ),
            }
        ],
        "SelectionPolicy": selection_policy(),
    }


def build_model(
    workspace_path: Path,
    import_proof_path: Path,
    *,
    output_root: Path | None = None,
    captured_at: str | None = None,
    replace: bool = False,
) -> tuple[dict[str, Any], Path]:
    profile = load_profile(workspace_path)
    proof = read_json(import_proof_path)
    products, failures, logs = validate_proof(proof, profile)
    if not inside(
        import_proof_path.resolve(strict=False),
        profile["ExtractedDataPath"],
    ):
        raise AssetBrowserPaneError(
            "Import proof must remain inside ExtractedDataPath."
        )

    captured = require_utc(
        captured_at
        or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "CapturedAt",
    )
    model_id = (
        "assetbrowser.model."
        + profile["ProfileId"]
        + "."
        + hashlib.sha256(
            canonical(
                [
                    proof.get("ImportProofId", ""),
                    products,
                    failures,
                    captured,
                ]
            )
        ).hexdigest()[:16]
    )
    root = (
        output_root.resolve(strict=False)
        if output_root is not None
        else (
            profile["ExtractedDataPath"]
            / "PreviewArtifacts"
            / "AssetBrowser"
            / model_id
        ).resolve(strict=False)
    )
    if not inside(root, profile["ExtractedDataPath"]):
        raise AssetBrowserPaneError(
            "Asset Browser output root must remain inside ExtractedDataPath."
        )
    if root.exists() and any(root.iterdir()):
        if replace:
            shutil.rmtree(root)
        else:
            raise AssetBrowserPaneError(
                f"Asset Browser output root is not empty: {root}"
            )
    root.mkdir(parents=True, exist_ok=True)

    entries = sorted(
        [product_entry(model_id, proof, item) for item in products]
        + [failure_entry(model_id, proof, item) for item in failures],
        key=lambda item: item["PaneEntryId"],
    )
    document = {
        "SchemaVersion": 1,
        "DocumentKind": DOCUMENT_KIND,
        "AssetBrowserModelId": model_id,
        "ProfileId": profile["ProfileId"],
        "GameVersion": profile["GameVersion"],
        "Branch": profile["Branch"],
        "RuntimeTarget": profile["RuntimeTarget"],
        "ToolId": TOOL_ID,
        "ToolVersion": TOOL_VERSION,
        "CapturedAt": captured,
        "PreviewIntent": "editor-preview-only",
        "SourceImportProofId": proof.get("ImportProofId", ""),
        "SourceConversionId": proof.get("SourceConversionId", ""),
        "SourceHandoffId": proof.get("SourceHandoffId", ""),
        "SourceIndexId": proof.get("SourceIndexId", ""),
        "PrimarySourceAssetRecordId": proof.get(
            "PrimarySourceAssetRecordId",
            "",
        ),
        "SourceDependencies": proof.get("SourceDependencies", []),
        "InputContract": {
            "ImportProofEvidenceConsumed": True,
            "RawConversionFileConsumed": False,
            "RawO3dePreviewSourceConsumed": False,
        },
        "PreviewStageStatus": {
            "ImportProofEvidenceConsumed": True,
            "AssetBrowserPaneModelEmitted": True,
            "AssetBrowserPaneEntriesEmitted": bool(entries),
            "O3deAssetBrowserEntryCreated": False,
            "TypedAuthoringBindingCreated": False,
            "FunctionCompleteAllowed": False,
            "NextRequiredStages": [
                "asset-browser-pane-ui-rendering",
                "3d-preview-viewport",
                "item-recipe-visual-selectors",
            ],
        },
        "PaneModelRoot": "$assetbrowser",
        "PaneEntries": entries,
        "ImportLogRefs": [
            {
                "LogEvidenceId": log.get("LogEvidenceId", ""),
                "Path": log.get("Path", ""),
                "Sha256": log.get("Sha256", ""),
            }
            for log in logs
        ],
        "Issues": [],
        "OperationalAuthority": {key: False for key in FALSE_KEYS},
    }
    no_private_paths(document)
    output_path = root / DEFAULT_NAME
    output_path.write_bytes(pretty(document))
    return document, output_path


def verify_model(
    model_path: Path,
    *,
    workspace_path: Path | None = None,
    import_proof_path: Path | None = None,
) -> dict[str, Any]:
    document = read_json(model_path)
    if (
        document.get("SchemaVersion") != 1
        or document.get("DocumentKind") != DOCUMENT_KIND
    ):
        raise AssetBrowserPaneError(
            "Input is not an Asset Browser pane model."
        )
    require_id(document.get("AssetBrowserModelId"), "AssetBrowserModelId")
    require_utc(document.get("CapturedAt"), "CapturedAt")

    contract = document.get("InputContract")
    if (
        not isinstance(contract, dict)
        or contract.get("ImportProofEvidenceConsumed") is not True
        or contract.get("RawConversionFileConsumed") is not False
        or contract.get("RawO3dePreviewSourceConsumed") is not False
    ):
        raise AssetBrowserPaneError(
            "Pane model must consume import-proof evidence only."
        )

    stage = document.get("PreviewStageStatus")
    if not isinstance(stage, dict):
        raise AssetBrowserPaneError("PreviewStageStatus is required.")
    for key in (
        "O3deAssetBrowserEntryCreated",
        "TypedAuthoringBindingCreated",
        "FunctionCompleteAllowed",
    ):
        if stage.get(key) is not False:
            raise AssetBrowserPaneError(f"Pane-model stage escalation: {key}")

    authority = document.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise AssetBrowserPaneError("OperationalAuthority is required.")
    for key in FALSE_KEYS:
        if authority.get(key) is not False:
            raise AssetBrowserPaneError(f"Pane-model authority escalation: {key}")

    entries = document.get("PaneEntries")
    if (
        not isinstance(entries, list)
        or not entries
        or len(entries) > MAX_ENTRIES
    ):
        raise AssetBrowserPaneError(
            f"PaneEntries must contain 1 to {MAX_ENTRIES} entries."
        )

    seen: set[str] = set()
    for entry in entries:
        if not isinstance(entry, dict):
            raise AssetBrowserPaneError("PaneEntries entries must be objects.")
        entry_id = require_id(entry.get("PaneEntryId"), "PaneEntryId")
        if entry_id in seen:
            raise AssetBrowserPaneError("Duplicate PaneEntryId.")
        seen.add(entry_id)
        policy = entry.get("SelectionPolicy")
        if not isinstance(policy, dict):
            raise AssetBrowserPaneError("SelectionPolicy is required.")
        for key in (
            "CanCreateTypedAuthoringBinding",
            "CatalogPromotionAllowed",
            "RuntimePermissionGranted",
            "RepositoryCommitAllowed",
            "RedistributionAllowed",
        ):
            if policy.get(key) is not False:
                raise AssetBrowserPaneError(
                    f"Pane-entry authority escalation: {key}"
                )
        if policy.get("RequiresExplicitBindingStep") is not True:
            raise AssetBrowserPaneError(
                "Pane entry must require an explicit binding step."
            )

        kind = entry.get("EntryKind")
        if kind == "o3de-preview-product":
            asset_ids = entry.get("ProductAssetIds")
            cache_paths = entry.get("ProductCachePaths")
            if not isinstance(asset_ids, list) or not asset_ids:
                raise AssetBrowserPaneError(
                    "Product entry requires ProductAssetIds."
                )
            for asset_id in asset_ids:
                require_asset_id(asset_id, "ProductAssetId")
            if not isinstance(cache_paths, list) or not cache_paths:
                raise AssetBrowserPaneError(
                    "Product entry requires ProductCachePaths."
                )
            if not all(
                isinstance(path, str) and path.startswith("$assetcache/")
                for path in cache_paths
            ):
                raise AssetBrowserPaneError(
                    "ProductCachePaths must use $assetcache tokens."
                )
        elif kind == "o3de-import-failure":
            if entry.get("PreviewAvailability") != "import-failed":
                raise AssetBrowserPaneError(
                    "Import-failure entry must remain import-failed."
                )
        else:
            raise AssetBrowserPaneError(f"Unsupported pane entry kind: {kind}")

    if workspace_path is not None:
        profile = load_profile(workspace_path)
        profile_bound(document, profile, "Pane model")
        if not inside(model_path.resolve(strict=False), profile["ExtractedDataPath"]):
            raise AssetBrowserPaneError(
                "Pane model must remain inside ExtractedDataPath."
            )

    if import_proof_path is not None:
        proof = read_json(import_proof_path)
        if document.get("SourceImportProofId") != proof.get("ImportProofId"):
            raise AssetBrowserPaneError("SourceImportProofId mismatch.")

    no_private_paths(document)
    return document


def write_workspace(root: Path) -> Path:
    (root / "game" / "FoA").mkdir(parents=True, exist_ok=True)
    (root / "workspace" / "Extracted").mkdir(parents=True, exist_ok=True)
    workspace = {
        "SchemaVersion": 1,
        "WorkspaceId": "fixture.workspace",
        "DisplayName": "Fixture",
        "RootPath": "./workspace",
        "OutputPath": "./workspace/Build",
        "StagingPath": "./workspace/Staging",
        "DeploymentPath": "./workspace/Deploy",
        "ActiveGameProfileId": "foa.mono.fixture",
        "GameProfiles": [
            {
                "ProfileId": "foa.mono.fixture",
                "DisplayName": "Fixture",
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
            }
        ],
    }
    path = root / "workspace.tgworkspace.json"
    path.write_bytes(pretty(workspace))
    return path


def write_proof(root: Path) -> Path:
    proof_root = (
        root
        / "workspace"
        / "Extracted"
        / "PreviewArtifacts"
        / "O3DE"
        / "o3de.preview.foa.mono.fixture.synthetic"
        / "ImportProofs"
        / "proof.fixture"
    )
    (proof_root / "logs").mkdir(parents=True, exist_ok=True)
    log_payload = b"asset processor log\n"
    (proof_root / "logs" / "asset_processor.log").write_bytes(log_payload)

    proof = {
        "SchemaVersion": 1,
        "DocumentKind": PROOF_KIND,
        "ImportProofId": "o3de.import-proof.foa.mono.fixture.synthetic",
        "ProfileId": "foa.mono.fixture",
        "GameVersion": "1.23.401",
        "Branch": "mono",
        "RuntimeTarget": "Mono",
        "CapturedAt": "2026-07-28T00:00:00Z",
        "SourceConversionId": "o3de.preview.foa.mono.fixture.synthetic",
        "SourceHandoffId": "preview.handoff.foa.mono.fixture.synthetic",
        "SourceIndexId": "visual.index.foa.mono.fixture.synthetic",
        "PrimarySourceAssetRecordId": "visual.asset.foa.mono.fixture.synthetic",
        "SourceDependencies": [
            {
                "SourceAssetRecordId": "visual.asset.foa.mono.fixture.synthetic",
                "NativeAssetRef": "$install/Tainted Grail_Data/LooseIcons/iron.png",
                "DependencyRole": "primary",
            }
        ],
        "PreviewStageStatus": {
            "O3deAssetProcessorInvocationObserved": True,
            "GeneratedO3dePreviewProductEvidence": True,
            "AssetBrowserEntryCreated": False,
            "TypedAuthoringBindingCreated": False,
            "FunctionCompleteAllowed": False,
        },
        "ImportedProducts": [
            {
                "PreviewProductEvidenceId": "o3de.preview-product.fixture",
                "PreviewProductId": "o3de.preview-product.fixture.texture",
                "O3dePreviewSourceId": "o3de.source.fixture",
                "PrimarySourceAssetRecordId": "visual.asset.foa.mono.fixture.synthetic",
                "AssetId": "{11111111-2222-3333-4444-555555555555}:00000001",
                "ProductKind": "texture-preview-product",
                "BuilderId": "o3de.asset-processor.fixture",
                "ProductCachePath": "$assetcache/pc/textures/iron.dds.streamingimage",
                "ProductSha256": sha256_bytes(b"product"),
                "ProductByteSize": len(b"product"),
                "GeneratedByO3deAssetProcessor": True,
                "LocalOnly": True,
                "RepositoryCommitAllowed": False,
                "RedistributionAllowed": False,
                "RuntimePermissionGranted": False,
                "TypedAuthoringBindingCreated": False,
                "DisplayName": "iron.png",
            }
        ],
        "ImportFailures": [
            {
                "FailureId": "o3de.import-failure.fixture",
                "O3dePreviewSourceId": "o3de.source.failed",
                "PrimarySourceAssetRecordId": "visual.asset.foa.mono.fixture.failed",
                "Code": "ap-build-failed",
                "Message": "Synthetic import failure.",
            }
        ],
        "ImportLogs": [
            {
                "LogEvidenceId": "o3de.import-log.fixture",
                "Path": "$importproof/logs/asset_processor.log",
                "Sha256": sha256_bytes(log_payload),
                "ByteSize": len(log_payload),
            }
        ],
        "OperationalAuthority": {
            "RuntimeInvocationAllowed": False,
            "GameMutationAllowed": False,
            "SaveAccessAllowed": False,
            "CatalogPromotionAllowed": False,
            "RuntimePermissionGranted": False,
            "UnityInvoked": False,
            "AssetBrowserEntryCreated": False,
            "TypedAuthoringBindingCreated": False,
            "DeploymentAllowed": False,
            "RepositoryCommitAllowed": False,
            "RedistributionAllowed": False,
            "FunctionCompleteAllowed": False,
            "AssetProcessorInvocationPerformedByThisTool": False,
        },
    }
    path = proof_root / "foa-o3de-asset-processor-import-proof.json"
    path.write_bytes(pretty(proof))
    return path


def generate_fixture(
    output: Path,
    *,
    replace: bool = False,
) -> dict[str, Any]:
    if output.exists():
        if replace:
            shutil.rmtree(output) if output.is_dir() else output.unlink()
        else:
            raise AssetBrowserPaneError(f"Fixture output already exists: {output}")
    workspace = write_workspace(output)
    proof = write_proof(output)
    model, path = build_model(
        workspace,
        proof,
        captured_at="2026-07-28T00:00:00Z",
    )
    verify_model(path, workspace_path=workspace, import_proof_path=proof)
    return {
        "AssetBrowserModelId": model["AssetBrowserModelId"],
        "PaneEntryCount": len(model["PaneEntries"]),
        "ModelPath": str(path),
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    build = subparsers.add_parser("build")
    build.add_argument("--workspace", type=Path, required=True)
    build.add_argument("--import-proof", type=Path, required=True)
    build.add_argument("--output-root", type=Path)
    build.add_argument("--captured-at")
    build.add_argument("--replace", action="store_true")

    verify = subparsers.add_parser("verify")
    verify.add_argument("--input", type=Path, required=True)
    verify.add_argument("--workspace", type=Path)
    verify.add_argument("--import-proof", type=Path)

    fixture = subparsers.add_parser("fixture")
    fixture.add_argument("--output", type=Path, required=True)
    fixture.add_argument("--replace", action="store_true")

    args = parser.parse_args(argv)
    try:
        if args.command == "fixture":
            result = generate_fixture(args.output, replace=args.replace)
            print(
                "FoA Asset Browser pane fixture wrote "
                f"{result['PaneEntryCount']} entries to {result['ModelPath']}."
            )
        elif args.command == "build":
            model, path = build_model(
                args.workspace,
                args.import_proof,
                output_root=args.output_root,
                captured_at=args.captured_at,
                replace=args.replace,
            )
            print(
                "FoA Asset Browser pane model wrote "
                f"{len(model['PaneEntries'])} entries to {path}."
            )
        else:
            model = verify_model(
                args.input,
                workspace_path=args.workspace,
                import_proof_path=args.import_proof,
            )
            print(
                "FoA Asset Browser pane model verified: "
                f"{model['AssetBrowserModelId']} with "
                f"{len(model['PaneEntries'])} entries."
            )
    except AssetBrowserPaneError as exc:
        print(f"FoA Asset Browser pane model failed: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
