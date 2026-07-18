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
    def make_files(self, root: Path) -> tuple[Path, Path, Path, Path]:
        project = root / "TaintedGrailModdingEditor"
        project.mkdir()
        icon = project / "TaintedGrailModdingEditor.ico"
        icon.write_bytes(b"icon")
        editor = root / "build/bin/profile/Editor.exe"
        editor.parent.mkdir(parents=True)
        editor.write_bytes(b"editor")
        shortcut = root / "build/Tainted Grail Modding Editor.lnk"
        shortcut.write_bytes(b"shortcut")
        return project, icon, editor, shortcut

    def manifest(self, project: Path, icon: Path, editor: Path, shortcut: Path) -> dict[str, object]:
        return {
            "target": str(editor),
            "arguments": ["--project-path", str(project)],
            "working_directory": str(editor.parent),
            "icon": str(icon),
        }

    def inspection(
        self,
        project: Path,
        icon: Path,
        editor: Path,
        *,
        target: Path | None = None,
    ) -> str:
        return json.dumps(
            {
                "target": str(target or editor),
                "arguments": entry.expected_argument_text(project),
                "working_directory": str(editor.parent),
                "icon": f"{icon},0",
                "description": entry.SHORTCUT_DESCRIPTION,
            }
        )

    def test_verify_entry_checks_semantic_properties(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            project, icon, editor, shortcut = self.make_files(Path(temporary))
            payload = self.manifest(project, icon, editor, shortcut)
            with mock.patch.object(
                entry.developer_preview_shortcut,
                "verify_shortcut",
                return_value=payload,
            ), mock.patch.object(entry, "require_windows_x64"), mock.patch.object(
                entry,
                "resolve_powershell",
                return_value="powershell.exe",
            ):
                result = entry.verify_entry(
                    shortcut,
                    runner=lambda *_: (0, self.inspection(project, icon, editor)),
                )
            self.assertIs(result, payload)

    def test_verify_entry_rejects_wrong_target(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            project, icon, editor, shortcut = self.make_files(Path(temporary))
            wrong = Path(temporary) / "wrong/Editor.exe"
            wrong.parent.mkdir()
            wrong.write_bytes(b"wrong")
            with mock.patch.object(
                entry.developer_preview_shortcut,
                "verify_shortcut",
                return_value=self.manifest(project, icon, editor, shortcut),
            ), mock.patch.object(entry, "require_windows_x64"), mock.patch.object(
                entry,
                "resolve_powershell",
                return_value="powershell.exe",
            ):
                with self.assertRaisesRegex(entry.EntryVerificationError, "target mismatch"):
                    entry.verify_entry(
                        shortcut,
                        runner=lambda *_: (
                            0,
                            self.inspection(project, icon, editor, target=wrong),
                        ),
                    )

    def test_replace_verifies_before_delegating(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _, _, editor, shortcut = self.make_files(root)
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
                    repo_root=root,
                    build_dir=root / "build",
                    explicit_editor=editor,
                    output=shortcut,
                    replace=True,
                    dry_run=False,
                )
            self.assertEqual(order, ["verify", "create", "verify"])

    def test_dry_run_delegates_without_semantic_verification(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "build/Tainted Grail Modding Editor.lnk"
            with mock.patch.object(
                entry,
                "verify_entry",
            ) as verify, mock.patch.object(
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

    def test_non_windows_host_fails_closed(self) -> None:
        with mock.patch.object(entry.platform, "system", return_value="Linux"), mock.patch.object(
            entry.platform,
            "machine",
            return_value="x86_64",
        ):
            with self.assertRaisesRegex(entry.EntryVerificationError, "Windows x64"):
                entry.require_windows_x64()


if __name__ == "__main__":
    unittest.main()
