# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Independent RGB/resource audit for grouped native source draws and saved reopening."""
import argparse
import copy
import hashlib
import json
import math
import struct
from pathlib import Path
from source_scene_sharing_cases import captured, require


def read(path):
    require(path.stat().st_size <= 16*1024*1024, 'Oversized native report.')
    return json.loads(path.read_text(encoding='utf-8-sig'))


def same_frame(actual, reference):
    require(actual.size == reference.size and actual.tobytes() == reference.tobytes(), 'Complete grouped RGB differs.')


def material_inputs(fixture):
    proof = fixture['material_layout_proof']; path = Path(proof['path']).resolve(strict=True)
    require(hashlib.sha256(path.read_bytes()).hexdigest() == proof['sha256'], 'Original shader layout evidence changed.')
    layout_proof = read(path)['source_program_bindings'][0]; stage = layout_proof['stages'][1]
    field = stage['layouts']['UnityPerMaterial']['fields']['_UnlitColorMap_ST']
    require(field['name'] == '_UnlitColorMap_ST' and field['bytes'] == 16 and not field['matrix'], 'Unqualified material UV field.')
    resource = next(r for r in stage['resources'] if r['namespace'] == 'cb' and r['signature'][1] == 'UnityPerMaterial')
    base = next(c for c in fixture['base']['draw']['stages'][1]['constants'] if c['slot'] == resource['index'])
    payloads = set()
    for entry in fixture['entries']:
        for i, member in enumerate(entry['draws']):
            expected = bytearray.fromhex(base['hex']); struct.pack_into('<4f', expected, field['index'], *entry['uv_transforms'][i])
            actual = next(c for c in member['draw']['stages'][1]['constants'] if c['slot'] == resource['index'])
            require(bytes.fromhex(actual['hex']) == expected, 'Per-draw material constants differ from qualified field data.')
            payloads.add(actual['hex'])
    require(len(payloads) == 2, 'Distinct grouped material inputs were not exercised.')


def resources(report, fixture):
    capacity = report['capacity']['statistics']; baseline = report['after_controls']
    unique = set()
    for entry in fixture['entries']:
        for item in entry['draws']:
            draw = item['draw']; unique.update(bytes.fromhex(s['hex']) for s in draw['streams']); unique.add(bytes.fromhex(draw['indices']))
    constant_bytes = sum(len(bytes.fromhex(c['hex'])) for stage in fixture['base']['draw']['stages'] for c in stage['constants'])
    require(capacity['entity_draws'] == 1024 and capacity['registered_entities'] == 11 and report['capacity']['groups'] == 8,
            'Aggregate grouped capacity was not reached.')
    require(capacity['geometry_payload_bytes'] == sum(map(len, unique)) and capacity['unique_geometry_buffers'] == len(unique),
            'Group geometry was duplicated or lost.')
    require(capacity['constant_payload_bytes'] == 1024*constant_bytes, 'Grouped mutable constants are incomplete.')
    require(capacity['shared_stages'] == capacity['sampler_reservations'] == 2, 'Distinct material stages were merged or duplicated.')
    require(capacity['loading_entity_draws'] == capacity['failed_entity_draws'] == capacity['dirty_entity_draws'] == 0, 'Capacity group is incomplete.')
    require(baseline['registered_entities'] == 3 and baseline['entity_draws'] == 4 and baseline['constant_payload_bytes'] == 4*constant_bytes
            and baseline['geometry_payload_bytes'] == sum(map(len, unique)), 'Group removal leaked or dropped resources.')
    failed = report['failed_group']
    require(failed['registered_entities'] == 4 and failed['entity_draws'] == 6 and failed['failed_entity_draws'] > 0, 'Late native failure was not executed.')
    budget = report['late_budget']; available = 64*1024*1024-budget['before']['resident_payload_bytes']
    require(budget['before'] == budget['after'] and budget['before']['entity_draws'] == 52,
            'Late group admission changed live resources.')
    require(budget['member_bytes'] < available < 4*budget['member_bytes'], 'Group budget failed before partial reservation was possible.')
    for state in (capacity, baseline, failed, report['final_release']):
        require(state['peak_tick_work'] <= 4 and state['peak_tick_visits'] <= 64, 'Grouped scheduling exceeded work bounds.')
        require(state['resident_payload_bytes'] == state['geometry_payload_bytes']+state['constant_payload_bytes'], 'Resident accounting differs.')
    for key in ('entity_draws', 'registered_entities', 'retired_draws', 'resident_payload_bytes', 'unique_geometry_buffers', 'shared_stages', 'sampler_reservations'):
        require(report['final_release'][key] == 0, 'Group cleanup leaked '+key)


