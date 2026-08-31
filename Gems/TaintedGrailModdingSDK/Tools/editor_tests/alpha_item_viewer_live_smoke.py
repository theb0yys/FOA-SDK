"""Windows O3DE Editor smoke test for the working Item Viewer lifecycle.

The smoke uses an isolated automatic workspace seeded with one reviewed synthetic
Asset Processor import proof. No Asset Browser pane model exists at launch.
It then proves that the production Refresh Assets control regenerates that model
inside the Editor, loads its products, and preserves the selected visual across
close/reopen. The fixture grants no runtime, deployment, save, promotion, or
redistribution authority.
"""

from __future__ import annotations

import glob
import json
import os
import sys

import azlmbr.paths
import azlmbr.legacy.general as general
from PySide6 import QtWidgets

PANE_NAME = "Tainted Grail Item and Recipe Editor"
STATUS_PANE_NAME = "Tainted Grail SDK Status"
SELECTOR_OBJECT_NAME = "TaintedGrailItemVisualSelector"
GRID_OBJECT_NAME = "FOAItemViewerThumbnailGrid"
VISUAL_TAB_NAME = "Visual Preview"
MODEL_FILENAME = "foa-asset-browser-pane-model.json"
ASSET_ID_ROLE = 257  # Qt::UserRole + 1, matching ItemVisualLifecycleWidget.cpp.


class Tests:
    refresh_fixture_configured = (
        "Isolated exact-profile refresh fixture is configured",
        "Item Viewer refresh fixture environment is missing",
    )
    refresh_fixture_ready = (
        "Isolated exact-profile workspace reached Ready to author",
        "Isolated Item Viewer workspace did not become ready",
    )
    refresh_starts_without_model = (
        "Refresh fixture starts without an Asset Browser pane model",
        "Refresh fixture already contained an Asset Browser pane model",
    )
    pane_opened = ("Item and Recipe Editor pane opened", "Item and Recipe Editor pane did not open")
    selector_created = ("Item visual selector was created", "Item visual selector was not created")
    visual_tab_once = ("Visual Preview tab exists exactly once", "Visual Preview tab was missing or duplicated")
    thumbnail_grid_present = ("Official product-thumbnail grid is present", "Product-thumbnail grid is missing")
    evidence_table_present = ("Validated product table is retained", "Validated product table is missing")
    live_previewer_present = ("Live O3DE preview frame is present", "Live O3DE preview frame is missing")
    refresh_assets_control = ("Item visuals expose one Refresh Assets action", "Refresh Assets action is missing or disabled")
    internal_model_controls_hidden = ("Raw pane-model controls are hidden", "Raw pane-model controls leaked into the user workflow")
    explicit_binding_controls = ("Explicit icon and model assignment controls are present", "Assignment controls are missing")
    refresh_regenerated_model = (
        "Refresh Assets regenerated a proof-bound pane model",
        "Refresh Assets did not regenerate the expected proof-bound pane model",
    )
    refresh_loaded_products = (
        "Regenerated pane model loaded into the validated product browser",
        "Regenerated pane model did not load into the Item Viewer",
    )
    selection_restored_after_reopen = (
        "Selected visual restored after Item Viewer close/reopen",
        "Selected visual was not restored after Item Viewer close/reopen",
    )
    reopened = ("Pane closed and reopened with one viewer lifecycle", "Pane did not reconstruct cleanly after reopen")


def _install_engine_test_paths() -> None:
    engine_root = os.path.normpath(azlmbr.paths.engroot)
    for candidate in (
        os.path.join(engine_root, "Tools", "LyTestTools"),
        os.path.join(engine_root, "AutomatedTesting", "Gem", "PythonTests", "EditorPythonTestTools"),
    ):
        if candidate not in sys.path:
            sys.path.insert(0, candidate)


def _same_path(left: str, right: str) -> bool:
    return os.path.normcase(os.path.abspath(os.path.normpath(left))) == os.path.normcase(
        os.path.abspath(os.path.normpath(right))
    )


