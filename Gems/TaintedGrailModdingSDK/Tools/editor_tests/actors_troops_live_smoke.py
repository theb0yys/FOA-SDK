# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Exercise compiled population authoring in a private isolated workspace."""
import hashlib
import json
import os
from pathlib import Path
import time
import traceback

import azlmbr.legacy.general as general
from PySide6 import QtCore, QtGui, QtTest, QtWidgets
from shiboken6 import isValid


def run():
    output = Path(os.environ["FOA_SDK_POPULATION_RESULT"])
    workspace = Path(os.environ["FOA_SDK_POPULATION_WORKSPACE"])
    result = {"status": "FAILED", "checks": []}
    app = QtWidgets.QApplication.instance()
    keep = []
    kept = set()
    timers = []

    def retain(value):
        if id(value) not in kept:
            kept.add(id(value))
            keep.append(value)
        return value

    def stage(name):
        result["stage"] = name
        output.write_text(json.dumps(result, indent=2), encoding="utf-8")

    def control(name):
        for window in [retain(value) for value in app.topLevelWidgets()]:
            if not isValid(window):
                continue
            retain(window)
            found = window if window.objectName() == name else window.findChild(QtWidgets.QWidget, name)
            if found is not None:
                return retain(found)
        raise AssertionError("Missing control: " + name)

    def button(text):
        for window in [retain(value) for value in app.topLevelWidgets()]:
            retain(window)
            for found in window.findChildren(QtWidgets.QPushButton):
                retain(found)
                if found.text() == text:
                    return found
        raise AssertionError("Missing button: " + text)

    def wait_for(predicate, timeout=30.0):
        start = time.monotonic()
        while not predicate():
            assert time.monotonic() - start < timeout, "Timed out at " + result["stage"]
            general.idle_wait(0.1)

    def choose(name, record_id):
        combo = control(name)
        index = combo.findData(record_id)
        assert index >= 0, "Missing exact choice " + str(record_id)
        combo.setCurrentIndex(index)

    def type_text(name, text):
        edit = control(name)
        edit.setFocus()
        edit.selectAll()
        QtTest.QTest.keyClicks(edit, text)

    def file_dialog(path, trigger):
        accepted = []
        timer = retain(QtCore.QTimer())
        timers.append(timer)
        def accept():
            dialog = next((d for d in [retain(value) for value in app.allWidgets()]
                if isValid(d) and isinstance(d, QtWidgets.QFileDialog) and d.isVisible()), None)
            if dialog is None:
                return
            retain(dialog)
            timer.stop()
            dialog.setDirectory(str(path.parent))
            dialog.selectFile(path.name)
            def finish():
                edit = dialog.findChild(QtWidgets.QLineEdit, "fileNameEdit")
                assert edit is not None
                edit.setText(str(path))
                assert Path(dialog.selectedFiles()[0]).resolve() == path.resolve()
                dialog.done(QtWidgets.QDialog.Accepted)
                accepted.append(True)
            QtCore.QTimer.singleShot(500, finish)
        timer.timeout.connect(accept)
        timer.start(100)
        QtCore.QMetaObject.invokeMethod(trigger, "click", QtCore.Qt.QueuedConnection)
        wait_for(lambda: accepted)
        general.idle_wait(0.5)

    def create(name, troop=False):
        expected = [None, name] if troop else [name]
        completed = []
        timer = retain(QtCore.QTimer())
        timers.append(timer)
        def accept():
            current = [retain(value) for value in app.allWidgets()]
            dialog = next((d for d in current if isValid(d) and isinstance(d, QtWidgets.QInputDialog) and d.isVisible()), None)
            if dialog is None or len(completed) == len(expected):
                return
            retain(dialog)
            value = expected[len(completed)]
            if value is not None:
                dialog.setTextValue(value)
            result["dialog"] = {"title": dialog.windowTitle(), "value": dialog.textValue(), "expected": value}
            stage("accepting_creation_dialog")
            completed.append(True)
            dialog.done(QtWidgets.QDialog.Accepted)
        timer.timeout.connect(accept)
        timer.start(100)
        QtCore.QMetaObject.invokeMethod(control("populationNewTroop" if troop else "populationNewActor"), "click", QtCore.Qt.QueuedConnection)
        wait_for(lambda: len(completed) == len(expected))
        general.idle_wait(0.2)
        timer.stop()
        assert "Created " in control("populationStatus").text(), control("populationStatus").text()
        return control("populationTroop" if troop else "populationActor").currentData()

    def save(troop=False):
        control("populationSaveTroop" if troop else "populationSaveActor").click()
        assert "saved" in control("populationStatus").text().lower(), control("populationStatus").text()
        assert "Error:" not in control("populationStatus").text(), control("populationStatus").text()

    def catalog_bytes():
        return (workspace.parent / "Catalog/catalog.tgcatalog.json").read_bytes()

    try:
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        general.idle_enable(True)
        general.idle_wait(3.0)
        stage("open_isolated_workspace")
        general.open_pane("Tainted Grail SDK Status")
        general.idle_wait(0.5)
        file_dialog(workspace, button("Open existing workspace..."))
        general.open_pane("Tainted Grail Actor and Troop Editor")
        general.idle_wait(0.3)
        stage("native_actor_intake")
        beats = []
        beat = retain(QtCore.QTimer())
        timers.append(beat)
        beat.timeout.connect(lambda: beats.append(time.monotonic()))
        beat.start(100)
        started = time.monotonic()
        control("populationReadGame").click()
        wait_for(lambda: control("populationReadGame").text() != "Cancel loading", 190)
        finished = time.monotonic()
        beat.stop()
        assert "Supported game actors loaded" in control("populationStatus").text(), control("populationStatus").text()
        choice = control("populationActor")
        native = [choice.itemData(i) for i in range(choice.count()) if str(choice.itemData(i)).startswith("native.actor.")]
        assert len(native) > 100
        result["native_actor_count"] = len(native)
        result["load_seconds"] = round(finished - started, 3)
        result["max_load_ui_gap_seconds"] = round(max(b-a for a, b in zip([started] + beats, beats + [finished])), 3)
        assert result["max_load_ui_gap_seconds"] < 3.0
        assert "no portrait or model binding" in control("populationPortraitState").text()
        result["checks"].append("native_intake_exact_identities_responsive_and_missing_visual_state")
        # Collapsed review tables must use current data when explicitly opened.
        assert control("populationActorLanes").rowCount() == 0
        control("populationActorLaneGroup").setChecked(True)
        assert control("populationActorLanes").rowCount() == 7
        control("populationActorLaneGroup").setChecked(False)
        general.open_pane("Tainted Grail SDK Status")
        control("foundationAdvancedToggle").click()
        assert control("foundationBlockers").rowCount() > 0
        control("foundationAdvancedToggle").click()
        general.open_pane("Tainted Grail Actor and Troop Editor")
        result["checks"].append("deferred_review_tables_populate_latest_snapshot_when_expanded")
        if os.environ.get("FOA_SDK_POPULATION_READY_ONLY") == "1":
            control("TaintedGrailActorTroopEditor").grab().save(str(output.with_suffix(".png")))
            result["status"] = "PASSED"
            stage("ready")
            return

        stage("create_mod")
        general.open_pane("Tainted Grail Pack Manager")
        general.idle_wait(0.3)
        button("New mod").click()
        for window in [retain(value) for value in app.topLevelWidgets()]:
            for edit in window.findChildren(QtWidgets.QLineEdit):
                retain(edit)
                if edit.placeholderText() == "My Fall of Avalon mod":
                    edit.setText("Population QA")
                if edit.placeholderText() == "author or namespace":
                    edit.setText("sdkqa")
        button("Save mod").click()
        general.idle_wait(0.2)
        general.open_pane("Tainted Grail Actor and Troop Editor")
        stage("create_actor_and_portrait")
        captain = create("QA Captain")
        type_text("populationArchetype", "veteran")
        control("populationMinimumLevel").setValue(5)
        control("populationMaximumLevel").setValue(8)
        save()
        # Test-owned pixels are generated only in the isolated workspace.
        portrait = workspace.parent / "Assets/Portraits/qa.png"
        portrait.parent.mkdir(parents=True, exist_ok=True)
        image = QtGui.QImage(64, 48, QtGui.QImage.Format_ARGB32)
        image.fill(QtGui.QColor(35, 95, 150))
        assert image.save(str(portrait))
        file_dialog(portrait, control("populationChoosePortrait"))
        choose("populationActor", native[0])
        assert control("populationActor").currentData() == captain, "Portrait selection must mark the draft dirty"
        save()
        assert control("populationPortrait").pixmap().toImage().pixelColor(10, 10) == image.pixelColor(10, 10)
        assert control("populationPortraitRef").text() == "$workspace/Assets/Portraits/qa.png"
        choose("populationActor", native[0])
        assert control("populationPortrait").pixmap().isNull(), "Previous actor portrait leaked to missing visual"
        choose("populationActor", captain)
        assert not control("populationPortrait").pixmap().isNull()
        result["checks"].append("custom_actor_fields_and_exact_portrait_pixels_saved_selection_clears_stale_image")

        stage("create_and_edit_troop")
        guard = create("QA Guard")
        choose("populationActor", captain)
        troop = create("QA Patrol", troop=True)
        assert control("populationMembers").rowCount() == 1
        assert control("populationLeader").currentData() == captain
        control("populationTroopLaneGroup").setChecked(True)
        assert control("populationTroopLanes").rowCount() == 7
        control("populationTroopLaneGroup").setChecked(False)
        control("populationNewMember").click()
        choose("populationMemberActor", guard)
        control("populationMemberRole").setCurrentText("melee")
        control("populationMemberMaximum").setValue(2)
        control("populationMinimumSize").setValue(2)
        control("populationMaximumSize").setValue(3)
        control("populationStageMember").click()
        save(troop=True)
        assert control("populationMembers").rowCount() == 2
        table = control("populationMembers")
        row = next(i for i in range(table.rowCount()) if table.item(i, 1).text() == guard)
        table.setCurrentCell(row, 0)
        table.selectRow(row)
        control("populationMemberMaximum").setValue(4)
        control("populationMaximumSize").setValue(5)
        save(troop=True)
        result["checks"].append("new_troop_initial_leader_and_automatic_member_identity_edit")

        stage("remove_and_invalid_composition")
        table = control("populationMembers")
        row = next(i for i in range(table.rowCount()) if table.item(i, 1).text() == guard)
        table.setCurrentCell(row, 0)
        table.selectRow(row)
        control("populationRemoveMember").click()
        assert control("populationMembers").rowCount() == 1
        before = catalog_bytes()
        control("populationSaveTroop").click()  # min size 2 cannot be met by the sole leader
        assert "Error:" in control("populationStatus").text()
        assert catalog_bytes() == before
        control("populationMinimumSize").setValue(1)
        control("populationMaximumSize").setValue(1)
        save(troop=True)
        assert control("populationMembers").rowCount() == 1
        result["checks"].append("explicit_member_removal_invalid_composition_preserves_catalog_then_valid_save")

        stage("dirty_drafts_and_cancellation")
        control("populationTabs").setCurrentIndex(0)
        choose("populationActor", captain)
        type_text("populationArchetype", "unsaved-veteran")
        choose("populationActor", guard)
        assert control("populationActor").currentData() == captain
        before = catalog_bytes()
        control("populationReadGame").click()
        assert control("populationReadGame").text() == "Load game actors"
        assert catalog_bytes() == before
        control("populationRevertActor").click()
        assert control("populationArchetype").text() == "veteran"
        control("populationReadGame").click()
        control("populationReadGame").click()
        wait_for(lambda: control("populationReadGame").text() == "Load game actors")
        assert "cancelled" in control("populationStatus").text().lower()
        assert catalog_bytes() == before
        result["checks"].append("dirty_selection_load_guard_revert_and_reader_cancellation")

        stage("reopen_saved_workspace")
        general.close_pane("Tainted Grail Actor and Troop Editor")
        general.open_pane("Tainted Grail SDK Status")
        file_dialog(workspace, button("Open existing workspace..."))
        general.open_pane("Tainted Grail Actor and Troop Editor")
        general.idle_wait(0.3)
        choose("populationActor", captain)
        assert control("populationArchetype").text() == "veteran"
        assert control("populationMinimumLevel").value() == 5
        assert control("populationMaximumLevel").value() == 8
        assert not control("populationPortrait").pixmap().isNull()
        choose("populationTroop", troop)
        assert control("populationMembers").rowCount() == 1
        assert control("populationMembers").item(0, 1).text() == captain
        result["checks"].append("workspace_reload_preserves_actor_fields_portrait_and_removed_members")
        for index in (0, 1):
            control("populationTabs").setCurrentIndex(index)
            general.idle_wait(0.2)
            scroll = control("populationTabs").widget(index)
            assert scroll.horizontalScrollBar().maximum() == 0, "Actor/troop page requires horizontal scrolling"
            control("TaintedGrailActorTroopEditor").grab().save(str(output.with_name(output.stem + ("-actor.png" if index == 0 else "-troop.png"))))
        control("populationTabs").setCurrentIndex(0)
        result["checks"].append("actor_and_troop_pages_fit_available_width")
        result["catalog_sha256"] = hashlib.sha256(catalog_bytes()).hexdigest()
        result["record_ids"] = {"captain": captain, "guard": guard, "troop": troop}
        control("TaintedGrailActorTroopEditor").grab().save(str(output.with_suffix(".png")))
        result["status"] = "PASSED"
        stage("complete")
    except Exception:
        result["error"] = traceback.format_exc()
        stage(result.get("stage", "failed"))
    finally:
        for timer in timers:
            timer.stop()
        output.write_text(json.dumps(result, indent=2), encoding="utf-8")


run()
