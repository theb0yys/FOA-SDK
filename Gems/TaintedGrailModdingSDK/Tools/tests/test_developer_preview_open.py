#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import argparse
import importlib.util
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
    def test_launch_arguments_select_dedicated_project(self) -> None:
        args = argparse.Namespace(
            editor=None,
            build_dir=Path("build/preview"),
            log_dir=Path("build/logs"),
            result=None,
            dry_run=True,
        )
        values = open_preview.launch_arguments(args, Path("C:/repo"))
        project_index = values.index("--project") + 1
        self.assertEqual(
            Path(values[project_index]),
            Path("C:/repo/TaintedGrailModdingEditor"),
        )
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
        values = open_preview.launch_arguments(args, Path("C:/repo"))
        self.assertIn("--editor", values)
        self.assertNotIn("--build-dir", values)
        self.assertIn("--result", values)

    def test_main_validates_project_then_delegates(self) -> None:
        with mock.patch.object(
            open_preview.validate_developer_preview_project,
            "validate_preview_project",
        ) as validate, mock.patch.object(
            open_preview.developer_preview_launch,
            "main",
            return_value=17,
        ) as launch:
            code = open_preview.main(["--dry-run"])
        self.assertEqual(code, 17)
        validate.assert_called_once()
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
        ), mock.patch.object(open_preview.developer_preview_launch, "main") as launch:
            code = open_preview.main(["--dry-run"])
        self.assertEqual(code, 2)
        launch.assert_not_called()


if __name__ == "__main__":
    unittest.main()
