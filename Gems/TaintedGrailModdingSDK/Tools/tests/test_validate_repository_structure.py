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
            "docs/tainted-grail-modding/INFORMATION_ARCHITECTURE.md",
            "docs/tainted-grail-modding/getting-started/README.md",
            "docs/tainted-grail-modding/systems/README.md",
        }
    )
    paths.update(contract.ALLOWED_GITHUB_FILES)
    return paths


class RepositoryStructureContractTests(unittest.TestCase):
    def test_reviewed_product_tree_passes(self) -> None:
        contract.validate_paths(valid_tree())

    def test_documentation_hub_roots_are_explicitly_governed(self) -> None:
        self.assertEqual(
            contract.ALLOWED_DOC_TREES,
            {"tainted-grail-sdk", "tainted-grail-modding"},
        )
        self.assertEqual(contract.ALLOWED_DOC_ROOT_FILES, {"docs/README.md"})
        contract.validate_paths(
            valid_tree()
            | {
                "docs/tainted-grail-modding/runtime/README.md",
                "docs/tainted-grail-modding/reference/README.md",
            }
        )

    def test_automatic_static_workflow_is_required(self) -> None:
        self.assertIn(
            contract.AUTOMATIC_STATIC_WORKFLOW,
            contract.ALLOWED_GITHUB_FILES,
        )
        self.assertIn(
            contract.AUTOMATIC_STATIC_WORKFLOW,
            contract.REQUIRED_PATHS,
        )
        paths = valid_tree() - {contract.AUTOMATIC_STATIC_WORKFLOW}
        with self.assertRaisesRegex(
            contract.RepositoryStructureError,
            "missing required",
        ):
            contract.validate_paths(paths)

    def test_installer_launcher_source_passes(self) -> None:
        paths = valid_tree() | {
            "Installer/Launcher/Windows/InstallerPayload.cs",
            "Installer/Launcher/Windows/WindowsInstallerRunner.cs",
        }
        contract.validate_paths(paths)

    def test_obsolete_installer_suite_lane_fails(self) -> None:
        paths = valid_tree() | {"Installer/Suites/Developer/suite.json"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected installer"):
            contract.validate_paths(paths)

    def test_obsolete_installer_package_lane_fails(self) -> None:
        paths = valid_tree() | {"Installer/Packages/Editor/package.json"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected installer"):
            contract.validate_paths(paths)

    def test_unknown_installer_lane_fails(self) -> None:
        paths = valid_tree() | {"Installer/Misc/Thing/file.txt"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected installer"):
            contract.validate_paths(paths)

    def test_loose_installer_lane_file_fails(self) -> None:
        paths = valid_tree() | {"Installer/Assets/banner.png"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected installer"):
            contract.validate_paths(paths)

    def test_legacy_gem_installer_path_fails(self) -> None:
        paths = valid_tree() | {"Gems/TaintedGrailModdingSDK/Installer/CMakeLists.txt"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "legacy Gem-owned installer"):
            contract.validate_paths(paths)

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

    def test_loose_docs_root_file_fails(self) -> None:
        paths = valid_tree() | {"docs/private-notes.md"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "unexpected top-level"):
            contract.validate_paths(paths)

    def test_missing_required_path_fails(self) -> None:
        paths = valid_tree() - {"Installer/Launcher/Windows/InstallerWizardForm.cs"}
        with self.assertRaisesRegex(contract.RepositoryStructureError, "missing required"):
            contract.validate_paths(paths)

    def test_noncanonical_path_fails(self) -> None:
        with self.assertRaisesRegex(contract.RepositoryStructureError, "Non-canonical"):
            contract.validate_paths(valid_tree() | {"Installer/../engine.json"})


if __name__ == "__main__":
    unittest.main()
