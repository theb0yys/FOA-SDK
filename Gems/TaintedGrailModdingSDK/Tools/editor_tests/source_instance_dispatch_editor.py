# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native instance-count, visibility and persistence acceptance with explicit fixtures."""
import copy
import hashlib
import json
import os
from pathlib import Path
import time
import traceback
from PySide6 import QtCore
import azlmbr.atom as atom
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.foa as foa
import azlmbr.math as math
import azlmbr.legacy.general as general

root=Path(os.environ['FOA_INSTANCE_DISPATCH_ROOT']).resolve(strict=True)
assert not any((p/'.git').exists() for p in (root,*root.parents))
fixture=json.loads((root/'fixture.json').read_text())
reopen=os.environ.get('FOA_INSTANCE_DISPATCH_REOPEN')=='1'
output=root/('reopen.json' if reopen else 'editor.json');assert not output.exists()
report={'status':'FAILED','stage':'starting','captures':[],'rejections':[]};handlers=[]


def service(event,*args):return foa.SourceShaderRenderBus(bus.Broadcast,event,*args)
def render(eid,event,*args):return foa.SourceSceneRenderBus(bus.Event,event,eid,*args)
def place(eid,event,*args):return foa.SourceScenePlacementBus(bus.Event,event,eid,*args)
def stats():return json.loads(service('GetStatistics'))
def record(stage):report['stage']=stage;output.write_text(json.dumps(report,indent=2))
def begin(name,eid=None):
    editor.ToolsApplicationRequestBus(bus.Broadcast,'BeginUndoBatch',name)
    if eid is not None:editor.ToolsApplicationRequestBus(bus.Broadcast,'AddDirtyEntity',eid)
def end():editor.ToolsApplicationRequestBus(bus.Broadcast,'EndUndoBatch')


def wait(predicate,seconds=90):
    deadline=time.monotonic()+seconds
    while not predicate():
        assert time.monotonic()<deadline,'Timeout: '+report['stage'];yield .1


def capture(name):
    editor.ToolsApplicationRequestBus(bus.Broadcast,'SetSelectedEntities',[]);yield .2
    record('capturing-'+name);path=root/(name+'.ppm');assert not path.exists()
    result=atom.FrameCaptureRequestBus(bus.Broadcast,'CaptureScreenshot',str(path));assert result.IsSuccess()
    done=[];handler=atom.FrameCaptureNotificationBusHandler();handler.connect(result.GetValue());handlers.append(handler)
    handler.add_callback('OnFrameCaptureFinished',lambda args:done.append(args))
    yield from wait(lambda:bool(done),30);assert done[0][0]==atom.FrameCaptureResult_Success;handler.disconnect()
    report['captures'].append(dict(name=name,path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest()))


def draw(value):
    started=time.monotonic();assert service('SetDraw',json.dumps(value))=='LOADING'
    yield from wait(lambda:service('GetStatus')!='LOADING');assert service('GetStatus')=='READY'
    yield .6
    report.setdefault('ready_seconds',[]).append(time.monotonic()-started)


def rejections(base):
    bad=[]
    for count in (0,-1,True,1.5,'3',None,4097,0xffffffff):bad.append(('count-'+str(count),dict(base,instance_count=count)))
    bad.extend([('missing-count',{k:v for k,v in base.items() if k!='instance_count'}),
                ('old-version-new-field',dict(base,version=2)),('unknown-field',dict(base,unexpected=0)),
                ('future-version',dict(base,version=4)),('boolean-version',dict(base,version=True)),
                ('wrong-shape',[]),('missing-version',{k:v for k,v in base.items() if k!='version'})])
    over=dict(base,instance_count=4096,indices='000000000100000002000000'*171)
    bad.append(('index-instance-work',over))
    before=stats()
    for name,value in bad:
        result=service('SetDraw',json.dumps(value));assert result.startswith('REJECTED:'),(name,result)
        assert service('GetStatus')=='READY';after=stats()
        for key in ('resident_payload_bytes','read_only_buffer_payload_bytes','constant_payload_bytes','unique_geometry_buffers'):
            assert before[key]==after[key],(name,key)
        report['rejections'].append(name)
    yield .6;yield from capture('rejected_preserved')
    # Validate the last complete triangle under the work ceiling, without submitting it.
    bounded=dict(over,indices='000000000100000002000000'*170)
    assert service('SetDraw',json.dumps(bounded))=='LOADING';service('ClearDraw')
    report['admitted_index_instance_work']=4096*510


def resolve():
    found=[e for e in entity.SearchBus(bus.Broadcast,'SearchEntities',entity.SearchFilter())
           if place(e,'GetSource') and json.loads(place(e,'GetSource'))==fixture['placement']]
    assert len(found)==1;return found[0]


