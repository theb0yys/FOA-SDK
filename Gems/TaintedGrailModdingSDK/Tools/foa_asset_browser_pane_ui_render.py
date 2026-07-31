#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Bounded Asset Browser pane UI rendering from pane-model evidence only.

Consumes `foa-asset-browser-pane-model.json` and emits local-only static UI
render evidence. This does not mutate O3DE's Asset Browser, create typed
item/recipe bindings, grant runtime permission, or claim function completion.
"""
from __future__ import annotations

import argparse
import hashlib
import html
import json
import os
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

TOOL_ID = "foa.asset-browser-pane-ui-render"
TOOL_VERSION = "0.1.0"
DOCUMENT_KIND = "foa-asset-browser-pane-ui-render"
MODEL_KIND = "foa-asset-browser-pane-model"
DEFAULT_RENDER_NAME = "foa-asset-browser-pane-ui-render.json"
MAX_ENTRY_COUNT = 10000
FALSE_KEYS = (
    "RuntimeInvocationAllowed",
    "GameMutationAllowed",
    "SaveAccessAllowed",
    "CatalogPromotionAllowed",
    "RuntimePermissionGranted",
    "UnityInvoked",
    "O3deAssetProcessorInvoked",
    "O3deEditorPaneMutated",
    "O3deAssetBrowserMutated",
    "AssetBrowserEntryCreated",
    "TypedAuthoringBindingCreated",
    "TypedSelectorCreated",
    "DeploymentAllowed",
    "RepositoryCommitAllowed",
    "RedistributionAllowed",
    "FunctionCompleteAllowed",
)
ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
PRIVATE_RE = re.compile(r"(^/|^~/|^[A-Za-z]:[\\/]|^\\\\)")
UI_TOKEN_RE = re.compile(r"^\$uirender(/[^\\\r\n]*)?$")
ASSET_CACHE_RE = re.compile(r"^\$assetcache(/[^\\\r\n]*)?$")

class AssetBrowserUiRenderError(RuntimeError):
    """Raised when bounded Asset Browser UI rendering fails."""

def pretty(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")

def canonical(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")

def sha256_bytes(payload: bytes) -> str:
    return "sha256:" + hashlib.sha256(payload).hexdigest()

def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8", errors="strict"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise AssetBrowserUiRenderError(f"Invalid UTF-8 JSON file {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise AssetBrowserUiRenderError(f"JSON document must be an object: {path}")
    return value

def require_id(value: Any, label: str) -> str:
    if not isinstance(value, str) or not ID_RE.match(value):
        raise AssetBrowserUiRenderError(f"{label} must be a lowercase stable identifier.")
    return value

def require_utc(value: Any, label: str) -> str:
    if not isinstance(value, str) or not UTC_RE.match(value):
        raise AssetBrowserUiRenderError(f"{label} must use whole-second UTC format.")
    datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    return value

def no_private(value: Any, label: str = "document") -> None:
    if isinstance(value, str):
        if PRIVATE_RE.search(value):
            raise AssetBrowserUiRenderError(f"{label} contains an absolute or private path: {value}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            no_private(child, f"{label}[{index}]")
    elif isinstance(value, dict):
        for key, child in value.items():
            no_private(child, f"{label}.{key}")

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
    root = resolve(str(workspace.get("RootPath", "")), workspace_path.parent)
    active = require_id(workspace.get("ActiveGameProfileId"), "ActiveGameProfileId")
    profiles = workspace.get("GameProfiles")
    if not isinstance(profiles, list):
        raise AssetBrowserUiRenderError("Workspace GameProfiles must be an array.")
    matches = [p for p in profiles if isinstance(p, dict) and p.get("ProfileId") == active]
    if len(matches) != 1:
        raise AssetBrowserUiRenderError("Workspace ActiveGameProfileId must bind to exactly one profile.")
    profile = matches[0]
    runtime = profile.get("RuntimeTarget")
    if runtime not in {"Mono", "IL2CPP"}:
        raise AssetBrowserUiRenderError("RuntimeTarget must be Mono or IL2CPP.")
    install = resolve(str(profile.get("InstallPath", "")), workspace_path.parent)
    extracted = resolve(str(profile.get("ExtractedDataPath", "")), workspace_path.parent)
    if not install.is_dir():
        raise AssetBrowserUiRenderError("Configured FoA install path does not exist or is not a directory.")
    if not inside(extracted, root):
        raise AssetBrowserUiRenderError("ExtractedDataPath must remain inside workspace root.")
    return {"ProfileId": require_id(profile.get("ProfileId"), "ProfileId"), "GameVersion": str(profile.get("GameVersion", "")), "Branch": str(profile.get("Branch", "")), "RuntimeTarget": runtime, "InstallPath": install, "ExtractedDataPath": extracted}

def profile_bound(document: Mapping[str, Any], profile: Mapping[str, Any], label: str) -> None:
    for key in ("ProfileId", "GameVersion", "Branch", "RuntimeTarget"):
        if document.get(key) != profile[key]:
            raise AssetBrowserUiRenderError(f"{label} must match the exact active workspace profile.")

def validate_model(model: Mapping[str, Any], profile: Mapping[str, Any]) -> list[Mapping[str, Any]]:
    if model.get("SchemaVersion") != 1 or model.get("DocumentKind") != MODEL_KIND:
        raise AssetBrowserUiRenderError("Input must be foa-asset-browser-pane-model.json, not import proof or raw conversion.")
    profile_bound(model, profile, "Asset Browser pane model")
    contract = model.get("InputContract")
    if not isinstance(contract, dict) or contract.get("ImportProofEvidenceConsumed") is not True:
        raise AssetBrowserUiRenderError("Pane model must prove it consumed import-proof evidence.")
    if contract.get("RawConversionFileConsumed") is not False or contract.get("RawO3dePreviewSourceConsumed") is not False:
        raise AssetBrowserUiRenderError("Pane model must not consume raw conversion/source files.")
    stage = model.get("PreviewStageStatus")
    if not isinstance(stage, dict) or stage.get("AssetBrowserPaneModelEmitted") is not True:
        raise AssetBrowserUiRenderError("Pane model stage is incomplete.")
    for key in ("O3deAssetBrowserEntryCreated", "TypedAuthoringBindingCreated", "FunctionCompleteAllowed"):
        if stage.get(key) is not False:
            raise AssetBrowserUiRenderError(f"Pane model stage escalation: {key}")
    authority = model.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise AssetBrowserUiRenderError("Pane model OperationalAuthority is required.")
    for key, value in authority.items():
        if value is not False:
            raise AssetBrowserUiRenderError(f"Pane model authority escalation: {key}")
    entries = model.get("PaneEntries")
    if not isinstance(entries, list) or not entries:
        raise AssetBrowserUiRenderError("Pane model requires non-empty PaneEntries.")
    if len(entries) > MAX_ENTRY_COUNT:
        raise AssetBrowserUiRenderError(f"PaneEntries exceeds {MAX_ENTRY_COUNT}.")
    seen: set[str] = set()
    for entry in entries:
        if not isinstance(entry, dict):
            raise AssetBrowserUiRenderError("PaneEntries entries must be objects.")
        entry_id = require_id(entry.get("PaneEntryId"), "PaneEntryId")
        if entry_id in seen:
            raise AssetBrowserUiRenderError("Duplicate PaneEntryId.")
        seen.add(entry_id)
        policy = entry.get("SelectionPolicy")
        if not isinstance(policy, dict):
            raise AssetBrowserUiRenderError("Pane entry SelectionPolicy is required.")
        for key in ("CanCreateTypedAuthoringBinding", "CatalogPromotionAllowed", "RuntimePermissionGranted", "RepositoryCommitAllowed", "RedistributionAllowed"):
            if policy.get(key) is not False:
                raise AssetBrowserUiRenderError(f"Pane entry selection authority escalation: {key}")
        if policy.get("RequiresExplicitBindingStep") is not True:
            raise AssetBrowserUiRenderError("Pane entry must require explicit binding step.")
        if entry.get("EntryKind") == "o3de-preview-product":
            for token in entry.get("ProductCachePaths", []):
                if not isinstance(token, str) or not ASSET_CACHE_RE.match(token):
                    raise AssetBrowserUiRenderError("ProductCachePaths must use $assetcache tokens.")
    no_private(model, "asset browser pane model")
    return entries

def ui_token(path: Path, root: Path) -> str:
    relative = path.resolve(strict=False).relative_to(root.resolve(strict=False))
    if any(part in {"", ".", ".."} for part in relative.parts):
        raise AssetBrowserUiRenderError("UI render path contains unsafe segment.")
    return "$uirender/" + relative.as_posix()

def write_bytes(path: Path, payload: bytes) -> dict[str, Any]:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(payload)
    os.replace(temporary, path)
    return {"Sha256": sha256_bytes(payload), "ByteSize": len(payload)}

def render_entry(entry: Mapping[str, Any]) -> dict[str, Any]:
    kind = str(entry.get("EntryKind", "unknown"))
    preview = str(entry.get("PreviewAvailability", "unknown"))
    return {"UiEntryId": "assetbrowser.ui-entry." + hashlib.sha256(canonical([entry.get("PaneEntryId"), kind, preview])).hexdigest()[:16], "SourcePaneEntryId": entry.get("PaneEntryId", ""), "DisplayName": str(entry.get("DisplayName", "Unnamed preview asset")), "EntryKind": kind, "PreviewAvailability": preview, "IssueSeverity": str(entry.get("IssueSeverity", "none")), "VisibleInPane": True, "SelectableInRenderedPane": True, "CanCreateTypedAuthoringBinding": False, "RequiresExplicitBindingStep": True, "ProductAssetIds": list(entry.get("ProductAssetIds", [])), "ProductCachePaths": list(entry.get("ProductCachePaths", [])), "EvidenceRefs": list(entry.get("EvidenceRefs", [])), "Issues": list(entry.get("Issues", []))}

def html_page(model: Mapping[str, Any], entries: Sequence[Mapping[str, Any]]) -> bytes:
    rows = []
    for entry in entries:
        issues = "; ".join(html.escape(str(i.get("Message", i))) for i in entry.get("Issues", []) if isinstance(i, dict))
        rows.append("<tr>" + f"<td>{html.escape(str(entry['DisplayName']))}</td>" + f"<td>{html.escape(str(entry['EntryKind']))}</td>" + f"<td>{html.escape(str(entry['PreviewAvailability']))}</td>" + f"<td>{html.escape(str(entry['IssueSeverity']))}</td>" + f"<td>{html.escape(', '.join(map(str, entry.get('ProductCachePaths', []))))}</td>" + f"<td>{html.escape(issues)}</td>" + "</tr>")
    source_model = html.escape(str(model.get("AssetBrowserModelId", "")))
    doc = (
        "<!doctype html>\n"
        '<html lang="en"><head><meta charset="utf-8"><title>FOA Asset Browser Preview Pane</title>'
        "<style>body{font-family:system-ui,sans-serif;margin:1.5rem}"
        "table{border-collapse:collapse;width:100%}"
        "th,td{border:1px solid #999;padding:.45rem;text-align:left}"
        ".boundary{border:1px solid #555;padding:.75rem;margin-bottom:1rem}</style></head><body>\n"
        '<h1>FOA Asset Browser Preview Pane</h1><div class="boundary"><strong>Boundary:</strong> '
        "static local UI render only. No ODE Asset Browser mutation, no typed bindings, no runtime authority.</div>\n"
        f"<p>Source model: {source_model}</p>"
        "<table><thead><tr><th>Name</th><th>Kind</th><th>Preview</th><th>Severity</th>"
        f"<th>Product cache path</th><th>Issues</th></tr></thead><tbody>{chr(10).join(rows)}</tbody>"
        "</table></body></html>\n"
    )
    return doc.encode("utf-8")

def build_render(workspace_path: Path, model_path: Path, *, output_root: Path | None = None, captured_at: str | None = None, replace: bool = False) -> tuple[dict[str, Any], Path]:
    profile = load_profile(workspace_path)
    model = read_json(model_path)
    entries = validate_model(model, profile)
    if not inside(model_path.resolve(strict=False), profile["ExtractedDataPath"]):
        raise AssetBrowserUiRenderError("Pane model must remain inside ExtractedDataPath.")
    captured_at = require_utc(captured_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "CapturedAt")
    render_id = "assetbrowser.render." + profile["ProfileId"] + "." + hashlib.sha256(canonical([model.get("AssetBrowserModelId"), [(e.get("PaneEntryId"), e.get("PreviewAvailability")) for e in entries], captured_at])).hexdigest()[:16]
    root = (output_root.resolve(strict=False) if output_root is not None else (profile["ExtractedDataPath"] / "PreviewArtifacts" / "AssetBrowserUI" / render_id).resolve(strict=False))
    if not inside(root, profile["ExtractedDataPath"]):
        raise AssetBrowserUiRenderError("UI render output root must remain inside ExtractedDataPath.")
    if root.exists() and any(root.iterdir()):
        if replace:
            shutil.rmtree(root)
        else:
            raise AssetBrowserUiRenderError(f"UI render output root is not empty: {root}")
    root.mkdir(parents=True, exist_ok=True)
    render_entries = sorted([render_entry(entry) for entry in entries], key=lambda item: item["UiEntryId"])
    data_path = root / "ui" / "asset-browser-pane-data.json"
    data_payload = {"SchemaVersion": 1, "DocumentKind": "foa-asset-browser-pane-ui-data", "RenderId": render_id, "SourceAssetBrowserModelId": model.get("AssetBrowserModelId", ""), "Entries": render_entries}
    data_written = write_bytes(data_path, pretty(data_payload))
    html_path = root / "ui" / "asset-browser-pane.html"
    html_written = write_bytes(html_path, html_page(model, render_entries))
    artifacts = [
        {"RenderArtifactId": "assetbrowser.render-artifact." + hashlib.sha256(canonical([render_id, "data", data_written["Sha256"]])).hexdigest()[:16], "Role": "ui-data", "Path": ui_token(data_path, root), "MediaType": "application/json", **data_written, "LocalOnly": True, "RepositoryCommitAllowed": False, "RedistributionAllowed": False},
        {"RenderArtifactId": "assetbrowser.render-artifact." + hashlib.sha256(canonical([render_id, "html", html_written["Sha256"]])).hexdigest()[:16], "Role": "static-html", "Path": ui_token(html_path, root), "MediaType": "text/html", **html_written, "LocalOnly": True, "RepositoryCommitAllowed": False, "RedistributionAllowed": False},
    ]
    document = {"SchemaVersion": 1, "DocumentKind": DOCUMENT_KIND, "AssetBrowserUiRenderId": render_id, "ProfileId": profile["ProfileId"], "GameVersion": profile["GameVersion"], "Branch": profile["Branch"], "RuntimeTarget": profile["RuntimeTarget"], "ToolId": TOOL_ID, "ToolVersion": TOOL_VERSION, "CapturedAt": captured_at, "PreviewIntent": "editor-preview-only", "SourceAssetBrowserModelId": model.get("AssetBrowserModelId", ""), "SourceImportProofId": model.get("SourceImportProofId", ""), "SourceConversionId": model.get("SourceConversionId", ""), "InputContract": {"AssetBrowserPaneModelConsumed": True, "ImportProofConsumedDirectly": False, "RawConversionFileConsumed": False, "RawO3dePreviewSourceConsumed": False}, "PreviewStageStatus": {"AssetBrowserPaneModelConsumed": True, "BoundedPaneUiRendered": True, "O3deEditorPaneMutated": False, "O3deAssetBrowserMutated": False, "AssetBrowserEntryCreated": False, "TypedAuthoringBindingCreated": False, "TypedSelectorCreated": False, "FunctionCompleteAllowed": False, "NextRequiredStages": ["3d-preview-viewport", "item-recipe-visual-selectors"]}, "RenderRoot": "$uirender", "UiEntries": render_entries, "RenderArtifacts": artifacts, "Issues": [], "OperationalAuthority": {key: False for key in FALSE_KEYS}}
    no_private(document)
    manifest_path = root / DEFAULT_RENDER_NAME
    manifest_path.write_bytes(pretty(document))
    return document, manifest_path

def token_file(token: str, root: Path) -> Path:
    if not isinstance(token, str) or not UI_TOKEN_RE.match(token):
        raise AssetBrowserUiRenderError("Render artifact path must use $uirender token.")
    path = (root / token[len("$uirender"):].lstrip("/")).resolve(strict=False)
    if not inside(path, root):
        raise AssetBrowserUiRenderError("Render artifact path escaped render root.")
    return path

def verify_render(path: Path, *, workspace_path: Path | None = None, model_path: Path | None = None, render_root: Path | None = None) -> dict[str, Any]:
    document = read_json(path)
    if document.get("SchemaVersion") != 1 or document.get("DocumentKind") != DOCUMENT_KIND:
        raise AssetBrowserUiRenderError("Input is not an Asset Browser pane UI render manifest.")
    require_id(document.get("AssetBrowserUiRenderId"), "AssetBrowserUiRenderId")
    require_utc(document.get("CapturedAt"), "CapturedAt")
    contract = document.get("InputContract")
    if not isinstance(contract, dict) or contract.get("AssetBrowserPaneModelConsumed") is not True:
        raise AssetBrowserUiRenderError("UI render must consume pane model only.")
    for key in ("ImportProofConsumedDirectly", "RawConversionFileConsumed", "RawO3dePreviewSourceConsumed"):
        if contract.get(key) is not False:
            raise AssetBrowserUiRenderError(f"UI render consumed forbidden input: {key}")
    stage = document.get("PreviewStageStatus")
    if not isinstance(stage, dict):
        raise AssetBrowserUiRenderError("PreviewStageStatus is required.")
    for key in ("O3deEditorPaneMutated", "O3deAssetBrowserMutated", "AssetBrowserEntryCreated", "TypedAuthoringBindingCreated", "TypedSelectorCreated", "FunctionCompleteAllowed"):
        if stage.get(key) is not False:
            raise AssetBrowserUiRenderError(f"UI render stage escalation: {key}")
    authority = document.get("OperationalAuthority")
    if not isinstance(authority, dict):
        raise AssetBrowserUiRenderError("OperationalAuthority is required.")
    for key in FALSE_KEYS:
        if authority.get(key) is not False:
            raise AssetBrowserUiRenderError(f"UI render authority escalation: {key}")
    entries = document.get("UiEntries")
    if not isinstance(entries, list) or not entries:
        raise AssetBrowserUiRenderError("UiEntries are required.")
    seen_entries: set[str] = set()
    for entry in entries:
        if not isinstance(entry, dict):
            raise AssetBrowserUiRenderError("UiEntries entries must be objects.")
        entry_id = require_id(entry.get("UiEntryId"), "UiEntryId")
        if entry_id in seen_entries:
            raise AssetBrowserUiRenderError("Duplicate UiEntryId.")
        seen_entries.add(entry_id)
        if entry.get("CanCreateTypedAuthoringBinding") is not False or entry.get("RequiresExplicitBindingStep") is not True:
            raise AssetBrowserUiRenderError("UI entry must remain view/select only and require a later binding step.")
        for token in entry.get("ProductCachePaths", []):
            if not isinstance(token, str) or not ASSET_CACHE_RE.match(token):
                raise AssetBrowserUiRenderError("UI entry product cache paths must stay tokenized.")
    root = render_root.resolve(strict=False) if render_root is not None else path.parent.resolve(strict=False)
    artifacts = document.get("RenderArtifacts")
    if not isinstance(artifacts, list) or not artifacts:
        raise AssetBrowserUiRenderError("RenderArtifacts are required.")
    seen_artifacts: set[str] = set()
    for artifact in artifacts:
        if not isinstance(artifact, dict):
            raise AssetBrowserUiRenderError("RenderArtifacts entries must be objects.")
        artifact_id = require_id(artifact.get("RenderArtifactId"), "RenderArtifactId")
        if artifact_id in seen_artifacts:
            raise AssetBrowserUiRenderError("Duplicate RenderArtifactId.")
        seen_artifacts.add(artifact_id)
        for key in ("RepositoryCommitAllowed", "RedistributionAllowed"):
            if artifact.get(key) is not False:
                raise AssetBrowserUiRenderError(f"Render artifact authority escalation: {key}")
        payload_file = token_file(str(artifact.get("Path", "")), root)
        if not payload_file.is_file():
            raise AssetBrowserUiRenderError(f"Render artifact payload missing: {artifact.get('Path')}")
        payload = payload_file.read_bytes()
        if artifact.get("ByteSize") != len(payload) or artifact.get("Sha256") != sha256_bytes(payload):
            raise AssetBrowserUiRenderError(f"Render artifact payload mismatch: {artifact.get('Path')}")
    if workspace_path is not None:
        profile = load_profile(workspace_path)
        profile_bound(document, profile, "UI render")
        if not inside(path.resolve(strict=False), profile["ExtractedDataPath"]):
            raise AssetBrowserUiRenderError("UI render manifest must remain inside ExtractedDataPath.")
    if model_path is not None and document.get("SourceAssetBrowserModelId") != read_json(model_path).get("AssetBrowserModelId"):
        raise AssetBrowserUiRenderError("SourceAssetBrowserModelId must match model input.")
    no_private(document)
    return document

def write_workspace(root: Path) -> Path:
    (root / "game" / "FoA").mkdir(parents=True, exist_ok=True)
    (root / "workspace" / "Extracted").mkdir(parents=True, exist_ok=True)
    workspace = {"SchemaVersion": 1, "WorkspaceId": "fixture.workspace", "DisplayName": "Fixture", "RootPath": "./workspace", "OutputPath": "./workspace/Build", "StagingPath": "./workspace/Staging", "DeploymentPath": "./workspace/Deploy", "ActiveGameProfileId": "foa.mono.fixture", "GameProfiles": [{"ProfileId": "foa.mono.fixture", "DisplayName": "Fixture", "InstallPath": "./game/FoA", "GameVersion": "1.23.401", "Branch": "mono", "RuntimeTarget": "Mono", "UnityVersion": "6000.0.64f1", "BepInExVersion": "5.4.23.3", "ManagedAssembliesPath": "", "PluginPath": "", "DiagnosticsPath": "./workspace/Diagnostics", "ExtractedDataPath": "./workspace/Extracted", "DlcScopes": ["base-game"]}]}
    path = root / "workspace.tgworkspace.json"
    path.write_bytes(pretty(workspace))
    return path

def write_model(root: Path) -> Path:
    model_root = root / "workspace" / "Extracted" / "PreviewArtifacts" / "AssetBrowser" / "assetbrowser.model.foa.mono.fixture.synthetic"
    model_root.mkdir(parents=True, exist_ok=True)
    model = {"SchemaVersion": 1, "DocumentKind": MODEL_KIND, "AssetBrowserModelId": "assetbrowser.model.foa.mono.fixture.synthetic", "ProfileId": "foa.mono.fixture", "GameVersion": "1.23.401", "Branch": "mono", "RuntimeTarget": "Mono", "CapturedAt": "2026-07-28T00:00:00Z", "SourceImportProofId": "o3de.importproof.foa.mono.fixture.synthetic", "SourceConversionId": "o3de.preview.foa.mono.fixture.synthetic", "InputContract": {"ImportProofEvidenceConsumed": True, "RawConversionFileConsumed": False, "RawO3dePreviewSourceConsumed": False}, "PreviewStageStatus": {"ImportProofEvidenceConsumed": True, "AssetBrowserPaneModelEmitted": True, "AssetBrowserPaneEntriesEmitted": True, "O3deAssetBrowserEntryCreated": False, "TypedAuthoringBindingCreated": False, "FunctionCompleteAllowed": False}, "PaneEntries": [{"PaneEntryId": "assetbrowser.entry.fixture.product", "EntryKind": "o3de-preview-product", "DisplayName": "iron.png", "PreviewAvailability": "product-imported", "IssueSeverity": "none", "ProductAssetIds": ["o3de.product.fixture.texture"], "ProductCachePaths": ["$assetcache/pc/textures/iron.dds.streamingimage"], "EvidenceRefs": ["o3de.product-evidence.fixture"], "Issues": [], "SelectionPolicy": {"SelectableInPane": True, "CanCreateTypedAuthoringBinding": False, "RequiresExplicitBindingStep": True, "CatalogPromotionAllowed": False, "RuntimePermissionGranted": False, "RepositoryCommitAllowed": False, "RedistributionAllowed": False}}, {"PaneEntryId": "assetbrowser.entry.fixture.failure", "EntryKind": "o3de-import-failure", "DisplayName": "failed preview", "PreviewAvailability": "import-failed", "IssueSeverity": "error", "ProductAssetIds": [], "ProductCachePaths": [], "EvidenceRefs": ["o3de.import-failure.fixture"], "Issues": [{"Code": "ap-build-failed", "Message": "Synthetic import failure."}], "SelectionPolicy": {"SelectableInPane": True, "CanCreateTypedAuthoringBinding": False, "RequiresExplicitBindingStep": True, "CatalogPromotionAllowed": False, "RuntimePermissionGranted": False, "RepositoryCommitAllowed": False, "RedistributionAllowed": False}}], "OperationalAuthority": {"RuntimeInvocationAllowed": False, "GameMutationAllowed": False, "SaveAccessAllowed": False, "CatalogPromotionAllowed": False, "RuntimePermissionGranted": False, "UnityInvoked": False, "O3deAssetProcessorInvokedByThisTool": False, "O3deAssetBrowserMutated": False, "AssetBrowserEntryCreated": False, "TypedAuthoringBindingCreated": False, "DeploymentAllowed": False, "RepositoryCommitAllowed": False, "RedistributionAllowed": False, "FunctionCompleteAllowed": False}}
    path = model_root / "foa-asset-browser-pane-model.json"
    path.write_bytes(pretty(model))
    return path

def generate_fixture(output: Path, *, replace: bool = False) -> dict[str, Any]:
    if output.exists():
        if replace:
            shutil.rmtree(output) if output.is_dir() else output.unlink()
        else:
            raise AssetBrowserUiRenderError(f"Fixture output is not empty: {output}")
    workspace = write_workspace(output)
    model = write_model(output)
    render, render_path = build_render(workspace, model, captured_at="2026-07-28T00:00:00Z")
    verify_render(render_path, workspace_path=workspace, model_path=model)
    return {"AssetBrowserUiRenderId": render["AssetBrowserUiRenderId"], "UiEntryCount": len(render["UiEntries"]), "RenderPath": str(render_path)}

def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    r = sub.add_parser("render"); r.add_argument("--workspace", type=Path, required=True); r.add_argument("--model", type=Path, required=True); r.add_argument("--output-root", type=Path); r.add_argument("--captured-at"); r.add_argument("--replace", action="store_true")
    v = sub.add_parser("verify"); v.add_argument("--input", type=Path, required=True); v.add_argument("--workspace", type=Path); v.add_argument("--model", type=Path); v.add_argument("--render-root", type=Path)
    f = sub.add_parser("fixture"); f.add_argument("--output", type=Path, required=True); f.add_argument("--replace", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.command == "fixture":
            result = generate_fixture(args.output, replace=args.replace); print(f"FoA Asset Browser UI render fixture wrote {result['UiEntryCount']} entries to {result['RenderPath']}.")
        elif args.command == "render":
            render, path = build_render(args.workspace, args.model, output_root=args.output_root, captured_at=args.captured_at, replace=args.replace); print(f"FoA Asset Browser UI render wrote {len(render['UiEntries'])} entries to {path}.")
        else:
            render = verify_render(args.input, workspace_path=args.workspace, model_path=args.model, render_root=args.render_root); print(f"FoA Asset Browser UI render verified: {render['AssetBrowserUiRenderId']} with {len(render['UiEntries'])} entries.")
    except AssetBrowserUiRenderError as exc:
        print(f"FoA Asset Browser UI render failed: {exc}"); return 1
    return 0
if __name__ == "__main__":
    raise SystemExit(main())
