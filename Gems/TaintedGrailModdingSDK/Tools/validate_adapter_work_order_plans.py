#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate deterministic, canonical, execution-prohibited Phase 7 work-order plans."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable


class AdapterWorkOrderPlanError(RuntimeError):
    """Raised when plan generation is incomplete, mutable, or runtime-capable."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise AdapterWorkOrderPlanError(f"Required work-order plan file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise AdapterWorkOrderPlanError(f"{label} is missing required fragment {fragment!r}.")


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise AdapterWorkOrderPlanError(f"{label} contains forbidden fragment {fragment!r}.")


def manifest_entries(path: Path) -> tuple[str, ...]:
    return tuple(
        re.findall(
            r"^\s+((?:Source|Tests)/[^\s)]+\.(?:cpp|h))\s*$",
            read_text(path),
            re.MULTILINE,
        )
    )


def validate_adapter_work_order_plans(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    work_order_manifest = code_root / "taintedgrailmoddingsdk_work_order_tests_files.cmake"
    cmake = read_text(code_root / "CMakeLists.txt")

    require_fragments(
        core_manifest,
        (
            "Source/AdapterWorkOrderPlanningService.cpp",
            "Source/AdapterWorkOrderPlanningService.h",
        ),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        (
            "Source/AdapterWorkOrderPlanWidget.cpp",
            "Source/AdapterWorkOrderPlanWidget.h",
        ),
        "Editor manifest",
    )
    if manifest_entries(work_order_manifest) != ("Tests/AdapterWorkOrderPlanningTests.cpp",):
        raise AdapterWorkOrderPlanError(
            "Work-order test manifest must contain only Tests/AdapterWorkOrderPlanningTests.cpp as a translation unit."
        )
    require_fragments(
        cmake,
        ("taintedgrailmoddingsdk_work_order_tests_files.cmake",),
        "Catalog test target",
    )

    planner_parts = sorted(source_root.glob("AdapterWorkOrderPlanningServicePart*.inl"))
    if len(planner_parts) < 2:
        raise AdapterWorkOrderPlanError("Core work-order planner implementation parts are incomplete.")
    planner = "\n".join(
        [
            read_text(source_root / "AdapterWorkOrderPlanningService.h"),
            read_text(source_root / "AdapterWorkOrderPlanningService.cpp"),
            *(read_text(path) for path in planner_parts),
        ]
    )
    require_fragments(
        planner,
        (
            "struct AdapterWorkOrderArgument",
            "struct AdapterWorkOrderStep",
            "struct AdapterWorkOrderPlan",
            "struct AdapterWorkOrderRefusal",
            "struct AdapterWorkOrderPlanSet",
            "class AdapterWorkOrderPlanningService",
            "BuildPlans",
            "BuildCapabilityMatrix",
            "GroupIsSupported",
            'row->m_status != "supported"',
            "group.m_rows.size() != sizeof(AllCapabilities)",
            "CollectReadySubjects",
            "CollectPermissionProof",
            "CollectRelationshipValidationProof",
            "RecordInputEvidenceIsValid",
            "RecordPayloadIsComplete",
            "typed recipe output lacks evidence for the exact output association, quantity, and probability",
            "canonical plans require a resolved target record ID",
            "no exact relationship-bound validation proof is available",
            '"workorder.plan:"',
            '":step:"',
            "SerializeCanonicalPlan",
            "ExecutionAllowed",
            "AppendJsonStringArray",
            "SortSteps",
            "m_canonicalJson",
            "m_inputEvidenceIds",
            "m_declarationEvidenceIds",
            "m_permissionEventIds",
            "m_permissionEvidenceIds",
            "m_validationProofIds",
            "m_executionAllowed = false",
            "AdapterCapability::Persistence",
            "AdapterCapability::Cleanup",
            "AdapterCapability::Rollback",
            '"ingredient."',
            '"output."',
        ),
        "Core work-order planner",
    )
    reject_fragments(
        planner,
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
            "adapter.tgworkorder.json",
            "ExecuteWorkOrder",
            "DispatchWorkOrder",
            "RunWorkOrder",
            "LaunchFoA",
            "DeployWorkOrder",
            "SaveGame",
            "QDateTime",
            "std::chrono",
            "system_clock",
            "random_device",
            "Uuid::CreateRandom",
        ),
        "Core work-order planner",
    )

    widget = read_text(source_root / "AdapterWorkOrderPlanWidget.cpp")
    require_fragments(
        widget,
        (
            "Tainted Grail Adapter Work-Order Plans",
            "Read-only canonical plan preview",
            "Execution is always prohibited",
            "BuildPlans",
            "AdapterContractRegistry::Get()",
            "QAbstractItemView::NoEditTriggers",
            'tr("Canonical JSON / refusal reasons")',
            "plan.m_canonicalJson",
            "RefusalDetails",
            'tr("prohibited")',
        ),
        "Adapter work-order plan widget",
    )
    reject_fragments(
        widget,
        (
            "QPushButton",
            "QLineEdit",
            "QFileDialog",
            "QSaveFile",
            "RegisterDeclaration",
            "SaveWorkOrder",
            "ExportWorkOrder",
            "ExecuteWorkOrder",
            "DispatchWorkOrder",
            "LaunchFoA",
            "DeployWorkOrder",
        ),
        "Adapter work-order plan widget",
    )

    system_component = read_text(source_root / "TaintedGrailModdingSDKSystemComponent.cpp")
    require_fragments(
        system_component,
        (
            '#include "AdapterWorkOrderPlanWidget.h"',
            "AdapterWorkOrderPlanViewPaneName",
            "RegisterViewPane<AdapterWorkOrderPlanWidget>",
            "UnregisterViewPane(AdapterWorkOrderPlanViewPaneName)",
            "TaintedGrailModdingSDK.AdapterWorkOrderPlans",
        ),
        "Editor pane registration",
    )

    test_parts = sorted(tests_root.glob("AdapterWorkOrderPlanningTestsPart*.inl"))
    fixture_parts = sorted(tests_root.glob("AdapterWorkOrderPlanningTestFixturePart*.inl"))
    if len(test_parts) < 2 or len(fixture_parts) < 2:
        raise AdapterWorkOrderPlanError("Production-linked work-order test parts are incomplete.")
    tests = "\n".join(
        [
            read_text(tests_root / "AdapterWorkOrderPlanningTests.cpp"),
            *(read_text(path) for path in fixture_parts),
            *(read_text(path) for path in test_parts),
        ]
    )
    require_fragments(
        tests,
        (
            "AnyNonSupportedCompatibilityRowRefusesWholePlan",
            "FullySupportedCatalogBuildsCanonicalPlanOnly",
            "AggregateSupportCannotLeakUnreviewedSubjectsIntoSteps",
            "RelationshipStepsBindResolvedTargetsAndProof",
            "MissingRelationshipProofRefusesWholePlan",
            "InvalidTypedPayloadEvidenceRefusesWholePlan",
            "CanonicalSerializationIsDeterministic",
            "PlanningDoesNotMutateInputs",
            'EXPECT_EQ(result.m_generatedPlanCount, 0)',
            'EXPECT_EQ(plan.m_steps.size(), 11)',
            "ExecutionAllowed",
            '"validation.relationship.relationship.vendor"',
            "declarationCountBefore",
            "relationshipCountBefore",
        ),
        "Adapter work-order planning tests",
    )

    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml")
    require_fragments(
        workflow,
        (
            "Test adapter work-order plan validator",
            "test_validate_adapter_work_order_plans.py",
            "Validate adapter work-order plan contract",
            "validate_adapter_work_order_plans.py",
        ),
        "Focused workflow",
    )

    design = read_text(repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_WORK_ORDER_PLANS.md")
    require_fragments(
        design,
        (
            "canonical plans only",
            "All eleven compatibility rows",
            "stable plan identity",
            "canonical JSON",
            "input evidence",
            "permission evidence",
            "validation proof",
            "resolved relationship targets",
            "transient",
            "ExecutionAllowed",
            "No runtime execution",
            "runtime-result evidence",
        ),
        "Work-order planning design",
    )
    docs_index = read_text(repo_root / "docs/tainted-grail-sdk/README.md")
    require_fragments(
        docs_index,
        ("FoA Adapter Work-Order Plans", "FOA_ADAPTER_WORK_ORDER_PLANS.md"),
        "Documentation index",
    )
    adapter_contracts = read_text(repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_CONTRACTS.md")
    require_fragments(
        adapter_contracts,
        ("Slice 9", "canonical work-order plans", "execution remains prohibited"),
        "Adapter contract design",
    )
    roadmap = read_text(repo_root / "ROADMAP.md")
    require_fragments(
        roadmap,
        (
            "Deterministic work-order plan generation",
            "Status: implemented, continuing hardening and Windows UI verification.",
            "Runtime-result evidence envelope",
        ),
        "Roadmap",
    )
    changelog = read_text(repo_root / "CHANGELOG.md")
    require_fragments(
        changelog,
        (
            "AdapterWorkOrderPlanningService",
            "Tainted Grail Adapter Work-Order Plans",
            "execution remains prohibited",
        ),
        "Changelog",
    )
    user_guide = read_text(repo_root / "docs/tainted-grail-sdk/USER_GUIDE.md")
    require_fragments(
        user_guide,
        (
            "Tainted Grail Adapter Work-Order Plans",
            "generated",
            "refused",
            "canonical JSON",
            "Execution is prohibited",
        ),
        "User guide",
    )
    manual_ui = read_text(repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md")
    require_fragments(
        manual_ui,
        (
            "All ten TG SDK panes",
            "Tainted Grail Adapter Work-Order Plans",
            "one refused plan group and zero generated steps",
            "non-editable",
        ),
        "Windows manual UI smoke",
    )
    core_graph = read_text(repo_root / "docs/tainted-grail-sdk/CORE_FRAMEWORK_BUILD_GRAPH.md")
    require_fragments(
        core_graph,
        ("work-order planning", "execution-prohibited"),
        "Core/Framework build graph",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_adapter_work_order_plans(repo_root)
    except (OSError, AdapterWorkOrderPlanError) as exc:
        print(f"Tainted Grail adapter work-order plan validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail adapter work-order plans passed: all-supported refusal, exact reviewed "
        "payloads, stable IDs, canonical JSON, read-only UI, and execution prohibition are wired."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
