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

spec = importlib.util.spec_from_file_location("planner_guard", Path(__file__).resolve().parents[1] / "validate_framework_planners.py")
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)

class PlannerGuardTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for path in guard.PATHS:
            destination = self.root / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(guard.ROOT / path, destination)

    def append(self, path, text):
        path = self.root / guard.CODE / path
        path.write_text(path.read_text(encoding="utf-8-sig") + text, encoding="utf-8")

    def test_current_boundary(self):
        self.assertEqual([], guard.validate(self.root))

    def test_process_and_authority_bypasses_are_rejected(self):
        self.append("Source/ExecutionPlanning/FrameworkPlannerService.cpp", "\nvoid bad() { CreateProcessW(); m_executionAllowed = true; }")
        self.assertGreaterEqual(len(guard.validate(self.root)), 2)

    def test_pane_owned_planner_is_rejected(self):
        self.append("Source/AdapterBuildManifestWidget.h", "\nAdapterBuildManifestService m_bypass;")
        self.assertTrue(guard.validate(self.root))

    def test_new_unreviewed_source_is_rejected(self):
        (self.root / guard.CODE / "Source/ExecutionPlanning/Extra.cpp").write_text("// extra")
        self.assertTrue(guard.validate(self.root))

    def test_missing_compiled_lane_is_rejected(self):
        path = self.root / guard.CODE / "taintedgrailmoddingsdk_catalog_tests_files.cmake"
        path.write_text(path.read_text().replace("Tests/FrameworkPlannerTests.cpp", ""), encoding="utf-8")
        self.assertTrue(guard.validate(self.root))
