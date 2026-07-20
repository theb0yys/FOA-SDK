#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the Phase 7 typed FoA adapter contract foundation."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Iterable


class AdapterContractError(RuntimeError):
    """Raised when the adapter contract foundation is incomplete or unsafe."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise AdapterContractError(f"Required adapter-contract file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise AdapterContractError(f"{label} is missing required fragment {fragment!r}.")


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise AdapterContractError(f"{label} contains forbidden fragment {fragment!r}.")


def validate_adapter_contracts(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    test_manifest = read_text(code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake")
    require_fragments(
        core_manifest,
        (
            "Source/AdapterContractRegistry.cpp",
            "Source/AdapterContractRegistry.h",
            "Source/AdapterCompatibilityService.cpp",
            "Source/AdapterCompatibilityService.h",
        ),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        (
            "Source/AdapterCapabilityMatrixWidget.cpp",
            "Source/AdapterCapabilityMatrixWidget.h",
        ),
        "Editor manifest",
    )
    require_fragments(
        test_manifest,
        ("Tests/AdapterContractTests.cpp",),
        "Catalog test manifest",
    )

    registry = read_text(source_root / "AdapterContractRegistry.h") + "\n" + read_text(
        source_root / "AdapterContractRegistry.cpp"
    )
    capabilities = (
        '"item_grant"',
        '"recipe_learn"',
        '"recipe_append"',
        '"custom_item_registration"',
        '"custom_recipe_registration"',
        '"vendor_mutation"',
        '"loot_mutation"',
        '"reward_mutation"',
        '"persistence"',
        '"cleanup"',
        '"rollback"',
    )
    require_fragments(
        registry,
        (
            "enum class AdapterCapability",
            "struct AdapterDeclaration",
            "class AdapterContractRegistry",
            "TryParseAdapterSemanticVersion",
            "CompareAdapterSemanticVersions",
            "IsAdapterVersionCompatible",
            "required.m_major == 0",
            "required.m_minor != declared.m_minor",
            "AdapterContractRegistry& AdapterContractRegistry::Get()",
            "static AdapterContractRegistry registry",
            "RegisterDeclaration",
            'runtimeTarget != "Mono"',
            'runtimeTarget != "IL2CPP"',
            *capabilities,
        ),
        "Adapter contract registry",
    )

    compatibility = read_text(source_root / "AdapterCompatibilityService.h") + "\n" + read_text(
        source_root / "AdapterCompatibilityService.cpp"
    )
    require_fragments(
        compatibility,
        (
            "class AdapterCompatibilityService",
            "BuildCapabilityMatrix",
            '"supported"',
            '"unsupported"',
            '"version_mismatch"',
            '"permission_missing"',
            '"proof_missing"',
            '"existing_item_grant"',
            '"existing_recipe_learn"',
            '"runtime_recipe_append"',
            '"vendor_or_loot_injection"',
            '"quest_or_contract_reward_injection"',
            "FindValidationById",
            "FindEffectiveGovernanceEvent",
            "m_sourceFingerprint",
            "m_profileId",
            "m_gameVersion",
            "m_branch",
            "m_runtimeTarget",
            "declarations.empty()",
        ),
        "Adapter compatibility service",
    )
    reject_fragments(
        registry + compatibility,
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
            "WorkOrder",
            "GenerateWorkOrder",
            "Deploy",
            "SaveGame",
            "LaunchFoA",
        ),
        "Core adapter contracts",
    )
    reject_fragments(
        registry,
        (
            "SaveDeclaration",
            "LoadDeclaration",
            "AdapterPersistenceService",
            "adapter.tgadapter.json",
        ),
        "Transient Core adapter registry",
    )

    widget = read_text(source_root / "AdapterCapabilityMatrixWidget.cpp")
    require_fragments(
        widget,
        (
            "Read-only Phase 7 compatibility analysis",
            "does not load or execute an adapter",
            "generate a work order",
            "QAbstractItemView::NoEditTriggers",
            "BuildCapabilityMatrix",
            "AdapterContractRegistry::Get()",
            'tr("Capability")',
            'tr("Status")',
        ),
        "Adapter capability matrix",
    )
    reject_fragments(
        widget,
        (
            "QPushButton",
            "QLineEdit",
            "RegisterDeclaration",
            "Save",
            "Launch",
            "Deploy",
            "WorkOrder",
        ),
        "Adapter capability matrix",
    )

    system_component = read_text(source_root / "TaintedGrailModdingSDKSystemComponent.cpp")
    require_fragments(
        system_component,
        (
            '#include "AdapterCapabilityMatrixWidget.h"',
            "AdapterCapabilityMatrixViewPaneName",
            "RegisterViewPane<AdapterCapabilityMatrixWidget>",
            "UnregisterViewPane(AdapterCapabilityMatrixViewPaneName)",
            "TaintedGrailModdingSDK.AdapterCapabilityMatrix",
            "AdapterContractRegistry::Get().Clear()",
        ),
        "Editor pane registration",
    )

    tests = read_text(code_root / "Tests/AdapterContractTests.cpp")
    require_fragments(
        tests,
        (
            "SemanticVersionsAndTypedCapabilitiesAreStrict",
            "EmptyRegistryAndMissingCapabilitiesFailClosed",
            "VersionMismatchPrecedesPermissionAndProofChecks",
            "PermissionMissingAndProofMissingRemainDistinct",
            "SupportedResultIsDeterministicAndDoesNotMutateInputs",
            'EXPECT_EQ(row->m_status, "version_mismatch")',
            'EXPECT_EQ(row->m_status, "permission_missing")',
            'EXPECT_EQ(row->m_status, "proof_missing")',
            'EXPECT_EQ(row->m_status, "supported")',
            "declarationCountBefore",
            "recordCountBefore",
        ),
        "Adapter contract tests",
    )

    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml")
    require_fragments(
        workflow,
        (
            "Test adapter contract validator",
            'test_validate_adapter_contracts.py',
            "Validate adapter contract foundation",
            "validate_adapter_contracts.py",
        ),
        "Focused workflow",
    )

    design = read_text(repo_root / "docs/tainted-grail-sdk/FOA_ADAPTER_CONTRACTS.md")
    require_fragments(
        design,
        (
            "typed adapter identity",
            "semantic version",
            "item_grant",
            "recipe_learn",
            "recipe_append",
            "vendor_mutation",
            "loot_mutation",
            "reward_mutation",
            "persistence",
            "cleanup",
            "rollback",
            "supported",
            "unsupported",
            "version_mismatch",
            "permission_missing",
            "proof_missing",
            "transient registry",
            "No runtime adapter implementation",
            "Slice 9",
            "canonical work-order plans",
            "execution remains prohibited",
        ),
        "FoA adapter contract design",
    )
    roadmap = read_text(repo_root / "ROADMAP.md")
    require_fragments(
        roadmap,
        (
            "Adapter capability contract foundation",
            "Status: implemented, continuing hardening and Windows UI verification.",
            "Deterministic work-order plan generation",
            "runtime-result evidence envelope",
        ),
        "Roadmap",
    )
    changelog = read_text(repo_root / "CHANGELOG.md")
    require_fragments(
        changelog,
        (
            "AdapterContractRegistry",
            "Tainted Grail Adapter Capability Matrix",
            "no runtime adapter implementation",
        ),
        "Changelog",
    )
    docs_index = read_text(repo_root / "docs/tainted-grail-sdk/README.md")
    require_fragments(
        docs_index,
        ("FoA Adapter Contracts", "FOA_ADAPTER_CONTRACTS.md"),
        "Documentation index",
    )
    user_guide = read_text(repo_root / "docs/tainted-grail-sdk/USER_GUIDE.md")
    require_fragments(
        user_guide,
        (
            "Tainted Grail Adapter Capability Matrix",
            "supported",
            "unsupported",
            "version_mismatch",
            "permission_missing",
            "proof_missing",
        ),
        "User guide",
    )
    manual_ui = read_text(repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md")
    require_fragments(
        manual_ui,
        (
            "All ten TG SDK panes",
            "Tainted Grail Adapter Capability Matrix",
            "unsupported` rows when no adapter declaration is registered",
            "non-editable",
        ),
        "Windows manual UI smoke",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_adapter_contracts(repo_root)
    except (OSError, AdapterContractError) as exc:
        print(f"Tainted Grail adapter contract validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail adapter contract foundation passed: typed declarations, semantic versions, "
        "capabilities, fail-closed compatibility, transient registry, read-only matrix, tests, and docs are present."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
