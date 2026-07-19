#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate deterministic, read-only staging and deployment preview contracts."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable


class AdapterStagingDeploymentPreviewContractError(RuntimeError):
    """Raised when Slice 13 contracts are incomplete or expose mutation paths."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise AdapterStagingDeploymentPreviewContractError(
            f"Required staging/deployment preview file is missing: {path}"
        )
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise AdapterStagingDeploymentPreviewContractError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise AdapterStagingDeploymentPreviewContractError(
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


def validate_adapter_staging_deployment_preview(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    test_manifest = code_root / "taintedgrailmoddingsdk_deployment_preview_tests_files.cmake"
    cmake = read_text(code_root / "CMakeLists.txt")

    require_fragments(
        core_manifest,
        (
            "Source/AdapterStagingDeploymentPreviewService.cpp",
            "Source/AdapterStagingDeploymentPreviewService.h",
            "Source/AdapterStagingDeploymentPreviewServicePart1.inl",
            "Source/AdapterStagingDeploymentPreviewServicePart2.inl",
            "Source/AdapterStagingDeploymentPreviewServicePart3.inl",
        ),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        (
            "Source/AdapterStagingDeploymentPreviewWidget.cpp",
            "Source/AdapterStagingDeploymentPreviewWidget.h",
        ),
        "Editor manifest",
    )
    if manifest_entries(test_manifest) != (
        "Tests/AdapterStagingDeploymentPreviewTests.cpp",
    ):
        raise AdapterStagingDeploymentPreviewContractError(
            "Deployment-preview test ownership must contain only "
            "Tests/AdapterStagingDeploymentPreviewTests.cpp."
        )
    require_fragments(
        cmake,
        ("taintedgrailmoddingsdk_deployment_preview_tests_files.cmake",),
        "Catalog test target",
    )

    service_parts = sorted(source_root.glob("AdapterStagingDeploymentPreviewServicePart*.inl"))
    if len(service_parts) < 3:
        raise AdapterStagingDeploymentPreviewContractError(
            "Staging/deployment preview implementation parts are incomplete."
        )
    service = "\n".join(
        [
            read_text(source_root / "AdapterStagingDeploymentPreviewService.h"),
            read_text(source_root / "AdapterStagingDeploymentPreviewService.cpp"),
            *(read_text(path) for path in service_parts),
        ]
    )
    require_fragments(
        service,
        (
            "enum class AdapterDeploymentTargetReviewDecision",
            "enum class AdapterDeploymentChangeKind",
            "enum class AdapterDeploymentRollbackAction",
            "enum class AdapterStagingDeploymentPreviewStatus",
            "PackageNotReady",
            "TargetUnreviewed",
            "InventoryBindingMismatch",
            "InventoryUntrusted",
            "PathInvalid",
            "Conflict",
            "BackupIncomplete",
            "RollbackIncomplete",
            '"add"',
            '"replace"',
            '"remove"',
            '"unchanged"',
            '"conflict"',
            '"remove_added"',
            '"restore_replaced"',
            '"restore_removed"',
            "struct AdapterDeploymentTargetReview",
            "struct AdapterDeploymentTargetEntry",
            "struct AdapterDeploymentTargetInventory",
            "struct AdapterDeploymentChange",
            "struct AdapterDeploymentConflict",
            "struct AdapterDeploymentBackupRequirement",
            "struct AdapterDeploymentRollbackStep",
            "struct AdapterStagingDeploymentPreviewRequest",
            "struct AdapterStagingDeploymentPreview",
            "class AdapterStagingDeploymentPreviewRegistry",
            "class AdapterStagingDeploymentPreviewService",
            "BuildPreview",
            "SerializeCanonicalPreview",
            "m_packagePreviewFingerprint",
            "m_inventoryFingerprint",
            "m_projectOwned",
            "m_managed",
            "m_replaceable",
            "m_removable",
            "m_backupRequired",
            "PathIsInsideRoot",
            "AddBackupRequirement",
            "BuildRollbackPlan",
            "deployment.package_not_ready",
            "deployment.target_unreviewed",
            "deployment.inventory_binding_mismatch",
            "deployment.inventory_untrusted",
            "deployment.path_invalid",
            "deployment.conflict",
            "deployment.backup_fingerprint_missing",
            "deployment.rollback_incomplete",
            "m_stagingMutationAllowed = false",
            "m_deploymentMutationAllowed = false",
            "m_rollbackExecutionAllowed = false",
            "m_launchAllowed = false",
            '"StagingMutationAllowed"',
            '"DeploymentMutationAllowed"',
            '"RollbackExecutionAllowed"',
            '"LaunchAllowed"',
            "std::locale::classic()",
        ),
        "Core staging/deployment preview service",
    )
    reject_fragments(
        service,
        (
            "#include <Q",
            "#include <Qt",
            "AzToolsFramework",
            "FoundationService.h",
            "std::filesystem",
            "AZ::IO",
            "QFile",
            "QSaveFile",
            "PersistenceJsonUtils",
            "SaveObjectToFile",
            "QProcess",
            "CreateProcess",
            "WriteProcessMemory",
            "FoA.exe",
            "BepInEx.Bootstrap",
            "HarmonyLib",
            "system(",
            "copy_file",
            "CopyFile",
            "remove_all",
            "MoveFile",
            "ReplaceFile",
            "CreateArchive",
            "DeployFiles",
            "LaunchFoA",
            "ExecuteAdapter",
            "SaveGame",
        ),
        "Core staging/deployment preview service",
    )

    widget = read_text(source_root / "AdapterStagingDeploymentPreviewWidget.cpp")
    require_fragments(
        widget,
        (
            "Tainted Grail Staging and Deployment Preview",
            "Read-only Phase 8 comparison",
            "StagingMutationAllowed, DeploymentMutationAllowed",
            "Nothing is copied, replaced, removed",
            "QAbstractItemView::NoEditTriggers",
            "AdapterStagingDeploymentPreviewRegistry::Get().GetRequests()",
            "BuildPreview",
            'tr("Additions")',
            'tr("Replacements")',
            'tr("Removals / unchanged")',
            'tr("Conflicts")',
            'tr("Backup requirements")',
            'tr("Rollback steps")',
            "filesystem mutation and launch: prohibited",
        ),
        "Staging/deployment preview widget",
    )
    reject_fragments(
        widget,
        (
            "QPushButton",
            "QLineEdit",
            "QFileDialog",
            "RegisterRequest",
            "SavePreview",
            "ExportPreview",
            "CopyFile",
            "RemoveFile",
            "CreateArchive",
            "DeployFiles",
            "LaunchFoA",
            "ExecuteAdapter",
        ),
        "Staging/deployment preview widget",
    )

    system_component = read_text(source_root / "TaintedGrailModdingSDKSystemComponent.cpp")
    require_fragments(
        system_component,
        (
            '#include "AdapterStagingDeploymentPreviewService.h"',
            '#include "AdapterStagingDeploymentPreviewWidget.h"',
            "AdapterStagingDeploymentPreviewViewPaneName",
            "RegisterViewPane<AdapterStagingDeploymentPreviewWidget>",
            "UnregisterViewPane(AdapterStagingDeploymentPreviewViewPaneName)",
            "TaintedGrailModdingSDK.StagingDeploymentPreview",
            "AdapterStagingDeploymentPreviewRegistry::Get().Clear()",
        ),
        "Editor pane registration",
    )

    tests = read_text(tests_root / "AdapterStagingDeploymentPreviewTests.cpp")
    require_fragments(
        tests,
        (
            "TypedStatusChangeAndRollbackVocabulariesAreStrict",
            "RegistryRejectsDuplicateTargetInventoryIdentity",
            "ReadyPreviewDerivesAllChangeKindsAndProhibitsMutation",
            "PackageNotReadyPrecedesTargetAndInventoryFailures",
            "ReviewAndInventoryBindingsFailClosed",
            "ForeignOwnershipAndDuplicateTargetsProduceConflicts",
            "MissingCurrentDigestMakesBackupAndRollbackIncomplete",
            "UnsafeTargetOrBackupPathIsRejected",
            "RollbackStepsAreExactDeterministicInverses",
            "CanonicalChangesBackupsAndRollbackAreDeterministic",
            "PreviewGenerationDoesNotMutateInputs",
            "EXPECT_FALSE(preview.m_stagingMutationAllowed)",
            '"DeploymentMutationAllowed\\\":false"',
            "packageEntryCount",
            "targetEntryCount",
        ),
        "Staging/deployment preview tests",
    )

    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml")
    require_fragments(
        workflow,
        (
            "Test staging/deployment preview validator",
            "test_validate_adapter_staging_deployment_preview.py",
            "Validate staging/deployment preview contract",
            "validate_adapter_staging_deployment_preview.py",
        ),
        "Focused workflow",
    )

    design = read_text(
        repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_STAGING_DEPLOYMENT_PREVIEW.md"
    )
    require_fragments(
        design,
        (
            "ready package layout",
            "declared target inventory",
            "additions",
            "replacements",
            "removals",
            "conflicts",
            "backup requirements",
            "rollback steps",
            "StagingMutationAllowed",
            "DeploymentMutationAllowed",
            "RollbackExecutionAllowed",
            "No copying, deletion, deployment, launch, or execution",
        ),
        "Staging/deployment preview design",
    )
    docs_index = read_text(repo_root / "docs/tainted-grail-sdk/README.md")
    require_fragments(
        docs_index,
        (
            "FoA Adapter Staging and Deployment Preview",
            "FOA_ADAPTER_STAGING_DEPLOYMENT_PREVIEW.md",
        ),
        "Documentation index",
    )
    package_design = read_text(
        repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_PACKAGE_ASSEMBLY_PREVIEW.md"
    )
    require_fragments(
        package_design,
        ("Slice 13", "staging and deployment preview", "no file mutation"),
        "Package-assembly preview handoff",
    )
    roadmap = read_text(repo_root / "ROADMAP.md")
    require_fragments(
        roadmap,
        (
            "Deterministic staging and deployment preview",
            "Status: implemented, continuing hardening and Windows UI verification.",
            "additions, replacements, removals, conflicts, backups, and rollback",
            "No copying, deletion, deployment, launch, or execution",
        ),
        "Roadmap",
    )
    changelog = read_text(repo_root / "CHANGELOG.md")
    require_fragments(
        changelog,
        (
            "AdapterStagingDeploymentPreviewService",
            "Tainted Grail Staging and Deployment Preview",
            "DeploymentMutationAllowed",
        ),
        "Changelog",
    )
    user_guide = read_text(repo_root / "docs/tainted-grail-sdk/USER_GUIDE.md")
    require_fragments(
        user_guide,
        (
            "Tainted Grail Staging and Deployment Preview",
            "backup_incomplete",
            "rollback_incomplete",
            "additions, replacements, removals, and unchanged paths",
            "nothing is copied or deleted",
        ),
        "User guide",
    )
    manual_ui = read_text(
        repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md"
    )
    require_fragments(
        manual_ui,
        (
            "All fourteen TG SDK panes",
            "Tainted Grail Staging and Deployment Preview",
            "zero registered staging/deployment-preview inputs",
            "non-editable",
        ),
        "Windows manual UI smoke",
    )
    core_graph = read_text(repo_root / "docs/tainted-grail-sdk/CORE_FRAMEWORK_BUILD_GRAPH.md")
    require_fragments(
        core_graph,
        (
            "staging/deployment preview derivation",
            "DeploymentMutationAllowed=false",
        ),
        "Core/Framework build graph",
    )
    release_process = read_text(repo_root / "docs/tainted-grail-sdk/RELEASE_PROCESS.md")
    require_fragments(
        release_process,
        (
            "additions, replacements, removals, and conflicts",
            "backup and rollback preview",
            "does not mutate deployment",
        ),
        "Release process",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_adapter_staging_deployment_preview(repo_root)
    except (OSError, AdapterStagingDeploymentPreviewContractError) as exc:
        print(f"Tainted Grail staging/deployment preview validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail staging/deployment preview passed: exact package/target binding, "
        "deterministic changes, conflicts, backups, rollback, read-only UI, and the no-mutation "
        "boundary are wired."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
