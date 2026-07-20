#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from run_local_validation import (  # noqa: E402
    TOOLS_ROOT as REAL_TOOLS_ROOT,
    VALIDATORS,
    build_static_commands,
)
from validate_ci_runner_policy import (  # noqa: E402
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

        local_runner = (
            repo
            / "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py"
        )
        local_runner.parent.mkdir(parents=True, exist_ok=True)
        local_runner.write_text("local runner\n", encoding="utf-8")

        policy = repo / "docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md"
        policy.parent.mkdir(parents=True, exist_ok=True)
        policy.write_text(
            "manual-only run_local_validation.py self-hosted runner "
            "registration token No automated per-commit test result is claimed\n",
            encoding="utf-8",
        )
        return repo

    def test_manual_only_policy_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_ci_runner_policy(self.make_repo(Path(temporary)))

    def test_automatic_pull_request_trigger_is_rejected(self) -> None:
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
            path = repo / MANUAL_WORKFLOWS[1]
            path.write_text(
                path.read_text(encoding="utf-8").replace(
                    "ubuntu-latest",
                    "self-hosted",
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(CiRunnerPolicyError, "self-hosted"):
                validate_ci_runner_policy(repo)

    def test_inherited_automatic_workflow_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / ".github/workflows/ar.yml"
            path.write_text("on: pull_request\n", encoding="utf-8")
            with self.assertRaisesRegex(CiRunnerPolicyError, "must remain removed"):
                validate_ci_runner_policy(repo)

    def test_missing_token_guidance_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            policy = repo / "docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md"
            policy.write_text(
                "manual-only run_local_validation.py self-hosted runner "
                "No automated per-commit test result is claimed\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(CiRunnerPolicyError, "registration token"):
                validate_ci_runner_policy(repo)


class LocalValidationEntrypointTests(unittest.TestCase):
    def test_validator_inventory_is_unique_and_present(self) -> None:
        self.assertEqual(len(VALIDATORS), len(set(VALIDATORS)))
        for validator in VALIDATORS:
            self.assertTrue((REAL_TOOLS_ROOT / validator).is_file(), validator)

    def test_static_commands_use_argument_vectors(self) -> None:
        commands = build_static_commands(
            include_unit_tests=True,
            include_source_policy=True,
        )
        self.assertGreater(len(commands), len(VALIDATORS))
        self.assertTrue(all(command.argv for command in commands))
        self.assertTrue(all(isinstance(command.argv, tuple) for command in commands))
        self.assertIn("Python unit tests", commands[0].label)
        self.assertEqual(commands[-1].label, "O3DE source policy")


if __name__ == "__main__":
    unittest.main()
