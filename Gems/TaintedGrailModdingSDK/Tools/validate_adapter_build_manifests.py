#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate deterministic, non-building Phase 8 adapter build manifests."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable


class AdapterBuildManifestContractError(RuntimeError):
    """Raised when adapter build manifests are incomplete or invoke build/runtime paths."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise AdapterBuildManifestContractError(
            f"Required adapter build-manifest file is missing: {path}"
        )
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise AdapterBuildManifestContractError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise AdapterBuildManifestContractError(
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


def validate_adapter_build_manifests(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    test_manifest = code_root / "taintedgrailmoddingsdk_build_manifest_tests_files.cmake"
    cmake = read_text(code_root / "CMakeLists.txt")

    require_fragments(
        core_manifest,
        (
            "Source/AdapterBuildManifestService.cpp",
            "Source/AdapterBuildManifestService.h",
        ),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        (
            "Source/AdapterBuildManifestWidget.cpp",
            "Source/AdapterBuildManifestWidget.h",
        ),
        "Editor manifest",
    )
    if manifest_entries(test_manifest) != ("Tests/AdapterBuildManifestTests.cpp",):
        raise AdapterBuildManifestContractError(
            "Build-manifest test ownership must contain only Tests/AdapterBuildManifestTests.cpp."
        )
    require_fragments(
        cmake,
        ("taintedgrailmoddingsdk_build_manifest_tests_files.cmake",),
        "Catalog test target",
    )

    service_parts = sorted(source_root.glob("AdapterBuildManifestServicePart*.inl"))
    if len(service_parts) < 3:
        raise AdapterBuildManifestContractError(
            "Adapter build-manifest service implementation parts are incomplete."
        )
    service = "\n".join(
        [
            read_text(source_root / "AdapterBuildManifestService.h"),
            read_text(source_root / "AdapterBuildManifestService.cpp"),
            *(read_text(path) for path in service_parts),
        ]
    )
    require_fragments(
        service,
        (
            "enum class AdapterBuildDependencyKind",
            "enum class AdapterBuildManifestStatus",
            "Ready",
            "PlanMismatch",
            "ToolchainUnresolved",
            "InputMissing",
            "FingerprintMissing",
            "PathInvalid",
            "RedistributionBlocked",
            "struct AdapterBuildMaterial",
            "struct AdapterBuildExpectedOutput",
            "struct AdapterBuildEnvironment",
            "struct AdapterBuildManifestRequest",
            "struct AdapterBuildManifest",
            "class AdapterBuildManifestService",
            "BuildManifest",
            "SerializeCanonicalManifest",
            'm_buildType = "foa.adapter.plugin"',
            "m_planFingerprint",
            "m_sourceCommit",
            "m_o3deRevision",
            "m_targetFramework",
            "m_compilerId",
            "m_deterministicBuild",
            "m_continuousIntegrationBuild",
            "m_pathMapEnabled",
            '"work_order_plan"',
            '"source_tree"',
            '"dependency_lock"',
            '"toolchain_lock"',
            '"license"',
            '"plugin_binary"',
            '"BepInEx/plugins/"',
            "PathIsInsideRoot",
            "Non-redistributable material cannot enter package output",
            "BuildAllowed",
            "m_buildAllowed = false",
            "SortDependencies",
            "SortMaterials",
            "SortOutputs",
            "std::locale::classic()",
        ),
        "Core adapter build-manifest service",
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
            "ExecuteBuild",
            "RunBuild",
            "dotnet build",
            "msbuild",
            "CreateArchive",
            "Deploy",
            "LaunchFoA",
            "SaveGame",
        ),
        "Core adapter build-manifest service",
    )

    widget = read_text(source_root / "AdapterBuildManifestWidget.cpp")
    require_fragments(
        widget,
        (
            "Tainted Grail Adapter Build Manifests",
            "Read-only Phase 8 build-definition research",
            "BuildAllowed is always false",
            "Nothing is compiled",
            "QAbstractItemView::NoEditTriggers",
            "BuildManifest",
            'tr("Resolved materials")',
            'tr("Expected package outputs")',
            'tr("Canonical JSON / reasons")',
            "build: prohibited",
        ),
        "Adapter build-manifest widget",
    )
    reject_fragments(
        widget,
        (
            "QPushButton",
            "QLineEdit",
            "QFileDialog",
            "QSaveFile",
            "SaveManifest",
            "ExportManifest",
            "RunBuild",
            "ExecuteBuild",
            "Deploy",
            "LaunchFoA",
        ),
        "Adapter build-manifest widget",
    )

    system_component = read_text(source_root / "TaintedGrailModdingSDKSystemComponent.cpp")
    require_fragments(
        system_component,
        (
            '#include "AdapterBuildManifestWidget.h"',
            "AdapterBuildManifestViewPaneName",
            "RegisterViewPane<AdapterBuildManifestWidget>",
            "UnregisterViewPane(AdapterBuildManifestViewPaneName)",
            "TaintedGrailModdingSDK.AdapterBuildManifests",
        ),
        "Editor pane registration",
    )

    tests = read_text(tests_root / "AdapterBuildManifestTests.cpp")
    require_fragments(
        tests,
        (
            "TypedStatusAndDependencyVocabulariesAreStrict",
            "CompleteDefinitionIsReadyButBuildProhibited",
            "PlanMismatchPrecedesOtherFailures",
            "MissingToolchainFailsClosed",
            "MissingRequiredInputAndFingerprintRemainDistinct",
            "PathTraversalAndDuplicateOutputsAreRejected",
            "NonRedistributablePackageMaterialIsBlocked",
            "CanonicalSerializationIsDeterministic",
            "ManifestGenerationDoesNotMutateInputs",
            "BepInExMetadataAndResolvedInputsArePreserved",
            'EXPECT_FALSE(manifest.m_buildAllowed)',
            '"BuildAllowed\\\":false"',
            "materialCount",
            "outputCount",
        ),
        "Adapter build-manifest tests",
    )

    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml")
    require_fragments(
        workflow,
        (
            "Test adapter build-manifest validator",
            "test_validate_adapter_build_manifests.py",
            "Validate adapter build-manifest contract",
            "validate_adapter_build_manifests.py",
        ),
        "Focused workflow",
    )

    design = read_text(repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_BUILD_MANIFESTS.md")
    require_fragments(
        design,
        (
            "reproducible build definition",
            "resolved materials",
            "BepInEx plugin metadata",
            "hard and soft dependencies",
            "safe relative locator",
            "redistribution",
            "ready",
            "toolchain_unresolved",
            "BuildAllowed",
            "No compilation, packaging, deployment, or execution",
            "SLSA",
        ),
        "Adapter build-manifest design",
    )
    docs_index = read_text(repo_root / "docs/tainted-grail-sdk/README.md")
    require_fragments(
        docs_index,
        ("FoA Adapter Build Manifests", "FOA_ADAPTER_BUILD_MANIFESTS.md"),
        "Documentation index",
    )
    roadmap = read_text(repo_root / "ROADMAP.md")
    require_fragments(
        roadmap,
        (
            "Reproducible adapter build manifests",
            "Status: implemented, continuing hardening and Windows UI verification.",
            "No compilation, packaging, deployment, launch, or execution",
        ),
        "Roadmap",
    )
    changelog = read_text(repo_root / "CHANGELOG.md")
    require_fragments(
        changelog,
        (
            "AdapterBuildManifestService",
            "Tainted Grail Adapter Build Manifests",
            "BuildAllowed",
        ),
        "Changelog",
    )
    user_guide = read_text(repo_root / "docs/tainted-grail-sdk/USER_GUIDE.md")
    require_fragments(
        user_guide,
        (
            "Tainted Grail Adapter Build Manifests",
            "toolchain_unresolved",
            "resolved materials",
            "expected package outputs",
            "nothing is built or packaged",
        ),
        "User guide",
    )
    manual_ui = read_text(repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md")
    require_fragments(
        manual_ui,
        (
            "All twelve TG SDK panes",
            "Tainted Grail Adapter Build Manifests",
            "zero ready build definitions",
            "non-editable",
        ),
        "Windows manual UI smoke",
    )
    core_graph = read_text(repo_root / "docs/tainted-grail-sdk/CORE_FRAMEWORK_BUILD_GRAPH.md")
    require_fragments(
        core_graph,
        ("reproducible adapter build-manifest generation", "BuildAllowed=false"),
        "Core/Framework build graph",
    )
    release_process = read_text(repo_root / "docs/tainted-grail-sdk/RELEASE_PROCESS.md")
    require_fragments(
        release_process,
        ("reproducible from documented inputs", "generated package contents", "SHA-256"),
        "Release process",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_adapter_build_manifests(repo_root)
    except (OSError, AdapterBuildManifestContractError) as exc:
        print(f"Tainted Grail adapter build-manifest validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail adapter build manifests passed: exact plans, build definitions, resolved "
        "fingerprints, BepInEx metadata, package paths, redistribution gates, read-only UI, and "
        "build prohibition are wired."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
