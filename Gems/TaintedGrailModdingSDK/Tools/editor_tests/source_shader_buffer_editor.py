# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native v2 buffer draw binding persistence, edits and independent reference captures."""
import copy
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import time
import traceback
from PySide6 import QtCore
import azlmbr.atom as atom
import azlmbr.bus as bus
import azlmbr.components as components
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.math as math
import azlmbr.legacy.general as general
import azlmbr.foa as foa
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from foa_scene_assembly_editor import matrix_bits, placement

root=Path(os.environ['FOA_BUFFER_ENTITY_ROOT']).resolve(strict=True)
assert not any((p/'.git').exists() for p in (root,*root.parents))
fixture=json.loads((root/'fixture.json').read_text());reopen=os.environ.get('FOA_BUFFER_ENTITY_REOPEN')=='1'
output=root/('reopen.json' if reopen else 'editor.json');assert not output.exists()
report=dict(status='FAILED',stage='starting',states=[],captures=[]);handlers=[];started=time.monotonic()


def service(event,*args):return foa.SourceShaderRenderBus(bus.Broadcast,event,*args)
def render(eid,event,*args):return foa.SourceSceneRenderBus(bus.Event,event,eid,*args)
def record(stage):report['stage']=stage;output.write_text(json.dumps(report,indent=2))
def statistics():return json.loads(service('GetStatistics'))
def begin(name,eid=None):
    editor.ToolsApplicationRequestBus(bus.Broadcast,'BeginUndoBatch',name)
    if eid is not None:editor.ToolsApplicationRequestBus(bus.Broadcast,'AddDirtyEntity',eid)
def end():editor.ToolsApplicationRequestBus(bus.Broadcast,'EndUndoBatch')
def visible(eid,value):editor.EditorEntityAPIBus(bus.Event,'SetVisibilityState',eid,value)
def wait(predicate):
    deadline=time.monotonic()+120
    while not predicate():
        assert time.monotonic()<deadline,report['stage'];yield .1


def capture(name):
    record('capture-'+name);path=root/(name+'.ppm');assert not path.exists()
    outcome=atom.FrameCaptureRequestBus(bus.Broadcast,'CaptureScreenshot',str(path));assert outcome.IsSuccess()
    done=[];handler=atom.FrameCaptureNotificationBusHandler();handler.connect(outcome.GetValue());handlers.append(handler)
    handler.add_callback('OnFrameCaptureFinished',lambda args:done.append(args))
    yield from wait(lambda:bool(done));assert done[0][0]==atom.FrameCaptureResult_Success;handler.disconnect()
    report['captures'].append(dict(name=name,path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest()))


def state(eid):
    return dict(source=placement(eid,'GetSource'),binding=render(eid,'GetBinding'),matrix_bits=list(matrix_bits(eid)))


def compare(name,eid):
    yield from wait(lambda:render(eid,'GetStatus')!='LOADING');assert render(eid,'GetStatus')=='READY'
    assert json.loads(render(eid,'GetBinding'))==fixture['binding']
    editor.ToolsApplicationRequestBus(bus.Broadcast,'SetSelectedEntities',[]);yield .5
    raw=struct.unpack('<16f',struct.pack('<16I',*matrix_bits(eid)));axis=(0,2,1,3)
    world=[raw[axis[r]*4+axis[c]] for r in range(4) for c in range(4)]
    report['states'].append(dict(name=name,matrix=world,statistics=statistics()))
    yield from capture(name+'-entity')


def references(eid):
    visible(eid,False);yield .3
    for state in report['states']:
        name,world=state['name'],state['matrix']
        # Reference uses CPU-transformed vertices and the unchanged identity constant.
        # It does not invoke the entity matrix-patching path being tested.
        draw=copy.deepcopy(fixture['binding']['draw']);data=bytes.fromhex(draw['streams'][0]['hex'])
        vertices=struct.unpack('<'+'f'*(len(data)//4),data);positions=[]
        for i in range(0,len(vertices),3):
            point=[*vertices[i:i+3],1.]
            positions.extend(sum(world[r*4+c]*point[c] for c in range(4)) for r in range(3))
        draw['streams'][0]['hex']=struct.pack('<'+'f'*len(positions),*positions).hex()
        assert service('SetDraw',json.dumps(draw))=='LOADING'
        yield from wait(lambda:service('GetStatus')!='LOADING');assert service('GetStatus')=='READY';yield .5
        yield from capture(name+'-reference');service('ClearDraw');yield .3
    visible(eid,True);yield .3


def run():
    general.idle_enable(True);general.set_cvar_integer('ed_keepEditorActive',1);record('opening-level')
    project=Path(fixture['project']).resolve(strict=True);level=Path(fixture['level']).resolve(strict=True)
    assert level.is_relative_to(project) and not any((p/'.git').exists() for p in (project,*project.parents))
    assert general.open_level(str(level));yield 1;general.set_cvar_integer('r_DisplayInfo',0)
    if reopen:
        previous=json.loads((root/'editor.json').read_text());assert previous['status']=='PASSED'
        matches=[e for e in entity.SearchBus(bus.Broadcast,'SearchEntities',entity.SearchFilter()) if placement(e,'GetSource')]
        assert len(matches)==1;eid=matches[0]
        assert state(eid)==previous['saved'];yield from compare('reopened',eid);report['restored']=state(eid)
    else:
        types=editor.EditorComponentAPIBus(bus.Broadcast,'FindComponentTypeIdsByEntityType',['Source scene placement','Source scene rendering'],entity.EntityType().Game)
        assert len(types)==2 and all(not t.IsNull() for t in types)
        begin('Create synthetic buffer placement')
        parent=editor.ToolsApplicationRequestBus(bus.Broadcast,'GetCurrentLevelEntityId')
        eid=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',math.Vector3(),parent);assert eid.IsValid()
        descriptor=fixture['placement'];world=struct.unpack('<16f',struct.pack('<16I',*descriptor['source_world_bits']))
        components.TransformBus(bus.Event,'SetWorldTranslation',eid,math.Vector3(world[3],world[11],world[7]))
        for typ in types:assert editor.EditorComponentAPIBus(bus.Broadcast,'AddComponentsOfType',eid,[typ]).IsSuccess()
        assert placement(eid,'BindSource',json.dumps(descriptor))
        assert render(eid,'BindDraw',json.dumps(fixture['binding'])) and placement(eid,'CommitAssemblyState');end()
        yield from compare('initial',eid)
        begin('Move buffer placement',eid);components.TransformBus(bus.Event,'SetWorldTranslation',eid,math.Vector3(.2,0.,.15));end();yield .3
        yield from compare('moved',eid);general.undo();yield .3;yield from compare('undo',eid)
        general.redo();yield .3;yield from compare('redo',eid)
        assert general.save_level();yield .5;report['saved']=state(eid)
    yield from references(eid)
    begin('Remove buffer placement');editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',eid);end();yield .5
    report['final_release']=statistics()
    assert all(report['final_release'][k]==0 for k in ('entity_draws','registered_entities','resident_payload_bytes','read_only_buffer_payload_bytes','retired_draws','shared_stages','sampler_reservations'))
    report.update(status='PASSED',elapsed_seconds=time.monotonic()-started,complete_campaign_maps='NOT_RUN',game_export='NOT_RUN');record('complete')


job=run()
def step():
    try:
        assert time.monotonic()-started<600
        delay=next(job);QtCore.QTimer.singleShot(round(delay*1000),step)
    except StopIteration:general.exit_no_prompt()
    except Exception as error:
        report.update(error=str(error),traceback=traceback.format_exc());record('failed');general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
