#!/usr/bin/env python3
"""Static boundary validation for the working item-viewer lifecycle."""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CODE = ROOT / "Gems" / "TaintedGrailModdingSDK" / "Code"
SOURCE = CODE / "Source"


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise RuntimeError(f"Missing {label}: {needle}")


def main() -> None:
    enhancer = (SOURCE / "ItemVisualLifecycleWidget.cpp").read_text(encoding="utf-8")
    installer = (SOURCE / "ItemVisualSelectorInstallerSystemComponent.cpp").read_text(encoding="utf-8")
    selector = (SOURCE / "ItemVisualSelectorWidget.cpp").read_text(encoding="utf-8")
    manifest = (CODE / "taintedgrailmoddingsdk_editor_files.cmake").read_text(encoding="utf-8")
    smoke = (ROOT / "Gems" / "TaintedGrailModdingSDK" / "Tools" / "editor_tests" / "alpha_item_viewer_live_smoke.py").read_text(encoding="utf-8")

    require(enhancer, "ProductThumbnailKey", "official O3DE product thumbnail key")
    require(enhancer, "ThumbnailWidget", "official O3DE thumbnail widget")
    require(enhancer, "FOAItemViewerThumbnailGrid", "visual thumbnail grid")
    require(enhancer, "QSettings", "close/reopen state")
    require(enhancer, "m_table->selectRow", "grid-to-validated-selection bridge")
    require(installer, "new ItemVisualLifecycleEnhancer(selector)", "direct lifecycle integration")
    require(selector, "PreviewerFrame", "registered live O3DE previewer")
    require(selector, "UpsertEconomyItemProfile", "existing save path")
    require(selector, "LoadedModelFileMatches", "existing source-index drift check")
    require(selector, "LoadedModelMatchesActiveProfile", "existing exact-profile check")
    require(manifest, "Source/ItemVisualLifecycleWidget.cpp", "production build registration")
    require(smoke, "FOAItemViewerThumbnailGrid", "Windows grid evidence")
    require(smoke, "pane.close()", "Windows close/reopen exercise")
    require(smoke, "general.open_pane(PANE_NAME)", "Windows pane reopen")

    forbidden = (
        "PromoteCandidateEvidence",
        "PromoteEvidenceToCatalog",
        "RuntimeInvocationAllowed = true",
        "DeploymentAllowed = true",
    )
    for token in forbidden:
        if token in enhancer:
            raise RuntimeError(f"Lifecycle enhancer contains forbidden authority: {token}")

    print("Item viewer working-lifecycle static validation passed.")


if __name__ == "__main__":
    main()
