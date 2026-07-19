#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


TOOLS_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = TOOLS_ROOT / "validation_receipt.py"
COMMIT = "a" * 40
OTHER_COMMIT = "b" * 40


class ValidationReceiptTests(unittest.TestCase):
    def run_tool(
        self,
        *arguments: str,
        expected: int = 0,
    ) -> subprocess.CompletedProcess[str]:
        completed = subprocess.run(
            [sys.executable, str(SCRIPT), *arguments],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            completed.returncode,
            expected,
            msg=f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}",
        )
        return completed

    def initialize(self, output: Path) -> None:
        self.run_tool(
            "init",
            "--output",
            str(output),
            "--source-commit",
            COMMIT,
            "--tester-alias",
            "test-reviewer",
            "--platform",
            "Windows 11 x64",
            "--configuration",
            "profile",
            "--created-at",
            "2026-07-19T10:00:00Z",
        )

    def record_passed(
        self,
        output: Path,
        gate: str,
        minute: int,
        log: Path | None = None,
    ) -> None:
        arguments = [
            "record",
            "--output",
            str(output),
            "--name",
            gate,
            "--command",
            f"synthetic {gate}",
            "--status",
            "passed",
            "--exit-code",
            "0",
            "--started-at",
            f"2026-07-19T10:{minute:02d}:00Z",
            "--finished-at",
            f"2026-07-19T10:{minute:02d}:30Z",
            "--notes",
            "Synthetic unit-test pass.",
        ]
        if log is not None:
            arguments.extend(["--stdout-log", str(log)])
        self.run_tool(*arguments)

    def record_not_run(self, output: Path, gate: str) -> None:
        self.run_tool(
            "record",
            "--output",
            str(output),
            "--name",
            gate,
            "--command",
            f"synthetic {gate}",
            "--status",
            "not-run",
            "--notes",
            "Unavailable in the synthetic unit-test environment.",
        )

    def accept_risk(self, output: Path, gate: str, minute: int) -> None:
        self.run_tool(
            "accept-risk",
            "--output",
            str(output),
            "--gate",
            gate,
            "--maintainer-alias",
            "test-maintainer",
            "--accepted-at",
            f"2026-07-19T11:{minute:02d}:00Z",
            "--rationale",
            "Synthetic unit-test acceptance for a deliberately not-run gate.",
        )

    def make_merge_ready(self, output: Path, log: Path | None = None) -> None:
        self.initialize(output)
        self.record_passed(output, "git-diff-check", 1, log)
        self.record_passed(output, "local-validation", 2)
        for gate in (
            "o3de-configure",
            "o3de-build",
            "compiled-tests",
            "windows-ui",
        ):
            self.record_not_run(output, gate)
        for minute, gate in enumerate(
            (
                "o3de-configure",
                "o3de-build",
                "compiled-tests",
                "windows-ui",
            ),
            start=1,
        ):
            self.accept_risk(output, gate, minute)
        self.run_tool(
            "finalize",
            "--output",
            str(output),
            "--expected-commit",
            COMMIT,
            "--finalized-at",
            "2026-07-19T12:00:00Z",
        )

    def test_merge_ready_receipt_verifies_and_summarizes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "receipt"
            log = root / "local.log"
            log.write_text("validation passed\n", encoding="utf-8")
            self.make_merge_ready(output, log)

            self.run_tool(
                "verify",
                "--output",
                str(output),
                "--expected-commit",
                COMMIT,
                "--require-merge-ready",
            )
            summary = self.run_tool(
                "summarize",
                "--output",
                str(output),
                "--expected-commit",
                COMMIT,
                "--require-merge-ready",
            )
            self.assertIn("## Exact-head validation receipt", summary.stdout)
            self.assertIn("`local-validation`", summary.stdout)
            self.assertIn("Maintainer risk acceptances", summary.stdout)

    def test_tampered_log_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "receipt"
            log = root / "local.log"
            log.write_text("validation passed\n", encoding="utf-8")
            self.make_merge_ready(output, log)
            copied = output / "logs/git-diff-check.stdout.log"
            copied.write_text("tampered\n", encoding="utf-8")

            result = self.run_tool(
                "verify",
                "--output",
                str(output),
                "--expected-commit",
                COMMIT,
                "--require-merge-ready",
                expected=1,
            )
            self.assertIn("hash or byte count does not match", result.stderr)

    def test_wrong_expected_commit_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "receipt"
            self.make_merge_ready(output)
            result = self.run_tool(
                "verify",
                "--output",
                str(output),
                "--expected-commit",
                OTHER_COMMIT,
                "--require-merge-ready",
                expected=1,
            )
            self.assertIn("does not match", result.stderr)

    def test_mandatory_local_validation_cannot_be_waived(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "receipt"
            self.initialize(output)
            self.record_passed(output, "git-diff-check", 1)
            self.record_not_run(output, "local-validation")
            for gate in (
                "o3de-configure",
                "o3de-build",
                "compiled-tests",
                "windows-ui",
            ):
                self.record_not_run(output, gate)
            result = self.run_tool(
                "finalize",
                "--output",
                str(output),
                "--expected-commit",
                COMMIT,
                "--finalized-at",
                "2026-07-19T12:00:00Z",
                expected=1,
            )
            self.assertIn("local-validation must pass", result.stderr)

    def test_skipped_host_gate_requires_maintainer_acceptance(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "receipt"
            self.initialize(output)
            self.record_passed(output, "git-diff-check", 1)
            self.record_passed(output, "local-validation", 2)
            for gate in (
                "o3de-configure",
                "o3de-build",
                "compiled-tests",
                "windows-ui",
            ):
                self.record_not_run(output, gate)
            result = self.run_tool(
                "finalize",
                "--output",
                str(output),
                "--expected-commit",
                COMMIT,
                "--finalized-at",
                "2026-07-19T12:00:00Z",
                expected=1,
            )
            self.assertIn("explicit maintainer risk acceptance", result.stderr)

    def test_finalized_receipt_is_immutable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "receipt"
            self.make_merge_ready(output)
            result = self.run_tool(
                "record",
                "--output",
                str(output),
                "--name",
                "git-diff-check",
                "--command",
                "synthetic rerun",
                "--status",
                "passed",
                "--exit-code",
                "0",
                "--started-at",
                "2026-07-19T13:00:00Z",
                "--finished-at",
                "2026-07-19T13:00:01Z",
                expected=1,
            )
            self.assertIn("immutable", result.stderr)

    def test_receipt_json_uses_exact_schema(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "receipt"
            self.initialize(output)
            receipt = json.loads(
                (output / "validation-receipt.json").read_text(encoding="utf-8")
            )
            self.assertEqual(receipt["schema_version"], 1)
            self.assertEqual(receipt["source_commit"], COMMIT)
            self.assertEqual(receipt["gates"], [])
            self.assertEqual(receipt["risk_acceptances"], [])
            self.assertIsNone(receipt["finalized_at_utc"])


if __name__ == "__main__":
    unittest.main()
