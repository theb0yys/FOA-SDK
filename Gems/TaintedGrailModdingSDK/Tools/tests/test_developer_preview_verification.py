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

SCRIPT = Path(__file__).resolve().parents[1] / "developer_preview_verification.py"
SPEC = importlib.util.spec_from_file_location("tg_preview_verification", SCRIPT)
assert SPEC and SPEC.loader
verification = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = verification
SPEC.loader.exec_module(verification)


class DeveloperPreviewVerificationTests(unittest.TestCase):
    def paths(self, root: Path) -> verification.VerificationPaths:
        repo = root / "repo"
        repo.mkdir()
        return verification.VerificationPaths(
            repo_root=repo,
            build_dir=repo / "build/tg-sdk-windows-profile",
            receipt_dir=root / "tg-sdk-exact-head-receipt",
            ui_evidence_dir=repo / "build/tg-sdk-ui-evidence",
            state_path=repo / "build/tg-sdk-verification.json",
        )

    def test_path_and_identity_guards(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary) / "FOA-SDK"
            defaults = verification.default_paths(repo, "a" * 40)
            self.assertFalse(verification.is_relative_to(defaults.receipt_dir, repo))
            self.assertTrue(
                verification.is_relative_to(defaults.ui_evidence_dir, repo / "build")
            )
            verification.validate_paths(defaults)
            paths = self.paths(Path(temporary))
            unsafe = verification.VerificationPaths(
                paths.repo_root,
                paths.build_dir,
                paths.repo_root / "build/receipt",
                paths.ui_evidence_dir,
                paths.state_path,
            )
            with self.assertRaisesRegex(verification.VerificationError, "outside"):
                verification.validate_paths(unsafe)
        verification.validate_identity_inputs("windows-reviewer", "Windows 11", 125)
        with self.assertRaisesRegex(verification.VerificationError, "Tester alias"):
            verification.validate_identity_inputs("bad alias", "Windows 11", 125)
        with self.assertRaisesRegex(verification.VerificationError, "Display scale"):
            verification.validate_identity_inputs("reviewer", "Windows 11", 250)

    def test_commands_resume_and_finalize_use_existing_fail_closed_tools(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            commit = "b" * 40
            prepared = verification.prepare_steps(
                paths,
                commit,
                tester_alias="windows-reviewer",
                windows_version="Windows 11",
                display_scale=125,
            )
            self.assertEqual(
                [step.name for step in prepared],
                ["prerequisites", "initialize-receipt", "initialize-ui-evidence"],
            )
            automated = verification.automated_steps(paths)
            self.assertEqual(
                [step.name for step in automated],
                ["prerequisites-recheck", *verification.AUTOMATED_GATE_NAMES],
            )
            commands = verification.automated_gate_commands(paths)
            self.assertEqual(commands["git-diff-check"], ("git", "diff", "--check"))
            self.assertIn("run_local_validation.py", commands["local-validation"][1])
            self.assertIn("configure", commands["o3de-configure"])
            self.assertIn("build", commands["o3de-build"])
            self.assertEqual(commands["compiled-tests"][0], "ctest")
            with self.assertRaisesRegex(verification.VerificationError, "new receipt"):
                verification.automated_steps(
                    paths,
                    recorded={"git-diff-check": "failed"},
                )
            with self.assertRaisesRegex(verification.VerificationError, "must pass"):
                verification.finalize_steps(paths, commit, recorded={})
            passed = {gate: "passed" for gate in verification.AUTOMATED_GATE_NAMES}
            finalized = verification.finalize_steps(paths, commit, recorded=passed)
            self.assertEqual(finalized[0].name, "windows-ui")
            self.assertIn("developer_preview_ui_evidence.py", " ".join(finalized[0].command))
            self.assertIn("--require-merge-ready", finalized[2].command)

    def test_execution_stops_on_failure_and_dry_run_executes_nothing(self) -> None:
        calls: list[str] = []

        def executor(command, cwd):
            del cwd
            calls.append(command[0])
            return 17 if command[0] == "fail" else 0

        steps = (
            verification.VerificationStep("one", ("ok",), Path("/repo")),
            verification.VerificationStep("two", ("fail",), Path("/repo")),
            verification.VerificationStep("three", ("never",), Path("/repo")),
        )
        results, code = verification.execute_steps(steps, dry_run=False, executor=executor)
        self.assertEqual((code, calls), (17, ["ok", "fail"]))
        self.assertEqual([result.status for result in results], ["passed", "failed"])
        calls.clear()
        results, code = verification.execute_steps(steps[:1], dry_run=True, executor=executor)
        self.assertEqual((code, calls, results[0].status), (0, [], "planned"))

    def test_pending_ui_is_not_a_pass_and_state_write_is_atomic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            paths.receipt_dir.mkdir(parents=True)
            paths.ui_evidence_dir.mkdir(parents=True)
            commit = "c" * 40
            (paths.receipt_dir / verification.RECEIPT_DOCUMENT).write_text(
                json.dumps({
                    "source_commit": commit,
                    "gates": [
                        {"name": gate, "status": "passed"}
                        for gate in verification.AUTOMATED_GATE_NAMES
                    ],
                    "finalized_at_utc": None,
                }),
                encoding="utf-8",
            )
            (paths.ui_evidence_dir / verification.UI_EVIDENCE_DOCUMENT).write_text(
                json.dumps({
                    "source_commit": commit,
                    "status": "pending",
                    "checklist": [{"status": "pending"}],
                    "screenshots": [],
                }),
                encoding="utf-8",
            )
            payload = verification.state_payload(paths, commit)
            self.assertIsNone(payload["receipt"]["gates"].get("windows-ui"))
            self.assertIn("manual Windows checklist", payload["next_action"])
            verification.atomic_write_json(paths.state_path, payload)
            loaded = json.loads(paths.state_path.read_text(encoding="utf-8"))
            self.assertEqual(loaded["source_commit"], commit)
            self.assertEqual(list(paths.state_path.parent.glob("*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
