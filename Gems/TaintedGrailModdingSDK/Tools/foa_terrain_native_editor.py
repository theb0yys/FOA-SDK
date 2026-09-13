# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Materialize a validated workspace heightmap using the existing O3DE Editor.

Run only in the embedded Editor. No viewport, brush, object editor or custom
scene format is implemented here. O3DE owns all subsequent editing and saving.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import time

import azlmbr.asset as asset
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.legacy.general as general
import azlmbr.math as math
import azlmbr.paths as paths
from PySide6 import QtCore, QtWidgets


def component_types(names, level=False):
    ids = editor.EditorComponentAPIBus(bus.Broadcast, "FindComponentTypeIdsByEntityType", names,
        entity.EntityType().Level if level else entity.EntityType().Game)
    if len(ids) != len(names) or any(i.IsNull() for i in ids):
        raise RuntimeError("Enable the O3DE Terrain Gem and rebuild the Editor.")
    return ids


def set_property(component, name, value):
    answer = editor.EditorComponentAPIBus(bus.Broadcast, "SetComponentProperty", component, name, value)
    if not answer.IsSuccess():
        raise RuntimeError("Unable to configure the native terrain component: " + name)


def create_entity(name, components, position):
    parent = editor.ToolsApplicationRequestBus(bus.Broadcast, "GetCurrentLevelEntityId")
    if not parent.IsValid():
        raise RuntimeError("Wait for the O3DE level to finish loading before opening terrain.")
    eid = editor.ToolsApplicationRequestBus(bus.Broadcast, "CreateNewEntityAtPosition", position, parent)
    if not eid.IsValid():
        raise RuntimeError("Unable to create a native terrain entity.")
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, name)
    answer = editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, component_types(components))
    if not answer.IsSuccess():
        editor.ToolsApplicationRequestBus(bus.Broadcast, "DeleteEntityById", eid)
        raise RuntimeError("Unable to add native terrain components.")
    return eid, dict(zip(components, answer.GetValue()))


def add_level_component(name):
    tid = component_types([name], True)[0]
    if editor.EditorLevelComponentAPIBus(bus.Broadcast, "HasComponentOfType", tid):
        answer = editor.EditorLevelComponentAPIBus(bus.Broadcast, "GetComponentOfType", tid)
        return answer.GetValue()
    answer = editor.EditorLevelComponentAPIBus(bus.Broadcast, "AddComponentsOfType", [tid])
    if not answer.IsSuccess():
        raise RuntimeError("Unable to add the native " + name + " component.")
    return answer.GetValue()[0]


