# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Run synthetic swatches in a disposable O3DE Editor, using staged Qt ticks."""
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

root=Path(os.environ['FOA_MATERIAL_GPU_ROOT']).resolve(strict=True)
if any((p/'.git').exists() for p in (root,*root.parents)):
    raise RuntimeError('Expected private synthetic fixture storage.')
fixture=json.loads((root/'material-gpu-fixtures.json').read_text())
if len(fixture['cases'])!=4: raise RuntimeError('Expected all four GPU cases.')
report={'status':'FAILED','stage':'starting','cases':[]}
output=root/'material-gpu-editor.json'
handlers=[]

def record(stage):
    report['stage']=stage
    output.write_text(json.dumps(report,indent=2),encoding='utf-8')

def aid(path):
    return asset.AssetCatalogRequestBus(bus.Broadcast,'GetAssetIdByPath',path,math.Uuid(),False)

def wait(predicate,seconds=90):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        if predicate(): return
        yield .2
    raise RuntimeError('Timed out: '+report['stage'])

def run():
    general.idle_enable(True)
    record('opening-empty-fixture')
    assert general.open_level(str(root/'Project/Levels/MaterialGpuProbe/MaterialGpuProbe.prefab'))
    yield 2
    general.set_current_view_position(0.,-5.,.35)
    general.set_current_view_rotation(0.,0.,0.)
    general.set_cvar_integer('r_DisplayInfo',0)
    general.set_viewport_expansion_policy('FixedSize')
    general.set_viewport_size(800,450)
    yield 2
    parent=editor.ToolsApplicationRequestBus(bus.Broadcast,'GetCurrentLevelEntityId')
    assert parent.IsValid()
    eid=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',math.Vector3(),parent)
    types=editor.EditorComponentAPIBus(bus.Broadcast,'FindComponentTypeIdsByEntityType',['Mesh','Material'],entity.EntityType().Game)
    assert len(types)==2 and all(not t.IsNull() for t in types)
    parts=editor.EditorComponentAPIBus(bus.Broadcast,'AddComponentsOfType',eid,types)
    assert parts.IsSuccess()
    mesh=parts.GetValue()[0]
    ready=[]
    handler=render.MeshComponentNotificationBusHandler()
    handler.connect(eid)
    handler.add_callback('OnModelReady',lambda args:ready.append(True))
    handlers.append(handler)
    for case in fixture['cases']:
        record('loading-'+case['name'])
        yield from wait(lambda:aid(case['model']).is_valid() and aid(case['material']).is_valid(),120)
        ready.clear()
        result=editor.EditorComponentAPIBus(bus.Broadcast,'SetComponentProperty',mesh,'Controller|Configuration|Model Asset',aid(case['model']))
        assert result.IsSuccess()
        # Returning to the same mesh can be already ready; the model bus is authoritative.
        yield 2
        render.MaterialComponentRequestBus(bus.Event,'SetMaterialAssetIdOnDefaultSlot',eid,aid(case['material']))
        yield 4
        assert render.MaterialComponentRequestBus(bus.Event,'GetMaterialAssetIdOnDefaultSlot',eid)==aid(case['material'])
        record('capturing-'+case['name'])
        path=root/('material-gpu-'+case['name']+'.ppm')
        assert not path.exists()
        outcome=atom.FrameCaptureRequestBus(bus.Broadcast,'CaptureScreenshot',str(path))
        assert outcome.IsSuccess()
        done=[]
        capture=atom.FrameCaptureNotificationBusHandler()
        capture.connect(outcome.GetValue())
        capture.add_callback('OnFrameCaptureFinished',lambda args:done.append(args))
        handlers.append(capture)
        yield from wait(lambda:bool(done),30)
        assert done[0][0]==atom.FrameCaptureResult_Success
        capture.disconnect()
        report['cases'].append({**case,'capture':path.name,'capture_status':'PASSED'})
    # Pixel comparison runs independently outside the Editor. Capture success alone is not parity.
    report['status']='PARTIAL';report['capture_status']='PASSED';report['pixel_comparison']='NOT_RUN'
    record('captured-all-cases')

job=run()
def step():
    try:
        delay=next(job)
        QtCore.QTimer.singleShot(round(delay*1000),step)
    except StopIteration:
        general.exit_no_prompt()
    except Exception as error:
        report['error']=str(error);report['traceback']=traceback.format_exc()
        record('failed')
        general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
