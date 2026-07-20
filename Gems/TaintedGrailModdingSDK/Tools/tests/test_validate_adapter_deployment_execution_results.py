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

from validate_adapter_deployment_execution_results import (  # noqa: E402
    AdapterDeploymentExecutionResultContractError,
    validate_adapter_deployment_execution_results,
)


class AdapterDeploymentExecutionResultValidatorTests(unittest.TestCase):
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
            "    Source/AdapterDeploymentExecutionResultContracts.cpp\n"
            "    Source/AdapterDeploymentExecutionResultContracts.h\n"
            "    Source/AdapterDeploymentExecutionEvidenceService.cpp\n"
            "    Source/AdapterDeploymentExecutionEvidenceService.h\n"
            "    Source/AdapterDeploymentExecutionEvidenceServicePart1.inl\n"
            "    Source/AdapterDeploymentExecutionEvidenceServicePart2.inl\n"
            "    Source/AdapterDeploymentExecutionEvidenceServicePart3.inl\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_editor_files.cmake").write_text(
            "set(FILES\n"
            "    Source/AdapterDeploymentExecutionEvidenceWidget.cpp\n"
            "    Source/AdapterDeploymentExecutionEvidenceWidget.h\n"
            ")\n",
            encoding="utf-8",
        )
        (
            code
            / "taintedgrailmoddingsdk_deployment_execution_result_tests_files.cmake"
        ).write_text(
            "set(FILES\n"
            "    Tests/AdapterDeploymentExecutionResultTests.cpp\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "CMakeLists.txt").write_text(
            "taintedgrailmoddingsdk_deployment_execution_result_tests_files.cmake\n",
            encoding="utf-8",
        )

        service = """
enum class AdapterDeploymentExecutorReviewDecision
enum class AdapterDeploymentExecutionOutcome
enum class AdapterDeploymentVerificationStatus
enum class AdapterDeploymentExecutionFailureKind
enum class AdapterDeploymentExecutionLogKind
enum class AdapterDeploymentRollbackAction
enum class AdapterDeploymentExecutionEnvelopeStatus
"not_attempted" "succeeded" "failed" "skipped"
"not_checked" "matched" "mismatched"
"remove_added" "restore_replaced" "restore_removed"
"work_order_not_ready" "executor_unreviewed" "work_order_binding_mismatch"
"envelope_invalid" "step_binding_mismatch" "backup_binding_mismatch"
"verification_binding_mismatch" "rollback_binding_mismatch"
"failure_log_binding_mismatch" "accepted"
struct AdapterDeploymentExecutorReview
struct AdapterDeploymentExecutionStepResult
struct AdapterDeploymentBackupResult
struct AdapterDeploymentTargetVerification
struct AdapterDeploymentRollbackResult
struct AdapterDeploymentExecutionFailure
struct AdapterDeploymentExecutionLogReference
struct AdapterDeploymentExecutionResultEnvelope
class AdapterDeploymentExecutionResultRegistry
class AdapterDeploymentExecutionEvidenceService
BuildEvidenceReturn
m_workOrderCanonicalJson m_workOrderFingerprint m_executorFingerprint
m_backupFingerprint m_observedFingerprint m_restoreFingerprint m_finalFingerprint
ValidateWorkOrderReadiness ValidateExecutorReview
ValidateEnvelopeAndWorkOrderBinding ValidateStepBindings
ValidateBackupBindings ValidateVerificationBindings ValidateRollbackBindings
ValidateFailureAndLogBindings BuildPrimaryEvidence BuildLogEvidence
m_sourceDocuments m_evidenceDocuments m_accepted = false
contract_validated
Nothing is executed
"""
        for name in (
            "AdapterDeploymentExecutionResultContracts.h",
            "AdapterDeploymentExecutionResultContracts.cpp",
            "AdapterDeploymentExecutionEvidenceService.h",
            "AdapterDeploymentExecutionEvidenceService.cpp",
            "AdapterDeploymentExecutionEvidenceServicePart1.inl",
            "AdapterDeploymentExecutionEvidenceServicePart2.inl",
            "AdapterDeploymentExecutionEvidenceServicePart3.inl",
        ):
            (source / name).write_text(service, encoding="utf-8")

        widget = """
Tainted Grail Deployment Execution Result Evidence
Read-only Phase 8 contract verification
separately reviewed executor
candidate source/evidence documents
Nothing is executed
QAbstractItemView::NoEditTriggers
AdapterDeploymentExecutionResultRegistry::Get()
BuildEvidenceReturn
tr("Backup outcome")
tr("Target verification")
tr("Rollback / restore outcome")
tr("Evidence candidates")
Automatic evidence promotion: prohibited
"""
        (source / "AdapterDeploymentExecutionEvidenceWidget.cpp").write_text(
            widget,
            encoding="utf-8",
        )
        (source / "AdapterDeploymentExecutionEvidenceWidget.h").write_text(
            "widget",
            encoding="utf-8",
        )
        (source / "TaintedGrailModdingSDKSystemComponent.cpp").write_text(
            '#include "AdapterDeploymentExecutionResultContracts.h"\n'
            '#include "AdapterDeploymentExecutionEvidenceWidget.h"\n'
            "AdapterDeploymentExecutionEvidenceViewPaneName\n"
            "RegisterViewPane<AdapterDeploymentExecutionEvidenceWidget>\n"
            "UnregisterViewPane(AdapterDeploymentExecutionEvidenceViewPaneName)\n"
            "TaintedGrailModdingSDK.DeploymentExecutionEvidence\n"
            "AdapterDeploymentExecutionResultRegistry::Get().Clear()\n",
            encoding="utf-8",
        )

        test_text = """
TypedVocabulariesReferencesAndDatesAreStrict
UnboundRegistrationIsProhibited
BoundRegistryRejectsDuplicateResultIdentity
CompleteEnvelopeReturnsCandidateEvidenceOnly
CallerSelectedWorkOrderAndResultFingerprintsFailClosed
MissingStepBackupVerificationAndRollbackFailClosed
UnknownFailureKindAndImpossibleCaptureDateAreRejectedBeforeStorage
CanonicalPayloadIsOrderIndependentAndContentSensitive
EXPECT_TRUE(result.m_accepted)
m_stepResultCount m_backupResultCount m_verificationCount m_rollbackResultCount
m_sourceDocuments.empty m_evidenceDocuments.empty
"""
        (tests / "AdapterDeploymentExecutionResultTests.cpp").write_text(
            test_text,
            encoding="utf-8",
        )

        (workflow / "tainted-grail-sdk-foundation.yml").write_text(
            "Test deployment execution-result validator\n"
            "test_validate_adapter_deployment_execution_results.py\n"
            "Validate deployment execution-result evidence contract\n"
            "validate_adapter_deployment_execution_results.py\n",
            encoding="utf-8",
        )

        (
            docs / "FOA_ADAPTER_DEPLOYMENT_EXECUTION_RESULTS.md"
        ).write_text(
            "Exact reviewed deployment work order separately reviewed executor "
            "attempted step identities backup and restore outcomes deployed fingerprints "
            "rollback results safe log references candidate evidence "
            "No executor, deployment command, launch path, adapter call, or automatic evidence promotion",
            encoding="utf-8",
        )
        (docs / "README.md").write_text(
            "FoA Adapter Deployment Execution Results "
            "FOA_ADAPTER_DEPLOYMENT_EXECUTION_RESULTS.md",
            encoding="utf-8",
        )
        (
            docs / "FOA_ADAPTER_DEPLOYMENT_CONFIRMATION_WORK_ORDERS.md"
        ).write_text(
            "Slice 15 deployment execution-result "
            "no executor or automatic evidence promotion",
            encoding="utf-8",
        )
        (repo / "ROADMAP.md").write_text(
            "Typed deployment execution-result and verification envelope\n"
            "Status: implemented, continuing hardening and Windows UI verification.\n"
            "backup/restore outcomes, target verification, rollback, failures, and logs\n"
            "No executor, deployment command, launch path, adapter call, or automatic evidence promotion",
            encoding="utf-8",
        )
        (repo / "CHANGELOG.md").write_text(
            "AdapterDeploymentExecutionEvidenceService "
            "Tainted Grail Deployment Execution Result Evidence "
            "automatic evidence promotion",
            encoding="utf-8",
        )
        (docs / "USER_GUIDE.md").write_text(
            "Tainted Grail Deployment Execution Result Evidence "
            "work_order_not_ready verification_binding_mismatch "
            "failed execution can still be contract-valid evidence "
            "nothing is executed or promoted automatically",
            encoding="utf-8",
        )
        (docs / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md").write_text(
            "All sixteen TG SDK panes "
            "Tainted Grail Deployment Execution Result Evidence "
            "zero registered deployment execution-result envelopes non-editable",
            encoding="utf-8",
        )
        (docs / "CORE_FRAMEWORK_BUILD_GRAPH.md").write_text(
            "deployment execution-result verification and candidate evidence return "
            "no executor or automatic evidence promotion",
            encoding="utf-8",
        )
        (docs / "RELEASE_PROCESS.md").write_text(
            "deployment execution-result envelope "
            "backup, restore, target-verification, rollback, and log evidence "
            "candidate evidence remains unpromoted",
            encoding="utf-8",
        )
        return repo

    def test_complete_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_adapter_deployment_execution_results(
                self.make_repo(Path(temporary))
            )

    def test_executor_or_file_mutation_code_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "Gems/TaintedGrailModdingSDK/Code/Source"
                / "AdapterDeploymentExecutionEvidenceService.cpp"
            )
            path.write_text(path.read_text() + "\nExecuteAdapter\n", encoding="utf-8")
            with self.assertRaisesRegex(
                AdapterDeploymentExecutionResultContractError,
                "ExecuteAdapter",
            ):
                validate_adapter_deployment_execution_results(repo)

    def test_automatic_evidence_promotion_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "Gems/TaintedGrailModdingSDK/Code/Source"
                / "AdapterDeploymentExecutionEvidenceServicePart3.inl"
            )
            path.write_text(
                path.read_text() + "\nApplyCatalogValidationDecision\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                AdapterDeploymentExecutionResultContractError,
                "ApplyCatalogValidationDecision",
            ):
                validate_adapter_deployment_execution_results(repo)

    def test_missing_rollback_binding_gate_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            for name in (
                "AdapterDeploymentExecutionResultContracts.h",
                "AdapterDeploymentExecutionResultContracts.cpp",
                "AdapterDeploymentExecutionEvidenceService.h",
                "AdapterDeploymentExecutionEvidenceService.cpp",
                "AdapterDeploymentExecutionEvidenceServicePart1.inl",
                "AdapterDeploymentExecutionEvidenceServicePart2.inl",
                "AdapterDeploymentExecutionEvidenceServicePart3.inl",
            ):
                path = repo / "Gems/TaintedGrailModdingSDK/Code/Source" / name
                path.write_text(
                    path.read_text().replace(
                        "ValidateRollbackBindings",
                        "rollback validation omitted",
                    ),
                    encoding="utf-8",
                )
            with self.assertRaisesRegex(
                AdapterDeploymentExecutionResultContractError,
                "ValidateRollbackBindings",
            ):
                validate_adapter_deployment_execution_results(repo)

    def test_mutable_widget_control_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "Gems/TaintedGrailModdingSDK/Code/Source"
                / "AdapterDeploymentExecutionEvidenceWidget.cpp"
            )
            path.write_text(path.read_text() + "\nQPushButton\n", encoding="utf-8")
            with self.assertRaisesRegex(
                AdapterDeploymentExecutionResultContractError,
                "QPushButton",
            ):
                validate_adapter_deployment_execution_results(repo)

    def test_missing_exact_step_test_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "Gems/TaintedGrailModdingSDK/Code/Tests"
                / "AdapterDeploymentExecutionResultTests.cpp"
            )
            path.write_text(
                path.read_text().replace(
                    "CompleteEnvelopeReturnsCandidateEvidenceOnly",
                    "step test missing",
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                AdapterDeploymentExecutionResultContractError,
                "CompleteEnvelopeReturnsCandidateEvidenceOnly",
            ):
                validate_adapter_deployment_execution_results(repo)

    def test_missing_sixteen_pane_contract_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "docs/tainted-grail-sdk"
                / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md"
            )
            path.write_text("All fifteen TG SDK panes", encoding="utf-8")
            with self.assertRaisesRegex(
                AdapterDeploymentExecutionResultContractError,
                "sixteen",
            ):
                validate_adapter_deployment_execution_results(repo)


if __name__ == "__main__":
    unittest.main()
