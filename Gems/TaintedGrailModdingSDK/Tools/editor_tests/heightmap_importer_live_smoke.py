# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT

"""Exercise the real terrain pane using an isolated synthetic workspace.

The launcher must isolate LOCALAPPDATA and Editor user/cache paths. FOA_HEIGHTMAP
_TEST_ROOT must contain synthetic Inputs only. No installed game is used as input.
The workspace must have its required output directories and a single synthetic mod
under Packs/<id>/pack.tgpack.json so Home can enable authoring.
Run mode import, then mode reopen in a second Editor process using the same fixture.
"""
import json
import os
import time
import traceback
from pathlib import Path

import azlmbr.legacy.general as general
from PySide6 import QtCore, QtGui, QtWidgets


def run():
    root = Path(os.environ["FOA_HEIGHTMAP_TEST_ROOT"]).resolve()
    mode = os.environ.get("FOA_HEIGHTMAP_TEST_MODE", "import")
    output = root / ("editor-" + mode + ".json")
    result = {"status": "FAILED", "mode": mode, "checks": []}
    app = QtWidgets.QApplication.instance()
    workspace = (Path(os.environ["LOCALAPPDATA"]) / "FOA-SDK/Workspace").resolve()
    # Embedded Python inherits Editor.exe long-path settings; Qt accepts these revision paths.
    if os.name == "nt":
        workspace = Path("\\\\?\\" + str(workspace))

    def record(stage):
        result["stage"] = stage
        output.write_text(json.dumps(result, indent=2), encoding="utf-8")

    def wait_idle(pane):
        button = pane.findChild(QtWidgets.QPushButton, "TerrainImportCancel")
        deadline = time.monotonic() + 30
        while button.isEnabled() and time.monotonic() < deadline:
            general.idle_wait(.1)
        assert not button.isEnabled(), "Terrain operation exceeded 30 seconds"
        return pane.findChild(QtWidgets.QLabel, "TerrainImportStatus").text()

    def drop(pane, filename):
        mime = QtCore.QMimeData()
        mime.setUrls([QtCore.QUrl.fromLocalFile(str(root / "Inputs" / filename))])
        enter = QtGui.QDragEnterEvent(QtCore.QPoint(100, 100), QtCore.Qt.CopyAction,
                                     mime, QtCore.Qt.LeftButton, QtCore.Qt.NoModifier)
        QtWidgets.QApplication.sendEvent(pane, enter)
        assert enter.isAccepted(), "Pane rejected a local heightmap drop"
        event = QtGui.QDropEvent(QtCore.QPointF(100, 100), QtCore.Qt.CopyAction,
                                mime, QtCore.Qt.LeftButton, QtCore.Qt.NoModifier)
        QtWidgets.QApplication.sendEvent(pane, event)
        assert event.isAccepted(), "Local heightmap was not submitted"

    try:
        record("starting")
        general.idle_enable(True)
        general.open_pane("FOA Development Hub")
        general.idle_wait(1.0)
        route = next(button for button in app.allWidgets() if isinstance(button, QtWidgets.QPushButton)
                     and button.text() == "Map editor")
        deadline = time.monotonic() + 10
        while not route.isEnabled() and time.monotonic() < deadline:
            general.idle_wait(.1)
        if not route.isEnabled():
            result["home_labels"] = []
            for widget in app.allWidgets():
                try:
                    if isinstance(widget, QtWidgets.QLabel):
                        result["home_labels"].append(widget.text())
                except RuntimeError:
                    pass
            route.window().grab().save(str(root / "home-unavailable.png"))
        assert route.isEnabled(), "Home heightmap route is unavailable"
        route.click()
        general.idle_wait(1.0)
        result["checks"].append("home-route")
        pane = next(widget for widget in app.allWidgets() if widget.objectName() == "FoaHeightmapImporter")
        record("pane-open")
        wait_idle(pane)
        assert pane.findChild(QtWidgets.QPushButton, "TerrainImportNewMap").isEnabled(), "Local importer must be ready"
        assert not pane.findChild(QtWidgets.QPushButton, "TerrainEditVanillaMap").isEnabled(), "Unqualified vanilla provider must remain unavailable"
        result["checks"].append("pane-and-route-availability")
        if mode == "import":
            drop(pane, "missing-metadata.raw")
            assert "metadata" in wait_idle(pane).lower(), "Missing metadata must be actionable"
            result["checks"].append("missing-metadata-error")
            drop(pane, "cancel.raw")
            pane.findChild(QtWidgets.QPushButton, "TerrainImportCancel").click()
            assert "cancelled" in wait_idle(pane).lower(), "Cancel did not stop import"
            assert not list(workspace.glob("Derived/Terrain/**/terrain.tgheightmap.json")), "Cancel left a published revision"
            result["checks"].append("cancel-without-publication")
            for filename in ("Synthetic-Hills.raw", "Synthetic-Ridge.png", "Large-Terrain.raw"):
                record("importing-" + filename)
                ticks = []
                heartbeat = QtCore.QTimer(pane)
                heartbeat.setInterval(10)
                heartbeat.timeout.connect(lambda: ticks.append(time.monotonic()))
                heartbeat.start()
                start = time.monotonic()
                drop(pane, filename)
                assert "imported and saved" in wait_idle(pane).lower(), "Import did not complete"
                result.setdefault("import_seconds", {})[filename] = round(time.monotonic() - start, 3)
                heartbeat.stop()
                assert ticks, "The UI event loop stopped during terrain import"
                result.setdefault("ui_heartbeat_ticks", {})[filename] = len(ticks)
                if filename == "Large-Terrain.raw":
                    large = next(json.loads(path.read_text()) for path in workspace.glob("Derived/Terrain/**/terrain.tgheightmap.json")
                                 if json.loads(path.read_text())["map_identity"]["display_name"] == "Large-Terrain")
                    assert large["grid"]["width"] == 4097 and len(large["tiles"]) == 25
                    result["checks"].append("4097-square-25-tiles-ui-responsive")
                preview = pane.findChild(QtWidgets.QLabel, "TerrainHeightPreview").pixmap()
                assert preview and not preview.isNull(), "Imported terrain has no visible preview"
            result["checks"].extend(["raw-import", "png-import", "visible-height-preview"])
        recent = pane.findChild(QtWidgets.QListWidget, "TerrainSavedRevisions")
        assert recent.count() == 3, "All saved terrain revisions must be listed"
        recent.setCurrentRow(next(index for index in range(recent.count()) if recent.item(index).text().startswith("Synthetic-Hills")))
        pane.findChild(QtWidgets.QPushButton, "TerrainPreviewRevision").click()
        assert "opened" in wait_idle(pane).lower(), "Saved terrain did not reopen"
        preview = pane.findChild(QtWidgets.QLabel, "TerrainHeightPreview").pixmap()
        assert preview and not preview.isNull(), "Reopened terrain has no visible preview"
        result["checks"].append("saved-revision-reopened")
        assert pane.grab().save(str(root / ("pane-" + mode + ".png"))), "Unable to capture the pane"
        if mode == "reopen":
            # Damage only the disposable synthetic fixture and prove the UI reports it.
            manifests = list(workspace.glob("Derived/Terrain/**/terrain.tgheightmap.json"))
            chosen = next(path for path in manifests if json.loads(path.read_text())["revision"]["revision_id"]
                          == recent.currentItem().data(QtCore.Qt.UserRole))
            document = json.loads(chosen.read_text())
            tile = (chosen.parent / document["tiles"][0]["relative_path"]).resolve()
            assert tile.is_relative_to(workspace.resolve()), "Fixture tile escaped its disposable workspace"
            original = tile.read_bytes()
            try:
                tile.write_bytes(bytes(len(original)))
                pane.findChild(QtWidgets.QPushButton, "TerrainPreviewRevision").click()
                assert "fingerprint" in wait_idle(pane).lower(), "Damaged terrain must not open"
                damaged_preview = pane.findChild(QtWidgets.QLabel, "TerrainHeightPreview").pixmap()
                assert not damaged_preview or damaged_preview.isNull(), "Damaged terrain retained a stale preview"
            finally:
                tile.write_bytes(original)
            result["checks"].append("damaged-tile-rejected")
        general.close_pane("Heightmap Importer")
        general.idle_wait(.1)
        result["checks"].append("pane-closed")
        result["status"] = "PASSED"
    except Exception as error:
        result["error"] = str(error)
        result["traceback"] = traceback.format_exc()
        if "pane" in locals():
            result["labels"] = [label.text() for label in pane.findChildren(QtWidgets.QLabel)]
    finally:
        record("finished")
        general.exit_no_prompt()


run()
