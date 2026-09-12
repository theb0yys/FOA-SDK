# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""M5 acceptance in a disposable Editor project. Private paths arrive through environment.
Modes: success, cancel-on-quit, crash after deployment, and fresh-confirmation recovery.
"""
import ctypes
import json
import os
from pathlib import Path
import time
from PySide6 import QtCore, QtWidgets
import azlmbr.legacy.general as general



output = Path(os.environ["FOA_M5_EDITOR_OUTPUT"])
output.mkdir(parents=True, exist_ok=True)
root = Path(os.environ["FOA_M5_EDITOR_TARGET"])
root.mkdir(parents=True, exist_ok=True)
mode = os.environ.get("FOA_M5_EDITOR_MODE", "success")
assert mode in ("success", "cancel", "crash", "recover")
dll = ctypes.CDLL(os.environ["FOA_M3_TEST_DLL"])
dll.FOAM5EditorStart.argtypes = [ctypes.c_char_p,ctypes.c_char_p,ctypes.c_char_p]
dll.FOAM5EditorStart.restype = ctypes.c_int
for name in ("FOAM3EditorStatus", "FOAM3EditorArtifactCount", "FOAM3EditorNativeStage", "FOAM3EditorContextVeto", "FOAM5EditorTargetState"):
    getattr(dll,name).restype = ctypes.c_int
app = QtWidgets.QApplication.instance()
result = {"status":"RUNNING", "mode":mode, "about_to_quit":False,
          "evidence_lane":"Editor service lifecycle", "manual_visual_acceptance":"NOT_RUN"}
started = time.monotonic()
exit_started = None

def save():
    (output / "result.json").write_text(json.dumps(result,indent=2),encoding="utf-8")

def quit_editor():
    global exit_started
    exit_started = time.monotonic()
    save()
    general.exit()

def quitting():
    result["about_to_quit"] = True
    result["service_stopped"] = dll.FOAM3EditorStatus() == -1
    result["target_restored"] = dll.FOAM5EditorTargetState() == 1
    result["shutdown_seconds"] = time.monotonic()-exit_started if exit_started else None
    result["status"] = "PASSED" if (result.get("workflow_passed") and result["service_stopped"] and result["target_restored"]
        and result["shutdown_seconds"] is not None and result["shutdown_seconds"] < 8) else "FAILED"
    save()

def poll():
    try:
        state = dll.FOAM3EditorStatus()
        assert state >= 0, f"Framework failed: {state}"
        assert time.monotonic()-started < 90, "Execution deadline exceeded"
        ready = state == 1 if mode == "success" else dll.FOAM5EditorTargetState() == 2 and dll.FOAM3EditorNativeStage() == 2
        if not ready:
            QtCore.QTimer.singleShot(20,poll)
            return
        if mode == "success":
            result["artifact_count"] = dll.FOAM3EditorArtifactCount()
            assert result["artifact_count"] == 6
            assert dll.FOAM5EditorTargetState() == 1
        else:
            result["active_context_change_rejected"] = dll.FOAM3EditorContextVeto() == 1
            assert result["active_context_change_rejected"]
        result["workflow_passed"] = True
        if mode == "crash":
            result["status"] = "INTERRUPTED_AFTER_DEPLOY"
            save()
            # Deliberate hard interruption of this disposable test process; no destructor cleanup.
            os._exit(86)
        quit_editor()
    except Exception as error:
        result["error"] = str(error)
        quit_editor()

def begin():
    try:
        save()
        selected = "hang" if mode in ("crash","cancel") else mode
        code = dll.FOAM5EditorStart(str(root).encode(),os.environ["FOA_M5_PROVIDER"].encode(),selected.encode())
        assert code == 0, f"M5 Foundation setup failed: {code}"
        if mode == "recover":
            assert dll.FOAM5EditorTargetState() == 1
            result["workflow_passed"] = True
            result["fresh_confirmation_recovery"] = True
            quit_editor()
        else:
            poll()
    except Exception as error:
        result["error"] = str(error)
        quit_editor()

app.aboutToQuit.connect(quitting)
QtCore.QTimer.singleShot(1000,begin)
