# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Private native source-entity qualification; not a campaign import/export command.

Run in a disposable Editor project with FOA_SCENE_PROOF_ROOT outside source control
and FOA_SCENE_PROOF_CACHE pointing to that project's processed pc assets. The caller
must prepare an explicitly authorized local source fixture and a fresh native level.
Identity tags are test-only tokens into private evidence, not a public scene contract.
No screenshots or game writes. Editor work runs on separate ticks to avoid nested
Qt loops under the engine Python lock. See CAMPAIGN_SCENE_EDITING.md for limits.
"""
import json,time,traceback,hashlib,os,sys
from pathlib import Path
from PySide6 import QtCore,QtWidgets
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.components as components
import azlmbr.math as math
import azlmbr.asset as asset
import azlmbr.render as render
import azlmbr.legacy.general as general
root=Path(os.environ['FOA_SCENE_PROOF_ROOT'])
cache=Path(os.environ['FOA_SCENE_PROOF_CACHE'])
for path in (root,cache):
 if not path.is_absolute() or path.resolve()!=path or any((p/'.git').exists() for p in [path,*path.parents]):
  raise RuntimeError('Native source proof requires direct private fixture paths outside source control')
def fixture_json(name):
 path=root/name
 if path.resolve()!=path or path.stat().st_size>16*1024*1024:
  raise RuntimeError('Private proof fixture exceeds its bounds or uses redirected storage')
 return json.loads(path.read_text(encoding='utf-8'))

doc=fixture_json('source_entity.private.json')
level_path=root/doc['native_level_relative_path']
if level_path.resolve()!=level_path or not level_path.is_relative_to(root) or level_path.suffix!='.prefab':
 raise RuntimeError('Native proof level must remain in the private fixture')
result={'status':'FAILED','stage':'starting','checks':[],'projection_limits':doc['projection_limits']}
output=root/'native-entity-import.json'
def record(stage):
 result['stage']=stage;output.write_text(json.dumps(result,indent=2),encoding='utf-8')
def types(names):
 ids=editor.EditorComponentAPIBus(bus.Broadcast,'FindComponentTypeIdsByEntityType',names,entity.EntityType().Game)
 if len(ids)!=len(names) or any(i.IsNull() for i in ids):raise RuntimeError('Missing native component')
 return ids
def setprop(c,path,value):
 r=editor.EditorComponentAPIBus(bus.Broadcast,'SetComponentProperty',c,path,value)
 if not r.IsSuccess():
  result['property_paths']=list(editor.EditorComponentAPIBus(bus.Broadcast,'BuildComponentPropertyList',c))
  raise RuntimeError('Component property failed: '+path)
def vec(v):return [v.x,v.y,v.z]
def wait(predicate,seconds=90):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if predicate():return
  yield .2
 raise RuntimeError('Timed out: '+result['stage'])
def run():
 general.idle_enable(True);record('waiting-for-source-model')
 model=lambda:asset.AssetCatalogRequestBus(bus.Broadcast,'GetAssetIdByPath',doc['host_asset'],math.Uuid(),False)
 yield from wait(lambda:model().is_valid(),120);model_id=model();result['model_id']=str(model_id)
 record('opening-private-level')
 assert general.open_level(str(level_path)),'Level open failed'
 yield 1
 parent=editor.ToolsApplicationRequestBus(bus.Broadcast,'GetCurrentLevelEntityId');assert parent.IsValid()
 created=[];comments=[];handlers=[];model_ready=[]
 for index,row in enumerate(doc['hierarchy']):
  editor.ToolsApplicationRequestBus(bus.Broadcast,'BeginUndoBatch','Import source entity')
  eid=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',math.Vector3(),parent);assert eid.IsValid()
  editor.EditorEntityAPIBus(bus.Event,'SetName',eid,'Source Proof '+row['gameobject_id'])
  names=['Tag']+(['Mesh'] if index==len(doc['hierarchy'])-1 else [])
  added=editor.EditorComponentAPIBus(bus.Broadcast,'AddComponentsOfType',eid,types(names));assert added.IsSuccess()
  parts=dict(zip(names,added.GetValue()))
  binding=json.dumps({'source_binding':doc['source_binding'],'transform_binding':row},sort_keys=True,separators=(',',':'))
  tag='foa-source-proof:'+hashlib.sha256(binding.encode()).hexdigest()
  setprop(parts['Tag'],'Tags',[tag]);comments.append({'tag':tag,'binding':binding})
  components.TransformBus(bus.Event,'SetLocalTranslation',eid,math.Vector3(*row['host_position']))
  components.TransformBus(bus.Event,'SetLocalRotationQuaternion',eid,math.Quaternion(*row['host_rotation']))
  # This proof records the source's near-unity scale; all positions must stay within the stated 1mm host tolerance.
  assert max(abs(s-1.) for s in row['host_scale'])<1e-7,'Nonuniform scale requires its own host mapping'
  components.TransformBus(bus.Event,'SetLocalUniformScale',eid,1.)
  if 'Mesh' in parts:
   notify=render.MeshComponentNotificationBusHandler();notify.connect(eid)
   notify.add_callback('OnModelReady',lambda args:model_ready.append(True));handlers.append(notify)
   setprop(parts['Mesh'],'Controller|Configuration|Model Asset',model_id)
  editor.ToolsApplicationRequestBus(bus.Broadcast,'EndUndoBatch');created.append(eid);parent=eid;yield .4
 leaf=created[-1];record('checking-native-model-ready');yield from wait(lambda:bool(model_ready))
 value=editor.EditorComponentAPIBus(bus.Broadcast,'GetComponentProperty',parts['Mesh'],'Controller|Configuration|Model Asset')
 assert value.IsSuccess() and value.GetValue()==model_id
 result['checks'].append('native-model-ready-with-exact-model-asset')
 cache_proof=fixture_json('cache-geometry-proof.json');assert cache_proof['status']=='PASSED'
 for product in cache_proof['products']:
  assert hashlib.sha256((cache/product['file']).read_bytes()).hexdigest()==product['sha256'],'Native product changed since geometry verification'
 result['cache_geometry']={k:cache_proof[k] for k in ['positions_bit_equal','oriented_triangle_geometry_equal','normal_uv_rows_bit_equal']}
 actual=cache_proof['local_aabb'];expected=doc['local_aabb'];error=max(abs(actual[k][i]-expected[k][i]) for k in ['min','max'] for i in range(3))
 result['local_bounds']={'expected':expected,'actual':actual,'max_error_metres':error};assert error<1e-4,'Imported mesh axes/units/bounds mismatch'
 result['checks'].append('exact-source-mesh-local-bounds')
 world=components.TransformBus(bus.Event,'GetWorldTM',leaf)
 native_positions=fixture_json('native_positions.private.json')
 transformed=[vec(world.Multiply(math.Vector3(*p))) for p in native_positions]
 expected_world=doc['world_aabb'];actual_world={'min':[min(v[i] for v in transformed) for i in range(3)],'max':[max(v[i] for v in transformed) for i in range(3)]}
 error=max(abs(actual_world[k][i]-expected_world[k][i]) for k in ['min','max'] for i in range(3))
 result['world_bounds']={'expected':expected_world,'actual':actual_world,'max_error_metres':error};assert error<.001,'Hierarchy/basis error exceeds 1mm'
 result['checks'].append('source-hierarchy-transforms-within-1mm')
 editor.ToolsApplicationRequestBus(bus.Broadcast,'SetSelectedEntities',[leaf]);yield .2
 assert editor.ToolsApplicationRequestBus(bus.Broadcast,'GetSelectedEntities')==[leaf]
 result['checks'].append('source-entity-selectable')
 before=vec(components.TransformBus(bus.Event,'GetWorldTranslation',leaf))
 editor.ToolsApplicationRequestBus(bus.Broadcast,'BeginUndoBatch','Move source entity')
 editor.ToolsApplicationRequestBus(bus.Broadcast,'AddDirtyEntity',leaf)
 components.TransformBus(bus.Event,'SetWorldTranslation',leaf,math.Vector3(before[0]+2.,before[1],before[2]))
 editor.ToolsApplicationRequestBus(bus.Broadcast,'EndUndoBatch');yield .4
 general.undo();yield .4;undone=vec(components.TransformBus(bus.Event,'GetWorldTranslation',leaf));assert max(abs(a-b) for a,b in zip(undone,before))<.001
 general.redo();yield .4;moved=vec(components.TransformBus(bus.Event,'GetWorldTranslation',leaf));assert abs(moved[0]-before[0]-2.)<.001
 result['checks'].append('source-entity-move-undo-redo')
 # Renaming proves persisted identity does not depend on its display label.
 editor.ToolsApplicationRequestBus(bus.Broadcast,'BeginUndoBatch','Rename proof object')
 editor.ToolsApplicationRequestBus(bus.Broadcast,'AddDirtyEntity',leaf)
 editor.EditorEntityAPIBus(bus.Event,'SetName',leaf,'Renamed source object')
 editor.ToolsApplicationRequestBus(bus.Broadcast,'EndUndoBatch');yield .4
 record('saving-source-entity');assert general.save_level(),'Save failed';yield 1
 result['moved_world_position']=moved;result['source_bindings']=comments;result['status']='PASSED';record('saved')

job=run()
def step():
 try:
  delay=next(job)
  QtCore.QTimer.singleShot(round(delay*1000),step)
 except StopIteration:
  general.exit_no_prompt()
 except Exception as ex:
  result['error']=str(ex);result['traceback']=traceback.format_exc();record('failed')
  general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
