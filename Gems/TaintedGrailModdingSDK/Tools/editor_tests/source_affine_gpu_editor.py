# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Capture synthetic native full-affine geometry and normal behavior on Qt ticks."""
import json
import os
from pathlib import Path
import time
import traceback
from PySide6 import QtCore, QtWidgets
import azlmbr.atom as atom
import azlmbr.asset as asset
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.math as math
import azlmbr.render as render
import azlmbr.legacy.general as general

root=Path(os.environ['FOA_AFFINE_GPU_ROOT']).resolve(strict=True)
if any((p/'.git').exists() for p in (root,*root.parents)):
    raise RuntimeError('Expected private synthetic fixture storage.')
fixture=json.loads((root/'affine-gpu-fixtures.json').read_text())
if [c['name'] for c in fixture['cases']]!=['identity','nonuniform','reflection','shear','parent_scale','renderer_offset']:
    raise RuntimeError('Unexpected native draw case set.')
report={'status':'FAILED','stage':'starting','cases':[]};handlers=[]
output=root/'affine-gpu-editor.json'

def quitting():
    report['about_to_quit']=True
    report['quit_stack']=traceback.format_stack()
    output.write_text(json.dumps(report,indent=2))

def last_window_closed():
    report['last_window_closed']=True
    output.write_text(json.dumps(report,indent=2))

QtWidgets.QApplication.instance().aboutToQuit.connect(quitting)
QtWidgets.QApplication.instance().lastWindowClosed.connect(last_window_closed)


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
    record('capturing-'+name);path=root/(name+'.ppm');assert not path.exists()
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
    project=Path(fixture['project']).resolve(strict=True)
    assert not any((p/'.git').exists() for p in (project,*project.parents))
    general.idle_enable(True);record('opening-synthetic-level')
    assert general.open_level(str(project/'Levels/MaterialGpuProbe/MaterialGpuProbe.prefab'));yield 2
    general.set_current_view_position(0.,-5.,.35);general.set_current_view_rotation(0.,0.,0.)
    general.set_cvar_integer('r_DisplayInfo',0);general.set_viewport_expansion_policy('FixedSize');general.set_viewport_size(800,450);yield 2
    parent=editor.ToolsApplicationRequestBus(bus.Broadcast,'GetCurrentLevelEntityId');assert parent.IsValid()
    background,bg_mesh=create(parent)
    bg='assets/affine_gpu_cases/background_foamesh.glb.azmodel';black='assets/affine_gpu_cases/black.azmaterial'
    paths=[bg,black]+[p for c in fixture['cases'] for p in (c['model'],c['material'],c['reference'])]
    record('waiting-for-affine-assets');yield from wait(lambda:all(aid(p).is_valid() for p in paths),120)
    set_model(bg_mesh,bg);yield 2
    assert editor.EditorComponentAPIBus(bus.Broadcast,'SetComponentProperty',bg_mesh,'Controller|Configuration|Sort Key',0).IsSuccess()
    for case in fixture['cases']:
        render.MaterialComponentRequestBus(bus.Event,'SetMaterialAssetIdOnDefaultSlot',background,aid(case['reference']));yield 3
        yield from capture(case['name']+'-reference')
        render.MaterialComponentRequestBus(bus.Event,'SetMaterialAssetIdOnDefaultSlot',background,aid(black));yield 1
        eid,mesh=create(parent);set_model(mesh,case['model']);yield 1
        assert editor.EditorComponentAPIBus(bus.Broadcast,'SetComponentProperty',mesh,'Controller|Configuration|Sort Key',1).IsSuccess()
        record('waiting-for-model-'+case['name'])
        yield from wait(lambda:not render.MaterialComponentRequestBus(bus.Event,'FindMaterialAssignmentId',eid,0,'SourceSlot0').IsDefault(),90)
        slot=render.MaterialComponentRequestBus(bus.Event,'FindMaterialAssignmentId',eid,0,'SourceSlot0');assert not slot.IsDefault()
        render.MaterialComponentRequestBus(bus.Event,'SetMaterialAssetId',eid,slot,aid(case['material']))
        assert render.MaterialComponentRequestBus(bus.Event,'GetMaterialAssetId',eid,slot)==aid(case['material'])
        yield 3;yield from capture(case['name']+'-actual')
        report['cases'].append({'name':case['name'],'material':case['material'],'capture_status':'PASSED'})
        editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityAndAllDescendants',eid);yield 1
    report.update(status='PARTIAL',capture_status='PASSED',pixel_comparison='NOT_RUN');record('captured-all-cases')


job=run()


def step():
    try:
        delay=next(job);QtCore.QTimer.singleShot(round(delay*1000),step)
    except StopIteration:
        report['script_finished']=True;record(report['stage']);general.exit_no_prompt()
    except Exception as error:
        report['error']=str(error);report['traceback']=traceback.format_exc();record('failed');general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