def samples(image, state, fixture):
    entries = {e['name']: e for e in fixture['entries']}; count = 0
    colors = ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0))
    points = ((-.4, .4, .5, 1.), (.4, .4, .5, 1.), (-.4, -.4, .5, 1.), (.4, -.4, .5, 1.))
    for item in state['items']:
        entry = entries[item['name']]; local = entry['locals'][item['draw']]; world = item['matrix']
        st = entry['uv_transforms'][item['draw']]; expected = []
        for u, v in ((.25, .25), (.75, .25), (.25, .75), (.75, .75)):
            if entry['flips'][item['draw']]: v = 1.-v
            u = (u*st[0]+st[2]) % 1.; v = (v*st[1]+st[3]) % 1.
            expected.append(colors[2*math.floor(v*2)+math.floor(u*2)])
        for point, color in zip(points, expected):
            source = [sum(local[r*4+c]*point[c] for c in range(4)) for r in range(4)]
            clip = [sum(world[r*4+c]*source[c] for c in range(4)) for r in range(4)]
            x = math.floor((clip[0]/clip[3]*.5+.5)*image.width); y = math.floor((.5-clip[1]/clip[3]*.5)*image.height)
            for dx, dy in ((-1, -1), (0, 0), (1, 1)):
                require(0 <= x+dx < image.width and 0 <= y+dy < image.height, 'Sample escaped frame.')
                require(image.getpixel((x+dx, y+dy)) == color, 'Independent grouped RGB sample differs.')
                count += 1
    return count


def assembly_results(initial, reopened, merged=False):
    require(initial['assembly_controls'] == ['stale-hierarchy', 'unknown-placement', 'late-bind-rejection', 'late-native-failure', 'cancel-after-two-created', 'loading-timeout', 'duplicate-import', 'whole-import-undo-redo'], 'Native rendered assembly controls incomplete.')
    require(initial['assembly_failed_history'] == ['late-bind-rejection', 'late-native-failure', 'cancel-after-two-created', 'loading-timeout'], 'Failed import undo/redo resurrected entities or was not tested.')
    jobs = initial['assembly_jobs']
    require([j['status'] for j in jobs] == ['FAILED', 'FAILED', 'CANCELLED', 'FAILED', 'PASSED'], 'Native assembly transaction outcomes differ.')
    require(jobs[2]['created'] == 2 and jobs[3]['native_ready_observed'] and 'timed out' in jobs[3]['error'], 'Cancellation/timeout were not executed after creation.')
    require(all(j['peak_step_entities'] <= 32 for j in jobs), 'Native assembly exceeded operation bounds.')
    require(initial['assembly_ready']['entity_draws'] == 4 and initial['assembly_ready']['registered_entities'] == 3, 'Native renderer assembly is incomplete.')
    for run in (initial, reopened):
        require(run['keep_editor_active'] == 1, 'Background native testing was not enabled.')
        require(len(run['assembly_modules']) == (4 if merged else 3), 'Native assembly module inventory is incomplete.')
        for path, sha in run['assembly_modules'].items():
            require(hashlib.sha256(Path(path).read_bytes()).hexdigest() == sha, 'Native assembly module changed.')