def ItemViewerLifecycleSmoke() -> None:
    _install_engine_test_paths()
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    application = QtWidgets.QApplication.instance()

    workspace_path = os.environ.get("FOA_SDK_ITEM_VIEWER_REFRESH_WORKSPACE", "")
    model_root = os.environ.get("FOA_SDK_ITEM_VIEWER_REFRESH_MODEL_ROOT", "")
    expected_proof_id = os.environ.get("FOA_SDK_ITEM_VIEWER_REFRESH_EXPECTED_PROOF_ID", "")
    fixture_configured = bool(workspace_path and model_root and expected_proof_id)
    Report.critical_result(Tests.refresh_fixture_configured, fixture_configured)

    def find_pane(name: str):
        for widget in application.allWidgets():
            if isinstance(widget, QtWidgets.QDockWidget) and (
                widget.objectName() == name or widget.windowTitle() == name
            ):
                return widget
        return None

    def find_item_pane():
        return find_pane(PANE_NAME)

    def find_selector():
        pane = find_item_pane()
        return pane.findChild(QtWidgets.QWidget, SELECTOR_OBJECT_NAME) if pane else None

    def find_product_table(selector):
        if not selector:
            return None
        for table in selector.findChildren(QtWidgets.QTableWidget):
            if table.accessibleName() in {"Evidence-backed preview products", "Selected asset details"}:
                return table
        return None

    def generated_models() -> list[str]:
        if not model_root:
            return []
        return sorted(
            glob.glob(os.path.join(model_root, "**", MODEL_FILENAME), recursive=True)
        )

    # Opening the status pane exercises the normal automatic-workspace startup
    # path. The PowerShell harness points LOCALAPPDATA at an isolated fixture, so
    # this cannot consume or overwrite a developer's real FOA-SDK workspace.
    general.open_pane(STATUS_PANE_NAME)
    Report.critical_result(
        Tests.refresh_fixture_ready,
        helper.wait_for_condition(
            lambda: general.is_pane_visible(STATUS_PANE_NAME)
            and find_pane(STATUS_PANE_NAME) is not None
            and any(
                label.text() == "Ready to author"
                for label in find_pane(STATUS_PANE_NAME).findChildren(QtWidgets.QLabel)
            ),
            20.0,
        ),
    )
    status_pane = find_pane(STATUS_PANE_NAME)
    if status_pane:
        status_pane.close()
        helper.wait_for_condition(lambda: not general.is_pane_visible(STATUS_PANE_NAME), 10.0)

    # The fixture deliberately removes every pane model before Editor launch.
    Report.critical_result(Tests.refresh_starts_without_model, len(generated_models()) == 0)

    general.open_pane(PANE_NAME)
    Report.critical_result(
        Tests.pane_opened,
        helper.wait_for_condition(lambda: general.is_pane_visible(PANE_NAME), 20.0),
    )
    Report.critical_result(
        Tests.selector_created,
        helper.wait_for_condition(lambda: find_selector() is not None, 20.0),
    )

    pane = find_item_pane()
    selector = find_selector()
    tab_titles = [
        tabs.tabText(index)
        for tabs in pane.findChildren(QtWidgets.QTabWidget)
        for index in range(tabs.count())
    ]
    Report.critical_result(Tests.visual_tab_once, tab_titles.count(VISUAL_TAB_NAME) == 1)
    grid = selector.findChild(QtWidgets.QListWidget, GRID_OBJECT_NAME)
    Report.critical_result(Tests.thumbnail_grid_present, grid is not None)
    product_table = find_product_table(selector)
    Report.critical_result(Tests.evidence_table_present, product_table is not None)
    Report.critical_result(
        Tests.live_previewer_present,
        any(
            widget.accessibleName() == "Live O3DE item preview"
            for widget in selector.findChildren(QtWidgets.QWidget)
        ),
    )

    buttons = selector.findChildren(QtWidgets.QPushButton)
    refresh_buttons = [button for button in buttons if button.text() == "Refresh Assets"]
    Report.critical_result(
        Tests.refresh_assets_control,
        len(refresh_buttons) == 1
        and not refresh_buttons[0].isHidden()
        and refresh_buttons[0].isEnabled(),
    )
    raw_choose_buttons = [button for button in buttons if button.text() == "Choose Model..."]
    raw_model_paths = [
        edit
        for edit in selector.findChildren(QtWidgets.QLineEdit)
        if edit.accessibleName() == "Loaded Asset Browser pane model path"
    ]
    Report.critical_result(
        Tests.internal_model_controls_hidden,
        all(button.isHidden() for button in raw_choose_buttons)
        and all(edit.isHidden() for edit in raw_model_paths),
    )

    button_texts = {button.text() for button in buttons}
    Report.critical_result(
        Tests.explicit_binding_controls,
        {"Use Selected as Icon Reference", "Use Selected as Asset Reference"}.issubset(button_texts),
    )

    # This is the actual product action under test. It must execute the shared
    # embedded refresh adapter, generate a new proof-bound pane model, then feed
    # that model back through the existing validated selector reload path.
    refresh_buttons[0].click()

    model_generated = helper.wait_for_condition(lambda: len(generated_models()) == 1, 30.0)
    generated_model_path = generated_models()[0] if model_generated else ""
    proof_bound = False
    if generated_model_path:
        try:
            with open(generated_model_path, "r", encoding="utf-8") as stream:
                model_document = json.load(stream)
            proof_bound = (
                model_document.get("DocumentKind") == "foa-asset-browser-pane-model"
                and model_document.get("SourceImportProofId") == expected_proof_id
                and bool(model_document.get("PaneEntries"))
            )
        except (OSError, ValueError, TypeError):
            proof_bound = False
    Report.critical_result(
        Tests.refresh_regenerated_model,
        model_generated and proof_bound,
    )

    product_loaded = helper.wait_for_condition(
        lambda: (
            find_selector() is not None
            and find_product_table(find_selector()) is not None
            and find_product_table(find_selector()).rowCount() > 0
        ),
        20.0,
    )
    selector = find_selector()
    grid = selector.findChild(QtWidgets.QListWidget, GRID_OBJECT_NAME) if selector else None
    hidden_path_matches = (
        len(raw_model_paths) == 1
        and bool(generated_model_path)
        and _same_path(raw_model_paths[0].text(), generated_model_path)
    )
    Report.critical_result(
        Tests.refresh_loaded_products,
        product_loaded
        and grid is not None
        and grid.count() > 0
        and hidden_path_matches
        and refresh_buttons[0].isEnabled(),
    )

    # Select one generated product so the existing QSettings/profile restore
    # path is exercised with real refreshed data rather than an empty viewer.
    grid.setCurrentRow(0)
    helper.wait_for_condition(lambda: grid.currentItem() is not None, 10.0)
    selected_asset_id = (
        str(grid.currentItem().data(ASSET_ID_ROLE)) if grid.currentItem() is not None else ""
    )

    pane.close()
    helper.wait_for_condition(lambda: not general.is_pane_visible(PANE_NAME), 10.0)
    general.open_pane(PANE_NAME)
    reopened = helper.wait_for_condition(
        lambda: general.is_pane_visible(PANE_NAME)
        and find_selector() is not None
        and find_selector().findChild(QtWidgets.QListWidget, GRID_OBJECT_NAME) is not None
        and find_product_table(find_selector()) is not None
        and find_product_table(find_selector()).rowCount() > 0,
        20.0,
    )
    Report.critical_result(Tests.reopened, reopened)

    reopened_grid = (
        find_selector().findChild(QtWidgets.QListWidget, GRID_OBJECT_NAME)
        if find_selector() is not None
        else None
    )
    selection_restored = helper.wait_for_condition(
        lambda: (
            reopened_grid is not None
            and reopened_grid.currentItem() is not None
            and selected_asset_id
            and str(reopened_grid.currentItem().data(ASSET_ID_ROLE)) == selected_asset_id
        ),
        20.0,
    )
    Report.critical_result(
        Tests.selection_restored_after_reopen,
        selection_restored,
    )
    find_item_pane().close()


if __name__ == "__main__":
    _install_engine_test_paths()
    from editor_python_test_tools.utils import Report

    Report.start_test(ItemViewerLifecycleSmoke)