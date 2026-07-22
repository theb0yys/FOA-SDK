#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "developer_preview.py"
SPEC = importlib.util.spec_from_file_location("tg_developer_preview", SCRIPT_PATH)
assert SPEC and SPEC.loader
preview = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = preview
SPEC.loader.exec_module(preview)


class DeveloperPreviewCommandTests(unittest.TestCase):
    pinned_commit = "68683f23fb747380d3efa2424bd5f30242e9c5a2"

    def make_product(self, root: Path) -> Path:
        (root / "Gems/TaintedGrailModdingSDK/Tools").mkdir(parents=True)
        (root / "Gems/ExternalToolchain").mkdir(parents=True)
        (root / "TaintedGrailModdingEditor").mkdir(parents=True)
        (root / "Gems/TaintedGrailModdingSDK/gem.json").write_text("{}\n", encoding="utf-8")
        (root / "Gems/ExternalToolchain/gem.json").write_text("{}\n", encoding="utf-8")
        (root / "TaintedGrailModdingEditor/project.json").write_text("{}\n", encoding="utf-8")
        (root / "o3de.lock.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "repository": "https://github.com/o3de/o3de.git",
                    "commit": self.pinned_commit,
                    "engine_name": "o3de",
                    "engine_version": "2.7.0",
                    "checkout_directory": "o3de",
                }
            )
            + "\n",
            encoding="utf-8",
        )
        for name in (
            "validate_developer_preview.py",
            "validate_foundation.py",
            "validate_governance_hardening.py",
            "validate_catalog_tests.py",
            "validate_canonical_interchange_compiled_tests.py",
        ):
            (root / "Gems/TaintedGrailModdingSDK/Tools" / name).write_text("", encoding="utf-8")
        return root

    def make_engine(self, root: Path) -> Path:
        (root / "scripts/commit_validation").mkdir(parents=True)
        (root / "CMakePresets.json").write_text("{}\n", encoding="utf-8")
        (root / "engine.json").write_text(
            '{"engine_name":"o3de","version":"2.7.0"}\n',
            encoding="utf-8",
        )
        (root / "scripts/commit_validation/validate_file_or_folder.py").write_text(
            "", encoding="utf-8"
        )
        return root

    def test_parse_version_accepts_two_or_three_components(self) -> None:
        self.assertEqual(preview.parse_version("cmake version 3.28.4"), (3, 28, 4))
        self.assertEqual(preview.parse_version("git version 2.44"), (2, 44, 0))
        self.assertIsNone(preview.parse_version("unknown"))

    def test_product_root_requires_lock_project_and_custom_gems(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(RuntimeError, "product root is invalid"):
                preview.validate_product_root(root)
            self.make_product(root)
            preview.validate_product_root(root)

    def test_engine_lock_requires_full_commit_and_expected_schema(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            product = self.make_product(Path(temporary) / "FOA-SDK")
            lock = preview.load_engine_lock(product)
            self.assertEqual(lock.commit, self.pinned_commit)
            payload = json.loads((product / "o3de.lock.json").read_text(encoding="utf-8"))
            payload["commit"] = "abc"
            (product / "o3de.lock.json").write_text(json.dumps(payload), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "40-character Git SHA"):
                preview.load_engine_lock(product)

    def test_engine_root_requires_descriptor_presets_validator_and_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            product = self.make_product(Path(temporary) / "FOA-SDK")
            lock = preview.load_engine_lock(product)
            engine = Path(temporary) / "o3de"
            with self.assertRaisesRegex(RuntimeError, "engine root is invalid"):
                preview.validate_engine_root(engine, lock)
            self.make_engine(engine)
            preview.validate_engine_root(engine, lock)
            (engine / "engine.json").write_text(
                '{"engine_name":"wrong","version":"2.7.0"}\n', encoding="utf-8"
            )
            with self.assertRaisesRegex(RuntimeError, "engine name mismatch"):
                preview.validate_engine_root(engine, lock)

    def test_default_engine_root_prefers_environment_then_sibling(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            product = self.make_product(base / "FOA-SDK")
            sibling = self.make_engine(base / "o3de")
            lock = preview.load_engine_lock(product)
            self.assertEqual(preview.default_engine_root(product, lock), sibling.resolve())
            explicit = base / "custom-engine"
            with mock.patch.dict(os.environ, {preview.ENGINE_ROOT_ENVIRONMENT_VARIABLE: str(explicit)}):
                self.assertEqual(preview.default_engine_root(product, lock), explicit.resolve())

    def test_build_directory_rejects_product_engine_and_unrelated_content(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            product = self.make_product(base / "FOA-SDK")
            engine = self.make_engine(base / "o3de")
            with self.assertRaisesRegex(RuntimeError, "separate from product_root"):
                preview.validate_build_directory(
                    product, engine, product, require_configured=False
                )
            with self.assertRaisesRegex(RuntimeError, "separate from product_root"):
                preview.validate_build_directory(
                    product, engine, engine, require_configured=False
                )
            with self.assertRaisesRegex(RuntimeError, "inside the FOA-SDK checkout"):
                preview.validate_build_directory(
                    product, engine, product / "build", require_configured=False
                )
            build_dir = base / "build"
            build_dir.mkdir()
            (build_dir / "unrelated.txt").write_text("data", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "non-empty"):
                preview.validate_build_directory(
                    product, engine, build_dir, require_configured=False
                )

    def test_build_directory_rejects_cache_for_another_engine_tree(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            product = self.make_product(base / "FOA-SDK")
            engine = self.make_engine(base / "o3de")
            build_dir = base / "build"
            build_dir.mkdir()
            (build_dir / "CMakeCache.txt").write_text(
                "CMAKE_HOME_DIRECTORY:INTERNAL=C:/other/source\n", encoding="utf-8"
            )
            with self.assertRaisesRegex(RuntimeError, "another engine source tree"):
                preview.validate_build_directory(
                    product, engine, build_dir, require_configured=True
                )

    def test_build_directory_requires_dedicated_external_project_configuration(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            product = self.make_product(base / "FOA-SDK")
            engine = self.make_engine(base / "o3de")
            build_dir = base / "build"
            build_dir.mkdir()
            (build_dir / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={engine}\nLY_PROJECTS:STRING=\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(RuntimeError, "dedicated TaintedGrailModdingEditor"):
                preview.validate_build_directory(
                    product, engine, build_dir, require_configured=True
                )

    def test_engine_pin_fails_closed_for_wrong_checkout(self) -> None:
        lock = preview.EngineLock(
            "https://github.com/o3de/o3de.git",
            self.pinned_commit,
            "o3de",
            "2.7.0",
            "o3de",
        )
        with mock.patch.object(
            preview, "engine_checkout_commit", return_value=(0, "0" * 40)
        ):
            with self.assertRaisesRegex(RuntimeError, "commit mismatch"):
                preview.validate_engine_pin(Path("/engine"), lock)

    def test_configure_command_uses_external_engine_and_product_project(self) -> None:
        product = Path("C:/src/FOA-SDK")
        engine = Path("C:/src/o3de")
        build = Path("D:/build/tg")
        command = preview.configure_command(product, engine, build)
        self.assertEqual(
            command,
            (
                "cmake",
                "--preset",
                "windows-vs-unity",
                "-S",
                str(engine),
                "-B",
                str(build),
                "-A",
                "x64",
                f"-DLY_PROJECTS={product / 'TaintedGrailModdingEditor'}",
            ),
        )

    def test_build_command_has_fixed_profile_editor_asset_and_catalog_targets(self) -> None:
        command = preview.build_command(Path("C:/build/tg"))
        self.assertEqual(command[0:2], ("cmake", "--build"))
        self.assertIn("profile", command)
        self.assertEqual(
            command[-4:],
            (
                "Editor",
                "AssetProcessorBatch",
                "TaintedGrailModdingSDK.Catalog.Tests",
                "TaintedGrailModdingSDK.CanonicalInterchange.Tests",
            ),
        )

    def test_validation_plan_separates_product_and_engine_tools(self) -> None:
        product = Path("/product")
        engine = Path("/engine")
        build = Path("/build")
        plan = preview.validation_plan(product, engine, build)
        self.assertEqual(
            [step.name for step in plan],
            [
                "developer-preview-command-tests",
                "developer-preview-contract",
                "foundation",
                "governance-hardening",
                "catalog-contract",
                "canonical-interchange-contract",
                "o3de-source-policy",
                "compiled-catalog-and-canonical-interchange-tests",
            ],
        )
        source_policy = next(step for step in plan if step.name == "o3de-source-policy")
        self.assertIn(str(engine / "scripts/commit_validation/validate_file_or_folder.py"), source_policy.command)
        self.assertIn(str(product / "Gems/TaintedGrailModdingSDK"), source_policy.command)
        compiled = next(step for step in plan if step.name == "compiled-catalog-and-canonical-interchange-tests")
        self.assertIn("--no-tests=error", compiled.command)
        self.assertIn(preview.CATALOG_TEST_PATTERN, compiled.command)
        self.assertTrue(all(step.cwd == str(product) for step in plan))

    def test_dry_run_never_invokes_executor(self) -> None:
        calls: list[tuple[tuple[str, ...], Path]] = []

        def executor(command, cwd):
            calls.append((tuple(command), cwd))
            return 0

        plan = [preview.CommandStep("one", ("tool", "arg"), "/product")]
        results, exit_code = preview.execute_plan(plan, dry_run=True, executor=executor)
        self.assertEqual(exit_code, 0)
        self.assertEqual(calls, [])
        self.assertEqual(results[0].status, "planned")
        self.assertIsNone(results[0].exit_code)

    def test_execution_stops_and_propagates_original_exit_code(self) -> None:
        calls: list[str] = []

        def executor(command, cwd):
            calls.append(command[0])
            return 17 if command[0] == "fail" else 0

        plan = [
            preview.CommandStep("first", ("ok",), "/product"),
            preview.CommandStep("second", ("fail",), "/product"),
            preview.CommandStep("third", ("never",), "/product"),
        ]
        results, exit_code = preview.execute_plan(plan, dry_run=False, executor=executor)
        self.assertEqual(exit_code, 17)
        self.assertEqual(calls, ["ok", "fail"])
        self.assertEqual([result.status for result in results], ["passed", "failed"])

    def test_atomic_json_result_is_machine_readable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "results/validation.json"
            preview.atomic_write_json(output, {"status": "passed", "schema_version": 2})
            self.assertEqual(
                json.loads(output.read_text(encoding="utf-8")),
                {"schema_version": 2, "status": "passed"},
            )
            self.assertEqual(list(output.parent.glob("*.tmp")), [])

    def test_payload_records_all_three_roots_and_pinned_commit(self) -> None:
        lock = preview.EngineLock(
            "https://github.com/o3de/o3de.git",
            self.pinned_commit,
            "o3de",
            "2.7.0",
            "o3de",
        )
        result = preview.StepResult(
            "catalog-contract", ("python", "validator"), "/product", "failed", 3, 0.1
        )
        payload = preview.validation_payload(
            Path("/product"),
            Path("/engine"),
            Path("/build"),
            lock,
            [result],
            3,
            dry_run=False,
        )
        self.assertEqual(payload["product_root"], str(Path("/product")))
        self.assertEqual(payload["engine_root"], str(Path("/engine")))
        self.assertEqual(payload["build_root"], str(Path("/build")))
        self.assertEqual(payload["engine_commit"], self.pinned_commit)
        self.assertEqual(payload["status"], "failed")

    def test_unsupported_host_fails_closed(self) -> None:
        with mock.patch.object(preview.platform, "system", return_value="Linux"), mock.patch.object(
            preview.platform, "machine", return_value="x86_64"
        ):
            with self.assertRaisesRegex(RuntimeError, "Windows x64 Profile"):
                preview.require_primary_host()


if __name__ == "__main__":
    unittest.main()
