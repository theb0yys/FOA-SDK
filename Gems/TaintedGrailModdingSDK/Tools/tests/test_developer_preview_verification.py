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

SCRIPT_PATH = Path(__file__).resolve().parent / "developer_preview_verification.py"
SPEC = importlib.util.spec_from_file_location("tg_developer_preview_verification", SCRIPT_PATH)
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
            build_dir=repo / "build/tg-sdk-developer-preview-0-windows-profile",
            receipt_dir=root / "tg-sdk-exact-head-receipt-0123456789ab",
            ui_evidence_dir=repo / "build/tg-sdk-developer-preview-0-ui-evidence",
            state_path=repo / "build/tg-sdk-developer-preview-0-verification.json",
        )

    def test_default_receipt_is_outside_repository_and_evidence_is_under_build(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary) / "FOA-SDK"
            commit = "a" * 40
            paths = verification.default_paths(repo, commit)
            self.assertFalse(verification.is_relative_to(paths.receipt_dir, repo))
            self.assertTrue(
                verification.is_relative_to(paths.ui_evidence_dir, repo / "build")
            )
            verification.validate_paths(paths)

    def test_receipt_inside_repository_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            invalid = verification.VerificationPaths(
                repo_root=paths.repo_root,
                build_dir=paths.build_dir,
                receipt_dir=paths.repo_root / "build/receipt",
                ui_evidence_dir=paths.ui_evidence_dir,
                state_path=paths.state_path,
            )
            with self.assertRaisesRegex(verification.VerificationError, "outside"):
                verification.validate_paths(invalid)



    def test_identity_inputs_fail_closed_before_evidence_initialization(self) -> None:
        verification.validate_identity_inputs(
            "windows-reviewer", "Windows 11 23H2", 125
        )
        with self.assertRaisesRegex(verification.VerificationError, "Tester alias"):
            verification.validate_identity_inputs("bad alias", "Windows 11", 125)
        with self.assertRaisesRegex(verification.VerificationError, "Windows version"):
            verification.validate_identity_inputs("reviewer", "Windows 11\nprivate", 125)
        with self.assertRaisesRegex(verification.VerificationError, "Display scale"):
            verification.validate_identity_inputs("reviewer", "Windows 11", 250)

    def test_prepare_refuses_nonempty_evidence_directories(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            paths.receipt_dir.mkdir(parents=True)
            (paths.receipt_dir / "old.txt").write_text("old", encoding="utf-8")
            with self.assertRaisesRegex(verification.VerificationError, "absent or empty"):
                verification.validate_prepare_targets(paths)

    def test_prepare_uses_exact_commit_and_existing_tools(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            commit = "b" * 40
            steps = verification.prepare_steps(
                paths,
                commit,
                tester_alias="windows-reviewer",
                windows_version="Windows 11 23H2",
                display_scale=125,
            )
            self.assertEqual(
                [step.name for step in steps],
                ["prerequisites", "initialize-receipt", "initialize-ui-evidence"],
            )
            self.assertIn("developer_preview.py", steps[0].command[1])
            self.assertIn("validation_receipt.py", steps[1].command[1])
            self.assertIn("developer_preview_ui_evidence.py", steps[2].command[1])
            self.assertIn(commit, steps[2].command)
            self.assertIn("125", steps[2].command)

    def test_automated_gates_wrap_exact_existing_commands_in_order(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            steps = verification.automated_steps(paths)
            self.assertEqual(
                [step.name for step in steps],
                ["prerequisites-recheck", *verification.AUTOMATED_GATE_NAMES],
            )
            gate_commands = verification.automated_gate_commands(paths)
            self.assertEqual(gate_commands["git-diff-check"], ("git", "diff", "--check"))
            self.assertIn("run_local_validation.py", gate_commands["local-validation"][1])
            self.assertNotIn("--ctest-build-dir", gate_commands["local-validation"])
            self.assertIn("developer_preview.py", gate_commands["o3de-configure"][1])
            self.assertIn("configure", gate_commands["o3de-configure"])
            self.assertIn("build", gate_commands["o3de-build"])
            self.assertEqual(gate_commands["compiled-tests"][0], "ctest")
            self.assertIn(verification.CATALOG_TEST_PATTERN, gate_commands["compiled-tests"])
            for step in steps[1:]:
                self.assertIn("validation_receipt.py", step.command[1])
                self.assertIn("record", step.command)
                self.assertIn("--", step.command)

    def test_automated_resume_skips_only_passed_gates(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            steps = verification.automated_steps(
                paths,
                recorded={
                    "git-diff-check": "passed",
                    "local-validation": "passed",
                },
            )
            self.assertEqual(
                [step.name for step in steps],
                ["prerequisites-recheck", "o3de-configure", "o3de-build", "compiled-tests"],
            )
            with self.assertRaisesRegex(verification.VerificationError, "new receipt"):
                verification.automated_steps(
                    paths,
                    recorded={"git-diff-check": "failed"},
                )

    def test_finalize_records_real_ui_verifier_then_finalizes_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            commit = "c" * 40
            recorded = {gate: "passed" for gate in verification.AUTOMATED_GATE_NAMES}
            steps = verification.finalize_steps(paths, commit, recorded=recorded)
            self.assertEqual(
                [step.name for step in steps],
                [
                    "windows-ui",
                    "finalize-receipt",
                    "verify-receipt",
                    "summarize-receipt",
                ],
            )
            windows_step = steps[0]
            self.assertIn("validation_receipt.py", windows_step.command[1])
            self.assertTrue(any(value.endswith("developer_preview_ui_evidence.py") for value in windows_step.command))
            self.assertIn("verify", windows_step.command)
            self.assertIn(commit, windows_step.command)
            self.assertIn("--require-merge-ready", steps[2].command)
            self.assertIn("--require-merge-ready", steps[3].command)

    def test_finalize_refuses_missing_or_failed_mandatory_gate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            with self.assertRaisesRegex(verification.VerificationError, "must pass"):
                verification.finalize_steps(paths, "d" * 40, recorded={})
            recorded = {gate: "passed" for gate in verification.AUTOMATED_GATE_NAMES}
            recorded["compiled-tests"] = "failed"
            with self.assertRaisesRegex(verification.VerificationError, "compiled-tests"):
                verification.finalize_steps(paths, "d" * 40, recorded=recorded)

    def test_execution_stops_on_first_failure_and_preserves_exit_code(self) -> None:
        calls: list[str] = []

        def executor(command, cwd):
            calls.append(command[0])
            return 19 if command[0] == "fail" else 0

        steps = (
            verification.VerificationStep("one", ("ok",), Path("/repo")),
            verification.VerificationStep("two", ("fail",), Path("/repo")),
            verification.VerificationStep("three", ("never",), Path("/repo")),
        )
        results, exit_code = verification.execute_steps(
            steps,
            dry_run=False,
            executor=executor,
        )
        self.assertEqual(exit_code, 19)
        self.assertEqual(calls, ["ok", "fail"])
        self.assertEqual([result.status for result in results], ["passed", "failed"])

    def test_dry_run_never_executes_processes(self) -> None:
        calls: list[tuple[tuple[str, ...], Path]] = []

        def executor(command, cwd):
            calls.append((tuple(command), cwd))
            return 0

        results, exit_code = verification.execute_steps(
            (verification.VerificationStep("one", ("tool",), Path("/repo")),),
            dry_run=True,
            executor=executor,
        )
        self.assertEqual(exit_code, 0)
        self.assertEqual(calls, [])
        self.assertEqual(results[0].status, "planned")

    def test_status_never_treats_pending_ui_evidence_as_passed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            paths.receipt_dir.mkdir(parents=True)
            paths.ui_evidence_dir.mkdir(parents=True)
            commit = "e" * 40
            (paths.receipt_dir / verification.RECEIPT_DOCUMENT).write_text(
                json.dumps(
                    {
                        "source_commit": commit,
                        "gates": [
                            {"name": name, "status": "passed"}
                            for name in verification.AUTOMATED_GATE_NAMES
                        ],
                        "finalized_at_utc": None,
                    }
                ),
                encoding="utf-8",
            )
            (paths.ui_evidence_dir / verification.UI_EVIDENCE_DOCUMENT).write_text(
                json.dumps(
                    {
                        "source_commit": commit,
                        "status": "pending",
                        "checklist": [{"status": "pending"}],
                        "screenshots": [],
                    }
                ),
                encoding="utf-8",
            )
            payload = verification.state_payload(paths, commit)
            self.assertEqual(payload["receipt"]["gates"].get("windows-ui"), None)
            self.assertIn("manual Windows checklist", payload["next_action"])
            self.assertEqual(payload["ui_evidence"]["checklist_counts"]["pending"], 1)
            self.assertEqual(payload["ui_evidence"]["pending_checks"], ["unknown"])

    def test_atomic_state_is_machine_readable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "build/status.json"
            verification.atomic_write_json(output, {"schema_version": 1, "status": "ready"})
            self.assertEqual(
                json.loads(output.read_text(encoding="utf-8")),
                {"schema_version": 1, "status": "ready"},
            )
            self.assertEqual(list(output.parent.glob("*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
