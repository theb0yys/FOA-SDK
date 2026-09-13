# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native four-map terrain loading, binding readback, captures and saved-level acceptance."""
import copy
import hashlib
import shutil
import json
import os
from pathlib import Path
import sys
import time
import traceback
from PySide6 import QtCore, QtWidgets, QtGui
import azlmbr.atom as atom
import azlmbr.bus as bus
import azlmbr.components as components
import azlmbr.math as native_math
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.foa as foa
import azlmbr.legacy.general as general
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_campaign_terrain_editor as native
from foa_campaign_terrain import IDENTITY, validate

root = Path(os.environ['FOA_CAMPAIGN_TERRAIN_ROOT']).resolve(strict=True)
assert not any((p/'.git').exists() for p in (root, *root.parents))
fixture = json.loads((root/'production-four-map-v2.private.json').read_bytes())
reopen = os.environ.get('FOA_CAMPAIGN_TERRAIN_REOPEN') == '1'
ui_mode = os.environ.get('FOA_CAMPAIGN_TERRAIN_UI') == '1'
controls = os.environ.get('FOA_CAMPAIGN_TERRAIN_CONTROLS') == '1'
output = root/os.environ.get('FOA_CAMPAIGN_TERRAIN_REPORT', 'native-terrain-v1.json')
assert not output.exists()
report = dict(status='FAILED', stage='starting', maps=[], captures=[])
handlers = []; started = time.monotonic()


def record(stage):
    report.update(stage=stage, elapsed_seconds=time.monotonic()-started,
                  current_level=general.get_current_level_path())
    output.write_text(json.dumps(report, indent=2))


def stats(): return json.loads(foa.SourceShaderRenderBus(bus.Broadcast, 'GetStatistics'))


def wait(predicate, timeout=180):
    end = time.monotonic()+timeout
    while not predicate():
        assert time.monotonic() < end, 'Timeout: '+report['stage']
        if native._active is not None:
            report['controller'] = dict(stage=native._active.stage, created=len(native._active.created), current_level_name=general.get_current_level_name(),prefab_owner=getattr(native._active,'last_owner',''))
            record(report['stage'])
        yield .1


def wait_draws(ids):
    deadline=time.monotonic()+90
    while True:
        states=[native.rendering(e,'GetStatus') for e in ids]
        report['draw_states']={state:states.count(state) for state in set(states)}
        record(report['stage'])
        assert all(state in ('READY','LOADING') for state in states),report['draw_states']
        if all(state=='READY' for state in states):return
        assert time.monotonic()<deadline,report['draw_states']
        yield .2


def audit(doc):
    rows = {}
    for eid in entity.SearchBus(bus.Broadcast, 'SearchEntities', entity.SearchFilter()):
        source = native.placement(eid, 'GetSource')
        if source:
            assert source not in rows, 'Duplicate native source terrain group'
            rows[source] = eid
    assert len(rows) == len(doc['groups']), (len(rows), len(doc['groups']))
    bound=[]
    for row in doc['groups']:
        source=json.dumps(row['placement'],separators=(',', ':')); draw=json.dumps(row['rendering'],separators=(',', ':'))
        eid=rows[source]
        assert native.rendering(eid,'GetBinding') == draw, 'Native geometry changed'
        assert list(native.placement(eid,'GetWorldMatrixBits')) == IDENTITY, 'Native placement changed'
        bound.append(eid)
    return bound


def capture(name):
    editor.ToolsApplicationRequestBus(bus.Broadcast,'SetSelectedEntities',[]); yield .3
    path=root/(output.stem+'-'+name+'.ppm'); assert not path.exists()
    outcome=atom.FrameCaptureRequestBus(bus.Broadcast,'CaptureScreenshot',str(path)); assert outcome.IsSuccess()
    done=[]; handler=atom.FrameCaptureNotificationBusHandler(); handler.connect(outcome.GetValue()); handlers.append(handler)
    handler.add_callback('OnFrameCaptureFinished',lambda args:done.append(args))
    yield from wait(lambda:bool(done),30); assert done[0][0]==atom.FrameCaptureResult_Success; handler.disconnect()
    image=QtGui.QImage(str(path)); assert not image.isNull()
    terrain_pixels=sum(1 for y in range(0,image.height(),4) for x in range(0,image.width(),4)
        if (lambda c:c.green()>c.red()+2 and c.green()>c.blue()+1)(image.pixelColor(x,y)))
    assert terrain_pixels>500, 'Native frame contains no visible terrain: '+str(terrain_pixels)
    report['captures'].append(dict(map=name,path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),terrain_pixels_sampled=terrain_pixels))



