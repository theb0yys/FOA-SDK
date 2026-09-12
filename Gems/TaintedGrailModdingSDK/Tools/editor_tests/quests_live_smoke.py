# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Exercise quest inspection and authoring through the running Editor in a private workspace."""
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
    output = Path(os.environ["FOA_SDK_QUEST_RESULT"])
    workspace = Path(os.environ["FOA_SDK_QUEST_WORKSPACE"])
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


    def catalog_bytes():
        return (workspace.parent / "Catalog/catalog.tgcatalog.json").read_bytes()

    def measured(name, action):
        beats = []
        timer = retain(QtCore.QTimer()); timers.append(timer)
        timer.timeout.connect(lambda: beats.append(time.monotonic())); timer.start(100)
        start = time.monotonic(); action(); general.idle_wait(0.2)
        end = time.monotonic(); timer.stop()
        gap = max(b-a for a,b in zip([start]+beats, beats+[end]))
        result.setdefault("timings", {})[name] = {"seconds": round(end-start,3), "max_ui_gap_seconds": round(gap,3)}
        assert gap < 3, "UI pause exceeded three seconds: " + str(gap)

    def modal(trigger, apply, object_name=None, input_text=None, answer=None):
        completed, errors = [], []
        timer = retain(QtCore.QTimer()); timers.append(timer)
        def act():
            dialogs = [retain(w) for w in app.allWidgets()]
            dialog = next((w for w in dialogs if isValid(w) and w.isVisible() and
                ((object_name and w.objectName() == object_name)
                 or (input_text is not None and isinstance(w, QtWidgets.QInputDialog))
                 or (answer is not None and isinstance(w, QtWidgets.QMessageBox)))), None)
            if dialog is None:
                return
            timer.stop()
            try:
                if input_text is not None:
                    dialog.setTextValue(input_text)
                apply(dialog)
                if answer is not None:
                    dialog.button(answer).click()
                else:
                    dialog.accept()
            except Exception:
                errors.append(traceback.format_exc()); dialog.reject()
            completed.append(True)
        timer.timeout.connect(act); timer.start(100)
        QtCore.QMetaObject.invokeMethod(control(trigger), "click", QtCore.Qt.QueuedConnection)
        wait_for(lambda: completed)
        general.idle_wait(0.3)
        assert not errors, errors

    def create_quest(name):
        modal("questNew", lambda d: None, input_text=name)
        assert "Created a local quest" in control("questStatus").text(), control("questStatus").text()
        return control("questRecords").currentData()

    def save():
        control("questSave").click()
        assert "Quest saved." in control("questStatus").text(), control("questStatus").text()

    def kind(index):
        control("questTabs").setCurrentIndex(1)
        control("questElementKind").setCurrentIndex(index)
        control("questElementSearch").clear()

    def select_row(identity):
        table = control("questElements")
        row = next(i for i in range(table.rowCount()) if table.item(i,0).data(QtCore.Qt.UserRole) == identity)
        table.selectRow(row)

    def rows():
        table = control("questElements")
        return [table.item(i,0).data(QtCore.Qt.UserRole) for i in range(table.rowCount())]

    def element(kind_index, values, selected=None):
        kind(kind_index)
        if selected:
            select_row(selected)
        before = set(rows())
        def apply(dialog):
            for key,value in values.items():
                widget = control("questRow_"+key)
                if isinstance(widget,QtWidgets.QLineEdit):
                    widget.setText(str(value))
                elif isinstance(widget,QtWidgets.QComboBox):
                    index=widget.findData(value)
                    if index >= 0:
                        widget.setCurrentIndex(index)
                    else:
                        assert widget.isEditable(), ("Missing choice",key,value)
                        widget.setEditText(str(value))
                elif isinstance(widget,QtWidgets.QCheckBox):
                    widget.setChecked(value)
                elif isinstance(widget,QtWidgets.QListWidget):
                    for i in range(widget.count()):
                        widget.item(i).setCheckState(QtCore.Qt.Checked if widget.item(i).data(QtCore.Qt.UserRole) in value else QtCore.Qt.Unchecked)
        modal("questElementEdit" if selected else "questElementAdd",apply,object_name="questRowDialog")
        assert "Element updated" in control("questStatus").text(), control("questStatus").text()
        return selected or next(x for x in rows() if x not in before)

    def current_profile(quest_id):
        return next(q for q in json.loads(catalog_bytes())["QuestProfiles"] if q["RecordId"] == quest_id)

    def reopen_mod():
        general.open_pane("Tainted Grail Pack Manager")
        window=button("Open selected").window()
        pair=next((c,i) for c in window.findChildren(QtWidgets.QComboBox) for i in range(c.count())
            if str(c.itemData(i)).replace("\\","/").endswith("/sdkqa.quest-qa/pack.tgpack.json"))
        pair[0].setCurrentIndex(pair[1]); button("Open selected").click()

    try:
        stage("open_private_schema_five_workspace")
        before=catalog_bytes(); initial=json.loads(before)
        assert initial["SchemaVersion"] == 5
        result["initial_counts"]={k:len(v) for k,v in initial.items() if isinstance(v,list)}
        general.open_pane("Tainted Grail SDK Status"); general.idle_wait(0.3)
        file_dialog(workspace,button("Open existing workspace..."))
        general.open_pane("Tainted Grail Pack Manager"); button("New mod").click()
        for window in [retain(v) for v in app.topLevelWidgets()]:
            for edit in window.findChildren(QtWidgets.QLineEdit):
                retain(edit)
                if edit.placeholderText()=="My Fall of Avalon mod": edit.setText("Quest QA")
                if edit.placeholderText()=="author or namespace": edit.setText("sdkqa")
        button("Save mod").click()
        general.open_pane("Tainted Grail Quest and State Inspector"); general.idle_wait(0.3)
        quest=create_quest("Find the North Gate")
        migrated=json.loads(catalog_bytes())
        assert migrated["SchemaVersion"] == 7
        assert any(p.read_bytes()==before for p in (workspace.parent/"Catalog").glob("*.schema-5.*.backup.json"))
        for key,value in initial.items():
            if isinstance(value,list) and key not in ("Records",):
                assert migrated[key] == value, "Migration changed "+key
        new_records={r["RecordId"]:r for r in migrated["Records"]}
        assert all(new_records[r["RecordId"]]==r for r in initial["Records"])
        result["checks"].append("schema_five_exact_backup_and_prior_domain_collections_preserved")

        stage("edit_elements_states_and_exact_bindings")
        definition=json.loads(current_profile(quest)["DefinitionJson"])
        start=next(p["phase_id"] for p in definition["phases"] if p["entry_phase"])
        end=next(p["phase_id"] for p in definition["phases"] if p["terminal_phase"])
        transition=definition["transitions"][0]["transition_id"]
        outcome=definition["outcomes"][0]["outcome_id"]
        state=element(8,{"key":"state.qa.gate-visited","type":"boolean","default":"false","description":"Authored visit flag"})
        actor=next(r["RecordId"] for r in initial["Records"] if r["RecordKind"]=="actor")
        location=next(r["RecordId"] for r in initial["Records"] if r["RecordKind"]=="location" and r["Domain"]=="world")
        item=next(r["RecordId"] for r in initial["Records"] if r["RecordKind"]=="item")
        role=element(6,{"label":"Gate guide","required":True})
        item_role=element(6,{"label":"Travel token","required":False})
        element(9,{"subject":role,"record":actor})
        element(9,{"subject":item_role,"record":item})
        condition=element(3,{"label":"Gate visited","type":"fact.equals","subject":state})
        place_condition=element(3,{"label":"At North Gate","type":"location.presence","subject":"subject.qa.gate"})
        element(9,{"subject":"subject.qa.gate","record":location})
        action=element(4,{"label":"Remember visit","type":"fact.set","subject":state})
        objective=element(1,{"label":"Reach the gate","phase":start,"conditions":[place_condition],"actions":[action]})
        element(2,{"label":"Complete the journey","from":start,"to":end,"trigger":"trigger.qa.gate","priority":"2","repeat":False,"conditions":[condition],"actions":[]},transition)
        element(0,{"label":"Journey begins","entry":True,"terminal":False,"actions":[]},start)
        element(5,{"label":"Gate reached","phase":end},outcome)
        element(7,{"label":"Guide requirement","role":role,"subjectKind":"subject.actor","usage":"usage.quest-guide"})
        type_text("questDescription","A local authoring fixture linking a guide, token and world location.")
        measured("save_full_quest",save)
        profile=current_profile(quest)
        assert len(profile["StateKeys"])==1 and len(profile["Bindings"])==3
        assert len(json.loads(profile["DefinitionJson"])["objectives"])==2
        result["checks"].append("all_quest_element_forms_state_keys_and_actor_item_location_links_save")

        stage("visible_graph_and_search")
        pane=control("questStateEditor"); dock=pane.parentWidget()
        while dock is not None and not isinstance(dock,QtWidgets.QDockWidget): dock=dock.parentWidget()
        retain(dock); assert dock is not None; dock.setFloating(True); dock.show()
        available=app.primaryScreen().availableGeometry()
        dock.resize(min(1200,available.width()-40),min(900,available.height()-60))
        control("questTabs").setCurrentIndex(2); general.idle_wait(0.5)
        scene=control("questGraph").scene()
        graph_items=[retain(i) for i in scene.items()]
        assert sum(i.data(0)=="quest-phase" for i in graph_items)==2
        assert sum(i.data(0)=="quest-transition" for i in graph_items)==1
        assert dock.grab().save(str(output.with_name("quest-progression.png")))
        result["manual_label_review_required"]=True
        type_text("questSearch","Find the North Gate")
        assert control("questRecords").findData(quest)>=0
        control("questSearch").clear()
        kind(8); type_text("questElementSearch","gate-visited")
        assert control("questElements").rowCount()==1
        control("questElementSearch").clear()
        result["checks"].append("search_and_actual_progression_graph_render")

        stage("invalid_and_failed_saves_preserve_draft")
        baseline=catalog_bytes()
        element(8,{"type":"boolean","default":"invalid","description":"Invalid candidate"},state)
        control("questSave").click()
        assert catalog_bytes()==baseline
        assert "Invalid state type/default" in control("questStatus").text()
        assert control("questIssues").rowCount()>0
        modal("questRevert",lambda d:None,answer=QtWidgets.QMessageBox.Yes)
        assert "Reloaded the saved definition" in control("questStatus").text(), control("questStatus").text()
        type_text("questName","Unsaved after failed write")
        catalog=workspace.parent/"Catalog/catalog.tgcatalog.json"; held=catalog.with_suffix(".held")
        catalog.rename(held); catalog.mkdir()
        try:
            control("questSave").click()
            assert control("questName").text()=="Unsaved after failed write"
        finally:
            catalog.rmdir(); held.rename(catalog)
        assert catalog_bytes()==baseline
        control("questRecords").setCurrentIndex(0)
        assert control("questRecords").currentData()==quest
        assert "Save or revert" in control("questStatus").text()
        modal("questRevert",lambda d:None,answer=QtWidgets.QMessageBox.Yes)
        assert "Reloaded the saved definition" in control("questStatus").text(), control("questStatus").text()
        result["checks"].append("invalid_write_failure_and_dirty_selection_preserve_published_state")

        stage("read_only_import_and_local_adoption")
        source=output.with_name("synthetic-inspection.tgquest.json")
        # Use a self-contained valid definition so adoption does not require extra local metadata.
        copy_id=create_quest("Inspection source")
        source.write_text(current_profile(copy_id)["DefinitionJson"],encoding="utf-8")
        snapshot=catalog_bytes()
        file_dialog(source,control("questLoad"))
        assert catalog_bytes()==snapshot
        assert control("questSave").text()=="Save editable copy to mod"
        save(); adopted=control("questRecords").currentData()
        assert adopted!=copy_id
        invalid=output.with_name("malformed-inspection.tgquest.json"); invalid.write_text('{"schema":"bad","schema_version":99}',encoding="utf-8")
        snapshot=catalog_bytes(); file_dialog(invalid,control("questLoad"))
        assert catalog_bytes()==snapshot and control("questIssues").rowCount()>0
        choose("questRecords",quest)
        result["checks"].append("read_only_valid_invalid_imports_and_explicit_local_copy")

        stage("reopen_edit_remove_and_small_window")
        file_dialog(workspace,button("Open existing workspace...")); reopen_mod()
        general.open_pane("Tainted Grail Quest and State Inspector"); choose("questRecords",quest)
        kind(8); assert rows()==[state]
        element(8,{"type":"boolean","default":"true","description":"Edited after reopen"},state)
        measured("save_reopened_edit",save)
        assert current_profile(quest)["StateKeys"][0]["DefaultValue"]=="true"
        extra=element(8,{"key":"state.qa.unused","type":"text","default":"temporary","description":""})
        save(); kind(8); select_row(extra); control("questElementRemove").click(); save()
        assert len(current_profile(quest)["StateKeys"])==1
        dock.resize(min(860,available.width()-40),min(640,available.height()-60)); general.idle_wait(0.3)
        assert dock.width()<=860 and dock.height()<=640
        assert pane.rect().contains(control("questSave").mapTo(pane,QtCore.QPoint(0,0)))
        assert dock.grab().save(str(output.with_name("quest-small-window.png")))
        result["small_window_size"]={"width":dock.width(),"height":dock.height()}
        dock.resize(min(1200,available.width()-40),min(900,available.height()-60))
        control("questTabs").setCurrentIndex(2); general.idle_wait(0.4)
        assert dock.grab().save(str(output.with_name("quest-final-preview.png")))
        result["checks"].append("reopen_update_remove_preserve_ids_and_small_window_controls_fit")
        result["quest_id"]=quest; result["catalog_sha256"]=hashlib.sha256(catalog_bytes()).hexdigest()
        result["status"]="PASSED"; stage("complete")
    except Exception:
        result["error"]=traceback.format_exc()
        try:
            result["quest_status"]=control("questStatus").text()
            control("questStateEditor").grab().save(str(output.with_name("quest-failure.png")))
        except Exception:
            pass
        stage(result.get("stage","failed"))
    finally:
        for timer in timers: timer.stop()
        output.write_text(json.dumps(result,indent=2),encoding="utf-8")


QtCore.QTimer.singleShot(2000,run)
