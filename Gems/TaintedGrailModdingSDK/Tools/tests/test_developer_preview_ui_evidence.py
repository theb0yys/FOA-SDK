#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "developer_preview_ui_evidence.py"
SPEC = importlib.util.spec_from_file_location("tg_developer_preview_ui_evidence", SCRIPT_PATH)
assert SPEC and SPEC.loader
ui = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ui
SPEC.loader.exec_module(ui)

COMMIT = "a" * 40


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def write_png(path: Path, width: int = 1280, height: int = 720) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    row = b"\x00" + (b"\x20\x40\x60" * width)
    image = row * height
    payload = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(image, 9))
        + png_chunk(b"IEND", b"")
    )
    path.write_bytes(payload)


class DeveloperPreviewUIEvidenceTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        (repo / ".git").mkdir(parents=True)
        (repo / "build").mkdir()
        return repo

    def initialize(self, root: Path) -> tuple[Path, Path]:
        repo = self.make_repo(root)
        output = repo / "build/ui-evidence"
        ui.initialize_evidence(
            output,
            repo_root=repo,
            source_commit=COMMIT,
            tester_alias="windows-reviewer",
            windows_version="Windows 11 23H2",
            display_scale_percent=125,
        )
        return repo, output

    def complete(self, output: Path, screenshot: Path | None = None) -> Path:
        for entry in ui.CHECKLIST:
            ui.record_check(
                output,
                check_id=entry["id"],
                status="pass",
                notes=f"Verified {entry['id']} on the synthetic preview workspace.",
            )
        screenshot = screenshot or output.parent / "capture.png"
        write_png(screenshot)
        ui.attach_screenshot(
            output,
            screenshot=screenshot,
            check_ids=sorted(ui.REQUIRED_SCREENSHOT_CHECKS),
            title="Reviewed TG SDK panes",
            description="Project-owned synthetic preview data displayed in the O3DE Editor.",
            privacy_reviewed=True,
            project_owned_only=True,
        )
        ui.attest_evidence(
            output,
            tested_at_utc="2026-07-18T12:00:00Z",
            launch_exit_code=0,
            activation_log_confirmed=True,
            reviewed_for_private_data=True,
            contains_only_project_owned_or_approved_content=True,
            no_game_files_or_saves_visible=True,
            no_credentials_or_private_paths_visible=True,
            no_runtime_or_deployment_action_exposed=True,
            tester_confirmed=True,
        )
        return screenshot

    def test_init_creates_pending_evidence_without_claiming_a_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            document = json.loads((output / ui.DOCUMENT_NAME).read_text(encoding="utf-8"))
            self.assertEqual(document["status"], "pending")
            self.assertTrue(all(entry["status"] == "pending" for entry in document["checklist"]))
            self.assertIn("Nothing is uploaded automatically", (output / ui.README_NAME).read_text())
            self.assertTrue((output / ui.SCREENSHOTS_DIRECTORY).is_dir())

    def test_init_rejects_invalid_source_commit(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            with self.assertRaisesRegex(ui.UIEvidenceError, "40-character"):
                ui.initialize_evidence(
                    repo / "build/ui",
                    repo_root=repo,
                    source_commit="abc",
                    tester_alias="reviewer",
                    windows_version="Windows 11",
                    display_scale_percent=100,
                )

    def test_init_rejects_tracked_source_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            with self.assertRaisesRegex(ui.UIEvidenceError, "beneath build"):
                ui.initialize_evidence(
                    repo / "docs/ui-evidence",
                    repo_root=repo,
                    source_commit=COMMIT,
                    tester_alias="reviewer",
                    windows_version="Windows 11",
                    display_scale_percent=100,
                )

    def test_init_refuses_nonempty_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            output = repo / "build/ui"
            output.mkdir()
            (output / "unrelated.txt").write_text("data", encoding="utf-8")
            with self.assertRaisesRegex(ui.UIEvidenceError, "must be empty"):
                ui.initialize_evidence(
                    output,
                    repo_root=repo,
                    source_commit=COMMIT,
                    tester_alias="reviewer",
                    windows_version="Windows 11",
                    display_scale_percent=100,
                )

    def test_record_requires_public_notes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            with self.assertRaisesRegex(ui.UIEvidenceError, "private absolute path"):
                ui.record_check(
                    output,
                    check_id="all-panes-open",
                    status="pass",
                    notes=r"Captured from C:\Users\Alice\Desktop",
                )

    def test_attach_rejects_non_png_content(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            screenshot = output.parent / "bad.png"
            screenshot.write_bytes(b"not a png")
            with self.assertRaisesRegex(ui.UIEvidenceError, "too short|invalid PNG"):
                ui.attach_screenshot(
                    output,
                    screenshot=screenshot,
                    check_ids=["all-panes-open"],
                    title="Bad screenshot",
                    description="Invalid screenshot fixture.",
                    privacy_reviewed=True,
                    project_owned_only=True,
                )

    def test_attach_records_hash_dimensions_and_coverage(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            screenshot = output.parent / "capture.png"
            write_png(screenshot, 1024, 768)
            document = ui.attach_screenshot(
                output,
                screenshot=screenshot,
                check_ids=["all-panes-open", "normal-scaling-readable"],
                title="TG SDK panes",
                description="Synthetic preview workspace only.",
                privacy_reviewed=True,
                project_owned_only=True,
            )
            entry = document["screenshots"][0]
            self.assertEqual(entry["width"], 1024)
            self.assertEqual(entry["height"], 768)
            self.assertEqual(entry["checks"], ["all-panes-open", "normal-scaling-readable"])
            self.assertEqual(entry["sha256"], ui.sha256_file(output / entry["path"]))

    def test_attest_requires_every_confirmation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            with self.assertRaisesRegex(ui.UIEvidenceError, "attestations"):
                ui.attest_evidence(
                    output,
                    tested_at_utc="2026-07-18T12:00:00Z",
                    launch_exit_code=0,
                    activation_log_confirmed=True,
                    reviewed_for_private_data=True,
                    contains_only_project_owned_or_approved_content=True,
                    no_game_files_or_saves_visible=True,
                    no_credentials_or_private_paths_visible=False,
                    no_runtime_or_deployment_action_exposed=True,
                    tester_confirmed=True,
                )

    def test_complete_evidence_verifies(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            self.complete(output)
            verified = ui.verify_evidence(output, expected_commit=COMMIT)
            self.assertEqual(verified["source_commit"], COMMIT)
            self.assertEqual(ui.actual_files(output), {
                ui.DOCUMENT_NAME,
                ui.README_NAME,
                verified["screenshots"][0]["path"],
            })

    def test_verify_rejects_pending_check(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            self.complete(output)
            document = ui.read_document(output)
            document["checklist"][0]["status"] = "pending"
            ui.write_document(output, document)
            with self.assertRaisesRegex(ui.UIEvidenceError, "did not pass"):
                ui.verify_evidence(output, expected_commit=COMMIT)

    def test_verify_rejects_missing_required_screenshot_coverage(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            for entry in ui.CHECKLIST:
                ui.record_check(output, check_id=entry["id"], status="pass", notes="Verified.")
            screenshot = output.parent / "capture.png"
            write_png(screenshot)
            ui.attach_screenshot(
                output,
                screenshot=screenshot,
                check_ids=["all-panes-open"],
                title="One pane",
                description="Only one required check is covered.",
                privacy_reviewed=True,
                project_owned_only=True,
            )
            ui.attest_evidence(
                output,
                tested_at_utc="2026-07-18T12:00:00Z",
                launch_exit_code=0,
                activation_log_confirmed=True,
                reviewed_for_private_data=True,
                contains_only_project_owned_or_approved_content=True,
                no_game_files_or_saves_visible=True,
                no_credentials_or_private_paths_visible=True,
                no_runtime_or_deployment_action_exposed=True,
                tester_confirmed=True,
            )
            with self.assertRaisesRegex(ui.UIEvidenceError, "coverage is missing"):
                ui.verify_evidence(output, expected_commit=COMMIT)

    def test_verify_detects_screenshot_tampering(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            self.complete(output)
            document = ui.read_document(output)
            screenshot = output / document["screenshots"][0]["path"]
            screenshot.write_bytes(screenshot.read_bytes() + b"tamper")
            with self.assertRaisesRegex(ui.UIEvidenceError, "hash mismatch|size mismatch"):
                ui.verify_evidence(output, expected_commit=COMMIT)

    def test_verify_rejects_unexpected_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            self.complete(output)
            (output / "desktop.txt").write_text("unexpected", encoding="utf-8")
            with self.assertRaisesRegex(ui.UIEvidenceError, "unexpected files"):
                ui.verify_evidence(output, expected_commit=COMMIT)

    def test_verify_rejects_symlink(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            self.complete(output)
            target = output / "outside.txt"
            target.write_text("data", encoding="utf-8")
            link = output / "screenshots/link.png"
            try:
                link.symlink_to(target)
            except (OSError, NotImplementedError):
                self.skipTest("Symbolic links are not available on this host.")
            with self.assertRaisesRegex(ui.UIEvidenceError, "symbolic links"):
                ui.verify_evidence(output, expected_commit=COMMIT)

    def test_verify_rejects_commit_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            _, output = self.initialize(Path(temporary))
            self.complete(output)
            with self.assertRaisesRegex(ui.UIEvidenceError, "does not match"):
                ui.verify_evidence(output, expected_commit="b" * 40)

    def test_cli_init_and_record(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            output = repo / "build/ui"
            self.assertEqual(ui.main([
                "init",
                "--output", str(output),
                "--repo-root", str(repo),
                "--source-commit", COMMIT,
                "--tester-alias", "reviewer",
                "--windows-version", "Windows 11",
                "--display-scale", "100",
            ]), 0)
            self.assertEqual(ui.main([
                "record",
                "--output", str(output),
                "--check", "keyboard-traversal",
                "--status", "pass",
                "--notes", "Tab order reached the primary controls.",
            ]), 0)


if __name__ == "__main__":
    unittest.main()
