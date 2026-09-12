# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Exercise image, translation and assignment authoring through the running Editor in a private workspace."""
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
    output = Path(os.environ["FOA_SDK_ASSET_RESULT"])
    workspace = Path(os.environ["FOA_SDK_ASSET_WORKSPACE"])
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
        result["file_picker"] = {"enabled": trigger.isEnabled(), "visible": trigger.isVisible(), "window": trigger.window().windowTitle()}
        stage(result.get("stage", "file_picker"))
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


    def saved_entry():
        control("assetSave").click()
        assert "Entry saved." in control("assetStatus").text(), control("assetStatus").text()
        return control("assetRecords").currentData()

    def add_variant(language, text):
        def apply(dialog):
            control("assetVariantLanguage").setText(language)
            control("assetVariantText").setPlainText(text)
        modal("assetVariantAdd", apply, object_name="assetVariantDialog")

    def reopen_pack():
        general.open_pane("Tainted Grail Pack Manager"); general.idle_wait(0.2)
        window = button("Open selected").window()
        pair = next((c,i) for c in window.findChildren(QtWidgets.QComboBox) for i in range(c.count())
            if str(c.itemData(i)).replace("\\","/").endswith("/preview.developer-preview-0/pack.tgpack.json"))
        pair[0].setCurrentIndex(pair[1])
        stage("opening_selected_synthetic_mod")
        button("Open selected").click()
        assert "Unable" not in control("packStatus").text(), control("packStatus").text()
        stage("synthetic_mod_opened")

    def screenshot(name):
        general.idle_wait(0.3)
        assert dock.grab().save(str(output.with_name(name)))

    def assignment(target, slot, value):
        control("assetTabs").setCurrentIndex(1)
        choose("assetTarget", target); choose("assetSlot", slot); choose("assetValue", value)
        control("assetAssign").click()
        assert "Assignment saved" in control("assetStatus").text(), control("assetStatus").text()

    watchdog = retain(QtCore.QTimer()); timers.append(watchdog)
    def inspect_pending_dialogs():
        snapshots = []
        for w in [retain(v) for v in app.topLevelWidgets()]:
            if isValid(w):
                snapshots.append({"title": w.windowTitle(), "name": w.objectName(), "visible": w.isVisible(), "class": w.metaObject().className(),
                    "labels": [label.text() for label in w.findChildren(QtWidgets.QLabel)]})
                if w.isVisible() and isinstance(w, QtWidgets.QDialog):
                    w.grab().save(str(output.with_name("pending-dialog.png")))
        output.with_name("pending-dialogs.json").write_text(json.dumps(snapshots, indent=2), encoding="utf-8")
    watchdog.timeout.connect(inspect_pending_dialogs); watchdog.start(5000)

    try:
        stage("open_synthetic_workspace")
        before = catalog_bytes(); initial = json.loads(before)
        general.open_pane("Tainted Grail SDK Status"); general.idle_wait(0.3)
        file_dialog(workspace, button("Open existing workspace..."))
        stage("synthetic_workspace_opened")
        reopen_pack()
        general.open_pane("Tainted Grail Asset and Localisation Manager"); general.idle_wait(0.3)
        pane = control("assetLocalisationManager"); dock = pane.parentWidget()
        while dock is not None and not isinstance(dock,QtWidgets.QDockWidget): dock = dock.parentWidget()
        retain(dock); assert dock is not None; dock.setFloating(True); dock.show()
        available=app.primaryScreen().availableGeometry()
        dock.resize(min(1120,available.width()-40),min(900,available.height()-60))

        stage("image_intake_preview_save_reopen")
        source = output.with_name("synthetic-shield.png")
        artwork = QtGui.QImage(320,240,QtGui.QImage.Format_ARGB32); artwork.fill(QtGui.QColor("#1c2836"))
        painter = QtGui.QPainter(artwork); painter.setRenderHint(QtGui.QPainter.Antialiasing)
        painter.setPen(QtGui.QPen(QtGui.QColor("#eacb82"),6)); painter.setBrush(QtGui.QColor("#357b89"))
        painter.drawPolygon(QtGui.QPolygon([QtCore.QPoint(90,45),QtCore.QPoint(230,45),QtCore.QPoint(215,160),QtCore.QPoint(160,205),QtCore.QPoint(105,160)]))
        painter.setPen(QtGui.QPen(QtGui.QColor("#eacb82"),8)); painter.drawLine(160,75,160,160); painter.drawLine(130,115,190,115); painter.end()
        assert artwork.save(str(source)); source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
        control("assetNewImage").click(); type_text("assetName","Watchkeeper shield")
        type_text("assetProvenance","Original synthetic artwork created for the Editor acceptance test")
        file_dialog(source,control("assetChooseImage"))
        assert not control("assetImagePreview").pixmap().isNull()
        measured("save_image",saved_entry); image_id=control("assetRecords").currentData()
        assert image_id and hashlib.sha256(source.read_bytes()).hexdigest()==source_hash
        screenshot("manager-image.png")
        migrated=json.loads(catalog_bytes()); assert migrated["SchemaVersion"]==7
        assert any(p.read_bytes()==before for p in (workspace.parent/"Catalog").glob("*.schema-6.*.backup.json"))
        assert all(next(r for r in migrated["Records"] if r["RecordId"]==old["RecordId"])==old for old in initial["Records"])
        result["checks"].append("image_preview_immutable_intake_schema_six_backup_prior_records_preserved")

        stage("translation_variants_fallback_duplicate_and_dirty_draft")
        control("assetNewText").click(); type_text("assetTextKey","Watchkeeper.Name")
        add_variant("en","Watchkeeper"); add_variant("fr","Gardien du guet")
        measured("save_translation",saved_entry); text_id=control("assetRecords").currentData()
        type_text("assetPreviewLanguage","fr"); assert control("assetTextPreview").toPlainText()=="Gardien du guet"
        type_text("assetPreviewLanguage","de"); assert control("assetTextPreview").toPlainText()=="Watchkeeper"; assert "default language" in control("assetFallback").text()
        type_text("assetTextKey","Watchkeeper.Draft")
        choose("assetRecords",image_id); assert control("assetTextKey").text()=="Watchkeeper.Draft"
        assert "Save or revert" in control("assetStatus").text()
        modal("assetRevert",lambda d:None,answer=QtWidgets.QMessageBox.Yes)
        assert control("assetTextKey").text()=="Watchkeeper.Name"
        before_duplicate=catalog_bytes(); control("assetNewText").click(); type_text("assetTextKey","Watchkeeper.Name"); add_variant("en","Duplicate")
        control("assetSave").click(); assert "already contains" in control("assetStatus").text(); assert catalog_bytes()==before_duplicate
        modal("assetRevert",lambda d:None,answer=QtWidgets.QMessageBox.Yes)
        choose("assetRecords",text_id); screenshot("manager-translations.png")
        result["checks"].append("language_add_exact_preview_explicit_fallback_duplicate_key_and_dirty_selection_guard")

        stage("item_actor_quest_assignments_and_previews")
        records=json.loads(catalog_bytes())["Records"]
        item=next(r["RecordId"] for r in records if r["RecordKind"]=="item" and r["Domain"]=="economy")
        general.open_pane("Tainted Grail Actor and Troop Editor"); general.idle_wait(0.3)
        modal("populationNewActor", lambda d:None, input_text="Watchkeeper")
        actor=control("populationActor").currentData()
        assert actor, "The synthetic actor was not created"
        general.open_pane("Tainted Grail Quest and State Inspector")
        modal("questNew",lambda d:None,input_text="Watchkeeper patrol")
        assert "Created a local quest" in control("questStatus").text(),control("questStatus").text()
        quest=control("questRecords").currentData()
        measured("assign_item_icon",lambda:assignment(item,"icon",image_id))
        assert not control("assetAssignmentPreview").pixmap().isNull()
        assignment(actor,"portrait",image_id); screenshot("manager-assignment.png")
        assignment(quest,"name",text_id); assert control("assetAssignmentPreview").text()=="Watchkeeper"
        type_text("assetPreviewLanguage","fr")
        assert control("assetAssignmentPreview").text()=="Gardien du guet"
        assert "default language" not in control("assetBindingStatus").text()
        type_text("assetPreviewLanguage","de")
        assignment(item,"description",text_id); assignment(actor,"name",text_id)
        control("assetClearAssignment").click(); assert "cleared" in control("assetStatus").text()
        assert next(b for b in json.loads(catalog_bytes())["PresentationBindings"] if b["TargetRecordId"]==actor and b["Slot"]=="name")["ValueRecordId"]==""
        assignment(actor,"name",text_id)
        general.open_pane("Tainted Grail Item and Recipe Editor"); general.idle_wait(0.3)
        item_combo=control("economyItemChoice")
        item_combo.setCurrentIndex(item_combo.findData(item)); assert not control("economyAssignedIcon").pixmap().isNull(), control("economyAssignedIcon").text()
        general.open_pane("Tainted Grail Actor and Troop Editor"); general.idle_wait(0.3)
        actor_combo=control("populationActor"); actor_combo.setCurrentIndex(actor_combo.findData(actor))
        assert not control("populationPortrait").pixmap().isNull()
        result["checks"].append("assign_replace_clear_for_items_actors_quests_and_existing_editor_previews")

        stage("missing_file_and_failed_save_preservation")
        control("assetTabs").setCurrentIndex(0); choose("assetRecords",image_id)
        image_profile=next(p for p in json.loads(catalog_bytes())["ProjectAssets"] if p["RecordId"]==image_id)
        managed=workspace.parent/image_profile["SourcePath"]; held=managed.with_suffix(".held")
        managed.rename(held); choose("assetRecords",text_id); choose("assetRecords",image_id)
        assert control("assetImagePreview").pixmap() is None or control("assetImagePreview").pixmap().isNull()
        assert "readable" in control("assetImageDetails").text()
        held.rename(managed); choose("assetRecords",text_id); choose("assetRecords",image_id)
        assert not control("assetImagePreview").pixmap().isNull()
        choose("assetRecords",text_id); type_text("assetTextKey","Watchkeeper.Renamed")
        catalog=workspace.parent/"Catalog/catalog.tgcatalog.json"; held_catalog=catalog.with_suffix(".held"); old=catalog.read_bytes()
        catalog.rename(held_catalog); catalog.mkdir()
        try:
            control("assetSave").click()
            assert "Entry saved." not in control("assetStatus").text()
            assert control("assetTextKey").text()=="Watchkeeper.Renamed"
        finally:
            catalog.rmdir(); held_catalog.rename(catalog)
        assert catalog.read_bytes()==old
        measured("save_after_recovery",saved_entry)
        result["checks"].append("missing_image_visible_error_and_failed_save_preserves_catalog_and_draft")

        stage("reopen_close_cancel_and_small_window")
        file_dialog(workspace,button("Open existing workspace...")); reopen_pack()
        choose("assetRecords",image_id); assert not control("assetImagePreview").pixmap().isNull()
        choose("assetRecords",text_id); assert control("assetTextKey").text()=="Watchkeeper.Renamed"
        assert control("assetVariants").rowCount()==2
        type_text("assetTextKey","Unsaved.Close")
        timer=retain(QtCore.QTimer()); timers.append(timer); closed=[]
        def cancel_close():
            dialog=next((w for w in [retain(x) for x in app.allWidgets()] if isValid(w) and isinstance(w,QtWidgets.QMessageBox) and w.isVisible()),None)
            if dialog is not None:
                timer.stop(); dialog.button(QtWidgets.QMessageBox.Cancel).click(); closed.append(True)
        timer.timeout.connect(cancel_close); timer.start(100)
        QtCore.QTimer.singleShot(100,lambda:pane.close()); wait_for(lambda:closed)
        assert pane.isVisible() and control("assetTextKey").text()=="Unsaved.Close"
        modal("assetRevert",lambda d:None,answer=QtWidgets.QMessageBox.Yes)
        dock.resize(min(850,available.width()-40),min(650,available.height()-60)); general.idle_wait(0.3)
        assert pane.rect().contains(control("assetSave").mapTo(pane,QtCore.QPoint(0,0)))
        screenshot("manager-small-window.png")
        dock.resize(min(1120,available.width()-40),min(900,available.height()-60))
        choose("assetRecords",image_id); screenshot("manager-final.png")
        result["checks"].append("workspace_reopen_restores_images_translations_assignments_close_cancel_and_small_window")
        result["catalog_sha256"]=hashlib.sha256(catalog_bytes()).hexdigest()
        result["status"]="PASSED"; stage("complete")
    except Exception:
        result["error"]=traceback.format_exc()
        try:
            result["manager_status"]=control("assetStatus").text()
            control("assetLocalisationManager").grab().save(str(output.with_name("manager-failure.png")))
        except Exception:
            pass
        stage(result.get("stage","failed"))
    finally:
        for timer in timers: timer.stop()
        output.write_text(json.dumps(result,indent=2),encoding="utf-8")


QtCore.QTimer.singleShot(2000,run)
