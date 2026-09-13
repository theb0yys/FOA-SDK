#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Run via Editor --runpython. Uses a synthetic authenticated listener, never FoA.

Set FOA_CONNECTION_EVIDENCE to an external output directory. This dedicated
test Editor exits after writing editor-connection.json and pane screenshots.
"""

import hashlib
import hmac
import json
import os
from pathlib import Path
import secrets
import socketserver
import struct
import sys
import threading
import time
import traceback

from PySide6 import QtCore, QtWidgets
import azlmbr.legacy.general as general

TOOLS_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS_ROOT))
from tge_sdk_client import decode_request
from tge_sdk_transport import HELLO, REQUEST_DOMAIN, RESPONSE_DOMAIN

PANE = "Tainted Grail Connect to Game"


class Listener(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = False

    def __init__(self):
        super().__init__(("127.0.0.1", 0), Handler)
        self.key = secrets.token_bytes(32)
        self.session = "synthetic-editor-session-1"
        self.release = threading.Event()
        self.stall = False
        self.calls = []
        self.player_available = False
        self.player_x = 1.25
        self.has_player_service = True
        self.supports_vitals = True
        self.has_encounter_service = True
        self.preview_rejected = False
        self.plan_number = 0
        self.vitals_unavailable = False
        self.vitals = {"health": "75.25", "healthMax": "100.5", "stamina": "50.5",
                       "staminaMax": "80.75", "mana": "10.75", "manaMax": "40.25"}


class Handler(socketserver.BaseRequestHandler):
    def read(self, length):
        result = bytearray()
        while len(result) < length:
            part = self.request.recv(length - len(result))
            if not part:
                raise ConnectionError()
            result.extend(part)
        return bytes(result)

    def handle(self):
        try:
            self.request.settimeout(5)
            nonce = secrets.token_bytes(32)
            self.request.sendall(HELLO + nonce)
            prefix = self.read(36)
            length = struct.unpack("!I", prefix[32:])[0]
            if not 1 <= length <= 65536:
                return
            body = self.read(length)
            tag = self.read(32)
            if not hmac.compare_digest(tag, hmac.digest(self.server.key, REQUEST_DOMAIN + nonce + prefix + body, hashlib.sha256)):
                return
            request = decode_request(body.decode("utf-8"))
            self.server.calls.append((request.service_id, request.operation))
            if self.server.stall:
                self.server.release.wait(8)
            position_request = request.service_id == "tge.foa.player" and request.operation == "position"
            vitals_request = request.service_id == "tge.foa.player" and request.operation == "vitals"
            preview_request = request.service_id == "tge.foa.encounters" and request.operation == "preview"
            if not position_request and not vitals_request and not preview_request and (request.service_id != "tge.core.identity" or request.operation not in ("describe", "services")):
                return
            status, code = "succeeded", "identity"
            if preview_request:
                assert set(request.arguments) == {"templates"} and request.service_version == "0.1"
                if self.server.preview_rejected:
                    status, code, values = "rejected", "single_actor_check_required", {}
                else:
                    self.server.plan_number += 1
                    code = "encounter_plan"
                    values = {"planId": f"{self.server.plan_number:032x}", "fingerprint": "b" * 64,
                              "templates": request.arguments["templates"], "placement": "fixture|12,0,0", "expiresInSeconds": "30"}
            elif vitals_request:
                assert not request.arguments and request.service_version == "0.1"
                if not self.server.supports_vitals:
                    status, code, values = "rejected", "unknown_operation", {}
                elif not self.server.player_available:
                    status, code, values = "rejected", "player_unavailable", {}
                elif self.server.vitals_unavailable:
                    status, code, values = "rejected", "vitals_unavailable", {}
                else:
                    code, values = "player_vitals", self.server.vitals
            elif position_request:
                assert not request.arguments and request.service_version == "0.1"
                if self.server.player_available:
                    code = "player_position"
                    values = {"sceneName": "<b>synthetic-scene</b>", "x": str(self.server.player_x), "y": "-2.5", "z": "3.75"}
                else:
                    status, code, values = "rejected", "player_unavailable", {}
            elif request.operation == "describe":
                values = {"hostId": "kane.tgfoa.tainted-grail-extender", "hostVersion": "0.1.0", "serviceCount": "17"}
            else:
                offset = int(request.arguments["offset"])
                end = min(offset + 16, 17)
                values = {"total": "17", "nextOffset": str(end) if end < 17 else ""}
                for index in range(end - offset):
                    player = offset + index == 1 and self.server.has_player_service
                    encounter = offset + index == 2 and self.server.has_encounter_service
                    values.update({f"{index}.id": "tge.core.identity" if offset + index == 0 else "tge.foa.player" if player else "tge.foa.encounters" if encounter else f"fixture.service{offset + index}",
                                   f"{index}.version": "0.1", f"{index}.owner": "tge.foa.player" if player else "tge.foa.encounters" if encounter else "fixture"})
            result = json.dumps({"contract": "foa-sdk-tge-response/1", "requestId": request.request_id,
                "correlationId": request.correlation_id, "sessionId": self.server.session,
                "serviceId": request.service_id, "serviceVersion": request.service_version,
                "status": status, "code": code, "message": "", "values": values}).encode()
            size = struct.pack("!I", len(result))
            tag = hmac.digest(self.server.key, RESPONSE_DOMAIN + nonce + prefix[:32] + size + result, hashlib.sha256)
            self.request.sendall(size + result + tag)
        except (OSError, ValueError):
            pass


def run():
    output = Path(os.environ["FOA_CONNECTION_EVIDENCE"]).resolve()
    if output.is_relative_to(TOOLS_ROOT.parents[2]):
        raise RuntimeError("Editor evidence must remain outside the source checkout")
    output.mkdir(parents=True, exist_ok=True)
    result = {"evidenceClass": "Editor UI with synthetic listener", "runtimeSignOff": "NOT_RUN", "checks": []}
    server = Listener()
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    app = QtWidgets.QApplication.instance()
    maximum_gap = 0

    def wait(predicate, timeout=7):
        nonlocal maximum_gap
        deadline = time.monotonic() + timeout
        while not predicate():
            assert time.monotonic() < deadline, "Editor control did not reach the expected state"
            before = time.monotonic()
            app.processEvents()
            time.sleep(0.01)
            maximum_gap = max(maximum_gap, time.monotonic() - before)

    def pane():
        return next((widget for widget in app.allWidgets() if widget.objectName() == "TgeGameConnectionPane"), None)

    try:
        general.open_pane("FOA Development Hub")
        wait(lambda: any(button.text() == "Connect to Game" for button in app.allWidgets() if isinstance(button, QtWidgets.QPushButton)))
        next(button for button in app.allWidgets() if isinstance(button, QtWidgets.QPushButton) and button.text() == "Connect to Game").click()
        wait(lambda: pane() is not None)
        widget = pane()
        dock = widget.parentWidget()
        while dock is not None and not isinstance(dock, QtWidgets.QDockWidget):
            dock = dock.parentWidget()
        if dock is not None:
            dock.setFloating(True)
            screen = dock.screen().availableGeometry()
            dock.resize(min(840, screen.width() - 40), min(1040, screen.height() - 40))
            dock.move(screen.topLeft() + QtCore.QPoint(20, 20))
            dock.show()
            app.processEvents()
        control = lambda cls, name: widget.findChild(cls, name)
        port = control(QtWidgets.QSpinBox, "TgeConnectionPort")
        key = control(QtWidgets.QLineEdit, "TgeConnectionKey")
        connect = control(QtWidgets.QPushButton, "TgeConnect")
        refresh = control(QtWidgets.QPushButton, "TgeRefresh")
        disconnect = control(QtWidgets.QPushButton, "TgeDisconnect")
        status = control(QtWidgets.QLabel, "TgeConnectionStatus")
        rows = control(QtWidgets.QTableWidget, "TgeConnectionServices")
        get_position = control(QtWidgets.QPushButton, "TgeGetPlayerPosition")
        position = control(QtWidgets.QLabel, "TgePlayerPosition")
        get_vitals = control(QtWidgets.QPushButton, "TgeGetPlayerVitals")
        vitals = control(QtWidgets.QLabel, "TgePlayerVitals")
        composition = control(QtWidgets.QLineEdit, "TgeEncounterComposition")
        browse = control(QtWidgets.QPushButton, "TgeBrowseComposition")
        preview = control(QtWidgets.QPushButton, "TgePreviewEncounter")
        encounter = control(QtWidgets.QLabel, "TgeEncounterPreview")
        assert browse.isEnabled() and composition.accessibleName() and not preview.isEnabled()
        assert len(widget.findChildren(QtWidgets.QPushButton, "TgeGetPlayerVitals")) == 1
        assert len(widget.findChildren(QtWidgets.QLabel, "TgePlayerVitals")) == 1
        assert not get_position.isEnabled() and not get_vitals.isEnabled()
        assert status.text() == "Disconnected" and rows.rowCount() == 0 and not refresh.isEnabled()
        assert key.echoMode() == QtWidgets.QLineEdit.Password
        result["checks"].append("Home route, disconnected controls and masked key")
        port.setValue(server.server_address[1])
        key.setText(server.key.hex())
        connect.click()
        wait(lambda: refresh.isEnabled())
        assert key.text() == "" and rows.rowCount() == 17
        assert rows.item(0, 0).text() == "tge.core.identity"
        app.processEvents()
        widget.layout().activate()
        details = control(QtWidgets.QLabel, "TgeConnectionDetails")
        assert widget.grab().save(str(output / "connected.png"))
        result["detailsLayout"] = {"height": details.height(), "hint": details.sizeHint().height(),
                                   "lineSpacing": details.fontMetrics().lineSpacing(), "text": details.text()}
        assert details.height() >= details.sizeHint().height(), "Connection details are clipped"
        result["checks"].append("Authenticated worker and complete two-page discovery")
        assert get_position.isEnabled()
        get_position.click()
        wait(lambda: get_position.isEnabled())
        assert "No player is loaded" in position.text() and "X:" not in position.text()
        result["checks"].append("No loaded player is explicit and has no coordinates")
        get_vitals.click()
        wait(lambda: get_vitals.isEnabled())
        assert "No player is loaded" in vitals.text() and "Health:" not in vitals.text()
        result["checks"].append("No loaded player has no invented vitals")
        server.player_available = True
        get_position.click()
        assert "Reading player position" in position.text() and not refresh.isEnabled()
        wait(lambda: get_position.isEnabled())
        assert "X: 1.25" in position.text() and "Y: -2.5" in position.text() and "Z: 3.75" in position.text()
        assert "<b>synthetic-scene</b>" in position.text() and position.textFormat() == QtCore.Qt.PlainText
        assert get_position.text() == "Refresh Position"
        app.processEvents()
        assert position.height() >= position.sizeHint().height(), "Player coordinates are clipped"
        assert widget.grab().save(str(output / "player-position.png"))
        result["checks"].append("Coordinates, plain-text scene, timestamp and readable layout")
        get_vitals.click()
        assert "Reading player vitals" in vitals.text() and not get_position.isEnabled()
        wait(lambda: get_vitals.isEnabled())
        assert "Health: 75.25 / 100.5" in vitals.text() and "Stamina: 50.5 / 80.75" in vitals.text()
        assert "Mana: 10.75 / 40.25" in vitals.text() and "Last read:" in vitals.text()
        assert get_vitals.text() == "Refresh Vitals" and "X: 1.25" in position.text()
        app.processEvents()
        assert vitals.height() >= vitals.sizeHint().height(), "Player vitals are clipped"
        widget.findChild(QtWidgets.QScrollArea).ensureWidgetVisible(vitals)
        app.processEvents()
        assert widget.grab().save(str(output / "player-vitals.png"))
        result["checks"].append("Current/maximum vitals, timestamp and readable layout")
        composition_path = output / "synthetic composition.json"
        composition_data = {"contract": "foa-tge-encounter-composition/1", "name": "<b>Synthetic patrol</b>",
                            "entries": [{"template": "wyrdspirit", "count": 1}], "source": None}
        composition_path.write_text(json.dumps(composition_data), encoding="utf-8")
        composition.setText(str(composition_path))
        assert preview.isEnabled()
        # Exercise the actual Browse dialog; selecting the test file needs no
        # game connection and preserves the user's other Editor instances.
        picker_cancelled = []
        picker_deadline = time.monotonic() + 8
        picker_timer = QtCore.QTimer(widget)
        picker_timer.setInterval(50)
        def cancel_picker():
            # C++-created dialogs may have a generic QWidget Python wrapper.
            # Query the Qt metaobject and invoke its real slot instead.
            dialog = next((candidate for candidate in app.allWidgets() if candidate.objectName() == "TgeCompositionDialog"), None)
            if dialog is not None:
                picker_timer.stop()
                picker_cancelled.append(True)
                QtCore.QMetaObject.invokeMethod(dialog, "reject", QtCore.Qt.QueuedConnection)
            elif time.monotonic() >= picker_deadline:
                picker_timer.stop()
                result["pickerFailure"] = "No QFileDialog metaobject found"
                result["topLevelClasses"] = [candidate.metaObject().className() for candidate in app.topLevelWidgets()]
                (output / "editor-picker-failure.json").write_text(json.dumps(result))
                general.exit_no_prompt()
        picker_timer.timeout.connect(cancel_picker)
        result["pickerButton"] = {"enabled": browse.isEnabled(), "visible": browse.isVisible(),
                                  "blocked": browse.signalsBlocked(), "receivers": browse.receivers(QtCore.SIGNAL("clicked(bool)"))}
        assert browse.isEnabled(), "Composition browse button unexpectedly disabled"
        widget.findChild(QtWidgets.QScrollArea).ensureWidgetVisible(browse)
        app.processEvents()
        picker_timer.start()
        browse.click()
        wait(lambda: bool(picker_cancelled), timeout=9)
        assert picker_cancelled, "Composition picker did not open"
        assert composition.text() == str(composition_path)
        result["checks"].append("Composition picker cancellation preserves the selected file")
        before_file = composition_path.read_bytes()
        preview.click()
        assert "Previewing" in encounter.text() and not browse.isEnabled() and not get_vitals.isEnabled()
        wait(lambda: preview.isEnabled())
        assert "Preview accepted" in encounter.text() and "Actors (1): wyrdspirit" in encounter.text()
        assert "<b>Synthetic patrol</b>" in encounter.text() and encounter.textFormat() == QtCore.Qt.PlainText
        assert hashlib.sha256(before_file).hexdigest() in encounter.text()
        assert composition_path.read_bytes() == before_file and "Health: 75.25" in vitals.text()
        app.processEvents()
        widget.findChild(QtWidgets.QScrollArea).ensureWidgetVisible(encounter)
        app.processEvents()
        assert encounter.height() >= encounter.sizeHint().height(), "Encounter preview is clipped"
        assert encounter.grab().save(str(output / "encounter-preview.png"))
        result["checks"].append("Preview displays composition, actors, placement, digest and time without spawning")
        # A second preview reads the selected file again, rather than using old data.
        composition_data["name"] = "Changed synthetic patrol"
        composition_data["entries"] = [{"template": "outlaw-1h", "count": 2}]
        composition_path.write_text(json.dumps(composition_data), encoding="utf-8")
        preview.click()
        wait(lambda: preview.isEnabled())
        assert "Actors (2): outlaw-1h, outlaw-1h" in encounter.text() and "Changed synthetic patrol" in encounter.text()
        result["checks"].append("Preview reads fresh composition contents and counts")
        wait(lambda: "Preview expired" in encounter.text(), timeout=32)
        result["checks"].append("Temporary preview expires visibly without network polling")
        composition.setText(str(output / "missing composition.json"))
        assert "Changed synthetic patrol" not in encounter.text()
        calls_before = len(server.calls)
        preview.click()
        wait(lambda: preview.isEnabled())
        assert "valid encounter composition" in encounter.text() and refresh.isEnabled()
        assert len(server.calls) == calls_before and "Health:" in vitals.text()
        result["checks"].append("Changing selection clears the plan; bad input preserves connection without network calls")
        composition.setText(str(composition_path))
        server.has_encounter_service = False
        refresh.click()
        wait(lambda: refresh.isEnabled())
        assert not preview.isEnabled() and "does not provide" in encounter.text()
        server.has_encounter_service = True
        refresh.click()
        wait(lambda: preview.isEnabled())
        result["checks"].append("Missing encounter service disables preview")
        server.preview_rejected = True
        preview.click()
        wait(lambda: connect.isEnabled())
        assert "preview was rejected" in status.text() and "Actors (" not in encounter.text()
        server.preview_rejected = False
        key.setText(server.key.hex())
        connect.click()
        wait(lambda: preview.isEnabled())
        result["checks"].append("Rejected preview clears plan and requires reconnect")
        server.vitals.update(health="60", stamina="12", mana="3")
        get_vitals.click()
        assert "Health:" not in vitals.text()
        wait(lambda: get_vitals.isEnabled())
        assert "Health: 60 / 100.5" in vitals.text() and "Stamina: 12 / 80.75" in vitals.text()
        assert "Mana: 3 / 40.25" in vitals.text()
        result["checks"].append("Refresh Vitals replaces old stats and preserves maxima")
        server.vitals.update(health="120.25", stamina="-1", manaMax="0")
        get_vitals.click()
        wait(lambda: get_vitals.isEnabled())
        assert "Health: 120.25 / 100.5" in vitals.text() and "Stamina: -1 / 80.75" in vitals.text()
        assert "Mana: 3 / 0" in vitals.text()
        result["checks"].append("Native negative, zero and above-maximum values are not clamped")
        retained_vitals = vitals.text()
        server.player_x = 10.5
        get_position.click()
        assert "X:" not in position.text()
        wait(lambda: get_position.isEnabled())
        assert "X: 10.5" in position.text()
        assert vitals.text() == retained_vitals
        result["checks"].append("Fresh movement snapshot replaces the previous coordinates")
        server.vitals_unavailable = True
        get_vitals.click()
        wait(lambda: get_vitals.isEnabled())
        assert "vitals are unavailable" in vitals.text() and "Health:" not in vitals.text()
        assert "X: 10.5" in position.text()
        result["checks"].append("Unavailable stats clear vitals and retain the separate position observation")
        server.vitals_unavailable = False
        server.player_available = False
        get_position.click()
        wait(lambda: get_position.isEnabled())
        assert "No player is loaded" in position.text() and "X:" not in position.text()
        get_vitals.click()
        wait(lambda: get_vitals.isEnabled())
        assert "No player is loaded" in vitals.text() and "Health:" not in vitals.text()
        result["checks"].append("Returning to menu removes previous coordinates")
        server.has_player_service = False
        refresh.click()
        wait(lambda: refresh.isEnabled())
        assert not get_position.isEnabled() and not get_vitals.isEnabled() and "does not provide" in position.text()
        server.has_player_service = True
        refresh.click()
        wait(lambda: refresh.isEnabled())
        result["checks"].append("Same-session refresh")
        result["checks"].append("Missing player service disables the query")
        server.session = "synthetic-editor-session-2"
        get_position.click()
        wait(lambda: connect.isEnabled())
        assert "restarted" in status.text() and rows.rowCount() == 0
        assert "X:" not in position.text() and not get_position.isEnabled()
        result["checks"].append("Restart requires explicit reconnect and clears services")
        key.setText(server.key.hex())
        connect.click()
        wait(lambda: refresh.isEnabled())
        disconnect.click()
        assert status.text() == "Disconnected" and rows.rowCount() == 0
        key.setText(secrets.token_hex(32))
        connect.click()
        wait(lambda: connect.isEnabled())
        assert rows.rowCount() == 0 and not refresh.isEnabled()
        assert widget.grab().save(str(output / "connection-error.png"))
        result["checks"].append("Disconnect and authentication failure")
        server.supports_vitals = False
        key.setText(server.key.hex())
        connect.click()
        wait(lambda: get_vitals.isEnabled())
        get_vitals.click()
        wait(lambda: connect.isEnabled())
        assert "may not support vitals" in status.text() and rows.rowCount() == 0
        assert "Health:" not in vitals.text() and "X:" not in position.text()
        result["checks"].append("Older host operation rejection clears the connection and observations")
        server.supports_vitals = True
        server.stall = True
        key.setText(server.key.hex())
        connect.click()
        wait(lambda: disconnect.text() == "Cancel")
        before = time.monotonic()
        disconnect.click()
        assert time.monotonic() - before < 0.25
        assert status.text() == "Disconnected"
        result["checks"].append("In-flight cancellation stays responsive")
        server.stall = False
        key.setText(server.key.hex())
        connect.click()
        wait(lambda: get_position.isEnabled())
        server.stall = True
        get_position.click()
        assert not get_position.isEnabled() and disconnect.text() == "Cancel"
        disconnect.click()
        assert "X:" not in position.text() and status.text() == "Disconnected"
        result["checks"].append("Position cancellation clears the observation")
        server.stall = False
        key.setText(server.key.hex())
        connect.click()
        wait(lambda: get_vitals.isEnabled())
        server.stall = True
        get_vitals.click()
        assert not get_position.isEnabled() and disconnect.text() == "Cancel"
        disconnect.click()
        assert "Health:" not in vitals.text() and status.text() == "Disconnected"
        result["checks"].append("Vitals cancellation clears the observation")
        server.stall = False
        key.setText(server.key.hex())
        connect.click()
        wait(lambda: preview.isEnabled())
        server.stall = True
        preview.click()
        assert not preview.isEnabled() and disconnect.text() == "Cancel"
        disconnect.click()
        assert "Actors (" not in encounter.text() and status.text() == "Disconnected"
        result["checks"].append("Preview cancellation clears the observation")
        server.stall = False
        key.setText(server.key.hex())
        connect.click()
        wait(lambda: get_position.isEnabled())
        server.stall = True
        preview.click()
        general.close_pane(PANE)
        wait(lambda: pane() is None or not pane().isVisible())
        server.release.set()
        general.open_pane(PANE)
        wait(lambda: pane() is not None and pane().isVisible())
        assert pane().findChild(QtWidgets.QLabel, "TgeConnectionStatus").text() == "Disconnected"
        assert pane().findChild(QtWidgets.QLineEdit, "TgeConnectionKey").text() == ""
        result["checks"].append("Close during request, reopen with no retained key or session")
        assert not pane().findChild(QtWidgets.QPushButton, "TgeGetPlayerPosition").isEnabled()
        assert not pane().findChild(QtWidgets.QPushButton, "TgeGetPlayerVitals").isEnabled()
        assert not pane().findChild(QtWidgets.QPushButton, "TgePreviewEncounter").isEnabled()
        assert all((service == "tge.core.identity" and operation in ("describe", "services"))
                   or (service == "tge.foa.player" and operation in ("position", "vitals"))
                   or (service == "tge.foa.encounters" and operation == "preview") for service, operation in server.calls)
        assert maximum_gap < 0.5, "UI timer gap exceeded 500 ms"
        result["maximumUiGapMs"] = round(maximum_gap * 1000, 3)
        result["status"] = "PASSED"
    except Exception:
        result["status"] = "FAILED"
        result["failure"] = traceback.format_exc()
    finally:
        server.release.set()
        server.shutdown()
        server.server_close()
        (output / "editor-connection.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        general.exit_no_prompt()


QtCore.QTimer.singleShot(3000, run)
