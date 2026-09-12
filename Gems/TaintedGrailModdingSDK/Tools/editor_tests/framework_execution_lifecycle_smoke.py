#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Disposable Editor fixture: real Foundation, real M2 supervisor, actual Qt shutdown.

FOA_M3_EDITOR_OUTPUT, FOA_M3_TEST_DLL and FOA_M2_FIXTURE are private host paths.
FOA_M3_EDITOR_MODE is success or hang. Reuse the success root to verify reopening.
The native entry points exist exclusively in the compiled acceptance-test DLL.
"""
import ctypes
import json
import os
from pathlib import Path
import time

from PySide6 import QtCore, QtWidgets
import azlmbr.legacy.general as general

output = Path(os.environ["FOA_M3_EDITOR_OUTPUT"])
mode = os.environ.get("FOA_M3_EDITOR_MODE", "success")
assert mode in ("success", "hang")
output.mkdir(parents=True, exist_ok=True)
fixture_root = output / mode
(fixture_root / "target").mkdir(parents=True, exist_ok=True)
dll = ctypes.CDLL(os.environ["FOA_M3_TEST_DLL"])
dll.FOAM3EditorStart.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
dll.FOAM3EditorStart.restype = ctypes.c_int
for name in ("FOAM3EditorStatus", "FOAM3EditorArtifactCount", "FOAM3EditorContextVeto", "FOAM3EditorNativeStage"):
    getattr(dll, name).restype = ctypes.c_int
result = {"status": "RUNNING", "mode": mode, "about_to_quit": False}
app = QtWidgets.QApplication.instance()
started = time.monotonic()
exit_started = None

def save():
    (output / "editor-lifecycle.json").write_text(json.dumps(result, indent=2), encoding="utf-8")

def quit_editor():
    global exit_started
    exit_started = time.monotonic()
    save()
    general.exit()

def quitting():
    result["about_to_quit"] = True
    result["foundation_service_stopped"] = dll.FOAM3EditorStatus() == -1
    result["shutdown_seconds"] = time.monotonic() - exit_started if exit_started else None
    result["status"] = "PASSED" if (result.get("workflow_passed") and result["foundation_service_stopped"]
        and result["shutdown_seconds"] is not None and result["shutdown_seconds"] <= 6) else "FAILED"
    save()

def poll():
    try:
        state = dll.FOAM3EditorStatus()
        assert state >= 0, f"Framework terminal failure: {state}"
        assert time.monotonic() - started < 90, "Framework workflow deadline exceeded"
        ready = state == 1 if mode == "success" else dll.FOAM3EditorNativeStage() == 2
        if not ready:
            QtCore.QTimer.singleShot(20, poll)
            return
        if mode == "success":
            result["artifact_count"] = dll.FOAM3EditorArtifactCount()
            assert result["artifact_count"] == 1, "Verified output custody was not retained exactly once"
        else:
            result["active_context_change_rejected"] = dll.FOAM3EditorContextVeto() == 1
            assert result["active_context_change_rejected"], "Active execution moved into another workspace"
        windows = [w for w in app.topLevelWidgets() if isinstance(w, QtWidgets.QMainWindow)]
        assert windows, "No running Editor main window"
        result["screenshot_saved"] = windows[0].grab().save(str(output / "editor-lifecycle.png"))
        result["workflow_passed"] = True
        quit_editor()
    except Exception as error:
        result["status"] = "FAILED"
        result["error"] = str(error)
        quit_editor()

def begin():
    try:
        code = dll.FOAM3EditorStart(str(fixture_root).encode("utf-8"), os.environ["FOA_M2_FIXTURE"].encode("utf-8"), mode.encode("ascii"))
        assert code == 0, f"Foundation execution setup failed: {code}"
        result["started"] = True
        poll()
    except Exception as error:
        result["status"] = "FAILED"
        result["error"] = str(error)
        quit_editor()

app.aboutToQuit.connect(quitting)
app._m3_lifecycle_fixture = (dll, begin, poll, quitting)
save()
QtCore.QTimer.singleShot(1500, begin)
