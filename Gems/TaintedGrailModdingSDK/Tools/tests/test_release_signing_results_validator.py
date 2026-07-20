#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(TOOLS_ROOT))

import validate_release_signing_results as validator


FIXTURE_PATHS = (
    "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseSigningResultContracts.h",
    "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseSigningResultContracts.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseSigningEvidenceService.h",
    "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseSigningEvidenceService.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseSigningResultWidget.h",
    "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseSigningResultWidget.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseSigningPaneSystemComponent.h",
    "Gems/TaintedGrailModdingSDK/Code/Source/AdapterReleaseSigningPaneSystemComponent.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/TaintedGrailModdingSDKEditorModule.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Tests/AdapterReleaseSigningResultTests.cpp",
    "Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt",
    "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_core_files.cmake",
    "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_editor_files.cmake",
    (
        "Gems/TaintedGrailModdingSDK/Code/"
        "taintedgrailmoddingsdk_release_signing_result_tests_files.cmake"
    ),
    "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py",
    "Gems/TaintedGrailModdingSDK/Tools/tests/test_release_signing_results_validator.py",
    "docs/tainted-grail-sdk/FOA_RELEASE_SIGNING_RESULTS.md",
    "docs/tainted-grail-sdk/README.md",
    "docs/tainted-grail-sdk/RELEASE_PROCESS.md",
    "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md",
    "ROADMAP.md",
)


class ReleaseSigningValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(
            prefix="release-signing-validator-"
        )
        self.root = Path(self.temporary.name)
        for relative in FIXTURE_PATHS:
            source = REPO_ROOT / relative
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def mutate(self, relative: str, transform) -> None:
        path = self.root / relative
        text = path.read_text(encoding="utf-8")
        path.write_text(transform(text), encoding="utf-8")

    def assert_validation_fails(self, expected_fragment: str) -> None:
        with self.assertRaisesRegex(
            validator.ReleaseSigningValidationError,
            expected_fragment,
        ):
            validator.validate(self.root)

    def test_repository_fixture_passes(self) -> None:
        validator.validate(self.root)

    def test_mutable_widget_control_fails_closed(self) -> None:
        self.mutate(
            (
                "Gems/TaintedGrailModdingSDK/Code/Source/"
                "AdapterReleaseSigningResultWidget.cpp"
            ),
            lambda text: text + "\nQPushButton* prohibitedAction = nullptr;\n",
        )
        self.assert_validation_fails("QPushButton")

    def test_missing_local_gate_registration_fails_closed(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py",
            lambda text: text.replace(
                '    "validate_release_signing_results.py",\n',
                "",
            ),
        )
        self.assert_validation_fails("local-validation gate")

    def test_missing_documentation_hub_links_fail_closed(self) -> None:
        self.mutate(
            "docs/tainted-grail-sdk/README.md",
            lambda text: text.replace("FOA_RELEASE_SIGNING_RESULTS.md", ""),
        )
        self.assert_validation_fails("documentation hub")

    def test_missing_release_process_gate_fails_closed(self) -> None:
        self.mutate(
            "docs/tainted-grail-sdk/RELEASE_PROCESS.md",
            lambda text: text.replace(
                "### Release-signing result evidence gate",
                "### Removed signing gate",
            ),
        )
        self.assert_validation_fails("release process")

    def test_roadmap_future_marker_fails_closed(self) -> None:
        self.mutate(
            "ROADMAP.md",
            lambda text: text.replace(
                "### Release-signing result envelope",
                "### Next ordered slice \u2014 release-signing result envelope",
            ),
        )
        self.assert_validation_fails("roadmap")

    def test_twenty_two_pane_contract_fails_closed(self) -> None:
        self.mutate(
            "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md",
            lambda text: text.replace(
                "All twenty-two TG SDK panes",
                "All twenty-one TG SDK panes",
            ),
        )
        self.assert_validation_fails("Twenty-two-pane")


if __name__ == "__main__":
    unittest.main()
