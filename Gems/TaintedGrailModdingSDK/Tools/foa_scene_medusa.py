# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Decode source Medusa scene arrays without guessing objects or terrain samples.

The caller binds the exact manager, scene name, archive and member bytes. Source
matrices, inverse matrices, LOD/culling and UV bytes are preserved. Selecting an
authoring LOD is explicit and does not reproduce the game's visibility policy.
"""
import hashlib
import re
import struct

from foa_heightmap_importer import HeightmapImportError, check_cancelled
from foa_scene_assembly import matrix, require
from foa_scene_component_audit import reference_key
from foa_scene_dots_buffers import PROFILE

MANAGER_TYPE = ('Awaken.ECS', 'Awaken.ECS.MedusaRenderer', 'MedusaRendererManager')
MAX_INSTANCES = 100000
MAX_RENDERERS = 16384
MAX_DRAWS = 1000000
MAX_MEMBER_BYTES = 64 * 1024 * 1024
MEMBERS = ('matrices.medusa', 'transformsBuffer.medusa', 'renderers.medusa',
           'reciprocalUvDistributions.medusa')


def count(value, maximum, label):
    require(type(value) is int and 0 <= value <= maximum, 'Invalid Medusa '+label+'.')
    return value


def manager_renderers(asset, tree, script_type):
    """Resolve typed source PPtrs, including empty renderer slots and their order."""
    require(tuple(script_type) == MANAGER_TYPE, 'An exact Medusa manager script binding is required.')
    required = {'m_GameObject', 'm_Enabled', 'm_Script', 'm_Name', '_renderers',
                '_transformsCount', '_allRenderersCount', '_allUvDistributionsCount'}
    require(type(tree) is dict and set(tree) == required and type(tree['m_Enabled']) is int
            and tree['m_Enabled'] == 1, 'Unsupported or disabled Medusa manager.')
    values = tree['_renderers']
    require(type(values) is list and 0 < len(values) <= MAX_RENDERERS, 'Invalid Medusa renderer table.')
    result = []
    for row in values:
        require(type(row) is dict and set(row) == {'renderData', 'lodMask', 'instancesCount'},
                'Unsupported Medusa renderer fields.')
        lod = count(row['lodMask'], 255, 'LOD mask')
        require(lod != 0, 'Empty Medusa LOD mask.')
        instances = count(row['instancesCount'], MAX_INSTANCES, 'renderer instance count')
        entries = row['renderData']
        require(type(entries) is list and len(entries) <= 4096, 'Invalid Medusa draw table.')
        draws = []
        for item in entries:
            require(type(item) is dict and set(item) == {'mesh', 'material', 'subMeshIndex'},
                    'Unsupported Medusa draw fields.')
            draws.append({'mesh': reference_key(asset, item['mesh']),
                          'material': reference_key(asset, item['material']),
                          'submesh': count(item['subMeshIndex'], 65535, 'submesh index')})
        result.append({'draws': tuple(draws), 'lod_mask': lod, 'instances': instances})
    return tuple(result)


def matrix_bits(raw):
    require(type(raw) is bytes and len(raw) == 48, 'Invalid Medusa packed matrix.')
    words = struct.unpack('<12I', raw)
    result = [words[col*3+row] for row in range(3) for col in range(4)] + [0, 0, 0, 0x3f800000]
    matrix(result)
    return result


class MedusaScene:
    """An immutable, bounded view over one exact scene's four archived arrays."""
    def __init__(self, scene_name, renderers, transform_count, renderer_count, uv_count,
                 members, profile, cancelled=lambda: False):
        require(type(profile) is dict and profile == PROFILE, 'Unqualified Medusa source profile.')
        require(type(scene_name) is str and re.fullmatch(r'[A-Za-z0-9_ -]{1,200}', scene_name),
                'Invalid exact Medusa source scene name.')
        require(type(renderers) is tuple and 0 < len(renderers) <= MAX_RENDERERS, 'Invalid Medusa renderer list.')
        n = count(transform_count, MAX_INSTANCES, 'transform count')
        count(renderer_count, MAX_DRAWS, 'flat renderer count'); count(uv_count, MAX_DRAWS, 'flat UV count')
        require(n > 0, 'Medusa scene has no source transforms.')
        sizes = (n*96, n*49, renderer_count*4, uv_count*4)
        names = tuple(scene_name+'/'+name for name in MEMBERS)
        require(type(members) is dict and set(members) == set(names), 'Missing or unclaimed Medusa scene members.')
        self.payloads = {}
        for name, size in zip(names, sizes):
            raw = members[name]
            require(type(raw) is bytes and len(raw) == size <= MAX_MEMBER_BYTES,
                    'Medusa member length differs from the source manager: '+name)
            self.payloads[name.rsplit('/', 1)[1]] = raw
        self.scene_name, self.count, self.cancelled = scene_name, n, cancelled
        self.hashes = {name: hashlib.sha256(raw).hexdigest() for name, raw in self.payloads.items()}
        self.renderers = []
        indices = self.payloads['renderers.medusa']; cursor = 0; uv_cursor = 0
        for ordinal, row in enumerate(renderers):
            check_cancelled(cancelled)
            require(type(row) is dict and set(row) == {'draws', 'lod_mask', 'instances'}, 'Invalid resolved Medusa renderer.')
            length = count(row['instances'], MAX_INSTANCES, 'renderer instances')
            require(type(row['draws']) is tuple and len(row['draws']) <= 4096, 'Invalid resolved Medusa draws.')
            lod = count(row['lod_mask'], 255, 'LOD mask'); require(lod > 0, 'Empty Medusa LOD mask.')
            require(cursor+length <= renderer_count, 'Medusa renderer indices are truncated.')
            selected = struct.unpack_from('<'+str(length)+'I', indices, cursor*4)
            require(len(set(selected)) == length and all(i < n for i in selected),
                    'Medusa renderer has absent or duplicate instance indices.')
            for draw in row['draws']:
                require(type(draw) is dict and set(draw) == {'mesh', 'material', 'submesh'}, 'Invalid resolved Medusa draw.')
                for kind in ('mesh', 'material'):
                    ref = draw[kind]
                    require(type(ref) is tuple and len(ref) == 2 and type(ref[0]) is str
                            and re.fullmatch(r'cab-[0-9a-f]{32}(?:\.sharedassets)?', ref[0])
                            and type(ref[1]) is int and -(1 << 63) <= ref[1] < (1 << 63) and ref[1] != 0,
                            'Invalid resolved Medusa asset reference.')
                count(draw['submesh'], 65535, 'submesh index')
            self.renderers.append({'ordinal': ordinal, 'lod_mask': lod, 'indices': selected,
                                   'draws': tuple(dict(draw) for draw in row['draws']),
                                   'index_offset': cursor*4, 'uv_offset': uv_cursor*4})
            cursor += length; uv_cursor += length*len(row['draws'])
        require(cursor == renderer_count and uv_cursor == uv_count, 'Unclaimed Medusa renderer or UV records.')
        for index in range(n):
            if index % 1024 == 0: check_cancelled(cancelled)
            matrix_bits(self.matrix_record(index)); matrix_bits(self.matrix_record(index, inverse=True))
        self.renderers = tuple(self.renderers)

    def matrix_record(self, index, *, inverse=False):
        require(type(index) is int and 0 <= index < self.count and type(inverse) is bool,
                'Medusa transform index is outside the source array.')
        start = ((self.count if inverse else 0)+index)*48
        return self.payloads['matrices.medusa'][start:start+48]

    def draws(self, *, lod):
        """Every draw at an explicitly selected authoring LOD; no distance culling."""
        require(type(lod) is int and 0 <= lod < 8, 'Select an explicit Medusa authoring LOD.')
        for renderer in self.renderers:
            check_cancelled(self.cancelled)
            if renderer['lod_mask'] & (1 << lod):
                for index in renderer['indices']:
                    for slot, draw in enumerate(renderer['draws']):
                        yield {'renderer_ordinal': renderer['ordinal'], 'instance_ordinal': index,
                               'draw_ordinal': slot, **draw}
