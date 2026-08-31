#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = TOOLS_ROOT.parents[2]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import validate_editor_lifecycle as contract


class EditorLifecycleContractTests(unittest.TestCase):
    def copy_fixture(self, root: Path) -> None:
        paths = {
            contract.HUB_SOURCE,
            contract.PACK_MANAGER_SOURCE,
            contract.FOUNDATION_SERVICE_HEADER,
            contract.FOUNDATION_STATUS_SOURCE,
            contract.EXTENSION_HOST,
        }
        for pane in contract.PANES:
            paths.update(
                {
                    pane.registration_source,
                    pane.widget_source,
                    pane.manifest,
                }
            )
        for relative in paths:
            source = REPO_ROOT / relative
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    def mutate(self, root: Path, relative: str, old: str, new: str) -> None:
        path = root / relative
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text)
        path.write_text(text.replace(old, new, 1), encoding="utf-8")

    def test_current_inventory_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            contract.validate_editor_lifecycle(root)

    def test_missing_hub_route_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                contract.HUB_SOURCE,
                '"Tainted Grail Map Editor (Road Atlas)"',
                '"Removed Road Atlas route"',
            )
            with self.assertRaisesRegex(contract.EditorLifecycleError, "Hub route"):
                contract.validate_editor_lifecycle(root)

    def test_missing_optional_gem_unregister_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            road_module = contract.PANES[-1].registration_source
            self.mutate(root, road_module, "UnregisterExtension", "RemovedExtensionLifecycle")
            with self.assertRaisesRegex(contract.EditorLifecycleError, "deactivation"):
                contract.validate_editor_lifecycle(root)

    def test_duplicate_layout_key_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            road_module = contract.PANES[-1].registration_source
            self.mutate(
                root,
                road_module,
                'QStringLiteral("RoadAtlasAuthoring.MapEditorDock")',
                'QStringLiteral("AvalonAIAuthoring.Editor")',
            )
            with self.assertRaisesRegex(contract.EditorLifecycleError, "unique layout"):
                contract.validate_editor_lifecycle(root)

    def test_secondary_pane_menu_entry_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            road_module = contract.PANES[-1].registration_source
            self.mutate(
                root,
                road_module,
                "options.showInMenu = false;",
                "options.showInMenu = true;",
            )
            with self.assertRaisesRegex(contract.EditorLifecycleError, "menu-hidden"):
                contract.validate_editor_lifecycle(root)

    def test_home_menu_label_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                contract.BASE_SOURCE,
                'hubOptions.optionalMenuText = QStringLiteral("FOA-SDK Home");',
                'hubOptions.optionalMenuText = QStringLiteral("Development Hub");',
            )
            with self.assertRaisesRegex(contract.EditorLifecycleError, "front door"):
                contract.validate_editor_lifecycle(root)

    def test_manifest_file_picker_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            path = root / contract.PACK_MANAGER_SOURCE
            path.write_text(
                path.read_text(encoding="utf-8") + "\nQFileDialog forbidden;\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(contract.EditorLifecycleError, "file picker"):
                contract.validate_editor_lifecycle(root)

    def test_setup_readiness_without_install_detection_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                contract.FOUNDATION_SERVICE_HEADER,
                "return m_gameInstallDetected",
                "return true",
            )
            with self.assertRaisesRegex(contract.EditorLifecycleError, "readiness"):
                contract.validate_editor_lifecycle(root)

    def test_non_atomic_extension_store_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(root, contract.EXTENSION_HOST, "QSaveFile file(", "QFile file(")
            with self.assertRaisesRegex(contract.EditorLifecycleError, "QSaveFile file"):
                contract.validate_editor_lifecycle(root)

    def test_missing_host_json_validation_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                contract.EXTENSION_HOST,
                "QJsonDocument::fromJson",
                "QJsonDocument::invalidFromJson",
            )
            with self.assertRaisesRegex(contract.EditorLifecycleError, "QJsonDocument::fromJson"):
                contract.validate_editor_lifecycle(root)

    def test_runtime_process_in_editor_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            widget = contract.PANES[-1].widget_source
            path = root / widget
            path.write_text(path.read_text(encoding="utf-8") + "\nQProcess forbidden;\n", encoding="utf-8")
            with self.assertRaisesRegex(contract.EditorLifecycleError, "forbidden"):
                contract.validate_editor_lifecycle(root)

    def test_missing_widget_build_ownership_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            manifest = contract.PANES[-1].manifest
            path = root / manifest
            text = path.read_text(encoding="utf-8")
            self.assertIn("Source/RoadAtlasEditorWidget.cpp", text)
            path.write_text(
                text.replace("Source/RoadAtlasEditorWidget.cpp", "Source/RemovedWidget.cpp"),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(contract.EditorLifecycleError, "build ownership"):
                contract.validate_editor_lifecycle(root)


if __name__ == "__main__":
    unittest.main()