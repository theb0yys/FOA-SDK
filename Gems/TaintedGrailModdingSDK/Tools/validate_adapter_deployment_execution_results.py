#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate Slice 15 deployment execution-result and evidence-return contracts."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable


class AdapterDeploymentExecutionResultContractError(RuntimeError):
    """Raised when Slice 15 contracts are incomplete or expose execution paths."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise AdapterDeploymentExecutionResultContractError(
            f"Required deployment execution-result file is missing: {path}"
        )
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise AdapterDeploymentExecutionResultContractError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise AdapterDeploymentExecutionResultContractError(
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


def validate_adapter_deployment_execution_results(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    test_manifest = (
        code_root
        / "taintedgrailmoddingsdk_deployment_execution_result_tests_files.cmake"
    )
    cmake = read_text(code_root / "CMakeLists.txt")

    require_fragments(
        core_manifest,
        (
            "Source/AdapterDeploymentExecutionResultContracts.cpp",
            "Source/AdapterDeploymentExecutionResultContracts.h",
            "Source/AdapterDeploymentExecutionEvidenceService.cpp",
            "Source/AdapterDeploymentExecutionEvidenceService.h",
            "Source/AdapterDeploymentExecutionEvidenceServicePart1.inl",
            "Source/AdapterDeploymentExecutionEvidenceServicePart2.inl",
            "Source/AdapterDeploymentExecutionEvidenceServicePart3.inl",
        ),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        (
            "Source/AdapterDeploymentExecutionEvidenceWidget.cpp",
            "Source/AdapterDeploymentExecutionEvidenceWidget.h",
        ),
        "Editor manifest",
    )
    if manifest_entries(test_manifest) != (
        "Tests/AdapterDeploymentExecutionResultTests.cpp",
    ):
        raise AdapterDeploymentExecutionResultContractError(
            "Deployment execution-result test ownership must contain only "
            "Tests/AdapterDeploymentExecutionResultTests.cpp."
        )
    require_fragments(
        cmake,
        (
            "taintedgrailmoddingsdk_deployment_execution_result_tests_files.cmake",
        ),
        "Catalog test target",
    )

    service_parts = sorted(
        source_root.glob("AdapterDeploymentExecutionEvidenceServicePart*.inl")
    )
    if len(service_parts) < 3:
        raise AdapterDeploymentExecutionResultContractError(
            "Deployment execution-result evidence implementation parts are incomplete."
        )

    contracts = "\n".join(
        [
            read_text(source_root / "AdapterDeploymentExecutionResultContracts.h"),
            read_text(source_root / "AdapterDeploymentExecutionResultContracts.cpp"),
        ]
    )
    service = "\n".join(
        [
            read_text(source_root / "AdapterDeploymentExecutionEvidenceService.h"),
            read_text(source_root / "AdapterDeploymentExecutionEvidenceService.cpp"),
            *(read_text(path) for path in service_parts),
        ]
    )
    combined = contracts + "\n" + service

    require_fragments(
        combined,
        (
            "enum class AdapterDeploymentExecutorReviewDecision",
            "enum class AdapterDeploymentExecutionOutcome",
            "enum class AdapterDeploymentVerificationStatus",
            "enum class AdapterDeploymentExecutionFailureKind",
            "enum class AdapterDeploymentExecutionLogKind",
            "enum class AdapterDeploymentRollbackAction",
            "enum class AdapterDeploymentExecutionEnvelopeStatus",
            '"not_attempted"',
            '"succeeded"',
            '"failed"',
            '"skipped"',
            '"not_checked"',
            '"matched"',
            '"mismatched"',
            '"remove_added"',
            '"restore_replaced"',
            '"restore_removed"',
            '"work_order_not_ready"',
            '"executor_unreviewed"',
            '"work_order_binding_mismatch"',
            '"envelope_invalid"',
            '"step_binding_mismatch"',
            '"backup_binding_mismatch"',
            '"verification_binding_mismatch"',
            '"rollback_binding_mismatch"',
            '"failure_log_binding_mismatch"',
            '"accepted"',
            "struct AdapterDeploymentExecutorReview",
            "struct AdapterDeploymentExecutionStepResult",
            "struct AdapterDeploymentBackupResult",
            "struct AdapterDeploymentTargetVerification",
            "struct AdapterDeploymentRollbackResult",
            "struct AdapterDeploymentExecutionFailure",
            "struct AdapterDeploymentExecutionLogReference",
            "struct AdapterDeploymentExecutionResultEnvelope",
            "class AdapterDeploymentExecutionResultRegistry",
            "class AdapterDeploymentExecutionEvidenceService",
            "BuildEvidenceReturn",
            "m_workOrderCanonicalJson",
            "m_workOrderFingerprint",
            "m_executorFingerprint",
            "m_backupFingerprint",
            "m_observedFingerprint",
            "m_restoreFingerprint",
            "m_finalFingerprint",
            "ValidateWorkOrderReadiness",
            "ValidateExecutorReview",
            "ValidateEnvelopeAndWorkOrderBinding",
            "ValidateStepBindings",
            "ValidateBackupBindings",
            "ValidateVerificationBindings",
            "ValidateRollbackBindings",
            "ValidateFailureAndLogBindings",
            "BuildPrimaryEvidence",
            "BuildLogEvidence",
            "m_sourceDocuments",
            "m_evidenceDocuments",
            "m_accepted = false",
            "contract_validated",
            "Nothing is executed",
        ),
        "Core deployment execution-result contract",
    )
    reject_fragments(
        combined,
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
            "SourceEvidenceRegistry::Get().Register",
            "ApplyCatalogValidationDecision",
            "ApplyCatalogGovernanceDecision",
        ),
        "Core deployment execution-result contract",
    )

    widget = read_text(
        source_root / "AdapterDeploymentExecutionEvidenceWidget.cpp"
    )
    require_fragments(
        widget,
        (
            "Tainted Grail Deployment Execution Result Evidence",
            "Read-only Phase 8 contract verification",
            "separately reviewed executor",
            "candidate source/evidence documents",
            "Nothing is executed",
            "QAbstractItemView::NoEditTriggers",
            "AdapterDeploymentExecutionResultRegistry::Get()",
            "BuildEvidenceReturn",
            'tr("Backup outcome")',
            'tr("Target verification")',
            'tr("Rollback / restore outcome")',
            'tr("Evidence candidates")',
            "Automatic evidence promotion: prohibited",
        ),
        "Deployment execution-result evidence widget",
    )
    reject_fragments(
        widget,
        (
            "QPushButton",
            "QLineEdit",
            "QFileDialog",
            "RegisterEnvelope",
            "SaveResult",
            "ImportEvidence",
            "PromoteEvidence",
            "CopyFile",
            "RemoveFile",
            "DeployFiles",
            "LaunchFoA",
            "ExecuteAdapter",
        ),
        "Deployment execution-result evidence widget",
    )

    system_component = read_text(
        source_root / "TaintedGrailModdingSDKSystemComponent.cpp"
    )
    require_fragments(
        system_component,
        (
            '#include "AdapterDeploymentExecutionResultContracts.h"',
            '#include "AdapterDeploymentExecutionEvidenceWidget.h"',
            "AdapterDeploymentExecutionEvidenceViewPaneName",
            "RegisterViewPane<AdapterDeploymentExecutionEvidenceWidget>",
            "UnregisterViewPane(AdapterDeploymentExecutionEvidenceViewPaneName)",
            "TaintedGrailModdingSDK.DeploymentExecutionEvidence",
            "AdapterDeploymentExecutionResultRegistry::Get().Clear()",
        ),
        "Editor pane registration",
    )

    tests = read_text(tests_root / "AdapterDeploymentExecutionResultTests.cpp")
    require_fragments(
        tests,
        (
            "TypedVocabulariesReferencesAndDatesAreStrict",
            "UnboundRegistrationIsProhibited",
            "BoundRegistryRejectsDuplicateResultIdentity",
            "CompleteEnvelopeReturnsCandidateEvidenceOnly",
            "CallerSelectedWorkOrderAndResultFingerprintsFailClosed",
            "MissingStepBackupVerificationAndRollbackFailClosed",
            "UnknownFailureKindAndImpossibleCaptureDateAreRejectedBeforeStorage",
            "CanonicalPayloadIsOrderIndependentAndContentSensitive",
            "EXPECT_TRUE(result.m_accepted)",
            "m_stepResultCount",
            "m_backupResultCount",
            "m_verificationCount",
            "m_rollbackResultCount",
            "m_sourceDocuments.empty",
            "m_evidenceDocuments.empty",
        ),
        "Deployment execution-result tests",
    )

    workflow = read_text(
        repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml"
    )
    require_fragments(
        workflow,
        (
            "Test deployment execution-result validator",
            "test_validate_adapter_deployment_execution_results.py",
            "Validate deployment execution-result evidence contract",
            "validate_adapter_deployment_execution_results.py",
        ),
        "Focused workflow",
    )

    design = read_text(
        repo_root
        / "docs/tainted-grail-sdk/FOA_ADAPTER_DEPLOYMENT_EXECUTION_RESULTS.md"
    )
    require_fragments(
        design,
        (
            "Exact reviewed deployment work order",
            "separately reviewed executor",
            "attempted step identities",
            "backup and restore outcomes",
            "deployed fingerprints",
            "rollback results",
            "safe log references",
            "candidate evidence",
            "No executor, deployment command, launch path, adapter call, or automatic evidence promotion",
        ),
        "Deployment execution-result design",
    )

    docs_index = read_text(repo_root / "docs/tainted-grail-sdk/README.md")
    require_fragments(
        docs_index,
        (
            "FoA Adapter Deployment Execution Results",
            "FOA_ADAPTER_DEPLOYMENT_EXECUTION_RESULTS.md",
        ),
        "Documentation index",
    )

    work_order_design = read_text(
        repo_root
        / "docs/tainted-grail-sdk/FOA_ADAPTER_DEPLOYMENT_CONFIRMATION_WORK_ORDERS.md"
    )
    require_fragments(
        work_order_design,
        (
            "Slice 15",
            "deployment execution-result",
            "no executor or automatic evidence promotion",
        ),
        "Deployment work-order handoff",
    )

    roadmap = read_text(repo_root / "ROADMAP.md")
    require_fragments(
        roadmap,
        (
            "Typed deployment execution-result and verification envelope",
            "Status: implemented, continuing hardening and Windows UI verification.",
            "backup/restore outcomes, target verification, rollback, failures, and logs",
            "No executor, deployment command, launch path, adapter call, or automatic evidence promotion",
        ),
        "Roadmap",
    )

    changelog = read_text(repo_root / "CHANGELOG.md")
    require_fragments(
        changelog,
        (
            "AdapterDeploymentExecutionEvidenceService",
            "Tainted Grail Deployment Execution Result Evidence",
            "automatic evidence promotion",
        ),
        "Changelog",
    )

    user_guide = read_text(repo_root / "docs/tainted-grail-sdk/USER_GUIDE.md")
    require_fragments(
        user_guide,
        (
            "Tainted Grail Deployment Execution Result Evidence",
            "work_order_not_ready",
            "verification_binding_mismatch",
            "failed execution can still be contract-valid evidence",
            "nothing is executed or promoted automatically",
        ),
        "User guide",
    )

    manual_ui = read_text(
        repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md"
    )
    require_fragments(
        manual_ui,
        (
            "All sixteen TG SDK panes",
            "Tainted Grail Deployment Execution Result Evidence",
            "zero registered deployment execution-result envelopes",
            "non-editable",
        ),
        "Windows manual UI smoke",
    )

    core_graph = read_text(
        repo_root / "docs/tainted-grail-sdk/CORE_FRAMEWORK_BUILD_GRAPH.md"
    )
    require_fragments(
        core_graph,
        (
            "deployment execution-result verification and candidate evidence return",
            "no executor or automatic evidence promotion",
        ),
        "Core/Framework build graph",
    )

    release = read_text(repo_root / "docs/tainted-grail-sdk/RELEASE_PROCESS.md")
    require_fragments(
        release,
        (
            "deployment execution-result envelope",
            "backup, restore, target-verification, rollback, and log evidence",
            "candidate evidence remains unpromoted",
        ),
        "Release process",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_adapter_deployment_execution_results(repo_root)
    except (OSError, AdapterDeploymentExecutionResultContractError) as exc:
        print(
            f"Deployment execution-result contract validation failed: {exc}",
            file=sys.stderr,
        )
        return 1
    print(
        "Deployment execution-result evidence contract passed: exact work-order "
        "and executor-review binding, typed steps/backups/verifications/rollback, "
        "safe logs, candidate evidence return, read-only presentation, tests, CI, "
        "and no-executor boundaries are present."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
