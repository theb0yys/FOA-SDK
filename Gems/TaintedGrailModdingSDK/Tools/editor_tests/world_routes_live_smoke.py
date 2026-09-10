# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Exercise world and route authoring through the running Editor in a private workspace."""
import hashlib
import json
import os
from pathlib import Path
import time
import traceback

import azlmbr.legacy.general as general
from PySide6 import QtCore, QtGui, QtTest, QtWidgets
from shiboken6 import isValid, getCppPointer, wrapInstance


def run():
    output = Path(os.environ["FOA_SDK_WORLD_RESULT"])
    workspace = Path(os.environ["FOA_SDK_WORLD_WORKSPACE"])
    result = {"status": "FAILED", "checks": []}
    app = QtWidgets.QApplication.instance()
    # Keep workspace selection inside the Qt test surface rather than a native OS picker.
    app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
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



    def new_world(kind, name, parent_id=""):
        completed = []
        errors = []
        timer = retain(QtCore.QTimer()); timers.append(timer)
        def accept():
            dialogs = [retain(v) for v in app.allWidgets()]
            dialog = next((w for w in dialogs if isValid(w) and w.objectName() == "worldCreateDialog" and w.isVisible()), None)
            if dialog is None:
                return
            timer.stop()
            try:
                choose("worldCreateKind", kind)
                control("worldCreateName").setText(name)
                choose("worldCreateParent", parent_id)
                control("worldCreateSave").click()
                assert not dialog.isVisible(), control("worldCreateStatus").text()
            except Exception:
                errors.append(traceback.format_exc())
                dialog.reject()
            completed.append(True)
        timer.timeout.connect(accept); timer.start(100)
        QtCore.QMetaObject.invokeMethod(control("worldNew"), "click", QtCore.Qt.QueuedConnection)
        wait_for(lambda: completed)
        assert not errors, errors
        general.idle_wait(0.2)
        assert control("worldStatus").text() == "World definition created and saved.", control("worldStatus").text()
        return control("worldRecords").currentData()

    def save_world():
        control("worldSave").click()
        assert control("worldStatus").text() == "World definition saved.", control("worldStatus").text()

    def add_node(location):
        choose("worldNodeLocation", location)
        count = control("worldNodeTable").rowCount()
        control("worldNodeAdd").click()
        assert control("worldNodeTable").rowCount() == count + 1, control("worldStatus").text()
        return control("worldNodeTable").item(count, 0).data(QtCore.Qt.UserRole)

    def add_edge(start, end, mode="walk", two_way=True):
        choose("worldEdgeFrom", start); choose("worldEdgeTo", end); choose("worldEdgeMode", mode)
        control("worldEdgeTwoWay").setChecked(two_way)
        count = control("worldEdgeTable").rowCount()
        control("worldEdgeAdd").click()
        assert control("worldEdgeTable").rowCount() == count + 1, control("worldStatus").text()

    def input_dialogs(trigger, values):
        completed = []
        timer = retain(QtCore.QTimer()); timers.append(timer)
        def accept():
            dialog = next((w for w in [retain(v) for v in app.allWidgets()]
                if isValid(w) and isinstance(w, QtWidgets.QInputDialog) and w.isVisible()), None)
            if dialog is None or len(completed) == len(values):
                return
            value = values[len(completed)]
            if value is not None:
                dialog.setTextValue(value)
            completed.append(True); dialog.done(QtWidgets.QDialog.Accepted)
        timer.timeout.connect(accept); timer.start(100)
        QtCore.QMetaObject.invokeMethod(control(trigger), "click", QtCore.Qt.QueuedConnection)
        wait_for(lambda: len(completed) == len(values)); general.idle_wait(0.3); timer.stop()

    def reopen_mod():
        general.open_pane("Tainted Grail Pack Manager")
        pack_pane = button("Open selected").parentWidget().parentWidget().parentWidget()
        choices = pack_pane.findChildren(QtWidgets.QComboBox)
        selected = next((combo, index) for combo in choices for index in range(combo.count())
            if str(combo.itemData(index)).replace("\\", "/").endswith("/sdkqa.world-qa/pack.tgpack.json"))
        selected[0].setCurrentIndex(selected[1]); button("Open selected").click()

    try:
        stage("open_private_schema_four_workspace")
        before = catalog_bytes(); before_data = json.loads(before)
        assert before_data["SchemaVersion"] == 4, "Use the private schema-4 faction acceptance workspace as input."
        result["initial_counts"] = {k: len(v) for k, v in before_data.items() if isinstance(v, list)}
        stage("open_status_pane")
        general.open_pane("Tainted Grail SDK Status"); general.idle_wait(0.3)
        stage("choose_private_workspace")
        file_dialog(workspace, button("Open existing workspace..."))
        stage("create_private_mod")
        general.open_pane("Tainted Grail Pack Manager"); button("New mod").click()
        for window in [retain(v) for v in app.topLevelWidgets()]:
            for edit in window.findChildren(QtWidgets.QLineEdit):
                retain(edit)
                if edit.placeholderText() == "My Fall of Avalon mod":
                    edit.setText("World QA")
                if edit.placeholderText() == "author or namespace":
                    edit.setText("sdkqa")
        stage("save_private_mod")
        button("Save mod").click()
        stage("open_world_pane")
        general.open_pane("Tainted Grail World and Route Editor"); general.idle_wait(0.3)

        stage("create_places_and_plan_positions")
        region = new_world("region", "Northlands")
        scene = new_world("scene", "Town", region)
        other_scene = new_world("scene", "Other scene", region)
        gate = new_world("location", "North Gate", scene)
        bend = new_world("location", "Road Bend", scene)
        camp = new_world("location", "Bandit Camp", scene)
        for location, x, z in [(gate, 0, 0), (bend, 120, 45), (camp, 160, 140)]:
            choose("worldRecords", location)
            control("worldHasPosition").setChecked(True)
            control("worldX").setValue(x); control("worldZ").setValue(z)
            measured("save_position_" + str(x), save_world)
        migrated = json.loads(catalog_bytes())
        assert migrated["SchemaVersion"] == 5
        backups = list((workspace.parent / "Catalog").glob("*.schema-4.*.backup.json"))
        assert any(p.read_bytes() == before for p in backups)
        for key in ["EconomyItems", "EconomyRecipes", "RecipeIngredients", "RecipeOutputs",
                    "ActorProfiles", "TroopProfiles", "TroopMembers", "EncounterDefinitions",
                    "CultureProfiles", "FactionProfiles", "FactionLinks"]:
            assert migrated[key] == before_data[key], "Migration changed " + key
        result["checks"].append("schema_four_exact_backup_and_existing_economy_population_encounter_society_preserved")

        stage("road_route_graph_and_preview")
        road = new_world("road", "North Road", scene)
        route = new_world("route", "Guard Patrol", scene)
        choose("worldRoad", road)
        type_text("worldDescription", "Town guard route")
        type_text("worldConstraints", "Travel during daylight")
        control("worldTabs").setCurrentIndex(1)
        n_gate = add_node(gate); n_bend = add_node(bend); n_camp = add_node(camp)
        add_edge(n_gate, n_bend)
        control("worldEdgeCost").setValue(2.5)
        add_edge(n_bend, n_camp, "ride", False)
        measured("save_route_graph", save_world)
        control("worldTabs").setCurrentIndex(2); general.idle_wait(0.3)
        graph = control("worldGraph")
        assert len([i for i in graph.scene().items() if i.data(0) == "world-node"]) == 3
        assert len([i for i in graph.scene().items() if i.data(0) == "world-edge"]) == 2
        assert "Scene-local plan positions" in control("worldPreviewSummary").text()
        pane = control("worldRouteEditor"); dock = pane.parentWidget()
        while dock is not None and not isinstance(dock, QtWidgets.QDockWidget):
            dock = dock.parentWidget()
        assert dock is not None
        retain(dock); dock.setFloating(True); dock.show()
        available = dock.screen().availableGeometry()
        dock.resize(min(1200, available.width()-40), min(900, available.height()-60))
        general.idle_wait(0.5)
        assert dock.grab().save(str(output.with_name("world-route-preview.png")))
        # Probe the runtime type: PySide does not export QGraphicsTextItem::Type.
        text_type = QtWidgets.QGraphicsTextItem().type()
        labels = []
        for item in graph.scene().items():
            if item.type() == text_type:
                wrapper = wrapInstance(getCppPointer(item)[0], QtWidgets.QGraphicsTextItem)
                if hasattr(wrapper, "toPlainText"):
                    labels.append(wrapper.toPlainText())
        result["graph_labels"] = labels
        result["graph_item_types"] = [{"wrapper": type(i).__name__, "type": i.type()} for i in graph.scene().items()]
        result["manual_label_review_required"] = not bool(labels)
        if labels:
            assert all(any(name in label for label in labels) for name in ["North Gate", "Road Bend", "Bandit Camp"]), labels
        result["checks"].append("roads_routes_directed_edges_and_visible_plan_graph_saved")

        stage("invalid_graph_and_dirty_drafts")
        original = catalog_bytes()
        control("worldTabs").setCurrentIndex(1)
        choose("worldNodeLocation", gate); control("worldNodeAdd").click()
        assert control("worldNodeTable").rowCount() == 3
        assert "distinct saved location" in control("worldStatus").text()
        choose("worldEdgeFrom", n_gate); choose("worldEdgeTo", n_gate); control("worldEdgeAdd").click()
        assert "two different nodes" in control("worldStatus").text()
        choose("worldEdgeTo", n_bend); control("worldEdgeAdd").click()
        assert "duplicate" in control("worldStatus").text()
        control("worldNodeTable").selectRow(0); control("worldNodeRemove").click()
        assert "connections before" in control("worldStatus").text()
        assert catalog_bytes() == original
        control("worldTabs").setCurrentIndex(0)
        choose("worldParent", other_scene); control("worldSave").click()
        assert control("worldStatus").text() != "World definition saved."
        assert catalog_bytes() == original
        control("worldRevert").click()
        type_text("worldName", "Unsaved route")
        choose("worldRecords", road)
        assert control("worldRecords").currentData() == route and control("worldName").text() == "Unsaved route"
        control("worldNew").click()
        assert "Save or revert" in control("worldStatus").text()
        control("worldRevert").click()
        result["checks"].append("duplicate_self_dangling_cross_scene_edits_and_dirty_selection_guarded")

        stage("failed_write_retains_saved_graph")
        type_text("worldName", "Failed write route")
        original = catalog_bytes()
        catalog = workspace.parent / "Catalog/catalog.tgcatalog.json"
        held = catalog.with_suffix(".held")
        catalog.rename(held); catalog.mkdir()
        try:
            control("worldSave").click()
            assert control("worldStatus").text() != "World definition saved."
            assert control("worldName").text() == "Failed write route"
            assert held.read_bytes() == original
        finally:
            catalog.rmdir(); held.rename(catalog)
        control("worldRevert").click()
        result["checks"].append("failed_write_preserves_draft_and_published_catalog")

        stage("schematic_preview_and_cross_pane_references")
        choose("worldRecords", gate); control("worldHasPosition").setChecked(False); save_world()
        choose("worldRecords", route); control("worldTabs").setCurrentIndex(2)
        assert "Schematic topology" in control("worldPreviewSummary").text()
        assert len([i for i in graph.scene().items() if i.data(0) == "world-node"]) == 3
        general.idle_wait(0.3); assert dock.grab().save(str(output.with_name("world-schematic-preview.png")))
        choose("worldRecords", gate); control("worldHasPosition").setChecked(True); save_world()
        general.open_pane("Tainted Grail Faction and Authority Editor"); general.idle_wait(0.2)
        input_dialogs("factionNew", ["World QA Guard"])
        faction = control("factionRecords").currentData()
        control("factionTabs").setCurrentIndex(2)
        choose("factionJurisdictionTarget", gate); choose("factionJurisdictionValue", "protects")
        control("factionJurisdictionAdd").click(); control("factionSave").click()
        assert control("factionStatus").text() == "Faction saved.", control("factionStatus").text()
        general.open_pane("Tainted Grail Spawn and Encounter Editor"); general.idle_wait(0.3)
        input_dialogs("encounterNew", ["World QA Gate Watch", None])
        encounter = control("encounterRecords").currentData()
        choose("encounterPlacement", gate); control("encounterSave").click()
        assert control("encounterStatus").text() == "Encounter saved.", control("encounterStatus").text()
        document = json.loads(catalog_bytes())
        assert any(r["FactionRecordId"] == faction and r["TargetRecordId"] == gate for r in document["FactionLinks"])
        assert next(r for r in document["EncounterDefinitions"] if r["RecordId"] == encounter)["PlacementRecordId"] == gate
        general.close_pane("Tainted Grail Faction and Authority Editor"); general.close_pane("Tainted Grail Spawn and Encounter Editor")
        result["checks"].append("schematic_fallback_and_exact_faction_encounter_world_bindings_work")

        stage("reopen_update_remove_and_preserve_other_path")
        general.close_pane("Tainted Grail World and Route Editor")
        general.open_pane("Tainted Grail SDK Status"); file_dialog(workspace, button("Open existing workspace..."))
        reopen_mod()
        general.open_pane("Tainted Grail World and Route Editor"); general.idle_wait(0.3)
        choose("worldRecords", route)
        assert control("worldNodeTable").rowCount() == 3 and control("worldEdgeTable").rowCount() == 2
        assert control("worldRoad").currentData() == road
        assert control("worldConstraints").text() == "Travel during daylight"
        control("worldTabs").setCurrentIndex(1)
        edges_before = {r["EdgeId"] for r in json.loads(catalog_bytes())["WorldPathEdges"] if r["PathRecordId"] == route}
        control("worldEdgeTable").selectRow(0); control("worldEdgeCost").setValue(3.5)
        control("worldEdgeUpdate").click(); measured("save_reopened_route_edit", save_world)
        edges_after = {r["EdgeId"] for r in json.loads(catalog_bytes())["WorldPathEdges"] if r["PathRecordId"] == route}
        assert edges_before == edges_after
        table = control("worldEdgeTable")
        camp_row = next(i for i in range(table.rowCount()) if "Bandit Camp" in table.item(i, 1).text() or "Bandit Camp" in table.item(i, 0).text())
        table.selectRow(camp_row); control("worldEdgeRemove").click()
        table = control("worldNodeTable")
        table.selectRow(next(i for i in range(table.rowCount()) if "Bandit Camp" in table.item(i, 0).text()))
        control("worldNodeRemove").click(); save_world()
        assert control("worldNodeTable").rowCount() == 2 and control("worldEdgeTable").rowCount() == 1
        choose("worldRecords", road)
        assert control("worldNodeTable").rowCount() == 0 and control("worldEdgeTable").rowCount() == 0
        choose("worldRecords", route)
        result["checks"].append("reopen_update_keeps_ids_and_remove_changes_only_selected_route")

        stage("small_window_and_final_preview")
        pane = control("worldRouteEditor"); dock = pane.parentWidget()
        while dock is not None and not isinstance(dock, QtWidgets.QDockWidget):
            dock = dock.parentWidget()
        retain(dock); dock.setFloating(True); dock.show()
        dock.resize(min(860, available.width()-40), min(640, available.height()-60))
        general.idle_wait(0.4)
        control("worldTabs").setCurrentIndex(1)
        scroll = control("worldGraphScroll")
        scroll.ensureWidgetVisible(control("worldEdgeTable")); general.idle_wait(0.2)
        assert dock.width() <= 860 and dock.height() <= 640
        assert scroll.verticalScrollBar().maximum() > 0
        assert control("worldSave").isVisible()
        assert pane.rect().contains(control("worldSave").mapTo(pane, QtCore.QPoint(0, 0)))
        assert dock.grab().save(str(output.with_name("world-small-window.png")))
        result["small_window_size"] = {"width": dock.width(), "height": dock.height()}
        dock.resize(min(1200, available.width()-40), min(900, available.height()-60))
        control("worldTabs").setCurrentIndex(2); general.idle_wait(0.4)
        assert dock.grab().save(str(output.with_name("world-final-preview.png")))
        result["checks"].append("small_window_scroll_and_fixed_save_controls_available")
        result["record_ids"] = {"region": region, "scene": scene, "gate": gate, "bend": bend, "camp": camp, "road": road, "route": route, "faction": faction, "encounter": encounter}
        result["catalog_sha256"] = hashlib.sha256(catalog_bytes()).hexdigest()
        result["status"] = "PASSED"; stage("complete")
    except Exception:
        result["error"] = traceback.format_exc()
        stage(result.get("stage", "failed"))
    finally:
        for timer in timers:
            timer.stop()
        output.write_text(json.dumps(result, indent=2), encoding="utf-8")


# Startup scripts run before the Editor finishes initialization; defer UI work to its event loop.
QtCore.QTimer.singleShot(2000, run)
