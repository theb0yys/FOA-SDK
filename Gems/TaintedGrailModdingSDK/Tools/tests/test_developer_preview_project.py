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
        (repo / "AutomatedTesting").mkdir(parents=True)
        tools.mkdir(parents=True)
        (repo / "docs/tainted-grail-sdk").mkdir(parents=True)

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
                    "gem_names": ["TaintedGrailModdingSDK"],
                }
            ),
            encoding="utf-8",
        )
        (project / "CMakeLists.txt").write_text("o3de_initialize()\n", encoding="utf-8")
        (project / "cmake/EngineFinder.cmake").write_text(
            "list(APPEND CMAKE_MODULE_PATH engine)\n",
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
            "validate_path_policy.py\n"
            "repository-owned path policy\n"
            "Tainted Grail Modding Editor.lnk\n"
            "Tools \u2192 Tainted Grail SDK\n"
            "TaintedGrailModdingEditor/user/log/Editor.log\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_open.py").write_text(
            'PREVIEW_PROJECT = Path("TaintedGrailModdingEditor")\n'
            "validate_preview_project(repo_root)\n"
            "developer_preview_launch.main(arguments)\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_path_policy.py").write_text(
            "APPROVED_BUILD_DIRECTORY\n"
            "resolve_source_built_entry\n"
            "resolve_diagnostic_entry\n"
            "validate_source_editor_binary\n"
            "developer_preview.validate_build_directory\n"
            "Diagnostic overrides must not replace\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_shortcut.py").write_text(
            "WScript.Shell\n"
            "TG_SHORTCUT_OUTPUT\n"
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
            "inspect_shortcut\n",
            encoding="utf-8",
        )
        (tools / "developer_preview_launch.py").write_text(
            'parser.add_argument("--project")\n'
            'command.extend(("--project-path", str(project)))\n'
            "validate_project_path(project)\n",
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