class NativeTerrainHandoff(QtCore.QObject):
    def __init__(self, request_path):
        super().__init__(QtWidgets.QApplication.instance())
        self.request_path = Path(request_path)
        self.timer = QtCore.QTimer(self)
        self.timer.setInterval(200)
        self.timer.timeout.connect(self.poll)
        self.finished = False
        self.stage = "asset"
        self.new_level = False

    def finish(self, status, message):
        self.timer.stop()
        self.finished = True
        output = Path(str(self.request_path) + ".result.json")
        temporary = Path(str(output) + ".pending")
        temporary.write_text(json.dumps({"status": status, "busy": False, "message": message}), encoding="utf-8")
        os.replace(temporary, output)
        self.deleteLater()

    def start(self):
        try:
            if self.request_path.stat().st_size > 4 * 1024 * 1024:
                raise RuntimeError("The terrain handoff exceeds its metadata limit.")
            self.request = json.loads(self.request_path.read_text(encoding="utf-8"))
            workspace = Path(self.request["workspace"])
            if workspace.resolve() != workspace or any((p / ".git").exists() for p in [workspace, *workspace.parents]):
                raise RuntimeError("Editable terrain requires a workspace outside source control.")
            for key in ("asset_root", "image", "level"):
                path = Path(self.request[key])
                if not path.resolve().is_relative_to(workspace) or path.resolve() != path:
                    raise RuntimeError("The native terrain assets must remain inside their workspace.")
            if not self.request_path.resolve().is_relative_to(workspace / "Staging" / "TerrainNative"):
                raise RuntimeError("The native terrain request has an invalid location.")
            if self.request["document"]["source_binding"]["exporter_id"] == "importer.campaign-ground-raw":
                raise RuntimeError("Campaign reconstructions cannot be opened for editing; source preservation and game round-trip are unverified.")
            component_types(["Image Gradient", "Terrain Layer Spawner"])
            # O3DE reads project/user/Registry for AP configuration. The registry
            # contains only a local scan-root setting, never extracted pixels.
            key = hashlib.sha256(str(workspace).encode("utf-8")).hexdigest()[:16]
            registry = Path(paths.projectroot) / "user" / "Registry"
            registry.mkdir(parents=True, exist_ok=True)
            config = registry / ("foa_terrain_" + key + ".setreg")
            if config.resolve() != config:
                raise RuntimeError("The local Asset Processor settings path is unsafe.")
            settings = {"Amazon": {"AssetProcessor": {"Settings": {
                "ScanFolder FOA Terrain " + key: {"watch": self.request["asset_root"],
                    "display": "Workspace Maps", "recursive": 1, "order": 10}}}}}
            encoded = json.dumps(settings, indent=2)
            self.new_scan = not config.exists() or config.read_text(encoding="utf-8") != encoded
            if self.new_scan:
                pending = config.with_suffix(".pending")
                pending.write_text(encoded, encoding="utf-8")
                os.replace(pending, config)
            self.deadline = time.monotonic() + (8 if self.new_scan else 90)
            self.timer.start()
            self.poll()
        except Exception as error:
            self.finish("failed", str(error)[:1000])

    def poll(self):
        try:
            if Path(str(self.request_path) + ".cancel").exists():
                self.finish("failed", "Opening terrain cancelled.")
                return
            if self.stage != "asset":
                self.advance_level()
                return
            image_id = asset.AssetCatalogRequestBus(bus.Broadcast, "GetAssetIdByPath", self.request["asset"], math.Uuid(), False)
            presentation_ready = all(asset.AssetCatalogRequestBus(bus.Broadcast, "GetAssetIdByPath", name, math.Uuid(), False).is_valid()
                for name in ("assets/materials/foa_editor_ground.azmaterial", "assets/textures/foa_editor_ground_basecolor.png.streamingimage"))
            if image_id.is_valid() and presentation_ready:
                self.timer.stop()
                self.open_level(image_id)
            elif time.monotonic() > self.deadline:
                self.finish("failed", "Workspace maps are registered. Restart Asset Processor once, then click Open in Editor. If it is already restarted, check its image-processing errors.")
        except Exception as error:
            self.finish("failed", str(error)[:1000])

    def open_level(self, image_id):
        level = Path(self.request["level"])
        self.new_level = not level.exists()
        if self.new_level:
            template = Path(paths.engroot) / "Assets" / "Editor" / "Prefabs" / "Basic.prefab"
            with template.open("rb") as source, level.open("xb") as target:
                shutil.copyfileobj(source, target)
        # This uses O3DE's normal unsaved-level handling. Never clear its dirty flag.
        if not general.open_level(str(level)):
            if self.new_level:
                level.unlink(missing_ok=True)
            self.finish("failed", "Opening the map was cancelled in the Editor.")
            return
        self.image_id = image_id
        # Opening a prefab schedules propagation on the Editor tick. Creating
        # children synchronously here binds them to the previous container.
        self.stage = "world" if self.new_level else "ready"
        self.timer.start()

    def advance_level(self):
        if self.stage == "world":
            self.configure_world()
            self.stage = "entities"
        elif self.stage == "entities":
            self.configure_level(self.image_id)
            self.stage = "save"
        elif self.stage == "save":
            if not general.save_level():
                raise RuntimeError("O3DE could not save the new map. Save it in the Editor before closing.")
            self.stage = "ready"
        elif self.stage == "ready":
            for pane in ("Asset Browser", "Entity Outliner", "Inspector"):
                general.open_pane(pane)
            self.finish("complete", "Map opened in O3DE. Select Paint Terrain Heights and click Paint in Image Gradient to sculpt. Place objects using Asset Browser and the normal entity tools; save with Ctrl+S.")

    def configure_world(self):
        doc = self.request["document"]
        grid, vertical = doc["grid"], doc["vertical_mapping"]
        world = add_level_component("Terrain World")
        add_level_component("Terrain World Renderer")
        set_property(world, "Configuration|Min Height", float(vertical["min_height_metres"]))
        set_property(world, "Configuration|Max Height", float(vertical["max_height_metres"]))
        set_property(world, "Configuration|Height Query Resolution (m)", float(min(grid["sample_spacing_x_metres"], grid["sample_spacing_y_metres"])))

    def configure_level(self, image_id):
        doc = self.request["document"]
        grid, vertical = doc["grid"], doc["vertical_mapping"]
        sx, sy = grid["sample_spacing_x_metres"], grid["sample_spacing_y_metres"]
        width, depth = (grid["width"] - 1) * sx, (grid["height"] - 1) * sy
        low, high = vertical["min_height_metres"], vertical["max_height_metres"]
        center = (low + high) / 2
        terrain_id, terrain_parts = create_entity(doc["map_identity"]["display_name"],
            ["Axis Aligned Box Shape", "Terrain Layer Spawner", "Terrain Height Gradient List"], math.Vector3(width/2, depth/2, center))
        set_property(terrain_parts["Axis Aligned Box Shape"], "Visible", False)
        set_property(terrain_parts["Axis Aligned Box Shape"], "Filled", False)
        set_property(terrain_parts["Axis Aligned Box Shape"], "Axis Aligned Box Shape|Box Configuration|Dimensions", math.Vector3(width, depth, high-low))
        # Native image gradients map UV 0..1 to N pixels. Give the gradient N
        # sample intervals so canonical sample positions retain exact spacing.
        gradient_id, gradient_parts = create_entity("Paint Terrain Heights",
            ["Axis Aligned Box Shape", "Gradient Transform Modifier", "Image Gradient"], math.Vector3((width+sx)/2, (depth+sy)/2, center))
        set_property(gradient_parts["Axis Aligned Box Shape"], "Visible", False)
        set_property(gradient_parts["Axis Aligned Box Shape"], "Filled", False)
        set_property(gradient_parts["Axis Aligned Box Shape"], "Axis Aligned Box Shape|Box Configuration|Dimensions", math.Vector3(width+sx, depth+sy, high-low))
        set_property(gradient_parts["Gradient Transform Modifier"], "Configuration|Wrapping Type", 1)
        set_property(gradient_parts["Image Gradient"], "Configuration|Image Asset", image_id)
        set_property(gradient_parts["Image Gradient"], "Configuration|Sampling Type", 1)
        set_property(terrain_parts["Terrain Height Gradient List"], "Configuration|Gradient Entities", [gradient_id])
        search = entity.SearchFilter()
        search.names = ["Sun"]
        suns = entity.SearchBus(bus.Broadcast, "SearchEntities", search)
        if suns:
            sky = editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", suns[0], component_types(["Physical Sky"]))
            if not sky.IsSuccess():
                raise RuntimeError("Unable to add daylight to the new map.")
        material = asset.AssetCatalogRequestBus(bus.Broadcast, "GetAssetIdByPath", "assets/materials/foa_editor_ground.azmaterial", math.Uuid(), False)
        if material.is_valid():
            surface = editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", terrain_id, component_types(["Terrain Surface Materials List"]))
            if not surface.IsSuccess():
                raise RuntimeError("Unable to add the terrain ground material.")
            set_property(surface.GetValue()[0], "Configuration|Default Material|Material asset", material)
        macro_image = asset.AssetCatalogRequestBus(bus.Broadcast, "GetAssetIdByPath", "assets/textures/foa_editor_ground_basecolor.png.streamingimage", math.Uuid(), False)
        if macro_image.is_valid():
            macro = editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", terrain_id, component_types(["Terrain Macro Material"]))
            if not macro.IsSuccess():
                raise RuntimeError("Unable to add the terrain overview material.")
            set_property(macro.GetValue()[0], "Configuration|Color Texture", macro_image)
        editor.ToolsApplicationRequestBus(bus.Broadcast, "SetSelectedEntities", [gradient_id])
        general.set_current_view_position(width/2, -depth*.65, high+width*.5)
        general.set_current_view_rotation(-35.0, 0.0, 0.0)


if __name__ == "__main__":
    handoff = NativeTerrainHandoff(sys.argv[1])
    QtCore.QTimer.singleShot(0, handoff.start)
