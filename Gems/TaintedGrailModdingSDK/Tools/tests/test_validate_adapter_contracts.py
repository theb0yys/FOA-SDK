#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from validate_adapter_contracts import AdapterContractError, validate_adapter_contracts  # noqa: E402


class AdapterContractValidatorTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        source = repo / "Gems/TaintedGrailModdingSDK/Code/Source"
        tests = repo / "Gems/TaintedGrailModdingSDK/Code/Tests"
        docs = repo / "docs/tainted-grail-sdk"
        workflow = repo / ".github/workflows"
        source.mkdir(parents=True)
        tests.mkdir(parents=True)
        docs.mkdir(parents=True)
        workflow.mkdir(parents=True)

        code = source.parent
        (code / "taintedgrailmoddingsdk_core_files.cmake").write_text(
            "Source/AdapterContractRegistry.cpp\nSource/AdapterContractRegistry.h\n"
            "Source/AdapterCompatibilityService.cpp\nSource/AdapterCompatibilityService.h\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_editor_files.cmake").write_text(
            "Source/AdapterCapabilityMatrixWidget.cpp\nSource/AdapterCapabilityMatrixWidget.h\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_catalog_tests_files.cmake").write_text(
            "Tests/AdapterContractTests.cpp\n",
            encoding="utf-8",
        )
        registry = """
enum class AdapterCapability
struct AdapterDeclaration class AdapterContractRegistry
TryParseAdapterSemanticVersion CompareAdapterSemanticVersions IsAdapterVersionCompatible
AdapterContractRegistry& AdapterContractRegistry::Get() static AdapterContractRegistry registry
required.m_major == 0 required.m_minor != declared.m_minor RegisterDeclaration
runtimeTarget != "Mono" runtimeTarget != "IL2CPP"
"item_grant" "recipe_learn" "recipe_append" "custom_item_registration"
"custom_recipe_registration" "vendor_mutation" "loot_mutation" "reward_mutation"
"persistence" "cleanup" "rollback"
"""
        (source / "AdapterContractRegistry.h").write_text(registry, encoding="utf-8")
        (source / "AdapterContractRegistry.cpp").write_text(registry, encoding="utf-8")
        compatibility = """
class AdapterCompatibilityService BuildCapabilityMatrix
"supported" "unsupported" "version_mismatch" "permission_missing" "proof_missing"
"existing_item_grant" "existing_recipe_learn" "runtime_recipe_append"
"vendor_or_loot_injection" "quest_or_contract_reward_injection"
FindValidationById GetGovernanceHistory m_sourceFingerprint m_profileId m_gameVersion m_branch
m_runtimeTarget declarations.empty()
"""
        (source / "AdapterCompatibilityService.h").write_text(compatibility, encoding="utf-8")
        (source / "AdapterCompatibilityService.cpp").write_text(compatibility, encoding="utf-8")
        widget = """
Read-only Phase 7 compatibility analysis does not load or execute an adapter
generate a work order QAbstractItemView::NoEditTriggers BuildCapabilityMatrix
AdapterContractRegistry::Get() tr("Capability") tr("Status")
"""
        (source / "AdapterCapabilityMatrixWidget.cpp").write_text(widget, encoding="utf-8")
        (source / "AdapterCapabilityMatrixWidget.h").write_text("widget", encoding="utf-8")
        (source / "TaintedGrailModdingSDKSystemComponent.cpp").write_text(
            '#include "AdapterCapabilityMatrixWidget.h"\nAdapterCapabilityMatrixViewPaneName\n'
            "RegisterViewPane<AdapterCapabilityMatrixWidget>\n"
            "UnregisterViewPane(AdapterCapabilityMatrixViewPaneName)\n"
            "TaintedGrailModdingSDK.AdapterCapabilityMatrix\n"
            "AdapterContractRegistry::Get().Clear()\n",
            encoding="utf-8",
        )
        (tests / "AdapterContractTests.cpp").write_text(
            "SemanticVersionsAndTypedCapabilitiesAreStrict\n"
            "EmptyRegistryAndMissingCapabilitiesFailClosed\n"
            "VersionMismatchPrecedesPermissionAndProofChecks\n"
            "PermissionMissingAndProofMissingRemainDistinct\n"
            "SupportedResultIsDeterministicAndDoesNotMutateInputs\n"
            'EXPECT_EQ(row->m_status, "version_mismatch")\n'
            'EXPECT_EQ(row->m_status, "permission_missing")\n'
            'EXPECT_EQ(row->m_status, "proof_missing")\n'
            'EXPECT_EQ(row->m_status, "supported")\n'
            "declarationCountBefore recordCountBefore\n",
            encoding="utf-8",
        )
        (workflow / "tainted-grail-sdk-foundation.yml").write_text(
            "Test adapter contract validator\n"
            "test_validate_adapter_contracts.py\n"
            "Validate adapter contract foundation\n"
            "validate_adapter_contracts.py\n",
            encoding="utf-8",
        )
        (docs / "FOA_ADAPTER_CONTRACTS.md").write_text(
            "typed adapter identity semantic version item_grant recipe_learn recipe_append "
            "vendor_mutation loot_mutation reward_mutation persistence cleanup rollback "
            "supported unsupported version_mismatch permission_missing proof_missing "
            "transient registry No runtime adapter implementation Slice 9",
            encoding="utf-8",
        )
        (docs / "README.md").write_text("FoA Adapter Contracts FOA_ADAPTER_CONTRACTS.md", encoding="utf-8")
        (docs / "USER_GUIDE.md").write_text(
            "Tainted Grail Adapter Capability Matrix supported unsupported version_mismatch "
            "permission_missing proof_missing",
            encoding="utf-8",
        )
        (docs / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md").write_text(
            "All nine TG SDK panes Tainted Grail Adapter Capability Matrix "
            "unsupported rows when no adapter declaration is registered non-editable",
            encoding="utf-8",
        )
        (repo / "ROADMAP.md").write_text(
            "Adapter capability contract foundation\n"
            "Status: implemented, continuing hardening and Windows UI verification.\n"
            "deterministic work-order generation plans only",
            encoding="utf-8",
        )
        (repo / "CHANGELOG.md").write_text(
            "AdapterContractRegistry Tainted Grail Adapter Capability Matrix "
            "no runtime adapter implementation",
            encoding="utf-8",
        )
        return repo

    def test_complete_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_adapter_contracts(self.make_repo(Path(temporary)))

    def test_runtime_code_in_core_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            source = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterCompatibilityService.cpp"
            source.write_text(source.read_text() + "\nBepInEx\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterContractError, "BepInEx"):
                validate_adapter_contracts(repo)

    def test_work_order_generation_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            source = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterCompatibilityService.cpp"
            source.write_text(source.read_text() + "\nGenerateWorkOrder\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterContractError, "WorkOrder"):
                validate_adapter_contracts(repo)

    def test_editing_controls_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            widget = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterCapabilityMatrixWidget.cpp"
            widget.write_text(widget.read_text() + "\nQPushButton\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterContractError, "QPushButton"):
                validate_adapter_contracts(repo)

    def test_adapter_persistence_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            source = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterContractRegistry.cpp"
            source.write_text(source.read_text() + "\nSaveDeclaration\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterContractError, "SaveDeclaration"):
                validate_adapter_contracts(repo)


if __name__ == "__main__":
    unittest.main()
