# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Private native placement packets for explicitly selected merged archive instances.

Instance ordinals belong to an exact archive member, never a guessed GameObject.
The source loader expands four float3 columns without applying a loader transform.
This packet does not select runtime LODs or supply missing native material bindings.
"""
import hashlib
import re
import struct

import foa_scene_assembly as a
from foa_scene_merged_renderers import MergedRenderers, MAX_SECTION_RECORDS

SCHEMA = 'foa.merged-placements'
IDENTITY_KEYS = {'archive_sha256', 'member_guid', 'member_sha256', 'instance_ordinal', 'record_sha256'}


def guid(value):
    a.require(type(value) is str and re.fullmatch('[0-9a-f]{32}', value) is not None, 'Invalid merged member GUID.')
    return value


def expand_bits(raw):
    """Expand the original 48 bytes by integer permutation, including signed zero."""
    a.require(type(raw) is bytes and len(raw) == 56, 'Invalid merged instance record.')
    columns = struct.unpack('<12I', raw[:48])
    result = [columns[c*3+r] for r in range(3) for c in range(4)] + [0, 0, 0, 0x3f800000]
    a.matrix(result)
    return result


def validate_document(doc, cancelled=lambda: False):
    a.keys(doc, {'schema', 'version', 'profile', 'source', 'entities'})
    a.require(doc['schema'] == SCHEMA and type(doc['version']) is int and doc['version'] == 1
              and doc['profile'] == a.PROFILE, 'Unsupported merged placement packet.')
    source = doc['source']
    a.keys(source, {'map', 'archive_sha256', 'member_guid', 'member_sha256', 'instance_count'})
    a.require(source['map'] in a.MAPS, 'Unknown campaign selection.')
    a.digest(source['archive_sha256']); a.digest(source['member_sha256']); guid(source['member_guid'])
    count = source['instance_count']
    a.require(type(count) is int and 0 < count <= MAX_SECTION_RECORDS, 'Invalid merged instance count.')
    entries = doc['entities']
    a.require(type(entries) is list and 0 < len(entries) <= min(count, a.MAX_BATCH_NODES), 'Invalid merged selection count.')
    result, seen = [], set()
    for item in entries:
        a.check_cancelled(cancelled)
        a.keys(item, {'name', 'binding', 'record_hex'})
        name = a.text(item['name'], 16384, empty=True)
        encoded = item['record_hex']
        a.require(type(encoded) is str and re.fullmatch('[0-9a-f]{112}', encoded) is not None, 'Invalid merged record bytes.')
        raw = bytes.fromhex(encoded); world = expand_bits(raw)
        binding = item['binding']; a.keys(binding, a.BINDING_KEYS)
        a.require(type(binding['schema_version']) is int and binding['schema_version'] == 3
                  and binding['profile'] == a.PROFILE and binding['source_parent'] is None
                  and binding['local_bounds'] is None, 'Invalid merged placement binding.')
        ident = binding['identity']; a.keys(ident, IDENTITY_KEYS)
        for key in ('archive_sha256', 'member_guid', 'member_sha256'):
            a.require(ident[key] == source[key], 'Merged placement belongs to another source.')
        ordinal = ident['instance_ordinal']
        a.require(type(ordinal) is int and 0 <= ordinal < count and ordinal not in seen, 'Unknown or duplicate merged instance.')
        seen.add(ordinal)
        a.require(a.digest(ident['record_sha256']) == hashlib.sha256(raw).hexdigest(), 'Merged source record changed.')
        a.matrix(binding['source_local_bits']); a.matrix(binding['source_world_bits'])
        a.require(binding['source_local_bits'] == binding['source_world_bits'] == world, 'Merged source matrix differs from record.')
        order = (0, 2, 1, 3)
        host = tuple(world[order[r]*4+order[c]] for r in range(4) for c in range(4))
        descriptor = a.encode(binding).decode('utf-8')
        a.require(len(descriptor) <= 8192, 'Merged native descriptor exceeds its bound.')
        key = (source['archive_sha256'], source['member_guid'], source['member_sha256'], ordinal)
        # Visibility here means explicitly selected for authoring, not runtime LOD activation.
        result.append(a.AssemblyEntity(name, True, True, 'MergedInstance', descriptor, key, -1, host,
                                       tuple(host[i] for i in (3, 7, 11))))
    return tuple(result)


def prepare_batch(member, selected, archive_sha256, map_key, cancelled=lambda: False):
    """Prepare exact ordinals after the caller qualifies the loader/member association.

    No automatic truncation, geometry/name join, parent transform or LOD filtering.
    All original instance bytes remain in the private packet for source comparison.
    """
    a.require(type(member) is MergedRenderers, 'An exact decoded archive member is required.')
    a.require(type(selected) in (list, tuple) and 0 < len(selected) <= a.MAX_BATCH_NODES, 'Invalid merged selection.')
    a.digest(archive_sha256)
    a.require(hashlib.sha256(member.payload).hexdigest() == member.sha256, 'Merged source payload changed.')
    entries = []
    for index in selected:
        a.check_cancelled(cancelled)
        raw = member.record('instances', index); world = expand_bits(raw)
        ident = dict(archive_sha256=archive_sha256, member_guid=member.guid, member_sha256=member.sha256,
                     instance_ordinal=index, record_sha256=hashlib.sha256(raw).hexdigest())
        binding = dict(schema_version=3, profile=a.PROFILE, identity=ident, source_parent=None,
                       source_local_bits=world, source_world_bits=world, local_bounds=None)
        entries.append(dict(name='Merged instance '+str(index), record_hex=raw.hex(), binding=binding))
    doc = dict(schema=SCHEMA, version=1, profile=a.PROFILE,
               source=dict(map=map_key, archive_sha256=archive_sha256, member_guid=member.guid,
                           member_sha256=member.sha256, instance_count=member.sections['instances'].count), entities=entries)
    raw = a.encode(doc)
    a.validate_batch(raw, cancelled)
    return raw
