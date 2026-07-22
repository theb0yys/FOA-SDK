from __future__ import annotations

import importlib.util
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

TEST_ROOT = Path(__file__).resolve().parent
BASE_TEST_PATH = TEST_ROOT / "test_approved_acquisition.py"
BASE_SPEC = importlib.util.spec_from_file_location(
    "foa_approved_acquisition_base_tests",
    BASE_TEST_PATH,
)
if BASE_SPEC is None or BASE_SPEC.loader is None:
    raise RuntimeError(f"Unable to load approved acquisition base tests: {BASE_TEST_PATH}")
BASE_TESTS = importlib.util.module_from_spec(BASE_SPEC)
sys.modules[BASE_SPEC.name] = BASE_TESTS
BASE_SPEC.loader.exec_module(BASE_TESTS)

ApprovedAcquisitionTests = BASE_TESTS.ApprovedAcquisitionTests
acquisition = BASE_TESTS.acquisition
canonical = BASE_TESTS.canonical


class ApprovedAcquisitionHardeningTests(unittest.TestCase):
    def make_contract(self, root: Path):
        helper = ApprovedAcquisitionTests(methodName="runTest")
        return helper.make_contract(root)

    def test_blocked_source_pattern_is_enforced_by_provider(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            manifest_path, _, _ = self.make_contract(Path(temporary))
            document = json.loads(manifest_path.read_text(encoding="utf-8"))
            document["blocked_sources"].append(
                {
                    "pattern": "Sources/**",
                    "reason": "fixture_sources_are_blocked",
                }
            )
            manifest_path.write_bytes(canonical(document))
            with self.assertRaisesRegex(
                acquisition.AcquisitionError,
                "prohibited by blocked_sources",
            ):
                acquisition.load_manifest(manifest_path)

    def test_blocked_bundle_pattern_is_enforced_by_provider(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            manifest_path, _, _ = self.make_contract(Path(temporary))
            document = json.loads(manifest_path.read_text(encoding="utf-8"))
            document["packages"][0]["files"][0]["bundle_path"] = (
                "Merlin/knowledge.json"
            )
            manifest_path.write_bytes(canonical(document))
            with self.assertRaisesRegex(
                acquisition.AcquisitionError,
                "prohibited by blocked_sources",
            ):
                acquisition.load_manifest(manifest_path)

    @unittest.skipIf(os.name == "nt", "Directory symlink creation requires privileges")
    def test_local_source_root_with_symlinked_ancestor_fails(self) -> None:
        with tempfile.TemporaryDirectory() as contract_temp, tempfile.TemporaryDirectory() as output_temp:
            root = Path(contract_temp)
            manifest_path, source_root, _ = self.make_contract(root)
            alias = root / "alias"
            alias.symlink_to(root, target_is_directory=True)
            linked_root = alias / source_root.name
            manifest = acquisition.load_manifest(manifest_path)
            with self.assertRaisesRegex(
                acquisition.AcquisitionError,
                "symlink or reparse point",
            ):
                acquisition.acquire(
                    manifest,
                    provider="local",
                    output=Path(output_temp) / "bundle",
                    source_root=linked_root,
                )

    def test_blocked_pattern_without_glob_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            manifest_path, _, _ = self.make_contract(Path(temporary))
            document = json.loads(manifest_path.read_text(encoding="utf-8"))
            document["blocked_sources"][0]["pattern"] = "Merlin"
            manifest_path.write_bytes(canonical(document))
            with self.assertRaisesRegex(
                acquisition.AcquisitionError,
                "explicit glob token",
            ):
                acquisition.load_manifest(manifest_path)


if __name__ == "__main__":
    unittest.main()
