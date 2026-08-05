"""Windows O3DE Editor smoke test for the Alpha item visual selector.

This test opens the existing Item and Recipe Editor pane and verifies that the
live visual selector was injected exactly once. It does not load proprietary
FoA data, mutate the catalog, grant runtime permission, or deploy content.
"""

from __future__ import annotations

import os
import sys

import azlmbr.paths
import azlmbr.legacy.general as general
from PySide6 import QtWidgets


PANE_NAME = "Tainted Grail Item and Recipe Editor"
SELECTOR_OBJECT_NAME = "TaintedGrailItemVisualSelector"
VISUAL_TAB_NAME = "Visual Preview"


class Tests:
    pane_opened = (
        "Item and Recipe Editor pane opened",
        "Item and Recipe Editor pane did not open",
    )
    selector_created = (
        "Alpha item visual selector widget was created",
        "Alpha item visual selector widget was not created",
    )
    visual_tab_once = (
        "Visual Preview tab exists exactly once",
        "Visual Preview tab was missing or duplicated",
    )
    evidence_table_present = (
        "Evidence-backed preview table is present",
        "Evidence-backed preview table is missing",
    )
    live_previewer_present = (
        "Live O3DE preview frame is present",
        "Live O3DE preview frame is missing",
    )
    explicit_binding_controls = (
        "Explicit icon and asset binding controls are present",
        "Explicit icon or asset binding control is missing",
    )
    boundary_visible = (
        "Editor-preview authority boundary is visible",
        "Editor-preview authority boundary is missing",
    )


def _install_engine_test_paths() -> None:
    engine_root = os.path.normpath(azlmbr.paths.engroot)
    candidates = (
        os.path.join(engine_root, "Tools", "LyTestTools"),
        os.path.join(
            engine_root,
            "AutomatedTesting",
            "Gem",
            "PythonTests",
            "EditorPythonTestTools",
        ),
    )
    for candidate in candidates:
        if candidate not in sys.path:
            sys.path.insert(0, candidate)


def AlphaItemViewerLiveSmoke() -> None:
    _install_engine_test_paths()
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    general.open_pane(PANE_NAME)
    pane_visible = helper.wait_for_condition(
        lambda: general.is_pane_visible(PANE_NAME),
        20.0,
    )
    Report.critical_result(Tests.pane_opened, pane_visible)

    application = QtWidgets.QApplication.instance()

    def find_pane():
        for widget in application.allWidgets():
            if not isinstance(widget, QtWidgets.QDockWidget):
                continue
            if widget.objectName() == PANE_NAME or widget.windowTitle() == PANE_NAME:
                return widget
        return None

    def find_selector():
        pane = find_pane()
        return pane.findChild(QtWidgets.QWidget, SELECTOR_OBJECT_NAME) if pane else None

    selector_ready = helper.wait_for_condition(
        lambda: find_selector() is not None,
        20.0,
    )
    Report.critical_result(Tests.selector_created, selector_ready)

    pane = find_pane()
    selector = find_selector()
    tab_titles = []
    for tabs in pane.findChildren(QtWidgets.QTabWidget):
        tab_titles.extend(tabs.tabText(index) for index in range(tabs.count()))
    Report.critical_result(
        Tests.visual_tab_once,
        tab_titles.count(VISUAL_TAB_NAME) == 1,
    )

    evidence_table = next(
        (
            table
            for table in selector.findChildren(QtWidgets.QTableWidget)
            if table.accessibleName() == "Evidence-backed preview products"
        ),
        None,
    )
    Report.critical_result(Tests.evidence_table_present, evidence_table is not None)

    live_previewer = next(
        (
            widget
            for widget in selector.findChildren(QtWidgets.QWidget)
            if widget.accessibleName() == "Live O3DE item preview"
        ),
        None,
    )
    Report.critical_result(Tests.live_previewer_present, live_previewer is not None)

    button_texts = {
        button.text()
        for button in selector.findChildren(QtWidgets.QPushButton)
    }
    Report.critical_result(
        Tests.explicit_binding_controls,
        {
            "Use Selected as Icon Reference",
            "Use Selected as Asset Reference",
        }.issubset(button_texts),
    )

    boundary_visible = any(
        "Editor-preview boundary" in label.text()
        for label in selector.findChildren(QtWidgets.QLabel)
    )
    Report.result(Tests.boundary_visible, boundary_visible)
    pane.close()


if __name__ == "__main__":
    _install_engine_test_paths()
    from editor_python_test_tools.utils import Report

    Report.start_test(AlphaItemViewerLiveSmoke)
