# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native source entity edits/camera/reopen. Launch with --autotest_mode in private storage."""
import copy
import hashlib
import json
import os
from pathlib import Path
import struct
import time
import traceback
from PySide6 import QtCore
import azlmbr.atom as atom
import azlmbr.bus as bus
import azlmbr.components as components
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.foa as foa
import azlmbr.math as math
import azlmbr.legacy.general as general

root=Path(os.environ['FOA_ENTITY_RENDER_ROOT']).resolve(strict=True)
assert not any((p/'.git').exists() for p in (root,*root.parents))
fixture=json.loads((root/'fixture.json').read_text())
reopen=os.environ.get('FOA_ENTITY_RENDER_REOPEN')=='1'
output=root/('reopen.json' if reopen else 'editor.json');assert not output.exists()
report={'status':'FAILED','stage':'starting','captures':[],'rejections':[],'states':[]}
handlers=[];created=[];started=time.monotonic()


def record(stage):
    report['stage']=stage;report['statistics']=json.loads(draw('GetStatistics'));output.write_text(json.dumps(report,indent=2))


def render(eid,event,*args):return foa.SourceSceneRenderBus(bus.Event,event,eid,*args)


def pose(eid,event,*args):return foa.SourceScenePlacementBus(bus.Event,event,eid,*args)


def draw(event,*args):return foa.SourceShaderRenderBus(bus.Broadcast,event,*args)


def visible(eid,show):editor.EditorEntityAPIBus(bus.Event,'SetVisibilityState',eid,show)


