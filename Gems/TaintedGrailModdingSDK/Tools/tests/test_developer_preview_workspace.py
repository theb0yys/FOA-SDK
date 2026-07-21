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

TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import developer_preview_workspace as workspace


class DeveloperPreviewWorkspaceTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        project = repo / workspace.PREVIEW_PROJECT
        for relative in (*workspace.MANAGED_PROJECT_FILES, *workspace.PRESERVED_PROJECT_FILES):
            path = project / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(f"source:{relative.as_posix()}\n".encode())
        return repo

    def environment(self, root: Path) -> dict[str, str]:
        local_app_data = root / "local"
        local_app_data.mkdir()
        return {"LOCALAPPDATA": str(local_app_data)}

    def test_dry_run_resolves_without_writing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo = self.make_repo(root)
            environment = self.environment(root)
            paths = workspace.materialize_preview_workspace(
                repo,
                environment=environment,
                dry_run=True,
            )
            self.assertFalse(paths.root.exists())
            self.assertTrue(str(paths.root).startswith(str(Path(environment["LOCALAPPDATA"]))))

    def test_materialization_creates_and_verifies_bounded_workspace(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo = self.make_repo(root)
            environment = self.environment(root)
            paths = workspace.materialize_preview_workspace(repo, environment=environment)
            verified = workspace.verify_preview_workspace(repo, environment=environment)
            self.assertEqual(verified, paths)
            self.assertTrue(paths.cache.is_dir())
            self.assertTrue(paths.user.is_dir())
            self.assertTrue(paths.log.is_dir())
            self.assertTrue(paths.launcher_log.is_dir())
            self.assertEqual(
                paths.startup_level.read_bytes(),
                (repo / workspace.PREVIEW_PROJECT / workspace.PREVIEW_STARTUP_LEVEL).read_bytes(),
            )

    def test_user_levels_and_modified_startup_level_are_preserved(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo = self.make_repo(root)
            environment = self.environment(root)
            paths = workspace.materialize_preview_workspace(repo, environment=environment)
            authored = paths.project / "Levels/UserLevel/UserLevel.prefab"
            authored.parent.mkdir(parents=True)
            authored.write_text("user level\n", encoding="utf-8")
            paths.startup_level.write_text("customized default\n", encoding="utf-8")

            workspace.materialize_preview_workspace(repo, environment=environment)

            self.assertEqual(authored.read_text(encoding="utf-8"), "user level\n")
            self.assertEqual(
                paths.startup_level.read_text(encoding="utf-8"),
                "customized default\n",
            )

    def test_unchanged_managed_file_is_safely_updated(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo = self.make_repo(root)
            environment = self.environment(root)
            paths = workspace.materialize_preview_workspace(repo, environment=environment)
            source = repo / workspace.PREVIEW_PROJECT / "project.json"
            source.write_text("updated source\n", encoding="utf-8")

            workspace.materialize_preview_workspace(repo, environment=environment)

            self.assertEqual((paths.project / "project.json").read_text(), "updated source\n")

    def test_modified_managed_file_is_not_overwritten(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo = self.make_repo(root)
            environment = self.environment(root)
            paths = workspace.materialize_preview_workspace(repo, environment=environment)
            (paths.project / "project.json").write_text("external change\n", encoding="utf-8")
            (repo / workspace.PREVIEW_PROJECT / "project.json").write_text(
                "updated source\n",
                encoding="utf-8",
            )

            with self.assertRaisesRegex(workspace.WorkspaceError, "refusing to overwrite"):
                workspace.materialize_preview_workspace(repo, environment=environment)

    def test_missing_or_relative_local_app_data_fails_closed(self) -> None:
        with self.assertRaisesRegex(workspace.WorkspaceError, "unavailable"):
            workspace.default_local_app_data({})
        with self.assertRaisesRegex(workspace.WorkspaceError, "absolute"):
            workspace.default_local_app_data({"LOCALAPPDATA": "relative"})

    def test_project_symlink_escape_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo = self.make_repo(root)
            environment = self.environment(root)
            paths = workspace.resolve_workspace_paths(environment=environment)
            paths.root.mkdir(parents=True)
            outside = root / "outside"
            outside.mkdir()
            try:
                paths.project.symlink_to(outside, target_is_directory=True)
            except OSError as exc:
                self.skipTest(f"symlinks unavailable: {exc}")
            with self.assertRaisesRegex(workspace.WorkspaceError, "escaped|symbolic link"):
                workspace.materialize_preview_workspace(repo, environment=environment)


if __name__ == "__main__":
    unittest.main()
