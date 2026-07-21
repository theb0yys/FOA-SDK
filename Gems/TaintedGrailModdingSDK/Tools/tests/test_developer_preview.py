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
    def make_repo(self, root: Path) -> Path:
        (root / "Gems/TaintedGrailModdingSDK").mkdir(parents=True)
        (root / "scripts/commit_validation").mkdir(parents=True)
        (root / "engine.json").write_text("{}\n", encoding="utf-8")
        (root / "CMakePresets.json").write_text("{}\n", encoding="utf-8")
        (root / "Gems/TaintedGrailModdingSDK/gem.json").write_text("{}\n", encoding="utf-8")
        for name in (
            "validate_foundation.py",
            "validate_governance_hardening.py",
            "validate_catalog_tests.py",
        ):
            (root / "Gems/TaintedGrailModdingSDK/Tools").mkdir(parents=True, exist_ok=True)
            (root / "Gems/TaintedGrailModdingSDK/Tools" / name).write_text("", encoding="utf-8")
        (root / "scripts/commit_validation/validate_file_or_folder.py").write_text("", encoding="utf-8")
        return root

    def test_parse_version_accepts_two_or_three_components(self) -> None:
        self.assertEqual(preview.parse_version("cmake version 3.28.4"), (3, 28, 4))
        self.assertEqual(preview.parse_version("git version 2.44"), (2, 44, 0))
        self.assertIsNone(preview.parse_version("unknown"))

    def test_repository_root_requires_engine_presets_and_gem(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(RuntimeError, "repository root is invalid"):
                preview.validate_repository_root(root)
            self.make_repo(root)
            preview.validate_repository_root(root)

    def test_build_directory_rejects_repository_root_and_unrelated_content(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = self.make_repo(Path(temporary) / "repo")
            with self.assertRaisesRegex(RuntimeError, "must not be the repository root"):
                preview.validate_build_directory(root, root, require_configured=False)

            build_dir = root / "build/preview"
            build_dir.mkdir(parents=True)
            (build_dir / "unrelated.txt").write_text("data", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "non-empty"):
                preview.validate_build_directory(root, build_dir, require_configured=False)

    def test_build_directory_rejects_cache_for_another_source_tree(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = self.make_repo(Path(temporary) / "repo")
            build_dir = Path(temporary) / "build"
            build_dir.mkdir()
            (build_dir / "CMakeCache.txt").write_text(
                "CMAKE_HOME_DIRECTORY:INTERNAL=C:/other/source\n", encoding="utf-8"
            )
            with self.assertRaisesRegex(RuntimeError, "another source tree"):
                preview.validate_build_directory(root, build_dir, require_configured=True)

    def test_build_directory_requires_dedicated_project_configuration(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = self.make_repo(Path(temporary) / "repo")
            build_dir = Path(temporary) / "build"
            build_dir.mkdir()
            (build_dir / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={root}\nLY_PROJECTS:STRING=\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(RuntimeError, "dedicated TaintedGrailModdingEditor"):
                preview.validate_build_directory(root, build_dir, require_configured=True)

    def test_configure_command_uses_approved_windows_x64_preset(self) -> None:
        repo = Path("C:/src/o3de")
        build = Path("D:/build/tg")
        command = preview.configure_command(repo, build)
        self.assertEqual(
            command,
            (
                "cmake",
                "--preset",
                "windows-vs-unity",
                "-S",
                str(repo),
                "-B",
                str(build),
                "-A",
                "x64",
                f"-DLY_PROJECTS={repo / 'TaintedGrailModdingEditor'}",
            ),
        )

    def test_build_command_has_fixed_profile_editor_asset_and_catalog_targets(self) -> None:
        command = preview.build_command(Path("C:/build/tg"))
        self.assertEqual(command[0:2], ("cmake", "--build"))
        self.assertIn("profile", command)
        self.assertEqual(
            command[-3:],
            ("Editor", "AssetProcessorBatch", "TaintedGrailModdingSDK.Catalog.Tests"),
        )

    def test_validation_plan_is_deterministic_and_ends_with_compiled_tests(self) -> None:
        repo = Path("/repo")
        build = Path("/build")
        plan = preview.validation_plan(repo, build)
        self.assertEqual(
            [step.name for step in plan],
            [
                "developer-preview-command-tests",
                "developer-preview-contract",
                "foundation",
                "governance-hardening",
                "catalog-contract",
                "o3de-source-policy",
                "compiled-catalog-tests",
            ],
        )
        self.assertEqual(plan[-1].command[0], "ctest")
        self.assertIn("TaintedGrailModdingSDK\\.Catalog\\.Tests", plan[-1].command)

    def test_dry_run_never_invokes_executor(self) -> None:
        calls: list[tuple[tuple[str, ...], Path]] = []

        def executor(command, cwd):
            calls.append((tuple(command), cwd))
            return 0

        plan = [preview.CommandStep("one", ("tool", "arg"), "/repo")]
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
            preview.CommandStep("first", ("ok",), "/repo"),
            preview.CommandStep("second", ("fail",), "/repo"),
            preview.CommandStep("third", ("never",), "/repo"),
        ]
        results, exit_code = preview.execute_plan(plan, dry_run=False, executor=executor)
        self.assertEqual(exit_code, 17)
        self.assertEqual(calls, ["ok", "fail"])
        self.assertEqual([result.status for result in results], ["passed", "failed"])

    def test_atomic_json_result_is_machine_readable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "results/validation.json"
            preview.atomic_write_json(output, {"status": "passed", "schema_version": 1})
            self.assertEqual(
                json.loads(output.read_text(encoding="utf-8")),
                {"schema_version": 1, "status": "passed"},
            )
            self.assertEqual(list(output.parent.glob("*.tmp")), [])

    def test_required_prerequisite_failures_control_overall_status(self) -> None:
        checks = [
            preview.CheckResult("required", "passed", True, "ok"),
            preview.CheckResult("optional", "failed", False, "not configured"),
        ]
        self.assertTrue(preview.required_checks_pass(checks))
        failed = checks + [preview.CheckResult("host", "failed", True, "wrong host")]
        self.assertFalse(preview.required_checks_pass(failed))

    def test_unsupported_host_fails_closed(self) -> None:
        with mock.patch.object(preview.platform, "system", return_value="Linux"), mock.patch.object(
            preview.platform, "machine", return_value="x86_64"
        ):
            with self.assertRaisesRegex(RuntimeError, "Windows x64 Profile"):
                preview.require_primary_host()

    def test_configure_cli_dry_run_prints_without_creating_build_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = self.make_repo(Path(temporary) / "repo")
            build_dir = Path(temporary) / "build"
            with mock.patch.object(preview.platform, "system", return_value="Windows"), mock.patch.object(
                preview.platform, "machine", return_value="AMD64"
            ):
                exit_code = preview.main(
                    [
                        "configure",
                        "--repo-root",
                        str(root),
                        "--build-dir",
                        str(build_dir),
                        "--dry-run",
                    ]
                )
            self.assertEqual(exit_code, 0)
            self.assertFalse(build_dir.exists())

    def test_validation_payload_records_failure_and_exact_step(self) -> None:
        result = preview.StepResult("catalog-contract", ("python", "validator"), "/repo", "failed", 3, 0.1)
        payload = preview.validation_payload(Path("/repo"), Path("/build"), [result], 3, dry_run=False)
        self.assertEqual(payload["status"], "failed")
        self.assertEqual(payload["exit_code"], 3)
        self.assertEqual(payload["steps"][0]["name"], "catalog-contract")


if __name__ == "__main__":
    unittest.main()
