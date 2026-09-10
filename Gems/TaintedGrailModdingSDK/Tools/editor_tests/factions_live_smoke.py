# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Exercise faction and culture authoring through the running Editor in a private workspace."""
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
    output = Path(os.environ["FOA_SDK_FACTION_RESULT"])
    workspace = Path(os.environ["FOA_SDK_FACTION_WORKSPACE"])
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


    def new_definition(name, culture=False):
        completed = []
        timer = retain(QtCore.QTimer()); timers.append(timer)
        def accept():
            dialog = next((d for d in [retain(v) for v in app.allWidgets()]
                if isValid(d) and isinstance(d, QtWidgets.QInputDialog) and d.isVisible()), None)
            if dialog is None:
                return
            timer.stop(); dialog.setTextValue(name); dialog.done(QtWidgets.QDialog.Accepted); completed.append(True)
        timer.timeout.connect(accept); timer.start(100)
        QtCore.QMetaObject.invokeMethod(control("factionNewCulture" if culture else "factionNew"), "click", QtCore.Qt.QueuedConnection)
        wait_for(lambda: completed); general.idle_wait(0.2)
        assert "created and saved" in control("factionStatus").text(), control("factionStatus").text()
        if culture:
            document = json.loads(catalog_bytes())
            return next(r["RecordId"] for r in document["Records"] if r["Domain"] == "society"
                and r["RecordKind"] == "culture" and r["DisplayName"] == name)
        return control("factionRecords").currentData()

    def edit_culture():
        completed = []
        timer = retain(QtCore.QTimer()); timers.append(timer)
        def edit():
            widgets = [retain(v) for v in app.allWidgets()]
            picker = next((w for w in widgets if isValid(w) and isinstance(w, QtWidgets.QInputDialog) and w.isVisible()), None)
            if picker is not None:
                picker.done(QtWidgets.QDialog.Accepted)
                return
            dialog = next((w for w in widgets if isValid(w) and w.objectName() == "cultureEditorDialog" and w.isVisible()), None)
            if dialog is None:
                return
            timer.stop()
            control("cultureName").setText("")
            control("cultureSave").click()
            assert "name" in control("cultureStatus").text().lower()
            control("cultureName").setText("Northlanders")
            control("cultureDescription").setText("Northern settlements")
            control("cultureLanguage").setText("Common speech")
            control("cultureSave").click()
            completed.append(True)
        timer.timeout.connect(edit); timer.start(100)
        QtCore.QMetaObject.invokeMethod(control("factionEditCulture"), "click", QtCore.Qt.QueuedConnection)
        wait_for(lambda: completed); general.idle_wait(0.2)
        assert control("factionStatus").text() == "Culture saved."

    def save_faction():
        control("factionSave").click()
        assert control("factionStatus").text() == "Faction saved.", control("factionStatus").text()

    def add_link(prefix, target, value, reference="", notes=""):
        choose(prefix + "Target", target); choose(prefix + "Value", value)
        control(prefix + "Reference").setText(reference); control(prefix + "Notes").setText(notes)
        control(prefix + "Add").click()
        assert not control("factionStatus").property("error"), control("factionStatus").text()

    def table_row(prefix, target_name):
        table = control(prefix + "Table")
        return next(i for i in range(table.rowCount()) if target_name in table.item(i, 0).text())

    def select_row(prefix, target_name):
        table = control(prefix + "Table"); table.selectRow(table_row(prefix, target_name)); general.idle_wait(0.1)

    try:
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        general.idle_enable(True); general.idle_wait(3.0)
        stage("open_isolated_workspace")
        before = catalog_bytes(); before_data = json.loads(before)
        assert before_data["SchemaVersion"] == 3, "Use the private schema-3 encounter acceptance workspace as input."
        result["initial_actor_count"] = len(before_data["ActorProfiles"])
        result["initial_item_count"] = len(before_data["EconomyItems"])
        result["initial_recipe_count"] = len(before_data["EconomyRecipes"])
        result["initial_encounter_count"] = len(before_data["EncounterDefinitions"])
        general.open_pane("Tainted Grail SDK Status"); general.idle_wait(0.3)
        file_dialog(workspace, button("Open existing workspace..."))
        stage("create_test_mod_and_population")
        general.open_pane("Tainted Grail Pack Manager"); button("New mod").click()
        for window in [retain(v) for v in app.topLevelWidgets()]:
            for edit in window.findChildren(QtWidgets.QLineEdit):
                retain(edit)
                if edit.placeholderText() == "My Fall of Avalon mod":
                    edit.setText("Faction QA")
                if edit.placeholderText() == "author or namespace":
                    edit.setText("sdkqa")
        button("Save mod").click()
        general.open_pane("Tainted Grail Actor and Troop Editor"); general.idle_wait(0.2)
        captain = create("Faction QA Captain"); guard = create("Faction QA Guard")
        troop = create("Faction QA Patrol", troop=True)
        general.close_pane("Tainted Grail Actor and Troop Editor")
        migrated = json.loads(catalog_bytes())
        assert migrated["SchemaVersion"] == 5
        backups = list((workspace.parent / "Catalog").glob("*.schema-3.*.backup.json"))
        assert any(p.read_bytes() == before for p in backups)
        for name in ["EconomyItems", "EconomyRecipes", "RecipeIngredients", "RecipeOutputs", "EncounterDefinitions"]:
            assert migrated[name] == before_data[name], "Migration changed " + name
        prior_actors = {a["RecordId"]: a for a in before_data["ActorProfiles"]}
        assert all(prior_actors[a["RecordId"]] == a for a in migrated["ActorProfiles"] if a["RecordId"] in prior_actors)
        result["checks"].append("schema_3_backup_exact_and_existing_encounters_economy_population_preserved")

        stage("cultures_and_factions")
        general.open_pane("Tainted Grail Faction and Authority Editor"); general.idle_wait(0.3)
        culture = new_definition("Northlanders", culture=True); edit_culture()
        town = new_definition("Town Guard"); bandits = new_definition("Bandits")
        choose("factionRecords", town); choose("factionCulture", culture)
        type_text("factionDescription", "Keep the town safe")
        type_text("factionAuthority", "Captain commands the guard")
        add_link("factionMember", captain, "leader")
        add_link("factionMember", guard, "member")
        add_link("factionMember", troop, "officer")
        control("factionTabs").setCurrentIndex(1)
        add_link("factionDisposition", bandits, "hostile", notes="Protect travelers")
        control("factionTabs").setCurrentIndex(2)
        add_link("factionJurisdiction", "", "controls", "North gate courtyard", "Guard the entrance")
        assert "3 members | 1 directed relationships | 1 jurisdiction plans" in control("factionPreview").text()
        assert "Unverified territory reference: North gate courtyard" in control("factionPreview").text()
        measured("save_town_guard", save_faction)
        document = json.loads(catalog_bytes())
        links = [r for r in document["FactionLinks"] if r["FactionRecordId"] == town]
        assert len(links) == 5
        assert not [r for r in document["FactionLinks"] if r["FactionRecordId"] == bandits]
        culture_row = next(r for r in document["CultureProfiles"] if r["RecordId"] == culture)
        assert culture_row["Description"] == "Northern settlements" and culture_row["Language"] == "Common speech"
        link_ids = {r["TargetRecordId"]: r["LinkId"] for r in links if r["TargetRecordId"]}
        result["checks"].append("culture_edit_membership_leadership_directed_hostility_and_jurisdiction_saved")

        stage("invalid_links_and_name")
        original = catalog_bytes()
        control("factionTabs").setCurrentIndex(0)
        choose("factionMemberTarget", guard); choose("factionMemberValue", "member"); control("factionMemberAdd").click()
        assert control("factionStatus").property("error")
        assert control("factionMemberTable").rowCount() == 3
        select_row("factionMember", "Faction QA Guard"); choose("factionMemberValue", "leader")
        control("factionMemberUpdate").click(); assert control("factionStatus").property("error")
        control("factionMemberTarget").setEditText("No matching saved actor")
        control("factionMemberAdd").click(); assert control("factionStatus").property("error")
        control("factionTabs").setCurrentIndex(1)
        choose("factionDispositionTarget", town); control("factionDispositionAdd").click()
        assert control("factionStatus").property("error")
        type_text("factionName", ""); control("factionSave").click()
        assert control("factionStatus").property("error")
        assert catalog_bytes() == original
        control("factionRevert").click()
        result["checks"].append("duplicates_multiple_leaders_unknown_targets_self_relationships_and_empty_names_rejected")

        stage("update_remove_and_preserve_other_faction")
        control("factionTabs").setCurrentIndex(0)
        select_row("factionMember", "Faction QA Guard"); choose("factionMemberValue", "officer")
        measured("update_member", lambda: control("factionMemberUpdate").click())
        assert not control("factionStatus").property("error")
        control("factionTabs").setCurrentIndex(1)
        select_row("factionDisposition", "Bandits"); choose("factionDispositionValue", "neutral")
        control("factionDispositionUpdate").click()
        measured("save_relationship_edit", save_faction)
        document = json.loads(catalog_bytes())
        current = [r for r in document["FactionLinks"] if r["FactionRecordId"] == town]
        assert all(r["LinkId"] == link_ids[r["TargetRecordId"]] for r in current if r["TargetRecordId"])
        assert next(r for r in current if r["TargetRecordId"] == bandits)["Value"] == "neutral"
        control("factionTabs").setCurrentIndex(0)
        select_row("factionMember", "Faction QA Guard"); control("factionMemberRemove").click(); save_faction()
        assert control("factionMemberTable").rowCount() == 2
        choose("factionRecords", bandits)
        assert control("factionMemberTable").rowCount() == 0 and control("factionDispositionTable").rowCount() == 0
        choose("factionRecords", town)
        result["checks"].append("updates_keep_link_ids_removal_persists_and_other_faction_remains_unchanged")

        stage("dirty_draft_and_failed_write")
        type_text("factionName", "Unsaved Town Guard")
        choose("factionRecords", bandits)
        assert control("factionRecords").currentData() == town and control("factionName").text() == "Unsaved Town Guard"
        assert control("factionStatus").property("error")
        control("factionNew").click(); assert control("factionStatus").property("error")
        control("factionRevert").click(); assert control("factionName").text() == "Town Guard"
        type_text("factionName", "Failed write faction")
        original = catalog_bytes(); catalog = workspace.parent / "Catalog/catalog.tgcatalog.json"; held = catalog.with_suffix(".held")
        catalog.rename(held); catalog.mkdir()
        try:
            control("factionSave").click()
            assert control("factionStatus").property("error")
            assert control("factionName").text() == "Failed write faction"
            assert held.read_bytes() == original
        finally:
            catalog.rmdir(); held.rename(catalog)
        control("factionRevert").click()
        result["checks"].append("dirty_selection_revert_and_failed_disk_write_preserve_draft_and_catalog")

        stage("reopen_and_edit")
        general.close_pane("Tainted Grail Faction and Authority Editor")
        general.open_pane("Tainted Grail SDK Status"); file_dialog(workspace, button("Open existing workspace..."))
        general.open_pane("Tainted Grail Pack Manager")
        pack_pane = button("Open selected").parentWidget().parentWidget().parentWidget()
        choices = pack_pane.findChildren(QtWidgets.QComboBox)
        selected = next((combo, index) for combo in choices for index in range(combo.count())
            if str(combo.itemData(index)).replace("\\", "/").endswith("/sdkqa.faction-qa/pack.tgpack.json"))
        selected[0].setCurrentIndex(selected[1]); button("Open selected").click()
        general.open_pane("Tainted Grail Faction and Authority Editor"); general.idle_wait(0.3)
        choose("factionRecords", town)
        assert control("factionMemberTable").rowCount() == 2
        assert control("factionCulture").currentData() == culture
        assert control("factionDescription").text() == "Keep the town safe"
        assert control("factionAuthority").text() == "Captain commands the guard"
        control("factionTabs").setCurrentIndex(1)
        select_row("factionDisposition", "Bandits"); choose("factionDispositionValue", "hostile")
        control("factionDispositionUpdate").click(); save_faction()
        result["checks"].append("workspace_reopen_restores_culture_links_and_authority_and_allows_further_edits")

        stage("visible_pane_and_small_window")
        pane = control("factionAuthorityEditor")
        dock = pane.parentWidget()
        while dock is not None and not isinstance(dock, QtWidgets.QDockWidget):
            dock = dock.parentWidget()
        assert dock is not None, "The faction pane must have its registered dock host."
        retain(dock); dock.setFloating(True); dock.show()
        available = dock.screen().availableGeometry()
        dock.resize(min(1200, available.width() - 40), min(900, available.height() - 60))
        general.idle_wait(0.5)
        scroll = control("factionScroll")
        for index, suffix, prefix in [(0, "members", "factionMember"),
                (1, "relationships", "factionDisposition"), (2, "jurisdiction", "factionJurisdiction")]:
            control("factionTabs").setCurrentIndex(index); general.idle_wait(0.2)
            table = control(prefix + "Table")
            scroll.ensureWidgetVisible(table); general.idle_wait(0.2)
            header_width = table.fontMetrics().horizontalAdvance(table.horizontalHeaderItem(1).text())
            assert table.columnWidth(1) >= header_width
            assert dock.grab().save(str(output.with_name("faction-" + suffix + ".png")))
        dock.resize(min(860, available.width() - 40), min(640, available.height() - 60))
        general.idle_wait(0.5)
        result["small_window_size"] = {"width": dock.width(), "height": dock.height()}
        assert dock.width() <= 860 and dock.height() <= 640
        scroll.ensureWidgetVisible(control("factionJurisdictionTable")); general.idle_wait(0.2)
        assert control("factionSave").isVisible()
        assert pane.rect().contains(control("factionSave").mapTo(pane, QtCore.QPoint(0, 0)))
        assert control("factionJurisdictionTable").isVisible()
        assert scroll.verticalScrollBar().maximum() > 0
        control("factionName").setFocus(); QtTest.QTest.keyClick(control("factionName"), QtCore.Qt.Key_Tab)
        assert app.focusWidget() is not None
        assert dock.grab().save(str(output.with_name("faction-small-window.png")))
        result["checks"].append("all_tabs_render_and_small_window_scroll_keyboard_and_save_controls_remain_available")
        dock.resize(min(1200, available.width() - 40), min(900, available.height() - 60))
        control("factionTabs").setCurrentIndex(0); scroll.ensureWidgetVisible(control("factionMemberTable"))
        result["catalog_sha256"] = hashlib.sha256(catalog_bytes()).hexdigest()
        result["record_ids"] = {"captain": captain, "guard": guard, "troop": troop, "culture": culture, "town": town, "bandits": bandits}
        result["status"] = "PASSED"; stage("complete")
    except Exception:
        result["error"] = traceback.format_exc()
        stage(result.get("stage", "failed"))
    finally:
        for timer in timers:
            timer.stop()
        output.write_text(json.dumps(result, indent=2), encoding="utf-8")


run()
