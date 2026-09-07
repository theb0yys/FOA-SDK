# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Compiled Editor economy workflow in an isolated, private workspace."""
import json
import os
from pathlib import Path
import time
import traceback

import azlmbr.legacy.general as general
from PySide6 import QtCore, QtWidgets
from shiboken6 import isValid


def run():
    output = Path(os.environ["FOA_SDK_ECONOMY_RESULT"])
    workspace = Path(os.environ["FOA_SDK_ECONOMY_WORKSPACE"])
    result = {"status": "FAILED", "checks": []}
    app = QtWidgets.QApplication.instance()
    references = []
    reference_ids = set()

    def record(stage):
        output.write_text(json.dumps({**result, "stage": stage}, indent=2), encoding="utf-8")

    def widgets():
        current = app.allWidgets()
        references.extend(value for value in current if id(value) not in reference_ids)
        reference_ids.update(id(value) for value in current)
        return [value for value in current if isValid(value)]

    def control(name):
        return next(value for value in reversed(widgets()) if value.objectName() == name)

    def button(text):
        return next(value for value in reversed(widgets()) if isinstance(value, QtWidgets.QPushButton) and value.text() == text)

    def input_name(name, trigger):
        accepted = []
        dialog_timer = QtCore.QTimer()
        def accept():
            dialog = next((value for value in widgets() if isinstance(value, QtWidgets.QInputDialog) and value.isVisible()), None)
            if dialog is None:
                return
            dialog_timer.stop()
            references.append(dialog)
            dialog.setTextValue(name)
            result["name_dialog"] = {"title": dialog.windowTitle(), "value": dialog.textValue()}
            record("accepting_name_dialog")
            accepted.append(True)
            dialog.done(QtWidgets.QDialog.Accepted)
        dialog_timer.setInterval(100)
        dialog_timer.timeout.connect(accept)
        dialog_timer.start()
        QtCore.QMetaObject.invokeMethod(control(trigger), "click", QtCore.Qt.QueuedConnection)
        for _ in range(100):
            general.idle_wait(0.1)
            if accepted:
                break
        dialog_timer.stop()
        assert accepted, "The name dialog did not complete through the test"
        assert "Created " in control("economyStatus").text(), control("economyStatus").text()

    def open_workspace(path):
        record("opening_status_pane")
        general.open_pane("Tainted Grail SDK Status")
        general.idle_wait(0.5)
        def accept():
            dialog = next(value for value in app.topLevelWidgets() if isinstance(value, QtWidgets.QFileDialog) and value.isVisible())
            references.append(dialog)
            record("accepting_workspace_dialog")
            dialog.setDirectory(str(path.parent))
            dialog.selectFile(path.name)
            def finish():
                file_name = dialog.findChild(QtWidgets.QLineEdit, "fileNameEdit")
                assert file_name is not None
                file_name.setText(str(path))
                result["workspace_selection"] = dialog.selectedFiles()
                assert Path(dialog.selectedFiles()[0]).resolve() == path.resolve()
                dialog.done(QtWidgets.QDialog.Accepted)
                result["workspace_dialog"] = {"visible": dialog.isVisible(), "result": dialog.result()}
                record("accepted_workspace_dialog")
            QtCore.QTimer.singleShot(500, finish)
        QtCore.QTimer.singleShot(150, accept)
        record("opening_workspace_dialog")
        QtCore.QMetaObject.invokeMethod(button("Open existing workspace..."), "click", QtCore.Qt.QueuedConnection)
        general.idle_wait(1.5)
        record("workspace_opened")
        general.idle_wait(0.5)

    try:
        def capture_errors():
            for value in widgets():
                if isinstance(value, QtWidgets.QMessageBox) and value.isVisible():
                    result.setdefault("dialogs", []).append(value.text())
                    record("editor_message")
                    value.accept()
        error_timer = QtCore.QTimer()
        error_timer.setInterval(500)
        error_timer.timeout.connect(capture_errors)
        error_timer.start()
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        general.idle_enable(True)
        general.idle_wait(3.0)
        record("open_isolated_workspace")
        open_workspace(workspace)
        general.open_pane("Tainted Grail Item and Recipe Editor")
        general.idle_wait(0.5)
        record("native_intake")
        beats = []
        timer = QtCore.QTimer()
        timer.setInterval(100)
        timer.timeout.connect(lambda: beats.append(time.monotonic()))
        timer.start()
        started = time.monotonic()
        control("economyReadGame").click()
        while control("economyReadGame").text() == "Cancel loading" and time.monotonic() - started < 190:
            general.idle_wait(0.15)
        timer.stop()
        finished = time.monotonic()
        assert "Game definitions loaded" in control("economyStatus").text(), control("economyStatus").text()
        item_choice = control("economyItemChoice")
        recipe_choice = control("economyRecipeChoice")
        result["native_items"] = sum(str(item_choice.itemData(index)).startswith("native.item.") for index in range(item_choice.count()))
        result["native_recipes"] = sum(str(recipe_choice.itemData(index)).startswith("native.recipe.") for index in range(recipe_choice.count()))
        assert result["native_items"] > 100 and result["native_recipes"] > 20
        gaps = [b - a for a, b in zip([started] + beats, beats + [finished])]
        result["load_seconds"] = round(finished - started, 3)
        result["max_ui_gap_seconds"] = round(max(gaps), 3)
        assert max(gaps) < 3.0, "UI stalled during economy intake"
        assert control("economyIngredients").rowCount() > 0
        assert control("economyOutputs").rowCount() > 0
        result["checks"].append("real_native_intake_exact_joins_responsive")
        if os.environ.get("FOA_SDK_ECONOMY_READY_ONLY") == "1":
            control("economyTabs").setCurrentIndex(1)
            if os.environ.get("FOA_SDK_ECONOMY_SCREENSHOT"):
                control("economyReadGame").parentWidget().grab().save(os.environ["FOA_SDK_ECONOMY_SCREENSHOT"])
            result["status"] = "PASSED"
            error_timer.stop()
            record("ready")
            return
        record("create_mod")
        general.open_pane("Tainted Grail Pack Manager")
        general.idle_wait(0.5)
        button("New mod").click()
        edits = [value for value in widgets() if isinstance(value, QtWidgets.QLineEdit)]
        next(value for value in reversed(edits) if value.placeholderText() == "My Fall of Avalon mod").setText("Economy QA")
        next(value for value in reversed(edits) if value.placeholderText() == "author or namespace").setText("sdkqa")
        button("Save mod").click()
        general.idle_wait(0.2)
        pack_root = button("Save mod").parentWidget()
        while pack_root.parentWidget() and not pack_root.findChildren(QtWidgets.QLineEdit):
            pack_root = pack_root.parentWidget()
        result["mod_save_labels"] = [value.text() for value in pack_root.findChildren(QtWidgets.QLabel)]
        record("mod_save_result")
        assert "Mod saved. You can start authoring." in result["mod_save_labels"], result["mod_save_labels"]
        general.open_pane("Tainted Grail Item and Recipe Editor")
        general.idle_wait(0.2)
        record("create_definitions")
        input_name("QA Ingredient", "economyNewItem")
        item_id = control("economyItemChoice").currentData()
        control("economyItemWeight").setValue(0.125)
        control("economySaveItem").click()
        assert "saved" in control("economyStatus").text(), control("economyStatus").text()
        input_name("QA Recipe", "economyNewRecipe")
        recipe_id = control("economyRecipeChoice").currentData()
        control("economyIngredientChoice").setCurrentIndex(control("economyIngredientChoice").findData(item_id))
        control("economyIngredientQuantity").setValue(3)
        control("economySaveIngredient").click()
        assert control("economyIngredients").rowCount() == 1, control("economyStatus").text()
        control("economyIngredients").selectRow(0)
        assert control("economyIngredientQuantity").value() == 3
        control("economyIngredientQuantity").setValue(7)
        control("economySaveIngredient").click()
        assert control("economyIngredients").rowCount() == 1
        assert control("economyIngredients").model().index(0, 2).data() == "7"
        control("economyOutputChoice").setCurrentIndex(control("economyOutputChoice").findData(item_id))
        control("economyOutputQuantity").setValue(2)
        control("economySaveOutput").click()
        assert control("economyOutputs").rowCount() == 1, control("economyStatus").text()
        control("economyOutputs").selectRow(0)
        assert control("economyOutputQuantity").value() == 2
        control("economyOutputQuantity").setValue(4)
        control("economySaveOutput").click()
        result["checks"].append("custom_create_and_link_selection_edit_update")
        record("draft_and_remove")
        control("economyItemWeight").setValue(0.375)
        control("economyRecipeSettings").setChecked(True)
        control("economyRecipeType").setText("qa-crafting")
        control("economySaveRecipe").click()
        assert control("economyItemWeight").value() == 0.375, "Unrelated save erased the item draft"
        control("economyItemChoice").setCurrentIndex(1)
        control("economyItemChoice").setCurrentIndex(control("economyItemChoice").findData(item_id))
        assert control("economyItemWeight").value() == 0.375, "Selection erased the item draft"
        control("economySaveItem").click()
        control("economyOutputs").selectRow(0)
        control("economyRemoveOutput").click()
        assert control("economyOutputs").rowCount() == 0
        control("economyNewOutput").click()
        control("economyOutputChoice").setCurrentIndex(control("economyOutputChoice").findData(item_id))
        control("economyOutputQuantity").setValue(4)
        control("economySaveOutput").click()
        result["checks"].append("draft_preservation_and_link_remove_readd")
        record("reload_from_disk")
        general.close_pane("Tainted Grail Item and Recipe Editor")
        general.idle_wait(0.5)
        open_workspace(workspace)
        general.open_pane("Tainted Grail Item and Recipe Editor")
        general.idle_wait(0.5)
        control("economyItemChoice").setCurrentIndex(control("economyItemChoice").findData(item_id))
        control("economyRecipeChoice").setCurrentIndex(control("economyRecipeChoice").findData(recipe_id))
        assert control("economyItemWeight").value() == 0.375
        assert control("economyRecipeType").text() == "qa-crafting"
        assert control("economyIngredients").model().index(0, 2).data() == "7"
        assert control("economyOutputs").model().index(0, 2).data() == "4"
        result["checks"].append("workspace_catalog_reopen_preserves_authored_values")
        record("cancel")
        control("economyReadGame").click()
        control("economyReadGame").click()
        for _ in range(50):
            if control("economyReadGame").text() != "Cancel loading":
                break
            general.idle_wait(0.1)
        assert "cancelled" in control("economyStatus").text().lower(), control("economyStatus").text()
        assert control("economyRecipeChoice").currentData() == recipe_id
        result["checks"].append("cancel_preserves_catalog_and_selection")
        control("economyTabs").setCurrentIndex(1)
        if os.environ.get("FOA_SDK_ECONOMY_SCREENSHOT"):
            control("economyReadGame").parentWidget().grab().save(os.environ["FOA_SDK_ECONOMY_SCREENSHOT"])
        assert not result.get("dialogs"), result.get("dialogs")
        result["status"] = "PASSED"
    except Exception as error:
        result["error"] = str(error)
        result["traceback"] = traceback.format_exc()
    error_timer.stop()
    record("complete")
    if os.environ.get("FOA_SDK_ECONOMY_KEEP_OPEN") != "1":
        general.exit_no_prompt()


run()
