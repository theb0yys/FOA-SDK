# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Exercise game discovery, the real folder picker, and persisted registration.

The caller must isolate LOCALAPPDATA and the Editor user/log directories. Game
inputs are read-only; no runtime or deployment proof is produced by this check.
Run once with mode auto, once with manual and a different seeded installation,
then again with mode reopen and the same workspace to verify durable selection.
"""

import json
import os
from pathlib import Path

import azlmbr.legacy.general as general
from PySide6 import QtCore, QtWidgets


def same_path(left, right):
    return os.path.normcase(os.path.realpath(left)) == os.path.normcase(os.path.realpath(right))


def run():
    output = Path(os.environ["FOA_SDK_GAME_LOCATION_RESULT"])
    expected = os.environ["FOA_SDK_GAME_LOCATION_EXPECTED"]
    mode = os.environ["FOA_SDK_GAME_LOCATION_MODE"]
    workspace_path = Path(os.environ["LOCALAPPDATA"]) / "FOA-SDK/Workspace/foa-sdk.tgworkspace.json"
    result = {"mode": mode, "status": "FAILED", "checks": []}
    output.write_text(json.dumps({**result, "stage": "started"}), encoding="utf-8")
    app = QtWidgets.QApplication.instance()
    try:
        general.idle_enable(True)
        general.open_pane("Tainted Grail SDK Status")
        general.idle_wait(1.0)
        output.write_text(json.dumps({**result, "stage": "pane_opened"}), encoding="utf-8")
        pane = next(widget for widget in app.allWidgets()
                    if isinstance(widget, QtWidgets.QDockWidget)
                    and widget.windowTitle() == "Tainted Grail SDK Status")
        if mode == "manual":
            previous = json.loads(workspace_path.read_text(encoding="utf-8"))
            assert not same_path(previous["GameProfiles"][0]["InstallPath"], expected), "Manual fixture must start at another installation"
            QtCore.QCoreApplication.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
            picker_result = []

            button = next(button for button in pane.findChildren(QtWidgets.QPushButton)
                          if button.text() in {"Locate Fall of Avalon...", "Change game folder..."})
            assert button.isVisible() and button.isEnabled(), "Manual location control must be usable"
            button.click()
            general.idle_wait(1.0)
            # O3DE can wrap the dialog in a window decoration container.
            dialog = next(widget for widget in app.allWidgets()
                          if isinstance(widget, QtWidgets.QFileDialog) and widget.isVisible())
            dialog.setDirectory(expected)
            general.idle_wait(1.0)
            dialog.selectFile(expected)
            general.idle_wait(0.5)
            filename = dialog.findChild(QtWidgets.QLineEdit, "fileNameEdit")
            if filename:
                filename.setText(expected)
            result["picker_selection"] = dialog.selectedFiles()
            dialog.accepted.connect(lambda: picker_result.append("selected"))
            buttons = dialog.findChild(QtWidgets.QDialogButtonBox)
            accept = buttons.button(QtWidgets.QDialogButtonBox.Open)
            assert accept and accept.isEnabled(), "Folder selection must be enabled"
            accept.click()
            general.idle_wait(1.0)
            assert picker_result == ["selected"], picker_result
            result["checks"].append("real_folder_picker_selected")

        general.idle_wait(0.5)
        document = json.loads(workspace_path.read_text(encoding="utf-8"))
        profile = next(profile for profile in document["GameProfiles"]
                       if profile["ProfileId"] == document["ActiveGameProfileId"])
        assert same_path(profile["InstallPath"], expected), "Persisted installation does not match selection"
        assert any(label.text() == "Ready to author" for label in pane.findChildren(QtWidgets.QLabel)), "Editor setup did not become ready"
        result["checks"].extend(["selected_installation_persisted", "editor_ready_to_author"])
        recheck = next(button for button in pane.findChildren(QtWidgets.QPushButton) if button.text() == "Check again")
        recheck.click()
        after = json.loads(workspace_path.read_text(encoding="utf-8"))
        assert after == document, "Rechecking changed the saved selection"
        result["checks"].append("recheck_preserves_registration")
        result["status"] = "PASSED"
    except Exception as error:
        result["error"] = str(error)
        if "pane" in locals():
            result["labels"] = [label.text() for label in pane.findChildren(QtWidgets.QLabel)]
    finally:
        output.write_text(json.dumps(result, indent=2), encoding="utf-8")
        general.exit_no_prompt()


run()
