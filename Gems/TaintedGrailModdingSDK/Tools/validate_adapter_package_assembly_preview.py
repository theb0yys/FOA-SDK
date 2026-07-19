#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate deterministic, non-mutating Phase 8 package-assembly previews."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable


class AdapterPackageAssemblyPreviewContractError(RuntimeError):
    """Raised when package previews are incomplete or expose assembly/runtime paths."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise AdapterPackageAssemblyPreviewContractError(
            f"Required package-assembly preview file is missing: {path}"
        )
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise AdapterPackageAssemblyPreviewContractError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise AdapterPackageAssemblyPreviewContractError(
                f"{label} contains forbidden fragment {fragment!r}."
            )


def manifest_entries(path: Path) -> tuple[str, ...]:
    return tuple(
        re.findall(
            r"^\s+((?:Source|Tests)/[^\s)]+\.(?:cpp|h))\s*$",
            read_text(path),
            re.MULTILINE,
        )
    )


def validate_adapter_package_assembly_preview(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    test_manifest = code_root / "taintedgrailmoddingsdk_package_preview_tests_files.cmake"
    cmake = read_text(code_root / "CMakeLists.txt")

    require_fragments(
        core_manifest,
        (
            "Source/AdapterPackageAssemblyPreviewService.cpp",
            "Source/AdapterPackageAssemblyPreviewService.h",
            "Source/AdapterPackageAssemblyPreviewServicePart1.inl",
            "Source/AdapterPackageAssemblyPreviewServicePart2.inl",
            "Source/AdapterPackageAssemblyPreviewServicePart3.inl",
        ),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        (
            "Source/AdapterPackageAssemblyPreviewWidget.cpp",
            "Source/AdapterPackageAssemblyPreviewWidget.h",
        ),
        "Editor manifest",
    )
    if manifest_entries(test_manifest) != (
        "Tests/AdapterPackageAssemblyPreviewTests.cpp",
    ):
        raise AdapterPackageAssemblyPreviewContractError(
            "Package-preview test manifest must contain only "
            "Tests/AdapterPackageAssemblyPreviewTests.cpp."
        )
    require_fragments(
        cmake,
        ("taintedgrailmoddingsdk_package_preview_tests_files.cmake",),
        "Catalog test target",
    )

    service_parts = sorted(source_root.glob("AdapterPackageAssemblyPreviewServicePart*.inl"))
    if len(service_parts) < 3:
        raise AdapterPackageAssemblyPreviewContractError(
            "Package-assembly preview implementation parts are incomplete."
        )
    service = "\n".join(
        [
            read_text(source_root / "AdapterPackageAssemblyPreviewService.h"),
            read_text(source_root / "AdapterPackageAssemblyPreviewService.cpp"),
            *(read_text(path) for path in service_parts),
        ]
    )
    require_fragments(
        service,
        (
            "enum class AdapterBuildManifestReviewDecision",
            "Accepted",
            "Rejected",
            "enum class AdapterPackageAssemblyPreviewStatus",
            "ManifestNotReady",
            "ManifestUnreviewed",
            "InventoryBindingMismatch",
            "InventoryUntrusted",
            "OutputMissing",
            "FingerprintMissing",
            "PathInvalid",
            "Collision",
            "RedistributionBlocked",
            "struct AdapterBuildManifestReview",
            "struct AdapterStagingInventoryEntry",
            "struct AdapterStagingInventory",
            "struct AdapterPackageLayoutEntry",
            "struct AdapterPackageAssemblyOmission",
            "struct AdapterPackageAssemblyCollision",
            "struct AdapterPackageAssemblyBlocker",
            "class AdapterPackageAssemblyPreviewRegistry",
            "class AdapterPackageAssemblyPreviewService",
            "RegisterRequest",
            "BuildPreview",
            "SerializeCanonicalPreview",
            "m_projectOwned",
            "m_outputDigest",
            "m_manifestFingerprint",
            "PathIsInsideRoot",
            "package_preview.output_missing",
            "package_preview.output_collision",
            "package_preview.redistribution_blocked",
            "package_preview.project_ownership_required",
            "m_assemblyAllowed = false",
            "m_archiveAllowed = false",
            "m_deploymentAllowed = false",
            '"AssemblyAllowed"',
            '"ArchiveAllowed"',
            '"DeploymentAllowed"',
            "std::locale::classic()",
        ),
        "Core package-assembly preview service",
    )
    reject_fragments(
        service,
        (
            "#include <Q",
            "#include <Qt",
            "AzToolsFramework",
            "FoundationService.h",
            "QProcess",
            "CreateProcess",
            "WriteProcessMemory",
            "FoA.exe",
            "BepInEx.Bootstrap",
            "HarmonyLib",
            "system(",
            "std::filesystem",
            "AZ::IO",
            "QFile",
            "QSaveFile",
            "PersistenceJsonUtils",
            "SaveObjectToFile",
            "CopyFile",
            "copy_file",
            "CreateArchive",
            "ZipArchive",
            "AssemblePackage",
            "DeployPackage",
            "LaunchFoA(",
            "SaveGame",
        ),
        "Core package-assembly preview service",
    )

    widget = read_text(source_root / "AdapterPackageAssemblyPreviewWidget.cpp")
    require_fragments(
        widget,
        (
            "Tainted Grail Package Assembly Preview",
            "Read-only Phase 8 package-layout research",
            "AssemblyAllowed, ArchiveAllowed, and DeploymentAllowed are always false",
            "Nothing is copied, archived, deployed, launched, or executed",
            "QAbstractItemView::NoEditTriggers",
            "AdapterPackageAssemblyPreviewRegistry::Get().GetRequests()",
            "BuildPreview",
            'tr("Derived layout and output digests")',
            'tr("Omissions")',
            'tr("Collisions")',
            'tr("Redistribution and trust blockers")',
            "assembly/archive/deployment: prohibited",
        ),
        "Package-assembly preview widget",
    )
    reject_fragments(
        widget,
        (
            "QPushButton",
            "QLineEdit",
            "QFileDialog",
            "QSaveFile",
            "RegisterRequest",
            "SavePreview",
            "ExportPreview",
            "CopyFiles",
            "CreateArchive",
            "AssemblePackage",
            "DeployPackage",
            "LaunchFoA(",
        ),
        "Package-assembly preview widget",
    )

    system_component = read_text(source_root / "TaintedGrailModdingSDKSystemComponent.cpp")
    require_fragments(
        system_component,
        (
            '#include "AdapterPackageAssemblyPreviewService.h"',
            '#include "AdapterPackageAssemblyPreviewWidget.h"',
            "AdapterPackageAssemblyPreviewViewPaneName",
            "RegisterViewPane<AdapterPackageAssemblyPreviewWidget>",
            "UnregisterViewPane(AdapterPackageAssemblyPreviewViewPaneName)",
            "TaintedGrailModdingSDK.PackageAssemblyPreview",
            "AdapterPackageAssemblyPreviewRegistry::Get().Clear()",
        ),
        "Editor pane registration",
    )

    tests = read_text(tests_root / "AdapterPackageAssemblyPreviewTests.cpp")
    require_fragments(
        tests,
        (
            "TypedStatusAndReviewVocabulariesAreStrict",
            "RegistryRejectsDuplicateInventoryIdentities",
            "ReviewedCompleteInventoryProducesReadyPreviewOnly",
            "ManifestNotReadyPrecedesInventoryFailures",
            "ReviewAndInventoryBindingsFailClosed",
            "MissingOutputProducesExplicitOmission",
            "MissingOutputDigestIsDistinct",
            "UnsafePathAndTargetCollisionAreRejected",
            "OwnershipAndRedistributionFailClosed",
            "CanonicalLayoutAndDigestsAreDeterministic",
            "PreviewGenerationDoesNotMutateInputs",
            'EXPECT_FALSE(preview.m_assemblyAllowed)',
            '"AssemblyAllowed\\\":false"',
            "expectedOutputCount",
            "inventoryCount",
        ),
        "Package-assembly preview tests",
    )

    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml")
    require_fragments(
        workflow,
        (
            "Test package-assembly preview validator",
            "test_validate_adapter_package_assembly_preview.py",
            "Validate package-assembly preview contract",
            "validate_adapter_package_assembly_preview.py",
        ),
        "Focused workflow",
    )

    design = read_text(repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_PACKAGE_ASSEMBLY_PREVIEW.md")
    require_fragments(
        design,
        (
            "reviewed build manifest",
            "project-owned staging inventory",
            "derived package layout",
            "output digests",
            "omissions",
            "collisions",
            "redistribution blockers",
            "AssemblyAllowed",
            "ArchiveAllowed",
            "DeploymentAllowed",
            "No file copying, archive creation, deployment, launch, or execution",
        ),
        "Package-assembly preview design",
    )
    docs_index = read_text(repo_root / "docs/tainted-grail-sdk/README.md")
    require_fragments(
        docs_index,
        (
            "FoA Adapter Package Assembly Preview",
            "FOA_ADAPTER_PACKAGE_ASSEMBLY_PREVIEW.md",
        ),
        "Documentation index",
    )
    build_manifest_design = read_text(repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_BUILD_MANIFESTS.md")
    require_fragments(
        build_manifest_design,
        ("Slice 12", "package-assembly preview", "no file copying"),
        "Build-manifest design",
    )
    roadmap = read_text(repo_root / "ROADMAP.md")
    require_fragments(
        roadmap,
        (
            "Deterministic package-assembly preview",
            "Status: implemented, continuing hardening and Windows UI verification.",
            "project-owned staging inventory",
            "No file copying, archive creation, deployment, launch, or execution",
        ),
        "Roadmap",
    )
    changelog = read_text(repo_root / "CHANGELOG.md")
    require_fragments(
        changelog,
        (
            "AdapterPackageAssemblyPreviewService",
            "Tainted Grail Package Assembly Preview",
            "AssemblyAllowed",
        ),
        "Changelog",
    )
    user_guide = read_text(repo_root / "docs/tainted-grail-sdk/USER_GUIDE.md")
    require_fragments(
        user_guide,
        (
            "Tainted Grail Package Assembly Preview",
            "output_missing",
            "collision",
            "derived layout",
            "nothing is copied or archived",
        ),
        "User guide",
    )
    manual_ui = read_text(repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md")
    require_fragments(
        manual_ui,
        (
            "All thirteen TG SDK panes",
            "Tainted Grail Package Assembly Preview",
            "zero registered package-preview inputs",
            "non-editable",
        ),
        "Windows manual UI smoke",
    )
    core_graph = read_text(repo_root / "docs/tainted-grail-sdk/CORE_FRAMEWORK_BUILD_GRAPH.md")
    require_fragments(
        core_graph,
        ("package-assembly preview derivation", "AssemblyAllowed=false"),
        "Core/Framework build graph",
    )
    release_process = read_text(repo_root / "docs/tainted-grail-sdk/RELEASE_PROCESS.md")
    require_fragments(
        release_process,
        ("generated packages inspected", "checksums", "redistributable output"),
        "Release process",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_adapter_package_assembly_preview(repo_root)
    except (OSError, AdapterPackageAssemblyPreviewContractError) as exc:
        print(f"Tainted Grail package-assembly preview validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail package-assembly preview passed: reviewed manifests, project-owned staging "
        "inventory, deterministic layout/digests, omissions, collisions, redistribution gates, "
        "read-only UI, and assembly prohibition are wired."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
