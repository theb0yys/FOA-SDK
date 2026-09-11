# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Run the actual Editor package workflow after synthetic asset authoring acceptance."""
import hashlib
import json
import os
from pathlib import Path
import time
import traceback
import azlmbr.legacy.general as general
from PySide6 import QtCore, QtWidgets
from shiboken6 import isValid

output = Path(os.environ["FOA_SDK_PACKAGE_RESULT"])
workspace = Path(os.environ["FOA_SDK_ASSET_WORKSPACE"])
asset_result = output.with_name(output.stem + "-assets.json")
os.environ["FOA_SDK_ASSET_RESULT"] = str(asset_result)
prerequisite_path = Path(__file__).with_name("assets_localisation_live_smoke.py")
# Keep callback globals alive throughout the composed asynchronous Editor test.
prerequisite_namespace = {"__file__": str(prerequisite_path), "__name__": "package_asset_prerequisite"}
exec(compile(prerequisite_path.read_text(encoding="utf-8"), str(prerequisite_path), "exec"), prerequisite_namespace)
app = QtWidgets.QApplication.instance()
retained = []
result = {"status": "FAILED", "checks": [], "timings": {}}
started = time.monotonic()

def stage(name):
    result["stage"] = name
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")

def control(name):
    for window in app.topLevelWidgets():
        retained.append(window)
        if not isValid(window):
            continue
        found = window if window.objectName() == name else window.findChild(QtWidgets.QWidget, name)
        if found is not None:
            retained.append(found)
            return found
    raise AssertionError("Missing control " + name)

def wait_for(predicate, timeout=60):
    begin = time.monotonic()
    while not predicate():
        assert time.monotonic() - begin < timeout, "Timed out: " + result["stage"]
        general.idle_wait(0.05)

def operation(button, done):
    gaps = []
    previous = [time.monotonic()]
    timer = QtCore.QTimer()
    retained.append(timer)
    def tick():
        now = time.monotonic()
        gaps.append(now - previous[0])
        previous[0] = now
    timer.timeout.connect(tick)
    timer.start(50)
    start = time.monotonic()
    control(button).click()
    wait_for(done)
    timer.stop()
    elapsed = time.monotonic() - start
    gap = max(gaps or [elapsed])
    result["timings"][result["stage"]] = {"seconds": round(elapsed, 3), "max_ui_gap_seconds": round(gap, 3)}
    assert gap < 3.0, ("UI responsiveness budget exceeded", gap)

def ready():
    return not control("packageCancel").isEnabled()

def file_dialog(path, button):
    accepted = []
    timer = QtCore.QTimer()
    retained.append(timer)
    def choose():
        for candidate in app.allWidgets():
            retained.append(candidate)
            if isValid(candidate) and isinstance(candidate, QtWidgets.QFileDialog) and candidate.isVisible():
                timer.stop()
                candidate.setDirectory(str(path.parent))
                candidate.selectFile(path.name)
                edit = candidate.findChild(QtWidgets.QLineEdit, "fileNameEdit")
                edit.setText(str(path))
                candidate.accept()
                accepted.append(True)
                return
    timer.timeout.connect(choose)
    timer.start(100)
    control(button).click()
    wait_for(lambda: accepted)
    timer.stop()

