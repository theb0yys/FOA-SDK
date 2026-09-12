# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Private source hierarchy handoff. Placement assembly is not a rendered game scene.

No file, game, UI or native mutation occurs in this module. The producer binds the
ownership snapshot to the qualified capture. The consumer validates the entire
batch before making any native calls. Exact source records remain return authority.
"""
from dataclasses import dataclass
import hashlib
import json
import re
import struct
import time

from foa_heightmap_importer import HeightmapImportError, check_cancelled
from foa_scene_transform_capture import audit_capture, bits, unique_object

PROFILE = 'unity-6000.0.64f1'
MAX_SCENE_NODES = 100000
MAX_SCENE_BYTES = 256 * 1024 * 1024
MAX_SNAPSHOT_ROW_BYTES = 32 * 1024 * 1024  # Up to 100,000 explicit child PPtrs.
MAX_BATCH_NODES = 4096
MAX_BATCH_BYTES = 16 * 1024 * 1024
MAX_STEP_ENTITIES = 32
MAX_DEPTH = 256
MAPS = ('hos', 'cuanacht', 'forlorn', 'sarras')
ROLES = ('root', 'map-static', 'static-subscene', 'dynamic-subscene')
SOURCE_KEYS = {'map', 'role', 'bundle_sha256', 'serialized_file', 'snapshot_sha256',
               'unity_input_sha256', 'capture_sha256', 'scene_nodes'}
BINDING_KEYS = {'schema_version', 'profile', 'identity', 'source_parent',
                'source_local_bits', 'source_world_bits', 'local_bounds'}


def require(condition, message):
    if not condition:
        raise HeightmapImportError(message)


def encode(value):
    return json.dumps(value, separators=(',', ':'), ensure_ascii=True, allow_nan=False).encode('utf-8')


def decode(raw, limit):
    require(type(raw) is bytes and 0 < len(raw) <= limit, 'Source hierarchy input exceeds byte bounds.')
    try:
        return json.loads(raw, object_pairs_hook=unique_object,
                          parse_constant=lambda _: require(False, 'Nonfinite hierarchy JSON.'))
    except (ValueError, UnicodeError, RecursionError) as error:
        raise HeightmapImportError('Invalid source hierarchy JSON.') from error


def keys(value, expected):
    require(type(value) is dict and set(value) == expected, 'Unsupported source hierarchy fields.')


def text(value, maximum, empty=False):
    require(type(value) is str, 'Invalid source hierarchy text.')
    try:
        size = len(value.encode('utf-8'))
    except UnicodeError as error:
        raise HeightmapImportError('Invalid source hierarchy Unicode.') from error
    require(type(value) is str and (empty or bool(value)) and '\0' not in value
            and size <= maximum, 'Invalid source hierarchy text.')
    return value


def digest(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{64}', value) is not None,
            'Invalid source hierarchy fingerprint.')
    return value


def path_id(value):
    require(type(value) is str and re.fullmatch('-?[1-9][0-9]{0,18}', value) is not None
            and -(1 << 63) <= int(value) < (1 << 63), 'Invalid signed source identity.')
    return value


def identity(value):
    keys(value, {'serialized_file', 'path_id'})
    file = text(value['serialized_file'], 256)
    require(file == file.lower(), 'Noncanonical serialized-file identity.')
    return file, path_id(value['path_id'])


def matrix(value):
    require(type(value) is list and len(value) == 16
            and all(type(v) is int and 0 <= v <= 0xffffffff for v in value), 'Invalid source matrix bits.')
    numbers = struct.unpack('<16f', struct.pack('<16I', *value))
    require(all(abs(v) <= 3.4028234663852886e38 for v in numbers)
            and numbers[12:] == (0., 0., 0., 1.), 'Nonfinite or non-affine source matrix.')
    return numbers


def derive_anchor(position_bits, parent_bits):
    """Match the pinned host's pure-translation local/world float32 operations."""
    position = struct.unpack('<3f', struct.pack('<3I', *position_bits))
    parent = struct.unpack('<3f', struct.pack('<3I', *parent_bits))
    try:
        local = struct.unpack('<3f', struct.pack('<3f', *(x-p for x, p in zip(position, parent))))
        return struct.unpack('<3I', struct.pack('<3f', *(p+x for p, x in zip(parent, local))))
    except (OverflowError, struct.error) as error:
        raise HeightmapImportError('Native parent-relative anchor exceeds float32 bounds.') from error


@dataclass(frozen=True)
class AssemblyEntity:
    name: str
    active_self: bool
    active_in_hierarchy: bool
    kind: str
    descriptor: str
    key: tuple
    parent: int
    host_world_bits: tuple
    native_anchor_bits: tuple


