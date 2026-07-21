#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import os
import sys
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path
from unittest import mock


TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from run_local_validation import (  # noqa: E402
    TOOLS_ROOT as REAL_TOOLS_ROOT,
    VALIDATORS,
    ValidationCommand,
    ValidationConfigurationError,
    build_ctest_command,
    build_static_commands,
    find_ctest,
    run_validation_pipeline,
    should_run_stage,
    validation_mode,
)
from validate_ci_runner_policy import (  # noqa: E402
    AUTOMATIC_STATIC_WORKFLOW,
    CiRunnerPolicyError,
    MANUAL_WORKFLOWS,
    validate_ci_runner_policy,
)


class CiRunnerPolicyTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        for relative_path in MANUAL_WORKFLOWS:
            path = repo / relative_path
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                "name: Manual validation\n"
                "# Automatic triggers are intentionally suspended\n"
                "on:\n"
                "  workflow_dispatch:\n"
                "jobs:\n"
                "  validate:\n"
                "    runs-on: ubuntu-latest\n",
                encoding="utf-8",
            )

        automatic = repo / AUTOMATIC_STATIC_WORKFLOW
        automatic.parent.mkdir(parents=True, exist_ok=True)
        automatic.write_text(
            "name: Tainted Grail SDK PR Static Validation\n"
            "on:\n"
            "  pull_request:\n"
            "  push:\n"
            "  workflow_dispatch:\n"
            "permissions:\n"
            "  contents: read\n"
            "jobs:\n"
            "  static-validation:\n"
            "    runs-on: ubuntu-latest\n"
            "    timeout-minutes: 45\n"
            "    steps:\n"
            "      - run: python "
            "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py "
            "--keep-going --static-only --skip-source-policy\n",
            encoding="utf-8",
        )

        local_runner = (
            repo
            / "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py"
        )
        local_runner.parent.mkdir(parents=True, exist_ok=True)
        local_runner.write_text(
            '"--static-only"\n'
            '"--ctest-build-dir"\n'
            '"--no-tests=error"\n'
            "def run_validation_pipeline():\n"
            "    pass\n"
            "def should_run_stage():\n"
            "    pass\n"
            "Pinned O3DE source policy, "
            "compiled tests, and Windows acceptance remain mandatory exact-head "
            "gates\n",
            encoding="utf-8",
        )

        policy = repo / "docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md"
        policy.parent.mkdir(parents=True, exist_ok=True)
        policy.write_text(
            "run_local_validation.py automatic pull-request static validation "
            "--static-only --ctest-build-dir compiled Catalog CTest "
            "self-hosted runner registration token does not claim an O3DE build "
            "Pending is not passing\n",
            encoding="utf-8",
        )
        return repo

    def test_split_ci_policy_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_ci_runner_policy(self.make_repo(Path(temporary)))

    def test_automatic_pull_request_trigger_is_rejected_on_manual_workflow(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / MANUAL_WORKFLOWS[0]
            path.write_text(
                path.read_text(encoding="utf-8") + "pull_request:\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(CiRunnerPolicyError, "pull_request"):
                validate_ci_runner_policy(repo)

    def test_self_hosted_runner_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / AUTOMATIC_STATIC_WORKFLOW
            path.write_text(
                path.read_text(encoding="utf-8").replace(
                    "ubuntu-latest",
                    "self-hosted",
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(CiRunnerPolicyError, "ubuntu-latest|self-hosted"):
                validate_ci_runner_policy(repo)

    def test_automatic_static_workflow_must_use_explicit_static_only_mode(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / AUTOMATIC_STATIC_WORKFLOW
            path.write_text(
                path.read_text(encoding="utf-8").replace(
                    " --static-only",
                    "",
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(CiRunnerPolicyError, "static-only"):
                validate_ci_runner_policy(repo)

    def test_inherited_automatic_workflow_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / ".github/workflows/ar.yml"
            path.write_text("on: pull_request\n", encoding="utf-8")
            with self.assertRaisesRegex(CiRunnerPolicyError, "remain removed"):
                validate_ci_runner_policy(repo)

    def test_missing_token_guidance_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            policy = repo / "docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md"
            policy.write_text(
                policy.read_text(encoding="utf-8").replace(
                    "registration token",
                    "runner setup",
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(CiRunnerPolicyError, "registration token"):
                validate_ci_runner_policy(repo)


class LocalValidationEntrypointTests(unittest.TestCase):
    def test_validator_inventory_is_unique_and_present(self) -> None:
        self.assertEqual(len(VALIDATORS), len(set(VALIDATORS)))
        self.assertIn("validate_population_actor_troop_editor.py", VALIDATORS)
        self.assertIn("validate_tainted_framework_editor_services.py", VALIDATORS)
        for validator in VALIDATORS:
            self.assertTrue((REAL_TOOLS_ROOT / validator).is_file(), validator)

    def test_static_commands_use_argument_vectors(self) -> None:
        commands = build_static_commands(
            include_unit_tests=True,
            source_policy_engine_root=Path("/external/o3de"),
        )
        self.assertGreater(len(commands), len(VALIDATORS))
        self.assertTrue(all(command.argv for command in commands))
        self.assertTrue(all(isinstance(command.argv, tuple) for command in commands))
        self.assertIn("Python unit tests", commands[0].label)
        self.assertEqual(commands[-1].label, "O3DE source policy: TaintedGrailModdingSDK")
        self.assertEqual(commands[-1].cwd, Path("/external/o3de"))

    def test_ctest_is_derived_from_configured_cmake_when_not_on_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cmake = root / "tools" / "cmake.exe"
            ctest = root / "tools" / ("ctest.exe" if os.name == "nt" else "ctest")
            cmake.parent.mkdir()
            cmake.touch()
            ctest.touch()
            build = root / "build"
            build.mkdir()
            (build / "CMakeCache.txt").write_text(
                f"CMAKE_COMMAND:INTERNAL={cmake.as_posix()}\n",
                encoding="utf-8",
            )
            with mock.patch("run_local_validation.shutil.which", return_value=None):
                self.assertEqual(find_ctest(build), str(ctest))

    def test_full_validation_requires_ctest_or_explicit_static_only(self) -> None:
        with self.assertRaisesRegex(
            ValidationConfigurationError,
            "--ctest-build-dir",
        ):
            validation_mode(
                Namespace(
                    static_only=False,
                    ctest_build_dir=None,
                    skip_source_policy=False,
                )
            )
        self.assertEqual(
            validation_mode(
                Namespace(
                    static_only=True,
                    ctest_build_dir=None,
                    skip_source_policy=True,
                )
            ),
            "static-only",
        )
        self.assertEqual(
            validation_mode(
                Namespace(
                    static_only=False,
                    ctest_build_dir=Path("build"),
                    skip_source_policy=False,
                )
            ),
            "full",
        )
        with self.assertRaisesRegex(
            ValidationConfigurationError,
            "cannot skip",
        ):
            validation_mode(
                Namespace(
                    static_only=False,
                    ctest_build_dir=Path("build"),
                    skip_source_policy=True,
                )
            )

    def test_compiled_ctest_fails_when_regex_matches_no_tests(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary)
            (build / "CMakeCache.txt").write_text("cache\n", encoding="utf-8")
            (build / "CTestTestfile.cmake").write_text("tests\n", encoding="utf-8")
            with mock.patch(
                "run_local_validation.find_ctest",
                return_value="ctest",
            ):
                command = build_ctest_command(build)
        self.assertIn("--no-tests=error", command.argv)
        self.assertIn("TaintedGrailModdingSDK.Catalog.Tests", command.argv)

    def test_keep_going_runs_static_fixtures_and_compiled_stages(self) -> None:
        arguments = Namespace(
            static_only=False,
            ctest_build_dir=Path("build"),
            skip_unit_tests=False,
            skip_source_policy=False,
            skip_fixtures=False,
            keep_going=True,
        )
        command = ValidationCommand("fixture", ("true",))
        with (
            mock.patch(
                "run_local_validation.build_static_commands",
                return_value=[command],
            ),
            mock.patch(
                "run_local_validation.build_fixture_commands",
                return_value=[command],
            ),
            mock.patch(
                "run_local_validation.build_ctest_command",
                return_value=command,
            ),
            mock.patch(
                "run_local_validation.run_commands",
                side_effect=[
                    ["static failed"],
                    ["fixture failed"],
                    ["compiled failed"],
                ],
            ) as run,
        ):
            failures = run_validation_pipeline(arguments, Path("/external/o3de"))
        self.assertEqual(run.call_count, 3)
        self.assertEqual(
            failures,
            ["static failed", "fixture failed", "compiled failed"],
        )

    def test_non_keep_going_stops_after_first_failed_stage(self) -> None:
        arguments = Namespace(
            static_only=False,
            ctest_build_dir=Path("build"),
            skip_unit_tests=False,
            skip_source_policy=False,
            skip_fixtures=False,
            keep_going=False,
        )
        command = ValidationCommand("fixture", ("true",))
        with (
            mock.patch(
                "run_local_validation.build_static_commands",
                return_value=[command],
            ),
            mock.patch(
                "run_local_validation.run_commands",
                return_value=["static failed"],
            ) as run,
        ):
            failures = run_validation_pipeline(arguments, Path("/external/o3de"))
        self.assertEqual(run.call_count, 1)
        self.assertEqual(failures, ["static failed"])

    def test_stage_policy_only_continues_after_failure_with_keep_going(self) -> None:
        self.assertTrue(should_run_stage([], keep_going=False))
        self.assertFalse(should_run_stage(["failed"], keep_going=False))
        self.assertTrue(should_run_stage(["failed"], keep_going=True))


if __name__ == "__main__":
    unittest.main()
