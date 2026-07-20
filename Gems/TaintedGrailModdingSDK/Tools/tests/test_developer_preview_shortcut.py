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

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "developer_preview_shortcut.py"
SPEC = importlib.util.spec_from_file_location("tg_developer_preview_shortcut", SCRIPT_PATH)
assert SPEC and SPEC.loader
shortcut = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = shortcut
SPEC.loader.exec_module(shortcut)


class DeveloperPreviewShortcutTests(unittest.TestCase):
    def make_repo(self, root: Path) -> tuple[Path, Path, Path]:
        repo = root / "repo"
        project = repo / "TaintedGrailModdingEditor"
        project.mkdir(parents=True)
        (project / "TaintedGrailModdingEditor.ico").write_bytes(b"icon")
        build = repo / shortcut.DEFAULT_BUILD_DIRECTORY
        editor = build / "bin/profile/Editor.exe"
        editor.parent.mkdir(parents=True)
        editor.write_bytes(b"editor")
        (build / "CMakeCache.txt").write_text("cache", encoding="utf-8")
        return repo, build, editor

    def expected_paths(
        self,
        repo: Path,
        build: Path,
        editor: Path,
        trust_mode: str,
    ):
        return shortcut.developer_preview_path_policy.PreviewEntryPaths(
            trust_mode=trust_mode,
            repo_root=repo.resolve(),
            build_directory=(build.resolve() if trust_mode == "source-built" else None),
            editor=editor.resolve(),
            project=(repo / "TaintedGrailModdingEditor").resolve(),
            icon=(repo / "TaintedGrailModdingEditor/TaintedGrailModdingEditor.ico").resolve(),
            working_directory=editor.parent.resolve(),
        )

    def test_dry_run_uses_source_build_policy(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, build, editor = self.make_repo(Path(temporary))
            output = repo / shortcut.DEFAULT_OUTPUT
            with mock.patch.object(
                shortcut.validate_developer_preview_project,
                "validate_preview_project",
            ), mock.patch.object(
                shortcut.developer_preview_path_policy,
                "resolve_source_built_entry",
                return_value=self.expected_paths(repo, build, editor, "source-built"),
            ):
                payload = shortcut.create_shortcut(
                    repo_root=repo,
                    build_dir=build,
                    explicit_editor=None,
                    output=output,
                    replace=False,
                    dry_run=True,
                )
            self.assertEqual(payload["trust_mode"], "source-built")
            self.assertFalse(output.exists())

    def test_create_uses_fixed_script_and_records_trust_mode(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, build, editor = self.make_repo(Path(temporary))
            output = repo / shortcut.DEFAULT_OUTPUT
            calls = []

            def runner(command, environment):
                calls.append((tuple(command), dict(environment)))
                Path(environment["TG_SHORTCUT_OUTPUT"]).write_bytes(b"shortcut")
                return 0, ""

            with mock.patch.object(
                shortcut.validate_developer_preview_project,
                "validate_preview_project",
            ), mock.patch.object(
                shortcut.developer_preview_path_policy,
                "resolve_source_built_entry",
                return_value=self.expected_paths(repo, build, editor, "source-built"),
            ), mock.patch.object(
                shortcut.developer_preview_path_policy,
                "resolve_shortcut_output",
                return_value=output.resolve(),
            ), mock.patch.object(shortcut, "require_windows_x64"), mock.patch.object(
                shortcut,
                "resolve_powershell",
                return_value="powershell.exe",
            ):
                payload = shortcut.create_shortcut(
                    repo_root=repo,
                    build_dir=build,
                    explicit_editor=None,
                    output=output,
                    replace=False,
                    dry_run=False,
                    runner=runner,
                )

            self.assertEqual(payload["target"], str(editor.resolve()))
            self.assertEqual(payload["trust_mode"], "source-built")
            self.assertIn("WScript.Shell", calls[0][0][-1])
            manifest = shortcut.manifest_path_for(output)
            document = json.loads(manifest.read_text(encoding="utf-8"))
            self.assertEqual(document["trust_mode"], "source-built")
            shortcut.verify_shortcut(output)

    def test_explicit_editor_requires_diagnostic_override(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, build, editor = self.make_repo(Path(temporary))
            with mock.patch.object(
                shortcut.validate_developer_preview_project,
                "validate_preview_project",
            ):
                with self.assertRaisesRegex(shortcut.ShortcutError, "diagnostic override"):
                    shortcut.create_shortcut(
                        repo_root=repo,
                        build_dir=build,
                        explicit_editor=editor,
                        output=repo / shortcut.DEFAULT_OUTPUT,
                        replace=False,
                        dry_run=True,
                    )

    def test_diagnostic_override_is_labeled(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, build, editor = self.make_repo(Path(temporary))
            output = repo / "build/diagnostic-entries/test.lnk"
            paths = self.expected_paths(repo, build, editor, "diagnostic-override")
            with mock.patch.object(
                shortcut.validate_developer_preview_project,
                "validate_preview_project",
            ), mock.patch.object(
                shortcut.developer_preview_path_policy,
                "resolve_diagnostic_entry",
                return_value=paths,
            ), mock.patch.object(
                shortcut.developer_preview_path_policy,
                "resolve_shortcut_output",
                return_value=output.resolve(),
            ):
                payload = shortcut.create_shortcut(
                    repo_root=repo,
                    build_dir=build,
                    explicit_editor=editor,
                    output=output,
                    replace=False,
                    dry_run=True,
                    diagnostic_override=True,
                )
            self.assertEqual(payload["trust_mode"], "diagnostic-override")
            self.assertNotIn("approved_build_directory", payload)

    def test_existing_shortcut_requires_replace(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, build, editor = self.make_repo(Path(temporary))
            output = repo / shortcut.DEFAULT_OUTPUT
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(b"existing")
            with mock.patch.object(
                shortcut.validate_developer_preview_project,
                "validate_preview_project",
            ), mock.patch.object(
                shortcut.developer_preview_path_policy,
                "resolve_source_built_entry",
                return_value=self.expected_paths(repo, build, editor, "source-built"),
            ), mock.patch.object(
                shortcut.developer_preview_path_policy,
                "resolve_shortcut_output",
                return_value=output.resolve(),
            ), mock.patch.object(shortcut, "require_windows_x64"):
                with self.assertRaisesRegex(shortcut.ShortcutError, "--replace"):
                    shortcut.create_shortcut(
                        repo_root=repo,
                        build_dir=build,
                        explicit_editor=None,
                        output=output,
                        replace=False,
                        dry_run=False,
                        runner=lambda *_: (0, ""),
                    )

    def test_verify_detects_tampering(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "entry.lnk"
            path.write_bytes(b"shortcut")
            payload = {
                "schema_version": 1,
                "trust_mode": "source-built",
                "shortcut": str(path.resolve()),
                "size_bytes": path.stat().st_size,
                "sha256": shortcut.sha256_file(path),
            }
            shortcut.manifest_path_for(path).write_text(
                json.dumps(payload),
                encoding="utf-8",
            )
            path.write_bytes(b"changed")
            with self.assertRaisesRegex(shortcut.ShortcutError, "size|SHA-256"):
                shortcut.verify_shortcut(path)

    def test_non_windows_host_fails_closed(self) -> None:
        with mock.patch.object(shortcut.platform, "system", return_value="Linux"), mock.patch.object(
            shortcut.platform,
            "machine",
            return_value="x86_64",
        ):
            with self.assertRaisesRegex(shortcut.ShortcutError, "Windows x64"):
                shortcut.require_windows_x64()


if __name__ == "__main__":
    unittest.main()
