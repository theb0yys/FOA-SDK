#!/usr/bin/env python3
"""Static boundary validation for the working item-viewer lifecycle."""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise RuntimeError(f"Missing {label}: {needle}")


def validate_item_viewer(root: Path = ROOT) -> None:
    code = root / "Gems" / "TaintedGrailModdingSDK" / "Code"
    source = code / "Source"
    enhancer = (source / "ItemVisualLifecycleWidget.cpp").read_text(encoding="utf-8")
    installer = (source / "ItemVisualSelectorInstallerSystemComponent.cpp").read_text(encoding="utf-8")
    selector = (source / "ItemVisualSelectorWidget.cpp").read_text(encoding="utf-8")
    manifest = (code / "taintedgrailmoddingsdk_editor_files.cmake").read_text(encoding="utf-8")
    smoke = (
        root
        / "Gems"
        / "TaintedGrailModdingSDK"
        / "Tools"
        / "editor_tests"
        / "alpha_item_viewer_live_smoke.py"
    ).read_text(encoding="utf-8")

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
    require(enhancer, "m_modelPath->hide();", "hidden internal model path")
    require(enhancer, "m_chooseModel->hide();", "hidden raw JSON chooser")
    require(enhancer, 'settings.remove(prefix + QStringLiteral("modelPath"));', "internal model-path persistence removal")
    require(installer, "new ItemVisualLifecycleEnhancer(selector)", "direct lifecycle integration")
    require(selector, "PreviewerFrame", "registered live O3DE previewer")
    require(selector, "UpsertEconomyItemProfile", "existing save path")
    require(selector, "LoadedModelFileMatches", "existing source-index drift check")
    require(selector, "LoadedModelMatchesActiveProfile", "existing exact-profile check")

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
        require(manifest, relative, f"production build registration for {relative}")

    require(smoke, "FOAItemViewerThumbnailGrid", "Windows grid evidence")
    require(smoke, "Refresh Assets", "Windows simplified refresh evidence")
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


def main() -> None:
    validate_item_viewer(ROOT)
    print("Item viewer working-lifecycle static validation passed.")


if __name__ == "__main__":
    main()