def choose_map(pane, key):
    pane.findChild(QtWidgets.QPushButton,'TerrainEditVanillaMap').click(); yield .1
    dialog=pane.findChild(QtWidgets.QDialog,'TerrainCampaignChooser'); assert dialog and dialog.isVisible()
    choices=dialog.findChild(QtWidgets.QComboBox,'TerrainCampaignSelection')
    assert {choices.itemData(i) for i in range(choices.count())}=={'hos','cuanacht','forlorn','sarras'}
    choices.setCurrentIndex(next(i for i in range(choices.count()) if choices.itemData(i)==key))
    dialog.findChild(QtWidgets.QDialogButtonBox).button(QtWidgets.QDialogButtonBox.Ok).click(); yield .2


def cancel_worker(pane):
    record('cancelling-source-worker')
    before={p.name for p in (root/'Staging/CampaignTerrain').iterdir() if p.is_dir()}
    level=general.get_current_level_path(); count=stats()['entity_draws']
    yield from choose_map(pane,'hos'); yield 1
    cancel=pane.findChild(QtWidgets.QPushButton,'TerrainImportCancel'); assert cancel.isEnabled()
    start=time.monotonic(); cancel.click(); yield from wait(lambda:not cancel.isEnabled(),15)
    message=pane.findChild(QtWidgets.QLabel,'TerrainImportStatus').text(); assert 'cancel' in message.lower(),message
    fresh=[p for p in (root/'Staging/CampaignTerrain').iterdir() if p.is_dir() and p.name not in before]
    assert len(fresh)==1 and (fresh[0]/'cancel.flag').exists()
    assert not (fresh[0]/'native.request.json').exists()
    assert general.get_current_level_path()==level and stats()['entity_draws']==count
    report['worker_cancel']=dict(status='PASSED',seconds=time.monotonic()-start,message=message)


