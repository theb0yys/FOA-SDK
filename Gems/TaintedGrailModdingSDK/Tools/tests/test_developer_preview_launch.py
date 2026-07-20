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
from unittest import mock

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "developer_preview_launch.py"
SPEC = importlib.util.spec_from_file_location("tg_developer_preview_launch", SCRIPT_PATH)
assert SPEC and SPEC.loader
launch = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = launch
SPEC.loader.exec_module(launch)


class DeveloperPreviewLaunchTests(unittest.TestCase):
    def make_editor(self, root: Path) -> Path:
        editor = root / "bin/profile/Editor.exe"
        editor.parent.mkdir(parents=True)
        editor.write_bytes(b"editor")
        return editor

    def make_project(self, root: Path) -> Path:
        root.mkdir(parents=True)
        (root / "project.json").write_text('{"project_name": "PreviewProject"}\n', encoding="utf-8")
        return root

    def test_editor_candidates_cover_profile_case_variants(self) -> None:
        candidates = launch.editor_candidates(Path("C:/build"))
        self.assertEqual(candidates[0], Path("C:/build/bin/profile/Editor.exe"))
        self.assertEqual(candidates[1], Path("C:/build/bin/Profile/Editor.exe"))

    def test_explicit_editor_must_be_editor_exe(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            wrong = root / "FoA.exe"
            wrong.write_bytes(b"no")
            with self.assertRaisesRegex(RuntimeError, "only Editor.exe"):
                launch.validate_editor_executable(wrong)

    def test_missing_editor_reports_build_guidance(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary) / "repo"
            repo.mkdir()
            with self.assertRaisesRegex(RuntimeError, "Run the preview build command"):
                launch.resolve_editor_executable(repo, explicit_editor=None, build_dir=Path(temporary) / "build")

    def test_project_requires_valid_project_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            root.mkdir(exist_ok=True)
            with self.assertRaisesRegex(RuntimeError, "no project.json"):
                launch.validate_project_path(root)
            (root / "project.json").write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "no project_name"):
                launch.validate_project_path(root)
            (root / "project.json").write_text("not json", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "not valid"):
                launch.validate_project_path(root)

    def test_launch_command_has_no_arbitrary_or_runtime_arguments(self) -> None:
        editor = Path("C:/build/bin/profile/Editor.exe")
        project = Path("C:/projects/Preview")
        command = launch.launch_command(editor, project)
        self.assertEqual(command, (str(editor), "--project-path", str(project)))
        joined = " ".join(command).lower()
        for prohibited in ("foa", "bepinex", "harmony", "avalon", "inject", "deploy"):
            self.assertNotIn(prohibited, joined)

    def test_dry_run_does_not_invoke_executor_or_create_log_dir(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            editor = self.make_editor(root)
            log_dir = root / "logs"
            called = []

            def executor(command, cwd, stdout, stderr):
                called.append(command)
                return 0

            result, exit_code = launch.run_launch(
                editor=editor,
                project=None,
                log_dir=log_dir,
                result_path=None,
                dry_run=True,
                executor=executor,
            )
            self.assertEqual(exit_code, 0)
            self.assertEqual(result.status, "planned")
            self.assertEqual(called, [])
            self.assertFalse(log_dir.exists())

    def test_real_launch_requires_windows_x64(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            editor = self.make_editor(Path(temporary))
            with mock.patch.object(launch.platform, "system", return_value="Linux"), mock.patch.object(
                launch.platform, "machine", return_value="x86_64"
            ):
                with self.assertRaisesRegex(RuntimeError, "Windows x64 Profile"):
                    launch.run_launch(
                        editor=editor,
                        project=None,
                        log_dir=None,
                        result_path=None,
                        dry_run=False,
                        executor=lambda *_: 0,
                    )

    def test_launch_propagates_exit_code_and_writes_result(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            editor = self.make_editor(root)
            project = self.make_project(root / "project")
            log_dir = root / "logs"

            def executor(command, cwd, stdout, stderr):
                self.assertEqual(command, (str(editor), "--project-path", str(project)))
                self.assertEqual(cwd, editor.parent)
                assert stdout and stderr
                stdout.write("stdout\n")
                stderr.write("stderr\n")
                return 19

            with mock.patch.object(launch.platform, "system", return_value="Windows"), mock.patch.object(
                launch.platform, "machine", return_value="AMD64"
            ):
                result, exit_code = launch.run_launch(
                    editor=editor,
                    project=project,
                    log_dir=log_dir,
                    result_path=None,
                    dry_run=False,
                    executor=executor,
                )
            self.assertEqual(exit_code, 19)
            self.assertEqual(result.status, "failed")
            self.assertEqual((log_dir / launch.STDOUT_FILENAME).read_text(encoding="utf-8"), "stdout\n")
            payload = json.loads((log_dir / launch.DEFAULT_RESULT_FILENAME).read_text(encoding="utf-8"))
            self.assertEqual(payload["exit_code"], 19)
            self.assertEqual(payload["schema_version"], 1)

    def test_os_error_becomes_127(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            editor = self.make_editor(Path(temporary))
            with mock.patch.object(launch.platform, "system", return_value="Windows"), mock.patch.object(
                launch.platform, "machine", return_value="AMD64"
            ):
                _, code = launch.run_launch(
                    editor=editor,
                    project=None,
                    log_dir=None,
                    result_path=None,
                    dry_run=False,
                    executor=lambda *_: (_ for _ in ()).throw(OSError("boom")),
                )
            self.assertEqual(code, 127)

    def test_log_directory_rejects_file_and_symlink(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            file_path = root / "not-dir"
            file_path.write_text("x", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "not a directory"):
                launch.validate_log_directory(file_path)
            target = root / "target"
            target.mkdir()
            link = root / "link"
            try:
                link.symlink_to(target, target_is_directory=True)
            except OSError:
                return
            with self.assertRaisesRegex(RuntimeError, "symbolic link"):
                launch.validate_log_directory(link)

    def test_pane_guidance_names_tools_menu_and_activation_log(self) -> None:
        guidance = launch.pane_guidance()
        self.assertIn("Tools -> Tainted Grail SDK", guidance)
        self.assertIn("TaintedGrailModdingSDK", guidance)
        self.assertIn("Editor.log", guidance)


if __name__ == "__main__":
    unittest.main()
