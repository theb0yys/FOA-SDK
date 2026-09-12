# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Qualify source affine placement in a private native level, including a fresh reopen.

FOA_NATIVE_PLACEMENT_ROOT supplies a private fixture and output directory. Set
FOA_NATIVE_PLACEMENT_REOPEN=1 for a separate process reading the saved level.
No source game files are changed. This is component acceptance, not full-map readiness.
"""
import copy
import hashlib
import json
import os
from pathlib import Path
import struct
import time
import traceback
from PySide6 import QtCore
import azlmbr.bus as bus
import azlmbr.components as components
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.foa as foa
import azlmbr.math as math
import azlmbr.legacy.general as general

root=Path(os.environ['FOA_NATIVE_PLACEMENT_ROOT']).resolve(strict=True)
if any((p/'.git').exists() for p in (root,*root.parents)):
    raise RuntimeError('Private placement fixture required.')
fixture=json.loads((root/'placement-fixtures.json').read_text())
reopen=os.environ.get('FOA_NATIVE_PLACEMENT_REOPEN')=='1'
output=root/('placement-reopen.json' if reopen else 'placement-editor.json')
if output.exists():raise RuntimeError('Existing placement evidence must not be overwritten.')
report={'status':'FAILED','stage':'starting','checks':[],'rejections':[],'states':[]}
identity=[1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.]


def record(stage):
    report['stage']=stage;output.write_text(json.dumps(report,indent=2))


def call(eid,event,*args):return foa.SourceScenePlacementBus(bus.Event,event,eid,*args)


def values(matrix):return [matrix.GetElement(r,c) for r in range(4) for c in range(4)]


def floats(bits):return list(struct.unpack('<16f',struct.pack('<16I',*bits)))


def bits(values):return list(struct.unpack('<16I',struct.pack('<16f',*values)))


def host(value):
    order=[0,2,1,3]
    return [value[order[r]*4+order[c]] for r in range(4) for c in range(4)]


def multiply(a,b):return [sum(a[r*4+k]*b[k*4+c] for k in range(4)) for r in range(4) for c in range(4)]


def corners(matrix,box):
    points=[]
    for corner in range(8):
        p=[box['max' if corner&(1<<axis) else 'min'][axis] for axis in range(3)]
        p=[p[0],p[2],p[1],1.]
        points.append([sum(matrix[r*4+c]*p[c] for c in range(4)) for r in range(3)])
    return {'min':[min(p[a] for p in points) for a in range(3)],'max':[max(p[a] for p in points) for a in range(3)]}


def check(eid,descriptor,name,exact=False,expected=None):
    assert call(eid,'GetStatus')=='READY'
    assert call(eid,'GetSource')==descriptor
    source=json.loads(descriptor);base=host(floats(source['source_world_bits']))
    anchor=components.TransformBus(bus.Event,'GetWorldTM',eid)
    if expected is None:
        cols=[anchor.GetBasisX(),anchor.GetBasisY(),anchor.GetBasisZ(),anchor.GetTranslation()]
        native=[col.GetElement(r) for r in range(3) for col in cols]+[0.,0.,0.,1.]
        linear=base.copy();linear[3]=linear[7]=linear[11]=0.
        expected=multiply(native,linear)
    actual=values(call(eid,'GetWorldMatrix'))
    if exact:assert bits(actual)==bits(base),name+': unchanged affine bits changed'
    else:
        error=max(abs(a-b) for a,b in zip(actual,expected));assert error<.0001,name+': edited affine changed '+str(error)
    box=call(eid,'GetWorldBounds');bounds=None
    if source['local_bounds'] is not None:
        expected_box=corners(actual,source['local_bounds'])
        low,high=box.min,box.max;bounds={'min':[low.x,low.y,low.z],'max':[high.x,high.y,high.z]}
        assert max(abs(bounds[k][i]-expected_box[k][i]) for k in bounds for i in range(3))<.0001,name+': bounds differ'
    report['states'].append({'name':name,'source_sha256':hashlib.sha256(descriptor.encode()).hexdigest(),'matrix_bits':bits(actual),'bounds':bounds})
    return actual


def begin(name,eid=None):
    editor.ToolsApplicationRequestBus(bus.Broadcast,'BeginUndoBatch',name)
    if eid is not None:editor.ToolsApplicationRequestBus(bus.Broadcast,'AddDirtyEntity',eid)


def end():editor.ToolsApplicationRequestBus(bus.Broadcast,'EndUndoBatch')


def create(parent,name,position,typ):
    eid=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',math.Vector3(),parent);assert eid.IsValid()
    editor.EditorEntityAPIBus(bus.Event,'SetName',eid,name)
    components.TransformBus(bus.Event,'SetWorldTranslation',eid,math.Vector3(*map(float,position)))
    outcome=editor.EditorComponentAPIBus(bus.Broadcast,'AddComponentsOfType',eid,[typ]);assert outcome.IsSuccess()
    return eid


def run():
    project=Path(fixture['project']).resolve(strict=True)
    assert not any((p/'.git').exists() for p in (project,*project.parents))
    level=Path(fixture['level']).resolve(strict=True)
    assert level.is_relative_to(project)
    general.idle_enable(True);record('opening-private-level')
    assert general.open_level(str(level));yield 2
    types=editor.EditorComponentAPIBus(bus.Broadcast,'FindComponentTypeIdsByEntityType',['Source scene placement'],entity.EntityType().Game)
    assert len(types)==1 and not types[0].IsNull(),'Native placement component missing'
    typ=types[0]
    if reopen:
        previous=json.loads((root/'placement-editor.json').read_text());assert previous['status']=='PASSED'
        all_entities=entity.SearchBus(bus.Broadcast,'SearchEntities',entity.SearchFilter());resolved={}
        for eid in all_entities:
            if editor.EditorComponentAPIBus(bus.Broadcast,'GetComponentOfType',eid,typ).IsSuccess():
                source=call(eid,'GetSource')
                if source:
                    digest=hashlib.sha256(source.encode()).hexdigest();assert digest not in resolved,'Duplicate persistent source binding'
                    resolved[digest]=eid
        for state in previous['saved']:
            eid=resolved[state['source_sha256']];descriptor=state['descriptor']
            check(eid,descriptor,'reopen-'+state['name'],expected=floats(state['matrix_bits']))
            assert bits(values(call(eid,'GetWorldMatrix')))==state['matrix_bits'],'Saved placement bits changed'
            parent_hash=state['parent_source_sha256']
            if parent_hash:assert editor.EditorEntityInfoRequestBus(bus.Event,'GetParent',eid)==resolved[parent_hash]
            editor.ToolsApplicationRequestBus(bus.Broadcast,'SetSelectedEntities',[eid]);yield .05
            assert editor.ToolsApplicationRequestBus(bus.Broadcast,'GetSelectedEntities')==[eid]
        report['checks']+=['unique-persistent-source-identities','native-parent-hierarchy','exact-saved-matrix-bits','reopened-selection']
        report['status']='PASSED';record('reopen-complete');return
    parent=editor.ToolsApplicationRequestBus(bus.Broadcast,'GetCurrentLevelEntityId');assert parent.IsValid()
    created=[];descriptors=[]
    for index,case in enumerate(fixture['cases']):
        descriptor=json.dumps(case['descriptor'],separators=(',',':'));base=host(floats(case['descriptor']['source_world_bits']))
        parent_eid=created[case['parent']] if case['parent']>=0 else parent
        begin('Import source placement')
        eid=create(parent_eid,'Source '+case['name'],[base[3],base[7],base[11]],typ)
        assert call(eid,'BindSource',descriptor),'Source binding failed: '+case['name']
        end();created.append(eid);descriptors.append(descriptor);yield .05
        check(eid,descriptor,'original-'+case['name'],exact=True)
        assert call(eid,'BindSource',descriptor),'Idempotent source binding failed'
        changed=copy.deepcopy(case['descriptor']);changed['identity']['record_sha256']='f'*64
        assert not call(eid,'BindSource',json.dumps(changed)),'Source binding was mutable'
    report['checks']+=['bit-identical-unchanged-affine','immutable-source-binding','native-selection-bounds']
    record('rejecting-invalid-bindings')
    original=fixture['cases'][0]['descriptor'];bad=[]
    for name,path,value in [
        ('future-version',['schema_version'],2),('bool-version',['schema_version'],True),
        ('wrong-profile',['profile'],'unknown'),('unbound-parent',['source_parent'],{'serialized_file':'synthetic-placement','path_id':'12'}),('missing-matrix',['source_local_bits'],None),
        ('bool-matrix-bit',['source_world_bits',0],True),('nonfinite-matrix',['source_world_bits',0],0x7fc00000),
        ('perspective-matrix',['source_world_bits',15],0),('numeric-id',['identity','path_id'],7),
        ('zero-id',['identity','path_id'],'0'),('overflow-id',['identity','path_id'],'9223372036854775808'),
        ('noncanonical-id',['identity','path_id'],'01'),('bad-fingerprint',['identity','bundle_sha256'],'x'*64),
        ('reversed-bounds',['local_bounds'],{'min':[1,1,1],'max':[0,0,0]})]:
        item=copy.deepcopy(original);target=item
        for part in path[:-1]:target=target[part]
        target[path[-1]]=value;bad.append((name,json.dumps(item)))
    item=copy.deepcopy(original);item['source_parent']={k:item['identity'][k] for k in ('serialized_file','path_id')};bad.append(('self-parent',json.dumps(item)))
    raw=json.dumps(original);bad += [('truncated',raw[:-1]),('duplicate-key',raw[:-1]+',"schema_version":1}'),('oversized',' '*8193)]
    begin('Invalid placement fixture');invalid=create(parent,'Invalid placement',[0,0,0],typ);end()
    for name,descriptor in bad:
        assert not call(invalid,'BindSource',descriptor),name+' was accepted'
        assert call(invalid,'GetSource')=='' and call(invalid,'GetStatus')=='UNBOUND'
        report['rejections'].append(name)
    components.TransformBus(bus.Event,'SetWorldTranslation',invalid,math.Vector3(100.,100.,100.))
    assert not call(invalid,'BindSource',json.dumps(original));report['rejections'].append('mismatched-anchor')
    editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',invalid);yield .1
    record('editing-native-placement')
    leaf=created[-1];descriptor=descriptors[-1]
    editor.ToolsApplicationRequestBus(bus.Broadcast,'SetSelectedEntities',[leaf]);yield .1
    assert editor.ToolsApplicationRequestBus(bus.Broadcast,'GetSelectedEntities')==[leaf]
    before=values(call(leaf,'GetWorldMatrix'));position=components.TransformBus(bus.Event,'GetWorldTranslation',leaf)
    begin('Move source object',leaf)
    components.TransformBus(bus.Event,'SetWorldTranslation',leaf,math.Vector3(position.x+2,position.y-1,position.z+.5));end();yield .2
    moved=check(leaf,descriptor,'moved-leaf')
    expected=before.copy();expected[3]+=2;expected[7]-=1;expected[11]+=.5
    assert max(abs(a-b) for a,b in zip(moved,expected))<.0001
    general.undo();yield .2;assert bits(values(call(leaf,'GetWorldMatrix')))==bits(before),'Undo changed source pose'
    general.redo();yield .2;assert bits(values(call(leaf,'GetWorldMatrix')))==bits(moved),'Redo changed moved pose'
    parent_index=fixture['cases'][-1]['parent'];assert parent_index>=0
    parent_eid=created[parent_index]
    begin('Rotate and scale source parent',parent_eid)
    components.TransformBus(bus.Event,'SetLocalRotationQuaternion',parent_eid,math.Quaternion_CreateRotationZ(.3))
    components.TransformBus(bus.Event,'SetLocalUniformScale',parent_eid,1.5);end();yield .2
    for index in (parent_index,len(created)-1):check(created[index],descriptors[index],'parent-edit-'+fixture['cases'][index]['name'])
    report['checks']+=['native-selection','move-undo-redo','parent-rotation-uniform-scale-with-full-source-affine']
    begin('Rename source object',leaf);editor.EditorEntityAPIBus(bus.Event,'SetName',leaf,'Renamed exact source object');end();yield .1
    saved=[]
    for index,(eid,descriptor) in enumerate(zip(created,descriptors)):
        case=fixture['cases'][index];digest=hashlib.sha256(descriptor.encode()).hexdigest()
        saved.append({'name':case['name'],'descriptor':descriptor,'source_sha256':digest,'matrix_bits':bits(values(call(eid,'GetWorldMatrix'))),
                      'parent_source_sha256':hashlib.sha256(descriptors[case['parent']].encode()).hexdigest() if case['parent']>=0 else None})
    record('saving-native-placement');assert general.save_level(),'Native level save failed';yield 2
    report['saved']=saved;report['status']='PASSED';record('native-placement-complete')


job=run()
started=time.monotonic()

def step():
    try:
        if time.monotonic()-started>180:raise RuntimeError('Native placement test deadline exceeded')
        delay=next(job);QtCore.QTimer.singleShot(round(delay*1000),step)
    except StopIteration:general.exit_no_prompt()
    except Exception as error:
        report['error']=str(error);report['traceback']=traceback.format_exc()
        try:record('failed')
        finally:general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
