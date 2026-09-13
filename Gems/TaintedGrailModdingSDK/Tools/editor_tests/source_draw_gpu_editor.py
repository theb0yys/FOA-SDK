# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Capture synthetic native submesh/slot and repeated-draw behavior on Qt ticks."""
import json
import os
from pathlib import Path
import time
import traceback
from PySide6 import QtCore
import azlmbr.atom as atom
import azlmbr.asset as asset
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.math as math
import azlmbr.render as render
import azlmbr.legacy.general as general

root=Path(os.environ['FOA_DRAW_GPU_ROOT']).resolve(strict=True)
if any((p/'.git').exists() for p in (root,*root.parents)):
    raise RuntimeError('Expected private synthetic fixture storage.')
fixture=json.loads((root/'draw-gpu-fixtures.json').read_text())
if [c['name'] for c in fixture['cases']]!=['fewer','balanced','repeated','additive']:
    raise RuntimeError('Unexpected native draw case set.')
report={'status':'FAILED','stage':'starting','cases':[]};handlers=[]
output=root/'draw-gpu-editor.json'


def record(stage):
    report['stage']=stage;output.write_text(json.dumps(report,indent=2))


def aid(path):
    return asset.AssetCatalogRequestBus(bus.Broadcast,'GetAssetIdByPath',path,math.Uuid(),False)


def wait(predicate,seconds=90):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        if predicate():return
        yield .2
    raise RuntimeError('Timed out: '+report['stage'])


def capture(name):
    record('capturing-'+name);path=root/('draw-gpu-'+name+'.ppm');assert not path.exists()
    outcome=atom.FrameCaptureRequestBus(bus.Broadcast,'CaptureScreenshot',str(path));assert outcome.IsSuccess()
    done=[];handler=atom.FrameCaptureNotificationBusHandler();handler.connect(outcome.GetValue())
    handler.add_callback('OnFrameCaptureFinished',lambda args:done.append(args));handlers.append(handler)
    yield from wait(lambda:bool(done),30)
    assert done[0][0]==atom.FrameCaptureResult_Success;handler.disconnect()


def create(parent):
    eid=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',math.Vector3(),parent)
    types=editor.EditorComponentAPIBus(bus.Broadcast,'FindComponentTypeIdsByEntityType',['Mesh','Material'],entity.EntityType().Game)
    assert len(types)==2 and all(not t.IsNull() for t in types)
    parts=editor.EditorComponentAPIBus(bus.Broadcast,'AddComponentsOfType',eid,types);assert parts.IsSuccess()
    return eid,parts.GetValue()[0]


def set_model(mesh,path):
    result=editor.EditorComponentAPIBus(bus.Broadcast,'SetComponentProperty',mesh,'Controller|Configuration|Model Asset',aid(path))
    assert result.IsSuccess()


def run():
    general.idle_enable(True);record('opening-synthetic-level')
    assert general.open_level(str(root/'Project/Levels/MaterialGpuProbe/MaterialGpuProbe.prefab'));yield 2
    general.set_current_view_position(0.,-5.,.35);general.set_current_view_rotation(0.,0.,0.)
    general.set_cvar_integer('r_DisplayInfo',0);general.set_viewport_expansion_policy('FixedSize');general.set_viewport_size(800,450);yield 2
    parent=editor.ToolsApplicationRequestBus(bus.Broadcast,'GetCurrentLevelEntityId');assert parent.IsValid()
    background,bg_mesh=create(parent)
    paths=['assets/draw_gpu_cases_ordered/background_foamesh.glb.azmodel','assets/draw_gpu_cases_ordered/opaque3.azmaterial','assets/draw_gpu_cases_ordered/opaque4.azmaterial']
    record('waiting-for-background');yield from wait(lambda:all(aid(p).is_valid() for p in paths),120)
    set_model(bg_mesh,paths[0]);yield 2
    assert editor.EditorComponentAPIBus(bus.Broadcast,'SetComponentProperty',bg_mesh,'Controller|Configuration|Sort Key',0).IsSuccess()
    render.MaterialComponentRequestBus(bus.Event,'SetMaterialAssetIdOnDefaultSlot',background,aid(paths[2]));yield 4
    yield from capture('reference')
    render.MaterialComponentRequestBus(bus.Event,'SetMaterialAssetIdOnDefaultSlot',background,aid(paths[1]));yield 4
    yield from capture('black')
    for case in fixture['cases']:
        group=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',math.Vector3(),parent)
        assert group.IsValid()
        record('loading-'+case['name'])
        paths=[p for draw in case['draws'] for p in (draw['model'],draw['material'])]
        yield from wait(lambda:all(aid(p).is_valid() for p in paths),120)
        assigned=[]
        for draw in case['draws']:
            eid,mesh=create(group);set_model(mesh,draw['model'])
            assert editor.EditorComponentAPIBus(bus.Broadcast,'SetComponentProperty',mesh,'Controller|Configuration|Sort Key',draw['sort_key']).IsSuccess()
            yield 1
            slot=render.MaterialComponentRequestBus(bus.Event,'FindMaterialAssignmentId',eid,0,'SourceSlot0')
            assert not slot.IsDefault() and str(render.MaterialComponentRequestBus(bus.Event,'GetMaterialLabel',eid,slot))=='SourceSlot0'
            render.MaterialComponentRequestBus(bus.Event,'SetMaterialAssetId',eid,slot,aid(draw['material']))
            assert render.MaterialComponentRequestBus(bus.Event,'GetMaterialAssetId',eid,slot)==aid(draw['material'])
            assigned.append(dict(draw,native_slot=slot.ToString()))
        yield 4;yield from capture(case['name'])
        report['cases'].append({'name':case['name'],'assigned':assigned,'capture_status':'PASSED'})
        editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityAndAllDescendants',group)
        yield 2
    report.update(status='PARTIAL',capture_status='PASSED',pixel_comparison='NOT_RUN');record('captured-all-cases')


job=run()


def step():
    try:
        delay=next(job);QtCore.QTimer.singleShot(round(delay*1000),step)
    except StopIteration:
        general.exit_no_prompt()
    except Exception as error:
        report['error']=str(error);report['traceback']=traceback.format_exc();record('failed');general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
