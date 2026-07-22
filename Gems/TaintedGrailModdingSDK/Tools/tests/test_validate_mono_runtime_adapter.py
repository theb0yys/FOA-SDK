# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

VALIDATOR_PATH = Path(__file__).resolve().parents[1] / "validate_mono_runtime_adapter.py"
SPEC = importlib.util.spec_from_file_location(
    "validate_mono_runtime_adapter_test_target", VALIDATOR_PATH
)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)

SOURCE_ROOT = Path(__file__).resolve().parents[4]


class MonoRuntimeAdapterValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.repo = Path(self.temporary.name)
        for relative in (
            "Plugins/RuntimeAdapters/Mono",
            "Plugins/RuntimeAdapters/README.md",
            "Gems/TaintedGrailModdingSDK/Tools/tests/test_mono_runtime_adapter.py",
            "Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp",
            "docs/tainted-grail-sdk/MONO_RUNTIME_ADAPTER.md",
            "docs/tainted-grail-modding/runtime/README.md",
        ):
            source = SOURCE_ROOT / relative
            destination = self.repo / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            if source.is_dir():
                shutil.copytree(source, destination, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
            else:
                shutil.copy2(source, destination)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def validate(self) -> None:
        validator.validate_mono_runtime_adapter(self.repo)

    def test_reviewed_contract_passes(self) -> None:
        self.validate()

    def test_source_tamper_fails(self) -> None:
        path = self.repo / "Plugins/RuntimeAdapters/Mono/Source/MonoRuntimeAdapterPlugin.cs"
        path.write_text(path.read_text() + "\n// harmless source-byte drift\n", encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError, "fingerprint mismatch"
        ):
            self.validate()

    def test_binary_payload_insertion_fails(self) -> None:
        path = self.repo / "Plugins/RuntimeAdapters/Mono/bin/adapter.dll"
        path.parent.mkdir(parents=True)
        path.write_bytes(b"MZ")
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError,
            "file set drifted|binary entered",
        ):
            self.validate()

    def test_plugin_runtime_conflation_fails(self) -> None:
        path = self.repo / "Plugins/RuntimeAdapters/Mono/plugin.json"
        document = json.loads(path.read_text())
        document["compatibility"]["runtime_targets"] = ["mono", "il2cpp"]
        path.write_text(json.dumps(document), encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError, "compatibility"
        ):
            self.validate()

    def test_authority_escalation_fails(self) -> None:
        path = self.repo / "Plugins/RuntimeAdapters/Mono/Tools/_mono_contract.py"
        text = path.read_text().replace(
            "return {field: False for field in AUTHORITY_FIELDS}",
            "return {field: (field == 'runtime_execution') for field in AUTHORITY_FIELDS}",
            1,
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError, "authority|provider contract"
        ):
            self.validate()

    def test_executor_import_fails(self) -> None:
        path = self.repo / "Plugins/RuntimeAdapters/Mono/Tools/mono_runtime_adapter.py"
        path.write_text(
            path.read_text().replace(
                "import argparse\n", "import argparse\nimport subprocess\n", 1
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError, "executor|network client"
        ):
            self.validate()

    def test_il2cpp_api_in_mono_source_fails(self) -> None:
        path = self.repo / "Plugins/RuntimeAdapters/Mono/Source/MonoRuntimeAdapterPlugin.cs"
        path.write_text(path.read_text() + "\n// IL2CPP\n", encoding="utf-8")
        manifest = self.repo / "Plugins/RuntimeAdapters/Mono/mono-adapter.json"
        document = json.loads(manifest.read_text())
        import hashlib
        document["source_project"]["source_files"][1]["sha256"] = (
            "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()
        )
        manifest.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError, "IL2CPP scope"
        ):
            self.validate()

    def test_canonical_route_conflation_fails(self) -> None:
        path = self.repo / "Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp"
        path.write_text(
            path.read_text().replace(
                'il2Cpp.m_runtimeTarget = "IL2CPP";',
                'il2Cpp.m_runtimeTarget = "Mono";',
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError, "separate IL2CPP"
        ):
            self.validate()

    def test_mandatory_bridge_removal_fails(self) -> None:
        path = self.repo / "Gems/TaintedGrailModdingSDK/Tools/tests/test_mono_runtime_adapter.py"
        path.write_text(path.read_text().replace("MonoRuntimeAdapterTests", "Removed"), encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError, "mandatory Gem discovery"
        ):
            self.validate()

    def test_adversarial_test_removal_fails(self) -> None:
        path = self.repo / "Plugins/RuntimeAdapters/Mono/Tests/test_mono_runtime_adapter.py"
        path.write_text(
            path.read_text().replace(
                "test_il2cpp_profile_is_refused", "removed_il2cpp_test", 1
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError, "test coverage drifted"
        ):
            self.validate()

    def test_golden_plan_tamper_fails(self) -> None:
        path = self.repo / "Plugins/RuntimeAdapters/Mono/Tests/Fixtures/build-plan.json"
        document = json.loads(path.read_text())
        document["package_version"] = "9.9.9"
        path.write_text(json.dumps(document), encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError, "build plan|provider contract"
        ):
            self.validate()

    def test_documented_separation_removal_fails(self) -> None:
        path = self.repo / "docs/tainted-grail-sdk/MONO_RUNTIME_ADAPTER.md"
        path.write_text(path.read_text().replace("Mono/IL2CPP separation", "Combined runtime"), encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MonoRuntimeAdapterValidationError, "public contract"
        ):
            self.validate()


if __name__ == "__main__":
    unittest.main()
