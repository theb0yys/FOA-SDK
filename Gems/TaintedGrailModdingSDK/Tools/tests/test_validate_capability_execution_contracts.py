#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Adversarial fixtures for the M1 source/target/CI guard."""
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_local_validation

TOOLS = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("capability_guard", TOOLS / "validate_capability_execution_contracts.py")
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)

class CapabilityExecutionBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        paths = [guard.CODE / "Source" / n for n in guard.FAMILY]
        paths += [guard.CODE / n for n in guard.TESTS]
        paths += [guard.CODE / "CMakeLists.txt", guard.WORKFLOW, guard.TOOLS / "run_local_validation.py"]
        paths += [p.relative_to(guard.ROOT) for p in (guard.ROOT / guard.CODE).glob("*files.cmake")]
        for path in paths:
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(guard.ROOT / path, target)
    def change(self, path, old, new):
        p = self.root / path
        text = p.read_text(encoding="utf-8")
        self.assertIn(old, text)
        p.write_text(text.replace(old, new, 1), encoding="utf-8")
    def rejected(self):
        self.assertTrue(guard.validate(self.root))
    def test_independent_ctest_requires_m1_matches(self):
        (self.root / "CMakeCache.txt").write_text("cache", encoding="utf-8")
        (self.root / "CTestTestfile.cmake").write_text("tests", encoding="utf-8")
        with mock.patch("run_local_validation.find_ctest", return_value="ctest"):
            command = run_local_validation.build_capability_ctest_command(self.root)
        self.assertIn("--no-tests=error", command.argv)
        self.assertEqual(r"TaintedGrailModdingSDK\.CapabilityExecution\.Tests", command.argv[command.argv.index("-R") + 1])
    def test_current_boundary(self):
        self.assertEqual([], guard.validate(self.root))
    def test_extra_production_file(self):
        (self.root / guard.CODE / "Source/CapabilityExecutionExecutor.cpp").write_text("// extra", encoding="utf-8")
        self.rejected()
    def test_missing_type(self):
        self.change(guard.CODE / "Source/CapabilityExecutionContracts.h", "struct ArtifactRecordV1", "struct AbsentV1")
        self.rejected()
    def test_side_effect(self):
        self.change(guard.CODE / "Source/CapabilityExecutionValidation.cpp", '#include "CapabilityExecutionValidation.h"', '#include "CapabilityExecutionValidation.h"\n#include <filesystem>')
        self.rejected()
    def test_missing_core_owner(self):
        self.change(guard.CODE / "taintedgrailmoddingsdk_core_files.cmake", "    Source/CapabilityExecutionContracts.cpp\n", "")
        self.rejected()
    def test_duplicate_owner(self):
        (self.root / guard.CODE / "extra_files.cmake").write_text("set(FILES\n Source/CapabilityExecutionContracts.cpp\n)\n", encoding="utf-8")
        self.rejected()
    def test_unauthorized_consumer(self):
        (self.root / guard.CODE / "Source/Unexpected.cpp").write_text('#include "CapabilityExecutionContracts.h"\n', encoding="utf-8")
        self.rejected()
    def test_test_manifest_production_recompile(self):
        p = self.root / guard.CODE / guard.MANIFEST
        p.write_text(p.read_text(encoding="utf-8") + "\nSource/CapabilityExecutionContracts.cpp\n", encoding="utf-8")
        self.rejected()
    def test_framework_dependency(self):
        self.change(guard.CODE / "CMakeLists.txt", "NAME ${gem_name}.CapabilityExecution.Tests", "NAME ${gem_name}.Missing.Tests")
        self.rejected()
    def test_ci_write_permission(self):
        self.change(guard.WORKFLOW, "contents: read", "contents: write")
        self.rejected()
    def test_missing_compiled_ci(self):
        self.change(guard.WORKFLOW, "  capability-execution-compiled:", "  absent-compiled:")
        self.rejected()
    def test_unknown_contract_version(self):
        self.change(guard.CODE / "Source/CapabilityExecutionContracts.h", "ContractVersion = 1", "ContractVersion = 2")
        self.rejected()

if __name__ == "__main__":
    unittest.main()
