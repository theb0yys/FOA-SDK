# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Independently audit native shared-scene resources and captured RGB buffers.

Requires a fresh-process reopen. Inputs and output stay in explicit private storage.
This fixture does not qualify a complete campaign map or its game export.
"""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
from PIL import Image


def require(value, message):
    if not value:
        raise ValueError(message)


def load(path):
    require(path.stat().st_size <= 8 * 1024 * 1024, 'Report exceeds byte bound.')
    return json.loads(path.read_text(encoding='utf-8'))


def resource_checks(report, fixture):
    draw = fixture['binding']['draw']
    geometry = [bytes.fromhex(s['hex']) for s in draw['streams']] + [bytes.fromhex(draw['indices'])]
    unique_bytes = sum(len(value) for value in set(geometry))
    constants = sum(len(bytes.fromhex(c['hex'])) for stage in draw['stages'] for c in stage['constants'])
    capacity = report['sharing_capacity']['statistics']
    require(capacity['entity_draws'] == 1024 and capacity['failed_entity_draws'] == 0, 'Incomplete capacity run.')
    require(capacity['geometry_payload_bytes'] == unique_bytes and capacity['unique_geometry_buffers'] == len(set(geometry)), 'Geometry was duplicated or omitted.')
    require(capacity['constant_payload_bytes'] == 1024 * constants, 'Mutable constants do not account for every object.')
    require(capacity['shared_stages'] == capacity['sampler_reservations'] == 1, 'Immutable material stage was duplicated.')
    for value in (capacity, report['sharing_after_removal'], report['sharing_distinct_uv'], report['sharing_after_batch'], report['sharing_after_samplers']):
        require(value['peak_tick_work'] <= 4 and value['peak_tick_visits'] <= 64, 'Native tick budget exceeded.')
        require(value['resident_payload_bytes'] == value['geometry_payload_bytes'] + value['constant_payload_bytes'], 'Resident accounting differs.')
    removal = report['sharing_after_removal']
    require(removal['entity_draws'] == 513 and removal['geometry_payload_bytes'] == unique_bytes and removal['constant_payload_bytes'] == 513 * constants, 'Removing owners changed shared data accounting.')
    uv = report['sharing_distinct_uv']
    require(uv['unique_geometry_buffers'] == len(set(geometry)) + 1 and uv['geometry_payload_bytes'] == unique_bytes + len(geometry[1]), 'Distinct UV bytes were merged.')
    for label in ('sharing_after_batch', 'sharing_after_budget', 'sharing_after_samplers'):
        value = report[label]
        require(value['entity_draws'] == 2 and value['resident_payload_bytes'] == unique_bytes + 2 * constants, 'Resource ownership leaked after ' + label)
    budget = report['sharing_budget_rejection']
    require(0 < budget['accepted_large_draws'] < 60 and budget['statistics']['resident_payload_bytes'] <= 64 * 1024 * 1024, 'Byte-budget rejection not exercised.')
    require(budget['statistics']['resident_payload_bytes'] + 65536 * 20 + constants > 64 * 1024 * 1024, 'Byte-budget rejection did not reach its threshold.')
    samplers = report['sharing_sampler_budget']
    require(samplers['ready_unique_stages'] == 511 and samplers['rejected_unique_stages'] == 3 and samplers['statistics']['sampler_reservations'] == 512, 'Sampler budget was not exercised.')
    final = report['sharing_final_release']
    require(all(final[k] == 0 for k in ('entity_draws', 'retired_draws', 'resident_payload_bytes', 'unique_geometry_buffers', 'shared_stages', 'sampler_reservations')), 'Last-owner cleanup leaked resources.')
    return {'capacity': capacity, 'geometry_bytes_without_sharing': 1024 * sum(map(len, geometry)), 'geometry_bytes_with_sharing': unique_bytes,
            'elapsed_capacity_seconds': report['sharing_capacity']['elapsed_seconds'], 'byte_budget': budget, 'sampler_budget': samplers, 'final_release': final}


def samples(image, state, fixture):
    colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0)]
    local = [(-.4, .4, .5, 1), (.4, .4, .5, 1), (-.4, -.4, .5, 1), (.4, -.4, .5, 1)]
    width, height = image.size
    uv = state.get('uv_by_object')
    if uv:
        raw = bytes.fromhex(fixture['binding']['draw']['streams'][1]['hex'])
        original = list(struct.unpack('<' + 'f' * (len(raw) // 4), raw))
        flipped = [1. - v if i % 2 else v for i, v in enumerate(original)]
        require(uv == [original, flipped], 'Unknown UV control.')
    count = 0
    for index, matrix in enumerate(state['matrices']):
        expected_colors = [colors[2], colors[3], colors[0], colors[1]] if uv and index == 1 else colors
        for point, expected in zip(local, expected_colors):
            clip = [sum(matrix[r * 4 + c] * point[c] for c in range(4)) for r in range(4)]
            x = math.floor((clip[0] / clip[3] * .5 + .5) * width)
            y = math.floor((.5 - clip[1] / clip[3] * .5) * height)
            for dx, dy in ((-1, -1), (0, 0), (1, 1)):
                require(0 <= x + dx < width and 0 <= y + dy < height, 'Sample lies outside capture.')
                require(image.getpixel((x + dx, y + dy)) == expected, 'Independent RGB sample differs.')
                count += 1
    return count


def captured(root, capture):
    path = Path(capture['path']).resolve(strict=True)
    require(path.parent == root and not Path(capture['path']).is_symlink(), 'Capture escapes the explicit fixture.')
    require(path.stat().st_size <= 64 * 1024 * 1024, 'Capture exceeds byte bound.')
    require(hashlib.sha256(path.read_bytes()).hexdigest() == capture['sha256'], 'Capture fingerprint changed.')
    with Image.open(path) as image:
        require(image.format == 'PPM' and image.mode == 'RGB' and 0 < image.width * image.height <= 20_000_000, 'Unexpected capture format or dimensions.')
        return image.copy()


def audit(root):
    fixture = load(root / 'fixture.json')
    report, reopened = (load(root / name) for name in ('editor.json', 'reopen.json'))
    require(fixture.get('shared_geometry') is True and not fixture.get('camera'), 'Unexpected sharing fixture.')
    require(all(r['status'] == 'PARTIAL' and r['native_checks'] == 'PASSED' for r in (report, reopened)), 'Native checks or fresh reopen did not pass.')
    resources = resource_checks(report, fixture)
    require(len(report['rejections']) == 22 and len(set(report['rejections'])) == 22, 'Missing malformed or capacity controls.')
    expected = {'initial', 'moved', 'undo', 'redo', 'parent-rotated', 'hidden-child', 'shown-child', 'deleted-child', 'restored-child', 'different-uv-shared-position-index', 'after-sharing-removals'}
    require({s['name'] for s in report['states']} == expected and len(report['states']) == len(expected), 'Missing native edit/UV/removal states.')
    require([s['name'] for s in reopened['states']] == ['reopened'], 'Missing reopen state.')
    pairs, pixel_count = [], 0
    for run in (report, reopened):
        captures = {c['name']: c for c in run['captures']}
        require(len(captures) == len(run['captures']) == len(run['states']) * 2, 'Duplicate or incomplete captures.')
        for state in run['states']:
            actual, reference = (captured(root, captures[state['name'] + '-' + kind]) for kind in ('entities', 'reference'))
            require(actual.size == reference.size and actual.tobytes() == reference.tobytes(), 'Complete RGB buffers differ: ' + state['name'])
            count = samples(actual, state, fixture) + samples(reference, state, fixture)
            pixel_count += count
            pairs.append({'state': state['name'], 'extent': list(actual.size), 'sampled_pixels': count, 'complete_rgb_equal': True})
    controls = []
    for name, key, field, value in [('duplicate-geometry', 'sharing_capacity', 'geometry_payload_bytes', 999999), ('tick-overrun', 'sharing_capacity', 'peak_tick_work', 5), ('final-owner-leak', 'sharing_final_release', 'shared_stages', 1)]:
        changed = copy.deepcopy(report)
        target = changed[key]['statistics'] if key == 'sharing_capacity' else changed[key]
        target[field] = value
        try:
            resource_checks(changed, fixture)
        except ValueError:
            controls.append(name)
        else:
            raise ValueError('Corrupted resource evidence was accepted: ' + name)
    first = report['states'][0]
    capture_map = {c['name']: c for c in report['captures']}
    for name, source in [('wrong-pose', 'moved-entities'), ('missing-child', 'hidden-child-entities')]:
        try:
            samples(captured(root, capture_map[source]), first, fixture)
        except ValueError:
            controls.append(name)
        else:
            raise ValueError('Wrong image accepted: ' + name)
    return {'status': 'PASSED', 'scope': 'Shared native source geometry and immutable stages with synthetic entities and original Unlit programs',
            'resources': resources, 'pairs': pairs, 'sampled_pixels': pixel_count, 'negative_controls': controls,
            'native_rejections': len(report['rejections']), 'fresh_reopen': 'PASSED', 'complete_campaign_maps': 'NOT_RUN', 'game_export': 'NOT_RUN'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    output = args.output.resolve()
    require(not output.exists(), 'Existing evidence must not be replaced.')
    require(not any((p / '.git').exists() for path in (root, output.parent) for p in (path, *path.parents)), 'Private storage outside source control required.')
    result = audit(root)
    with output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
