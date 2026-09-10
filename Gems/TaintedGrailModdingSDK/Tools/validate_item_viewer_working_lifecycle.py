#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Static boundary validation for the working item-viewer lifecycle."""
from __future__ import annotations

import ast
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise RuntimeError(f"Missing {label}: {needle}")


def reject(text: str, needle: str, label: str) -> None:
    if needle in text:
        raise RuntimeError(f"Forbidden {label}: {needle}")


def validate_item_viewer(root: Path = ROOT) -> None:
    workflow = (root / ".github" / "workflows" / "item-viewer-windows-validation.yml").read_text(encoding="utf-8")
    engine_checkout = re.search(
        r"(?ms)^      - name: Check out pinned O3DE source\s*\n.*?(?=^      - name:|\Z)", workflow
    )
    if engine_checkout is None or not re.search(r"(?m)^          lfs: false\s*$", engine_checkout.group()):
        raise RuntimeError("Pinned O3DE LFS assets must be fetched after checkout so .lfsconfig is available.")
    require(engine_checkout.group(), "repository: o3de/o3de", "pinned engine checkout owner")
    lfs_download = re.search(
        r"(?ms)^      - name: Download pinned O3DE LFS assets\s*\n.*?(?=^      - name:|\Z)", workflow
    )
    build_gate = workflow.find("      - name: Run working Item Viewer lifecycle gate")
    if lfs_download is None or not engine_checkout.end() <= lfs_download.start() < build_gate:
        raise RuntimeError("Download Git LFS assets after engine checkout and before the Editor build.")
    require(lfs_download.group(), "git -C o3de lfs pull", "pinned engine LFS download")
    require(lfs_download.group(), "if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }", "LFS failure propagation")

    code = root / "Gems" / "TaintedGrailModdingSDK" / "Code"
    source = code / "Source"
    tools = root / "Gems" / "TaintedGrailModdingSDK" / "Tools"
    enhancer = (source / "ItemVisualLifecycleWidget.cpp").read_text(encoding="utf-8")
    refresh_service = (source / "AssetBrowserPreviewRefreshService.cpp").read_text(encoding="utf-8")
    refresh_adapter = (tools / "foa_asset_browser_pane_refresh.py").read_text(encoding="utf-8")
    installer = (source / "ItemVisualSelectorInstallerSystemComponent.cpp").read_text(encoding="utf-8")
    selector = (source / "ItemVisualSelectorWidget.cpp").read_text(encoding="utf-8")
    editor_manifest = (code / "taintedgrailmoddingsdk_editor_files.cmake").read_text(encoding="utf-8")
    framework_manifest = (code / "taintedgrailmoddingsdk_framework_files.cmake").read_text(encoding="utf-8")
    code_cmake = (code / "CMakeLists.txt").read_text(encoding="utf-8")
    smoke = (tools / "editor_tests" / "alpha_item_viewer_live_smoke.py").read_text(encoding="utf-8")

    require(enhancer, "ProductThumbnailKey", "official O3DE product thumbnail key")
    require(enhancer, "ThumbnailWidget", "official O3DE thumbnail widget")
    require(enhancer, "FOAItemViewerThumbnailGrid", "visual thumbnail grid")
    require(enhancer, "QSettings", "close/reopen state")
    require(enhancer, "m_table->selectRow", "grid-to-validated-selection bridge")
    require(enhancer, "PreviewArtifacts/AssetBrowser", "automatic generated-index discovery root")
    require(enhancer, "foa-asset-browser-pane-model.json", "canonical automatic model discovery")
    require(enhancer, "CandidateMatchesActiveProfile", "exact-profile candidate filtering")
    require(enhancer, "PathPolicyService::IsCanonicalPathContained", "contained automatic discovery")
    require(enhancer, 'tr("Refresh Assets")', "user-facing refresh action")
    require(enhancer, "RefreshActiveProfileModel", "refresh-to-generation service integration")
    require(enhancer, "m_modelPath->hide();", "hidden internal model path")
    require(enhancer, "m_chooseModel->hide();", "hidden raw JSON chooser")
    require(enhancer, 'settings.remove(prefix + QStringLiteral("modelPath"));', "internal model-path persistence removal")

    require(refresh_service, "EditorPythonRunnerRequestBus::BroadcastResult", "embedded Python execution")
    require(refresh_service, "foa_asset_browser_pane_refresh.py", "embedded refresh adapter")
    require(refresh_service, 'ownedArgs.emplace_back("--workspace")', "active workspace generator binding")
    require(refresh_service, 'ownedArgs.emplace_back("--import-proof")', "reviewed import-proof generator input")
    require(refresh_service, 'ownedArgs.emplace_back("--replace")', "idempotent generated-model refresh")
    require(refresh_service, "SourceImportProofId", "generated model proof binding")
    require(refresh_service, "PathPolicyService::IsCanonicalPathContained", "refresh evidence containment")
    require(refresh_service, "FindLatestImportProof", "exact-profile import-proof discovery")
    require(refresh_service, "FindLatestPaneModel", "post-generation exact-profile model resolution")

    require(refresh_adapter, "import foa_asset_browser_pane_model as pane", "single pane-model generator owner")
    require(refresh_adapter, "pane.build_model", "shared pane-model build call")
    require(refresh_adapter, "pane.verify_model", "shared pane-model verification call")
    reject(refresh_adapter, "subprocess", "external process use in embedded adapter")
    # Inspect executable references, excluding comments and explanatory docstrings.
    for node in ast.walk(ast.parse(refresh_adapter)):
        if (isinstance(node, ast.Name) and node.id == "SystemExit") or (
            isinstance(node, ast.Attribute) and node.attr == "SystemExit"
        ):
            raise RuntimeError("Forbidden process-exit contract in embedded adapter: SystemExit")

    require(code_cmake, "TG_SDK_ASSET_BROWSER_PANE_REFRESH_TOOL_SOURCE", "developer-checkout refresh-adapter path")
    require(code_cmake, "ly_install_files", "installed refresh tooling packaging")
    require(code_cmake, "DESTINATION\n        scripts/foa-sdk\n", "private installed refresh tooling location")
    require(code_cmake, "../Tools/foa_asset_browser_pane_refresh.py", "installed embedded refresh adapter")
    require(code_cmake, "../Tools/foa_asset_browser_pane_model.py", "installed pane-model generator")

    require(installer, "new ItemVisualLifecycleEnhancer(selector)", "direct lifecycle integration")
    require(selector, "PreviewerFrame", "registered live O3DE previewer")
    require(selector, "UpsertEconomyItemProfile", "existing save path")
    require(selector, "LoadedModelFileMatches", "existing source-index drift check")
    require(selector, "LoadedModelMatchesActiveProfile", "existing exact-profile check")

    required_framework_files = (
        "Source/AssetBrowserPreviewRefreshService.cpp",
        "Source/AssetBrowserPreviewRefreshService.h",
    )
    for relative in required_framework_files:
        require(framework_manifest, relative, f"Framework production ownership for {relative}")
        reject(editor_manifest, relative, f"duplicate Editor ownership for {relative}")

    required_editor_files = (
        "Source/ItemVisualLifecycleEnhancer.h",
        "Source/ItemVisualLifecycleWidget.cpp",
        "Source/ItemVisualSelectionRestoreBridge.h",
        "Source/ItemVisualSelectionRestoreWidget.cpp",
        "Source/ItemVisualSelectorInstallerSystemComponent.cpp",
        "Source/ItemVisualSelectorInstallerSystemComponent.h",
        "Source/ItemVisualSelectorWidget.cpp",
        "Source/ItemVisualSelectorWidget.h",
    )
    for relative in required_editor_files:
        require(editor_manifest, relative, f"Editor production build registration for {relative}")

    require(smoke, "FOAItemViewerThumbnailGrid", "Windows grid evidence")
    require(smoke, "Refresh Assets", "Windows simplified refresh evidence")
    require(smoke, "refresh_regenerated_model", "Windows refresh-generation evidence")
    require(smoke, "refresh_loaded_products", "Windows refreshed-product-load evidence")
    require(smoke, "selection_restored_after_reopen", "Windows selection restore evidence")
    require(smoke, "internal_model_controls_hidden", "Windows internal-control hiding evidence")
    require(smoke, "pane.close()", "Windows close/reopen exercise")
    require(smoke, "general.open_pane(PANE_NAME)", "Windows pane reopen")

    forbidden = (
        "PromoteCandidateEvidence",
        "PromoteEvidenceToCatalog",
        "RuntimeInvocationAllowed = true",
        "DeploymentAllowed = true",
        'settings.setValue(prefix + QStringLiteral("modelPath")',
    )
    for token in forbidden:
        if token in enhancer:
            raise RuntimeError(f"Lifecycle enhancer contains forbidden authority or stale UX state: {token}")

    external_process_tokens = (
        "QProcess",
        "std::system",
        "CreateProcess",
        "ShellExecute",
        "subprocess",
    )
    for token in external_process_tokens:
        if token in refresh_service:
            raise RuntimeError(f"Item visual refresh must remain inside the Editor process: {token}")


def main() -> None:
    validate_item_viewer(ROOT)
    print("Item viewer working-lifecycle static validation passed.")


if __name__ == "__main__":
    main()
