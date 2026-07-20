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

from validate_adapter_deployment_work_orders import (  # noqa: E402
    AdapterDeploymentWorkOrderContractError,
    validate_adapter_deployment_work_orders,
)


class AdapterDeploymentWorkOrderValidatorTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        code = repo / "Gems/TaintedGrailModdingSDK/Code"
        source = code / "Source"
        tests = code / "Tests"
        docs = repo / "docs/tainted-grail-sdk"
        workflow = repo / ".github/workflows"
        source.mkdir(parents=True)
        tests.mkdir(parents=True)
        docs.mkdir(parents=True)
        workflow.mkdir(parents=True)

        (code / "taintedgrailmoddingsdk_core_files.cmake").write_text(
            "set(FILES\n"
            "    Source/AdapterDeploymentWorkOrderService.cpp\n"
            "    Source/AdapterDeploymentWorkOrderService.h\n"
            "    Source/AdapterDeploymentWorkOrderServicePart1.inl\n"
            "    Source/AdapterDeploymentWorkOrderServicePart2.inl\n"
            "    Source/AdapterDeploymentWorkOrderServicePart3.inl\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_editor_files.cmake").write_text(
            "set(FILES\n"
            "    Source/AdapterDeploymentWorkOrderWidget.cpp\n"
            "    Source/AdapterDeploymentWorkOrderWidget.h\n"
            ")\n",
            encoding="utf-8",
        )
        (
            code / "taintedgrailmoddingsdk_deployment_work_order_tests_files.cmake"
        ).write_text(
            "set(FILES\n"
            "    Tests/AdapterDeploymentWorkOrderTests.cpp\n"
            "    Tests/AdapterDeploymentPipelineHandoffTests.cpp\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "CMakeLists.txt").write_text(
            "taintedgrailmoddingsdk_deployment_work_order_tests_files.cmake\n",
            encoding="utf-8",
        )

        service = """
enum class AdapterDeploymentConfirmationDecision Unknown Confirmed Rejected
enum class AdapterDeploymentConfirmationScope AdditionsOnly AdditionsAndReplacements FullPreview
enum class AdapterDeploymentPreflightKind PackageIntegrity TargetInventory
BackupReadiness RollbackReadiness OperatorReadiness
enum class AdapterDeploymentPreflightStatus Unknown Passed Failed
enum class AdapterDeploymentWorkOrderStepKind VerifyPreflight ConfirmMaintenanceWindow
Backup Add Replace Remove VerifyDeployment PreserveRollback
enum class AdapterDeploymentChecklistState ContractSatisfied OperatorActionRequired Blocked
enum class AdapterDeploymentWorkOrderStatus PreviewNotReady ConfirmationMissing
ConfirmationRejected ConfirmationBindingMismatch ScopeMismatch ConfirmationExpired
MaintenanceWindowInvalid OutsideMaintenanceWindow PreflightMissing PreflightFailed
WorkOrderIncomplete ReviewReady
"additions_only" "additions_and_replacements" "full_preview"
"package_integrity" "target_inventory" "backup_readiness"
"rollback_readiness" "operator_readiness"
struct AdapterDeploymentConfirmation
struct AdapterDeploymentMaintenanceWindow
struct AdapterDeploymentPreflightEvidence
struct AdapterDeploymentWorkOrderStep
struct AdapterDeploymentOperatorChecklistItem
struct AdapterDeploymentWorkOrderRequest
struct AdapterDeploymentWorkOrder
class AdapterDeploymentWorkOrderRegistry RegisterRequest
class AdapterDeploymentWorkOrderService BuildWorkOrder SerializeCanonicalWorkOrder
ScopeCoversPreview IsUtcTimestamp HasExactWorkOrderCoverage
m_previewFingerprint m_issuedAtUtc m_expiresAtUtc m_startAtUtc m_endAtUtc m_operatorGroup
m_acknowledgementRecorded = false
m_executionAllowed = false m_copyAllowed = false m_deleteAllowed = false
m_backupAllowed = false m_restoreAllowed = false m_deploymentAllowed = false
m_launchAllowed = false
"ExecutionAllowed" "CopyAllowed" "DeleteAllowed" "BackupAllowed"
"RestoreAllowed" "DeploymentAllowed" "LaunchAllowed"
std::locale::classic()
"""
        for name in (
            "AdapterDeploymentWorkOrderService.h",
            "AdapterDeploymentWorkOrderService.cpp",
            "AdapterDeploymentWorkOrderServicePart1.inl",
            "AdapterDeploymentWorkOrderServicePart2.inl",
            "AdapterDeploymentWorkOrderServicePart3.inl",
        ):
            (source / name).write_text(service, encoding="utf-8")

        widget = """
Tainted Grail Deployment Confirmation and Work Orders
Read-only Phase 8 explicit-confirmation and work-order contract
ReviewReady never authorizes execution
Nothing is copied, deleted, backed up, restored, deployed, launched, or executed
QAbstractItemView::NoEditTriggers
AdapterDeploymentWorkOrderRegistry::Get().GetRequests()
BuildWorkOrder
tr("Confirmation") tr("Maintenance window") tr("Typed work-order steps")
tr("Operator checklist") tr("Execution permissions")
execution, copy, delete, backup, restore, deployment, and launch: prohibited
"""
        (source / "AdapterDeploymentWorkOrderWidget.cpp").write_text(
            widget,
            encoding="utf-8",
        )
        (source / "AdapterDeploymentWorkOrderWidget.h").write_text(
            "widget",
            encoding="utf-8",
        )
        (source / "TaintedGrailModdingSDKSystemComponent.cpp").write_text(
            '#include "AdapterDeploymentWorkOrderService.h"\n'
            '#include "AdapterDeploymentWorkOrderWidget.h"\n'
            "AdapterDeploymentWorkOrderViewPaneName\n"
            "RegisterViewPane<AdapterDeploymentWorkOrderWidget>\n"
            "UnregisterViewPane(AdapterDeploymentWorkOrderViewPaneName)\n"
            "TaintedGrailModdingSDK.DeploymentWorkOrders\n"
            "AdapterDeploymentWorkOrderRegistry::Get().Clear()\n",
            encoding="utf-8",
        )

        test_text = """
TypedConfirmationScopePreflightAndStatusVocabulariesAreStrict
RegistryRejectsDuplicateConfirmationOrPreviewIdentity
CompleteConfirmationProducesReviewReadyWorkOrderOnly
PreviewNotReadyPrecedesConfirmationAndPreflightFailures
ConfirmationDecisionBindingAndScopeFailClosed
ExpiryAndMaintenanceWindowFailClosed
MissingAndFailedPreflightRemainDistinct
WorkOrderStepsCoverChangesBackupsAndRollback
OperatorChecklistRemainsPendingAndNonExecutable
CanonicalWorkOrderIsDeterministic
WorkOrderGenerationDoesNotMutateInputs
EXPECT_FALSE(workOrder.m_executionAllowed)
"ExecutionAllowed\\\":false"
additionCount preflightCount
"""
        (tests / "AdapterDeploymentWorkOrderTests.cpp").write_text(
            test_text,
            encoding="utf-8",
        )

        (workflow / "tainted-grail-sdk-foundation.yml").write_text(
            "Test deployment confirmation/work-order validator\n"
            "test_validate_adapter_deployment_work_orders.py\n"
            "Validate deployment confirmation/work-order contract\n"
            "validate_adapter_deployment_work_orders.py\n",
            encoding="utf-8",
        )

        (
            docs / "FOA_ADAPTER_DEPLOYMENT_CONFIRMATION_WORK_ORDERS.md"
        ).write_text(
            "exact ready staging/deployment preview named reviewer confirmation scope "
            "expiry maintenance window preflight evidence operator-facing checklist "
            "review_ready ExecutionAllowed "
            "No copy, delete, backup, restore, deployment, launch, or adapter call",
            encoding="utf-8",
        )
        (docs / "README.md").write_text(
            "FoA Adapter Deployment Confirmation and Work Orders "
            "FOA_ADAPTER_DEPLOYMENT_CONFIRMATION_WORK_ORDERS.md",
            encoding="utf-8",
        )
        (docs / "FOA_ADAPTER_STAGING_DEPLOYMENT_PREVIEW.md").write_text(
            "Slice 14 explicit confirmation deployment work-order "
            "no execution authority",
            encoding="utf-8",
        )
        (docs / "USER_GUIDE.md").write_text(
            "Tainted Grail Deployment Confirmation and Work Orders "
            "confirmation_expired outside_maintenance_window preflight_missing "
            "review_ready nothing is copied, deleted, backed up, restored, "
            "deployed, or launched",
            encoding="utf-8",
        )
        (docs / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md").write_text(
            "All fifteen TG SDK panes "
            "Tainted Grail Deployment Confirmation and Work Orders "
            "zero registered deployment confirmation/work-order inputs non-editable",
            encoding="utf-8",
        )
        (docs / "CORE_FRAMEWORK_BUILD_GRAPH.md").write_text(
            "deployment confirmation/work-order derivation ExecutionAllowed=false",
            encoding="utf-8",
        )
        (docs / "RELEASE_PROCESS.md").write_text(
            "explicit deployment confirmation maintenance window preflight evidence "
            "operator checklist execution remains separately prohibited",
            encoding="utf-8",
        )
        (repo / "ROADMAP.md").write_text(
            "Typed deployment confirmation and work-order contract\n"
            "Status: implemented, continuing hardening and Windows UI verification.\n"
            "confirmation scope, expiry, maintenance window, preflight evidence, "
            "and operator checklist\n"
            "No copy, delete, backup, restore, deployment, launch, or adapter call",
            encoding="utf-8",
        )
        (repo / "CHANGELOG.md").write_text(
            "AdapterDeploymentWorkOrderService "
            "Tainted Grail Deployment Confirmation and Work Orders "
            "ExecutionAllowed",
            encoding="utf-8",
        )
        return repo

    def test_complete_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_adapter_deployment_work_orders(
                self.make_repo(Path(temporary))
            )

    def test_runtime_or_file_mutation_code_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "Gems/TaintedGrailModdingSDK/Code/Source"
                / "AdapterDeploymentWorkOrderService.cpp"
            )
            path.write_text(
                path.read_text() + "\nCopyFile\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                AdapterDeploymentWorkOrderContractError,
                "CopyFile",
            ):
                validate_adapter_deployment_work_orders(repo)

    def test_missing_expiry_gate_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            for name in (
                "AdapterDeploymentWorkOrderService.h",
                "AdapterDeploymentWorkOrderService.cpp",
                "AdapterDeploymentWorkOrderServicePart1.inl",
                "AdapterDeploymentWorkOrderServicePart2.inl",
                "AdapterDeploymentWorkOrderServicePart3.inl",
            ):
                path = (
                    repo
                    / "Gems/TaintedGrailModdingSDK/Code/Source"
                    / name
                )
                path.write_text(
                    path.read_text().replace(
                        "m_expiresAtUtc",
                        "expiry omitted",
                    ),
                    encoding="utf-8",
                )
            with self.assertRaisesRegex(
                AdapterDeploymentWorkOrderContractError,
                "m_expiresAtUtc",
            ):
                validate_adapter_deployment_work_orders(repo)

    def test_mutable_widget_control_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "Gems/TaintedGrailModdingSDK/Code/Source"
                / "AdapterDeploymentWorkOrderWidget.cpp"
            )
            path.write_text(
                path.read_text() + "\nQPushButton\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                AdapterDeploymentWorkOrderContractError,
                "QPushButton",
            ):
                validate_adapter_deployment_work_orders(repo)

    def test_missing_scope_test_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "Gems/TaintedGrailModdingSDK/Code/Tests"
                / "AdapterDeploymentWorkOrderTests.cpp"
            )
            path.write_text(
                path.read_text().replace(
                    "ConfirmationDecisionBindingAndScopeFailClosed",
                    "scope test missing",
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                AdapterDeploymentWorkOrderContractError,
                "Scope",
            ):
                validate_adapter_deployment_work_orders(repo)

    def test_missing_fifteen_pane_contract_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "docs/tainted-grail-sdk"
                / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md"
            )
            path.write_text(
                "All fourteen TG SDK panes",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                AdapterDeploymentWorkOrderContractError,
                "fifteen",
            ):
                validate_adapter_deployment_work_orders(repo)


if __name__ == "__main__":
    unittest.main()
