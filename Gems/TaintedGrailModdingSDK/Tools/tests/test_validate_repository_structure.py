#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import sys
import unittest
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import validate_repository_structure as contract


def valid_tree() -> set[str]:
    paths = set(contract.REQUIRED_PATHS)
    paths.update(
        {
            ".clang-format",
            ".editorconfig",
            ".gitattributes",
            ".gitignore",
            "CHANGELOG.md",
            "CODE_OF_CONDUCT.md",
            "CONTRIBUTING.md",
            "GOVERNANCE.md",
            "LICENSE.txt",
            "LICENSE_APACHE2.TXT",
            "LICENSE_MIT.TXT",
            "ROADMAP.md",
            "SECURITY.md",
            "SUPPORT.md",
            "Gems/ExternalToolchain/Code/CMakeLists.txt",
            "Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt",
            "Research/o3de-to-unity-conversion-and-runtime-bridge/inputs/report.md",
            "TaintedGrailModdingEditor/Levels/DefaultLevel/DefaultLevel.prefab",
            "docs/tainted-grail-sdk/ARCHITECTURE.md",
        }
    )
    paths.update(contract.ALLOWED_GITHUB_FILES)
    return paths


class RepositoryStructureContractTests(unittest.TestCase):
    def test_reviewed_product_tree_passes(self) -> None:
        contract.validate_paths(valid_tree())

    def test_manifest_backed_plugin_package_passes(self) -> None:
        paths = valid_tree() | {
            "Plugins/Authoring/AvalonAI/plugin.json",
            "Plugins/Authoring/AvalonAI/Code/AvalonAI.cpp",
            "Plugins/Authoring/AvalonAI/Tests/AvalonAITests.cpp",
        }
        contract.validate_paths(paths)

    def test_plugin_package_without_manifest_fails(self) -> None:
        paths = valid_tree() | {"Plugins/Integrations/MerlinsWorkshop/Code/Provider.cpp"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "missing plugin.json"):
            contract.validate_paths(paths)

    def test_unknown_plugin_category_fails(self) -> None:
        paths = valid_tree() | {"Plugins/Misc/Thing/plugin.json"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected plug-in"):
            contract.validate_paths(paths)

    def test_loose_plugin_category_file_fails(self) -> None:
        paths = valid_tree() | {"Plugins/Authoring/notes.txt"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected plug-in"):
            contract.validate_paths(paths)

    def test_inherited_engine_root_fails(self) -> None:
        paths = valid_tree() | {"Code/Framework/AzCore/CMakeLists.txt"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "inherited O3DE"):
            contract.validate_paths(paths)

    def test_inherited_engine_file_fails(self) -> None:
        paths = valid_tree() | {"engine.json"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "inherited O3DE"):
            contract.validate_paths(paths)

    def test_stock_gem_fails(self) -> None:
        paths = valid_tree() | {"Gems/Atom/gem.json"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected root Gems"):
            contract.validate_paths(paths)

    def test_upstream_github_workflow_fails(self) -> None:
        paths = valid_tree() | {".github/workflows/android-build.yml"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected .github"):
            contract.validate_paths(paths)

    def test_unapproved_root_file_fails(self) -> None:
        paths = valid_tree() | {"Doxyfile"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected root files"):
            contract.validate_paths(paths)

    def test_non_foa_docs_root_fails(self) -> None:
        paths = valid_tree() | {"docs/upstream-engine/building.md"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected top-level"):
            contract.validate_paths(paths)

    def test_missing_required_path_fails(self) -> None:
        paths = valid_tree() - {"Plugins/plugin.schema.json"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "missing required"):
            contract.validate_paths(paths)

    def test_noncanonical_path_fails(self) -> None:
        with self.assertRaisesRegex(contract.RepositoryStructureError, "Non-canonical"):
            contract.validate_paths(valid_tree() | {"Plugins/../engine.json"})


if __name__ == "__main__":
    unittest.main()
