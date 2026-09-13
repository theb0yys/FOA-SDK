# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Load an already prepared original campaign terrain into the existing Editor.

O3DE owns the viewport, selection, transforms, undo and prefab persistence. This
controller does not read game files or advertise neutral shading as game parity.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import sys
import time
import traceback

sys.path.insert(0, str(Path(__file__).resolve().parent))
import azlmbr.asset as asset
import azlmbr.bus as bus
import azlmbr.components as components
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.foa as foa
import azlmbr.legacy.general as general
import azlmbr.math as native_math
import azlmbr.paths as paths
import azlmbr.prefab as prefab
import azlmbr.settingsregistry as settingsregistry
from PySide6 import QtCore, QtWidgets
from foa_campaign_terrain import validate, SHADER_ASSET, IDENTITY
from foa_heightmap_importer import require_direct_path

NAMES = {'hos': 'Horns of the South', 'cuanacht': 'Cuanacht', 'forlorn': 'Forlorn Swords', 'sarras': 'Sarras'}
_active = None


def private(path):
    path = require_direct_path(Path(path))
    if any((p/'.git').exists() for p in (path, *path.parents)):
        raise RuntimeError('Campaign terrain must remain outside source control.')
    return path


def read_json(path, maximum):
    path = private(path)
    if not path.is_file() or path.stat().st_size > maximum: raise RuntimeError('Invalid terrain metadata file.')
    return json.loads(path.read_bytes())


def placement(eid, event, *args): return foa.SourceScenePlacementBus(bus.Event, event, eid, *args)


def rendering(eid, event, *args): return foa.SourceSceneRenderBus(bus.Event, event, eid, *args)


def frame(document):
    """Frame original coordinates without clipping campaign-sized geometry."""
    general.set_cvar_integer('r_DisplayInfo', 0)
    bounds = [row['placement']['local_bounds'] for row in document['groups']]
    lo = [min(b['min'][i] for b in bounds) for i in range(3)]
    hi = [max(b['max'][i] for b in bounds) for i in range(3)]
    center = [(x+y)*.5 for x,y in zip(lo,hi)]
    extent = max(hi[0]-lo[0], hi[2]-lo[2], 1.)
    distance = extent*.8; height = hi[1]+extent*.6
    far = max(1024., extent*4+hi[1]-lo[1])
    registry = settingsregistry.g_SettingsRegistry
    setting = '/Amazon/Preferences/Editor/Camera/FarPlaneDistance'
    existing = registry.GetFloat(setting)
    if existing: far = max(far, existing.value())
    if not registry.SetFloat(setting, far): raise RuntimeError('The campaign camera clipping range could not be set.')
    general.set_current_view_position(center[0], center[2]-distance, height)
    general.set_current_view_rotation(-math.degrees(math.atan2(height-center[1], distance)), 0., 0.)
    return dict(far_clip=far, host_position=[center[0], center[2]-distance, height])


