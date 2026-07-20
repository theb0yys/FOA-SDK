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

from validate_adapter_build_manifests import (  # noqa: E402
    AdapterBuildManifestContractError,
    validate_adapter_build_manifests,
)


class AdapterBuildManifestValidatorTests(unittest.TestCase):
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
            "Source/AdapterBuildManifestService.cpp\n"
            "Source/AdapterBuildManifestService.h\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_editor_files.cmake").write_text(
            "Source/AdapterBuildManifestWidget.cpp\n"
            "Source/AdapterBuildManifestWidget.h\n",
            encoding="utf-8",
        )
        (code / "taintedgrailmoddingsdk_build_manifest_tests_files.cmake").write_text(
            "set(FILES\n    Tests/AdapterBuildManifestTests.cpp\n)\n",
            encoding="utf-8",
        )
        (code / "CMakeLists.txt").write_text(
            "taintedgrailmoddingsdk_build_manifest_tests_files.cmake\n",
            encoding="utf-8",
        )

        service = """
enum class AdapterBuildDependencyKind
enum class AdapterBuildManifestStatus Ready PlanMismatch ToolchainUnresolved InputMissing
FingerprintMissing PathInvalid RedistributionBlocked
struct AdapterBuildMaterial struct AdapterBuildExpectedOutput struct AdapterBuildEnvironment
struct AdapterBuildManifestRequest struct AdapterBuildManifest class AdapterBuildManifestService
BuildManifest SerializeCanonicalManifest m_buildType = "foa.adapter.plugin"
m_planFingerprint m_sourceCommit m_o3deRevision m_targetFramework m_compilerId
m_deterministicBuild m_continuousIntegrationBuild m_pathMapEnabled
"work_order_plan" "source_tree" "dependency_lock" "toolchain_lock" "license"
"plugin_binary" "BepInEx/plugins/" PathIsInsideRoot
Non-redistributable material cannot enter package output
BuildAllowed m_buildAllowed = false SortDependencies SortMaterials SortOutputs
std::locale::classic()
"""
        (source / "AdapterBuildManifestService.h").write_text(service, encoding="utf-8")
        (source / "AdapterBuildManifestService.cpp").write_text(service, encoding="utf-8")
        for index in range(1, 4):
            (source / f"AdapterBuildManifestServicePart{index}.inl").write_text(
                service,
                encoding="utf-8",
            )

        widget = """
Tainted Grail Adapter Build Manifests
Read-only Phase 8 build-definition research
BuildAllowed is always false Nothing is compiled
QAbstractItemView::NoEditTriggers BuildManifest
tr("Resolved materials") tr("Expected package outputs")
tr("Canonical JSON / reasons") build: prohibited
"""
        (source / "AdapterBuildManifestWidget.cpp").write_text(widget, encoding="utf-8")
        (source / "AdapterBuildManifestWidget.h").write_text("widget", encoding="utf-8")
        (source / "TaintedGrailModdingSDKSystemComponent.cpp").write_text(
            '#include "AdapterBuildManifestWidget.h"\n'
            "AdapterBuildManifestViewPaneName\n"
            "RegisterViewPane<AdapterBuildManifestWidget>\n"
            "UnregisterViewPane(AdapterBuildManifestViewPaneName)\n"
            "TaintedGrailModdingSDK.AdapterBuildManifests\n",
            encoding="utf-8",
        )

        test_text = """
TypedStatusAndDependencyVocabulariesAreStrict
CompleteDefinitionIsReadyButBuildProhibited
PlanMismatchPrecedesOtherFailures
MissingToolchainFailsClosed
MissingRequiredInputAndFingerprintRemainDistinct
PathTraversalAndDuplicateOutputsAreRejected
NonRedistributablePackageMaterialIsBlocked
CanonicalSerializationIsDeterministic
ManifestGenerationDoesNotMutateInputs
BepInExMetadataAndResolvedInputsArePreserved
EXPECT_FALSE(manifest.m_buildAllowed)
"BuildAllowed\\\":false"
materialCount outputCount
"""
        (tests / "AdapterBuildManifestTests.cpp").write_text(test_text, encoding="utf-8")

        (workflow / "tainted-grail-sdk-foundation.yml").write_text(
            "Test adapter build-manifest validator\n"
            "test_validate_adapter_build_manifests.py\n"
            "Validate adapter build-manifest contract\n"
            "validate_adapter_build_manifests.py\n",
            encoding="utf-8",
        )
        (docs / "FOA_ADAPTER_BUILD_MANIFESTS.md").write_text(
            "reproducible build definition resolved materials BepInEx plugin metadata "
            "hard and soft dependencies safe relative locator redistribution ready "
            "toolchain_unresolved BuildAllowed No compilation, packaging, deployment, or execution SLSA",
            encoding="utf-8",
        )
        (docs / "README.md").write_text(
            "FoA Adapter Build Manifests FOA_ADAPTER_BUILD_MANIFESTS.md",
            encoding="utf-8",
        )
        (docs / "USER_GUIDE.md").write_text(
            "Tainted Grail Adapter Build Manifests toolchain_unresolved resolved materials "
            "expected package outputs nothing is built or packaged",
            encoding="utf-8",
        )
        (docs / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md").write_text(
            "All twelve TG SDK panes Tainted Grail Adapter Build Manifests "
            "zero ready build definitions non-editable",
            encoding="utf-8",
        )
        (docs / "CORE_FRAMEWORK_BUILD_GRAPH.md").write_text(
            "reproducible adapter build-manifest generation BuildAllowed=false",
            encoding="utf-8",
        )
        (docs / "RELEASE_PROCESS.md").write_text(
            "reproducible from documented inputs generated package contents SHA-256",
            encoding="utf-8",
        )
        (repo / "ROADMAP.md").write_text(
            "Reproducible adapter build manifests\n"
            "Status: implemented, continuing hardening and Windows UI verification.\n"
            "No compilation, packaging, deployment, launch, or execution",
            encoding="utf-8",
        )
        (repo / "CHANGELOG.md").write_text(
            "AdapterBuildManifestService Tainted Grail Adapter Build Manifests BuildAllowed",
            encoding="utf-8",
        )
        return repo

    def test_complete_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_adapter_build_manifests(self.make_repo(Path(temporary)))

    def test_build_execution_code_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterBuildManifestService.cpp"
            path.write_text(path.read_text() + "\nExecuteBuild\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterBuildManifestContractError, "ExecuteBuild"):
                validate_adapter_build_manifests(repo)

    def test_mutable_widget_control_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Code/Source/AdapterBuildManifestWidget.cpp"
            path.write_text(path.read_text() + "\nQPushButton\n", encoding="utf-8")
            with self.assertRaisesRegex(AdapterBuildManifestContractError, "QPushButton"):
                validate_adapter_build_manifests(repo)

    def test_missing_path_containment_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            for path in (repo / "Gems/TaintedGrailModdingSDK/Code/Source").glob(
                "AdapterBuildManifestService*"
            ):
                path.write_text(path.read_text().replace("PathIsInsideRoot", "path omitted"), encoding="utf-8")
            with self.assertRaisesRegex(AdapterBuildManifestContractError, "PathIsInsideRoot"):
                validate_adapter_build_manifests(repo)

    def test_missing_redistribution_gate_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            for path in (repo / "Gems/TaintedGrailModdingSDK/Code/Source").glob(
                "AdapterBuildManifestService*"
            ):
                path.write_text(
                    path.read_text().replace(
                        "Non-redistributable material cannot enter package output",
                        "gate omitted",
                    ),
                    encoding="utf-8",
                )
            with self.assertRaisesRegex(AdapterBuildManifestContractError, "Non-redistributable"):
                validate_adapter_build_manifests(repo)

    def test_missing_determinism_test_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Code/Tests/AdapterBuildManifestTests.cpp"
            path.write_text(
                path.read_text().replace("CanonicalSerializationIsDeterministic", "missing"),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(AdapterBuildManifestContractError, "CanonicalSerialization"):
                validate_adapter_build_manifests(repo)


if __name__ == "__main__":
    unittest.main()
