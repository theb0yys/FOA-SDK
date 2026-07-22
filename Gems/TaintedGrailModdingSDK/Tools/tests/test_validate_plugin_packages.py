#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import validate_plugin_packages as contract


def manifest(extension_id: str, category: str, *, dependencies: list[str] | None = None) -> dict:
    return {
        "schema_version": 1,
        "id": extension_id,
        "name": extension_id,
        "version": "1.0.0",
        "category": category,
        "status": "available",
        "optional": True,
        "entry_points": {"o3de_gems": ["Gem"]},
        "capabilities": ["read-active-profile"],
        "dependencies": dependencies or [],
        "compatibility": {
            "game_versions": ["1.23.401"],
            "branches": ["mono"],
            "runtime_targets": ["editor-only" if category == "authoring" else "mono"],
        },
        "provenance": {
            "origin": "owner/repository",
            "revision": "a" * 40,
            "license": "NOASSERTION",
            "redistribution_reviewed": False,
        },
    }


class PluginPackageContractTests(unittest.TestCase):
    def make_tree(self, root: Path, packages: list[tuple[str, str, dict]]) -> None:
        for category in contract.CATEGORY_BY_DIRECTORY:
            category_root = root / "Plugins" / category
            category_root.mkdir(parents=True, exist_ok=True)
            (category_root / "README.md").write_text(category, encoding="utf-8")

        external = ["../Gems/ExternalToolchain", "../Gems/TaintedGrailModdingSDK"]
        gems = ["ExternalToolchain", "TaintedGrailModdingSDK"]
        for category, name, payload in packages:
            package_root = root / "Plugins" / category / name
            gem_root = package_root / "Gem"
            gem_root.mkdir(parents=True)
            (package_root / "plugin.json").write_text(
                json.dumps(payload), encoding="utf-8"
            )
            gem_name = name + "Gem"
            (gem_root / "gem.json").write_text(
                json.dumps(
                    {
                        "gem_name": gem_name,
                        "type": "Tool",
                        "dependencies": ["TaintedGrailModdingSDK"],
                    }
                ),
                encoding="utf-8",
            )
            (gem_root / "CMakeLists.txt").write_text("project(Test)\n", encoding="utf-8")
            external.append(f"../Plugins/{category}/{name}/Gem")
            gems.append(gem_name)

        project_root = root / "TaintedGrailModdingEditor"
        project_root.mkdir(parents=True)
        (project_root / "project.json").write_text(
            json.dumps(
                {
                    "external_subdirectories": external,
                    "gem_names": gems,
                }
            ),
            encoding="utf-8",
        )

    def test_valid_tool_gem_package_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_tree(
                root,
                [("Authoring", "RoadAtlas", manifest("extension.road-atlas", "authoring"))],
            )
            packages = contract.validate_plugin_packages(root)
            self.assertEqual([package.extension_id for package in packages], ["extension.road-atlas"])

    def test_project_owned_package_may_use_project_owned_revision(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = manifest("extension.local-tooling", "authoring")
            payload["provenance"] = {
                "origin": "theb0yys/FOA-SDK",
                "revision": "project-owned",
                "license": "Apache-2.0 OR MIT",
                "redistribution_reviewed": True,
            }
            self.make_tree(root, [("Authoring", "LocalTooling", payload)])
            packages = contract.validate_plugin_packages(root)
            self.assertEqual([package.extension_id for package in packages], ["extension.local-tooling"])

    def test_integration_package_may_leave_game_branch_compatibility_unbound(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = manifest("extension.integration", "integration")
            payload["compatibility"]["game_versions"] = []
            payload["compatibility"]["branches"] = []
            self.make_tree(root, [("Integrations", "Integration", payload)])
            packages = contract.validate_plugin_packages(root)
            self.assertEqual([package.extension_id for package in packages], ["extension.integration"])

    def test_unknown_manifest_key_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = manifest("extension.road-atlas", "authoring")
            payload["authority"] = "runtime"
            self.make_tree(root, [("Authoring", "RoadAtlas", payload)])
            with self.assertRaisesRegex(contract.PluginPackageError, "keys mismatch"):
                contract.validate_plugin_packages(root)

    def test_category_mismatch_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_tree(
                root,
                [("Authoring", "RoadAtlas", manifest("extension.road-atlas", "integration"))],
            )
            with self.assertRaisesRegex(contract.PluginPackageError, "category"):
                contract.validate_plugin_packages(root)

    def test_unknown_capability_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = manifest("extension.road-atlas", "authoring")
            payload["capabilities"] = ["runtime.execute"]
            self.make_tree(root, [("Authoring", "RoadAtlas", payload)])
            with self.assertRaisesRegex(contract.PluginPackageError, "unknown governed"):
                contract.validate_plugin_packages(root)

    def test_missing_entry_point_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = manifest("extension.road-atlas", "authoring")
            payload["entry_points"] = {"o3de_gems": ["Missing"]}
            self.make_tree(root, [("Authoring", "RoadAtlas", payload)])
            with self.assertRaisesRegex(contract.PluginPackageError, "does not exist"):
                contract.validate_plugin_packages(root)

    def test_dependency_cycle_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_tree(
                root,
                [
                    ("Authoring", "Alpha", manifest("extension.alpha", "authoring", dependencies=["extension.beta"])),
                    ("Authoring", "Beta", manifest("extension.beta", "authoring", dependencies=["extension.alpha"])),
                ],
            )
            with self.assertRaisesRegex(contract.PluginPackageError, "cycle"):
                contract.validate_plugin_packages(root)

    def test_unlicensed_payload_cannot_be_redistribution_reviewed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = manifest("extension.road-atlas", "authoring")
            payload["provenance"]["redistribution_reviewed"] = True
            self.make_tree(root, [("Authoring", "RoadAtlas", payload)])
            with self.assertRaisesRegex(contract.PluginPackageError, "unlicensed"):
                contract.validate_plugin_packages(root)

    def test_project_must_select_package_once(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_tree(
                root,
                [("Authoring", "RoadAtlas", manifest("extension.road-atlas", "authoring"))],
            )
            project = root / "TaintedGrailModdingEditor/project.json"
            payload = json.loads(project.read_text(encoding="utf-8"))
            payload["gem_names"].remove("RoadAtlasGem")
            project.write_text(json.dumps(payload), encoding="utf-8")
            with self.assertRaisesRegex(contract.PluginPackageError, "does not deterministically select"):
                contract.validate_plugin_packages(root)


if __name__ == "__main__":
    unittest.main()
