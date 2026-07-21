#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import developer_preview_path_policy as policy


def write_test_editor(path: Path) -> None:
    data = bytearray(4096)
    data[:2] = b"MZ"
    pe_offset = 0x80
    struct.pack_into("<I", data, 0x3C, pe_offset)
    data[pe_offset : pe_offset + 4] = b"PE\0\0"
    struct.pack_into("<H", data, pe_offset + 4, 0x8664)
    struct.pack_into("<H", data, pe_offset + 20, 0xF0)
    optional_offset = pe_offset + 24
    struct.pack_into("<H", data, optional_offset, 0x20B)
    struct.pack_into("<H", data, optional_offset + 68, 2)
    path.write_bytes(data)


class DeveloperPreviewPathPolicyTests(unittest.TestCase):
    def workspace(self, root: Path):
        workspace_root = root / "local-workspace"
        project = workspace_root / "project"
        return policy.developer_preview_workspace.PreviewWorkspacePaths(
            root=workspace_root.resolve(),
            project=project.resolve(),
            startup_level=(project / "Levels/DefaultLevel/DefaultLevel.prefab").resolve(),
            cache=(workspace_root / "project/Cache").resolve(),
            user=(workspace_root / "project/user").resolve(),
            log=(workspace_root / "project/user/log").resolve(),
            launcher_log=(workspace_root / "launcher").resolve(),
            manifest=(workspace_root / ".tg-preview-materialization.json").resolve(),
        )

    def make_repo(self, root: Path) -> tuple[Path, Path, Path]:
        repo = root / "repo"
        project = repo / "TaintedGrailModdingEditor"
        project.mkdir(parents=True)
        (project / "TaintedGrailModdingEditor.ico").write_bytes(b"icon")
        startup_level = repo / policy.PREVIEW_STARTUP_LEVEL
        startup_level.parent.mkdir(parents=True)
        startup_level.write_text("{}\n", encoding="utf-8")
        build = repo / policy.APPROVED_BUILD_DIRECTORY
        editor = build / policy.EDITOR_CANDIDATES[0]
        editor.parent.mkdir(parents=True)
        write_test_editor(editor)
        (build / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={repo.resolve()}\n"
            f"LY_PROJECTS:STRING={project.resolve()}\n",
            encoding="utf-8",
        )
        return repo, build, editor

    def test_component_containment_does_not_use_string_prefixes(self) -> None:
        root = Path("/workspace/project")
        self.assertFalse(policy.is_contained(root, Path("/workspace/project-escape/file")))
        self.assertTrue(policy.is_contained(root, Path("/workspace/project/file")))

    def test_case_insensitive_component_comparison_is_available(self) -> None:
        self.assertTrue(
            policy.is_contained(
                Path("C:/Workspace"),
                Path("c:/workspace/Packs/a.tgpack.json"),
                case_insensitive=True,
            )
        )

    def test_symlink_escape_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace = root / "workspace"
            outside = root / "outside"
            workspace.mkdir()
            outside.mkdir()
            link = workspace / "link"
            try:
                link.symlink_to(outside, target_is_directory=True)
            except OSError as exc:
                self.skipTest(f"symlinks unavailable: {exc}")
            with self.assertRaisesRegex(policy.PathPolicyError, "must remain inside"):
                policy.require_contained(workspace, link / "file.json", label="Pack")

    def test_repository_project_symlink_escape_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo = root / "repo"
            outside = root / "outside-project"
            repo.mkdir()
            outside.mkdir()
            (outside / "TaintedGrailModdingEditor.ico").write_bytes(b"icon")
            try:
                (repo / policy.PREVIEW_PROJECT).symlink_to(outside, target_is_directory=True)
            except OSError as exc:
                self.skipTest(f"symlinks unavailable: {exc}")
            with self.assertRaisesRegex(policy.PathPolicyError, "inside the repository"):
                policy.resolve_diagnostic_entry(repo, root / "Editor.exe")

    def test_source_build_must_use_exact_approved_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, _, _ = self.make_repo(root)
            with mock.patch.object(policy, "_workspace_paths", return_value=self.workspace(root)):
                with self.assertRaisesRegex(policy.PathPolicyError, "approved Developer Preview"):
                    policy.resolve_source_built_entry(
                        repo,
                        repo / "build/other",
                        require_editor=False,
                        require_configured=False,
                    )

    def test_source_build_uses_repository_owned_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, build, editor = self.make_repo(root)
            workspace = self.workspace(root)
            with mock.patch.object(policy, "_workspace_paths", return_value=workspace):
                paths = policy.resolve_source_built_entry(
                    repo,
                    build,
                    require_editor=True,
                    require_configured=True,
                )
            self.assertEqual(paths.editor, editor.resolve())
            self.assertEqual(paths.engine, repo.resolve())
            self.assertEqual(paths.project, workspace.project)
            self.assertEqual(paths.startup_level, workspace.startup_level)
            self.assertEqual(paths.project_cache, workspace.cache)
            self.assertEqual(paths.project_user, workspace.user)
            self.assertEqual(paths.project_log, workspace.log)
            self.assertEqual(paths.icon, (repo / policy.PREVIEW_ICON).resolve())
            self.assertEqual(paths.trust_mode, policy.TRUST_MODE_SOURCE_BUILD)

    def test_source_build_requires_explicit_cmake_source_binding(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, build, _ = self.make_repo(root)
            (build / "CMakeCache.txt").write_text(
                f"LY_PROJECTS:STRING={(repo / policy.PREVIEW_PROJECT).resolve()}\n",
                encoding="utf-8",
            )
            with mock.patch.object(policy, "_workspace_paths", return_value=self.workspace(root)):
                with self.assertRaisesRegex(policy.PathPolicyError, "CMAKE_HOME_DIRECTORY"):
                    policy.resolve_source_built_entry(
                        repo,
                        build,
                        require_editor=True,
                        require_configured=True,
                    )

    def test_source_build_rejects_arbitrary_editor_contents(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, build, editor = self.make_repo(root)
            editor.write_bytes(b"not an editor")
            with mock.patch.object(policy, "_workspace_paths", return_value=self.workspace(root)):
                with self.assertRaisesRegex(policy.PathPolicyError, "too small|PE header"):
                    policy.resolve_source_built_entry(
                        repo,
                        build,
                        require_editor=True,
                        require_configured=True,
                    )

    def test_diagnostic_override_is_labeled_and_requires_editor_exe(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, _, _ = self.make_repo(root)
            external = Path(temporary) / "external/Editor.exe"
            external.parent.mkdir()
            external.write_bytes(b"external")
            with mock.patch.object(policy, "_workspace_paths", return_value=self.workspace(root)):
                paths = policy.resolve_diagnostic_entry(repo, external)
            self.assertEqual(paths.trust_mode, policy.TRUST_MODE_DIAGNOSTIC_OVERRIDE)
            bad = external.with_name("not-editor.exe")
            bad.write_bytes(b"bad")
            with mock.patch.object(policy, "_workspace_paths", return_value=self.workspace(root)):
                with self.assertRaisesRegex(policy.PathPolicyError, "Editor.exe"):
                    policy.resolve_diagnostic_entry(repo, bad)

    def test_diagnostic_override_cannot_use_standard_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, _, _ = self.make_repo(Path(temporary))
            with self.assertRaisesRegex(policy.PathPolicyError, "must not replace"):
                policy.resolve_shortcut_output(
                    repo,
                    repo / policy.DEFAULT_SHORTCUT_OUTPUT,
                    diagnostic_override=True,
                )

    def test_shortcut_build_root_symlink_escape_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo = root / "repo"
            outside = root / "outside-build"
            repo.mkdir()
            outside.mkdir()
            try:
                (repo / "build").symlink_to(outside, target_is_directory=True)
            except OSError as exc:
                self.skipTest(f"symlinks unavailable: {exc}")
            with self.assertRaisesRegex(policy.PathPolicyError, "inside the repository"):
                policy.resolve_shortcut_output(
                    repo,
                    repo / policy.DEFAULT_SHORTCUT_OUTPUT,
                    diagnostic_override=False,
                )


if __name__ == "__main__":
    unittest.main()
