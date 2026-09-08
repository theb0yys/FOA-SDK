# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Verify native item/recipe images in the compiled Editor, using private local inputs."""
import hashlib
import json
import os
from pathlib import Path
import time
import traceback

import azlmbr.legacy.general as general
from PySide6 import QtCore, QtGui, QtWidgets
from shiboken6 import isValid


def run():
    output = Path(os.environ["FOA_SDK_PREVIEW_RESULT"])
    catalog_path = Path(os.environ["FOA_SDK_PREVIEW_CATALOG"])
    manifest = json.loads(Path(os.environ["FOA_SDK_PREVIEW_MANIFEST"]).read_text(encoding="utf-8"))
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    records = {record["RecordId"]: record for record in catalog["Records"]}
    artifacts = {entry["NativeAssetRef"]: entry for entry in manifest["ThumbnailArtifacts"]}
    before = hashlib.sha256(catalog_path.read_bytes()).hexdigest()
    result = {"status": "FAILED", "checks": []}
    references = []
    reference_ids = set()
    app = QtWidgets.QApplication.instance()
    beats = []
    gaps = []
    active_stage = ["starting"]
    slow_gaps = []
    timer = QtCore.QTimer()
    timer.setInterval(100)
    def beat():
        now = time.monotonic()
        if beats:
            gaps.append(now - beats[-1])
        if beats and now - beats[-1] >= 0.5:
            slow_gaps.append({"stage": active_stage[0], "seconds": round(now - beats[-1], 3)})
        beats.append(now)
    timer.timeout.connect(beat)

    def stage(name):
        active_stage[0] = name
        output.write_text(json.dumps({**result, "stage": name}, indent=2), encoding="utf-8")

    def control(name):
        # Resolve names in Qt rather than creating Python wrappers for every widget
        # in the entire Editor. Wrapper enumeration distorts the UI timer measurement.
        for window in app.topLevelWidgets():
            if not isValid(window):
                continue
            value = window if window.objectName() == name else window.findChild(QtWidgets.QWidget, name)
            if value is not None:
                for reference in (window, value):
                    if id(reference) not in reference_ids:
                        references.append(reference)
                        reference_ids.add(id(reference))
                return value
        raise AssertionError(f"Missing Editor control: {name}")

    def wait_for(predicate, timeout=35.0):
        started = time.monotonic()
        while not predicate():
            assert time.monotonic() - started < timeout, "Timed out waiting for preview state"
            general.idle_wait(0.1)

    def choose(name, record_id):
        combo = control(name)
        index = combo.findData(record_id)
        assert index >= 0, f"Missing canonical choice: {record_id}"
        combo.setCurrentIndex(index)
        general.idle_wait(0.2)

    def open_preview():
        tabs = control("economyTabs")
        index = next(index for index in range(tabs.count()) if tabs.tabText(index) == "Visual Preview")
        tabs.setCurrentIndex(index)
        general.idle_wait(0.3)

    def check_image(record_id):
        image = control("economyNativePreviewImage")
        native_ref = records[record_id]["NativeRefExact"]
        wait_for(lambda: image.property("nativeRefExact") == native_ref and not image.pixmap().isNull())
        assert image.isVisible(), "The selected image exists but is hidden"
        assert image.property("itemRecordId") == record_id
        source_path = Path(image.property("previewPath"))
        actual_hash = hashlib.sha256(source_path.read_bytes()).hexdigest()
        assert "sha256:" + actual_hash == artifacts[native_ref]["OutputSha256"]
        actual = image.pixmap().toImage().convertToFormat(QtGui.QImage.Format_RGBA8888)
        expected = QtGui.QPixmap(str(source_path)).scaled(
            image.contentsRect().size().boundedTo(QtCore.QSize(512, 512)),
            QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation
        ).toImage().convertToFormat(QtGui.QImage.Format_RGBA8888)
        assert actual == expected, "Displayed pixels do not match the selected native icon"
        assert min(actual.width(), actual.height()) >= 64
        return actual_hash

    try:
        general.idle_enable(True)
        general.idle_wait(2.0)
        started = time.monotonic()
        beats.append(started)
        stage("open_preview_without_asset_browser")
        pane_started = time.monotonic()
        general.open_pane("Tainted Grail Item and Recipe Editor")
        result["pane_construct_seconds"] = round(time.monotonic() - pane_started, 3)
        general.idle_wait(0.5)
        result["pane_first_idle_seconds"] = round(time.monotonic() - pane_started, 3)
        # Pane construction includes the entire authoring form. Record that cost
        # separately; the preview interaction budget covers image loading/selection.
        beats[:] = [time.monotonic()]
        timer.start()
        target = os.environ["FOA_SDK_PREVIEW_TARGET"]
        control("economyTabs").setCurrentIndex(0)
        choose("economyItemChoice", target)
        open_preview()
        first_hash = check_image(target)
        result["first_preview_seconds"] = round(time.monotonic() - started, 3)
        result["checks"].append("selected_item_visible_pixels_match_native_icon_without_asset_browser")
        if os.environ.get("FOA_SDK_PREVIEW_READY_ONLY") == "1":
            timer.stop()
            assert hashlib.sha256(catalog_path.read_bytes()).hexdigest() == before
            if os.environ.get("FOA_SDK_PREVIEW_SCREENSHOT"):
                control("economyTabs").parentWidget().grab().save(os.environ["FOA_SDK_PREVIEW_SCREENSHOT"])
            result["status"] = "PASSED"
            stage("ready")
            return
        stage("switch_item")
        other = next(record_id for record_id, record in records.items()
                     if record.get("NativeRefExact") in artifacts
                     and artifacts[record["NativeRefExact"]].get("OutputSha256") not in (None, "sha256:" + first_hash)
                     and artifacts[record["NativeRefExact"]]["Status"] == "generated")
        control("economyTabs").setCurrentIndex(0)
        choose("economyItemChoice", other)
        open_preview()
        assert check_image(other) != first_hash
        result["checks"].append("authoring_item_selection_changes_the_displayed_image")
        stage("recipe_output")
        recipe_output = next(link for link in catalog["RecipeOutputs"]
                             if records.get(link["ItemRecordId"], {}).get("NativeRefExact") in artifacts
                             and artifacts[records[link["ItemRecordId"]]["NativeRefExact"]]["Status"] == "generated")
        control("economyTabs").setCurrentIndex(1)
        choose("economyRecipeChoice", recipe_output["RecipeRecordId"])
        open_preview()
        assert control("economyPreviewTarget").currentData() == recipe_output["RecipeRecordId"]
        assert control("economyPreviewRecipeItem").currentData() == recipe_output["ItemRecordId"]
        check_image(recipe_output["ItemRecordId"])
        linked = control("economyPreviewRecipeItem")
        for index in range(1, linked.count()):
            record_id = linked.itemData(index)
            if record_id != recipe_output["ItemRecordId"] and artifacts.get(records[record_id]["NativeRefExact"], {}).get("Status") == "generated":
                linked.setCurrentIndex(index)
                general.idle_wait(0.2)
                check_image(record_id)
                result["checks"].append("recipe_linked_item_selection_changes_the_image")
                break
        result["checks"].append("recipe_defaults_to_its_resolved_output_icon")
        stage("unsupported_icon")
        unsupported = next(record_id for record_id, record in records.items()
                           if artifacts.get(record.get("NativeRefExact"), {}).get("Status") == "unsupported")
        choose("economyPreviewTarget", unsupported)
        image = control("economyNativePreviewImage")
        assert image.pixmap().isNull() and not image.property("previewPath")
        assert "No supported game icon" in image.text()
        result["checks"].append("unsupported_item_clears_previous_image_and_explains_missing_icon")
        stage("cancel_refresh")
        choose("economyPreviewTarget", target)
        check_image(target)
        refresh = control("economyNativePreviewRefresh")
        refresh.click()
        general.idle_wait(0.5)
        assert refresh.text() == "Cancel refresh"
        refresh.click()
        wait_for(lambda: refresh.text() != "Cancel refresh")
        assert "cancelled" in control("economyNativePreviewStatus").text().lower()
        check_image(target)
        result["checks"].append("cancelled_reader_preserves_selected_icon")
        stage("custom_visuals_separate")
        control("economyPreviewTabs").setCurrentIndex(1)
        general.idle_wait(0.2)
        selector = control("TaintedGrailItemVisualSelector")
        buttons = selector.findChildren(QtWidgets.QPushButton)
        for text in ("Use Selected as Icon Reference", "Use Selected as Asset Reference"):
            assert not next(button for button in buttons if button.text() == text).isEnabled()
        control("economyPreviewTabs").setCurrentIndex(0)
        result["checks"].append("native_image_does_not_enable_custom_product_binding")
        stage("reopen")
        timer.stop()
        pane_started = time.monotonic()
        general.close_pane("Tainted Grail Item and Recipe Editor")
        result["pane_close_seconds"] = round(time.monotonic() - pane_started, 3)
        general.idle_wait(0.5)
        pane_started = time.monotonic()
        general.open_pane("Tainted Grail Item and Recipe Editor")
        result["pane_reopen_seconds"] = round(time.monotonic() - pane_started, 3)
        general.idle_wait(0.5)
        beats[:] = [time.monotonic()]
        timer.start()
        control("economyTabs").setCurrentIndex(0)
        choose("economyItemChoice", target)
        open_preview()
        check_image(target)
        result["checks"].append("reopened_pane_loads_selected_icon")
        timer.stop()
        result["max_preview_ui_gap_seconds"] = round(max(gaps, default=0.0), 3)
        result["slow_ui_gaps"] = slow_gaps
        assert result["max_preview_ui_gap_seconds"] < 3.0, "Preview interaction stalled the Editor"
        assert hashlib.sha256(catalog_path.read_bytes()).hexdigest() == before, "Preview changed persisted authoring data"
        result["checks"].append("preview_does_not_write_catalog")
        if os.environ.get("FOA_SDK_PREVIEW_SCREENSHOT"):
            control("economyTabs").parentWidget().grab().save(os.environ["FOA_SDK_PREVIEW_SCREENSHOT"])
        result["status"] = "PASSED"
        stage("ready")
    except Exception:
        result["error"] = traceback.format_exc()
        if os.environ.get("FOA_SDK_PREVIEW_SCREENSHOT"):
            control("economyTabs").parentWidget().grab().save(os.environ["FOA_SDK_PREVIEW_SCREENSHOT"])
        stage("failed")
    finally:
        timer.stop()
        if os.environ.get("FOA_SDK_PREVIEW_KEEP_OPEN") != "1":
            general.exit_no_prompt()


run()
