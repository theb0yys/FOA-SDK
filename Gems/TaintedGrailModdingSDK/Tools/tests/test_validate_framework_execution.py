#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("framework_guard", Path(__file__).resolve().parents[1] / "validate_framework_execution.py")
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)

class FrameworkGuardTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        paths = [guard.CODE / "Source/ExecutionFramework" / n for n in guard.FAMILY]
        paths += [guard.CODE / "CMakeLists.txt", guard.CODE / "taintedgrailmoddingsdk_framework_execution_files.cmake",
                  Path(".github/workflows/tainted-grail-sdk-pr-validation.yml")]
        paths += [guard.CODE / f"Tests/FrameworkExecution{s}Tests.cpp" for s in ("ProviderPolicy", "Repository", "Orchestrator", "Operational")]
        for p in paths:
            destination = self.root / p
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(guard.ROOT / p, destination)
    def test_current_boundary(self):
        self.assertEqual([], guard.validate(self.root))
    def test_process_bypass_is_rejected(self):
        p = self.root / guard.CODE / "Source/ExecutionFramework/FrameworkExecutionService.cpp"
        p.write_text(p.read_text(encoding="utf-8") + "\nvoid bad() { CreateProcessW(); }\n", encoding="utf-8")
        self.assertTrue(guard.validate(self.root))
    def test_native_skip_is_rejected(self):
        p = self.root / guard.CODE / "Tests/FrameworkExecutionOperationalTests.cpp"
        p.write_text(p.read_text(encoding="utf-8") + "\nGTEST_SKIP();\n", encoding="utf-8")
        self.assertTrue(guard.validate(self.root))
    def test_extra_source_is_rejected(self):
        p = self.root / guard.CODE / "Source/ExecutionFramework/Unreviewed.cpp"
        p.write_text("// extra", encoding="utf-8")
        self.assertTrue(guard.validate(self.root))
