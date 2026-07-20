#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "developer_preview_entry.py"
SPEC = importlib.util.spec_from_file_location("tg_developer_preview_entry", SCRIPT_PATH)
assert SPEC and SPEC.loader
entry = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = entry
SPEC.loader.exec_module(entry)


class DeveloperPreviewEntryTests(unittest.TestCase):
    def make_files(self, root: Path) -> tuple[Path, Path, Path, Path, Path]:
        repo = root / "repo"
        project = repo / "TaintedGrailModdingEditor"
        project.mkdir(parents=True)
        icon = project / "TaintedGrailModdingEditor.ico"
        icon.write_bytes(b"icon")
        build = repo / entry.developer_preview_path_policy.APPROVED_BUILD_DIRECTORY
        editor = build / "bin/profile/Editor.exe"
        editor.parent.mkdir(parents=True)
        editor.write_bytes(b"editor")
        (build / "CMakeCache.txt").write_text("cache", encoding="utf-8")
        shortcut = repo / entry.developer_preview_path_policy.DEFAULT_SHORTCUT_OUTPUT
        shortcut.parent.mkdir(parents=True, exist_ok=True)
        shortcut.write_bytes(b"shortcut")
        return repo, project, icon, editor, shortcut

    def expected(
        self,
        repo: Path,
        project: Path,
        icon: Path,
        editor: Path,
        mode: str = "source-built",
    ):
        return entry.developer_preview_path_policy.PreviewEntryPaths(
            trust_mode=mode,
            repo_root=repo.resolve(),
            build_directory=(
                (repo / entry.developer_preview_path_policy.APPROVED_BUILD_DIRECTORY).resolve()
                if mode == "source-built"
                else None
            ),
            editor=editor.resolve(),
            project=project.resolve(),
            icon=icon.resolve(),
            working_directory=editor.parent.resolve(),
        )

    def payload(self, expected):
        return {
            "trust_mode": expected.trust_mode,
            "target": str(expected.editor),
            "arguments": ["--project-path", str(expected.project)],
            "working_directory": str(expected.working_directory),
            "icon": str(expected.icon),
        }

    def inspection(self, expected, *, target: Path | None = None) -> str:
        return json.dumps(
            {
                "target": str(target or expected.editor),
                "arguments": entry.expected_argument_text(expected.project),
                "working_directory": str(expected.working_directory),
                "icon": f"{expected.icon},0",
                "description": entry.SHORTCUT_DESCRIPTION,
            }
        )

    def test_verify_source_entry_uses_repository_policy(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, project, icon, editor, shortcut = self.make_files(Path(temporary))
            expected = self.expected(repo, project, icon, editor)
            payload = self.payload(expected)
            with mock.patch.object(
                entry.developer_preview_shortcut,
                "verify_shortcut",
                return_value=payload,
            ), mock.patch.object(
                entry.developer_preview_path_policy,
                "resolve_source_built_entry",
                return_value=expected,
            ), mock.patch.object(entry, "require_windows_x64"), mock.patch.object(
                entry,
                "resolve_powershell",
                return_value="powershell.exe",
            ):
                result = entry.verify_entry(
                    shortcut,
                    repo_root=repo,
                    runner=lambda *_: (0, self.inspection(expected)),
                )
            self.assertIs(result, payload)

    def test_self_consistent_manifest_with_unapproved_editor_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, project, icon, editor, shortcut = self.make_files(Path(temporary))
            approved = self.expected(repo, project, icon, editor)
            outside = Path(temporary) / "outside/Editor.exe"
            outside.parent.mkdir()
            outside.write_bytes(b"outside")
            malicious = self.expected(repo, project, icon, outside)
            with mock.patch.object(
                entry.developer_preview_shortcut,
                "verify_shortcut",
                return_value=self.payload(malicious),
            ), mock.patch.object(
                entry.developer_preview_path_policy,
                "resolve_source_built_entry",
                return_value=approved,
            ):
                with self.assertRaisesRegex(
                    entry.EntryVerificationError,
                    "repository-owned path policy",
                ):
                    entry.verify_entry(shortcut, repo_root=repo)

    def test_wrong_shortcut_target_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, project, icon, editor, shortcut = self.make_files(Path(temporary))
            expected = self.expected(repo, project, icon, editor)
            wrong = Path(temporary) / "wrong/Editor.exe"
            wrong.parent.mkdir()
            wrong.write_bytes(b"wrong")
            with mock.patch.object(
                entry.developer_preview_shortcut,
                "verify_shortcut",
                return_value=self.payload(expected),
            ), mock.patch.object(
                entry.developer_preview_path_policy,
                "resolve_source_built_entry",
                return_value=expected,
            ), mock.patch.object(entry, "require_windows_x64"), mock.patch.object(
                entry,
                "resolve_powershell",
                return_value="powershell.exe",
            ):
                with self.assertRaisesRegex(entry.EntryVerificationError, "target mismatch"):
                    entry.verify_entry(
                        shortcut,
                        repo_root=repo,
                        runner=lambda *_: (0, self.inspection(expected, target=wrong)),
                    )

    def test_diagnostic_override_is_not_verified_by_default(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, project, icon, editor, shortcut = self.make_files(Path(temporary))
            expected = self.expected(
                repo,
                project,
                icon,
                editor,
                mode="diagnostic-override",
            )
            with mock.patch.object(
                entry.developer_preview_shortcut,
                "verify_shortcut",
                return_value=self.payload(expected),
            ):
                with self.assertRaisesRegex(entry.EntryVerificationError, "not verified"):
                    entry.verify_entry(shortcut, repo_root=repo)

    def test_diagnostic_override_can_be_deliberately_inspected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, project, icon, editor, shortcut = self.make_files(Path(temporary))
            expected = self.expected(
                repo,
                project,
                icon,
                editor,
                mode="diagnostic-override",
            )
            with mock.patch.object(
                entry.developer_preview_shortcut,
                "verify_shortcut",
                return_value=self.payload(expected),
            ), mock.patch.object(
                entry.developer_preview_path_policy,
                "resolve_diagnostic_entry",
                return_value=expected,
            ), mock.patch.object(entry, "require_windows_x64"), mock.patch.object(
                entry,
                "resolve_powershell",
                return_value="powershell.exe",
            ):
                entry.verify_entry(
                    shortcut,
                    repo_root=repo,
                    allow_diagnostic_override=True,
                    runner=lambda *_: (0, self.inspection(expected)),
                )

    def test_replace_verifies_before_delegating(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, _, _, editor, shortcut = self.make_files(Path(temporary))
            order = []
            with mock.patch.object(
                entry,
                "verify_entry",
                side_effect=lambda *_args, **_kwargs: order.append("verify"),
            ), mock.patch.object(
                entry.developer_preview_shortcut,
                "create_shortcut",
                side_effect=lambda **_kwargs: order.append("create") or {"status": "created"},
            ):
                entry.create_entry(
                    repo_root=repo,
                    build_dir=repo / "build",
                    explicit_editor=editor,
                    output=shortcut,
                    replace=True,
                    dry_run=False,
                )
            self.assertEqual(order, ["verify", "create", "verify"])

    def test_dry_run_does_not_semantically_verify(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "build/Tainted Grail Modding Editor.lnk"
            with mock.patch.object(entry, "verify_entry") as verify, mock.patch.object(
                entry.developer_preview_shortcut,
                "create_shortcut",
                return_value={"status": "planned"},
            ):
                payload = entry.create_entry(
                    repo_root=root,
                    build_dir=root / "build",
                    explicit_editor=None,
                    output=output,
                    replace=False,
                    dry_run=True,
                )
            self.assertEqual(payload["status"], "planned")
            verify.assert_not_called()


if __name__ == "__main__":
    unittest.main()