class TerrainLoad(QtCore.QObject):
    def __init__(self, request_path):
        super().__init__(QtWidgets.QApplication.instance())
        self.path = Path(request_path); self.created = []; self.cursor = 0; self.stage = 'prepare'
        self.output_allowed = False
        self.undo = False; self.finished = False; self.cancelled = False
        self.timer = QtCore.QTimer(self); self.timer.setInterval(20); self.timer.timeout.connect(self.step)
        self.dialog = QtWidgets.QProgressDialog('Checking original terrain...', 'Cancel', 0, 100)
        self.dialog.setWindowTitle('Campaign terrain'); self.dialog.setWindowModality(QtCore.Qt.ApplicationModal)
        self.dialog.setAutoClose(False); self.dialog.setAutoReset(False); self.dialog.canceled.connect(self.cancel)

    def cancel(self): self.cancelled = True

    def finish(self, status, message):
        global _active
        self.timer.stop()
        if self.undo: editor.ToolsApplicationRequestBus(bus.Broadcast, 'EndUndoBatch'); self.undo = False
        self.finished = True
        result = dict(status=status, busy=False, message=message, map=self.request['map'])
        if status != 'complete':
            result.update(failure_stage=getattr(self,'failure_stage',self.stage),created_before_rollback=self.cursor,prefab_owner=getattr(self,'last_owner',''))
        if status == 'complete':
            result.update(level=str(self.level), source_draws=self.document['source_draw_count'],
                          native_binding_readback='PASSED', original_material_rendering='NOT_RUN')
        output = Path(str(self.path)+'.result.json'); pending = Path(str(output)+'.pending')
        if self.output_allowed:
            with pending.open('x', encoding='utf-8') as file: json.dump(result, file)
            os.replace(pending, output)
        else: print('Campaign terrain:', message)
        self.dialog.close(); self.dialog.deleteLater(); _active = None; self.deleteLater()

    def start(self):
        try:
            self.path = private(self.path); self.request = read_json(self.path, 65536)
            if (self.request.get('schema') != 'foa.campaign-terrain.handoff' or self.request.get('version') != 1
                    or self.request.get('map') not in NAMES): raise RuntimeError('Invalid campaign terrain handoff.')
            self.workspace = private(self.request['workspace'])
            if not self.path.is_relative_to(self.workspace/'Staging'/'CampaignTerrain'):
                raise RuntimeError('Terrain handoff is outside the active workspace.')
            if Path(str(self.path)+'.result.json').exists() or Path(str(self.path)+'.result.json.pending').exists():
                raise RuntimeError('This terrain operation already has a result; preserve it and start a new operation.')
            self.output_allowed = True
            prepared_path = private(self.request['prepared'])
            if prepared_path.parent != self.path.parent: raise RuntimeError('Terrain preparation belongs to another operation.')
            prepared = read_json(prepared_path, 1024*1024)
            if prepared.get('status') != 'PASSED' or prepared.get('map') != self.request['map'] or not prepared.get('source_unchanged'):
                raise RuntimeError('Terrain source preparation failed.')
            packet = private(prepared['packet'])
            if packet.parent != self.path.parent or packet.stat().st_size > 128*1024*1024:
                raise RuntimeError('Terrain packet is outside its operation or exceeds its bound.')
            raw = packet.read_bytes()
            if hashlib.sha256(raw).hexdigest() != prepared['packet_sha256']: raise RuntimeError('Terrain packet changed after preparation.')
            self.document = validate(raw)
            if self.document['map'] != self.request['map']: raise RuntimeError('The selected map and terrain packet differ.')
            self.asset_root = private(prepared['asset_root'])
            if self.asset_root != self.workspace/'EditorAssets': raise RuntimeError('Terrain assets belong to another workspace.')
            shader = private(self.asset_root/'Assets/foa_terrain_shape/terrain.foashader')
            if hashlib.sha256(shader.read_bytes()).hexdigest() != prepared['shader_sha256']:
                raise RuntimeError('Terrain inspection shader changed after preparation.')
            self.dialog.setLabelText('Waiting for the terrain inspection shader...'); self.dialog.show()
            registry = Path(paths.projectroot)/'user'/'Registry'
            require_direct_path(registry); registry.mkdir(parents=True, exist_ok=True)
            key = hashlib.sha256(str(self.workspace).encode()).hexdigest()[:16]
            config = registry/('foa_campaign_terrain_'+key+'.setreg')
            settings = {'Amazon': {'AssetProcessor': {'Settings': {'ScanFolder FOA Campaign Terrain '+key:
                dict(watch=str(self.asset_root), display='Campaign Terrain', recursive=1, order=10)}}}}
            encoded = json.dumps(settings, indent=2)
            self.new_scan = not config.exists() or config.read_text() != encoded
            if self.new_scan:
                require_direct_path(config)
                pending = config.with_suffix('.pending'); pending.write_text(encoded); os.replace(pending, config)
            self.deadline = time.monotonic()+90; self.stage = 'asset'; self.timer.start()
        except Exception as error:
            self.request = getattr(self, 'request', {'map': 'unknown'})
            self.finish('failed', str(error)[:1000])

    def step(self):
        try:
            if self.cancelled or Path(str(self.path)+'.cancel').exists():
                self.stage = 'rollback'; self.error = 'Opening campaign terrain cancelled.'
            if self.stage == 'rollback':
                for _ in range(min(4, len(self.created))):
                    eid = self.created.pop(); editor.ToolsApplicationRequestBus(bus.Broadcast, 'DeleteEntityById', eid)
                if not self.created: self.finish('failed', self.error)
                return
            if self.stage == 'asset':
                asset_id = asset.AssetCatalogRequestBus(bus.Broadcast, 'GetAssetIdByPath', SHADER_ASSET, native_math.Uuid(), False)
                if not asset_id.is_valid():
                    if time.monotonic() > self.deadline:
                        raise RuntimeError('Campaign terrain is prepared. Restart Asset Processor to load this workspace scan folder, then open the map again.')
                    return
                self.open_level(); return
            if self.stage == 'level':
                parent = editor.ToolsApplicationRequestBus(bus.Broadcast, 'GetCurrentLevelEntityId')
                loaded = Path(general.get_current_level_path())
                owner = prefab.PrefabPublicRequestBus(bus.Broadcast, 'GetOwningInstancePrefabPath', parent) if parent.IsValid() else ''
                self.last_owner = owner
                expected = self.level.relative_to(self.asset_root).as_posix().casefold()
                owned = owner.replace('\\', '/').casefold()
                # The level name changes before asynchronous prefab loading completes.
                # A valid container ID alone may still belong to the reset root instance.
                if (not parent.IsValid() or loaded.resolve() != self.level.parent.resolve()
                        or general.get_current_level_name() != self.level.parent.name
                        or owned not in (expected, self.level.as_posix().casefold())):
                    if time.monotonic() > self.deadline: raise RuntimeError('The campaign level did not open.')
                    return
                focused = prefab.PrefabFocusPublicRequestBus(bus.Broadcast, 'FocusOnOwningPrefab', parent)
                if not focused.IsSuccess(): raise RuntimeError('The campaign level could not receive authoring focus: '+focused.GetError())
                self.parent = parent
                types = editor.EditorComponentAPIBus(bus.Broadcast, 'FindComponentTypeIdsByEntityType',
                    ['Source scene placement', 'Source scene rendering'], entity.EntityType().Game)
                if len(types) != 2 or any(t.IsNull() for t in types): raise RuntimeError('The native terrain components are unavailable.')
                self.types = types
                editor.ToolsApplicationRequestBus(bus.Broadcast, 'BeginUndoBatch', 'Import '+NAMES[self.request['map']]+' terrain')
                self.undo = True; self.stage = 'create'; self.dialog.setLabelText('Assembling original terrain and cliffs...')
                self.dialog.setMaximum(len(self.document['groups'])); return
            if self.stage == 'create':
                rows = self.document['groups']
                for _ in range(min(4, len(rows)-self.cursor)):
                    row = rows[self.cursor]
                    eid = editor.ToolsApplicationRequestBus(bus.Broadcast, 'CreateNewEntityAtPosition', native_math.Vector3(), self.parent)
                    if not eid.IsValid(): raise RuntimeError('Native terrain entity creation failed.')
                    self.created.append(eid)
                    editor.EditorEntityAPIBus(bus.Event, 'SetName', eid, NAMES[self.request['map']]+' terrain '+str(self.cursor+1))
                    added = editor.EditorComponentAPIBus(bus.Broadcast, 'AddComponentsOfType', eid, self.types)
                    if not added.IsSuccess(): raise RuntimeError('Native terrain component creation failed.')
                    source = json.dumps(row['placement'], separators=(',', ':')); draw = json.dumps(row['rendering'], separators=(',', ':'))
                    if not placement(eid, 'BindSource', source) or not rendering(eid, 'BindDraw', draw):
                        raise RuntimeError('Native terrain source or rendering binding was rejected.')
                    if (placement(eid, 'GetSource') != source or rendering(eid, 'GetBinding') != draw
                            or list(placement(eid, 'GetWorldMatrixBits')) != IDENTITY):
                        raise RuntimeError('Native terrain geometry or placement readback differs.')
                    if not placement(eid, 'CommitAssemblyState'): raise RuntimeError('Native terrain persistence failed.')
                    self.cursor += 1; self.dialog.setValue(self.cursor)
                if self.cursor == len(rows): self.stage = 'ready'; self.deadline = time.monotonic()+90
                return
            if self.stage == 'ready':
                statuses = [rendering(eid, 'GetStatus') for eid in self.created]
                if any(s not in ('READY', 'LOADING') for s in statuses): raise RuntimeError('A native terrain draw failed: '+str(set(statuses)))
                if not all(s == 'READY' for s in statuses):
                    if time.monotonic() > self.deadline: raise RuntimeError('Native terrain loading timed out.')
                    return
                frame(self.document)
                if self.undo:
                    editor.ToolsApplicationRequestBus(bus.Broadcast, 'EndUndoBatch'); self.undo = False
                if not general.save_level(): raise RuntimeError('The campaign terrain level could not be saved.')
                self.finish('complete', NAMES[self.request['map']]+' terrain loaded with '+str(self.document['source_draw_count'])+
                    ' original mesh instances. Neutral terrain shading.'); return
        except Exception as error:
            self.failure_stage=self.stage
            if self.output_allowed:
                diagnostic=Path(str(self.path)+'.failure.log')
                if not diagnostic.exists(): diagnostic.write_text(traceback.format_exc()[-16384:],encoding='utf-8')
            self.error = 'Terrain '+self.stage+': '+str(error)[:900]; self.stage = 'rollback'

    def open_level(self):
        directory = self.asset_root/'Levels'/('Campaign_'+self.request['map'])/self.path.parent.name
        require_direct_path(directory); directory.mkdir(parents=True, exist_ok=True); private(directory)
        self.level = directory/(self.request['map']+'.prefab')
        if self.level.exists(): raise RuntimeError('A level already exists for this import operation; preserve it and choose a new operation.')
        template = Path(paths.engroot)/'Assets/Editor/Prefabs/Basic.prefab'
        data = json.loads(template.read_bytes())
        # The template supplies the native level container. Source terrain replaces
        # its demonstration objects; no existing user level or entity is cleared.
        data['Entities'] = {}; data['Instances'] = {}
        data['Source'] = self.level.relative_to(self.asset_root).as_posix()
        for component in data.get('ContainerEntity', {}).get('Components', {}).values():
            if 'Child Entity Order' in component: component['Child Entity Order'] = []
        with self.level.open('x', encoding='utf-8') as output: json.dump(data, output, indent=2)
        if not general.open_level(str(self.level)): raise RuntimeError('Opening the campaign level was cancelled.')
        self.stage = 'level'; self.deadline = time.monotonic()+60


def start(request_path):
    global _active
    if _active is not None: raise RuntimeError('A campaign terrain import is already running.')
    controller = TerrainLoad(request_path); _active = controller; controller.start()
    return controller


if __name__ == '__main__': start(sys.argv[1])
