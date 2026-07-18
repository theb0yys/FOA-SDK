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
        icon = project / "TaintedGrailModdingEditor.ico"
        icon.write_bytes(b"\x00\x00\x01\x00\x01\x00icon")
        build = repo / "build/preview"
        editor = build / "bin/profile/Editor.exe"
        editor.parent.mkdir(parents=True)
        editor.write_bytes(b"editor")
        return repo, build, editor

    def test_dry_run_uses_dedicated_project_and_icon(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, build, _ = self.make_repo(Path(temporary))
            output = repo / "build/Tainted Grail Modding Editor.lnk"
            with mock.patch.object(
                shortcut.validate_developer_preview_project,
                "validate_preview_project",
            ):
                payload = shortcut.create_shortcut(
                    repo_root=repo,
                    build_dir=build,
                    explicit_editor=None,
                    output=output,
                    replace=False,
                    dry_run=True,
                )
            self.assertEqual(payload["status"], "planned")
            self.assertIn("TaintedGrailModdingEditor", payload["arguments"][1])
            self.assertTrue(str(payload["icon"]).endswith("TaintedGrailModdingEditor.ico"))
            self.assertFalse(output.exists())

    def test_create_uses_fixed_powershell_script_and_writes_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, build, editor = self.make_repo(Path(temporary))
            output = repo / "build/Tainted Grail Modding Editor.lnk"
            calls = []

            def runner(command, environment):
                calls.append((tuple(command), dict(environment)))
                Path(environment["TG_SHORTCUT_OUTPUT"]).write_bytes(b"shortcut")
                return 0, ""

            with mock.patch.object(
                shortcut.validate_developer_preview_project,
                "validate_preview_project",
            ), mock.patch.object(
                shortcut,
                "require_windows_x64",
            ), mock.patch.object(
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

            self.assertEqual(payload["target"], str(editor))
            self.assertEqual(calls[0][1]["TG_PROJECT_PATH"], str(repo / "TaintedGrailModdingEditor"))
            self.assertIn("WScript.Shell", calls[0][0][-1])
            manifest = output.with_name(shortcut.MANIFEST_NAME)
            self.assertTrue(manifest.is_file())
            shortcut.verify_shortcut(output)

    def test_existing_shortcut_requires_replace(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo, build, _ = self.make_repo(Path(temporary))
            output = repo / "build/Tainted Grail Modding Editor.lnk"
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(b"existing")
            with mock.patch.object(
                shortcut.validate_developer_preview_project,
                "validate_preview_project",
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
            path = Path(temporary) / "Tainted Grail Modding Editor.lnk"
            path.write_bytes(b"shortcut")
            payload = {
                "shortcut": str(path),
                "size_bytes": path.stat().st_size,
                "sha256": shortcut.sha256_file(path),
            }
            path.with_name(shortcut.MANIFEST_NAME).write_text(
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