def _validate(document, maximum, cancelled):
    keys(document, {'schema', 'version', 'profile', 'source', 'entities'})
    require(document['schema'] == 'foa.source-hierarchy' and type(document['version']) is int
            and document['version'] == 1 and document['profile'] == PROFILE, 'Unsupported hierarchy version/profile.')
    source = document['source']; keys(source, SOURCE_KEYS)
    require(source['map'] in MAPS and source['role'] in ROLES, 'Unknown campaign source selection.')
    for field in ('bundle_sha256', 'snapshot_sha256', 'unity_input_sha256', 'capture_sha256'):
        digest(source[field])
    file = text(source['serialized_file'], 256)
    require(file == file.lower() and type(source['scene_nodes']) is int
            and 0 < source['scene_nodes'] <= MAX_SCENE_NODES, 'Invalid scene identity/count.')
    entries = document['entities']
    require(type(entries) is list and 0 < len(entries) <= min(maximum, source['scene_nodes']), 'Invalid assembly entity count.')
    resolved, owners, rows, depths = {}, set(), [], []
    for item in entries:
        check_cancelled(cancelled)
        keys(item, {'name', 'active_self', 'active_in_hierarchy', 'kind', 'binding'})
        name = text(item['name'], 16384, empty=True)
        require(type(item['active_self']) is bool and type(item['active_in_hierarchy']) is bool
                and item['kind'] in ('Transform', 'RectTransform'), 'Invalid source entity state.')
        binding = item['binding']
        require(type(binding) is dict and type(binding.get('schema_version')) is int and binding['schema_version'] in (1, 2),
                'Unsupported placement binding version.')
        keys(binding, BINDING_KEYS | ({'native_anchor_bits'} if binding['schema_version'] == 2 else set()))
        require(type(binding['schema_version']) is int and binding['schema_version'] in (1, 2)
                and binding['profile'] == PROFILE and binding['local_bounds'] is None,
                'Unsupported hierarchy placement binding.')
        ident = binding['identity']; keys(ident, {'bundle_sha256', 'serialized_file', 'path_id', 'gameobject_id', 'record_sha256'})
        require(ident['bundle_sha256'] == source['bundle_sha256'] and ident['serialized_file'] == file,
                'Cross-source hierarchy identity.')
        key = (file, path_id(ident['path_id'])); owner = (file, path_id(ident['gameobject_id']))
        digest(ident['record_sha256'])
        require(key not in resolved and owner not in owners, 'Duplicate source Transform or GameObject.')
        parent_key = identity(binding['source_parent']) if binding['source_parent'] is not None else None
        require(parent_key is None or parent_key in resolved, 'Missing, cyclic or unordered source parent.')
        parent = resolved[parent_key] if parent_key is not None else -1
        depth = depths[parent] + 1 if parent >= 0 else 0
        require(depth <= MAX_DEPTH, 'Source hierarchy exceeds depth limit.')
        active = item['active_self'] and (rows[parent].active_in_hierarchy if parent >= 0 else True)
        require(active == item['active_in_hierarchy'], 'Source active hierarchy disagrees.')
        matrix(binding['source_local_bits']); matrix(binding['source_world_bits'])
        descriptor = encode(binding).decode('utf-8')
        require(len(descriptor.encode('utf-8')) <= 8192, 'Native source binding exceeds bounds.')
        order = (0, 2, 1, 3)
        world = tuple(binding['source_world_bits'][order[r]*4+order[c]] for r in range(4) for c in range(4))
        source_position = tuple(world[i] for i in (3, 7, 11))
        parent_anchor = rows[parent].native_anchor_bits if parent >= 0 else (0, 0, 0)
        anchor = derive_anchor(source_position, parent_anchor) if binding['schema_version'] == 2 else source_position
        if binding['schema_version'] == 2:
            require(type(binding['native_anchor_bits']) is list and all(type(v) is int for v in binding['native_anchor_bits'])
                    and tuple(binding['native_anchor_bits']) == anchor, 'Native anchor derivation disagrees with source hierarchy.')
        rows.append(AssemblyEntity(name, item['active_self'], active, item['kind'], descriptor, key, parent, world, anchor))
        resolved[key] = len(rows)-1; owners.add(owner); depths.append(depth)
    return tuple(rows)


