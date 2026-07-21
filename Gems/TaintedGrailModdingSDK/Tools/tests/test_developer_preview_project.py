#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "validate_developer_preview_project.py"
SPEC = importlib.util.spec_from_file_location("tg_validate_developer_preview_project", SCRIPT_PATH)
assert SPEC and SPEC.loader
contract = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = contract
SPEC.loader.exec_module(contract)


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + kind
        + data
        + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    )


def write_png(path: Path, width: int = 256, height: int = 256) -> None:
    row = b"\x00" + (b"\x20\x40\x60" * width)
    payload = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(row * height))
        + png_chunk(b"IEND", b"")
    )
    path.write_bytes(payload)


class DeveloperPreviewProjectContractTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        project = repo / "TaintedGrailModdingEditor"
        tools = repo / "Gems/TaintedGrailModdingSDK/Tools"
        (project / "cmake").mkdir(parents=True)
        (project / "ShaderLib").mkdir(parents=True)
        (project / "Levels").mkdir()
        (project / "Levels/DefaultLevel").mkdir()
        (repo / "Assets/Editor/Prefabs").mkdir(parents=True)
        (repo / "AutomatedTesting").mkdir(parents=True)
        tools.mkdir(parents=True)
        (repo / "docs/tainted-grail-sdk").mkdir(parents=True)

        default_level = {"ContainerEntity": {}, "Entities": {"Grid": {}, "Ground": {}}}
        (repo / "Assets/Editor/Prefabs/Default_Level.prefab").write_text(
            json.dumps(default_level),
            encoding="utf-8",
        )
        (project / "Levels/DefaultLevel/DefaultLevel.prefab").write_text(
            json.dumps(default_level),
            encoding="utf-8",
        )

        (repo / "engine.json").write_text(
            json.dumps(
                {
                    "external_subdirectories": ["Gems/TaintedGrailModdingSDK"],
                    "projects": ["AutomatedTesting", "TaintedGrailModdingEditor"],
                }
            ),
            encoding="utf-8",
        )
        (project / "project.json").write_text(
            json.dumps(
                {
                    "project_name": "TaintedGrailModdingEditor",
                    "display_name": "Tainted Grail Modding Editor",
                    "product_name": "Tainted Grail Modding Editor",
                    "executable_name": "TaintedGrailModdingEditor",
                    "icon_path": "preview.png",
                    "engine": "o3de",
                    "gem_names": [
                        "Atom",
                        "DiffuseProbeGrid",
                        "PhysX5",
                        "ExternalToolchain",
                        "TaintedGrailModdingSDK",
                    ],
                }
            ),
            encoding="utf-8",
        )
        (project / "CMakeLists.txt").write_text("o3de_initialize()\n", encoding="utf-8")
        (project / "cmake/EngineFinder.cmake").write_text(
            "list(APPEND CMAKE_MODULE_PATH engine)\n",
            encoding="utf-8",
        )
        (project / "ShaderLib/scenesrg.srgi").write_text(
            "#pragma once\n"
            "#include <Atom/Features/SrgSemantics.azsli>\n"
            "partial ShaderResourceGroup SceneSrg : SRG_PerScene {};\n",
            encoding="utf-8",
        )
        (project / "ShaderLib/viewsrg.srgi").write_text(
            "#pragma once\n"
            "#include <Atom/Features/SrgSemantics.azsli>\n"
            "partial ShaderResourceGroup ViewSrg : SRG_PerView {};\n",
            encoding="utf-8",
        )
        write_png(project / "preview.png")
        (project / "TaintedGrailModdingEditor.ico").write_bytes(
            b"\x00\x00\x01\x00\x01\x00icon"
        )
        (repo / "AutomatedTesting/project.json").write_text(
            json.dumps({"project_name": "AutomatedTesting", "gem_names": ["Atom"]}),
            encoding="utf-8",
        )
        (repo / "docs/tainted-grail-sdk/OPEN_AND_TEST_EDITOR.md").write_text(
            "TaintedGrailModdingEditor\n"
            "developer_preview_entry.py create\n"
            "developer_preview_entry.py verify\n"
            "DefaultLevel.prefab\n"
            "validate_path_policy.py\n"
            "bounded per-user storage\n"
            "Tainted Grail Modding Editor.lnk\n"
            "Tools \u2192 Tainted Grail SDK\n"
            "LOCALAPPDATA\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_open.py").write_text(
            "validate_preview_project(repo_root)\n"
            "materialize_preview_workspace(repo_root)\n"
            '"--project-cache"\n'
            '"--project-user"\n'
            '"--project-log"\n'
            "developer_preview_assets.prepare_assets()\n"
            "developer_preview_launch.main(arguments)\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_path_policy.py").write_text(
            "APPROVED_BUILD_DIRECTORY\n"
            "resolve_source_built_entry\n"
            "resolve_diagnostic_entry\n"
            "validate_source_editor_binary\n"
            "PREVIEW_STARTUP_LEVEL\n"
            "verify_preview_workspace\n"
            "developer_preview.validate_build_directory\n"
            "Diagnostic overrides must not replace\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_shortcut.py").write_text(
            "WScript.Shell\n"
            "TG_SHORTCUT_OUTPUT\n"
            "TG_ENGINE_PATH\n"
            "TG_PROJECT_CACHE_PATH\n"
            "TG_PROJECT_USER_PATH\n"
            "TG_PROJECT_LOG_PATH\n"
            "TG_STARTUP_LEVEL\n"
            "developer_preview_assets.prepare_assets()\n"
            "developer_preview_path_policy\n"
            "resolve_source_built_entry\n"
            "resolve_diagnostic_entry\n"
            "diagnostic_override\n"
            '"trust_mode": paths.trust_mode\n'
            "validate_preview_project\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_entry.py").write_text(
            "WScript.Shell\n"
            "developer_preview_shortcut.verify_shortcut\n"
            "developer_preview_shortcut.create_shortcut\n"
            "resolve_source_built_entry\n"
            "_require_manifest_matches_policy\n"
            "Diagnostic override shortcuts are not verified source-built entries\n"
            "allow_diagnostic_override\n"
            "inspect_shortcut\n"
            "expected.startup_level\n"
            "expected.project_cache\n"
            "expected.project_user\n"
            "expected.project_log\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_workspace.py").write_text(
            "LOCALAPPDATA\n"
            "MANAGED_PROJECT_FILES\n"
            "PRESERVED_PROJECT_FILES\n"
            "materialize_preview_workspace\n"
            "verify_preview_workspace\n"
            "refusing to overwrite\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_launch.py").write_text(
            'parser.add_argument("--project")\n'
            'parser.add_argument("--level")\n'
            'parser.add_argument("--engine")\n'
            'parser.add_argument("--project-cache")\n'
            'parser.add_argument("--project-user")\n'
            'parser.add_argument("--project-log")\n'
            'command.extend(("--project-path", str(project)))\n'
            "validate_project_path(project)\n"
            "validate_write_paths(cache, user, log)\n"
            "validate_startup_level(project, level)\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_assets.py").write_text(
            "AssetProcessorBatch.exe\n"
            "--project-cache-path\n"
            "--project-user-path\n"
            "--project-log-path\n"
            "--platforms=pc\n"
            "verify_preview_workspace\n"
            "Asset preparation failed\n",
            encoding="utf-8",
        )
        (tools / "developer_preview.py").write_text(
            'ASSET_PROCESSOR_BATCH_TARGET = "AssetProcessorBatch"\n'
            "targets = (ASSET_PROCESSOR_BATCH_TARGET,)\n",
            encoding="utf-8",
        )
        return repo

    def test_valid_dedicated_project_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            contract.validate_preview_project(self.make_repo(Path(temporary)))

    def test_missing_gem_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "TaintedGrailModdingEditor/project.json"
            document = json.loads(path.read_text(encoding="utf-8"))
            document["gem_names"] = []
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "must enable"):
                contract.validate_preview_project(repo)

    def test_automated_testing_must_not_host_preview(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "AutomatedTesting/project.json"
            document = json.loads(path.read_text(encoding="utf-8"))
            document["gem_names"].append("TaintedGrailModdingSDK")
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "must not host"):
                contract.validate_preview_project(repo)

    def test_project_identity_and_icons_are_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "TaintedGrailModdingEditor/project.json"
            document = json.loads(path.read_text(encoding="utf-8"))
            document["display_name"] = "Wrong"
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "display_name"):
                contract.validate_preview_project(repo)

    def test_missing_renderer_host_gem_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "TaintedGrailModdingEditor/project.json"
            document = json.loads(path.read_text(encoding="utf-8"))
            document["gem_names"].remove("DiffuseProbeGrid")
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "DiffuseProbeGrid"):
                contract.validate_preview_project(repo)

    def test_project_scene_and_view_srgs_are_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            (repo / "TaintedGrailModdingEditor/ShaderLib/scenesrg.srgi").unlink()
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "scene SRG"):
                contract.validate_preview_project(repo)

        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            (repo / "TaintedGrailModdingEditor/ShaderLib/viewsrg.srgi").unlink()
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "view SRG"):
                contract.validate_preview_project(repo)

    def test_level_root_is_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            (repo / "TaintedGrailModdingEditor/Levels/DefaultLevel/DefaultLevel.prefab").unlink()
            (repo / "TaintedGrailModdingEditor/Levels/DefaultLevel").rmdir()
            (repo / "TaintedGrailModdingEditor/Levels").rmdir()
            with self.assertRaisesRegex(
                contract.PreviewProjectContractError,
                "level root is missing",
            ):
                contract.validate_preview_project(repo)

    def test_default_startup_level_is_required_and_must_match_template(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            startup = repo / "TaintedGrailModdingEditor/Levels/DefaultLevel/DefaultLevel.prefab"
            startup.unlink()
            with self.assertRaisesRegex(
                contract.PreviewProjectContractError,
                "startup level is missing",
            ):
                contract.validate_preview_project(repo)

        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            startup = repo / "TaintedGrailModdingEditor/Levels/DefaultLevel/DefaultLevel.prefab"
            startup.write_text('{"different": true}\n', encoding="utf-8")
            with self.assertRaisesRegex(
                contract.PreviewProjectContractError,
                "must remain the O3DE default level template",
            ):
                contract.validate_preview_project(repo)

    def test_quickstart_must_name_path_policy(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "docs/tainted-grail-sdk/OPEN_AND_TEST_EDITOR.md"
            path.write_text("developer_preview_entry.py create\n", encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "missing required"):
                contract.validate_preview_project(repo)

    def test_sidecar_cannot_be_source_trust_root(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py"
            path.write_text(
                path.read_text(encoding="utf-8") + "expected_target = Path(target_value)\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "sidecar"):
                contract.validate_preview_project(repo)

    def test_path_policy_file_is_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_path_policy.py"
            path.unlink()
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "path and executable"):
                contract.validate_preview_project(repo)


if __name__ == "__main__":
    unittest.main()
