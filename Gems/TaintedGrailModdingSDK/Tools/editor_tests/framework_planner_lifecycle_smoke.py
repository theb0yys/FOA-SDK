#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""M4 acceptance in a disposable Editor: open, refresh by recreation, close and reopen six existing preview panes.

No native input, provider, execution, game installation or user workspace is loaded.
FOA_M4_EDITOR_OUTPUT is a private evidence directory supplied by the harness.
"""
import json
import os
from pathlib import Path
import time

from PySide6 import QtCore, QtWidgets
from shiboken6 import isValid
import azlmbr.legacy.general as general

PANES = (
    ("Tainted Grail Adapter Work-Order Plans", "AdapterWorkOrderPlans", 11),
    ("Tainted Grail Adapter Build Manifests", "AdapterBuildManifests", 11),
    ("Tainted Grail Package Assembly Preview", "PackageAssemblyPreview", 11),
    ("Tainted Grail Staging and Deployment Preview", "StagingDeploymentPreview", 13),
    ("Tainted Grail Deployment Confirmation and Work Orders", "DeploymentWorkOrders", 11),
    ("Tainted Grail Post-Deployment Verification and Release Blockers", "PostDeploymentVerification", 10),
)
output = Path(os.environ["FOA_M4_EDITOR_OUTPUT"])
output.mkdir(parents=True, exist_ok=True)
app = QtWidgets.QApplication.instance()
result = {"status": "RUNNING", "panes": [], "about_to_quit": False, "input_fixture": "empty disposable workspace"}
started = time.monotonic()
index = 0
stage = "open"
deadline = started + 180

def save():
    (output / "planner-lifecycle.json").write_text(json.dumps(result, indent=2), encoding="utf-8")

def find_pane(key):
    return next((w for w in app.allWidgets() if isValid(w) and isinstance(w, QtWidgets.QDockWidget)
                 and w.isVisible() and w.objectName() == "TaintedGrailModdingSDK." + key), None)

def quitting():
    result["about_to_quit"] = True
    result["seconds"] = time.monotonic() - started
    result["status"] = "PASSED" if len(result["panes"]) == len(PANES) and "error" not in result else "FAILED"
    save()

def tick():
    global index, stage, deadline
    try:
        assert time.monotonic() < deadline, "Preview pane lifecycle deadline exceeded"
        if index == len(PANES):
            save()
            general.exit()
            return
        name, key, columns = PANES[index]
        if stage == "open":
            general.open_pane(name)
            stage = "inspect"
        elif stage in ("inspect", "inspect_reopened"):
            pane = find_pane(key)
            if pane is None:
                QtCore.QTimer.singleShot(100, tick)
                return
            assert general.is_pane_visible(name), name + " is not visible"
            tables = [t for t in pane.findChildren(QtWidgets.QTableWidget) if isValid(t)]
            assert len(tables) == 1, name + " requires its preview table"
            table = tables[0]
            assert table.columnCount() == columns, (name, table.columnCount(), columns)
            assert table.editTriggers() == QtWidgets.QAbstractItemView.NoEditTriggers
            if stage == "inspect":
                assert pane.grab().save(str(output / (key + ".png"))), "Could not capture pane"
                general.close_pane(name)
                stage = "closed"
            else:
                result["panes"].append({"name": name, "columns": table.columnCount(),
                                       "rows": table.rowCount(), "opened": True, "closed": True, "reopened": True})
                general.close_pane(name)
                index += 1
                stage = "open"
                save()
        elif stage == "closed":
            if general.is_pane_visible(name) or find_pane(key) is not None:
                QtCore.QTimer.singleShot(100, tick)
                return
            general.open_pane(name)
            stage = "inspect_reopened"
        QtCore.QTimer.singleShot(250, tick)
    except Exception as error:
        result["error"] = str(error)
        result["status"] = "FAILED"
        save()
        general.exit()

app.aboutToQuit.connect(quitting)
app._m4_lifecycle = (tick, quitting)
save()
QtCore.QTimer.singleShot(1500, tick)
