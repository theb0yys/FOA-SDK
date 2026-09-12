# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""World Authoring handoff joining exact placements to already qualified native draws.

This private packet does not infer source ownership, materials, passes or runtime
inputs. The producer must establish those before supplying render bindings. The
native service remains authoritative for resource and shader contract validation.
"""
from dataclasses import dataclass
import hashlib
from types import MappingProxyType

from foa_scene_assembly import check_cancelled, decode, digest, encode, keys, require, validate_batch

MAX_RENDER_BYTES = 64 * 1024 * 1024
MAX_DRAW_BYTES = 16 * 1024 * 1024
MAX_DRAWS = 1024
MAX_GROUP_DRAWS = 128


def placement_digest(row):
    return hashlib.sha256(row.descriptor.encode('utf-8')).hexdigest()


def draw_count(value):
    require(type(value) is dict and type(value.get('version')) is int, 'Invalid native render binding version.')
    version = value['version']
    if version in (1, 2):
        keys(value, {'version', 'draw', 'matrices'} | ({'vectors'} if version == 2 else set()))
        require(type(value['draw']) is dict and type(value['matrices']) is list, 'Invalid explicit native render binding.')
        return 1
    require(version == 3, 'Unsupported native render binding version.')
    keys(value, {'version', 'draws'})
    members = value['draws']
    require(type(members) is list and 0 < len(members) <= MAX_GROUP_DRAWS, 'Invalid native render group size.')
    previous = -1
    for member in members:
        require(type(member) is dict and type(member.get('version')) is int and member['version'] in (1, 2),
                'Nested or unsupported native render member.')
        draw_count(member)
        ordinal = member['draw'].get('sort_key')
        require(type(ordinal) is int and previous < ordinal <= 0xffffffff, 'Invalid native render group ordering.')
        previous = ordinal
    return len(members)


@dataclass(frozen=True)
class RenderAssembly:
    bindings: object
    draw_count: int
    hierarchy_sha256: str


def validate_render_batch(hierarchy_raw, render_raw, cancelled=lambda: False):
    rows = validate_batch(hierarchy_raw, cancelled)
    document = decode(render_raw, MAX_RENDER_BYTES)
    keys(document, {'schema', 'version', 'hierarchy_sha256', 'renderers'})
    require(document['schema'] == 'foa.source-render-assembly' and type(document['version']) is int
            and document['version'] == 1, 'Unsupported rendered hierarchy packet.')
    fingerprint = digest(document['hierarchy_sha256'])
    require(hashlib.sha256(hierarchy_raw).hexdigest() == fingerprint, 'Rendered hierarchy input changed.')
    entries = document['renderers']
    require(type(entries) is list and 0 < len(entries) <= min(len(rows), MAX_DRAWS), 'Invalid rendered placement count.')
    known = {placement_digest(row) for row in rows}; result = {}; count = 0
    for entry in entries:
        check_cancelled(cancelled); keys(entry, {'placement_sha256', 'binding'})
        source = digest(entry['placement_sha256'])
        require(source in known and source not in result, 'Unknown, duplicate or stale rendered placement.')
        raw = encode(entry['binding'])
        require(len(raw) <= MAX_DRAW_BYTES, 'Native rendered placement exceeds its descriptor bound.')
        count += draw_count(entry['binding'])
        require(count <= MAX_DRAWS, 'Rendered hierarchy exceeds aggregate native draw capacity.')
        result[source] = raw.decode('utf-8')
    return RenderAssembly(MappingProxyType(result), count, fingerprint)


def prepare_render_batch(hierarchy_raw, renderers, cancelled=lambda: False):
    """Bind explicit (complete placement digest, native binding) pairs without joining labels."""
    require(type(renderers) in (list, tuple) and 0 < len(renderers) <= MAX_DRAWS, 'Invalid rendered placement selection.')
    entries = []
    for item in renderers:
        check_cancelled(cancelled)
        require(type(item) in (tuple, list) and len(item) == 2, 'Invalid rendered placement entry.')
        entries.append({'placement_sha256': item[0], 'binding': item[1]})
    raw = encode({'schema': 'foa.source-render-assembly', 'version': 1,
                  'hierarchy_sha256': hashlib.sha256(hierarchy_raw).hexdigest(), 'renderers': entries})
    validate_render_batch(hierarchy_raw, raw, cancelled)
    return raw