def floats(data):return list(struct.unpack('<'+'f'*(len(data)//8),bytes.fromhex(data)))


def hex_floats(data):return struct.pack('<'+'f'*len(data),*data).hex()


def matrix(eid):
    native=pose(eid,'GetWorldMatrix');axis=[0,2,1,3]
    return [native.GetElement(axis[r],axis[c]) for r in range(4) for c in range(4)]


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
    report['captures'].append({'name':name,'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})


def camera_state():
    projection=draw('GetViewportWorldToClip');position=draw('GetViewportCameraPosition')
    rotation=general.get_current_view_rotation()
    return {'host_world_to_clip':[projection.GetElement(r,c) for r in range(4) for c in range(4)],
            'host_position':[position.x,position.y,position.z],'rotation':[rotation.x,rotation.y,rotation.z]}


def reference(matrices,camera=None,uv_by_object=None):
    result=copy.deepcopy(fixture['binding']['draw']);streams=result['streams'];indices=[]
    positions=floats(streams[0]['hex']);uv=floats(streams[1]['hex']);out=[];uv_out=[]
    for object_index,world in enumerate(matrices):
        for offset in range(0,len(positions),3):
            p=positions[offset:offset+3]+[1.]
            out.extend(sum(world[r*4+c]*p[c] for c in range(4)) for r in range(3))
        indices.extend(i+len(uv_out)//2 for i in [0,1,2,0,2,3]);uv_out.extend(uv_by_object[object_index] if uv_by_object else uv)
    streams[0]['hex']=hex_floats(out);streams[1]['hex']=hex_floats(uv_out)
    result['vertex_count']=4*len(matrices);result['indices']=struct.pack('<'+'I'*len(indices),*indices).hex()
    if camera:
        # Reference geometry is already in absolute source world coordinates. Bind the absolute native
        # projection, with camera subtraction disabled, independently of the entity's relative route.
        matrix=camera['host_world_to_clip'];axis=[0,2,1,3]
        packed=struct.pack('<16f',*(matrix[r*4+axis[c]] for c in range(4) for r in range(4)))
        cb=result['stages'][0]['constants'][0];assert cb['slot']==0
        raw=bytearray.fromhex(cb['hex']);raw[320:384]=packed;raw[768:784]=bytes(16);cb['hex']=raw.hex()
    return result


def compare(name,ids,uv_by_object=None):
    yield from wait(lambda:all(render(e,'GetStatus')!='LOADING' for e in ids))
    assert all(render(e,'GetStatus')=='READY' for e in ids),[render(e,'GetStatus') for e in ids]
    editor.ToolsApplicationRequestBus(bus.Broadcast,'SetSelectedEntities',[]);yield .7
    state={'name':name,'matrices':[matrix(e) for e in ids]}
    if fixture.get('camera'):state['camera']=camera_state()
    if uv_by_object:state['uv_by_object']=uv_by_object
    report['states'].append(state)
    yield from capture(name+'-entities')


def references():
    for eid in created:visible(eid,False)
    yield .3
    for state in report['states']:
        if state.get('camera'):
            general.set_current_view_position(*state['camera']['host_position']);general.set_current_view_rotation(*state['camera']['rotation']);yield 1
        assert draw('SetDraw',json.dumps(reference(state['matrices'],state.get('camera'),state.get('uv_by_object'))))=='LOADING'
        yield from wait(lambda:draw('GetStatus')!='LOADING');assert draw('GetStatus')=='READY';yield .7
        yield from capture(state['name']+'-reference')
        draw('ClearDraw');yield .2
    for eid in created:visible(eid,True)
    yield .3


def begin(name,eid=None):
    editor.ToolsApplicationRequestBus(bus.Broadcast,'BeginUndoBatch',name)
    if eid is not None:editor.ToolsApplicationRequestBus(bus.Broadcast,'AddDirtyEntity',eid)


def end():editor.ToolsApplicationRequestBus(bus.Broadcast,'EndUndoBatch')


def add_component(eid,typ):
    outcome=editor.EditorComponentAPIBus(bus.Broadcast,'AddComponentsOfType',eid,[typ]);assert outcome.IsSuccess()


def shared_geometry_batch(root_entity,placement_type,render_type):
    record('shared-geometry-capacity')
    before=json.loads(draw('GetStatistics'))
    binding=fixture['binding'];geometry=[bytes.fromhex(v['hex']) for v in binding['draw']['streams']]
    geometry.append(bytes.fromhex(binding['draw']['indices']))
    unique={value for value in geometry}
    geometry_bytes=sum(map(len,unique))
    constant_bytes=sum(len(bytes.fromhex(c['hex'])) for stage in binding['draw']['stages'] for c in stage['constants'])
    assert before['entity_draws']==2 and before['unique_geometry_buffers']==len(unique)
    assert before['geometry_payload_bytes']==geometry_bytes and before['constant_payload_bytes']==2*constant_bytes
    batch=[];begin('Shared scene capacity fixture');batch_start=time.monotonic()
    def create(index):
        eid=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',math.Vector3(),root_entity);assert eid.IsValid()
        source=copy.deepcopy(fixture['cases'][1]['placement'])
        source['identity']['path_id']=str(index+10000);source['identity']['gameobject_id']=str(index+20000)
        source['identity']['record_sha256']=hashlib.sha256(('shared-'+str(index)).encode()).hexdigest()
        pos=fixture['cases'][1]['world'];components.TransformBus(bus.Event,'SetWorldTranslation',eid,math.Vector3(pos[3],pos[11],pos[7]))
        add_component(eid,placement_type);assert pose(eid,'BindSource',json.dumps(source));add_component(eid,render_type)
        visible(eid,False);return eid
    for index in range(1022):
        eid=create(index);assert render(eid,'BindDraw',json.dumps(binding));batch.append(eid)
        if index%32==31:yield .01
    rejected=create(1022);snapshot=json.loads(draw('GetStatistics'))
    assert not render(rejected,'BindDraw',json.dumps(binding))
    assert render(rejected,'GetBinding')=='' and render(rejected,'GetStatus')=='UNBOUND'
    after=json.loads(draw('GetStatistics'))
    for field in ('entity_draws','resident_payload_bytes','unique_geometry_buffers','geometry_buffer_reuses'):
        assert snapshot[field]==after[field],field+' changed on rejected admission'
    editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',rejected);end()
    report['rejections'].append('entity-capacity-1025-without-resource-leak')
    yield from wait(lambda:all(render(e,'GetStatus')!='LOADING' for e in batch),90)
    assert all(render(e,'GetStatus')=='READY' for e in batch)
    yield .5
    stats=json.loads(draw('GetStatistics'))
    assert stats['entity_draws']==1024 and stats['geometry_payload_bytes']==geometry_bytes
    assert stats['unique_geometry_buffers']==len(unique) and stats['constant_payload_bytes']==1024*constant_bytes
    assert stats['geometry_buffer_builds']==before['geometry_buffer_builds']
    assert stats['shared_stages']==1 and stats['sampler_reservations']==1
    assert stats['shared_stage_reuses']-before['shared_stage_reuses']==1022
    assert stats['geometry_buffer_reuses']-before['geometry_buffer_reuses']==1022*len(geometry)
    assert stats['peak_tick_work']==4 and 0<stats['peak_tick_visits']<=64
    report['sharing_capacity']={'statistics':stats,'elapsed_seconds':time.monotonic()-batch_start,
        'unshared_geometry_bytes':1024*sum(map(len,geometry)),'shared_geometry_bytes':geometry_bytes,
        'expected_constant_bytes_per_draw':constant_bytes}
    record('removing-shared-geometry-owners')
    removed=batch[::2];retained=batch[1::2]
    for index,eid in enumerate(removed):
        editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',eid)
        if index%32==31:yield .01
    for index,eid in enumerate(retained):
        position=components.TransformBus(bus.Event,'GetWorldTranslation',eid)
        components.TransformBus(bus.Event,'SetWorldTranslation',eid,math.Vector3(position.x+.125,position.y,position.z))
        if index%32==31:yield .01
    yield from wait(lambda:json.loads(draw('GetStatistics'))['dirty_entity_draws']==0)
    stats=json.loads(draw('GetStatistics'))
    assert stats['entity_draws']==513 and stats['retired_draws']==0
    assert stats['constant_payload_bytes']==513*constant_bytes and stats['geometry_payload_bytes']==geometry_bytes
    assert stats['geometry_buffer_builds']==before['geometry_buffer_builds'] and stats['failed_entity_draws']==0
    report['sharing_after_removal']=stats
    # Equal position/index data with different UV bytes must share only the equal resources.
    variant=create(2048);changed=copy.deepcopy(binding)
    original_uv=floats(changed['draw']['streams'][1]['hex'])
    flipped_uv=[1.-v if i%2 else v for i,v in enumerate(original_uv)]
    changed['draw']['streams'][1]['hex']=hex_floats(flipped_uv)
    assert render(variant,'BindDraw',json.dumps(changed))
    components.TransformBus(bus.Event,'SetWorldTranslation',variant,math.Vector3(.45,0.,0.))
    yield from wait(lambda:render(variant,'GetStatus')!='LOADING')
    assert render(variant,'GetStatus')=='READY'
    for eid in created:visible(eid,False)
    visible(retained[0],True);visible(variant,True);yield .5
    stats=json.loads(draw('GetStatistics'))
    assert stats['unique_geometry_buffers']==len(unique)+1
    assert stats['geometry_payload_bytes']==geometry_bytes+len(bytes.fromhex(changed['draw']['streams'][1]['hex']))
    report['sharing_distinct_uv']=stats
    yield from compare('different-uv-shared-position-index',[retained[0],variant],[original_uv,flipped_uv])
    visible(retained[0],False);editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',variant)
    for eid in created:visible(eid,True)
    for index,eid in enumerate(retained):
        editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',eid)
        if index%32==31:yield .01
    yield .5
    stats=json.loads(draw('GetStatistics'))
    assert stats['entity_draws']==2 and stats['retired_draws']==0
    assert stats['resident_payload_bytes']==before['resident_payload_bytes']
    report['sharing_after_batch']=stats
    record('shared-geometry-byte-budget')
    large=copy.deepcopy(binding);large['draw']['vertex_count']=65536
    arrays=[]
    for stream in large['draw']['streams']:
        original=bytes.fromhex(stream['hex']);raw=bytearray(65536*stream['components']*4)
        raw[:len(original)]=original;arrays.append(raw)
    admitted=[];rejected_budget=False
    for index in range(60):
        for stream,raw in zip(large['draw']['streams'],arrays):
            raw[-4:]=struct.pack('<I',index+1);stream['hex']=raw.hex()
        eid=create(3000+index);prior=json.loads(draw('GetStatistics'))
        accepted=render(eid,'BindDraw',json.dumps(large));current=json.loads(draw('GetStatistics'))
        assert current['resident_payload_bytes']<=64*1024*1024
        if not accepted:
            assert render(eid,'GetBinding')=='' and render(eid,'GetStatus')=='UNBOUND'
            for field in ('entity_draws','resident_payload_bytes','unique_geometry_buffers','geometry_buffer_reuses'):
                assert prior[field]==current[field],field+' leaked during byte-budget rejection'
            assert prior['resident_payload_bytes']+sum(map(len,arrays))+constant_bytes>64*1024*1024
            report['sharing_budget_rejection']={'accepted_large_draws':len(admitted),'statistics':current}
            report['rejections'].append('geometry-byte-budget-without-partial-admission')
            editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',eid);rejected_budget=True;break
        admitted.append(eid);yield .01
    assert rejected_budget,'The native resident-byte budget was not enforced'
    for eid in admitted:editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',eid)
    yield from wait(lambda:json.loads(draw('GetStatistics'))['resident_payload_bytes']==before['resident_payload_bytes'])
    report['sharing_after_budget']=json.loads(draw('GetStatistics'))
    record('shared-stage-sampler-budget')
    stages=[];variant_binding=copy.deepcopy(binding)
    assert len(variant_binding['draw']['stages'][1]['samplers'])==1
    constant=variant_binding['draw']['stages'][1]['constants'][0];raw=bytearray.fromhex(constant['hex'])
    for index in range(514):
        raw[-4:]=struct.pack('<I',index+1);constant['hex']=raw.hex()
        eid=create(4000+index);assert render(eid,'BindDraw',json.dumps(variant_binding));stages.append(eid)
        if index%32==31:yield .01
    yield from wait(lambda:all(render(e,'GetStatus')!='LOADING' for e in stages),90)
    statuses=[render(e,'GetStatus') for e in stages]
    stats=json.loads(draw('GetStatistics'))
    assert stats['shared_stages']==stats['sampler_reservations']==512,stats
    assert statuses.count('READY')==511 and sum(s.startswith('FAILED:') for s in statuses)==3
    report['sharing_sampler_budget']={'ready_unique_stages':511,'rejected_unique_stages':3,'statistics':stats}
    report['rejections'].append('sampler-budget-before-native-pool-exhaustion')
    for index,eid in enumerate(stages):
        editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',eid)
        if index%32==31:yield .01
    yield from wait(lambda:json.loads(draw('GetStatistics'))['sampler_reservations']==1)
    stats=json.loads(draw('GetStatistics'));assert stats['failed_entity_draws']==0 and stats['shared_stages']==1
    assert stats['resident_payload_bytes']==before['resident_payload_bytes']
    report['sharing_after_samplers']=stats
    yield from compare('after-sharing-removals',created)


def run():
    project=Path(fixture['project']).resolve(strict=True);level=Path(fixture['level']).resolve(strict=True)
    assert level.is_relative_to(project) and not any((p/'.git').exists() for p in (project,*project.parents))
    general.idle_enable(True);record('opening-private-level');assert general.open_level(str(level));yield 2
    general.set_cvar_integer('r_DisplayInfo',0)
    general.set_viewport_expansion_policy('FixedSize');general.set_viewport_size(800,450);yield 1
    if fixture.get('camera'):
        general.set_current_view_position(*fixture['camera']['position']);general.set_current_view_rotation(*fixture['camera']['rotation']);yield 1
    types=editor.EditorComponentAPIBus(bus.Broadcast,'FindComponentTypeIdsByEntityType',['Source scene placement','Source scene rendering'],entity.EntityType().Game)
    assert len(types)==2 and all(not t.IsNull() for t in types)
    placement_type,render_type=types
    if reopen:
        previous=json.loads((root/'editor.json').read_text());assert previous['status']=='PARTIAL' and previous['native_checks']=='PASSED'
        found={}
        for eid in entity.SearchBus(bus.Broadcast,'SearchEntities',entity.SearchFilter()):
            text=pose(eid,'GetSource')
            if text:found[hashlib.sha256(text.encode()).hexdigest()]=eid
        for saved in previous['saved']:
            eid=found[saved['source_sha256']];created.append(eid)
            assert render(eid,'GetBinding')==saved['binding']
            assert matrix(eid)==saved['matrix'],'Saved source placement changed'
        yield from compare('reopened',created)
        yield from references()
        report.update(status='PARTIAL',native_checks='PASSED',pixel_comparison='NOT_RUN');record('reopened');return
    root_entity=editor.ToolsApplicationRequestBus(bus.Broadcast,'GetCurrentLevelEntityId');assert root_entity.IsValid()
    all_created=[]
    for case in fixture['cases']:
        descriptor=json.dumps(case['placement'],separators=(',',':'));values=case['world'];parent=all_created[case['parent']] if case['parent']>=0 else root_entity
        begin('Create source shader object')
        eid=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',math.Vector3(),parent);assert eid.IsValid()
        editor.EditorEntityAPIBus(bus.Event,'SetName',eid,case['name'])
        components.TransformBus(bus.Event,'SetWorldTranslation',eid,math.Vector3(values[3],values[11],values[7]))
        add_component(eid,placement_type);assert pose(eid,'BindSource',descriptor)
        if case['render']:
            add_component(eid,render_type)
            assert render(eid,'BindDraw',json.dumps(case.get('binding',fixture['binding']))),render(eid,'GetStatus')
            created.append(eid)
        all_created.append(eid);end();yield .1
    assert len(created)==2
    record('rejecting-invalid-render-bindings')
    invalid=all_created[0];add_component(invalid,render_type);valid=fixture['binding'];bad=[]
    for name,path,value in [
        ('future-version',['version'],3),('boolean-version',['version'],True),('no-matrices',['matrices'],[]),
        ('wrong-stage',['matrices',0,'stage'],2),('boolean-slot',['matrices',0,'slot'],True),
        ('missing-buffer',['matrices',0,'slot'],13),('unaligned-offset',['matrices',0,'offset'],1),
        ('outside-buffer',['matrices',0,'offset'],624),('wrong-layout',['matrices',0,'layout'],'row_major'),
        ('unknown-matrix',['matrices',0,'value'],'guess'),('wrong-draw-version',['draw','version'],2)]:
        changed=copy.deepcopy(valid);target=changed
        for part in path[:-1]:target=target[part]
        target[path[-1]]=value;bad.append((name,json.dumps(changed)))
    changed=copy.deepcopy(valid);changed['matrices']*=2;bad.append(('overlapping-matrices',json.dumps(changed)))
    changed=copy.deepcopy(valid);changed['matrices']*=9;bad.append(('matrix-count',json.dumps(changed)))
    if valid['version']==2:
        for name,path,value in [
            ('missing-vectors',['vectors'],None),('vector-count',['vectors'],valid['vectors']*9),
            ('vector-stage',['vectors',0,'stage'],2),('vector-layout',['vectors',0,'layout'],'column_major'),
            ('unknown-vector',['vectors',0,'value'],'guess'),('vector-unaligned',['vectors',0,'offset'],769),
            ('vector-outside',['vectors',0,'offset'],3536),('matrix-vector-overlap',['vectors',0,'offset'],320),
            ('old-version-camera',['version'],1)]:
            changed=copy.deepcopy(valid);target=changed
            for part in path[:-1]:target=target[part]
            target[path[-1]]=value;bad.append((name,json.dumps(changed)))
        changed=copy.deepcopy(valid);changed['vectors']*=2;bad.append(('vector-overlap',json.dumps(changed)))
    raw=json.dumps(valid);bad.append(('deep-draw','{"version":1,"matrices":'+json.dumps(valid['matrices'])+',"draw":'+'['*8192+'0'+']'*8192+'}'))
    bad.extend([('unknown-field',raw[:-1]+',"unexpected":true}'),('duplicate-field',raw[:-1]+',"version":1}'),('truncated',raw[:-1]),('oversized',' '*((16<<20)+1))])
    for name,text in bad:
        assert not render(invalid,'BindDraw',text),name
        assert render(invalid,'GetStatus')=='UNBOUND' and render(invalid,'GetBinding')==''
        report['rejections'].append(name)
    changed=copy.deepcopy(valid);changed['draw']['sort_key']+=1
    assert not render(created[0],'BindDraw',json.dumps(changed));report['rejections'].append('immutable-binding')
    yield from compare('initial',created)
    eid=created[0];position=components.TransformBus(bus.Event,'GetWorldTranslation',eid)
    begin('Move source drawing',eid)
    components.TransformBus(bus.Event,'SetWorldTranslation',eid,math.Vector3(position.x+.1,position.y,position.z+.3));end();yield .3
    yield from compare('moved',created)
    # Visibility operations are not part of the transform undo batch.
    general.undo();yield .3
    yield from compare('undo',created)
    assert report['states'][-1]['matrices']==report['states'][0]['matrices'],'Undo did not restore the source rendering pose'
    general.redo();yield .3
    yield from compare('redo',created)
    assert report['states'][-1]['matrices']==report['states'][1]['matrices'],'Redo did not restore the edited source rendering pose'
    parent=all_created[0];begin('Rotate source parent',parent)
    components.TransformBus(bus.Event,'SetLocalRotationQuaternion',parent,math.Quaternion_CreateRotationY(.25));end();yield .3
    yield from compare('parent-rotated',created)
    visible(created[1],False);yield .5
    yield from compare('hidden-child',[created[0]])
    visible(created[1],True);yield .5
    yield from compare('shown-child',created)
    # Deleting an entity must retire its draw; undo recreates its persisted component binding.
    deleted_source=pose(created[1],'GetSource');begin('Delete source drawing')
    editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',created[1]);end();yield .5
    yield from compare('deleted-child',[created[0]])
    general.undo();yield .5
    restored=[e for e in entity.SearchBus(bus.Broadcast,'SearchEntities',entity.SearchFilter()) if pose(e,'GetSource')==deleted_source]
    assert len(restored)==1;created[1]=restored[0]
    yield from compare('restored-child',created)
    if fixture.get('camera'):
        general.set_current_view_position(1.2,-3.0,.5);yield 1
        yield from compare('camera-translated',created)
        general.set_current_view_rotation(2.,0.,-5.);yield 1
        yield from compare('camera-rotated',created)
    if fixture.get('shared_geometry'):
        yield from shared_geometry_batch(root_entity,placement_type,render_type)
    else:
        record('bounded-resource-batch')
        extra=[]
        for index in range(6):
            eid=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',math.Vector3(),root_entity);assert eid.IsValid()
            source=copy.deepcopy(fixture['cases'][1]['placement']);source['identity']['path_id']=str(index+1000);source['identity']['gameobject_id']=str(index+2000)
            source['identity']['record_sha256']=hashlib.sha256(('batch-'+str(index)).encode()).hexdigest()
            pos=fixture['cases'][1]['world'];components.TransformBus(bus.Event,'SetWorldTranslation',eid,math.Vector3(pos[3],pos[11],pos[7]))
            add_component(eid,placement_type);assert pose(eid,'BindSource',json.dumps(source));add_component(eid,render_type)
            assert render(eid,'BindDraw',json.dumps(fixture['binding']));visible(eid,False);extra.append(eid)
        yield from wait(lambda:all(render(e,'GetStatus')!='LOADING' for e in extra))
        assert all(render(e,'GetStatus')=='READY' for e in extra)
        stats=json.loads(draw('GetStatistics'));assert stats['entity_draws']==8 and stats['peak_tick_work']==4,stats
        report['batch_statistics']=stats
        for eid in extra:editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',eid)
        yield .5
        stats=json.loads(draw('GetStatistics'));assert stats['entity_draws']==2 and stats['retired_draws']==0,stats
        report['after_batch_statistics']=stats
    yield from references()
    report['saved']=[{'source_sha256':hashlib.sha256(pose(e,'GetSource').encode()).hexdigest(),'binding':render(e,'GetBinding'),'matrix':matrix(e)} for e in created]
    assert general.save_level();yield 1
    if fixture.get('shared_geometry'):
        for eid in created:editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',eid)
        draw('ClearDraw')
        yield from wait(lambda:json.loads(draw('GetStatistics'))['resident_payload_bytes']==0)
        stats=json.loads(draw('GetStatistics'))
        assert stats['entity_draws']==stats['retired_draws']==stats['unique_geometry_buffers']==stats['geometry_payload_bytes']==stats['shared_stages']==stats['sampler_reservations']==0
        report['sharing_final_release']=stats
        # Do not save the cleanup: the second process must reopen the previously saved scene.
    report.update(status='PARTIAL',native_checks='PASSED',pixel_comparison='NOT_RUN');record('saved')


job=run()


def step():
    try:
        if time.monotonic()-started>(900 if fixture.get('shared_geometry') else 300):raise RuntimeError('Native entity rendering deadline exceeded')
        delay=next(job);QtCore.QTimer.singleShot(round(delay*1000),step)
    except StopIteration:general.exit_no_prompt()
    except Exception as error:
        report.update(error=str(error),traceback=traceback.format_exc())
        try:record('failed')
        finally:draw('ClearDraw');general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
