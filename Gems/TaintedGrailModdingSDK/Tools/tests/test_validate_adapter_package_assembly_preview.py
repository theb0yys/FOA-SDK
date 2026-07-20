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

from validate_adapter_package_assembly_preview import (  # noqa: E402
    AdapterPackageAssemblyPreviewContractError,
    validate_adapter_package_assembly_preview,
)


class AdapterPackageAssemblyPreviewValidatorTests(unittest.TestCase):
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
            "    Source/AdapterPackageAssemblyPreviewService.cpp\n"
            "    Source/AdapterPackageAssemblyPreviewService.h\n"
            "    Source/AdapterPackageAssemblyPreviewServicePart1.inl\n"
            "    Source/AdapterPackageAssemblyPreviewServicePart2.inl\n"
            "    Source/AdapterPackageAssemblyPreviewServicePart3.inl\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_editor_files.cmake").write_text(
            "set(FILES\n"
            "    Source/AdapterPackageAssemblyPreviewWidget.cpp\n"
            "    Source/AdapterPackageAssemblyPreviewWidget.h\n"
            ")\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_package_preview_tests_files.cmake").write_text(
            "set(FILES\n    Tests/AdapterPackageAssemblyPreviewTests.cpp\n)\n",
            encoding="utf-8",
        )
        (code / "CMakeLists.txt").write_text(
            "taintedgrailmoddingsdk_package_preview_tests_files.cmake\n",
            encoding="utf-8",
        )

        service = """
enum class AdapterBuildManifestReviewDecision Accepted Rejected
enum class AdapterPackageAssemblyPreviewStatus ManifestNotReady ManifestUnreviewed
InventoryBindingMismatch InventoryUntrusted OutputMissing FingerprintMissing PathInvalid
Collision RedistributionBlocked
struct AdapterBuildManifestReview struct AdapterStagingInventoryEntry
struct AdapterStagingInventory struct AdapterPackageLayoutEntry
struct AdapterPackageAssemblyOmission struct AdapterPackageAssemblyCollision
struct AdapterPackageAssemblyBlocker
class AdapterPackageAssemblyPreviewRegistry RegisterRequest
class AdapterPackageAssemblyPreviewService BuildPreview SerializeCanonicalPreview
m_projectOwned m_outputDigest m_manifestFingerprint PathIsInsideRoot
package_preview.output_missing package_preview.output_collision
package_preview.redistribution_blocked package_preview.project_ownership_required
m_assemblyAllowed = false m_archiveAllowed = false m_deploymentAllowed = false
"AssemblyAllowed" "ArchiveAllowed" "DeploymentAllowed" std::locale::classic()
"""
        (source / "AdapterPackageAssemblyPreviewService.h").write_text(service, encoding="utf-8")
        (source / "AdapterPackageAssemblyPreviewService.cpp").write_text(service, encoding="utf-8")
        for index in range(1, 4):
            (source / f"AdapterPackageAssemblyPreviewServicePart{index}.inl").write_text(
                service,
                encoding="utf-8",
            )

        widget = """
Tainted Grail Package Assembly Preview
Read-only Phase 8 package-layout research
AssemblyAllowed, ArchiveAllowed, and DeploymentAllowed are always false
Nothing is copied, archived, deployed, launched, or executed
QAbstractItemView::NoEditTriggers
AdapterPackageAssemblyPreviewRegistry::Get().GetRequests() BuildPreview
tr("Derived layout and output digests") tr("Omissions") tr("Collisions")
tr("Redistribution and trust blockers") assembly/archive/deployment: prohibited
"""
        (source / "AdapterPackageAssemblyPreviewWidget.cpp").write_text(widget, encoding="utf-8")
        (source / "AdapterPackageAssemblyPreviewWidget.h").write_text("widget", encoding="utf-8")
        (source / "TaintedGrailModdingSDKSystemComponent.cpp").write_text(
            '#include "AdapterPackageAssemblyPreviewService.h"\n'
            '#include "AdapterPackageAssemblyPreviewWidget.h"\n'
            "AdapterPackageAssemblyPreviewViewPaneName\n"
            "RegisterViewPane<AdapterPackageAssemblyPreviewWidget>\n"
            "UnregisterViewPane(AdapterPackageAssemblyPreviewViewPaneName)\n"
            "TaintedGrailModdingSDK.PackageAssemblyPreview\n"
            "AdapterPackageAssemblyPreviewRegistry::Get().Clear()\n",
            encoding="utf-8",
        )

        test_text = """
TypedStatusAndReviewVocabulariesAreStrict
RegistryRejectsDuplicateInventoryIdentities
ReviewedCompleteInventoryProducesReadyPreviewOnly
ManifestNotReadyPrecedesInventoryFailures
ReviewAndInventoryBindingsFailClosed
MissingOutputProducesExplicitOmission
MissingOutputDigestIsDistinct
UnsafePathAndTargetCollisionAreRejected
OwnershipAndRedistributionFailClosed
CanonicalLayoutAndDigestsAreDeterministic
PreviewGenerationDoesNotMutateInputs
EXPECT_FALSE(preview.m_assemblyAllowed)
"AssemblyAllowed\\\":false"
expectedOutputCount inventoryCount
"""
        (tests / "AdapterPackageAssemblyPreviewTests.cpp").write_text(
            test_text,
            encoding="utf-8",
        )

        (workflow / "tainted-grail-sdk-foundation.yml").write_text(
            "Test package-assembly preview validator\n"
            "test_validate_adapter_package_assembly_preview.py\n"
            "Validate package-assembly preview contract\n"
            "validate_adapter_package_assembly_preview.py\n",
            encoding="utf-8",
        )
        (docs / "FOA_ADAPTER_PACKAGE_ASSEMBLY_PREVIEW.md").write_text(
            "reviewed build manifest project-owned staging inventory derived package layout "
            "output digests omissions collisions redistribution blockers AssemblyAllowed "
            "ArchiveAllowed DeploymentAllowed No file copying, archive creation, deployment, "
            "launch, or execution",
            encoding="utf-8",
        )
        (docs / "README.md").write_text(
            "FoA Adapter Package Assembly Preview FOA_ADAPTER_PACKAGE_ASSEMBLY_PREVIEW.md",
            encoding="utf-8",
        )
        (docs / "FOA_ADAPTER_BUILD_MANIFESTS.md").write_text(
            "Slice 12 package-assembly preview no file copying",
            encoding="utf-8",
        )
        (docs / "USER_GUIDE.md").write_text(
            "Tainted Grail Package Assembly Preview output_missing collision derived layout "
            "nothing is copied or archived",
            encoding="utf-8",
        )
        (docs / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md").write_text(
            "All thirteen TG SDK panes Tainted Grail Package Assembly Preview "
            "zero registered package-preview inputs non-editable",
            encoding="utf-8",
        )
        (docs / "CORE_FRAMEWORK_BUILD_GRAPH.md").write_text(
            "package-assembly preview derivation AssemblyAllowed=false",
            encoding="utf-8",
        )
        (docs / "RELEASE_PROCESS.md").write_text(
            "generated packages inspected checksums redistributable output",
            encoding="utf-8",
        )
        (repo / "ROADMAP.md").write_text(
            "Deterministic package-assembly preview\n"
            "Status: implemented, continuing hardening and Windows UI verification.\n"
            "project-owned staging inventory\n"
            "No file copying, archive creation, deployment, launch, or execution",
            encoding="utf-8",
        )
        (repo / "CHANGELOG.md").write_text(
            "AdapterPackageAssemblyPreviewService Tainted Grail Package Assembly Preview "
            "AssemblyAllowed",
            encoding="utf-8",
        )
        return repo

    def test_complete_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_adapter_package_assembly_preview(self.make_repo(Path(temporary)))

    def test_copy_or_archive_code_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterPackageAssemblyPreviewService.cpp"
            path.write_text(path.read_text() + "\nCreateArchive\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterPackageAssemblyPreviewContractError, "CreateArchive"):
                validate_adapter_package_assembly_preview(repo)

    def test_missing_project_ownership_gate_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            for name in (
                "AdapterPackageAssemblyPreviewService.h",
                "AdapterPackageAssemblyPreviewService.cpp",
                "AdapterPackageAssemblyPreviewServicePart1.inl",
                "AdapterPackageAssemblyPreviewServicePart2.inl",
                "AdapterPackageAssemblyPreviewServicePart3.inl",
            ):
                path = repo / "Gems/TaintedGrailModdingSDK/Code/Source" / name
                path.write_text(
                    path.read_text().replace("m_projectOwned", "ownership omitted"),
                    encoding="utf-8",
                )
            with self.assertRaisesRegex(AdapterPackageAssemblyPreviewContractError, "m_projectOwned"):
                validate_adapter_package_assembly_preview(repo)

    def test_mutable_widget_control_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterPackageAssemblyPreviewWidget.cpp"
            path.write_text(path.read_text() + "\nQPushButton\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterPackageAssemblyPreviewContractError, "QPushButton"):
                validate_adapter_package_assembly_preview(repo)

    def test_missing_collision_test_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Code/Tests/AdapterPackageAssemblyPreviewTests.cpp"
            path.write_text(
                path.read_text().replace("UnsafePathAndTargetCollisionAreRejected", "missing"),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(AdapterPackageAssemblyPreviewContractError, "Collision"):
                validate_adapter_package_assembly_preview(repo)

    def test_missing_windows_acceptance_contract_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md"
            path.write_text("All twelve TG SDK panes", encoding="utf-8")
            with self.assertRaisesRegex(AdapterPackageAssemblyPreviewContractError, "thirteen"):
                validate_adapter_package_assembly_preview(repo)


if __name__ == "__main__":
    unittest.main()
