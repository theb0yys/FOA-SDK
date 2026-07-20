#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate typed, evidence-returning, non-executing Phase 7 runtime-result contracts."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable


class AdapterRuntimeResultContractError(RuntimeError):
    """Raised when runtime-result contracts are incomplete or expose execution paths."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise AdapterRuntimeResultContractError(
            f"Required adapter runtime-result contract file is missing: {path}"
        )
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise AdapterRuntimeResultContractError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise AdapterRuntimeResultContractError(
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


def validate_adapter_runtime_results(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    result_test_manifest = code_root / "taintedgrailmoddingsdk_runtime_result_tests_files.cmake"
    cmake = read_text(code_root / "CMakeLists.txt")

    require_fragments(
        core_manifest,
        (
            "Source/AdapterRuntimeResultContracts.cpp",
            "Source/AdapterRuntimeResultContracts.h",
            "Source/AdapterRuntimeResultEvidenceService.cpp",
            "Source/AdapterRuntimeResultEvidenceService.h",
        ),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        (
            "Source/AdapterRuntimeResultEvidenceWidget.cpp",
            "Source/AdapterRuntimeResultEvidenceWidget.h",
        ),
        "Editor manifest",
    )
    if manifest_entries(result_test_manifest) != (
        "Tests/AdapterRuntimeResultEvidenceTests.cpp",
    ):
        raise AdapterRuntimeResultContractError(
            "Runtime-result test manifest must contain only "
            "Tests/AdapterRuntimeResultEvidenceTests.cpp."
        )
    require_fragments(
        cmake,
        ("taintedgrailmoddingsdk_runtime_result_tests_files.cmake",),
        "Catalog test target",
    )

    contracts = read_text(source_root / "AdapterRuntimeResultContracts.h") + "\n" + read_text(
        source_root / "AdapterRuntimeResultContracts.cpp"
    )
    require_fragments(
        contracts,
        (
            "enum class AdapterRuntimeOutcome",
            "NotAttempted",
            "Succeeded",
            "Failed",
            "Skipped",
            "enum class AdapterRuntimeFailureKind",
            "Persistence",
            "Cleanup",
            "Rollback",
            "enum class AdapterRuntimeLogKind",
            "struct AdapterRuntimeFailure",
            "struct AdapterRuntimeLogReference",
            "struct AdapterRuntimeStepResult",
            "struct AdapterRuntimeRecoveryResult",
            "struct AdapterRuntimeResultEnvelope",
            "class AdapterRuntimeResultRegistry",
            "RegisterEnvelope",
            "m_attempted",
            "m_cleanupResult",
            "m_rollbackResult",
            "m_planCanonicalJson",
            "m_planFingerprint",
            "m_resultFingerprint",
            "IsSha256Fingerprint",
            "CanonicalSha256Matches",
            "IsAdapterRuntimeLogReference",
            "safe relative locators",
            "Failed and skipped outcomes require failures",
        ),
        "Runtime-result contracts",
    )

    service_parts = sorted(source_root.glob("AdapterRuntimeResultEvidenceServicePart*.inl"))
    service = "\n".join(
        [
            read_text(source_root / "AdapterRuntimeResultEvidenceService.h"),
            read_text(source_root / "AdapterRuntimeResultEvidenceService.cpp"),
            *(read_text(path) for path in service_parts),
        ]
    )
    require_fragments(
        service,
        (
            "struct AdapterRuntimeEvidenceReturn",
            "class AdapterRuntimeResultEvidenceService",
            "BuildEvidenceReturn",
            "runtime_result.plan_canonical_mismatch",
            "runtime_result.step_count_mismatch",
            "runtime_result.step_missing",
            "runtime_result.step_unknown",
            "runtime_result.cleanup_mismatch",
            "runtime_result.rollback_mismatch",
            "RecoveryMatchesStep",
            "FindPlanCapabilityStep(plan, \"cleanup\")",
            "FindPlanCapabilityStep(plan, \"rollback\")",
            "SourceDocument",
            "EvidenceDocument",
            '"adapter_runtime_step_result"',
            '"adapter_runtime_failure"',
            'const char* recoveryNames[] = { "cleanup", "rollback" }',
            'AZStd::string("adapter_runtime_") + recoveryNames[index] + "_result"',
            '"adapter_runtime_log_reference"',
            '"adapter_runtime_plan_binding"',
            "m_sourceDocuments",
            "m_evidenceDocuments",
            "m_accepted = true",
            "validation and permission remain unchanged",
            "Referenced log content is not opened, persisted, or inspected",
        ),
        "Runtime-result evidence service",
    )
    reject_fragments(
        contracts + service,
        (
            "#include <Q",
            "#include <Qt",
            "AzToolsFramework",
            "FoundationService.h",
            "QProcess",
            "CreateProcess",
            "WriteProcessMemory",
            "FoA.exe",
            "BepInEx",
            "Harmony",
            "system(",
            "std::filesystem",
            "AZ::IO",
            "QFile",
            "QSaveFile",
            "PersistenceJsonUtils",
            "SaveObjectToFile",
            "RegisterSource(",
            "RegisterEvidence(",
            "ApplyCatalogGovernanceDecision",
            "ApplyCatalogValidationDecision",
            "ExecuteWorkOrder",
            "DispatchWorkOrder",
            "LaunchFoA",
            "DeployWorkOrder",
            "SaveGame",
        ),
        "Core runtime-result contracts",
    )

    widget = read_text(source_root / "AdapterRuntimeResultEvidenceWidget.cpp")
    require_fragments(
        widget,
        (
            "Tainted Grail Adapter Runtime Result Evidence",
            "Read-only Phase 7 contract verification",
            "Nothing is imported, persisted, promoted, dispatched, executed, deployed, launched",
            "QAbstractItemView::NoEditTriggers",
            "AdapterRuntimeResultRegistry::Get().GetEnvelopes()",
            "BuildEvidenceReturn",
            'tr("Outcome")',
            'tr("Failures")',
            'tr("Cleanup / rollback")',
            'tr("Logs and fingerprints")',
            'tr("Evidence candidates")',
            "execution: prohibited",
        ),
        "Runtime-result evidence widget",
    )
    reject_fragments(
        widget,
        (
            "QPushButton",
            "QLineEdit",
            "QFileDialog",
            "RegisterEnvelope",
            "Save",
            "Export",
            "Dispatch",
            "Execute",
            "LaunchFoA",
            "Deploy",
        ),
        "Runtime-result evidence widget",
    )

    system_component = read_text(source_root / "TaintedGrailModdingSDKSystemComponent.cpp")
    require_fragments(
        system_component,
        (
            '#include "AdapterRuntimeResultContracts.h"',
            '#include "AdapterRuntimeResultEvidenceWidget.h"',
            "AdapterRuntimeResultEvidenceViewPaneName",
            "RegisterViewPane<AdapterRuntimeResultEvidenceWidget>",
            "UnregisterViewPane(AdapterRuntimeResultEvidenceViewPaneName)",
            "TaintedGrailModdingSDK.AdapterRuntimeResultEvidence",
            "AdapterRuntimeResultRegistry::Get().Clear()",
        ),
        "Editor pane registration",
    )

    tests = read_text(tests_root / "AdapterRuntimeResultEvidenceTests.cpp")
    require_fragments(
        tests,
        (
            "TypedOutcomeFailureAndLogVocabulariesAreStrict",
            "RegistryRejectsCallerSelectedAndDuplicateFingerprints",
            "PlanFingerprintMustHashExactCanonicalJson",
            "ExactAttemptedPlanProducesCandidateEvidenceOnly",
            "FailedStepAndTypedFailureReturnAsNewEvidence",
            "UnknownOrMissingStepIdentityFailsClosed",
            "OutcomeAndFailureShapeIsValidatedBeforeEvidenceReturn",
            "CleanupAndRollbackSummariesMustMatchTheirStepResults",
            "ImpossibleUtcDateAndUnknownFailureKindAreRejected",
            "CanonicalPayloadIsOrderIndependentButContentSensitive",
            'EXPECT_TRUE(HasEvidenceKind(result, "adapter_runtime_failure"))',
            'EXPECT_TRUE(HasIssue(result, "runtime_result.cleanup_mismatch"))',
        ),
        "Runtime-result evidence tests",
    )

    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml")
    require_fragments(
        workflow,
        (
            "Test adapter runtime-result validator",
            "test_validate_adapter_runtime_results.py",
            "Validate adapter runtime-result evidence contract",
            "validate_adapter_runtime_results.py",
        ),
        "Focused workflow",
    )

    design = read_text(repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_RUNTIME_RESULT_EVIDENCE.md")
    require_fragments(
        design,
        (
            "attempted plan",
            "attempted step",
            "not_attempted",
            "succeeded",
            "failed",
            "skipped",
            "cleanup",
            "rollback",
            "log references",
            "SHA-256",
            "new evidence",
            "does not promote validation or permission",
            "No adapter implementation or execution path",
        ),
        "Runtime-result evidence design",
    )
    docs_index = read_text(repo_root / "docs/tainted-grail-sdk/README.md")
    require_fragments(
        docs_index,
        (
            "FoA Adapter Runtime Result Evidence",
            "FOA_ADAPTER_RUNTIME_RESULT_EVIDENCE.md",
        ),
        "Documentation index",
    )
    work_orders = read_text(repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_WORK_ORDER_PLANS.md")
    require_fragments(
        work_orders,
        ("Slice 10", "runtime-result evidence", "no execution path"),
        "Work-order planning design",
    )
    roadmap = read_text(repo_root / "ROADMAP.md")
    require_fragments(
        roadmap,
        (
            "Runtime-result evidence envelope",
            "Status: implemented, continuing hardening and Windows UI verification.",
            "attempted-plan and attempted-step identities",
            "No actual FoA adapter implementation or execution path",
        ),
        "Roadmap",
    )
    changelog = read_text(repo_root / "CHANGELOG.md")
    require_fragments(
        changelog,
        (
            "AdapterRuntimeResultRegistry",
            "AdapterRuntimeResultEvidenceService",
            "Tainted Grail Adapter Runtime Result Evidence",
            "does not promote validation or permission",
        ),
        "Changelog",
    )
    core_graph = read_text(repo_root / "docs/tainted-grail-sdk/CORE_FRAMEWORK_BUILD_GRAPH.md")
    require_fragments(
        core_graph,
        ("runtime-result contract validation", "candidate source/evidence return"),
        "Core/Framework build graph",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_adapter_runtime_results(repo_root)
    except (OSError, AdapterRuntimeResultContractError) as exc:
        print(f"Tainted Grail adapter runtime-result validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail adapter runtime-result contract passed: exact attempted identities, typed "
        "outcomes and failures, cleanup/rollback, safe logs, fingerprints, candidate evidence, "
        "read-only UI, and the no-execution boundary are wired."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
