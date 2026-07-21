#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import json
import os
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


PINNED_COMMIT = "68683f23fb747380d3efa2424bd5f30242e9c5a2"


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
            cache=(project / "Cache").resolve(),
            user=(project / "user").resolve(),
            log=(project / "user/log").resolve(),
            launcher_log=(workspace_root / "launcher").resolve(),
            manifest=(workspace_root / ".tg-preview-materialization.json").resolve(),
        )

    def make_layout(self, root: Path) -> tuple[Path, Path, Path, Path]:
        product = root / "FOA-SDK"
        project = product / "TaintedGrailModdingEditor"
        project.mkdir(parents=True)
        (project / "project.json").write_text("{}\n", encoding="utf-8")
        (project / "TaintedGrailModdingEditor.ico").write_bytes(b"icon")
        startup_level = product / policy.PREVIEW_STARTUP_LEVEL
        startup_level.parent.mkdir(parents=True)
        startup_level.write_text("{}\n", encoding="utf-8")
        for gem in ("TaintedGrailModdingSDK", "ExternalToolchain"):
            gem_root = product / "Gems" / gem
            gem_root.mkdir(parents=True)
            (gem_root / "gem.json").write_text("{}\n", encoding="utf-8")
        (product / "o3de.lock.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "repository": "https://github.com/o3de/o3de.git",
                    "commit": PINNED_COMMIT,
                    "engine_name": "o3de",
                    "engine_version": "2.7.0",
                    "checkout_directory": "o3de",
                }
            ),
            encoding="utf-8",
        )

        engine = root / "o3de"
        (engine / "scripts/commit_validation").mkdir(parents=True)
        (engine / "scripts/commit_validation/validate_file_or_folder.py").write_text("", encoding="utf-8")
        (engine / "CMakePresets.json").write_text("{}\n", encoding="utf-8")
        (engine / "engine.json").write_text(
            json.dumps({"engine_name": "o3de", "version": "2.7.0"}),
            encoding="utf-8",
        )

        build = root / "foa-build/tg-sdk-developer-preview-0-windows-profile"
        editor = build / policy.EDITOR_CANDIDATES[0]
        editor.parent.mkdir(parents=True)
        write_test_editor(editor)
        (build / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={engine.resolve()}\n"
            f"LY_PROJECTS:STRING={project.resolve()}\n",
            encoding="utf-8",
        )
        return product, engine, build, editor

    def engine_environment(self, engine: Path):
        return mock.patch.dict(os.environ, {"FOA_O3DE_ROOT": str(engine)}, clear=False)

    def pin_mock(self):
        return mock.patch.object(policy.developer_preview, "validate_engine_pin", return_value=None)

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

    def test_product_project_symlink_escape_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            product = root / "FOA-SDK"
            outside = root / "outside-project"
            product.mkdir()
            outside.mkdir()
            (outside / "TaintedGrailModdingEditor.ico").write_bytes(b"icon")
            try:
                (product / policy.PREVIEW_PROJECT).symlink_to(outside, target_is_directory=True)
            except OSError as exc:
                self.skipTest(f"symlinks unavailable: {exc}")
            with mock.patch.object(policy.developer_preview, "validate_product_root", return_value=None):
                with self.assertRaisesRegex(policy.PathPolicyError, "product checkout"):
                    policy._product_paths(product)

    def test_source_build_must_use_exact_external_build_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            product, engine, _, _ = self.make_layout(root)
            with self.engine_environment(engine), self.pin_mock(), mock.patch.object(
                policy, "_workspace_paths", return_value=self.workspace(root)
            ):
                with self.assertRaisesRegex(policy.PathPolicyError, "approved external"):
                    policy.resolve_source_built_entry(
                        product,
                        product / "build/other",
                        require_editor=False,
                        require_configured=False,
                    )

    def test_source_build_uses_separate_product_engine_and_build_roots(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            product, engine, build, editor = self.make_layout(root)
            workspace = self.workspace(root)
            with self.engine_environment(engine), self.pin_mock(), mock.patch.object(
                policy, "_workspace_paths", return_value=workspace
            ):
                paths = policy.resolve_source_built_entry(
                    product,
                    build,
                    require_editor=True,
                    require_configured=True,
                )
            self.assertEqual(paths.repo_root, product.resolve())
            self.assertEqual(paths.engine, engine.resolve())
            self.assertEqual(paths.build_directory, build.resolve())
            self.assertEqual(paths.editor, editor.resolve())
            self.assertEqual(paths.project, workspace.project)
            self.assertFalse(policy.is_contained(product.resolve(), paths.build_directory))
            self.assertFalse(policy.is_contained(engine.resolve(), paths.build_directory))

    def test_source_build_rejects_cache_bound_to_product_as_engine(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            product, engine, build, _ = self.make_layout(root)
            (build / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={product.resolve()}\n"
                f"LY_PROJECTS:STRING={(product / policy.PREVIEW_PROJECT).resolve()}\n",
                encoding="utf-8",
            )
            with self.engine_environment(engine), self.pin_mock(), mock.patch.object(
                policy, "_workspace_paths", return_value=self.workspace(root)
            ):
                with self.assertRaisesRegex(policy.PathPolicyError, "another engine source tree"):
                    policy.resolve_source_built_entry(
                        product,
                        build,
                        require_editor=True,
                        require_configured=True,
                    )

    def test_source_build_requires_foa_project_binding(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            product, engine, build, _ = self.make_layout(root)
            (build / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={engine.resolve()}\nLY_PROJECTS:STRING=\n",
                encoding="utf-8",
            )
            with self.engine_environment(engine), self.pin_mock(), mock.patch.object(
                policy, "_workspace_paths", return_value=self.workspace(root)
            ):
                with self.assertRaisesRegex(policy.PathPolicyError, "FOA-SDK project"):
                    policy.resolve_source_built_entry(
                        product,
                        build,
                        require_editor=True,
                        require_configured=True,
                    )

    def test_source_build_rejects_arbitrary_editor_contents(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            product, engine, build, editor = self.make_layout(root)
            editor.write_bytes(b"not an editor")
            with self.engine_environment(engine), self.pin_mock(), mock.patch.object(
                policy, "_workspace_paths", return_value=self.workspace(root)
            ):
                with self.assertRaisesRegex(policy.PathPolicyError, "too small|PE header"):
                    policy.resolve_source_built_entry(
                        product,
                        build,
                        require_editor=True,
                        require_configured=True,
                    )

    def test_diagnostic_override_keeps_external_engine_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            product, engine, _, _ = self.make_layout(root)
            external = root / "diagnostic/Editor.exe"
            external.parent.mkdir()
            external.write_bytes(b"external")
            with self.engine_environment(engine), self.pin_mock(), mock.patch.object(
                policy, "_workspace_paths", return_value=self.workspace(root)
            ):
                paths = policy.resolve_diagnostic_entry(product, external)
            self.assertEqual(paths.trust_mode, policy.TRUST_MODE_DIAGNOSTIC_OVERRIDE)
            self.assertEqual(paths.engine, engine.resolve())

    def test_shortcut_outputs_are_outside_product_checkout(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            product, _, _, _ = self.make_layout(root)
            output = root / "foa-build/Tainted Grail Modding Editor.lnk"
            resolved = policy.resolve_shortcut_output(product, output, diagnostic_override=False)
            self.assertEqual(resolved, output.resolve())
            self.assertFalse(policy.is_contained(product.resolve(), resolved))
            with self.assertRaisesRegex(policy.PathPolicyError, "must not replace"):
                policy.resolve_shortcut_output(product, output, diagnostic_override=True)

    def test_shortcut_output_cannot_escape_external_build_root(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            product, _, _, _ = self.make_layout(root)
            with self.assertRaisesRegex(policy.PathPolicyError, "must remain inside"):
                policy.resolve_shortcut_output(
                    product,
                    root / "outside/entry.lnk",
                    diagnostic_override=False,
                )


if __name__ == "__main__":
    unittest.main()
