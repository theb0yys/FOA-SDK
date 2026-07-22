#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import hashlib
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

VALIDATOR_PATH = Path(__file__).resolve().parents[1] / "validate_approved_acquisition.py"
SPEC = importlib.util.spec_from_file_location("validate_approved_acquisition_test_target", VALIDATOR_PATH)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)

PROVIDER_SOURCE = Path(__file__).resolve().parents[4] / "Plugins/Integrations/ApprovedAcquisition/Tools/approved_acquisition.py"


def canonical(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def load_module(path: Path):
    spec = importlib.util.spec_from_file_location("approved_acquisition_fixture_module", path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class ApprovedAcquisitionValidatorTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        package = root / "Plugins/Integrations/ApprovedAcquisition"
        tool = package / "Tools/approved_acquisition.py"
        tool.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(PROVIDER_SOURCE, tool)

        assets = {
            "action.svg": b"<svg>action</svg>",
            "device.svg": b"<svg>device</svg>",
            "frame.svg": b"<svg>frame</svg>",
        }
        file_rows = []
        for name, payload in assets.items():
            source_path = f"Plugins/Integrations/ApprovedAcquisition/Seeds/Assets/{name}"
            destination = root / source_path
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(payload)
            file_rows.append(
                {
                    "bundle_path": f"Assets/{name}",
                    "bytes": len(payload),
                    "media_type": "image/svg+xml",
                    "sha256": hashlib.sha256(payload).hexdigest(),
                    "source_path": source_path,
                }
            )
        manifest = {
            "authority": {
                "catalog_mutation_allowed": False,
                "deployment_allowed": False,
                "evidence_promotion_allowed": False,
                "publication_allowed": False,
                "runtime_execution_allowed": False,
            },
            "blocked_sources": [
                {"pattern": "Merlin/**", "reason": "optional_merlin_provider_is_separate_later_unit"},
                {"pattern": "Runtime/**", "reason": "runtime_adapters_are_separate_later_units"},
            ],
            "limits": {
                "maximum_file_bytes": 1024,
                "maximum_files": 8,
                "maximum_total_bytes": 4096,
            },
            "packages": [
                {
                    "files": file_rows,
                    "id": "fallback-assets",
                    "license": "Apache-2.0 OR MIT",
                    "redistribution_reviewed": True,
                    "version": "1.0.0",
                }
            ],
            "provider_id": "extension.approved-acquisition",
            "schema_version": 1,
            "source": {
                "commit": "7d3d31371b4327ee3abbaa48b8741b4390bb9e0e",
                "display_branch": "fixture",
                "repository": "theb0yys/FOA-SDK",
            },
        }
        (package / "approved-sources.json").write_bytes(canonical(manifest))
        plugin = {
            "schema_version": 1,
            "id": "extension.approved-acquisition",
            "name": "Approved Local and Pinned-GitHub Acquisition",
            "version": "0.1.0",
            "category": "integration",
            "status": "experimental",
            "optional": True,
            "entry_points": {
                "python_modules": ["Tools/approved_acquisition.py"],
                "tools": ["Tools/approved_acquisition.py"],
            },
            "capabilities": [
                "acquisition.bundle.verify",
                "acquisition.github.pinned.read",
                "acquisition.local.read",
                "acquisition.plan",
            ],
            "dependencies": [],
            "compatibility": {
                "game_versions": ["1.23.401"],
                "branches": ["Mono"],
                "runtime_targets": ["editor-only"],
            },
            "provenance": {
                "origin": "theb0yys/FOA-SDK",
                "revision": "fixture",
                "license": "Apache-2.0 OR MIT",
                "redistribution_reviewed": True,
            },
        }
        (package / "plugin.json").write_text(json.dumps(plugin), encoding="utf-8")
        (package / "README.md").write_text("approved acquisition\n", encoding="utf-8")
        test_file = package / "Tests/test_approved_acquisition.py"
        test_file.parent.mkdir(parents=True, exist_ok=True)
        test_file.write_text(
            "\n".join(
                (
                    "test_local_acquisition_is_atomic_and_verifiable",
                    "test_pinned_github_route_uses_injected_verified_bytes",
                    "test_local_symlink_source_fails",
                    "test_tampered_bundle_fails",
                    "test_production_manifest_matches_golden_plan_and_seed_hashes",
                )
            ),
            encoding="utf-8",
        )
        hardening_test = package / "Tests/test_approved_acquisition_hardening.py"
        hardening_test.write_text(
            "\n".join(
                (
                    "test_blocked_source_pattern_is_enforced_by_provider",
                    "test_blocked_bundle_pattern_is_enforced_by_provider",
                    "test_local_source_root_with_symlinked_ancestor_fails",
                )
            ),
            encoding="utf-8",
        )
        module = load_module(tool)
        approved = module.load_manifest(package / "approved-sources.json")
        golden = package / "Tests/Fixtures/pinned-github-all.plan.json"
        golden.parent.mkdir(parents=True, exist_ok=True)
        golden.write_bytes(module.canonical_json_bytes(module.build_plan(approved, provider="pinned-github")))

        bridge = root / "Gems/TaintedGrailModdingSDK/Tools/tests/test_approved_acquisition.py"
        bridge.parent.mkdir(parents=True, exist_ok=True)
        bridge.write_text(
            "ApprovedAcquisition/Tests/test_approved_acquisition.py\nload_tests\nApprovedAcquisitionTests\n",
            encoding="utf-8",
        )
        hardening_bridge = root / "Gems/TaintedGrailModdingSDK/Tools/tests/test_approved_acquisition_hardening.py"
        hardening_bridge.write_text(
            "test_approved_acquisition_hardening.py\nApprovedAcquisitionHardeningTests\n",
            encoding="utf-8",
        )
        integrations = root / "Plugins/Integrations/README.md"
        integrations.parent.mkdir(parents=True, exist_ok=True)
        integrations.write_text("ApprovedAcquisition\n", encoding="utf-8")
        public = root / "docs/tainted-grail-sdk/APPROVED_ACQUISITION_PROVIDERS.md"
        public.parent.mkdir(parents=True, exist_ok=True)
        public.write_text(
            "local pinned-GitHub 7d3d31371b4327ee3abbaa48b8741b4390bb9e0e "
            "NO upstream PNG outside the FOA-SDK checkout candidate evidence Merlin Mono IL2CPP\n",
            encoding="utf-8",
        )
        return root

    def test_reviewed_package_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validator.validate_approved_acquisition(self.make_repo(Path(temporary)))

    def test_authority_escalation_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Plugins/Integrations/ApprovedAcquisition/approved-sources.json"
            document = json.loads(path.read_text())
            document["authority"]["runtime_execution_allowed"] = True
            path.write_bytes(canonical(document))
            with self.assertRaisesRegex(validator.ApprovedAcquisitionValidationError, "manifest is invalid"):
                validator.validate_approved_acquisition(repo)

    def test_seed_tamper_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Plugins/Integrations/ApprovedAcquisition/Seeds/Assets/frame.svg"
            path.write_text("tampered", encoding="utf-8")
            with self.assertRaisesRegex(validator.ApprovedAcquisitionValidationError, "source bytes drifted"):
                validator.validate_approved_acquisition(repo)

    def test_unapproved_png_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Plugins/Integrations/ApprovedAcquisition/Seeds/Assets/upstream.png"
            path.write_bytes(b"png")
            with self.assertRaisesRegex(validator.ApprovedAcquisitionValidationError, "Unapproved payload"):
                validator.validate_approved_acquisition(repo)

    def test_atomic_publication_removal_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Plugins/Integrations/ApprovedAcquisition/Tools/approved_acquisition.py"
            path.write_text(path.read_text().replace("os.replace(temporary, destination)", "destination.mkdir()"), encoding="utf-8")
            with self.assertRaisesRegex(validator.ApprovedAcquisitionValidationError, "os.replace"):
                validator.validate_approved_acquisition(repo)

    def test_stale_golden_plan_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Plugins/Integrations/ApprovedAcquisition/Tests/Fixtures/pinned-github-all.plan.json"
            path.write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(validator.ApprovedAcquisitionValidationError, "golden plan"):
                validator.validate_approved_acquisition(repo)

    def test_bridge_removal_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Tools/tests/test_approved_acquisition.py"
            path.write_text(path.read_text().replace("load_tests", "removed"), encoding="utf-8")
            with self.assertRaisesRegex(validator.ApprovedAcquisitionValidationError, "load_tests"):
                validator.validate_approved_acquisition(repo)


if __name__ == "__main__":
    unittest.main()