def audit(root):
    fixture = read(root/'fixture.json'); initial = read(root/'editor.json'); reopened = read(root/'reopen.json')
    require(initial['status'] == reopened['status'] == 'PASSED' and initial['stage'] == reopened['stage'] == 'complete', 'Native grouped checks incomplete.')
    material_inputs(fixture); resources(initial, fixture)
    if 'render_assembly' in fixture: assembly_results(initial, reopened, 'merged_preparation' in fixture)
    if 'merged_preparation' in fixture:
        require(initial['merged_placement_rejections'] == ['guid','ordinal','parent','world','hash','foreign-owner','future'], 'Merged native rejection controls incomplete.')
        expected_batches = fixture['merged_preparation']['source_batches']
        actual_batches = initial['merged_source_imports']
        require(len(actual_batches) == len(expected_batches) > 0 and {v['map'] for v in actual_batches} == {'hos','cuanacht','forlorn','sarras'}, 'Real merged source selections incomplete.')
        for actual, expected_batch in zip(actual_batches, expected_batches):
            require(actual['map'] == expected_batch['map'] and actual['sha256'] == expected_batch['sha256']
                    and actual['entities'] == expected_batch['entities'] and actual['exact_matrix_bits']
                    and actual['peak_step_entities'] <= 32, 'Real merged source native placement differs.')
            require(hashlib.sha256(Path(expected_batch['path']).read_bytes()).hexdigest() == actual['sha256'], 'Real merged batch changed.')
    require(initial['saved'] == reopened['restored'], 'Saved grouped bindings/placements changed.')
    require([e['binding']['version'] for e in fixture['entries']] == [3, 1, 2], 'Saved reader coverage differs.')
    require(len(initial['rejections']) == len(set(initial['rejections'])) == 15, 'Malformed/failure controls incomplete.')
    expected = ['initial', 'moved', 'undo', 'redo', 'rotated', 'scaled', 'hidden', 'shown', 'deleted', 'restored', 'failed-member', 'after-controls']
    require([s['name'] for s in initial['states']] == expected and [s['name'] for s in reopened['states']] == ['reopened'], 'Missing native edit states.')
    pairs = []; total = 0
    for report in (initial, reopened):
        require(report['complete_campaign_maps'] == report['game_export'] == 'NOT_RUN', 'Unsupported map/runtime promotion.')
        require(report['viewport_display_info'] == 0, 'Diagnostic overlay was not disabled.')
        for key in ('entity_draws', 'registered_entities', 'retired_draws', 'resident_payload_bytes', 'unique_geometry_buffers', 'shared_stages', 'sampler_reservations'):
            require(report['final_release'][key] == 0, 'Final native release leaked.')
        captures = {c['name']: c for c in report['captures']}; require(len(captures) == len(report['captures']) == 2*len(report['states']), 'Incomplete or duplicate captures.')
        for state in report['states']:
            expected_items = [(e['name'], i) for e in fixture['entries'] if e['name'] != 'group' or state['name'] not in ('hidden', 'deleted') for i in range(len(e['draws']))]
            require([(v['name'], v['draw']) for v in state['items']] == expected_items, 'Subdraw multiplicity changed.')
            actual, reference = (captured(root, captures[state['name']+'-'+suffix]) for suffix in ('entities', 'reference'))
            same_frame(actual, reference)
            count = samples(actual, state, fixture)+samples(reference, state, fixture); total += count
            pairs.append({'state': state['name'], 'extent': list(actual.size), 'sampled_pixels': count, 'complete_rgb_equal': True})
    negatives = []
    for name, key, field, value in [('capacity', 'capacity', 'entity_draws', 11), ('missing-constants', 'capacity', 'constant_payload_bytes', 0),
                                  ('merged-material-stage', 'capacity', 'shared_stages', 1),
                                  ('tick-overrun', 'capacity', 'peak_tick_work', 5), ('stage-leak', 'final_release', 'shared_stages', 1)]:
        bad = copy.deepcopy(initial); target = bad[key]['statistics'] if key == 'capacity' else bad[key]; target[field] = value
        try: resources(bad, fixture)
        except ValueError: negatives.append(name)
        else: raise ValueError('Corrupt grouped evidence accepted: '+name)
    if 'render_assembly' in fixture:
        for name in ('missing-assembly-control', 'failed-import-history', 'false-ready', 'missing-timeout', 'assembly-step-overrun'):
            bad = copy.deepcopy(initial)
            if name == 'missing-assembly-control': bad['assembly_controls'].pop()
            elif name == 'failed-import-history': bad['assembly_failed_history'].pop()
            elif name == 'false-ready': bad['assembly_jobs'][-1]['status'] = 'WAITING'
            elif name == 'missing-timeout': bad['assembly_jobs'][3]['native_ready_observed'] = False
            else: bad['assembly_jobs'][0]['peak_step_entities'] = 33
            try: assembly_results(bad, reopened)
            except ValueError: negatives.append(name)
            else: raise ValueError('Corrupt assembly evidence accepted: '+name)
    bad_fixture = copy.deepcopy(fixture); bad_fixture['entries'][0]['uv_transforms'][1] = [1, 1, 0, 0]
    try: material_inputs(bad_fixture)
    except ValueError: negatives.append('lost-material-state')
    else: raise ValueError('Lost grouped material state accepted.')
    captures = {c['name']: c for c in initial['captures']}
    for name, source in [('wrong-pose', 'moved-entities'), ('missing-group', 'hidden-entities')]:
        try: same_frame(captured(root, captures[source]), captured(root, captures['initial-reference']))
        except ValueError: negatives.append(name)
        else: raise ValueError('Wrong grouped image accepted: '+name)
    return {'status': 'PASSED', 'scope': 'Explicit native multi-draw entities with the qualified original Unlit pair and synthetic geometry',
            'rendered_assembly_controls': initial.get('assembly_controls', []), 'distinct_material_inputs': 2, 'pairs': pairs, 'sampled_pixels': total, 'native_rejections': len(initial['rejections']), 'negative_controls': negatives,
            'capacity': initial['capacity'], 'late_budget': initial['late_budget'], 'fresh_reopen': 'PASSED',
            'saved_binding_versions': [1, 2, 3], 'complete_campaign_maps': 'NOT_RUN', 'game_export': 'NOT_RUN'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--root', type=Path, required=True); parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(); root = args.root.resolve(strict=True); output = args.output.resolve()
    require(not output.exists() and not any((p/'.git').exists() for path in (root, output.parent) for p in (path, *path.parents)), 'Private new evidence output required.')
    result = audit(root)
    with output.open('x', encoding='utf-8') as stream: json.dump(result, stream, indent=2)
    print(json.dumps(result))
