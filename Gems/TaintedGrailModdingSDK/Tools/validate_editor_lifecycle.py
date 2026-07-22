#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate lifecycle ownership and Hub reachability for every TG SDK pane."""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path


class EditorLifecycleError(RuntimeError):
    """Raised when an Editor pane is unreachable or owns an unsafe lifecycle."""


@dataclass(frozen=True)
class Pane:
    name: str
    registration_source: str
    widget_source: str
    manifest: str
    extension_id: str | None = None


BASE_SOURCE = "Gems/TaintedGrailModdingSDK/Code/Source/TaintedGrailModdingSDKSystemComponent.cpp"
BASE_MANIFEST = "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_editor_files.cmake"
HUB_SOURCE = "Gems/TaintedGrailModdingSDK/Code/Source/DevelopmentHubWidget.cpp"
EXTENSION_HOST = "Gems/TaintedGrailModdingSDK/Code/Source/FoundationExtensionRequestBus.cpp"


def base_pane(name: str, widget: str) -> Pane:
    return Pane(name, BASE_SOURCE, f"Gems/TaintedGrailModdingSDK/Code/Source/{widget}.cpp", BASE_MANIFEST)


PANES = (
    base_pane("FOA Development Hub", "DevelopmentHubWidget"),
    base_pane("Tainted Grail SDK Status", "FoundationStatusWidget"),
    base_pane("Tainted Grail Pack Manager", "PackManagerWidget"),
    base_pane("Tainted Grail Source Intake", "SourceEvidenceIntakeWidget"),
    base_pane("Tainted Grail Catalog Browser", "CatalogBrowserWidget"),
    base_pane("Tainted Grail Catalog Governance", "CatalogGovernanceWidget"),
    base_pane("Tainted Grail Item and Recipe Editor", "ItemRecipeEditorWidget"),
    base_pane("Tainted Grail Actor and Troop Editor", "ActorTroopEditorWidget"),
    base_pane("Tainted Grail Economy Acquisition Coverage", "EconomyCoverageDashboardWidget"),
    base_pane("Tainted Grail Economy Cross-Pack Duplicates", "EconomyDuplicateReportWidget"),
    base_pane("Tainted Grail Adapter Capability Matrix", "AdapterCapabilityMatrixWidget"),
    base_pane("Tainted Grail Adapter Work-Order Plans", "AdapterWorkOrderPlanWidget"),
    base_pane("Tainted Grail Adapter Runtime Result Evidence", "AdapterRuntimeResultEvidenceWidget"),
    base_pane("Tainted Grail Adapter Build Manifests", "AdapterBuildManifestWidget"),
    base_pane("Tainted Grail Package Assembly Preview", "AdapterPackageAssemblyPreviewWidget"),
    base_pane("Tainted Grail Staging and Deployment Preview", "AdapterStagingDeploymentPreviewWidget"),
    base_pane("Tainted Grail Deployment Confirmation and Work Orders", "AdapterDeploymentWorkOrderWidget"),
    base_pane("Tainted Grail Deployment Execution Result Evidence", "AdapterDeploymentExecutionEvidenceWidget"),
    base_pane("Tainted Grail Post-Deployment Verification and Release Blockers", "AdapterPostDeploymentVerificationWidget"),
    base_pane("Tainted Grail Independent Post-Deployment Verifier Results", "AdapterPostDeploymentVerifierWidget"),
    base_pane("Tainted Grail Verifier Evidence Reconciliation and Release Decision", "AdapterVerifierEvidenceReconciliationWidget"),
    Pane(
        "Tainted Grail Release Artifact Provenance and Signing Intent",
        "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseArtifactPaneSystemComponent.cpp",
        "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseArtifactWidget.cpp",
        BASE_MANIFEST,
    ),
    Pane(
        "Tainted Grail Release Assembly and Checksum Results",
        "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseAssemblyPaneSystemComponent.cpp",
        "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseAssemblyResultWidget.cpp",
        BASE_MANIFEST,
    ),
    Pane(
        "Tainted Grail Release Signing Results",
        "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseSigningPaneSystemComponent.cpp",
        "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseSigningResultWidget.cpp",
        BASE_MANIFEST,
    ),
    Pane(
        "Tainted Grail Avalon AI Editor",
        "Plugins/Authoring/AvalonAI/Gem/Code/Source/AvalonAIEditorModule.cpp",
        "Plugins/Authoring/AvalonAI/Gem/Code/Source/AvalonAIEditorWidget.cpp",
        "Plugins/Authoring/AvalonAI/Gem/Code/avalon_ai_authoring_files.cmake",
        "extension.avalon-ai-authoring",
    ),
    Pane(
        "Tainted Grail Road Atlas Editor",
        "Plugins/Authoring/RoadAtlas/Gem/Code/Source/RoadAtlasEditorModule.cpp",
        "Plugins/Authoring/RoadAtlas/Gem/Code/Source/RoadAtlasEditorWidget.cpp",
        "Plugins/Authoring/RoadAtlas/Gem/Code/road_atlas_authoring_files.cmake",
        "extension.avalon-core-road-atlas",
    ),
)


