# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Rejection-only smoke in a disposable Editor process; no game input or pixels.

Launch with isolated project/user/cache paths, FOA_HEIGHTMAP_TEST_ROOT and
FOA_HEIGHTMAP_TOOLS_ROOT pointing to the qualified SDK Tools directory.
This script exits its test Editor after checking the disabled campaign route
and a synthetic legacy handoff. It does not load or edit any map.
"""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import time

import azlmbr.legacy.general as general
from PySide6 import QtCore, QtWidgets

root = Path(os.environ["FOA_HEIGHTMAP_TEST_ROOT"]).resolve()
output = root / "campaign-rejection-editor.json"
result = {"status": "FAILED", "checks": []}
deadline = time.monotonic() + 30

def finish():
    timer.stop()
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    general.exit_no_prompt()

def check():
    try:
        pane = next((w for w in QtWidgets.QApplication.allWidgets()
                     if w.objectName() == "FoaHeightmapImporter"), None)
        if pane is None or pane.findChild(QtWidgets.QPushButton, "TerrainImportCancel").isEnabled():
            assert time.monotonic() < deadline, "Importer did not become idle"
            return
        if not pane.findChild(QtWidgets.QPushButton, "TerrainImportNewMap").isEnabled():
            message = pane.findChild(QtWidgets.QLabel, "TerrainImportStatus").text()
            assert time.monotonic() < deadline, "Local importer did not initialize: " + message
            pane.findChild(QtWidgets.QPushButton, "TerrainRefresh").click()
            return
        button = pane.findChild(QtWidgets.QPushButton, "TerrainEditVanillaMap")
        assert not button.isEnabled(), "Campaign import must be disabled"
        assert "unavailable" in button.text(), "Campaign status must be explicit"
        assert "round-trip" in button.toolTip(), "Unavailable reason must be truthful"
        assert pane.findChild(QtWidgets.QPushButton, "TerrainImportNewMap").isEnabled(), "Local heightmap import regressed"
        result["checks"].append("campaign-disabled-local-import-available")
        source = Path(os.environ["FOA_HEIGHTMAP_TOOLS_ROOT"]).resolve() / "foa_terrain_native_editor.py"
        spec = importlib.util.spec_from_file_location("foa_rejection_test_native", source)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory(prefix="foa-rejection-", dir=root) as directory:
            workspace = Path(directory).resolve()
            staging = workspace / "Staging" / "TerrainNative"
            staging.mkdir(parents=True)
            request_path = staging / "synthetic-legacy.json"
            request = {"workspace": str(workspace), "asset_root": str(workspace / "EditorAssets"),
                       "image": str(workspace / "EditorAssets" / "synthetic.tif"),
                       "level": str(workspace / "EditorAssets" / "Map.prefab"),
                       "document": {"source_binding": {"exporter_id": "importer.campaign-ground-raw"}}}
            request_path.write_text(json.dumps(request), encoding="utf-8")
            handoff = module.NativeTerrainHandoff(str(request_path))
            handoff.start()
            rejection = json.loads(Path(str(request_path) + ".result.json").read_text(encoding="utf-8"))
            assert rejection["status"] == "failed", rejection
            assert "Campaign reconstructions cannot be opened" in rejection["message"], rejection
            assert not (workspace / "EditorAssets").exists(), "Rejected handoff wrote editable assets"
            result["checks"].append("legacy-python-handoff-rejected-before-assets")
        result["status"] = "PASSED"
        finish()
    except Exception as error:
        result["error"] = str(error)
        finish()

general.idle_enable(True)
general.open_pane("FOA Development Hub")
general.open_pane("Heightmap Importer")
timer = QtCore.QTimer(QtWidgets.QApplication.instance())
timer.setInterval(200)
timer.timeout.connect(check)
timer.start()
