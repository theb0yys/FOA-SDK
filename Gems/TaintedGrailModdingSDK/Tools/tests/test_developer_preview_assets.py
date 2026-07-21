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
from unittest import mock

TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import developer_preview_assets as assets
import developer_preview_workspace as workspace_module


class DeveloperPreviewAssetTests(unittest.TestCase):
    def paths(self, root: Path) -> workspace_module.PreviewWorkspacePaths:
        workspace = root / "workspace"
        project = workspace / "project"
        for directory in (
            project,
            project / "Cache",
            project / "user/log",
            workspace / "launcher",
        ):
            directory.mkdir(parents=True, exist_ok=True)
        (project / "project.json").write_text('{}\n', encoding="utf-8")
        startup = project / "Levels/DefaultLevel/DefaultLevel.prefab"
        startup.parent.mkdir(parents=True)
        startup.write_text('{}\n', encoding="utf-8")
        return workspace_module.PreviewWorkspacePaths(
            root=workspace,
            project=project,
            startup_level=startup,
            cache=project / "Cache",
            user=project / "user",
            log=project / "user/log",
            launcher_log=workspace / "launcher",
            manifest=workspace / workspace_module.MANIFEST_NAME,
        )

    def make_engine_and_editor(self, root: Path, *, batch: bool = True) -> tuple[Path, Path]:
        engine = root / "engine"
        engine.mkdir()
        (engine / "engine.json").write_text('{}\n', encoding="utf-8")
        editor = root / "build/bin/profile/Editor.exe"
        editor.parent.mkdir(parents=True)
        editor.write_bytes(b"editor")
        if batch:
            (editor.parent / assets.ASSET_PROCESSOR_BATCH_FILENAME).write_bytes(b"batch")
        return engine, editor

    def test_command_has_fixed_bounded_paths_and_pc_platform(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            engine, editor = self.make_engine_and_editor(root)
            workspace = self.paths(root)
            batch = assets.resolve_asset_processor_batch(editor, require_exists=True)
            command = assets.asset_processor_command(batch, engine, workspace)
            self.assertEqual(command[0], str(batch))
            self.assertEqual(command[command.index("--project-path") + 1], str(workspace.project))
            self.assertEqual(
                command[command.index("--project-cache-path") + 1],
                str(workspace.cache),
            )
            self.assertEqual(command[-1], "--platforms=pc")

    def test_dry_run_does_not_require_binary_or_execute(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            engine, editor = self.make_engine_and_editor(root, batch=False)
            executor = mock.Mock(return_value=0)
            command = assets.prepare_assets(
                editor=editor,
                repo_root=engine,
                workspace=self.paths(root),
                dry_run=True,
                executor=executor,
            )
            self.assertEqual(Path(command[0]).name, assets.ASSET_PROCESSOR_BATCH_FILENAME)
            executor.assert_not_called()

    def test_success_waits_for_batch_and_writes_bounded_logs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            engine, editor = self.make_engine_and_editor(root)
            workspace = self.paths(root)
            calls = []

            def executor(command, cwd, stdout, stderr):
                calls.append((tuple(command), cwd))
                stdout.write("prepared\n")
                stderr.write("")
                return 0

            with mock.patch.object(
                assets.developer_preview_workspace,
                "verify_preview_workspace",
                return_value=workspace,
            ) as verify:
                assets.prepare_assets(
                    editor=editor,
                    repo_root=engine,
                    workspace=workspace,
                    dry_run=False,
                    executor=executor,
                )
            verify.assert_called_once_with(engine)
            self.assertEqual(calls[0][1], editor.parent)
            self.assertEqual(
                (workspace.launcher_log / assets.STDOUT_FILENAME).read_text(),
                "prepared\n",
            )

    def test_nonzero_exit_is_a_launch_blocker(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            engine, editor = self.make_engine_and_editor(root)
            workspace = self.paths(root)
            with mock.patch.object(
                assets.developer_preview_workspace,
                "verify_preview_workspace",
                return_value=workspace,
            ), self.assertRaisesRegex(assets.AssetPreparationError, "exit code 9"):
                assets.prepare_assets(
                    editor=editor,
                    repo_root=engine,
                    workspace=workspace,
                    dry_run=False,
                    executor=lambda *_: 9,
                )

    def test_missing_batch_binary_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            engine, editor = self.make_engine_and_editor(root, batch=False)
            with self.assertRaisesRegex(assets.AssetPreparationError, "not built"):
                assets.prepare_assets(
                    editor=editor,
                    repo_root=engine,
                    workspace=self.paths(root),
                    dry_run=False,
                )


if __name__ == "__main__":
    unittest.main()
