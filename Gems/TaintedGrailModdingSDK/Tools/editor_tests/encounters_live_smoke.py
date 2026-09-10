# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Exercise encounter authoring through the running Editor in a private workspace."""
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
    output = Path(os.environ["FOA_SDK_ENCOUNTER_RESULT"])
    workspace = Path(os.environ["FOA_SDK_ENCOUNTER_WORKSPACE"])
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

    def new_encounter(name, target_name):
        values = [name, target_name]
        completed = []
        timer = retain(QtCore.QTimer())
        timers.append(timer)
        def accept():
            dialog = next((d for d in [retain(v) for v in app.allWidgets()]
                if isValid(d) and isinstance(d, QtWidgets.QInputDialog) and d.isVisible()), None)
            if dialog is None or len(completed) == 2:
                return
            if not completed:
                dialog.setTextValue(values[0])
            else:
                combo = dialog.findChild(QtWidgets.QComboBox)
                index = next(i for i in range(combo.count()) if values[1] in combo.itemText(i))
                combo.setCurrentIndex(index)
            completed.append(True)
            dialog.done(QtWidgets.QDialog.Accepted)
        timer.timeout.connect(accept)
        timer.start(100)
        QtCore.QMetaObject.invokeMethod(control("encounterNew"), "click", QtCore.Qt.QueuedConnection)
        wait_for(lambda: len(completed) == 2)
        general.idle_wait(0.2)
        timer.stop()
        assert "created and saved" in control("encounterStatus").text(), control("encounterStatus").text()
        return control("encounterRecords").currentData()

    def save_encounter():
        control("encounterSave").click()
        assert control("encounterStatus").text() == "Encounter saved.", control("encounterStatus").text()

    def row_named(name):
        table = control("encounterEntries")
        return next(i for i in range(table.rowCount()) if table.item(i, 0).text() == name)

    def quantities(name, minimum, maximum):
        table = control("encounterEntries")
        row = row_named(name)
        table.cellWidget(row, 2).setValue(minimum)
        table.cellWidget(row, 3).setValue(maximum)

    def measured(name, action):
        beats = []
        timer = retain(QtCore.QTimer()); timers.append(timer)
        timer.timeout.connect(lambda: beats.append(time.monotonic()))
        timer.start(100)
        start = time.monotonic()
        action()
        general.idle_wait(0.2)
        end = time.monotonic(); timer.stop()
        gap = max(b-a for a,b in zip([start] + beats, beats + [end]))
        result.setdefault("timings", {})[name] = {"seconds": round(end-start, 3), "max_ui_gap_seconds": round(gap, 3)}
        assert gap < 3.0, "UI paused too long: " + name + " " + str(gap)

    try:
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        general.idle_enable(True)
        general.idle_wait(3.0)
        stage("open_isolated_workspace")
        before = catalog_bytes()
        before_data = json.loads(before)
        general.open_pane("Tainted Grail SDK Status")
        general.idle_wait(0.3)
        file_dialog(workspace, button("Open existing workspace..."))
        general.open_pane("Tainted Grail Actor and Troop Editor")
        general.idle_wait(0.2)
        assert control("populationActor").count() >= 885
        result["initial_actor_count"] = len(before_data["ActorProfiles"])
        result["initial_item_count"] = len(before_data["EconomyItems"])
        result["initial_recipe_count"] = len(before_data["EconomyRecipes"])
        stage("create_test_mod_and_actors")
        general.open_pane("Tainted Grail Pack Manager")
        button("New mod").click()
        for window in [retain(v) for v in app.topLevelWidgets()]:
            for edit in window.findChildren(QtWidgets.QLineEdit):
                retain(edit)
                if edit.placeholderText() == "My Fall of Avalon mod":
                    edit.setText("Encounter QA")
                if edit.placeholderText() == "author or namespace":
                    edit.setText("sdkqa")
        button("Save mod").click()
        general.open_pane("Tainted Grail Actor and Troop Editor")
        captain = create("Encounter QA Captain")
        guard = create("Encounter QA Guard")
        migrated = json.loads(catalog_bytes())
        assert migrated["SchemaVersion"] == 4
        backups = list((workspace.parent / "Catalog").glob("*.schema-2.*.backup.json"))
        assert any(p.read_bytes() == before for p in backups)
        assert migrated["EconomyItems"] == before_data["EconomyItems"]
        assert migrated["EconomyRecipes"] == before_data["EconomyRecipes"]
        assert migrated["RecipeIngredients"] == before_data["RecipeIngredients"]
        assert migrated["RecipeOutputs"] == before_data["RecipeOutputs"]
        prior_actors = {a["RecordId"]: a for a in before_data["ActorProfiles"]}
        assert all(prior_actors[a["RecordId"]] == a for a in migrated["ActorProfiles"] if a["RecordId"] in prior_actors)
        result["checks"].append("schema_2_backup_exact_and_existing_economy_population_preserved")

        stage("create_patrol_encounter")
        general.open_pane("Tainted Grail Spawn and Encounter Editor")
        general.idle_wait(0.2)
        encounter = new_encounter("North gate patrol", "Encounter QA Captain")
        choose("encounterTarget", guard)
        control("encounterAddEntry").click()
        quantities("Encounter QA Guard", 3, 3)
        type_text("encounterPlacementSubject", "North gate courtyard")
        choose("encounterActivation", "all_conditions")
        control("encounterConditions").setPlainText("Player enters the north gate")
        type_text("encounterCleanup", "Retire patrol when the area resets")
        type_text("encounterRollback", "Restore the previous patrol plan")
        control("encounterInstances").setValue(2)
        control("encounterPopulationLimit").setValue(8)
        assert "4-4 actors per instance; up to 8" in control("encounterPreview").text()
        measured("save_patrol", save_encounter)
        definition = next(d for d in json.loads(catalog_bytes())["EncounterDefinitions"] if d["RecordId"] == encounter)
        entry_ids = {e["TargetRecordId"]: e["EntryId"] for e in definition["Entries"]}
        assert definition["PlacementSubjectRef"] == "North gate courtyard"
        assert definition["Conditions"] == ["Player enters the north gate"]
        result["checks"].append("captain_and_three_guards_preview_placement_conditions_limits_saved")
        pane = control("spawnEncounterEditor")
        pane.resize(1200, 960)
        pane.grab().save(str(output.with_name("encounter-patrol.png")))

        stage("invalid_limit_and_unmatched_selection")
        before_invalid = catalog_bytes()
        control("encounterPopulationLimit").setValue(7)
        control("encounterSave").click()
        assert control("encounterStatus").property("error")
        assert catalog_bytes() == before_invalid
        control("encounterPopulationLimit").setValue(8)
        target = control("encounterTarget")
        target.setEditText("This actor does not exist")
        rows = control("encounterEntries").rowCount()
        control("encounterAddEntry").click()
        assert control("encounterEntries").rowCount() == rows
        assert "matching saved actor" in control("encounterStatus").text()
        choose("encounterTarget", guard)
        control("encounterAddEntry").click()
        assert control("encounterEntries").rowCount() == rows
        control("encounterRevert").click()
        result["checks"].append("invalid_population_and_unknown_or_duplicate_actor_preserve_saved_state")

        stage("change_and_remove_composition")
        measured("quantity_edit", lambda: quantities("Encounter QA Guard", 2, 2))
        assert "3-3 actors per instance; up to 6" in control("encounterPreview").text()
        measured("save_edited_patrol", save_encounter)
        edited = next(d for d in json.loads(catalog_bytes())["EncounterDefinitions"] if d["RecordId"] == encounter)
        assert {e["TargetRecordId"]: e["EntryId"] for e in edited["Entries"]} == entry_ids
        table = control("encounterEntries")
        table.setCurrentCell(row_named("Encounter QA Guard"), 0)
        control("encounterRemoveEntry").click()
        assert "1-1 actors per instance; up to 2" in control("encounterPreview").text()
        save_encounter()
        assert control("encounterEntries").rowCount() == 1
        table.setCurrentCell(0, 0); control("encounterRemoveEntry").click()
        before_empty = catalog_bytes()
        control("encounterSave").click()
        assert control("encounterStatus").property("error")
        assert catalog_bytes() == before_empty
        control("encounterRevert").click()
        result["checks"].append("quantity_changes_keep_entry_ids_removal_persists_and_empty_save_is_rejected")

        stage("dirty_draft_and_failed_save")
        type_text("encounterName", "Unsaved patrol")
        choose("encounterRecords", "")
        assert control("encounterRecords").currentData() == encounter
        control("encounterRevert").click()
        assert control("encounterName").text() == "North gate patrol"
        type_text("encounterName", "Failed write patrol")
        catalog = workspace.parent / "Catalog/catalog.tgcatalog.json"
        retained = catalog.with_suffix(".qa-retained")
        assert not retained.exists()
        original = catalog.read_bytes()
        catalog.rename(retained); catalog.mkdir()
        try:
            control("encounterSave").click()
            assert control("encounterStatus").property("error")
            assert control("encounterName").text() == "Failed write patrol"
            assert retained.read_bytes() == original
        finally:
            catalog.rmdir(); retained.rename(catalog)
        control("encounterRevert").click()
        assert control("encounterName").text() == "North gate patrol"
        result["checks"].append("dirty_selection_revert_and_failed_disk_write_preserve_draft_and_catalog")

        stage("reopen_workspace")
        general.close_pane("Tainted Grail Spawn and Encounter Editor")
        general.close_pane("Tainted Grail Actor and Troop Editor")
        general.open_pane("Tainted Grail SDK Status")
        file_dialog(workspace, button("Open existing workspace..."))
        general.open_pane("Tainted Grail Spawn and Encounter Editor")
        general.idle_wait(0.3)
        choose("encounterRecords", encounter)
        assert control("encounterEntries").rowCount() == 1
        assert control("encounterName").text() == "North gate patrol"
        assert control("encounterConditions").toPlainText() == "Player enters the north gate"
        assert control("encounterPlacementSubject").text() == "North gate courtyard"
        choose("encounterTarget", guard); control("encounterAddEntry").click()
        quantities("Encounter QA Guard", 3, 3)
        save_encounter()
        assert "4-4 actors per instance; up to 8" in control("encounterPreview").text()
        result["checks"].append("reopen_restores_complete_plan_and_supports_further_edits")
        pane = control("spawnEncounterEditor")
        pane.grab().save(str(output.with_name("encounter-reopened.png")))
        result["catalog_sha256"] = hashlib.sha256(catalog_bytes()).hexdigest()
        result["record_ids"] = {"captain": captain, "guard": guard, "encounter": encounter}
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
