# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Authorized local campaign UI proof. Keep all artifacts private and outside Git.

The launcher isolates LOCALAPPDATA/workspace/user/cache, copies the registered
profile into that workspace, and configures the qualified extraction runtime.
Run import and reopen in separate Editor processes. No game files are written.
"""
import json
import os
import time
import traceback
from pathlib import Path
import azlmbr.legacy.general as general
from PySide6 import QtCore, QtWidgets

root=Path(os.environ["FOA_HEIGHTMAP_TEST_ROOT"])
mode=os.environ.get("FOA_HEIGHTMAP_TEST_MODE","import")
workspace=(Path(os.environ["LOCALAPPDATA"])/"FOA-SDK/Workspace").resolve()
if os.name=="nt":
    workspace=Path("\\\\?\\"+str(workspace))
result={"status":"FAILED","mode":mode,"checks":[]}
output=root/("campaign-editor-"+mode+".json")
app=QtWidgets.QApplication.instance()


def record(stage):
    result["stage"]=stage
    output.write_text(json.dumps(result,indent=2),encoding="utf-8")


def pump(seconds):
    loop=QtCore.QEventLoop()
    QtCore.QTimer.singleShot(round(seconds*1000),loop.quit)
    loop.exec()


def wait_idle(pane):
    deadline=time.monotonic()+310
    while pane.findChild(QtWidgets.QPushButton,"TerrainImportCancel").isEnabled() and time.monotonic()<deadline:
        pump(.1)
    assert not pane.findChild(QtWidgets.QPushButton,"TerrainImportCancel").isEnabled(),"Campaign exceeded its bounded UI operation time"
    return pane.findChild(QtWidgets.QLabel,"TerrainImportStatus").text()


def choose_campaign(pane,name):
    button=pane.findChild(QtWidgets.QPushButton,"TerrainEditVanillaMap")
    assert button.isEnabled(),"Campaign button is unavailable"
    button.click()
    pump(.05)
    dialog=pane.findChild(QtWidgets.QDialog,"TerrainCampaignChooser")
    assert dialog and dialog.isVisible(),"Campaign chooser did not open"
    combo=dialog.findChild(QtWidgets.QComboBox,"TerrainCampaignSelection")
    index=combo.findText(name)
    assert index>=0,"Requested campaign is absent"
    combo.setCurrentIndex(index)
    dialog.findChild(QtWidgets.QDialogButtonBox).button(QtWidgets.QDialogButtonBox.Ok).click()
    assert pane.findChild(QtWidgets.QPushButton,"TerrainImportCancel").isEnabled(),"Campaign operation did not start"


try:
    record("opening")
    general.idle_enable(True)
    general.open_pane("FOA Development Hub")
    record("home-open")
    pump(1.0)
    route=next(w for w in app.allWidgets() if isinstance(w,QtWidgets.QPushButton) and w.text()=="Heightmap Importer")
    assert route.isEnabled(),"Home requires a configured workspace and mod"
    route.click()
    record("route-clicked")
    pump(1.0)
    pane=next(w for w in app.allWidgets() if w.objectName()=="FoaHeightmapImporter")
    record("pane-open")
    wait_idle(pane)
    assert pane.findChild(QtWidgets.QPushButton,"TerrainEditVanillaMap").isEnabled(),"Campaign provider was not discovered"
    result["checks"].append("home-and-campaign-provider")
    if mode=="import":
        choose_campaign(pane,"Horns of the South")
        pane.findChild(QtWidgets.QPushButton,"TerrainImportCancel").click()
        assert "cancelled" in wait_idle(pane).lower(),"Campaign cancellation failed"
        assert not list(workspace.glob("Derived/Terrain/**/terrain.tgheightmap.json")),"Cancelled campaign published a revision"
        result["checks"].append("campaign-worker-cancelled-without-revision")
        for name in ("Horns of the South","Cuanacht","Forlorn Swords","Sarras"):
            record("importing-"+name)
            before=pane.findChild(QtWidgets.QListWidget,"TerrainSavedRevisions").count()
            ticks=[]
            timer=QtCore.QTimer(pane)
            timer.setInterval(10)
            timer.timeout.connect(lambda:ticks.append(time.monotonic()))
            start=time.monotonic()
            timer.start()
            choose_campaign(pane,name)
            message=wait_idle(pane)
            timer.stop()
            assert "imported and saved" in message.lower(),message
            recent=pane.findChild(QtWidgets.QListWidget,"TerrainSavedRevisions")
            assert recent.count()==before+1,"Import must add exactly one new revision"
            assert recent.currentItem().text()==name+" Terrain","Imported campaign does not match its selection"
            assert len(ticks)>2,"Editor stopped responding during extraction"
            result.setdefault("performance",{})[name]={
                "seconds":round(time.monotonic()-start,3),"heartbeat_ticks":len(ticks),
                "maximum_heartbeat_gap_ms":round(max(b-a for a,b in zip(ticks,ticks[1:]))*1000,3)}
            details=pane.findChild(QtWidgets.QLabel,"TerrainHeightDetails").text()
            assert "Ground meshes cover" in details and "estimated" in details,"Source coverage must be visible"
            pixmap=pane.findChild(QtWidgets.QLabel,"TerrainHeightPreview").pixmap()
            assert pixmap and not pixmap.isNull(),"Campaign preview is absent"
            image=pixmap.toImage()
            pixels=[image.pixelColor(x,y).red() for y in range(0,image.height(),17) for x in range(0,image.width(),17)]
            assert max(pixels)-min(pixels)>10,"Campaign preview lacks height variation"
            result["checks"].append(name+"-import-preview-coverage")
    recent=pane.findChild(QtWidgets.QListWidget,"TerrainSavedRevisions")
    assert recent.count()==4,"Four campaign revisions should persist"
    recent.setCurrentRow(next(i for i in range(recent.count()) if recent.item(i).text()=="Horns of the South Terrain"))
    pane.findChild(QtWidgets.QPushButton,"TerrainPreviewRevision").click()
    assert "opened" in wait_idle(pane).lower(),"Campaign revision did not reopen"
    assert "estimated" in pane.findChild(QtWidgets.QLabel,"TerrainHeightDetails").text(),"Coverage notice did not survive reopen"
    result["checks"].append("persisted-campaign-reopened")
    # Local-only artifact: do not attach this protected preview to Git or upload it.
    pane.grab().save(str(root/("campaign-pane-"+mode+".png")))
    general.close_pane("Heightmap Importer")
    pump(.1)
    result["checks"].append("pane-closed")
    result["status"]="PASSED"
except Exception as error:
    result["error"]=str(error)
    result["traceback"]=traceback.format_exc()
    if "pane" in locals():
        result["status_text"]=pane.findChild(QtWidgets.QLabel,"TerrainImportStatus").text()
finally:
    record("finished")
    general.exit_no_prompt()
