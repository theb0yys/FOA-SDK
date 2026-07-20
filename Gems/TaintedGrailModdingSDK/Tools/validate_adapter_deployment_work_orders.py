#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate typed, read-only deployment confirmation and work-order contracts."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable


class AdapterDeploymentWorkOrderContractError(RuntimeError):
    """Raised when Slice 14 contracts are incomplete or expose execution paths."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise AdapterDeploymentWorkOrderContractError(
            f"Required deployment work-order file is missing: {path}"
        )
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise AdapterDeploymentWorkOrderContractError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise AdapterDeploymentWorkOrderContractError(
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


def validate_adapter_deployment_work_orders(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    test_manifest = code_root / "taintedgrailmoddingsdk_deployment_work_order_tests_files.cmake"
    cmake = read_text(code_root / "CMakeLists.txt")

    require_fragments(
        core_manifest,
        (
            "Source/AdapterDeploymentWorkOrderService.cpp",
            "Source/AdapterDeploymentWorkOrderService.h",
            "Source/AdapterDeploymentWorkOrderServicePart1.inl",
            "Source/AdapterDeploymentWorkOrderServicePart2.inl",
            "Source/AdapterDeploymentWorkOrderServicePart3.inl",
        ),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        (
            "Source/AdapterDeploymentWorkOrderWidget.cpp",
            "Source/AdapterDeploymentWorkOrderWidget.h",
        ),
        "Editor manifest",
    )
    if manifest_entries(test_manifest) != (
        "Tests/AdapterDeploymentWorkOrderTests.cpp",
        "Tests/AdapterDeploymentPipelineHandoffTests.cpp",
    ):
        raise AdapterDeploymentWorkOrderContractError(
            "Deployment work-order test ownership must contain the service tests and "
            "the real staging-preview-to-work-order handoff regression test."
        )
    require_fragments(
        cmake,
        ("taintedgrailmoddingsdk_deployment_work_order_tests_files.cmake",),
        "Catalog test target",
    )

    service_parts = sorted(source_root.glob("AdapterDeploymentWorkOrderServicePart*.inl"))
    if len(service_parts) < 3:
        raise AdapterDeploymentWorkOrderContractError(
            "Deployment work-order implementation parts are incomplete."
        )
    service = "\n".join(
        [
            read_text(source_root / "AdapterDeploymentWorkOrderService.h"),
            read_text(source_root / "AdapterDeploymentWorkOrderService.cpp"),
            *(read_text(path) for path in service_parts),
        ]
    )
    service = service.replace('\\"', '"')
    require_fragments(
        service,
        (
            "enum class AdapterDeploymentConfirmationDecision",
            "enum class AdapterDeploymentConfirmationScope",
            "enum class AdapterDeploymentPreflightKind",
            "enum class AdapterDeploymentPreflightStatus",
            "enum class AdapterDeploymentWorkOrderStepKind",
            "enum class AdapterDeploymentChecklistState",
            "enum class AdapterDeploymentWorkOrderStatus",
            "PreviewNotReady",
            "ConfirmationMissing",
            "ConfirmationRejected",
            "ConfirmationBindingMismatch",
            "ScopeMismatch",
            "ConfirmationExpired",
            "MaintenanceWindowInvalid",
            "OutsideMaintenanceWindow",
            "PreflightMissing",
            "PreflightFailed",
            "WorkOrderIncomplete",
            "ReviewReady",
            '"additions_only"',
            '"additions_and_replacements"',
            '"full_preview"',
            '"package_integrity"',
            '"target_inventory"',
            '"backup_readiness"',
            '"rollback_readiness"',
            '"operator_readiness"',
            "struct AdapterDeploymentConfirmation",
            "struct AdapterDeploymentMaintenanceWindow",
            "struct AdapterDeploymentPreflightEvidence",
            "struct AdapterDeploymentWorkOrderStep",
            "struct AdapterDeploymentOperatorChecklistItem",
            "struct AdapterDeploymentWorkOrderRequest",
            "struct AdapterDeploymentWorkOrder",
            "class AdapterDeploymentWorkOrderRegistry",
            "class AdapterDeploymentWorkOrderService",
            "BuildWorkOrder",
            "SerializeCanonicalWorkOrder",
            "ScopeCoversPreview",
            "IsUtcTimestamp",
            "HasExactWorkOrderCoverage",
            "m_previewFingerprint",
            "m_issuedAtUtc",
            "m_expiresAtUtc",
            "m_startAtUtc",
            "m_endAtUtc",
            "m_operatorGroup",
            "m_acknowledgementRecorded = false",
            "m_executionAllowed = false",
            "m_copyAllowed = false",
            "m_deleteAllowed = false",
            "m_backupAllowed = false",
            "m_restoreAllowed = false",
            "m_deploymentAllowed = false",
            "m_launchAllowed = false",
            '"ExecutionAllowed"',
            '"CopyAllowed"',
            '"DeleteAllowed"',
            '"BackupAllowed"',
            '"RestoreAllowed"',
            '"DeploymentAllowed"',
            '"LaunchAllowed"',
            "std::locale::classic()",
        ),
        "Core deployment work-order service",
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
            "DeleteFile",
            "MoveFile",
            "ReplaceFile",
            "CreateArchive",
            "DeployFiles",
            "BackupFiles",
            "RestoreFiles",
            "LaunchFoA",
            "ExecuteAdapter",
            "SaveGame",
        ),
        "Core deployment work-order service",
    )

    widget = read_text(source_root / "AdapterDeploymentWorkOrderWidget.cpp")
    require_fragments(
        widget,
        (
            "Tainted Grail Deployment Confirmation and Work Orders",
            "Read-only Phase 8 explicit-confirmation and work-order contract",
            "ReviewReady never authorizes execution",
            "Nothing is copied, deleted, backed up, restored, deployed, launched, or executed",
            "QAbstractItemView::NoEditTriggers",
            "AdapterDeploymentWorkOrderRegistry::Get().GetRequests()",
            "BuildWorkOrder",
            'tr("Confirmation")',
            'tr("Maintenance window")',
            'tr("Typed work-order steps")',
            'tr("Operator checklist")',
            'tr("Execution permissions")',
            "execution, copy, delete, backup, restore, deployment, and launch: prohibited",
        ),
        "Deployment work-order widget",
    )
    reject_fragments(
        widget,
        (
            "QPushButton",
            "QLineEdit",
            "QFileDialog",
            "RegisterRequest",
            "SaveWorkOrder",
            "ExportWorkOrder",
            "CopyFile",
            "DeleteFile",
            "BackupFiles",
            "RestoreFiles",
            "DeployFiles",
            "LaunchFoA",
            "ExecuteAdapter",
        ),
        "Deployment work-order widget",
    )

    system_component = read_text(source_root / "TaintedGrailModdingSDKSystemComponent.cpp")
    require_fragments(
        system_component,
        (
            '#include "AdapterDeploymentWorkOrderService.h"',
            '#include "AdapterDeploymentWorkOrderWidget.h"',
            "AdapterDeploymentWorkOrderViewPaneName",
            "RegisterViewPane<AdapterDeploymentWorkOrderWidget>",
            "UnregisterViewPane(AdapterDeploymentWorkOrderViewPaneName)",
            "TaintedGrailModdingSDK.DeploymentWorkOrders",
            "AdapterDeploymentWorkOrderRegistry::Get().Clear()",
        ),
        "Editor pane registration",
    )

    tests = read_text(tests_root / "AdapterDeploymentWorkOrderTests.cpp")
    require_fragments(
        tests,
        (
            "TypedConfirmationScopePreflightAndStatusVocabulariesAreStrict",
            "RegistryRejectsDuplicateConfirmationOrPreviewIdentity",
            "CompleteConfirmationProducesReviewReadyWorkOrderOnly",
            "PreviewNotReadyPrecedesConfirmationAndPreflightFailures",
            "ConfirmationDecisionBindingAndScopeFailClosed",
            "ExpiryAndMaintenanceWindowFailClosed",
            "MissingAndFailedPreflightRemainDistinct",
            "WorkOrderStepsCoverChangesBackupsAndRollback",
            "OperatorChecklistRemainsPendingAndNonExecutable",
            "CanonicalWorkOrderIsDeterministic",
            "WorkOrderGenerationDoesNotMutateInputs",
            "EXPECT_FALSE(workOrder.m_executionAllowed)",
            '"ExecutionAllowed\\\":false"',
            "additionCount",
            "preflightCount",
        ),
        "Deployment work-order tests",
    )

    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml")
    require_fragments(
        workflow,
        (
            "Test deployment confirmation/work-order validator",
            "test_validate_adapter_deployment_work_orders.py",
            "Validate deployment confirmation/work-order contract",
            "validate_adapter_deployment_work_orders.py",
        ),
        "Focused workflow",
    )

    design = read_text(
        repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_DEPLOYMENT_CONFIRMATION_WORK_ORDERS.md"
    )
    require_fragments(
        design,
        (
            "exact ready staging/deployment preview",
            "named reviewer",
            "confirmation scope",
            "expiry",
            "maintenance window",
            "preflight evidence",
            "operator-facing checklist",
            "review_ready",
            "ExecutionAllowed",
            "No copy, delete, backup, restore, deployment, launch, or adapter call",
        ),
        "Deployment confirmation/work-order design",
    )

    docs_index = read_text(repo_root / "docs/tainted-grail-sdk/README.md")
    require_fragments(
        docs_index,
        (
            "FoA Adapter Deployment Confirmation and Work Orders",
            "FOA_ADAPTER_DEPLOYMENT_CONFIRMATION_WORK_ORDERS.md",
        ),
        "Documentation index",
    )

    preview_design = read_text(
        repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_STAGING_DEPLOYMENT_PREVIEW.md"
    )
    require_fragments(
        preview_design,
        ("Slice 14", "explicit confirmation", "deployment work-order", "no execution authority"),
        "Staging/deployment preview handoff",
    )

    roadmap = read_text(repo_root / "ROADMAP.md")
    require_fragments(
        roadmap,
        (
            "Typed deployment confirmation and work-order contract",
            "Status: implemented, continuing hardening and Windows UI verification.",
            "confirmation scope, expiry, maintenance window, preflight evidence, and operator checklist",
            "No copy, delete, backup, restore, deployment, launch, or adapter call",
        ),
        "Roadmap",
    )

    changelog = read_text(repo_root / "CHANGELOG.md")
    require_fragments(
        changelog,
        (
            "AdapterDeploymentWorkOrderService",
            "Tainted Grail Deployment Confirmation and Work Orders",
            "ExecutionAllowed",
        ),
        "Changelog",
    )

    user_guide = read_text(repo_root / "docs/tainted-grail-sdk/USER_GUIDE.md")
    require_fragments(
        user_guide,
        (
            "Tainted Grail Deployment Confirmation and Work Orders",
            "confirmation_expired",
            "outside_maintenance_window",
            "preflight_missing",
            "review_ready",
            "nothing is copied, deleted, backed up, restored, deployed, or launched",
        ),
        "User guide",
    )

    manual_ui = read_text(
        repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md"
    )
    require_fragments(
        manual_ui,
        (
            "All fifteen TG SDK panes",
            "Tainted Grail Deployment Confirmation and Work Orders",
            "zero registered deployment confirmation/work-order inputs",
            "non-editable",
        ),
        "Windows manual UI smoke",
    )

    core_graph = read_text(repo_root / "docs/tainted-grail-sdk/CORE_FRAMEWORK_BUILD_GRAPH.md")
    require_fragments(
        core_graph,
        (
            "deployment confirmation/work-order derivation",
            "ExecutionAllowed=false",
        ),
        "Core/Framework build graph",
    )

    release_process = read_text(repo_root / "docs/tainted-grail-sdk/RELEASE_PROCESS.md")
    require_fragments(
        release_process,
        (
            "explicit deployment confirmation",
            "maintenance window",
            "preflight evidence",
            "operator checklist",
            "execution remains separately prohibited",
        ),
        "Release process",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_adapter_deployment_work_orders(repo_root)
    except (OSError, AdapterDeploymentWorkOrderContractError) as exc:
        print(
            f"Tainted Grail deployment work-order validation failed: {exc}",
            file=sys.stderr,
        )
        return 1
    print(
        "Tainted Grail deployment confirmation/work-order contract passed: exact preview "
        "binding, named confirmation, typed scope/expiry/window/preflight gates, deterministic "
        "operator checklist, read-only UI, and the no-execution boundary are wired."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