def camera_and_rejections(doc, ids):
    record('camera-and-native-binding-controls')
    general.set_cvar_integer('r_DisplayInfo',0)
    eid=ids[0]; original=native.placement(eid,'GetSource')
    editor.ToolsApplicationRequestBus(bus.Broadcast,'BeginUndoBatch','Terrain camera acceptance fixture')
    parent=editor.ToolsApplicationRequestBus(bus.Broadcast,'GetCurrentLevelEntityId')
    probe=editor.ToolsApplicationRequestBus(bus.Broadcast,'CreateNewEntityAtPosition',native_math.Vector3(),parent)
    types=editor.EditorComponentAPIBus(bus.Broadcast,'FindComponentTypeIdsByEntityType',
        ['Source scene placement','Source scene rendering'],entity.EntityType().Game)
    assert editor.EditorComponentAPIBus(bus.Broadcast,'AddComponentsOfType',probe,types).IsSuccess()
    rejected=[]
    for field,value in [('instance_count',0),('archive_sha256','bad'),('first_instance',-1)]:
        changed=copy.deepcopy(doc['groups'][0]['placement']); changed['identity'][field]=value
        assert not native.placement(probe,'BindSource',json.dumps(changed)),field
        assert native.placement(probe,'GetSource')==''
        rejected.append(field)
    assert native.placement(probe,'BindSource',original)
    mixed=os.environ.get('FOA_CAMPAIGN_TERRAIN_MIXED_CAMERA')=='1'
    if mixed:
        editor.EditorEntityAPIBus(bus.Event,'SetVisibilityState',probe,False)
        record('mixed-camera-layout'); yield from capture('camera-original')
        changed=copy.deepcopy(doc['groups'][0]['rendering'])
        changed['draw']['shader']='assets/foa_terrain_shape/mixed-camera.foashader.azshader'
        changed['draw']['stages'][0]['constants'][1]['hex']=bytes(80).hex()
        assert native.rendering(probe,'BindDraw',json.dumps(changed))
        yield from wait_draws([probe])
        editor.EditorEntityAPIBus(bus.Event,'SetVisibilityState',eid,False)
        editor.EditorEntityAPIBus(bus.Event,'SetVisibilityState',probe,True); yield .5
        yield from capture('camera-mixed')
        assert report['captures'][-2]['sha256']==report['captures'][-1]['sha256'], 'Mixed camera changed the frame'
    before=stats(); position=general.get_current_view_position()
    for step in range(5):
        general.set_current_view_position(position.x+(step+1)*3,position.y,position.z); yield .3
        current=stats()
        assert current['dirty_entity_draws']==0 and current['submitted_entity_draws']==len(ids),current
        assert current['geometry_buffer_builds']==before['geometry_buffer_builds']
    after=stats(); delta=after['shared_camera_buffer_updates']-before['shared_camera_buffer_updates']
    assert 5<=delta<=10,(before,after)
    native.frame(doc); yield .5
    editor.ToolsApplicationRequestBus(bus.Broadcast,'DeleteEntityById',probe)
    editor.EditorEntityAPIBus(bus.Event,'SetVisibilityState',eid,True); yield .5
    editor.ToolsApplicationRequestBus(bus.Broadcast,'EndUndoBatch')
    if mixed:
        yield from capture('camera-restored')
        assert report['captures'][-1]['sha256']==report['captures'][-3]['sha256'], 'Camera restore changed the frame'
    assert audit(doc)==ids
    assert general.save_level()
    report['camera_controls']=dict(status='PASSED',view_changes=5,shared_uploads=delta,
        geometry_rebuilt=0,rejected_native_identity_fields=rejected,mixed_layout='PASSED' if mixed else 'NOT_RUN')


def cancel_assembly():
    record('cancelling-partial-native-assembly')
    entry=fixture[0]; previous=Path(entry['request']); old=json.loads(previous.read_bytes())
    prepared=json.loads(Path(old['prepared']).read_bytes())
    op=root/'Staging/CampaignTerrain'/(output.stem+'-cancel-assembly'); op.mkdir()
    packet=op/'terrain.private.json'; shutil.copyfile(prepared['packet'],packet)
    prepared['packet']=str(packet); (op/'prepared.json').write_text(json.dumps(prepared))
    request=op/'native.request.json'; old['prepared']=str(op/'prepared.json'); request.write_text(json.dumps(old))
    saved={row['level']:hashlib.sha256(Path(row['level']).read_bytes()).hexdigest() for row in report['maps']}
    operation=native.start(str(request))
    yield from wait(lambda:len(operation.created)>=8 or operation.finished,90)
    assert not operation.finished and len(operation.created)>=8
    created=list(operation.created); start=time.monotonic()
    cancel=next(w for w in operation.dialog.findChildren(QtWidgets.QPushButton) if w.text()=='Cancel'); cancel.click()
    yield from wait(lambda:operation.finished,30)
    result=json.loads(Path(str(request)+'.result.json').read_bytes())
    assert result['status']=='failed' and 'cancel' in result['message'].lower(),result
    assert not operation.created
    yield from wait(lambda:stats()['entity_draws']==0 and stats()['resident_payload_bytes']==0,30)
    assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in saved.items())
    assert general.save_level() # Save only the new empty test level after rollback.
    report['native_cancel']=dict(status='PASSED',created_before_cancel=len(created),seconds=time.monotonic()-start,
        original_levels_unchanged=True,remaining_draws=stats()['entity_draws'])


