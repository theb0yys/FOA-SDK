# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest
spec = importlib.util.spec_from_file_location("synthetic_guard",Path(__file__).resolve().parents[1]/"validate_framework_synthetic.py")
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)
class SyntheticGuardTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        paths = [guard.CODE / "Source/ExecutionSynthetic" / name for name in guard.FILES]
        paths += [guard.CODE / "CMakeLists.txt", guard.CODE / "Source/ExecutionFramework/FrameworkProviderService.cpp",
                  guard.CODE / "Tests/FrameworkExecutionOperationalTests.cpp"]
        for path in paths:
            target = self.root / path
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(guard.ROOT/path,target)
    def test_current_boundary(self):
        self.assertEqual([],guard.validate(self.root))
    def test_unsupervised_process_is_rejected(self):
        target = self.root/guard.CODE/"Source/ExecutionSynthetic/FrameworkSyntheticTarget.cpp"
        target.write_text(target.read_text()+"\nvoid bad() { CreateProcessW(); }\n")
        self.assertTrue(guard.validate(self.root))
    def test_default_scope_cannot_be_widened(self):
        target = self.root/guard.CODE/"Source/ExecutionFramework/FrameworkProviderService.cpp"
        target.write_text(" ".join(target.read_text().split()).replace("if (!m_synthetic) return Supported(d)","return true"))
        self.assertTrue(guard.validate(self.root))
    def test_native_provider_must_be_registered(self):
        target = self.root/guard.CODE/"CMakeLists.txt"
        target.write_text(target.read_text().replace("FOA_M5_PROVIDER", "REMOVED"))
        self.assertTrue(guard.validate(self.root))
