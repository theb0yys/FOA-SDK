#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import validate_developer_preview_project as contract


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
    def make_product(self, root: Path) -> Path:
        product = root / "FOA-SDK"
        project = product / "TaintedGrailModdingEditor"
        tools = product / "Gems/TaintedGrailModdingSDK/Tools"
        external = product / "Gems/ExternalToolchain"
        (project / "cmake").mkdir(parents=True)
        (project / "ShaderLib").mkdir(parents=True)
        (project / "Levels/DefaultLevel").mkdir(parents=True)
        tools.mkdir(parents=True)
        external.mkdir(parents=True)
        (product / "docs/tainted-grail-sdk").mkdir(parents=True)

        (product / "o3de.lock.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "repository": "https://github.com/o3de/o3de.git",
                    "commit": "68683f23fb747380d3efa2424bd5f30242e9c5a2",
                    "engine_name": "o3de",
                    "engine_version": "2.7.0",
                    "checkout_directory": "o3de",
                }
            ),
            encoding="utf-8",
        )
        (product / "Gems/TaintedGrailModdingSDK/gem.json").write_text(
            '{"gem_name":"TaintedGrailModdingSDK"}\n', encoding="utf-8"
        )
        (external / "gem.json").write_text(
            '{"gem_name":"ExternalToolchain"}\n', encoding="utf-8"
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
                    "external_subdirectories": [
                        "../Gems/ExternalToolchain",
                        "../Gems/TaintedGrailModdingSDK",
                        "../Plugins/Authoring/AvalonAI/Gem",
                        "../Plugins/Authoring/RoadAtlas/Gem",
                    ],
                    "gem_names": [
                        "Atom",
                        "DiffuseProbeGrid",
                        "PhysX5",
                        "ExternalToolchain",
                        "TaintedGrailModdingSDK",
                        "AvalonAIAuthoring",
                        "RoadAtlasAuthoring",
                    ],
                }
            ),
            encoding="utf-8",
        )
        (project / "CMakeLists.txt").write_text("o3de_initialize()\n", encoding="utf-8")
        (project / "cmake/EngineFinder.cmake").write_text(
            "FOA_O3DE_ROOT\n"
            "o3de.lock.json\n"
            "External O3DE commit mismatch\n"
            "tg_editor_product_root\n"
            "tg_editor_engine_root\n"
            "set(tg_editor_engine_root)\n",
            encoding="utf-8",
        )
        (project / "Levels/DefaultLevel/DefaultLevel.prefab").write_text(
            json.dumps({"ContainerEntity": {}, "Entities": {}}), encoding="utf-8"
        )
        (project / "ShaderLib/scenesrg.srgi").write_text(
            "#pragma once\nSrgSemantics.azsli\n"
            "partial ShaderResourceGroup SceneSrg : SRG_PerScene {};\n",
            encoding="utf-8",
        )
        (project / "ShaderLib/viewsrg.srgi").write_text(
            "#pragma once\nSrgSemantics.azsli\n"
            "partial ShaderResourceGroup ViewSrg : SRG_PerView {};\n",
            encoding="utf-8",
        )
        write_png(project / "preview.png")
        (project / "TaintedGrailModdingEditor.ico").write_bytes(
            b"\x00\x00\x01\x00\x01\x00icon"
        )

        (product / "docs/tainted-grail-sdk/OPEN_AND_TEST_EDITOR.md").write_text(
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
        (tools / "developer_preview_path_policy.py").write_text(
            "APPROVED_BUILD_DIRECTORY\n"
            "resolve_source_built_entry\n"
            "resolve_diagnostic_entry\n"
            "validate_source_editor_binary\n"
            "PREVIEW_STARTUP_LEVEL\n"
            "verify_preview_workspace\n"
            "developer_preview.validate_build_directory\n"
            "FOA-SDK product_root and O3DE engine_root must be separate checkouts\n"
            "Diagnostic overrides must not replace\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_shortcut.py").write_text(
            "TG_ENGINE_PATH\nTG_PROJECT_CACHE_PATH\nTG_PROJECT_USER_PATH\n"
            "TG_PROJECT_LOG_PATH\nTG_STARTUP_LEVEL\nresolve_source_built_entry\n"
            "resolve_diagnostic_entry\n"
            '"trust_mode": paths.trust_mode\n',
            encoding="utf-8",
        )
        (tools / "developer_preview_entry.py").write_text(
            "verify_shortcut\ncreate_shortcut\nresolve_source_built_entry\n"
            "_require_manifest_matches_policy\n"
            "Diagnostic override shortcuts are not verified source-built entries\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_workspace.py").write_text(
            "LOCALAPPDATA\nMANAGED_PROJECT_FILES\nPRESERVED_PROJECT_FILES\n"
            "materialize_preview_workspace\nverify_preview_workspace\nrefusing to overwrite\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_assets.py").write_text(
            "AssetProcessorBatch.exe\n--project-cache-path\n--project-user-path\n"
            "--project-log-path\nverify_preview_workspace\nAsset preparation failed\n",
            encoding="utf-8",
        )
        return product

    def test_valid_extracted_product_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            contract.validate_preview_project(self.make_product(Path(temporary)))

    def test_inherited_engine_root_path_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            product = self.make_product(Path(temporary))
            (product / "Code").mkdir()
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "Inherited O3DE path"):
                contract.validate_preview_project(product)

    def test_stock_or_unknown_gem_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            product = self.make_product(Path(temporary))
            stock = product / "Gems/Atom"
            stock.mkdir()
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "exactly the two"):
                contract.validate_preview_project(product)

    def test_project_must_register_exact_product_gem_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            product = self.make_product(Path(temporary))
            path = product / "TaintedGrailModdingEditor/project.json"
            document = json.loads(path.read_text(encoding="utf-8"))
            document["external_subdirectories"] = ["../Gems/TaintedGrailModdingSDK"]
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "complete product-owned"):
                contract.validate_preview_project(product)

    def test_missing_required_gem_name_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            product = self.make_product(Path(temporary))
            path = product / "TaintedGrailModdingEditor/project.json"
            document = json.loads(path.read_text(encoding="utf-8"))
            document["gem_names"].remove("ExternalToolchain")
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "ExternalToolchain"):
                contract.validate_preview_project(product)

    def test_startup_level_is_product_owned_and_structurally_canonical(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            product = self.make_product(Path(temporary))
            startup = product / "TaintedGrailModdingEditor/Levels/DefaultLevel/DefaultLevel.prefab"
            startup.write_text('{"Entities": {}}\n', encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "ContainerEntity"):
                contract.validate_preview_project(product)

    def test_dependency_lock_requires_full_commit(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            product = self.make_product(Path(temporary))
            lock = product / "o3de.lock.json"
            document = json.loads(lock.read_text(encoding="utf-8"))
            document["commit"] = "short"
            lock.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "full commit SHA"):
                contract.validate_preview_project(product)

    def test_engine_finder_must_enforce_exact_commit(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            product = self.make_product(Path(temporary))
            finder = product / "TaintedGrailModdingEditor/cmake/EngineFinder.cmake"
            finder.write_text("FOA_O3DE_ROOT\no3de.lock.json\n", encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "External O3DE commit mismatch"):
                contract.validate_preview_project(product)


if __name__ == "__main__":
    unittest.main()
