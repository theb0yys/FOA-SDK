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

from validate_adapter_staging_deployment_preview import (  # noqa: E402
    AdapterStagingDeploymentPreviewContractError,
    validate_adapter_staging_deployment_preview,
)


class AdapterStagingDeploymentPreviewValidatorTests(unittest.TestCase):
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
            "    Source/AdapterStagingDeploymentPreviewService.cpp\n"
            "    Source/AdapterStagingDeploymentPreviewService.h\n"
            "    Source/AdapterStagingDeploymentPreviewServicePart1.inl\n"
            "    Source/AdapterStagingDeploymentPreviewServicePart2.inl\n"
            "    Source/AdapterStagingDeploymentPreviewServicePart3.inl\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_editor_files.cmake").write_text(
            "set(FILES\n"
            "    Source/AdapterStagingDeploymentPreviewWidget.cpp\n"
            "    Source/AdapterStagingDeploymentPreviewWidget.h\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_deployment_preview_tests_files.cmake").write_text(
            "set(FILES\n    Tests/AdapterStagingDeploymentPreviewTests.cpp\n)\n",
            encoding="utf-8",
        )
        (code / "CMakeLists.txt").write_text(
            "taintedgrailmoddingsdk_deployment_preview_tests_files.cmake\n",
            encoding="utf-8",
        )

        service = """
enum class AdapterDeploymentTargetReviewDecision
enum class AdapterDeploymentChangeKind
enum class AdapterDeploymentRollbackAction
enum class AdapterStagingDeploymentPreviewStatus
PackageNotReady TargetUnreviewed InventoryBindingMismatch InventoryUntrusted
PathInvalid Conflict BackupIncomplete RollbackIncomplete
"add" "replace" "remove" "unchanged" "conflict"
"remove_added" "restore_replaced" "restore_removed"
struct AdapterDeploymentTargetReview struct AdapterDeploymentTargetEntry
struct AdapterDeploymentTargetInventory struct AdapterDeploymentChange
struct AdapterDeploymentConflict struct AdapterDeploymentBackupRequirement
struct AdapterDeploymentRollbackStep struct AdapterStagingDeploymentPreviewRequest
struct AdapterStagingDeploymentPreview
class AdapterStagingDeploymentPreviewRegistry RegisterRequest
class AdapterStagingDeploymentPreviewService BuildPreview SerializeCanonicalPreview
m_packagePreviewFingerprint m_inventoryFingerprint m_projectOwned m_managed
m_replaceable m_removable m_backupRequired PathIsInsideRoot
AddBackupRequirement BuildRollbackPlan
deployment.package_not_ready deployment.target_unreviewed
deployment.inventory_binding_mismatch deployment.inventory_untrusted
deployment.path_invalid deployment.conflict
deployment.backup_fingerprint_missing deployment.rollback_incomplete
m_stagingMutationAllowed = false m_deploymentMutationAllowed = false
m_rollbackExecutionAllowed = false m_launchAllowed = false
"StagingMutationAllowed" "DeploymentMutationAllowed"
"RollbackExecutionAllowed" "LaunchAllowed" std::locale::classic()
"""
        (source / "AdapterStagingDeploymentPreviewService.h").write_text(
            service, encoding="utf-8"
        )
        (source / "AdapterStagingDeploymentPreviewService.cpp").write_text(
            service, encoding="utf-8"
        )
        for index in range(1, 4):
            (source / f"AdapterStagingDeploymentPreviewServicePart{index}.inl").write_text(
                service,
                encoding="utf-8",
            )

        widget = """
Tainted Grail Staging and Deployment Preview
Read-only Phase 8 comparison
StagingMutationAllowed, DeploymentMutationAllowed
Nothing is copied, replaced, removed
QAbstractItemView::NoEditTriggers
AdapterStagingDeploymentPreviewRegistry::Get().GetRequests()
BuildPreview
tr("Additions") tr("Replacements") tr("Removals / unchanged")
tr("Conflicts") tr("Backup requirements") tr("Rollback steps")
filesystem mutation and launch: prohibited
"""
        (source / "AdapterStagingDeploymentPreviewWidget.cpp").write_text(
            widget, encoding="utf-8"
        )
        (source / "AdapterStagingDeploymentPreviewWidget.h").write_text(
            "widget", encoding="utf-8"
        )
        (source / "TaintedGrailModdingSDKSystemComponent.cpp").write_text(
            '#include "AdapterStagingDeploymentPreviewService.h"\n'
            '#include "AdapterStagingDeploymentPreviewWidget.h"\n'
            "AdapterStagingDeploymentPreviewViewPaneName\n"
            "RegisterViewPane<AdapterStagingDeploymentPreviewWidget>\n"
            "UnregisterViewPane(AdapterStagingDeploymentPreviewViewPaneName)\n"
            "TaintedGrailModdingSDK.StagingDeploymentPreview\n"
            "AdapterStagingDeploymentPreviewRegistry::Get().Clear()\n",
            encoding="utf-8",
        )

        test_text = """
TypedStatusChangeAndRollbackVocabulariesAreStrict
RegistryRejectsDuplicateTargetInventoryIdentity
ReadyPreviewDerivesAllChangeKindsAndProhibitsMutation
PackageNotReadyPrecedesTargetAndInventoryFailures
ReviewAndInventoryBindingsFailClosed
ForeignOwnershipAndDuplicateTargetsProduceConflicts
MissingCurrentDigestMakesBackupAndRollbackIncomplete
UnsafeTargetOrBackupPathIsRejected
RollbackStepsAreExactDeterministicInverses
CanonicalChangesBackupsAndRollbackAreDeterministic
PreviewGenerationDoesNotMutateInputs
EXPECT_FALSE(preview.m_stagingMutationAllowed)
"DeploymentMutationAllowed\\":false"
packageEntryCount targetEntryCount
"""
        (tests / "AdapterStagingDeploymentPreviewTests.cpp").write_text(
            test_text,
            encoding="utf-8",
        )

        (workflow / "tainted-grail-sdk-foundation.yml").write_text(
            "Test staging/deployment preview validator\n"
            "test_validate_adapter_staging_deployment_preview.py\n"
            "Validate staging/deployment preview contract\n"
            "validate_adapter_staging_deployment_preview.py\n",
            encoding="utf-8",
        )
        (docs / "FOA_ADAPTER_STAGING_DEPLOYMENT_PREVIEW.md").write_text(
            "ready package layout declared target inventory additions replacements removals "
            "conflicts backup requirements rollback steps StagingMutationAllowed "
            "DeploymentMutationAllowed RollbackExecutionAllowed "
            "No copying, deletion, deployment, launch, or execution",
            encoding="utf-8",
        )
        (docs / "README.md").write_text(
            "FoA Adapter Staging and Deployment Preview "
            "FOA_ADAPTER_STAGING_DEPLOYMENT_PREVIEW.md",
            encoding="utf-8",
        )
        (docs / "FOA_ADAPTER_PACKAGE_ASSEMBLY_PREVIEW.md").write_text(
            "Slice 13 staging and deployment preview no file mutation",
            encoding="utf-8",
        )
        (docs / "USER_GUIDE.md").write_text(
            "Tainted Grail Staging and Deployment Preview backup_incomplete "
            "rollback_incomplete additions, replacements, removals, and unchanged paths "
            "nothing is copied or deleted",
            encoding="utf-8",
        )
        (docs / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md").write_text(
            "All fourteen TG SDK panes Tainted Grail Staging and Deployment Preview "
            "zero registered staging/deployment-preview inputs non-editable",
            encoding="utf-8",
        )
        (docs / "CORE_FRAMEWORK_BUILD_GRAPH.md").write_text(
            "staging/deployment preview derivation DeploymentMutationAllowed=false",
            encoding="utf-8",
        )
        (docs / "RELEASE_PROCESS.md").write_text(
            "additions, replacements, removals, and conflicts backup and rollback preview "
            "does not mutate deployment",
            encoding="utf-8",
        )
        (repo / "ROADMAP.md").write_text(
            "Deterministic staging and deployment preview\n"
            "Status: implemented, continuing hardening and Windows UI verification.\n"
            "additions, replacements, removals, conflicts, backups, and rollback\n"
            "No copying, deletion, deployment, launch, or execution",
            encoding="utf-8",
        )
        (repo / "CHANGELOG.md").write_text(
            "AdapterStagingDeploymentPreviewService "
            "Tainted Grail Staging and Deployment Preview DeploymentMutationAllowed",
            encoding="utf-8",
        )
        return repo

    def test_complete_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_adapter_staging_deployment_preview(self.make_repo(Path(temporary)))

    def test_filesystem_mutation_code_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterStagingDeploymentPreviewService.cpp"
            )
            path.write_text(path.read_text() + "\ncopy_file\n", encoding="utf-8")
            with self.assertRaisesRegex(
                AdapterStagingDeploymentPreviewContractError, "copy_file"
            ):
                validate_adapter_staging_deployment_preview(repo)

    def test_missing_backup_gate_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            for name in (
                "AdapterStagingDeploymentPreviewService.h",
                "AdapterStagingDeploymentPreviewService.cpp",
                "AdapterStagingDeploymentPreviewServicePart1.inl",
                "AdapterStagingDeploymentPreviewServicePart2.inl",
                "AdapterStagingDeploymentPreviewServicePart3.inl",
            ):
                path = repo / "Gems/TaintedGrailModdingSDK/Code/Source" / name
                path.write_text(
                    path.read_text().replace("AddBackupRequirement", "backup gate omitted"),
                    encoding="utf-8",
                )
            with self.assertRaisesRegex(
                AdapterStagingDeploymentPreviewContractError, "AddBackupRequirement"
            ):
                validate_adapter_staging_deployment_preview(repo)

    def test_mutable_widget_control_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterStagingDeploymentPreviewWidget.cpp"
            )
            path.write_text(path.read_text() + "\nQPushButton\n", encoding="utf-8")
            with self.assertRaisesRegex(
                AdapterStagingDeploymentPreviewContractError, "QPushButton"
            ):
                validate_adapter_staging_deployment_preview(repo)

    def test_missing_rollback_test_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = (
                repo
                / "Gems/TaintedGrailModdingSDK/Code/Tests/AdapterStagingDeploymentPreviewTests.cpp"
            )
            path.write_text(
                path.read_text().replace(
                    "RollbackStepsAreExactDeterministicInverses", "rollback test omitted"
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                AdapterStagingDeploymentPreviewContractError,
                "RollbackStepsAreExactDeterministicInverses",
            ):
                validate_adapter_staging_deployment_preview(repo)

    def test_missing_fourteen_pane_contract_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md"
            path.write_text("All thirteen TG SDK panes", encoding="utf-8")
            with self.assertRaisesRegex(
                AdapterStagingDeploymentPreviewContractError, "fourteen"
            ):
                validate_adapter_staging_deployment_preview(repo)


if __name__ == "__main__":
    unittest.main()