def binding():
    case=next(c for c in fixture['cases'] if c['name']=='indirect_subset')
    descriptor=json.loads((root/case['descriptor']).read_text())
    # Bytes 0..63 are unused by this exact original VS. This explicit synthetic
    # matrix satisfies the entity envelope without changing the test camera.
    # Instance transforms remain in caller-owned buffers; editing them is separate.
    return {'version':1,'draw':descriptor,'matrices':[dict(stage=0,slot=0,offset=0,layout='column_major',value='source_object_to_world')]}


def run():
    project=Path(fixture['project']).resolve(strict=True);level=Path(fixture['level']).resolve(strict=True)
    assert level.is_relative_to(project) and not any((p/'.git').exists() for p in (project,*project.parents))
    general.idle_enable(True);assert general.open_level(str(level));yield 2
    general.set_cvar_integer('r_DisplayInfo',0);general.set_cvar_integer('ed_keepEditorActive',1)
    editor.ToolsApplicationRequestBus(bus.Broadcast,'SetSelectedEntities',[]);yield .5
    expected=binding()
    if reopen:
        assert json.loads((root/'editor.json').read_text())['status']=='PARTIAL'
        eid=resolve();yield from wait(lambda:render(eid,'GetStatus')=='READY')
        assert json.loads(render(eid,'GetBinding'))==expected;yield .6;yield from capture('reopened')
        report['binding_sha256']=hashlib.sha256(render(eid,'GetBinding').encode()).hexdigest()
        assert report['binding_sha256']==json.loads((root/'editor.json').read_text())['binding_sha256']
    else:
        assert service('GetStatus')=='EMPTY';yield from capture('cleared')
        for case in fixture['cases']:
            record('loading-'+case['name']);descriptor=json.loads((root/case['descriptor']).read_text())
            yield from draw(descriptor);yield from capture(case['name'])
            actual=stats();assert actual['read_only_buffer_payload_bytes']==case['raw_payload_bytes']
            assert actual['constant_payload_bytes']==case['constant_bytes']
        control=next(c for c in fixture['cases'] if c['name']=='direct_all')
        descriptor=json.loads((root/control['descriptor']).read_text());yield from draw(descriptor)
        yield from rejections(descriptor);yield .5
        yield from draw(descriptor);yield from capture('after_rejections');service('ClearDraw');yield .6
        types=editor.EditorComponentAPIBus(bus.Broadcast,'FindComponentTypeIdsByEntityType',
            ['Source scene placement','Source scene rendering'],entity.EntityType().Game)
        assert len(types)==2 and all(not t.IsNull() for t in types)
        begin('Create instanced source draw');parent=editor.ToolsApplicationRequestBus(bus.Broadcast,'GetCurrentLevelEntityId')
        eid=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',math.Vector3(),parent);assert eid.IsValid()
        editor.EditorEntityAPIBus(bus.Event,'SetName',eid,'Synthetic instanced source draw')
        for typ in types:assert editor.EditorComponentAPIBus(bus.Broadcast,'AddComponentsOfType',eid,[typ]).IsSuccess()
        assert place(eid,'BindSource',json.dumps(fixture['placement']))
        assert render(eid,'BindDraw',json.dumps(expected));assert place(eid,'CommitAssemblyState');end()
        yield from wait(lambda:render(eid,'GetStatus')=='READY');yield .6;yield from capture('entity')
        assert json.loads(render(eid,'GetBinding'))==expected
        editor.EditorEntityAPIBus(bus.Event,'SetVisibilityState',eid,False);yield .6;yield from capture('hidden')
        editor.EditorEntityAPIBus(bus.Event,'SetVisibilityState',eid,True);yield .6;yield from capture('shown')
        begin('Delete instanced draw');editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',eid);end()
        yield .6;yield from capture('deleted')
        general.undo();yield .6;eid=resolve();yield from wait(lambda:render(eid,'GetStatus')=='READY');yield .6;yield from capture('undo')
        general.redo();yield .6;yield from capture('redo')
        general.undo();yield .6;eid=resolve();yield from wait(lambda:render(eid,'GetStatus')=='READY');yield .6;yield from capture('restored')
        assert json.loads(render(eid,'GetBinding'))==expected
        assert general.save_level();yield .6
        report['binding_sha256']=hashlib.sha256(render(eid,'GetBinding').encode()).hexdigest()
    begin('Release test draw');editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',eid);end();yield .6
    yield from capture('released_reopen' if reopen else 'released')
    final=stats();assert all(final[k]==0 for k in ('entity_draws','registered_entities','retired_draws','resident_payload_bytes','sampler_reservations'))
    assert final['peak_tick_work']<=4 and final['peak_tick_visits']<=64;report['final_statistics']=final
    report.update(status='PARTIAL',native_execution='PASSED',pixel_comparison='NOT_RUN');record('captured')


job=run()


def step():
    try:QtCore.QTimer.singleShot(round(next(job)*1000),step)
    except StopIteration:general.exit_no_prompt()
    except Exception as error:
        report.update(error=str(error),traceback=traceback.format_exc());record('failed');service('ClearDraw');general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
