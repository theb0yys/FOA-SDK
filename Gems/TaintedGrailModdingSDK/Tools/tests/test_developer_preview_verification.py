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
import subprocess
import sys
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "developer_preview_verification.py"
SPEC = importlib.util.spec_from_file_location("tg_preview_verification", SCRIPT)
assert SPEC and SPEC.loader
verification = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = verification
SPEC.loader.exec_module(verification)


class DeveloperPreviewVerificationTests(unittest.TestCase):
    def paths(self, root: Path) -> dict[str, Path]:
        repo = root / "repo"
        repo.mkdir()
        return {
            "repo": repo,
            "build": repo / "build/o3de",
            "receipt": root / "receipt",
            "ui": repo / "build/ui",
            "state": repo / "build/verification",
        }

    def test_prerequisites_do_not_poison_fresh_build_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            values = self.paths(Path(temporary))
            command = verification.prerequisites(values)
            output = Path(command[command.index("--json-output") + 1])
            self.assertEqual(output, values["state"] / "prerequisites.json")
            self.assertFalse(output.is_relative_to(values["build"]))

    def test_whitespace_gate_records_explicit_reviewed_range(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            values = self.paths(Path(temporary))
            base = "a" * 40
            self.assertEqual(
                verification.gate_commands(values, base)["git-diff-check"],
                ("git", "diff", "--check", base, "HEAD"),
            )

    def test_finalized_receipt_reruns_both_authoritative_verifiers(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            values = self.paths(Path(temporary))
            steps = verification.final_steps(values, "b" * 40, {}, True)
            self.assertEqual(
                [name for name, _ in steps],
                ["verify-receipt", "verify-ui-evidence", "summarize-receipt"],
            )
            self.assertIn("--require-merge-ready", steps[0][1])
            self.assertIn("developer_preview_ui_evidence.py", " ".join(steps[1][1]))

    def test_status_requires_receipt_ui_and_exact_range_verification(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            values = self.paths(Path(temporary))
            values["receipt"].mkdir()
            values["ui"].mkdir(parents=True)
            base = "c" * 40
            head = "d" * 40
            (values["receipt"] / verification.RECEIPT_NAME).write_text(
                json.dumps(
                    {
                        "finalized_at_utc": "2026-07-20T12:00:00Z",
                        "gates": [
                            {
                                "name": "git-diff-check",
                                "status": "passed",
                                "command": f"git diff --check {base} HEAD",
                            },
                            {"name": verification.UI_GATE, "status": "passed"},
                        ],
                    }
                ),
                encoding="utf-8",
            )
            (values["ui"] / "ui-evidence.json").write_text("{}\n", encoding="utf-8")
            calls: list[tuple[str, ...]] = []

            def successful(command, cwd, capture=False):
                del cwd, capture
                calls.append(tuple(command))
                return 0, "verified"

            with patch.object(verification, "invoke", side_effect=successful):
                report = verification.status(values, head, base)
            self.assertTrue(report["complete_verified"])
            self.assertEqual(len(calls), 2)

            with patch.object(verification, "invoke", return_value=(1, "hash mismatch")):
                report = verification.status(values, head, base)
            self.assertFalse(report["complete_verified"])

    def test_raw_passed_strings_cannot_claim_completion(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            values = self.paths(Path(temporary))
            values["receipt"].mkdir()
            base = "e" * 40
            (values["receipt"] / verification.RECEIPT_NAME).write_text(
                json.dumps(
                    {
                        "finalized_at_utc": "2026-07-20T12:00:00Z",
                        "gates": [
                            {
                                "name": "git-diff-check",
                                "status": "passed",
                                "command": f"git diff --check {base} HEAD",
                            },
                            {"name": verification.UI_GATE, "status": "passed"},
                        ],
                    }
                ),
                encoding="utf-8",
            )
            with patch.object(verification, "invoke", return_value=(1, "tampered evidence")):
                report = verification.status(values, "f" * 40, base)
            self.assertFalse(report["complete_verified"])
            self.assertEqual(report["receipt_metadata_unverified"][verification.UI_GATE], "passed")

    def test_review_base_must_be_nonempty_ancestor(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary) / "repo"
            repo.mkdir()
            subprocess.run(["git", "-C", str(repo), "init"], check=True, capture_output=True)
            (repo / "engine.json").write_text("{}\n", encoding="utf-8")
            tools = repo / "Gems/TaintedGrailModdingSDK/Tools"
            tools.mkdir(parents=True)
            for name in (
                "developer_preview.py",
                "validation_receipt.py",
                "developer_preview_ui_evidence.py",
            ):
                (tools / name).write_text("# fixture\n", encoding="utf-8")
            (repo / "file.txt").write_text("base\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(repo), "add", "."], check=True)
            commit = [
                "git", "-C", str(repo), "-c", "user.name=Test",
                "-c", "user.email=test@example.invalid", "commit", "-m",
            ]
            subprocess.run([*commit, "base"], check=True, capture_output=True)
            base = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
            (repo / "file.txt").write_text("head\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(repo), "add", "file.txt"], check=True)
            subprocess.run([*commit, "head"], check=True, capture_output=True)
            head = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
            args = Namespace(repo_root=repo, review_base=base)
            self.assertEqual(verification.repository(args), (repo, head, base))
            args.review_base = head
            with self.assertRaisesRegex(verification.VerificationError, "different"):
                verification.repository(args)


if __name__ == "__main__":
    unittest.main()
