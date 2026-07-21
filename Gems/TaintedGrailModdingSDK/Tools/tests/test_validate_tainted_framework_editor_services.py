#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
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
    / "validate_tainted_framework_editor_services.py"
)
SPEC = importlib.util.spec_from_file_location("tf_editor_validator", VALIDATOR_PATH)
validator = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(validator)


class TaintedFrameworkEditorServicesValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / "repo"
        source_gem = REPO_ROOT / "Gems" / "TaintedGrailModdingSDK"
        target_gem = self.root / "Gems" / "TaintedGrailModdingSDK"
        (target_gem / "Code" / "Source").mkdir(parents=True)
        (target_gem / "Code" / "Tests").mkdir(parents=True)
        (target_gem / "Tools").mkdir(parents=True)

        for relative in (
            "Code/Source/TaintedFrameworkEditorServices.h",
            "Code/Source/TaintedFrameworkEditorServices.cpp",
            "Code/Source/FoundationService.h",
            "Code/Source/FoundationTaintedFrameworkEditorServices.cpp",
            "Code/Tests/TaintedFrameworkEditorServicesTests.cpp",
            "Code/taintedgrailmoddingsdk_framework_files.cmake",
            "Code/taintedgrailmoddingsdk_tainted_framework_tests_files.cmake",
            "Tools/run_local_validation.py",
        ):
            source = source_gem / relative
            target = target_gem / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def path(self, relative: str) -> Path:
        return self.root / "Gems" / "TaintedGrailModdingSDK" / relative

    def replace(self, relative: str, old: str, new: str) -> None:
        path = self.path(relative)
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text)
        path.write_text(text.replace(old, new, 1), encoding="utf-8")

    def assert_fails(self, fragment: str) -> None:
        with self.assertRaisesRegex(
            validator.TaintedFrameworkEditorServicesError,
            fragment,
        ):
            validator.validate(self.root)

    def test_repository_service_passes(self) -> None:
        validator.validate(self.root)

    def test_runtime_authority_escalation_fails(self) -> None:
        self.replace(
            "Code/Source/TaintedFrameworkEditorServices.cpp",
            "plan.m_runtimeInvocationAllowed = false",
            "plan.m_runtimeInvocationAllowed = true",
        )
        self.assert_fails("Permanent no-authority flag missing")

    def test_version_check_removal_fails(self) -> None:
        self.replace(
            "Code/Source/TaintedFrameworkEditorServices.cpp",
            "if (row.m_gameVersion == gameVersion)",
            "if (true)",
        )
        self.assert_fails("Missing required service fragment")

    def test_bepinex_include_fails(self) -> None:
        path = self.path("Code/Source/TaintedFrameworkEditorServices.h")
        path.write_text(
            path.read_text(encoding="utf-8") + "\n#include <BepInEx/BaseUnityPlugin.h>\n",
            encoding="utf-8",
        )
        self.assert_fails("prohibited authority")

    def test_compiled_test_removal_fails(self) -> None:
        self.replace(
            "Code/taintedgrailmoddingsdk_tainted_framework_tests_files.cmake",
            "    Tests/TaintedFrameworkEditorServicesTests.cpp\n",
            "",
        )
        self.assert_fails("tests are not compiled")

    def test_foundation_service_ownership_removal_fails(self) -> None:
        self.replace(
            "Code/Source/FoundationService.h",
            "        TaintedFrameworkEditorServices::Service m_taintedFrameworkEditorServices;\n",
            "",
        )
        self.assert_fails("Foundation must own")


if __name__ == "__main__":
    unittest.main()
