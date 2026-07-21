#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import shutil
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
VALIDATOR_PATH = (
    REPO_ROOT
    / "Gems"
    / "TaintedGrailModdingSDK"
    / "Tools"
    / "validate_tainted_interface_ui_utilities.py"
)
SPEC = importlib.util.spec_from_file_location("ti_ui_validator", VALIDATOR_PATH)
validator = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(validator)


class TaintedInterfaceUiUtilitiesValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / "repo"
        source_gem = REPO_ROOT / "Gems" / "TaintedGrailModdingSDK"
        target_gem = self.root / "Gems" / "TaintedGrailModdingSDK"

        for relative in (
            "Code/Source/TaintedInterfaceUiUtilities.h",
            "Code/Source/TaintedInterfaceUiUtilities.cpp",
            "Code/Source/FoundationService.h",
            "Code/Source/FoundationTaintedInterfaceUiUtilities.cpp",
            "Code/Tests/TaintedInterfaceUiUtilitiesTests.cpp",
            "Code/taintedgrailmoddingsdk_framework_files.cmake",
            "Code/taintedgrailmoddingsdk_tainted_framework_tests_files.cmake",
            "Tools/run_local_validation.py",
        ):
            source = source_gem / relative
            target = target_gem / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

        source_docs = REPO_ROOT / "docs" / "tainted-grail-sdk"
        target_docs = self.root / "docs" / "tainted-grail-sdk"
        target_docs.mkdir(parents=True)
        for name in (
            "README.md",
            "TAINTED_INTERFACE_UI_UTILITIES.md",
            "TAINTED_INTERFACE_UI_UTILITIES_REVIEW.md",
        ):
            shutil.copy2(source_docs / name, target_docs / name)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def path(self, relative: str) -> Path:
        return self.root / relative

    def replace(self, relative: str, old: str, new: str) -> None:
        path = self.path(relative)
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text)
        path.write_text(text.replace(old, new, 1), encoding="utf-8")

    def assert_fails(self, fragment: str) -> None:
        with self.assertRaisesRegex(
            validator.TaintedInterfaceUiUtilitiesError,
            fragment,
        ):
            validator.validate(self.root)

    def test_repository_fixture_passes(self) -> None:
        validator.validate(self.root)

    def test_upstream_payload_embedding_fails_closed(self) -> None:
        self.replace(
            "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.cpp",
            '"NOASSERTION", false, false, true }',
            '"NOASSERTION", true, false, true }',
        )
        self.assert_fails("Every curated upstream asset")

    def test_upstream_redistribution_fails_closed(self) -> None:
        self.replace(
            "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.cpp",
            '"NOASSERTION", false, false, true }',
            '"NOASSERTION", false, true, true }',
        )
        self.assert_fails("Every curated upstream asset")

    def test_svg_byte_tamper_fails_fingerprint_verification(self) -> None:
        self.replace(
            "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.cpp",
            'fill="#1f2425"',
            'fill="#1f2426"',
        )
        self.assert_fails("Embedded SVG fingerprint mismatch")

    def test_runtime_dependency_leak_fails(self) -> None:
        path = self.path(
            "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.h"
        )
        path.write_text(
            path.read_text(encoding="utf-8")
            + "\n#include <BepInEx/BaseUnityPlugin.h>\n",
            encoding="utf-8",
        )
        self.assert_fails("prohibited authority")

    def test_curated_catalog_removal_fails(self) -> None:
        self.replace(
            "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.cpp",
            '"ui.hud.panel"',
            '"ui.hud.panel-removed"',
        )
        self.assert_fails("Curated semantic asset catalog drifted")

    def test_foundation_ownership_removal_fails(self) -> None:
        self.replace(
            "Gems/TaintedGrailModdingSDK/Code/Source/FoundationService.h",
            "        TaintedInterfaceUi::Service m_taintedInterfaceUiUtilities;\n",
            "",
        )
        self.assert_fails("Foundation must own")

    def test_compiled_test_wiring_removal_fails(self) -> None:
        self.replace(
            "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_tainted_framework_tests_files.cmake",
            "    Tests/TaintedInterfaceUiUtilitiesTests.cpp\n",
            "",
        )
        self.assert_fails("tests are not compiled")


if __name__ == "__main__":
    unittest.main()
