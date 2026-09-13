# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Audit private native hierarchy import/reopen against the captured source batches."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_scene_assembly import MAX_BATCH_BYTES, encode, validate_batch


def require(value, message):
    if not value: raise ValueError(message)


def read_json(path, limit=16*1024*1024):
    require(path.stat().st_size <= limit, 'Audit input exceeds bounds.')
    return json.loads(path.read_text(encoding='utf-8-sig'))


def expected_state(raw):
    rows = validate_batch(raw); records = []
    for row in rows:
        records.append({'source': hashlib.sha256(row.descriptor.encode()).hexdigest(), 'parent': row.parent,
                        'name': row.name, 'visible': row.active_in_hierarchy, 'matrix_bits': row.host_world_bits})
    return {'entities': len(rows), 'state_sha256': hashlib.sha256(encode(records)).hexdigest()}


def check_reports(fixture, expected, initial, reopened):
    names = [s['name'] for s in fixture['scenes']]
    require(len(names) == 16 and len(set(names)) == 16, 'Four complete source cohorts are required.')
    require({name.split('-')[0] for name in names} == {'hos', 'cuanacht', 'forlorn', 'sarras'}, 'A campaign is missing.')
    for report in (initial, reopened):
        require(report.get('status') == 'PASSED' and report.get('stage') == 'complete', 'Native run is incomplete or failed.')
        scenes = report.get('scenes', [])
        require(len(scenes) == len(names) and [s['name'] for s in scenes] == names, 'Native scene coverage/order differs.')
        for scene in scenes:
            require(scene.get('saved') == expected[scene['name']], 'Native source identity, hierarchy, name, visibility or matrix differs.')
        require(report.get('entities') == sum(s['entities'] for s in expected.values()), 'Native entity accounting differs.')
        require(type(report.get('subnormal_coefficients')) is int and report['subnormal_coefficients'] > 0, 'Native subnormal coefficient proof missing.')
        require(report.get('complete_rendered_scenes') == report.get('game_export') == 'NOT_RUN', 'Unsupported completion promotion.')
        require(report.get('legacy_readback') == {'matrices': fixture['legacy']['matrices'],
                'binding_hashes': sorted(fixture['legacy']['bindings'])}, 'Version-one native readback differs.')
    for scene, request in zip(initial['scenes'], fixture['scenes']):
        require(type(scene['peak_step_entities']) is int and 0 < scene['peak_step_entities'] <= 32, 'Native step budget violated.')
        require(scene['complete_hierarchy'] == request['complete_hierarchy'], 'Partial hierarchy promoted to complete.')
    require(initial.get('legacy_version_one') == reopened.get('legacy_version_one') == {'status': 'PASSED', 'bindings': 3, 'edited_matrices': 2},
            'Version-one saved compatibility was not verified.')
    controls = initial.get('controls', [])
    require([c['name'] for c in controls] == ['modal-cancellation-rollback', 'late-native-failure-rollback',
            'single-operation-undo-redo', 'duplicate-preflight-no-mutation', 'cross-bundle-parent-rejected', 'native-anchor-negatives', 'rounded-anchor-edit-undo-redo'], 'Native controls incomplete.')
    require(all(c['status'] == 'PASSED' for c in controls) and controls[0]['created'] > 0 and controls[1]['created'] == 7
            and controls[5].get('rejected') == 3,
            'Native rollback/control did not execute.')


def audit(root):
    require(not any((p/'.git').exists() for p in (root, *root.parents)), 'Private evidence storage required.')
    fixture = read_json(root/'fixture.json'); expected = {}; artifacts = []
    for item in fixture['scenes']:
        path = Path(item['batch']); require(path.stat().st_size <= MAX_BATCH_BYTES, 'Batch exceeds bounds.')
        raw = path.read_bytes(); require(hashlib.sha256(raw).hexdigest() == item['batch_sha256'], 'Source batch changed.')
        expected[item['name']] = expected_state(raw)
        document = json.loads(raw)
        require(item['name'] == document['source']['map']+'-'+document['source']['role'], 'Source scene selection differs.')
        require(item['complete_hierarchy'] == (len(document['entities']) == document['source']['scene_nodes']), 'Incorrect hierarchy completeness.')
        for artifact in (path, Path(item['level'])):
            require(artifact.is_file(), 'Native saved level is absent.')
            artifacts.append({'path': str(artifact), 'sha256': hashlib.sha256(artifact.read_bytes()).hexdigest(), 'bytes': artifact.stat().st_size})
    initial, reopened = read_json(root/'editor.json'), read_json(root/'reopen.json')
    check_reports(fixture, expected, initial, reopened)
    subnormals = sum(0 < (value & 0x7fffffff) < 0x00800000
                    for item in fixture['scenes'] for row in validate_batch(Path(item['batch']).read_bytes())
                    for value in row.host_world_bits)
    require(initial['subnormal_coefficients'] == reopened['subnormal_coefficients'] == subnormals,
            'Native subnormal coefficient accounting differs.')
    negatives = []
    for name in ('matrix', 'missing-scene', 'duplicate-scene', 'count', 'budget', 'incomplete', 'promotion', 'subnormal', 'legacy', 'anchor-controls'):
        bad = copy.deepcopy(initial)
        if name == 'matrix': bad['scenes'][0]['saved']['state_sha256'] = '0'*64
        if name == 'missing-scene': bad['scenes'].pop()
        if name == 'duplicate-scene': bad['scenes'][-1] = bad['scenes'][0]
        if name == 'count': bad['entities'] = 0
        if name == 'budget': bad['scenes'][0]['peak_step_entities'] = 33
        if name == 'incomplete': bad['status'] = 'PARTIAL'
        if name == 'promotion': bad['complete_rendered_scenes'] = 'PASSED'
        if name == 'subnormal': bad['subnormal_coefficients'] = 0
        if name == 'legacy': bad['legacy_readback']['matrices'] = {}
        if name == 'anchor-controls': bad['controls'][5]['rejected'] = 0
        try: check_reports(fixture, expected, bad, reopened)
        except ValueError: negatives.append(name)
        else: raise ValueError('Corrupt native evidence accepted: '+name)
    return {'status': 'PASSED', 'scope': 'source hierarchy placement assembly and fresh-process reopen',
            'maps': 4, 'scenes': 16, 'entities': sum(s['entities'] for s in expected.values()),
            'complete_source_hierarchies': sum(s['complete_hierarchy'] for s in fixture['scenes']),
            'subnormal_coefficients': subnormals,
            'negative_controls': negatives, 'native_controls': initial['controls'], 'artifacts': artifacts,
            'complete_rendered_scenes': 'NOT_RUN', 'game_export': 'NOT_RUN'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--root', type=Path, required=True); parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(); result = audit(args.root.resolve(strict=True))
    require(not any((p/'.git').exists() for p in (args.output.parent, *args.output.parent.parents)), 'Private output required.')
    with args.output.open('x', encoding='utf-8') as stream: json.dump(result, stream, indent=2)
    print(json.dumps({k: v for k, v in result.items() if k != 'artifacts'}))