def prepare_scene(snapshot_bytes, input_bytes, capture_bytes, bundle_sha256, map_key, role, cancelled=lambda: False):
    """Join exact ordered source records and measured matrices, never names/positions.

    The caller must verify the bundle and snapshot fingerprints against its source
    inventory. Hashes bind inputs; this function does not establish their authority.
    """
    require(type(snapshot_bytes) is bytes and 0 < len(snapshot_bytes) <= MAX_SCENE_BYTES,
            'Source snapshot exceeds bounds.')
    digest(bundle_sha256)
    capture = decode(capture_bytes, 128 * 1024 * 1024)
    checked = audit_capture(input_bytes, capture, cancelled)
    nodes = decode(input_bytes, 32 * 1024 * 1024)['nodes']
    # Discard arithmetic arrays after validation; retain the original measured bits.
    require(checked['transforms'] == len(nodes), 'Capture count disagrees.')
    del checked
    entities, seen, child_lists, actual_children, anchors = [], {}, [], {}, []
    for index, line in enumerate(snapshot_bytes.splitlines()):
        check_cancelled(cancelled)
        require(index < len(nodes) and len(line) <= MAX_SNAPSHOT_ROW_BYTES, 'Extra or oversized source snapshot row.')
        row = decode(line, MAX_SNAPSHOT_ROW_BYTES)
        require(type(row) is dict and {'transform', 'owner', 'parent', 'children', 'kind', 'name',
                'active_self', 'active_in_hierarchy', 'position', 'rotation', 'scale', 'record_sha256'} <= set(row),
                'Incomplete source snapshot row.')
        key, owner = identity(row['transform']), identity(row['owner'])
        require(key not in seen and key[0] == owner[0], 'Invalid source snapshot ownership.')
        node, measured = nodes[index], capture['rows'][index]
        parent = identity(row['parent']) if row['parent'] is not None else None
        require(parent is None or parent in seen, 'Invalid source snapshot parent order.')
        require(node['parent'] == (seen[parent] if parent is not None else -1) and row['kind'] == node['kind'],
                'Source hierarchy and capture disagree.')
        require(bits([*row['position'], *row['rotation'], *row['scale']]) == tuple(measured['inputBits']),
                'Source TRS and capture disagree.')
        children = row['children']
        require(type(children) is list and len(children) <= MAX_SCENE_NODES, 'Invalid source child inventory.')
        child_keys = tuple(identity(child) for child in children)
        require(len(set(child_keys)) == len(child_keys), 'Duplicate source child.')
        child_lists.append((key, child_keys)); actual_children.setdefault(key, [])
        if parent is not None:
            actual_children[parent].append(key)
        seen[key] = index
        captured_world = [v & 0xffffffff for v in measured['worldBits']]
        anchor = derive_anchor(tuple(captured_world[i] for i in (3, 11, 7)), anchors[node['parent']] if node['parent'] >= 0 else (0, 0, 0))
        anchors.append(anchor)
        binding = {'schema_version': 2, 'profile': PROFILE, 'native_anchor_bits': list(anchor),
                   'identity': {'bundle_sha256': bundle_sha256, 'serialized_file': key[0], 'path_id': key[1],
                                'gameobject_id': owner[1], 'record_sha256': row['record_sha256']},
                   'source_parent': row['parent'], 'source_local_bits': [v & 0xffffffff for v in measured['localBits']],
                   'source_world_bits': [v & 0xffffffff for v in measured['worldBits']], 'local_bounds': None}
        entities.append({k: row[k] for k in ('name', 'active_self', 'active_in_hierarchy', 'kind')} | {'binding': binding})
    require(len(entities) == len(nodes), 'Incomplete source snapshot.')
    for key, children in child_lists:
        check_cancelled(cancelled)
        require(set(children) == set(actual_children[key]), 'Nonreciprocal source child inventory.')
    document = {'schema': 'foa.source-hierarchy', 'version': 1, 'profile': PROFILE,
                'source': {'map': map_key, 'role': role, 'bundle_sha256': bundle_sha256,
                           'serialized_file': entities[0]['binding']['identity']['serialized_file'],
                           'snapshot_sha256': hashlib.sha256(snapshot_bytes).hexdigest(),
                           'unity_input_sha256': hashlib.sha256(input_bytes).hexdigest(),
                           'capture_sha256': hashlib.sha256(capture_bytes).hexdigest(), 'scene_nodes': len(entities)},
                'entities': entities}
    _validate(document, MAX_SCENE_NODES, cancelled)
    return document


