# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native grouped source draws: exact RGB references, lifecycle and bounded admission."""
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
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_scene_assembly_editor import matrix_bits, placement, start_import
from foa_scene_assembly import validate_batch
from foa_scene_render_assembly import prepare_render_batch
import azlmbr.foa as foa

root = Path(os.environ['FOA_MULTIDRAW_ROOT']).resolve(strict=True)
assert not any((p/'.git').exists() for p in (root, *root.parents))
fixture = json.loads((root/'fixture.json').read_text())
reopen = os.environ.get('FOA_MULTIDRAW_REOPEN') == '1'
output = root/('reopen.json' if reopen else 'editor.json'); assert not output.exists()
report = {'status': 'FAILED', 'stage': 'starting', 'captures': [], 'states': [], 'rejections': []}
started = time.monotonic(); handlers = []; ids = {}; serial = 1000


def service(event, *args): return foa.SourceShaderRenderBus(bus.Broadcast, event, *args)
def render(eid, event, *args): return foa.SourceSceneRenderBus(bus.Event, event, eid, *args)
def statistics(): return json.loads(service('GetStatistics'))
def record(stage):
    report['stage'] = stage; report['statistics'] = statistics(); output.write_text(json.dumps(report, indent=2))
def begin(name, eid=None):
    editor.ToolsApplicationRequestBus(bus.Broadcast, 'BeginUndoBatch', name)
    if eid is not None: editor.ToolsApplicationRequestBus(bus.Broadcast, 'AddDirtyEntity', eid)
def end(): editor.ToolsApplicationRequestBus(bus.Broadcast, 'EndUndoBatch')
def visible(eid, value): editor.EditorEntityAPIBus(bus.Event, 'SetVisibilityState', eid, value)
def all_entities(): return entity.SearchBus(bus.Broadcast, 'SearchEntities', entity.SearchFilter())
def source_hash(eid): return hashlib.sha256(placement(eid, 'GetSource').encode()).hexdigest()
def resolve():
    found = {source_hash(e): e for e in all_entities() if placement(e, 'GetSource')}
    for entry in fixture['entries']: ids[entry['name']] = found[entry['source_sha256']]
def native_matrix(eid):
    raw = struct.unpack('<16f', struct.pack('<16I', *matrix_bits(eid))); axis = (0, 2, 1, 3)
    return [raw[axis[r]*4+axis[c]] for r in range(4) for c in range(4)]
def wait(predicate, timeout=120):
    deadline = time.monotonic()+timeout
    while not predicate():
        assert time.monotonic() < deadline, 'Timeout: '+report['stage']
        yield .1


def make(entry, binding=None, shown=True):
    global serial
    begin('Create source draw group')
    parent = editor.ToolsApplicationRequestBus(bus.Broadcast, 'GetCurrentLevelEntityId')
    eid = editor.ToolsApplicationRequestBus(bus.Broadcast, 'CreateNewEntityAtPosition', math.Vector3(), parent); assert eid.IsValid()
    editor.EditorEntityAPIBus(bus.Event, 'SetName', eid, entry['name'])
    descriptor = copy.deepcopy(entry['placement'])
    if binding is not None:
        serial += 1
        if descriptor['schema_version'] == 3: descriptor['identity']['instance_ordinal'] = serial
        else:
            descriptor['identity']['path_id'] = str(serial); descriptor['identity']['gameobject_id'] = str(serial+10000)
    world = struct.unpack('<16f', struct.pack('<16I', *descriptor['source_world_bits']))
    components.TransformBus(bus.Event, 'SetWorldTranslation', eid, math.Vector3(world[3], world[11], world[7]))
    for typ in types:
        assert editor.EditorComponentAPIBus(bus.Broadcast, 'AddComponentsOfType', eid, [typ]).IsSuccess()
    assert placement(eid, 'BindSource', json.dumps(descriptor, separators=(',', ':')))
    visible(eid, shown)
    accepted = render(eid, 'BindDraw', json.dumps(entry['binding'] if binding is None else binding, separators=(',', ':')))
    assert placement(eid, 'CommitAssemblyState'); end()
    return eid, accepted


def delete(eid):
    begin('Remove source draw group'); editor.ToolsApplicationRequestBus(bus.Broadcast, 'DeleteEntityById', eid); end()


