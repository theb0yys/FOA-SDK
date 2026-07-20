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

from validate_adapter_runtime_results import (  # noqa: E402
    AdapterRuntimeResultContractError,
    validate_adapter_runtime_results,
)


class AdapterRuntimeResultValidatorTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        code = repo / "Gems/TaintedGrailModdingSDK/Code"
        source = code / "Source"
        tests = code / "Tests"
        tools_tests = repo / "Gems/TaintedGrailModdingSDK/Tools/tests"
        docs = repo / "docs/tainted-grail-sdk"
        workflow = repo / ".github/workflows"
        source.mkdir(parents=True)
        tests.mkdir(parents=True)
        tools_tests.mkdir(parents=True)
        docs.mkdir(parents=True)
        workflow.mkdir(parents=True)

        (code / "taintedgrailmoddingsdk_core_files.cmake").write_text(
            "set(FILES\n"
            "    Source/AdapterRuntimeResultContracts.cpp\n"
            "    Source/AdapterRuntimeResultContracts.h\n"
            "    Source/AdapterRuntimeResultEvidenceService.cpp\n"
            "    Source/AdapterRuntimeResultEvidenceService.h\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_editor_files.cmake").write_text(
            "set(FILES\n"
            "    Source/AdapterRuntimeResultEvidenceWidget.cpp\n"
            "    Source/AdapterRuntimeResultEvidenceWidget.h\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_runtime_result_tests_files.cmake").write_text(
            "set(FILES\n    Tests/AdapterRuntimeResultEvidenceTests.cpp\n)\n",
            encoding="utf-8",
        )
        (code / "CMakeLists.txt").write_text(
            "taintedgrailmoddingsdk_runtime_result_tests_files.cmake\n",
            encoding="utf-8",
        )

        contracts = """
enum class AdapterRuntimeOutcome NotAttempted Succeeded Failed Skipped
enum class AdapterRuntimeFailureKind Persistence Cleanup Rollback
enum class AdapterRuntimeLogKind
struct AdapterRuntimeFailure struct AdapterRuntimeLogReference
struct AdapterRuntimeStepResult struct AdapterRuntimeRecoveryResult
struct AdapterRuntimeResultEnvelope class AdapterRuntimeResultRegistry RegisterEnvelope
m_attempted m_cleanupResult m_rollbackResult m_planCanonicalJson m_planFingerprint
m_resultFingerprint IsSha256Fingerprint CanonicalSha256Matches IsAdapterRuntimeLogReference safe relative locators
Failed and skipped outcomes require failures
"""
        (source / "AdapterRuntimeResultContracts.h").write_text(contracts, encoding="utf-8")
        (source / "AdapterRuntimeResultContracts.cpp").write_text(contracts, encoding="utf-8")

        service = """
struct AdapterRuntimeEvidenceReturn class AdapterRuntimeResultEvidenceService BuildEvidenceReturn
runtime_result.plan_canonical_mismatch runtime_result.step_count_mismatch
runtime_result.step_missing runtime_result.step_unknown runtime_result.cleanup_mismatch
runtime_result.rollback_mismatch RecoveryMatchesStep
FindPlanCapabilityStep(plan, "cleanup") FindPlanCapabilityStep(plan, "rollback")
SourceDocument EvidenceDocument "adapter_runtime_step_result" "adapter_runtime_failure"
const char* recoveryNames[] = { "cleanup", "rollback" }
AZStd::string("adapter_runtime_") + recoveryNames[index] + "_result"
"adapter_runtime_log_reference" "adapter_runtime_plan_binding"
m_sourceDocuments m_evidenceDocuments m_accepted = true
validation and permission remain unchanged
Referenced log content is not opened, persisted, or inspected
"""
        (source / "AdapterRuntimeResultEvidenceService.h").write_text(service, encoding="utf-8")
        (source / "AdapterRuntimeResultEvidenceService.cpp").write_text(service, encoding="utf-8")

        widget = """
Tainted Grail Adapter Runtime Result Evidence
Read-only Phase 7 contract verification
Nothing is imported, persisted, promoted, dispatched, executed, deployed, launched
QAbstractItemView::NoEditTriggers
AdapterRuntimeResultRegistry::Get().GetEnvelopes() BuildEvidenceReturn
tr("Outcome") tr("Failures") tr("Cleanup / rollback") tr("Logs and fingerprints")
tr("Evidence candidates") tr("execution: prohibited")
"""
        (source / "AdapterRuntimeResultEvidenceWidget.cpp").write_text(widget, encoding="utf-8")
        (source / "AdapterRuntimeResultEvidenceWidget.h").write_text("widget", encoding="utf-8")
        (source / "TaintedGrailModdingSDKSystemComponent.cpp").write_text(
            '#include "AdapterRuntimeResultContracts.h"\n'
            '#include "AdapterRuntimeResultEvidenceWidget.h"\n'
            "AdapterRuntimeResultEvidenceViewPaneName\n"
            "RegisterViewPane<AdapterRuntimeResultEvidenceWidget>\n"
            "UnregisterViewPane(AdapterRuntimeResultEvidenceViewPaneName)\n"
            "TaintedGrailModdingSDK.AdapterRuntimeResultEvidence\n"
            "AdapterRuntimeResultRegistry::Get().Clear()\n",
            encoding="utf-8",
        )

        test_text = """
TypedOutcomeFailureAndLogVocabulariesAreStrict
RegistryRejectsCallerSelectedAndDuplicateFingerprints
PlanFingerprintMustHashExactCanonicalJson
ExactAttemptedPlanProducesCandidateEvidenceOnly
FailedStepAndTypedFailureReturnAsNewEvidence
UnknownOrMissingStepIdentityFailsClosed
OutcomeAndFailureShapeIsValidatedBeforeEvidenceReturn
CleanupAndRollbackSummariesMustMatchTheirStepResults
ImpossibleUtcDateAndUnknownFailureKindAreRejected
CanonicalPayloadIsOrderIndependentButContentSensitive
EXPECT_TRUE(HasEvidenceKind(result, "adapter_runtime_failure"))
EXPECT_TRUE(HasIssue(result, "runtime_result.cleanup_mismatch"))
"""
        (tests / "AdapterRuntimeResultEvidenceTests.cpp").write_text(test_text, encoding="utf-8")
        (workflow / "tainted-grail-sdk-foundation.yml").write_text(
            "Test adapter runtime-result validator\n"
            "test_validate_adapter_runtime_results.py\n"
            "Validate adapter runtime-result evidence contract\n"
            "validate_adapter_runtime_results.py\n",
            encoding="utf-8",
        )

        (docs / "FOA_ADAPTER_RUNTIME_RESULT_EVIDENCE.md").write_text(
            "attempted plan attempted step not_attempted succeeded failed skipped cleanup rollback "
            "log references SHA-256 new evidence does not promote validation or permission "
            "No adapter implementation or execution path",
            encoding="utf-8",
        )
        (docs / "README.md").write_text(
            "FoA Adapter Runtime Result Evidence FOA_ADAPTER_RUNTIME_RESULT_EVIDENCE.md",
            encoding="utf-8",
        )
        (docs / "FOA_ADAPTER_WORK_ORDER_PLANS.md").write_text(
            "Slice 10 runtime-result evidence no execution path",
            encoding="utf-8",
        )
        (docs / "USER_GUIDE.md").write_text(
            "Tainted Grail Adapter Runtime Result Evidence not_attempted succeeded failed skipped "
            "candidate source and evidence documents nothing is persisted or promoted",
            encoding="utf-8",
        )
        (docs / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md").write_text(
            "All eleven TG SDK panes Tainted Grail Adapter Runtime Result Evidence "
            "zero registered runtime-result envelopes non-editable",
            encoding="utf-8",
        )
        (docs / "CORE_FRAMEWORK_BUILD_GRAPH.md").write_text(
            "runtime-result contract validation candidate source/evidence return",
            encoding="utf-8",
        )
        (docs / "DATA_FORMATS.md").write_text(
            "Adapter runtime-result envelope ContractVersion PlanFingerprint ResultFingerprint "
            "CleanupResult RollbackResult LogReferences",
            encoding="utf-8",
        )
        (repo / "ROADMAP.md").write_text(
            "Runtime-result evidence envelope\n"
            "Status: implemented, continuing hardening and Windows UI verification.\n"
            "attempted-plan and attempted-step identities\n"
            "No actual FoA adapter implementation or execution path",
            encoding="utf-8",
        )
        (repo / "CHANGELOG.md").write_text(
            "AdapterRuntimeResultRegistry AdapterRuntimeResultEvidenceService "
            "Tainted Grail Adapter Runtime Result Evidence does not promote validation or permission",
            encoding="utf-8",
        )
        return repo

    def test_complete_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_adapter_runtime_results(self.make_repo(Path(temporary)))

    def test_execution_code_in_core_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterRuntimeResultEvidenceService.cpp"
            path.write_text(path.read_text() + "\nExecuteWorkOrder\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterRuntimeResultContractError, "ExecuteWorkOrder"):
                validate_adapter_runtime_results(repo)

    def test_missing_recovery_binding_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            for name in ("AdapterRuntimeResultEvidenceService.h", "AdapterRuntimeResultEvidenceService.cpp"):
                path = repo / "Gems/TaintedGrailModdingSDK/Code/Source" / name
                path.write_text(
                    path.read_text().replace('FindPlanCapabilityStep(plan, "rollback")', "rollback omitted"),
                    encoding="utf-8",
                )
            with self.assertRaisesRegex(AdapterRuntimeResultContractError, "rollback"):
                validate_adapter_runtime_results(repo)

    def test_mutable_widget_control_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterRuntimeResultEvidenceWidget.cpp"
            path.write_text(path.read_text() + "\nQPushButton\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterRuntimeResultContractError, "QPushButton"):
                validate_adapter_runtime_results(repo)

    def test_automatic_evidence_registration_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterRuntimeResultEvidenceService.cpp"
            path.write_text(path.read_text() + "\nRegisterEvidence(\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterRuntimeResultContractError, "RegisterEvidence"):
                validate_adapter_runtime_results(repo)

    def test_missing_step_coverage_test_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Code/Tests/AdapterRuntimeResultEvidenceTests.cpp"
            path.write_text(
                path.read_text().replace("UnknownOrMissingStepIdentityFailsClosed", "missing"),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(AdapterRuntimeResultContractError, "UnknownOrMissing"):
                validate_adapter_runtime_results(repo)


if __name__ == "__main__":
    unittest.main()