def run():
    try:
        stage("image_redistribution_review")
        control("assetTabs").setCurrentIndex(0)
        distribution = control("assetRedistribution")
        distribution.setCurrentIndex(distribution.findData("declared_permitted"))
        control("assetSave").click()
        general.idle_wait(0.2)
        before = json.loads((workspace.parent / "Catalog/catalog.tgcatalog.json").read_text(encoding="utf-8"))
        general.open_pane("Tainted Grail Pack Manager")
        general.idle_wait(0.3)
        control("packBuildExport").click()
        pane = control("modPackageBuilder")
        pane.window().resize(1100, 800)
        stage("preview")
        operation("packagePreview", ready)
        assert control("packageExport").isEnabled(), control("packageStatus").text()
        assert control("packageInventory").rowCount() > 3
        pane.grab().save(str(output.with_name("package-preview.png")))

        stage("export")
        archive = output.with_name("acceptance.tgmod")
        file_dialog(archive, "packageExport")
        wait_for(ready)
        assert archive.is_file(), control("packageStatus").text()
        assert "exported and verified" in control("packageStatus").text()
        assert str(workspace.parent) not in archive.read_text(encoding="utf-8")
        archive_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
        result["checks"].append("preview_reviewed_inventory_and_actual_export")

        stage("inspect")
        control("packageArchivePath").setText(str(archive))
        operation("packageInspect", ready)
        assert not control("packageExport").isEnabled(), "Inspected inventory must not export a different prior preview"
        destination = output.with_name("reopened-workspace")
        control("packageDestination").setText(str(destination))
        assert control("packageImport").isEnabled(), control("packageStatus").text()
        stage("import")
        operation("packageImport", ready)
        assert (destination / "workspace.tgworkspace.json").is_file(), control("packageStatus").text()
        after = json.loads((destination / "Catalog/catalog.tgcatalog.json").read_text(encoding="utf-8"))
        assert {p["RecordId"] for p in after["ProjectAssets"]} == {p["RecordId"] for p in before["ProjectAssets"]}
        assert after["LocalisationEntries"] == before["LocalisationEntries"]
        assert after["PresentationBindings"] == before["PresentationBindings"]
        for profile in after["ProjectAssets"]:
            content = (destination / profile["SourcePath"]).read_bytes()
            assert "sha256:" + hashlib.sha256(content).hexdigest() == profile["Fingerprint"]
        assert hashlib.sha256(archive.read_bytes()).hexdigest() == archive_hash
        result["checks"].append("clean_workspace_reconstruction_preserves_images_translations_assignments")

        stage("existing_target_refused")
        operation("packageImport", ready)
        assert "new workspace" in control("packageStatus").text().lower()
        assert (destination / "workspace.tgworkspace.json").is_file()
        result["checks"].append("existing_destination_preserved")

        stage("open_created_workspace")
        control("packageOpen").click()
        general.idle_wait(0.4)
        assert "Imported workspace opened" in control("packageStatus").text(), control("packageStatus").text()
        general.open_pane("Tainted Grail Asset and Localisation Manager")
        general.idle_wait(0.3)
        records = control("assetRecords")
        records.setCurrentIndex(records.findData(after["ProjectAssets"][0]["RecordId"]))
        assert not control("assetImagePreview").pixmap().isNull()
        pane.grab().save(str(output.with_name("package-reopened.png")))
        result["checks"].append("actual_foundation_open_and_image_preview")

        stage("corrupt_archive_refused")
        corrupt = output.with_name("corrupt.tgmod")
        corrupt.write_bytes(archive.read_bytes()[:100])
        control("packageArchivePath").setText(str(corrupt))
        operation("packageInspect", ready)
        assert not control("packageImport").isEnabled()
        assert "malformed" in control("packageStatus").text().lower()
        control("packageArchivePath").setText(str(archive))
        stage("recovery")
        operation("packageInspect", ready)
        assert control("packageImport").isEnabled()
        pane.window().resize(850, 650)
        general.idle_wait(0.1)
        assert pane.rect().contains(control("packageInspect").mapTo(pane, QtCore.QPoint(0, 0)))
        pane.grab().save(str(output.with_name("package-small-window.png")))
        result["checks"].append("corrupt_archive_recovery_and_small_window")
        stage("cancel_and_recover")
        control("packagePreview").click()
        control("packageCancel").click()
        wait_for(ready)
        assert not control("packageExport").isEnabled()
        stage("preview_recovery")
        operation("packagePreview", ready)
        assert control("packageExport").isEnabled(), control("packageStatus").text()
        result["checks"].append("cancelled_preview_invalidates_inventory_and_recovers")
        stage("close_busy_builder")
        close_start = time.monotonic()
        control("packagePreview").click()
        pane.window().close()
        general.idle_wait(0.2)
        close_seconds = time.monotonic() - close_start
        assert close_seconds < 3.0
        result["timings"]["close_busy_builder"] = {"seconds": round(close_seconds, 3)}
        control("packBuildExport").click()
        pane = control("modPackageBuilder")
        stage("reopen_builder")
        operation("packagePreview", ready)
        assert control("packageExport").isEnabled(), control("packageStatus").text()
        pane.grab().save(str(output.with_name("package-final.png")))
        result["checks"].append("busy_builder_close_and_reopen")
        result["archive_sha256"] = archive_hash
        result["status"] = "PASSED"
        stage("complete")
    except Exception:
        result["error"] = traceback.format_exc()
        try:
            result["ui_status"] = control("packageStatus").text()
            control("modPackageBuilder").grab().save(str(output.with_name("package-failure.png")))
        except Exception:
            pass
        stage(result.get("stage", "failed"))

poll = QtCore.QTimer()
retained.append(poll)
def check_prerequisite():
    if asset_result.exists():
        prerequisite = json.loads(asset_result.read_text(encoding="utf-8"))
        if prerequisite.get("stage") == "complete" and prerequisite.get("status") == "PASSED":
            poll.stop()
            QtCore.QTimer.singleShot(1000, run)
            return
        if "error" in prerequisite:
            poll.stop()
            result["error"] = prerequisite["error"]
            stage("asset_prerequisite_failed")
            return
    if time.monotonic() - started > 300:
        poll.stop()
        result["error"] = "Asset authoring prerequisite timed out."
        stage("asset_prerequisite_timeout")
poll.timeout.connect(check_prerequisite)
poll.start(1000)
