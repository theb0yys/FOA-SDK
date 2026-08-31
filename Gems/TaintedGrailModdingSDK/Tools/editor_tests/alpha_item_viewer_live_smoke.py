"""Windows O3DE Editor smoke test for the working item-viewer UI lifecycle.

This smoke verifies production Editor integration, the official O3DE thumbnail
grid, registered live previewer, simplified exact-profile asset refresh UX,
explicit assignment controls, and close/reopen reconstruction. A configured
exact-profile preview cohort is still required for product-render evidence.
"""

from __future__ import annotations

import os
import sys

import azlmbr.paths
import azlmbr.legacy.general as general
from PySide6 import QtWidgets

PANE_NAME = "Tainted Grail Item and Recipe Editor"
SELECTOR_OBJECT_NAME = "TaintedGrailItemVisualSelector"
GRID_OBJECT_NAME = "FOAItemViewerThumbnailGrid"
VISUAL_TAB_NAME = "Visual Preview"


class Tests:
    pane_opened = ("Item and Recipe Editor pane opened", "Item and Recipe Editor pane did not open")
    selector_created = ("Item visual selector was created", "Item visual selector was not created")
    visual_tab_once = ("Visual Preview tab exists exactly once", "Visual Preview tab was missing or duplicated")
    thumbnail_grid_present = ("Official product-thumbnail grid is present", "Product-thumbnail grid is missing")
    evidence_table_present = ("Validated product table is retained", "Validated product table is missing")
    live_previewer_present = ("Live O3DE preview frame is present", "Live O3DE preview frame is missing")
    refresh_assets_control = ("Item visuals expose one Refresh Assets action", "Refresh Assets action is missing or disabled")
    internal_model_controls_hidden = ("Raw pane-model controls are hidden", "Raw pane-model controls leaked into the user workflow")
    explicit_binding_controls = ("Explicit icon and model assignment controls are present", "Assignment controls are missing")
    reopened = ("Pane closed and reopened with one viewer lifecycle", "Pane did not reconstruct cleanly after reopen")


def _install_engine_test_paths() -> None:
    engine_root = os.path.normpath(azlmbr.paths.engroot)
    for candidate in (
        os.path.join(engine_root, "Tools", "LyTestTools"),
        os.path.join(engine_root, "AutomatedTesting", "Gem", "PythonTests", "EditorPythonTestTools"),
    ):
        if candidate not in sys.path:
            sys.path.insert(0, candidate)


def ItemViewerLifecycleSmoke() -> None:
    _install_engine_test_paths()
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    application = QtWidgets.QApplication.instance()

    def find_pane():
        for widget in application.allWidgets():
            if isinstance(widget, QtWidgets.QDockWidget) and (
                widget.objectName() == PANE_NAME or widget.windowTitle() == PANE_NAME
            ):
                return widget
        return None

    def find_selector():
        pane = find_pane()
        return pane.findChild(QtWidgets.QWidget, SELECTOR_OBJECT_NAME) if pane else None

    general.open_pane(PANE_NAME)
    Report.critical_result(
        Tests.pane_opened,
        helper.wait_for_condition(lambda: general.is_pane_visible(PANE_NAME), 20.0),
    )
    Report.critical_result(
        Tests.selector_created,
        helper.wait_for_condition(lambda: find_selector() is not None, 20.0),
    )

    pane = find_pane()
    selector = find_selector()
    tab_titles = [
        tabs.tabText(index)
        for tabs in pane.findChildren(QtWidgets.QTabWidget)
        for index in range(tabs.count())
    ]
    Report.critical_result(Tests.visual_tab_once, tab_titles.count(VISUAL_TAB_NAME) == 1)
    Report.critical_result(
        Tests.thumbnail_grid_present,
        selector.findChild(QtWidgets.QListWidget, GRID_OBJECT_NAME) is not None,
    )
    Report.critical_result(
        Tests.evidence_table_present,
        any(
            table.accessibleName() in {"Evidence-backed preview products", "Selected asset details"}
            for table in selector.findChildren(QtWidgets.QTableWidget)
        ),
    )
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

    pane.close()
    helper.wait_for_condition(lambda: not general.is_pane_visible(PANE_NAME), 10.0)
    general.open_pane(PANE_NAME)
    reopened = helper.wait_for_condition(
        lambda: general.is_pane_visible(PANE_NAME)
        and find_selector() is not None
        and find_selector().findChild(QtWidgets.QListWidget, GRID_OBJECT_NAME) is not None,
        20.0,
    )
    Report.critical_result(Tests.reopened, reopened)
    find_pane().close()


if __name__ == "__main__":
    _install_engine_test_paths()
    from editor_python_test_tools.utils import Report

    Report.start_test(ItemViewerLifecycleSmoke)
