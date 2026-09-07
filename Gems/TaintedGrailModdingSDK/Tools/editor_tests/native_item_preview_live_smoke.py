# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Real installed-game icon workflow, run inside the compiled Editor.

Inputs and result paths come from the private local harness. No proprietary
payload, screenshot, or machine path belongs in a committed fixture.
"""
import json
import os
from pathlib import Path
import time
import traceback

import azlmbr.paths
import azlmbr.legacy.general as general
from PySide6 import QtCore, QtWidgets


def run():
    output = Path(os.environ["FOA_SDK_NATIVE_ITEM_RESULT"])
    result = {"status": "FAILED", "checks": []}
    keep_open = os.environ.get("FOA_SDK_NATIVE_ITEM_KEEP_OPEN") == "1"
    inspect_only = os.environ.get("FOA_SDK_NATIVE_ITEM_MODE") == "inspect"
    result["mode"] = "cached-inspection" if inspect_only else "refresh"
    def record(stage):
        output.write_text(json.dumps({**result, "stage": stage}, indent=2), encoding="utf-8")
    def rows(tree):
        model = tree.model()
        categories = [model.index(n, 0) for n in range(model.rowCount())]
        return [model.index(k, 0, category) for category in categories for k in range(model.rowCount(category))]
    app = QtWidgets.QApplication.instance()
    try:
        general.idle_enable(True)
        general.idle_wait(3.0)
        general.open_pane("Tainted Grail Asset Browser Preview")
        general.idle_wait(1.0)
        # Keep the owning widget wrappers alive throughout O3DE's nested idle loop.
        widget_references = app.allWidgets()
        button = next(w for w in widget_references if w.objectName() == "NativeItemRefreshButton")
        pane = button.parentWidget()
        while pane and not pane.findChild(QtWidgets.QTreeWidget, "AssetPreviewTree"):
            pane = pane.parentWidget()
        tree = pane.findChild(QtWidgets.QTreeWidget, "AssetPreviewTree")
        status = pane.findChild(QtWidgets.QLabel, "AssetPreviewStatus")
        search = pane.findChild(QtWidgets.QLineEdit, "AssetPreviewSearch")
        category = pane.findChild(QtWidgets.QComboBox, "AssetPreviewCategory")
        subcategory = pane.findChild(QtWidgets.QComboBox, "AssetPreviewSubcategory")
        image = pane.findChild(QtWidgets.QLabel, "AssetPreviewImage")
        custom = next(edit for edit in pane.findChildren(QtWidgets.QLineEdit)
                      if edit.placeholderText() == "Workspace Assets folder")
        expected_assets = Path(azlmbr.paths.projectroot) / "Assets"
        assert os.path.normcase(os.path.realpath(custom.text())) == os.path.normcase(os.path.realpath(expected_assets)), "Custom Assets does not use the active project"
        result["checks"].append("custom_assets_uses_active_project")
        result["checks"].append("compiled_pane_and_controls_found")
        record("refresh")
        initial_deadline = time.monotonic() + 190
        while button.text() != "Refresh assets" and time.monotonic() < initial_deadline:
            general.idle_wait(0.25)
        assert button.text() == "Refresh assets", "Initial preview load did not finish"
        if not inspect_only and button.text() == "Refresh assets":
            button.click()
        started = time.monotonic()
        heartbeats = []
        timer = QtCore.QTimer()
        timer.setInterval(100)
        timer.timeout.connect(lambda: heartbeats.append(time.monotonic()))
        timer.start()
        while button.text() != "Refresh assets" and time.monotonic() - started < 190:
            general.idle_wait(0.25)
            result["progress"] = status.text()
            record("refresh_running")
        timer.stop()
        finished = time.monotonic()
        elapsed = finished - started
        assert button.text() == "Refresh assets", "Refresh exceeded its supervised timeout"
        assert status.text().startswith("Loaded "), status.text()
        if os.environ.get("FOA_SDK_NATIVE_ITEM_SCREENSHOT"):
            pane.grab().save(os.environ["FOA_SDK_NATIVE_ITEM_SCREENSHOT"])
        record("collecting_rows")
        all_rows = rows(tree)
        assert len(all_rows) >= 100, "Real game preview must contain an actual item cohort"
        generated = [row for row in all_rows if row.siblingAtColumn(2).data() == "generated"]
        assert generated, "No decoded icons were loaded"
        assert elapsed < 180, "End-to-end item preview exceeded 180 seconds"
        gaps = [right - left for left, right in zip(heartbeats, heartbeats[1:])]
        if heartbeats:
            gaps.append(heartbeats[0] - started)
            gaps.append(finished - heartbeats[-1])
        result.update({"item_count": len(all_rows), "generated_count": len(generated),
                       "elapsed_seconds": round(elapsed, 3), "max_ui_gap_seconds": round(max(gaps, default=0), 3)})
        assert inspect_only or len(heartbeats) >= 5, "The UI event loop did not remain active"
        assert max(gaps, default=0) < 3.0, "The UI stalled for more than 3 seconds"
        result["checks"].extend(["cached_items_loaded"] if inspect_only else
                                ["actual_refresh_produced_item_rows", "ui_responsive_during_extraction"])

        groups = [category.itemData(i) for i in range(1, category.count())]
        assert {"Armor", "Weapons", "Ingredients", "Consumables", "Developer templates"}.issubset(groups), groups
        assert len(groups) <= 20 and not any(group.startswith("Items / ") for group in groups), groups
        category_counts = {}
        for index, group in enumerate(groups, 1):
            category.setCurrentIndex(index)
            general.idle_wait(0.1)
            category_rows = rows(tree)
            assert category_rows and all(row.parent().data().split(" / ", 1)[0] == group for row in category_rows), group
            category_counts[group] = len(category_rows)
        assert sum(category_counts.values()) == len(all_rows), "Categories lost or duplicated items"
        category.setCurrentIndex(category.findData("Weapons"))
        subcategory.setCurrentIndex(subcategory.findData("Weapons / Swords"))
        assert rows(tree) and all(row.parent().data() == "Weapons / Swords" for row in rows(tree))
        category.setCurrentIndex(category.findData("Armor"))
        assert subcategory.currentData() == "", "Changing category retained an incompatible subcategory"
        light_index = subcategory.findData("Armor / Light")
        assert light_index > 0, "Light armor subcategory is missing"
        subcategory.setCurrentIndex(light_index)
        assert rows(tree) and all(row.parent().data() == "Armor / Light" for row in rows(tree))
        result["category_counts"] = category_counts
        result["checks"].extend(["readable_category_groups_cover_all_items", "category_and_subcategory_filters_match_rows"])
        selected_name = next(row.data() for row in rows(tree) if row.siblingAtColumn(2).data() == "generated")
        search.setText(selected_name)
        general.idle_wait(0.5)
        filtered = rows(tree)
        assert filtered and len(filtered) < len(all_rows), "Item search did not filter results"
        chosen = next(row for row in filtered if row.siblingAtColumn(2).data() == "generated")
        tree.setCurrentIndex(chosen)
        general.idle_wait(0.25)
        assert tree.visualRect(chosen).height() >= 72, "Thumbnail rows are clipped below the icon height"
        assert image.pixmap() is not None and not image.pixmap().isNull(), "Selecting the item did not display its icon"
        assert image.pixmap().width() > 1 and image.pixmap().height() > 1
        result["checks"].extend(["item_search_filters_rows", "selection_displays_decoded_icon"])
        before_cancel = len(filtered)
        button.click()
        general.idle_wait(0.3)
        assert button.text() == "Cancel refresh", "Refresh did not expose cancellation"
        button.click()
        for _ in range(40):
            if button.text() == "Refresh assets":
                break
            general.idle_wait(0.1)
        assert "cancelled" in status.text().lower(), status.text()
        assert len(rows(tree)) == before_cancel, "Cancellation replaced the completed snapshot"
        result["checks"].append("cancel_preserves_completed_previews")
        load = next(b for b in pane.findChildren(QtWidgets.QPushButton) if b.text() == "Load assets")
        load.click()
        reload_deadline = time.monotonic() + 35
        while button.text() != "Refresh assets" and time.monotonic() < reload_deadline:
            general.idle_wait(0.25)
        assert status.text().startswith("Loaded "), status.text()
        assert category.currentData() == "Armor" and subcategory.currentData() == "Armor / Light", "Reload discarded category filters"
        result["checks"].append("completed_previews_reload")
        result["checks"].append("category_filters_survive_reload")
        search.clear()
        category.setCurrentIndex(0)
        general.idle_wait(0.5)
        pane.window().raise_()
        pane.window().activateWindow()
        if os.environ.get("FOA_SDK_NATIVE_ITEM_SCREENSHOT"):
            # This image is private, outside the checkout, and never release evidence.
            pane.grab().save(os.environ["FOA_SDK_NATIVE_ITEM_SCREENSHOT"])
        result["status"] = "PASSED"
    except Exception as error:
        result["error"] = str(error)
        result["traceback"] = traceback.format_exc()
        result["remaining_preview_widgets"] = [w.objectName() for w in app.allWidgets()
                                                if w.objectName().startswith(("AssetPreview", "NativeItem"))]
    finally:
        record("complete")
        if not keep_open:
            general.exit_no_prompt()


run()
