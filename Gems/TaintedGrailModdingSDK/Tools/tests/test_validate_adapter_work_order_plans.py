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

from validate_adapter_work_order_plans import (  # noqa: E402
    AdapterWorkOrderPlanError,
    validate_adapter_work_order_plans,
)


class AdapterWorkOrderPlanValidatorTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        gem = repo / "Gems/TaintedGrailModdingSDK"
        code = gem / "Code"
        source = code / "Source"
        tests = code / "Tests"
        tools_tests = gem / "Tools/tests"
        docs = repo / "docs/tainted-grail-sdk"
        workflow = repo / ".github/workflows"
        for path in (source, tests, tools_tests, docs, workflow):
            path.mkdir(parents=True, exist_ok=True)

        (code / "taintedgrailmoddingsdk_core_files.cmake").write_text(
            "set(FILES\n"
            "    Source/AdapterWorkOrderPlanningService.cpp\n"
            "    Source/AdapterWorkOrderPlanningService.h\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_editor_files.cmake").write_text(
            "set(FILES\n"
            "    Source/AdapterWorkOrderPlanWidget.cpp\n"
            "    Source/AdapterWorkOrderPlanWidget.h\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_work_order_tests_files.cmake").write_text(
            "set(FILES\n    Tests/AdapterWorkOrderPlanningTests.cpp\n)\n",
            encoding="utf-8",
        )
        (code / "CMakeLists.txt").write_text(
            "taintedgrailmoddingsdk_work_order_tests_files.cmake\n",
            encoding="utf-8",
        )

        planner = """
struct AdapterWorkOrderArgument
struct AdapterWorkOrderStep
struct AdapterWorkOrderPlan
struct AdapterWorkOrderRefusal
struct AdapterWorkOrderPlanSet
class AdapterWorkOrderPlanningService
BuildPlans BuildCapabilityMatrix GroupIsSupported
row->m_status != "supported"
group.m_rows.size() != sizeof(AllCapabilities)
CollectReadySubjects CollectPermissionProof CollectRelationshipValidationProof
RecordInputEvidenceIsValid RecordPayloadIsComplete
typed recipe output lacks evidence for the exact output association, quantity, and probability
canonical plans require a resolved target record ID
no exact relationship-bound validation proof is available
"workorder.plan:" ":step:" SerializeCanonicalPlan ExecutionAllowed
AppendJsonStringArray SortSteps m_canonicalJson
m_inputEvidenceIds m_declarationEvidenceIds m_permissionEventIds
m_permissionEvidenceIds m_validationProofIds m_executionAllowed = false
AdapterCapability::Persistence AdapterCapability::Cleanup AdapterCapability::Rollback
"ingredient." "output."
"""
        (source / "AdapterWorkOrderPlanningService.h").write_text(planner, encoding="utf-8")
        (source / "AdapterWorkOrderPlanningService.cpp").write_text(
            '#include "AdapterWorkOrderPlanningServicePart1.inl"\n'
            '#include "AdapterWorkOrderPlanningServicePart2.inl"\n',
            encoding="utf-8",
        )
        (source / "AdapterWorkOrderPlanningServicePart1.inl").write_text(
            "all eleven compatibility rows canonical sorting\n",
            encoding="utf-8",
        )
        (source / "AdapterWorkOrderPlanningServicePart2.inl").write_text(
            "stable deterministic canonical JSON\n",
            encoding="utf-8",
        )

        widget = """
Tainted Grail Adapter Work-Order Plans
Read-only canonical plan preview
Execution is always prohibited
BuildPlans AdapterContractRegistry::Get()
QAbstractItemView::NoEditTriggers
tr("Canonical JSON / refusal reasons")
plan.m_canonicalJson RefusalDetails tr("prohibited")
"""
        (source / "AdapterWorkOrderPlanWidget.cpp").write_text(widget, encoding="utf-8")
        (source / "AdapterWorkOrderPlanWidget.h").write_text("widget\n", encoding="utf-8")
        (source / "TaintedGrailModdingSDKSystemComponent.cpp").write_text(
            '#include "AdapterWorkOrderPlanWidget.h"\n'
            "AdapterWorkOrderPlanViewPaneName\n"
            "RegisterViewPane<AdapterWorkOrderPlanWidget>\n"
            "UnregisterViewPane(AdapterWorkOrderPlanViewPaneName)\n"
            "TaintedGrailModdingSDK.AdapterWorkOrderPlans\n",
            encoding="utf-8",
        )

        test_text = """
AnyNonSupportedCompatibilityRowRefusesWholePlan
FullySupportedCatalogBuildsCanonicalPlanOnly
AggregateSupportCannotLeakUnreviewedSubjectsIntoSteps
RelationshipStepsBindResolvedTargetsAndProof
MissingRelationshipProofRefusesWholePlan
InvalidTypedPayloadEvidenceRefusesWholePlan
CanonicalSerializationIsDeterministic
PlanningDoesNotMutateInputs
EXPECT_EQ(result.m_generatedPlanCount, 0)
EXPECT_EQ(plan.m_steps.size(), 11)
ExecutionAllowed
"validation.relationship.relationship.vendor"
declarationCountBefore relationshipCountBefore
"""
        (tests / "AdapterWorkOrderPlanningTests.cpp").write_text(test_text, encoding="utf-8")
        for name in (
            "AdapterWorkOrderPlanningTestFixturePart1.inl",
            "AdapterWorkOrderPlanningTestFixturePart2.inl",
            "AdapterWorkOrderPlanningTestsPart1.inl",
            "AdapterWorkOrderPlanningTestsPart2.inl",
        ):
            (tests / name).write_text("fixture or test part\n", encoding="utf-8")

        (workflow / "tainted-grail-sdk-foundation.yml").write_text(
            "Test adapter work-order plan validator\n"
            "test_validate_adapter_work_order_plans.py\n"
            "Validate adapter work-order plan contract\n"
            "validate_adapter_work_order_plans.py\n",
            encoding="utf-8",
        )
        (docs / "FOA_ADAPTER_WORK_ORDER_PLANS.md").write_text(
            "canonical plans only All eleven compatibility rows stable plan identity canonical JSON "
            "input evidence permission evidence validation proof resolved relationship targets transient "
            "ExecutionAllowed No runtime execution runtime-result evidence envelope\n",
            encoding="utf-8",
        )
        (docs / "README.md").write_text(
            "FoA Adapter Work-Order Plans FOA_ADAPTER_WORK_ORDER_PLANS.md\n",
            encoding="utf-8",
        )
        (docs / "FOA_ADAPTER_CONTRACTS.md").write_text(
            "Slice 9 canonical work-order plans execution remains prohibited\n",
            encoding="utf-8",
        )
        (docs / "USER_GUIDE.md").write_text(
            "Tainted Grail Adapter Work-Order Plans generated refused canonical JSON Execution is prohibited\n",
            encoding="utf-8",
        )
        (docs / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md").write_text(
            "All ten TG SDK panes Tainted Grail Adapter Work-Order Plans "
            "one refused plan group and zero generated steps non-editable\n",
            encoding="utf-8",
        )
        (docs / "CORE_FRAMEWORK_BUILD_GRAPH.md").write_text(
            "work-order planning execution-prohibited\n",
            encoding="utf-8",
        )
        (repo / "ROADMAP.md").write_text(
            "Deterministic work-order plan generation\n"
            "Status: implemented, continuing hardening and Windows UI verification.\n"
            "Runtime-result evidence envelope\n",
            encoding="utf-8",
        )
        (repo / "CHANGELOG.md").write_text(
            "AdapterWorkOrderPlanningService Tainted Grail Adapter Work-Order Plans "
            "execution remains prohibited\n",
            encoding="utf-8",
        )
        return repo

    def test_complete_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_adapter_work_order_plans(self.make_repo(Path(temporary)))

    def test_execution_path_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            planner = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterWorkOrderPlanningServicePart2.inl"
            planner.write_text(planner.read_text() + "\nExecuteWorkOrder\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterWorkOrderPlanError, "ExecuteWorkOrder"):
                validate_adapter_work_order_plans(repo)

    def test_persistence_path_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            planner = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterWorkOrderPlanningServicePart2.inl"
            planner.write_text(planner.read_text() + "\nQSaveFile\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterWorkOrderPlanError, "QSaveFile"):
                validate_adapter_work_order_plans(repo)

    def test_mutable_widget_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            widget = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterWorkOrderPlanWidget.cpp"
            widget.write_text(widget.read_text() + "\nQPushButton\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterWorkOrderPlanError, "QPushButton"):
                validate_adapter_work_order_plans(repo)

    def test_weakened_all_capability_gate_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            header = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterWorkOrderPlanningService.h"
            header.write_text(
                header.read_text().replace('row->m_status != "supported"', 'row->m_status == "unsupported"'),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(AdapterWorkOrderPlanError, "row->m_status"):
                validate_adapter_work_order_plans(repo)

    def test_missing_canonical_sort_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            header = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterWorkOrderPlanningService.h"
            header.write_text(header.read_text().replace("SortSteps", "UnsortedSteps"), encoding="utf-8")
            with self.assertRaisesRegex(AdapterWorkOrderPlanError, "SortSteps"):
                validate_adapter_work_order_plans(repo)


if __name__ == "__main__":
    unittest.main()