def run():
    general.idle_enable(True); yield 3
    previous=json.loads((root/os.environ['FOA_CAMPAIGN_TERRAIN_PREVIOUS']).read_bytes()) if reopen else None
    if reopen: assert previous['status']=='PASSED'
    if ui_mode:
        general.open_pane('FOA Development Hub'); yield 1
        general.open_pane('Heightmap Importer')
        yield from wait(lambda:any(w.objectName()=='FoaHeightmapImporter' for w in QtWidgets.QApplication.allWidgets()),30)
        pane=next(w for w in QtWidgets.QApplication.allWidgets() if w.objectName()=='FoaHeightmapImporter')
        button=pane.findChild(QtWidgets.QPushButton,'TerrainEditVanillaMap')
        record('waiting-importer-provider')
        for attempt in range(15):
            if button.isEnabled(): break
            report['ui_status']=pane.findChild(QtWidgets.QLabel,'TerrainImportStatus').text()
            report['ui_labels']=[w.text() for w in pane.findChildren(QtWidgets.QLabel)]
            record('waiting-importer-provider'); pane.grab().save(str(root/(output.stem+'-waiting.png')))
            pane.findChild(QtWidgets.QPushButton,'TerrainRefresh').click(); yield 2
        assert button.isEnabled(), report.get('ui_status')
        report['four_map_ui']='OPEN'
        if controls: yield from cancel_worker(pane)
    for entry in fixture:
        key=entry['map']; request=Path(entry['request']); prepared=json.loads(Path(json.loads(request.read_bytes())['prepared']).read_bytes())
        raw=Path(prepared['packet']).read_bytes(); assert hashlib.sha256(raw).hexdigest()==prepared['packet_sha256']; doc=validate(raw)
        record('opening-'+key)
        if ui_mode:
            before={p.name for p in (root/'Staging/CampaignTerrain').iterdir() if p.is_dir()}
            yield from choose_map(pane,key)
            cancel=pane.findChild(QtWidgets.QPushButton,'TerrainImportCancel')
            yield from wait(lambda:not cancel.isEnabled(),960)
            message=pane.findChild(QtWidgets.QLabel,'TerrainImportStatus').text(); report['ui_message']=message
            fresh=[p for p in (root/'Staging/CampaignTerrain').iterdir() if p.is_dir() and p.name not in before]
            assert len(fresh)==1, (message,[str(p) for p in fresh])
            request=fresh[0]/'native.request.json'
            assert Path(str(request)+'.result.json').exists(),message
            result=json.loads(Path(str(request)+'.result.json').read_bytes()); assert result['status']=='complete',result
            assert (fresh[0]/'terrain.private.json').read_bytes()==raw, 'UI prepared a different source terrain packet'
            report['four_map_ui']='PASSED'
        elif reopen:
            old=next(row for row in previous['maps'] if row['map']==key)
            assert general.open_level(old['level']); yield 1; result=dict(level=old['level'])
        else:
            operation=native.start(str(request)); yield from wait(lambda:operation.finished,300)
            result=json.loads(Path(str(request)+'.result.json').read_bytes()); assert result['status']=='complete',result
        ids=audit(doc); native.frame(doc); record('waiting-draws-'+key)
        yield from wait_draws(ids); yield 1
        current=stats(); assert current['entity_draws']==len(ids), current
        report['maps'].append(dict(map=key,level=result['level'],groups=len(ids),source_draws=doc['source_draw_count'],
            binding_readback='PASSED', placement_readback='PASSED', statistics=current))
        record('capturing-'+key); yield from capture(key)
        assert general.save_level(); yield .2
        assert audit(doc)==ids
        record('saved-'+key)
        if controls and key=='hos': yield from camera_and_rejections(doc,ids)
    if controls: yield from cancel_assembly()
    if ui_mode:
        pane.grab().save(str(root/(output.stem+'-pane.png')))
    assert len({r['sha256'] for r in report['captures'] if r['map'] in native.NAMES})==4, 'Map frames are identical'
    report.update(status='PASSED', original_material_rendering='NOT_RUN', game_export='NOT_RUN')
    record('complete')


job=run()

def step():
    try:
        assert time.monotonic()-started<1800, 'Native campaign acceptance deadline exceeded'
        delay=next(job); QtCore.QTimer.singleShot(round(delay*1000),step)
    except StopIteration:
        if os.environ.get('FOA_CAMPAIGN_TERRAIN_KEEP_OPEN')!='1': general.exit_no_prompt()
    except Exception as error:
        report.update(error=str(error),traceback=traceback.format_exc()); record('failed'); general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
