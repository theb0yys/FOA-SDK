# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations
import importlib.util
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[2]
spec = importlib.util.spec_from_file_location("m2_policy", TOOLS / "validate_tool_execution.py")
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)
legacy_spec = importlib.util.spec_from_file_location("legacy_policy", TOOLS / "validate_external_toolchain_foundation.py")
legacy = importlib.util.module_from_spec(legacy_spec)
legacy_spec.loader.exec_module(legacy)

class ToolExecutionPolicyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / "Gems/ExternalToolchain", self.root / "Gems/ExternalToolchain")
    def mutate(self, path, before, after):
        file = self.root / "Gems/ExternalToolchain" / path
        text = file.read_text(encoding="utf-8")
        self.assertIn(before, text)
        file.write_text(text.replace(before, after), encoding="utf-8")
    def test_ci_native_lane_requires_fixture_and_nonzero_tests(self):
        ci_tools = ROOT / "Gems/TaintedGrailModdingSDK/Tools"
        sys.path.insert(0, str(ci_tools))
        try:
            ci_spec = importlib.util.spec_from_file_location("m2_ci_policy", ci_tools / "validate_ci_runner_policy.py")
            ci = importlib.util.module_from_spec(ci_spec)
            ci_spec.loader.exec_module(ci)
            workflow = (ROOT / ".github/workflows/tainted-grail-sdk-pr-validation.yml").read_text(encoding="utf-8")
            ci.validate_tool_execution_workflow(workflow)
            for fragment in ("tool-execution-operational:", "Test-Path -LiteralPath $fixture -PathType Leaf"):
                with self.assertRaises(ci.CiRunnerPolicyError):
                    ci.validate_tool_execution_workflow(workflow.replace(fragment, "missing"))
        finally:
            sys.path.remove(str(ci_tools))

    def test_current_boundaries(self):
        policy.validate(self.root)
    def test_default_execution_cannot_be_enabled(self):
        self.mutate("Registry/external_toolchain.setreg", '"ProcessExecutionEnabled": false', '"ProcessExecutionEnabled": true')
        with self.assertRaises(policy.ToolExecutionPolicyError):
            policy.validate(self.root)
    def test_core_cannot_gain_process_dependency(self):
        self.mutate("Code/Source/Execution/ToolExecutionContracts.cpp", "#include <algorithm>", "#include <algorithm>\n#include <AzFramework/Process/ProcessWatcher.h>")
        with self.assertRaises(policy.ToolExecutionPolicyError):
            policy.validate(self.root)
    def test_cleanup_cannot_be_detached(self):
        self.mutate("Code/Source/Execution/ToolExecutionService.cpp", "worker.join()", "worker.detach()")
        with self.assertRaises(policy.ToolExecutionPolicyError):
            policy.validate(self.root)
    def test_editor_fast_exit_requires_early_join(self):
        self.mutate("Code/Source/Editor/ExternalToolchainEditorSystemComponent.cpp", "QCoreApplication::aboutToQuit", "late_only_shutdown")
        with self.assertRaises(policy.ToolExecutionPolicyError):
            policy.validate(self.root)
    def test_exact_capability_set_is_required(self):
        self.mutate("Code/Source/Execution/Platform/Windows/ToolSandbox_Windows.cpp",
                    "capabilities->GroupCount != 1", "capabilities->GroupCount > 1")
        with self.assertRaises(policy.ToolExecutionPolicyError):
            policy.validate(self.root)
    def test_handle_allowlist_is_required(self):
        self.mutate("Code/Source/Execution/Platform/Windows/ToolProcessBackend_Windows.cpp", "PROC_THREAD_ATTRIBUTE_HANDLE_LIST", "UNRESTRICTED_HANDLES")
        with self.assertRaises(policy.ToolExecutionPolicyError):
            policy.validate(self.root)
    def test_production_sources_are_not_recompiled_in_tests(self):
        self.mutate("Code/externaltoolchain_execution_tests_files.cmake", "Tests/Main.cpp", "Source/Execution/ToolExecutionService.cpp")
        with self.assertRaises(policy.ToolExecutionPolicyError):
            policy.validate(self.root)
    def test_legacy_registration_uses_product_project_not_engine_json(self):
        project = self.root / "TaintedGrailModdingEditor"
        project.mkdir()
        source = ROOT / "TaintedGrailModdingEditor/project.json"
        shutil.copyfile(source, project / "project.json")
        legacy.validate_registration(self.root)
        self.assertFalse((self.root / "engine.json").exists())
        text = (project / "project.json").read_text(encoding="utf-8")
        (project / "project.json").write_text(text.replace('"../Gems/ExternalToolchain",', ''), encoding="utf-8")
        with self.assertRaises(SystemExit):
            legacy.validate_registration(self.root)

if __name__ == "__main__":
    unittest.main()