def capture(name):
    record('capturing-'+name); path = root/(name+'.ppm'); assert not path.exists()
    result = atom.FrameCaptureRequestBus(bus.Broadcast, 'CaptureScreenshot', str(path)); assert result.IsSuccess()
    done = []; handler = atom.FrameCaptureNotificationBusHandler(); handler.connect(result.GetValue()); handlers.append(handler)
    handler.add_callback('OnFrameCaptureFinished', lambda args: done.append(args))
    yield from wait(lambda: bool(done), 30); assert done[0][0] == atom.FrameCaptureResult_Success; handler.disconnect()
    report['captures'].append({'name': name, 'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})


def compare(name, names):
    yield from wait(lambda: all(render(ids[n], 'GetStatus') == 'READY' for n in names))
    editor.ToolsApplicationRequestBus(bus.Broadcast, 'SetSelectedEntities', []); yield .5
    items = []
    for entry in fixture['entries']:
        if entry['name'] not in names: continue
        eid = ids[entry['name']]
        assert source_hash(eid) == entry['source_sha256']
        assert json.loads(render(eid, 'GetBinding')) == entry['binding']
        for index in range(len(entry['draws'])): items.append({'name': entry['name'], 'draw': index, 'matrix': native_matrix(eid)})
    stats = statistics(); assert stats['submitted_entities'] == len(names) and stats['submitted_entity_draws'] == len(items)
    report['states'].append({'name': name, 'items': items}); yield from capture(name+'-entities')


def references():
    for eid in ids.values(): visible(eid, False)
    yield .4
    entries = {e['name']: e for e in fixture['entries']}
    for state in report['states']:
        descriptor = copy.deepcopy(fixture['base']['draw']); positions = []; uv = []; indices = []
        for item in state['items']:
            source = entries[item['name']]['draws'][item['draw']]['draw']; matrix = item['matrix']
            raw = bytes.fromhex(source['streams'][0]['hex']); vertices = struct.unpack('<'+'f'*(len(raw)//4), raw)
            offset = len(positions)//3
            for i in range(0, len(vertices), 3):
                point = [*vertices[i:i+3], 1.]
                positions.extend(sum(matrix[r*4+c]*point[c] for c in range(4)) for r in range(3))
            raw = bytes.fromhex(source['streams'][1]['hex']); original_uv = struct.unpack('<'+'f'*(len(raw)//4), raw)
            st = entries[item['name']]['uv_transforms'][item['draw']]
            uv.extend(value*st[i % 2]+st[2+i % 2] for i, value in enumerate(original_uv))
            raw = bytes.fromhex(source['indices']); indices.extend(i+offset for i in struct.unpack('<'+'I'*(len(raw)//4), raw))
        descriptor['vertex_count'] = len(positions)//3
        descriptor['streams'][0]['hex'] = struct.pack('<'+'f'*len(positions), *positions).hex()
        descriptor['streams'][1]['hex'] = struct.pack('<'+'f'*len(uv), *uv).hex()
        descriptor['indices'] = struct.pack('<'+'I'*len(indices), *indices).hex()
        assert service('SetDraw', json.dumps(descriptor)) == 'LOADING'
        yield from wait(lambda: service('GetStatus') != 'LOADING'); assert service('GetStatus') == 'READY'; yield .5
        yield from capture(state['name']+'-reference'); service('ClearDraw'); yield .2
    for eid in ids.values(): visible(eid, True)
    yield .3


def live(stats):
    return {key: stats[key] for key in ('entity_draws', 'registered_entities', 'resident_payload_bytes', 'geometry_payload_bytes',
            'constant_payload_bytes', 'unique_geometry_buffers', 'shared_stages', 'sampler_reservations')}


def controls():
    base = fixture['base']; entry = fixture['entries'][0]
    def group(count):
        members = [copy.deepcopy(base) for _ in range(count)]
        for index, member in enumerate(members): member['draw']['sort_key'] = index
        return {'version': 3, 'draws': members}
    bad = []
    for name, binding in [('empty', {'version': 3, 'draws': []}), ('over-member-limit', group(129)),
                          ('nested', {'version': 3, 'draws': [group(1)]}), ('future', {'version': 4, 'draws': [base]}),
                          ('boolean-version', {'version': True, 'draws': [base]}), ('wrong-array', {'version': 3, 'draws': {}})]: bad.append((name, binding))
    for name in ('duplicate-order', 'reverse-order', 'unknown-field', 'late-malformed', 'late-missing-asset', 'oversized'):
        value = group(2)
        if name == 'duplicate-order': value['draws'][1]['draw']['sort_key'] = 0
        if name == 'reverse-order': value['draws'][0]['draw']['sort_key'] = 2
        if name == 'unknown-field': value['extra'] = 1
        if name == 'late-malformed': value['draws'][1]['draw']['indices'] = 'ffffffff'*3
        if name == 'late-missing-asset': value['draws'][1]['draw']['shader'] = 'assets/missing.foashader.azshader'
        if name == 'oversized': value['extra'] = 'x'*(16*1024*1024)
        bad.append((name, value))
    before = live(statistics())
    if 'merged_preparation' in fixture:
        report['merged_placement_rejections'] = []
        for name in ('guid', 'ordinal', 'parent', 'world', 'hash', 'foreign-owner', 'future'):
            descriptor = copy.deepcopy(entry['placement'])
            if name == 'guid': descriptor['identity']['member_guid'] = 'A'*32
            elif name == 'ordinal': descriptor['identity']['instance_ordinal'] = 1000000
            elif name == 'parent': descriptor['source_parent'] = {'serialized_file':'fake', 'path_id':'1'}
            elif name == 'world': descriptor['source_local_bits'][0] ^= 1
            elif name == 'hash': descriptor['identity']['archive_sha256'] = 'x'*64
            elif name == 'foreign-owner': descriptor['identity']['gameobject_id'] = '1'
            else: descriptor['schema_version'] = 4
            begin('Reject invalid merged placement')
            parent = editor.ToolsApplicationRequestBus(bus.Broadcast, 'GetCurrentLevelEntityId')
            eid = editor.ToolsApplicationRequestBus(bus.Broadcast, 'CreateNewEntityAtPosition', math.Vector3(), parent)
            world = struct.unpack('<16f', struct.pack('<16I', *descriptor['source_world_bits']))
            components.TransformBus(bus.Event, 'SetWorldTranslation', eid, math.Vector3(world[3], world[11], world[7]))
            assert editor.EditorComponentAPIBus(bus.Broadcast, 'AddComponentsOfType', eid, [types[0]]).IsSuccess()
            assert not placement(eid, 'BindSource', json.dumps(descriptor)), name
            assert placement(eid, 'GetSource') == ''
            end(); delete(eid); report['merged_placement_rejections'].append(name)
        assert live(statistics()) == before
    for name, binding in bad:
        eid, accepted = make(entry, binding, False); assert not accepted, name
        assert render(eid, 'GetBinding') == ''; delete(eid)
        assert live(statistics()) == before, ('Partial admission', name)
        report['rejections'].append(name)
    # Contract-valid input whose second member fails native shader stream validation.
    value = group(2); value['draws'][1]['draw']['streams'][0]['index'] = 9
    eid, accepted = make(entry, value); assert accepted
    yield from wait(lambda: render(eid, 'GetStatus').startswith('FAILED'))
    report['failed_group'] = statistics()
    yield from compare('failed-member', list(ids))
    delete(eid); yield .5; assert live(statistics()) == before
    report['rejections'].append('late-native-build')
    # Draw capacity is aggregate, not entity count; 128-member groups retain 4-work scheduling.
    fill = []; phase = time.monotonic()
    while statistics()['entity_draws'] < 1024:
        count = min(128, 1024-statistics()['entity_draws']); eid, accepted = make(entry, group(count), False); assert accepted; fill.append(eid)
    yield from wait(lambda: all(render(e, 'GetStatus') == 'READY' for e in fill), 240)
    report['capacity'] = {'statistics': statistics(), 'seconds': time.monotonic()-phase, 'groups': len(fill)}
    eid, accepted = make(entry, group(1), False); assert not accepted; delete(eid); report['rejections'].append('aggregate-draw-limit')
    # Remove alternating groups to exercise dense scheduler index repair within multi-draw ownership.
    for eid in fill[::2]+fill[1::2]: delete(eid)
    yield .5; assert live(statistics()) == before
    # Force failure after earlier group members have reserved new shared geometry.
    def large(index):
        member = copy.deepcopy(base); draw = member['draw']; draw['vertex_count'] = 65536
        positions = bytearray(65536*12); coords = bytearray(65536*8)
        positions[-4:] = struct.pack('<f', float(index+1)); coords[-4:] = struct.pack('<f', float(index+1))
        draw['streams'][0]['hex'] = positions.hex(); draw['streams'][1]['hex'] = coords.hex()
        return member
    fill = []
    for index in range(48):
        eid, accepted = make(entry, large(index), False); assert accepted; fill.append(eid)
    yield from wait(lambda: all(render(e, 'GetStatus') == 'READY' for e in fill), 240)
    prior = live(statistics()); value = {'version': 3, 'draws': [large(100+i) for i in range(4)]}
    for index, member in enumerate(value['draws']): member['draw']['sort_key'] = index
    added_per_draw = 65536*20+sum(len(bytes.fromhex(c['hex'])) for stage in base['draw']['stages'] for c in stage['constants'])
    available = 64*1024*1024-prior['resident_payload_bytes']; assert added_per_draw < available < 4*added_per_draw
    eid, accepted = make(entry, value, False); assert not accepted; delete(eid)
    assert live(statistics()) == prior
    report['late_budget'] = {'before': prior, 'after': live(statistics()), 'member_bytes': added_per_draw}
    report['rejections'].append('late-resident-budget')
    for eid in fill: delete(eid)
    yield .5; assert live(statistics()) == before
    report['after_controls'] = statistics()


def assembly_inputs():
    entry = fixture['render_assembly']; values = []
    for name in ('hierarchy', 'rendering'):
        path = Path(entry[name]['path']).resolve(strict=True)
        assert path.parent == root and path.stat().st_size <= 64*1024*1024
        raw = path.read_bytes(); assert hashlib.sha256(raw).hexdigest() == entry[name]['sha256']; values.append(raw)
    return values


def assembled_import(hierarchy, rendering, control=None):
    record('assembly-'+str(control or 'import'))
    done = []; parent = editor.ToolsApplicationRequestBus(bus.Broadcast, 'GetCurrentLevelEntityId')
    controller = start_import(hierarchy, parent, done.append, render_batch=rendering); job = controller['job']
    if control == 'cancel':
        configure = job.adapter.configure
        def cancel_after_two(eid, row):
            configure(eid, row)
            if len(job.created) == 2: controller['dialog'].cancel()
        job.adapter.configure = cancel_after_two
    if control == 'timeout':
        ready = job.ready; job.ready_timeout = .5; job.native_ready_observed = False
        def hold_ready(eid, row):
            job.native_ready_observed |= ready(eid, row)
            return False
        job.ready = hold_ready
    yield from wait(lambda: bool(done), 150)
    assert len(done) == 1 and done[0] is job and not job.cleanup_errors
    report.setdefault('assembly_jobs', []).append({'control': control, 'status': job.status, 'created': len(job.created),
        'error': job.error, 'peak_step_entities': job.peak_step_entities, 'native_ready_observed': getattr(job, 'native_ready_observed', None)})
    expected = 'CANCELLED' if control == 'cancel' else 'FAILED' if control else 'PASSED'
    assert job.status == expected, (control, job.status, job.error)
    assert job.peak_step_entities <= 32
    if control == 'cancel': assert len(job.created) == 2
    if control == 'timeout': assert job.native_ready_observed and 'timed out' in job.error
    if control is not None: assert all(not editor.ToolsApplicationRequestBus(bus.Broadcast, 'EntityExists', e) for e in job.created)
    yield .4


def assembly_controls():
    hierarchy, rendering = assembly_inputs(); before = live(statistics())
    parent = editor.ToolsApplicationRequestBus(bus.Broadcast, 'GetCurrentLevelEntityId')
    sentinel = editor.ToolsApplicationRequestBus(bus.Broadcast, 'CreateNewEntityAtPosition', math.Vector3(), parent)
    editor.EditorEntityAPIBus(bus.Event, 'SetName', sentinel, 'Unrelated source import sentinel')
    prior_entities = {e.ToString() for e in all_entities()}
    report['assembly_controls'] = []
    report['assembly_failed_history'] = []
    def failed_history(name):
        for action in ('undo', 'redo'):
            record('assembly-'+name+'-'+action)
            getattr(general, action)(); yield .4
            assert live(statistics()) == before and {e.ToString() for e in all_entities()} == prior_entities
        report['assembly_failed_history'].append(name)
    def preflight_bad(h, r, name):
        try: start_import(h, parent, lambda _: (_ for _ in ()).throw(AssertionError('Unexpected import')), render_batch=r)
        except Exception: pass
        else: raise AssertionError('Invalid render assembly accepted: '+name)
        assert {e.ToString() for e in all_entities()} == prior_entities
        report['assembly_controls'].append(name)
    preflight_bad(hierarchy+b' ', rendering, 'stale-hierarchy')
    bad = json.loads(rendering); bad['renderers'][0]['placement_sha256'] = 'f'*64
    preflight_bad(hierarchy, json.dumps(bad).encode(), 'unknown-placement')
    for name in ('late-bind-rejection', 'late-native-failure'):
        bad = json.loads(rendering)
        if name == 'late-bind-rejection': bad['renderers'][1]['binding']['draw']['shader'] = 'assets/missing.foashader.azshader'
        else: bad['renderers'][0]['binding']['draws'][1]['draw']['streams'][0]['index'] = 9
        # Packet source identity remains exact; the native shader/resource consumer rejects this payload.
        raw = prepare_render_batch(hierarchy, [(e['placement_sha256'], e['binding']) for e in bad['renderers']])
        yield from assembled_import(hierarchy, raw, name)
        assert live(statistics()) == before and {e.ToString() for e in all_entities()} == prior_entities
        yield from failed_history(name)
        report['assembly_controls'].append(name)
    for control, name in (('cancel', 'cancel-after-two-created'), ('timeout', 'loading-timeout')):
        yield from assembled_import(hierarchy, rendering, control)
        assert live(statistics()) == before and {e.ToString() for e in all_entities()} == prior_entities
        yield from failed_history(name)
        report['assembly_controls'].append(name)
    yield from assembled_import(hierarchy, rendering)
    resolve(); yield from wait(lambda: all(render(e, 'GetStatus') == 'READY' for e in ids.values()))
    baseline = {n: (source_hash(e), render(e, 'GetBinding'), list(matrix_bits(e))) for n, e in ids.items()}
    prior_entities = {e.ToString() for e in all_entities()}
    preflight_bad(hierarchy, rendering, 'duplicate-import')
    record('assembly-undo')
    general.undo(); yield .4
    assert live(statistics()) == before and editor.ToolsApplicationRequestBus(bus.Broadcast, 'EntityExists', sentinel)
    assert all(not editor.ToolsApplicationRequestBus(bus.Broadcast, 'EntityExists', e) for e in ids.values())
    record('assembly-redo')
    general.redo(); yield .4; resolve()
    report['assembly_redo_bindings'] = {n: {'source': source_hash(e), 'render': hashlib.sha256(render(e, 'GetBinding').encode()).hexdigest(), 'status': render(e, 'GetStatus')} for n, e in ids.items()}
    record('assembly-redo-readiness')
    yield from wait(lambda: all(render(e, 'GetStatus') == 'READY' for e in ids.values()))
    assert {n: (source_hash(e), render(e, 'GetBinding'), list(matrix_bits(e))) for n, e in ids.items()} == baseline
    assert editor.EditorEntityInfoRequestBus(bus.Event, 'GetName', sentinel) == 'Unrelated source import sentinel'
    report['assembly_controls'].append('whole-import-undo-redo')
    report['assembly_ready'] = statistics()


def actual_merged_imports():
    report['merged_source_imports'] = []
    for item in fixture['merged_preparation']['source_batches']:
        path = Path(item['path']); raw = path.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == item['sha256']
        rows = validate_batch(raw); before = {e.ToString() for e in all_entities()}
        parent = editor.ToolsApplicationRequestBus(bus.Broadcast, 'GetCurrentLevelEntityId'); done = []
        controller = start_import(raw, parent, done.append)
        yield from wait(lambda: bool(done), 120)
        job = controller['job']; assert done == [job] and job.status == 'PASSED' and not job.cleanup_errors
        for eid, row in zip(job.created, rows):
            assert matrix_bits(eid) == row.host_world_bits and placement(eid, 'GetSource') == row.descriptor
        report['merged_source_imports'].append({'map': item['map'], 'sha256':item['sha256'], 'entities':len(rows),
                                               'exact_matrix_bits':True, 'peak_step_entities':job.peak_step_entities})
        # This removes the complete import through its own undo transaction.
        general.undo(); yield .4
        assert {e.ToString() for e in all_entities()} == before


def run():
    global types
    general.idle_enable(True)
    general.set_cvar_integer('ed_keepEditorActive', 1)
    assert general.get_cvar('ed_keepEditorActive') == '1'
    report['keep_editor_active'] = 1
    if 'render_assembly' in fixture:
        from foa_scene_render_assembly import __file__ as rendering_module
        from foa_scene_assembly import __file__ as hierarchy_module
        from foa_scene_assembly_editor import __file__ as adapter_module
        modules = [rendering_module, hierarchy_module, adapter_module]
        if 'merged_preparation' in fixture:
            from foa_scene_merged_assembly import __file__ as merged_module
            modules.append(merged_module)
        report['assembly_modules'] = {path: hashlib.sha256(Path(path).read_bytes()).hexdigest() for path in modules}
    level = Path(fixture['level']).resolve(strict=True); project = Path(fixture['project']).resolve(strict=True)
    assert level.is_relative_to(project) and not any((p/'.git').exists() for p in (project, *project.parents))
    assert general.open_level(str(level)); yield 1
    general.set_cvar_integer('r_displayInfo', 0)
    assert general.get_cvar('r_displayInfo') == '0'
    report['viewport_display_info'] = 0
    types = editor.EditorComponentAPIBus(bus.Broadcast, 'FindComponentTypeIdsByEntityType', ['Source scene placement', 'Source scene rendering'], entity.EntityType().Game)
    assert len(types) == 2 and all(not t.IsNull() for t in types)
    if not reopen and 'merged_preparation' in fixture: yield from actual_merged_imports()
    if reopen:
        assert json.loads((root/'editor.json').read_text())['status'] == 'PASSED'
        resolve(); yield from compare('reopened', list(ids)); yield from references()
    else:
        if 'render_assembly' in fixture:
            yield from assembly_controls()
        else:
            for entry in fixture['entries']:
                eid, accepted = make(entry); assert accepted; ids[entry['name']] = eid
        yield from compare('initial', list(ids))
        target = ids['group']; begin('Move grouped object', target)
        components.TransformBus(bus.Event, 'SetWorldTranslation', target, math.Vector3(.1, 0., .1)); end(); yield .3
        yield from compare('moved', list(ids)); general.undo(); yield .3; yield from compare('undo', list(ids))
        general.redo(); yield .3; yield from compare('redo', list(ids))
        begin('Rotate grouped object', target); components.TransformBus(bus.Event, 'SetLocalRotationQuaternion', target, math.Quaternion_CreateRotationY(.15)); end(); yield .3
        yield from compare('rotated', list(ids))
        begin('Scale grouped object', target); components.TransformBus(bus.Event, 'SetLocalUniformScale', target, 1.1); end(); yield .3
        yield from compare('scaled', list(ids)); visible(target, False); yield .3
        yield from compare('hidden', [n for n in ids if n != 'group']); visible(target, True); yield .3
        yield from compare('shown', list(ids)); delete(target); yield .3
        yield from compare('deleted', [n for n in ids if n != 'group']); general.undo(); yield .5; resolve()
        yield from compare('restored', list(ids)); yield from controls(); yield from compare('after-controls', list(ids))
        assert general.save_level(); yield .5
        report['saved'] = {entry['name']: {'source': source_hash(ids[entry['name']]), 'binding': hashlib.sha256(render(ids[entry['name']], 'GetBinding').encode()).hexdigest(),
                           'matrix_bits': list(matrix_bits(ids[entry['name']]))} for entry in fixture['entries']}
        yield from references()
    report['restored'] = {entry['name']: {'source': source_hash(ids[entry['name']]), 'binding': hashlib.sha256(render(ids[entry['name']], 'GetBinding').encode()).hexdigest(),
                           'matrix_bits': list(matrix_bits(ids[entry['name']]))} for entry in fixture['entries']}
    if reopen: assert report['restored'] == json.loads((root/'editor.json').read_text())['saved']
    for eid in ids.values(): delete(eid)
    yield .5
    report['final_release'] = statistics()
    assert all(report['final_release'][k] == 0 for k in ('entity_draws', 'registered_entities', 'retired_draws', 'resident_payload_bytes', 'unique_geometry_buffers', 'shared_stages', 'sampler_reservations'))
    assert report['final_release']['peak_tick_work'] <= 4 and report['final_release']['peak_tick_visits'] <= 64
    report.update(status='PASSED', elapsed_seconds=time.monotonic()-started, complete_campaign_maps='NOT_RUN', game_export='NOT_RUN'); record('complete')


job = run()
def step():
    try:
        assert time.monotonic()-started < 1800, 'Multi-draw test deadline exceeded'
        delay = next(job); QtCore.QTimer.singleShot(round(delay*1000), step)
    except StopIteration: general.exit_no_prompt()
    except Exception as error:
        report.update(error=str(error), traceback=traceback.format_exc()); record('failed'); general.exit_no_prompt()
QtCore.QTimer.singleShot(0, step)
