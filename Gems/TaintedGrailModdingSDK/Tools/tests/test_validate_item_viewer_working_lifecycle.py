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

import validate_item_viewer_working_lifecycle as contract


class ItemViewerWorkingLifecycleTests(unittest.TestCase):
    FIXTURE_PATHS = (
        "Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt",
        "Gems/TaintedGrailModdingSDK/Code/Source/AssetBrowserPreviewRefreshService.cpp",
        "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualLifecycleWidget.cpp",
        "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualSelectorInstallerSystemComponent.cpp",
        "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualSelectorWidget.cpp",
        "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_editor_files.cmake",
        "Gems/TaintedGrailModdingSDK/Tools/editor_tests/alpha_item_viewer_live_smoke.py",
    )

    def copy_fixture(self, root: Path) -> None:
        for relative in self.FIXTURE_PATHS:
            source = REPO_ROOT / relative
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    def mutate(self, root: Path, relative: str, old: str, new: str, *, all_occurrences: bool = False) -> None:
        path = root / relative
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text)
        path.write_text(
            text.replace(old, new) if all_occurrences else text.replace(old, new, 1),
            encoding="utf-8",
        )

    def test_current_item_viewer_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            contract.validate_item_viewer(root)

    def test_missing_production_build_registration_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_editor_files.cmake",
                "Source/ItemVisualSelectorWidget.cpp",
                "Source/RemovedItemVisualSelectorWidget.cpp",
            )
            with self.assertRaisesRegex(RuntimeError, "production build registration"):
                contract.validate_item_viewer(root)

    def test_raw_model_chooser_must_remain_hidden(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualLifecycleWidget.cpp",
                "m_chooseModel->hide();",
                "m_chooseModel->show();",
            )
            with self.assertRaisesRegex(RuntimeError, "hidden raw JSON chooser"):
                contract.validate_item_viewer(root)

    def test_automatic_profile_filter_is_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualLifecycleWidget.cpp",
                "CandidateMatchesActiveProfile",
                "CandidateMatchesAnyProfile",
                all_occurrences=True,
            )
            with self.assertRaisesRegex(RuntimeError, "exact-profile candidate filtering"):
                contract.validate_item_viewer(root)

    def test_refresh_must_regenerate_the_shared_model(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualLifecycleWidget.cpp",
                "RefreshActiveProfileModel",
                "LoadLatestAvailableModel",
            )
            with self.assertRaisesRegex(RuntimeError, "refresh-to-generation service integration"):
                contract.validate_item_viewer(root)

    def test_refresh_must_use_embedded_python_not_an_external_process(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            path = root / "Gems/TaintedGrailModdingSDK/Code/Source/AssetBrowserPreviewRefreshService.cpp"
            path.write_text(path.read_text(encoding="utf-8") + "\nQProcess forbidden;\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "remain inside the Editor process"):
                contract.validate_item_viewer(root)

    def test_pane_model_generator_must_ship_with_the_installed_editor(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt",
                "scripts/foa-sdk",
                "scripts/missing-item-viewer-tool",
            )
            with self.assertRaisesRegex(RuntimeError, "private installed generator location"):
                contract.validate_item_viewer(root)

    def test_internal_model_path_cannot_return_to_settings(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            path = root / "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualLifecycleWidget.cpp"
            path.write_text(
                path.read_text(encoding="utf-8")
                + '\nsettings.setValue(prefix + QStringLiteral("modelPath"), QStringLiteral("forbidden"));\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(RuntimeError, "stale UX state"):
                contract.validate_item_viewer(root)


if __name__ == "__main__":
    unittest.main()