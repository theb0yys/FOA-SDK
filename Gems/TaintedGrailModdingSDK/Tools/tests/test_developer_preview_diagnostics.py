#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "developer_preview_diagnostics.py"
SPEC = importlib.util.spec_from_file_location("tg_developer_preview_diagnostics", SCRIPT_PATH)
assert SPEC and SPEC.loader
diagnostics = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = diagnostics
SPEC.loader.exec_module(diagnostics)


class DeveloperPreviewDiagnosticsTests(unittest.TestCase):
    def make_roots(self, root: Path):
        repo = root / "repo"
        build = root / "build"
        project = root / "project"
        workspace = root / "workspace"
        for path in (repo, build, project, workspace):
            path.mkdir(parents=True)
        return repo, build, project, workspace

    def runner(self, command, cwd):
        name = " ".join(command)
        return 0, f"ok {name} cwd={cwd}"

    def test_redaction_replaces_known_and_generic_private_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, build, project, workspace = self.make_roots(root)
            replacements = diagnostics.known_path_replacements(
                repo_root=repo, build_dir=build, project_root=project, workspace_root=workspace
            )
            text = (
                f"repo={repo} build={build} project={project} workspace={workspace} "
                "win=C:\\Users\\Alice\\secret.txt posix=/home/alice/private.txt unc=\\\\server\\share\\file.txt"
            )
            redacted = diagnostics.redact_text(text, replacements=replacements)
            self.assertIn("<REPO_ROOT>", redacted)
            self.assertIn("<BUILD_DIR>", redacted)
            self.assertIn("<PROJECT_ROOT>", redacted)
            self.assertIn("<WORKSPACE_ROOT>", redacted)
            self.assertNotIn("Alice", redacted)
            self.assertNotIn("alice", redacted)
            diagnostics.assert_redacted_text(redacted, "test")

    def test_redaction_removes_secret_assignments_tokens_and_url_credentials(self) -> None:
        text = (
            "token=abc123 password: 'hunter2' Authorization=Bearer AAA.BBB.CCC "
            "ghp_abcdefghijklmnopqrstuvwxyz123456 https://user:pass@example.com/path"
        )
        redacted = diagnostics.redact_text(text, replacements=[])
        self.assertNotIn("abc123", redacted)
        self.assertNotIn("hunter2", redacted)
        self.assertNotIn("abcdefghijklmnopqrstuvwxyz", redacted)
        self.assertNotIn("user:pass", redacted)
        diagnostics.assert_redacted_text(redacted, "secrets")

    def test_argument_redaction_hides_secret_switch_values(self) -> None:
        values = diagnostics.redact_argument_list(
            ["--output", "bundle", "--token", "secret", "--api-key=abc", "--project", "p"]
        )
        self.assertEqual(values[3], "<REDACTED>")
        self.assertEqual(values[4], "--api-key=<REDACTED>")
        self.assertNotIn("secret", values)
        self.assertNotIn("abc", " ".join(values))

    def test_workspace_inventory_includes_only_durable_documents(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = Path(temporary)
            durable = workspace / "Catalog/catalog.tgcatalog.json"
            durable.parent.mkdir(parents=True)
            durable.write_text("{}\n", encoding="utf-8")
            source = workspace / "Input/source.json"
            source.parent.mkdir(parents=True)
            source.write_text('{"private": "content"}\n', encoding="utf-8")
            inventory = diagnostics.workspace_inventory(workspace)
            self.assertEqual([entry["path"] for entry in inventory["files"]], ["Catalog/catalog.tgcatalog.json"])
            self.assertNotIn("private", json.dumps(inventory))

    def test_workspace_inventory_rejects_symlink(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = Path(temporary)
            target = workspace / "target.tgpack.json"
            target.write_text("{}\n", encoding="utf-8")
            link = workspace / "link.tgpack.json"
            try:
                link.symlink_to(target)
            except OSError:
                return
            with self.assertRaisesRegex(diagnostics.DiagnosticsError, "symbolic link"):
                diagnostics.workspace_inventory(workspace)

    def test_log_excerpt_is_tail_limited(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "Editor.log"
            path.write_text("A" * 100 + "TAIL", encoding="utf-8")
            excerpt = diagnostics.read_log_excerpt(path, 20)
            self.assertIn("TAIL", excerpt)
            self.assertNotIn("A" * 50, excerpt)
            with self.assertRaises(diagnostics.DiagnosticsError):
                diagnostics.read_log_excerpt(path, diagnostics.MAX_LOG_BYTES_HARD + 1)

    def test_collect_and_verify_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, build, project, workspace = self.make_roots(root)
            (build / "CMakeCache.txt").write_text(
                f"CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config\nCMAKE_HOME_DIRECTORY:INTERNAL={repo}\n",
                encoding="utf-8",
            )
            (project / "project.json").write_text('{"project_name":"Preview"}\n', encoding="utf-8")
            durable = workspace / "preview.tgworkspace.json"
            durable.write_text("{}\n", encoding="utf-8")
            validation = root / "validation.json"
            validation.write_text(
                json.dumps({"status": "passed", "path": str(repo), "token": "secret"}),
                encoding="utf-8",
            )
            launch = root / "launch.json"
            launch.write_text(
                json.dumps({"exit_code": 0, "command": ["Editor.exe", "--password", "bad"]}),
                encoding="utf-8",
            )
            log = root / "Editor.log"
            log.write_text(f"Editor path {project} token=hidden\nOK\n", encoding="utf-8")
            output = root / "bundle"
            diagnostics.collect_bundle(
                output_dir=output,
                repo_root=repo,
                build_dir=build,
                project_root=project,
                workspace_root=workspace,
                validation_result=validation,
                launch_result=launch,
                editor_logs=[log],
                maximum_log_bytes=4096,
                replace=False,
                argv=["collect", "--token", "cli-secret", "--output", str(output)],
                runner=self.runner,
            )
            manifest = diagnostics.verify_bundle(output)
            self.assertEqual(manifest["bundle_id"], diagnostics.BUNDLE_ID)
            all_text = "\n".join(path.read_text(encoding="utf-8") for path in output.rglob("*") if path.is_file())
            self.assertNotIn("cli-secret", all_text)
            self.assertNotIn("token=hidden", all_text)
            self.assertNotIn("hunter2", all_text)
            self.assertNotIn(str(repo), all_text)
            self.assertIn("Review every file", (output / diagnostics.README_NAME).read_text(encoding="utf-8"))

    def test_collect_refuses_output_inside_workspace(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, build, project, workspace = self.make_roots(root)
            with self.assertRaisesRegex(diagnostics.DiagnosticsError, "must not be inside"):
                diagnostics.collect_bundle(
                    output_dir=workspace / "diagnostics",
                    repo_root=repo,
                    build_dir=build,
                    project_root=project,
                    workspace_root=workspace,
                    validation_result=None,
                    launch_result=None,
                    editor_logs=[],
                    maximum_log_bytes=100,
                    replace=False,
                    argv=[],
                    runner=self.runner,
                )

    def test_nonempty_output_requires_verified_replace(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "bundle"
            output.mkdir()
            (output / "unrelated.txt").write_text("x", encoding="utf-8")
            with self.assertRaisesRegex(diagnostics.DiagnosticsError, "not empty"):
                diagnostics.prepare_output_directory(output, replace=False)
            with self.assertRaises(diagnostics.DiagnosticsError):
                diagnostics.prepare_output_directory(output, replace=True)

    def test_verify_detects_tampering(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, build, project, workspace = self.make_roots(root)
            output = root / "bundle"
            diagnostics.collect_bundle(
                output_dir=output,
                repo_root=repo,
                build_dir=build,
                project_root=project,
                workspace_root=workspace,
                validation_result=None,
                launch_result=None,
                editor_logs=[],
                maximum_log_bytes=100,
                replace=False,
                argv=[],
                runner=self.runner,
            )
            (output / diagnostics.SYSTEM_NAME).write_text("tampered\n", encoding="utf-8")
            with self.assertRaisesRegex(diagnostics.DiagnosticsError, "(size|hash) mismatch"):
                diagnostics.verify_bundle(output)

    def test_verify_rejects_unexpected_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, build, project, workspace = self.make_roots(root)
            output = root / "bundle"
            diagnostics.collect_bundle(
                output_dir=output,
                repo_root=repo,
                build_dir=build,
                project_root=project,
                workspace_root=workspace,
                validation_result=None,
                launch_result=None,
                editor_logs=[],
                maximum_log_bytes=100,
                replace=False,
                argv=[],
                runner=self.runner,
            )
            (output / "game-save.bin").write_bytes(b"no")
            with self.assertRaisesRegex(diagnostics.DiagnosticsError, "file set"):
                diagnostics.verify_bundle(output)

    def test_manifest_path_traversal_is_rejected(self) -> None:
        with self.assertRaisesRegex(diagnostics.DiagnosticsError, "Unsafe"):
            diagnostics.normalize_relative_path("../secret.txt")
        with self.assertRaisesRegex(diagnostics.DiagnosticsError, "Unsafe"):
            diagnostics.normalize_relative_path("C:/secret.txt")

    def test_tool_failure_is_recorded_not_raised(self) -> None:
        summary = diagnostics.collect_tool_summary(Path("/repo"), lambda command, cwd: (17, "missing"))
        self.assertEqual(summary["git"]["exit_code"], 17)
        self.assertEqual(summary["git"]["output"], "missing")

    def test_infer_editor_log_uses_explicit_paths_first(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            explicit = root / "custom.log"
            explicit.write_text("x", encoding="utf-8")
            project = root / "project"
            (project / "user/log").mkdir(parents=True)
            (project / "user/log/Editor.log").write_text("y", encoding="utf-8")
            self.assertEqual(diagnostics.infer_editor_logs(project, [explicit]), [explicit])
            self.assertEqual(diagnostics.infer_editor_logs(project, []), [project / "user/log/Editor.log"])


if __name__ == "__main__":
    unittest.main()