def read(repo_root: Path, relative: str) -> str:
    path = repo_root / relative
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as exc:
        raise EditorLifecycleError(f"Required Editor lifecycle file is unavailable: {path}: {exc}") from exc


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise EditorLifecycleError(f"{label} is missing required fragment {fragment!r}")


def validate_editor_lifecycle(repo_root: Path) -> None:
    if len(PANES) != 26 or len({pane.name for pane in PANES}) != len(PANES):
        raise EditorLifecycleError("The canonical Editor inventory must contain 26 unique pane names")

    hub = read(repo_root, HUB_SOURCE)
    source_cache: dict[str, str] = {}
    manifest_cache: dict[str, str] = {}
    save_keys: list[str] = []

    for pane in PANES:
        registration = source_cache.setdefault(
            pane.registration_source,
            read(repo_root, pane.registration_source),
        )
        widget = read(repo_root, pane.widget_source)
        manifest = manifest_cache.setdefault(pane.manifest, read(repo_root, pane.manifest))
        widget_name = Path(pane.widget_source).stem

        require(registration, pane.name, f"{pane.name} registration")
        require(registration, "RegisterViewPane<", f"{pane.name} registration")
        require(registration, "UnregisterViewPane", f"{pane.name} lifecycle")
        require(registration, "isDeletable = true", f"{pane.name} options")
        require(registration, "isPreview = true", f"{pane.name} options")
        require(registration, "saveKeyName", f"{pane.name} persistent layout")
        require(manifest, f"Source/{widget_name}.cpp", f"{pane.name} build ownership")
        require(widget, f"{widget_name}::", f"{pane.name} widget implementation")

        if pane.name != "FOA Development Hub":
            require(hub, pane.name, f"{pane.name} Hub route")

        if pane.extension_id:
            require(registration, pane.extension_id, f"{pane.name} extension identity")
            require(registration, "GetRequiredServices", f"{pane.name} activation ordering")
            require(registration, "TaintedGrailModdingSDKService", f"{pane.name} Foundation dependency")
            require(registration, "RegisterExtension", f"{pane.name} activation")
            require(registration, "UnregisterExtension", f"{pane.name} deactivation")
            require(widget, "LoadExtensionDocument", f"{pane.name} durable load")
            require(widget, "SaveExtensionDocument", f"{pane.name} atomic save")
            require(widget, "Validate", f"{pane.name} fail-closed validation")
            for forbidden in (
                "QProcess",
                "std::system",
                "ShellExecute",
                "runtimeMutationAllowed = true",
                "executionEnabled = true",
            ):
                if forbidden in registration or forbidden in widget:
                    raise EditorLifecycleError(
                        f"{pane.name} contains forbidden editor/runtime authority: {forbidden}"
                    )

    for registration in source_cache.values():
        for match in re.finditer(r'saveKeyName\s*=\s*QStringLiteral\("([^"]+)"\)', registration):
            save_keys.append(match.group(1))

    if len(save_keys) != 26 or len(set(save_keys)) != 26:
        raise EditorLifecycleError(
            f"Every pane needs one unique layout save key; found {len(save_keys)} keys and {len(set(save_keys))} unique values"
        )

    extension_host = read(repo_root, EXTENSION_HOST)
    for fragment in (
        "MaximumExtensionDocumentBytes",
        "IsSafeRelativePath",
        "IsExtensionRegistered",
        "QSaveFile file(",
        "QJsonDocument::fromJson",
        "cancelWriting",
        "ResolveExtensionDocumentPath",
        "Extension document writes require prior deterministic registration",
    ):
        require(extension_host, fragment, "host-owned extension document boundary")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_editor_lifecycle(repo_root)
    except EditorLifecycleError as exc:
        print(f"TG SDK Editor lifecycle validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "TG SDK Editor lifecycle validation passed: all 26 panes have build ownership, "
        "registration, deactivation, unique layout state, Hub reachability, and bounded "
        "optional-Gem persistence."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
