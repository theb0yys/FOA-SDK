#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import argparse
import importlib.util
import os
import sys
import unittest
from pathlib import Path
from unittest import mock

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "developer_preview_open.py"
SPEC = importlib.util.spec_from_file_location("tg_developer_preview_open", SCRIPT_PATH)
assert SPEC and SPEC.loader
open_preview = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = open_preview
SPEC.loader.exec_module(open_preview)


class DeveloperPreviewOpenTests(unittest.TestCase):
    def workspace(self) -> open_preview.developer_preview_workspace.PreviewWorkspacePaths:
        root = Path("C:/Users/test/AppData/Local/O3DE/TGEditor")
        project = root / "project"
        return open_preview.developer_preview_workspace.PreviewWorkspacePaths(
            root=root,
            project=project,
            startup_level=project / "Levels/DefaultLevel/DefaultLevel.prefab",
            cache=project / "Cache",
            user=project / "user",
            log=project / "user/log",
            launcher_log=root / "launcher",
            manifest=root / ".tg-preview-materialization.json",
        )

    def test_launch_arguments_select_dedicated_project(self) -> None:
        args = argparse.Namespace(
            editor=None,
            build_dir=Path("build/preview"),
            log_dir=Path("build/logs"),
            result=None,
            dry_run=True,
        )
        workspace = self.workspace()
        values = open_preview.launch_arguments(args, Path("C:/repo"), workspace)
        project_index = values.index("--project") + 1
        self.assertEqual(Path(values[project_index]), workspace.project)
        level_index = values.index("--level") + 1
        self.assertEqual(
            Path(values[level_index]),
            workspace.startup_level,
        )
        self.assertEqual(Path(values[values.index("--project-cache") + 1]), workspace.cache)
        self.assertEqual(Path(values[values.index("--project-user") + 1]), workspace.user)
        self.assertEqual(Path(values[values.index("--project-log") + 1]), workspace.log)
        self.assertEqual(Path(values[values.index("--engine") + 1]), Path("C:/repo"))
        self.assertIn("--build-dir", values)
        self.assertIn("--dry-run", values)

    def test_explicit_editor_replaces_build_directory(self) -> None:
        args = argparse.Namespace(
            editor=Path("C:/build/bin/profile/Editor.exe"),
            build_dir=Path("ignored"),
            log_dir=Path("build/logs"),
            result=Path("build/result.json"),
            dry_run=False,
        )
        values = open_preview.launch_arguments(args, Path("C:/repo"), self.workspace())
        self.assertIn("--editor", values)
        self.assertNotIn("--build-dir", values)
        self.assertIn("--result", values)

    def test_launcher_outputs_must_remain_in_bounded_storage(self) -> None:
        outside = Path("D:/outside/logs") if os.name == "nt" else Path("/outside/logs")
        args = argparse.Namespace(
            editor=None,
            build_dir=Path("build/preview"),
            log_dir=outside,
            result=None,
            dry_run=True,
        )
        with self.assertRaisesRegex(
            open_preview.developer_preview_workspace.WorkspaceError,
            "must remain inside",
        ):
            open_preview.launch_arguments(args, Path("C:/repo"), self.workspace())

    def test_main_validates_project_then_delegates(self) -> None:
        with mock.patch.object(
            open_preview.validate_developer_preview_project,
            "validate_preview_project",
        ) as validate, mock.patch.object(
            open_preview.developer_preview_workspace,
            "materialize_preview_workspace",
            return_value=self.workspace(),
        ) as materialize, mock.patch.object(
            open_preview.developer_preview_assets,
            "prepare_assets",
        ) as prepare, mock.patch.object(
            open_preview.developer_preview_launch,
            "main",
            return_value=17,
        ) as launch:
            code = open_preview.main(["--dry-run"])
        self.assertEqual(code, 17)
        validate.assert_called_once()
        materialize.assert_called_once()
        prepare.assert_called_once()
        delegated = launch.call_args.args[0]
        self.assertIn("--project", delegated)
        self.assertIn("--dry-run", delegated)

    def test_main_fails_before_launch_for_invalid_project(self) -> None:
        with mock.patch.object(
            open_preview.validate_developer_preview_project,
            "validate_preview_project",
            side_effect=open_preview.validate_developer_preview_project.PreviewProjectContractError(
                "missing"
            ),
        ), mock.patch.object(
            open_preview.developer_preview_assets,
            "prepare_assets",
        ) as prepare, mock.patch.object(open_preview.developer_preview_launch, "main") as launch:
            code = open_preview.main(["--dry-run"])
        self.assertEqual(code, 2)
        prepare.assert_not_called()
        launch.assert_not_called()


if __name__ == "__main__":
    unittest.main()
