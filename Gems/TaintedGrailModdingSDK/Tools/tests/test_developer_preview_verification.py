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
import tempfile
import unittest
import sys
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

    def write_receipt(
        self,
        paths: verification.VerificationPaths,
        commit: str,
        *,
        finalized: bool = False,
        windows_ui: bool = False,
    ) -> None:
        paths.receipt_dir.mkdir(parents=True, exist_ok=True)
        gates = [
            {"name": gate, "status": "passed"}
            for gate in verification.AUTOMATED_GATE_NAMES
        ]
        if windows_ui:
            gates.append({"name": verification.WINDOWS_UI_GATE, "status": "passed"})
        (paths.receipt_dir / verification.RECEIPT_DOCUMENT).write_text(
            json.dumps(
                {
                    "source_commit": commit,
                    "gates": gates,
                    "finalized_at_utc": "2026-07-20T18:00:00Z" if finalized else None,
                }
            ),
            encoding="utf-8",
        )

    def write_ui(
        self,
        paths: verification.VerificationPaths,
        commit: str,
        *,
        status: str = "pending",
    ) -> None:
        paths.ui_evidence_dir.mkdir(parents=True, exist_ok=True)
        (paths.ui_evidence_dir / verification.UI_EVIDENCE_DOCUMENT).write_text(
            json.dumps(
                {
                    "source_commit": commit,
                    "status": status,
                    "checklist": [{"id": "all-panes-open", "status": status}],
                    "screenshots": [],
                }
            ),
            encoding="utf-8",
        )

    def test_prerequisite_output_does_not_poison_build_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            verification.validate_paths(paths)
            self.assertFalse(
                verification.is_relative_to(
                    verification.prerequisites_path(paths),
                    paths.build_dir,
                )
            )
            step = verification.prerequisites_step(paths)
            output_index = step.command.index("--json-output") + 1
            self.assertEqual(
                step.command[output_index],
                str(verification.prerequisites_path(paths)),
            )
            self.assertNotEqual(
                Path(step.command[output_index]).parent,
                paths.build_dir,
            )

    def test_prerequisite_output_inside_build_directory_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            unsafe = verification.VerificationPaths(
                repo_root=paths.repo_root,
                build_dir=paths.build_dir,
                receipt_dir=paths.receipt_dir,
                ui_evidence_dir=paths.ui_evidence_dir,
                state_path=paths.build_dir / "verification.json",
            )
            with self.assertRaisesRegex(verification.VerificationError, "unconfigured"):
                verification.validate_paths(unsafe)

    def test_committed_diff_gate_uses_explicit_base_commit(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            base = "a" * 40
            command = verification.automated_gate_commands(
                paths,
                base,
            )["git-diff-check"]
            self.assertEqual(
                command,
                ("git", "diff", "--check", base, "HEAD"),
            )

    def test_sync_ref_must_be_ancestor_of_head(self) -> None:
        repo = Path("/repo")
        head = "a" * 40
        resolved = {"origin/main": "b" * 40}

        def fake_capture(command, cwd):
            del cwd
            if "rev-parse" in command:
                return 0, resolved[command[-1][:-9]]
            if "merge-base" in command:
                return 1, ""
            raise AssertionError(command)

        original = verification.capture_command
        verification.capture_command = fake_capture
        try:
            with self.assertRaisesRegex(verification.VerificationError, "not synchronized"):
                verification.resolve_review_ancestry(
                    repo,
                    head,
                    "origin/main",
                    ("origin/main",),
                )
        finally:
            verification.capture_command = original

    def test_prepare_refuses_nonempty_evidence_directories(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            paths.receipt_dir.mkdir(parents=True)
            (paths.receipt_dir / "old.txt").write_text("old", encoding="utf-8")
            with self.assertRaisesRegex(verification.VerificationError, "absent or empty"):
                verification.validate_prepare_targets(paths)

    def test_failed_receipt_probe_prevents_complete_status(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            commit = "c" * 40
            self.write_receipt(paths, commit, finalized=True, windows_ui=True)
            self.write_ui(paths, commit, status="pass")

            def runner(command, cwd):
                del cwd
                text = " ".join(command)
                if "validation_receipt.py" in text:
                    return 1, "captured log hash does not match"
                return 0, "UI evidence verified"

            payload = verification.state_payload(
                paths,
                commit,
                "origin/main",
                "b" * 40,
                {"origin/main": "b" * 40},
                runner=runner,
            )
            self.assertNotEqual(payload["next_action"], "complete")
            self.assertFalse(payload["receipt"]["integrity"]["valid"])

    def test_complete_requires_authoritative_receipt_and_ui_verifiers(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            commit = "d" * 40
            self.write_receipt(paths, commit, finalized=True, windows_ui=True)
            self.write_ui(paths, commit, status="pass")

            def runner(command, cwd):
                del command, cwd
                return 0, "verified"

            payload = verification.state_payload(
                paths,
                commit,
                "origin/main",
                "b" * 40,
                {"origin/main": "b" * 40},
                runner=runner,
            )
            self.assertEqual(payload["next_action"], "complete")
            self.assertTrue(payload["receipt"]["integrity"]["valid"])
            self.assertTrue(
                payload["receipt"]["merge_ready_verification"]["valid"]
            )
            self.assertTrue(payload["ui_evidence"]["verification"]["valid"])

    def test_finalized_receipt_is_reverified_instead_of_no_op(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            paths = self.paths(Path(temporary))
            commit = "e" * 40
            recorded = {
                gate: "passed"
                for gate in (*verification.AUTOMATED_GATE_NAMES, verification.WINDOWS_UI_GATE)
            }
            steps = verification.finalize_steps(
                paths,
                commit,
                recorded=recorded,
                receipt_finalized=True,
            )
            self.assertEqual(
                [step.name for step in steps],
                [
                    "verify-ui-evidence",
                    "verify-receipt",
                    "summarize-receipt",
                ],
            )
            self.assertIn("--require-merge-ready", steps[1].command)
            self.assertIn("--require-merge-ready", steps[2].command)

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
        results, code = verification.execute_steps(
            steps,
            dry_run=False,
            executor=executor,
        )
        self.assertEqual((code, calls), (17, ["ok", "fail"]))
        self.assertEqual([result.status for result in results], ["passed", "failed"])
        calls.clear()
        results, code = verification.execute_steps(
            steps[:1],
            dry_run=True,
            executor=executor,
        )
        self.assertEqual((code, calls, results[0].status), (0, [], "planned"))

    def test_atomic_state_write_is_machine_readable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "build/status.json"
            verification.atomic_write_json(
                output,
                {"schema_version": 2, "status": "ready"},
            )
            self.assertEqual(
                json.loads(output.read_text(encoding="utf-8")),
                {"schema_version": 2, "status": "ready"},
            )
            self.assertEqual(list(output.parent.glob("*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
