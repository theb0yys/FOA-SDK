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
import os
import json,time,traceback,hashlib
from pathlib import Path
from PySide6 import QtCore
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.components as components
import azlmbr.math as math
import azlmbr.asset as asset
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

previous=fixture_json('native-entity-import.json');doc=fixture_json('source_entity.private.json')
level_path=root/doc['native_level_relative_path']
if level_path.resolve()!=level_path or not level_path.is_relative_to(root) or level_path.suffix!='.prefab':
 raise RuntimeError('Native proof level must remain in the private fixture')
result={'status':'FAILED','checks':[]};output=root/'native-entity-reopen.json'
def record(stage):
 result['stage']=stage;output.write_text(json.dumps(result,indent=2),encoding='utf-8')
def types(names):return editor.EditorComponentAPIBus(bus.Broadcast,'FindComponentTypeIdsByEntityType',names,entity.EntityType().Game)
def component(eid,tid):
 answer=editor.EditorComponentAPIBus(bus.Broadcast,'GetComponentOfType',eid,tid)
 return answer.GetValue() if answer.IsSuccess() else None
def property(c,path):
 answer=editor.EditorComponentAPIBus(bus.Broadcast,'GetComponentProperty',c,path)
 if not answer.IsSuccess():raise RuntimeError('Failed to read native property')
 return answer.GetValue()
def vec(v):return [v.x,v.y,v.z]
def run():
 assert previous['status']=='PASSED'
 general.idle_enable(True);record('opening-saved-level')
 assert general.open_level(str(level_path))
 yield 1.0
 record('resolving-persisted-source-identities')
 tag_type,mesh_type=types(['Tag','Mesh']);all_entities=entity.SearchBus(bus.Broadcast,'SearchEntities',entity.SearchFilter());resolved=[]
 for expected in previous['source_bindings']:
  tag=expected['tag'];binding=expected['binding']
  assert tag=='foa-source-proof:'+hashlib.sha256(binding.encode()).hexdigest()
  source=json.loads(binding);assert source['source_binding']==doc['source_binding']
  matches=[]
  for eid in all_entities:
   c=component(eid,tag_type)
   if c is not None and tag in property(c,'Tags'):matches.append(eid)
  assert len(matches)==1,'Source identity must resolve to one entity after reopen'
  resolved.append(matches[0])
 assert len(set(str(e) for e in resolved))==len(doc['hierarchy'])
 assert editor.EditorEntityInfoRequestBus(bus.Event,'GetParent',resolved[1])==resolved[0],'Source hierarchy changed'
 result['checks'].append('unique-source-identities-and-parent-preserved')
 result['local_transforms']=[]
 for eid in resolved:
  rotation=components.TransformBus(bus.Event,'GetLocalRotationQuaternion',eid)
  result['local_transforms'].append({'position':vec(components.TransformBus(bus.Event,'GetLocalTranslation',eid)),
   'rotation':[rotation.x,rotation.y,rotation.z,rotation.w],
   'uniform_scale':components.TransformBus(bus.Event,'GetLocalUniformScale',eid)})
 leaf=resolved[-1];name=editor.EditorEntityInfoRequestBus(bus.Event,'GetName',leaf);assert name=='Renamed source object'
 result['checks'].append('identity-independent-of-display-name')
 moved=vec(components.TransformBus(bus.Event,'GetWorldTranslation',leaf));expected=previous['moved_world_position']
 error=max(abs(a-b) for a,b in zip(moved,expected));assert error<.001,'Saved edit changed on reopen'
 result['position']={'expected':expected,'actual':moved,'max_error_metres':error}
 result['checks'].append('saved-two-metre-world-edit-preserved')
 expected_model=asset.AssetCatalogRequestBus(bus.Broadcast,'GetAssetIdByPath',doc['host_asset'],math.Uuid(),False)
 c=component(leaf,mesh_type);assert c is not None and property(c,'Controller|Configuration|Model Asset')==expected_model
 result['checks'].append('exact-source-model-binding-preserved')
 editor.ToolsApplicationRequestBus(bus.Broadcast,'SetSelectedEntities',[leaf]);yield .2
 assert editor.ToolsApplicationRequestBus(bus.Broadcast,'GetSelectedEntities')==[leaf]
 result['checks'].append('reopened-source-object-selectable')
 result['status']='PASSED';result['scope']='One placed renderer; no full-map or game-export claim';record('reopen-complete')
job=run()
def step():
 try:delay=next(job);QtCore.QTimer.singleShot(round(delay*1000),step)
 except StopIteration:general.exit_no_prompt()
 except Exception as ex:
  result['error']=str(ex);result['traceback']=traceback.format_exc();record('failed');general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
