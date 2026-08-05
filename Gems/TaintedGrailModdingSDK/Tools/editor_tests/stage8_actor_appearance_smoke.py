"""Windows O3DE Editor smoke for the Stage 8 actor appearance preview.

Uses only widget structure and project-owned editor state. It does not load
proprietary FoA assets, bind a product, mutate saves, deploy, or invoke runtime.
"""

from __future__ import annotations

import os
import sys

import azlmbr.paths
import azlmbr.legacy.general as general
from PySide6 import QtWidgets

PANE_NAME = "Tainted Grail Actor and Troop Editor"
WIDGET_NAME = "TaintedGrailActorAppearancePreview"
TAB_NAME = "Appearance Preview"


class Tests:
    pane = ("Actor/Troop pane opened", "Actor/Troop pane did not open")
    widget = ("Stage 8 widget exists", "Stage 8 widget is missing")
    tab = ("Appearance Preview tab exists once", "Appearance Preview tab is missing or duplicated")
    controls = ("Stage 8 controls exist", "One or more Stage 8 controls are missing")
    boundary = ("Stage 8 authority boundary is visible", "Stage 8 authority boundary is missing")


def _paths() -> None:
    engine_root = os.path.normpath(azlmbr.paths.engroot)
    for candidate in (
        os.path.join(engine_root, "Tools", "LyTestTools"),
        os.path.join(engine_root, "AutomatedTesting", "Gem", "PythonTests", "EditorPythonTestTools"),
    ):
        if candidate not in sys.path:
            sys.path.insert(0, candidate)


def Stage8ActorAppearanceSmoke() -> None:
    _paths()
    from editor_python_test_tools.utils import Report
    from editor_python_test_tools.utils import TestHelper as helper

    helper.init_idle()
    general.open_pane(PANE_NAME)
    Report.critical_result(
        Tests.pane,
        helper.wait_for_condition(lambda: general.is_pane_visible(PANE_NAME), 20.0),
    )

    app = QtWidgets.QApplication.instance()

    def pane_widget():
        for widget in app.allWidgets():
            if isinstance(widget, QtWidgets.QDockWidget) and (
                widget.objectName() == PANE_NAME or widget.windowTitle() == PANE_NAME
            ):
                return widget
        return None

    def preview_widget():
        pane = pane_widget()
        return pane.findChild(QtWidgets.QWidget, WIDGET_NAME) if pane else None

    Report.critical_result(
        Tests.widget,
        helper.wait_for_condition(lambda: preview_widget() is not None, 20.0),
    )
    pane = pane_widget()
    preview = preview_widget()
    tab_titles = [
        tabs.tabText(index)
        for tabs in pane.findChildren(QtWidgets.QTabWidget)
        for index in range(tabs.count())
    ]
    Report.critical_result(Tests.tab, tab_titles.count(TAB_NAME) == 1)

    required_names = {
        "ActorAppearanceActorSelector",
        "ActorAppearancePaneModelPath",
        "ActorAppearanceProductTable",
        "ActorAppearanceLivePreview",
        "ActorAppearanceEquipmentTable",
        "ActorAppearanceBindPortrait",
        "ActorAppearanceBindModel",
    }
    found_names = {
        widget.objectName()
        for widget in preview.findChildren(QtWidgets.QWidget)
        if widget.objectName()
    }
    Report.critical_result(Tests.controls, required_names.issubset(found_names))

    boundary = preview.findChild(QtWidgets.QLabel, "ActorAppearanceBoundaryWarning")
    Report.result(
        Tests.boundary,
        boundary is not None and "does not reconstruct" in boundary.text(),
    )
    pane.close()


if __name__ == "__main__":
    _paths()
    from editor_python_test_tools.utils import Report

    Report.start_test(Stage8ActorAppearanceSmoke)