def select_batch(plan, selected, cancelled=lambda: False):
    """Select explicit Transform keys and all ancestors in source order.

    Nothing is silently truncated to fit the native batch bound. Repeated or
    missing selections reject. Consumers import one batch in an empty source scope.
    """
    rows = _validate(plan, MAX_SCENE_NODES, cancelled)
    require(type(selected) in (tuple, list) and 0 < len(selected) <= MAX_BATCH_NODES, 'Invalid source selection count.')
    lookup = {row.key: i for i, row in enumerate(rows)}
    indices, requested = set(), set()
    for key in selected:
        check_cancelled(cancelled)
        require(type(key) is tuple and key in lookup and key not in requested, 'Missing or duplicate selected source identity.')
        requested.add(key); index = lookup[key]
        while index >= 0 and index not in indices:
            indices.add(index)
            require(len(indices) <= MAX_BATCH_NODES, 'Source ancestor closure exceeds assembly capacity.')
            index = rows[index].parent
    packet = dict(plan, entities=[plan['entities'][i] for i in sorted(indices)])
    raw = encode(packet)
    validate_batch(raw, cancelled)
    return raw


def validate_batch(raw, cancelled=lambda: False):
    document = decode(raw, MAX_BATCH_BYTES)
    if type(document) is dict and document.get('schema') == 'foa.merged-placements':
        from foa_scene_merged_assembly import validate_document
        return validate_document(document, cancelled)
    return _validate(document, MAX_BATCH_NODES, cancelled)


class AssemblyJob:
    """Bounded native transaction using an injected host adapter.

    Adapter create must return its entity before configure/bind can fail. No
    callback may create additional unreported entities. The host isolates edits
    during the job. remove must confirm removal; failures retain recovery IDs.
    """
    def __init__(self, raw, adapter, cancelled=lambda: False, clock=time.monotonic, *, ready=None, ready_timeout=120.):
        require(ready is None or callable(ready), 'Invalid assembly readiness callback.')
        require(type(ready_timeout) in (int, float) and 0 < ready_timeout <= 120, 'Invalid assembly readiness timeout.')
        self.rows = validate_batch(raw, cancelled)
        self.adapter, self.cancelled, self.clock = adapter, cancelled, clock
        adapter.preflight(self.rows)  # read-only; duplicate identities reject here
        self.created, self.remaining = [], []
        self.status, self.error, self.cleanup_errors = 'READY', None, []
        self.peak_step_entities = 0
        self.ready, self.ready_timeout, self.ready_index, self.ready_deadline = ready, ready_timeout, 0, None

    def step(self, limit=MAX_STEP_ENTITIES, seconds=.008):
        require(type(limit) is int and 0 < limit <= MAX_STEP_ENTITIES and type(seconds) in (float, int)
                and 0 < seconds <= .1, 'Invalid native assembly step budget.')
        if self.status in ('PASSED', 'FAILED', 'CANCELLED', 'CLEANUP_FAILED'):
            return self.status
        started, count = self.clock(), 0
        self.status = 'RUNNING' if self.status == 'READY' else self.status
        while count < limit and (count == 0 or self.clock()-started < seconds):
            if self.status == 'ROLLING_BACK':
                if not self.remaining:
                    self.status = 'CLEANUP_FAILED' if self.cleanup_errors else self.failure_status
                    break
                eid = self.remaining.pop()
                try:
                    self.adapter.remove(eid)
                except Exception as error:
                    self.cleanup_errors.append((eid, str(error)))
                count += 1
                continue
            try:
                if self.cancelled():
                    self.failure_status = 'CANCELLED'
                    raise HeightmapImportError('Source hierarchy assembly cancelled.')
                index = len(self.created)
                if index == len(self.rows):
                    if self.ready is None or self.ready_index == len(self.rows):
                        self.status = 'PASSED'
                        break
                    self.status = 'WAITING'
                    if self.ready_deadline is None:
                        self.ready_deadline = self.clock()+self.ready_timeout
                    require(self.clock() < self.ready_deadline, 'Native rendering readiness timed out.')
                    ready = self.ready(self.created[self.ready_index], self.rows[self.ready_index])
                    count += 1
                    require(type(ready) is bool, 'Invalid native readiness result.')
                    if not ready:
                        break
                    self.ready_index += 1
                    continue
                row = self.rows[index]
                parent = self.created[row.parent] if row.parent >= 0 else None
                eid = self.adapter.create(parent)
                self.created.append(eid)
                count += 1
                self.adapter.configure(eid, row)
                self.adapter.verify(eid, row, parent)
            except Exception as error:
                self.error = str(error)
                self.failure_status = getattr(self, 'failure_status', 'FAILED')
                self.remaining = list(self.created)
                self.status = 'ROLLING_BACK'
        self.peak_step_entities = max(self.peak_step_entities, count)
        return self.status
