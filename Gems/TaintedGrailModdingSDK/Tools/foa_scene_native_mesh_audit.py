# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Read-only qualification of a generated GLB against the pinned native cache.

This checks geometry and mapped vertex channels, not shaders, game compatibility,
or editable source vertex identity. Asset references resolve through the native
catalog; hints and display names do not identify source game objects.
"""
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import sqlite3
import struct
import uuid
import zlib

from foa_heightmap_importer import HeightmapImportError, check_cancelled
from foa_scene_mesh import MAX_BYTES, MAX_INDICES, MAX_VERTICES


def require(condition, message):
    if not condition:
        raise HeightmapImportError(message)


class ObjectStream:
    def __init__(self, data):
        require(len(data) <= MAX_BYTES and data[:5] == b'\0\0\0\0\3', 'Expected bounded native ObjectStream v3.')
        self.data, self.pos, self.nodes, self.roots = data, 5, 0, []
        while self.pos < len(data):
            node = self.read(0)
            if node is None:
                require(self.pos == len(data), 'Trailing native object bytes.')
                break
            self.roots.append(node)

    def take(self, count):
        require(0 <= count <= len(self.data) - self.pos, 'Truncated native object.')
        result = self.data[self.pos:self.pos+count]
        self.pos += count
        return result

    def read(self, depth):
        self.nodes += 1
        require(depth <= 32 and self.nodes <= 100000, 'Native object structure exceeds its bound.')
        flags = self.take(1)[0]
        if not flags:
            return None
        require(flags & 8, 'Missing native element header.')
        name = int.from_bytes(self.take(4), 'big') if flags & 64 else None
        version = self.take(1)[0] if flags & 128 else 0
        type_id = str(uuid.UUID(bytes=self.take(16)))
        value = b''
        if flags & 16:
            size = flags & 7
            if flags & 32:
                require(size in (1, 2, 4), 'Invalid native value length.')
                size = int.from_bytes(self.take(size), 'big')
            value = self.take(size)
        children = []
        while True:
            child = self.read(depth+1)
            if child is None:
                break
            children.append(child)
        return {'name': name, 'version': version, 'type': type_id, 'value': value, 'children': children}


def field(node, name):
    matches = [c for c in node['children'] if c['name'] == zlib.crc32(name.lower().encode())]
    require(len(matches) == 1, 'Missing or ambiguous native field: ' + name)
    return matches[0]


def integer(node, name):
    return int.from_bytes(field(node, name)['value'], 'big')


class NativeCache:
    def __init__(self, root):
        self.root = Path(root).resolve(strict=True)
        self.db = sqlite3.connect((self.root.parent/'assetdb.sqlite').as_uri()+'?mode=ro', uri=True)
        self.products = {}

    def close(self):
        self.db.close()

    def read(self, path):
        path = Path(path).resolve(strict=True)
        require(path.is_relative_to(self.root) and path.stat().st_size <= MAX_BYTES, 'Native product escapes cache or exceeds its bound.')
        raw = path.read_bytes()
        roots = ObjectStream(raw).roots
        require(len(roots) == 1, 'Native product has multiple roots.')
        key = str(path.relative_to(self.root)); digest = hashlib.sha256(raw).hexdigest()
        require(self.products.setdefault(key, digest) == digest, 'Native product changed during verification.')
        return roots[0]

    def resolve(self, node):
        data = node['value']
        require(len(data) >= 57 and data[20:32] == b'\0'*12, 'Unqualified native asset reference framing.')
        size = int.from_bytes(data[48:56], 'big')
        require(size <= 1024 and len(data) == 57+size, 'Invalid native asset reference length.')
        rows = self.db.execute('SELECT DISTINCT p.ProductName FROM Products p JOIN Jobs j ON p.JobPK=j.JobID JOIN Sources s ON j.SourcePK=s.SourceID WHERE s.SourceGuid=? AND p.SubID=? AND j.Platform=?',
                               (data[:16], int.from_bytes(data[16:20], 'big'), 'pc')).fetchmany(2)
        require(len(rows) == 1, 'Native asset ID does not resolve uniquely.')
        return self.root.parent/rows[0][0]

    def model(self, source_name):
        source_name = source_name.replace(chr(92), "/")
        rows = self.db.execute('SELECT p.ProductName FROM Products p JOIN Jobs j ON p.JobPK=j.JobID JOIN Sources s ON j.SourcePK=s.SourceID WHERE s.SourceName=? AND p.ProductName=? AND j.Status=4 AND j.Platform=?',
                               (source_name, 'pc/'+source_name.lower()+'.azmodel', 'pc')).fetchmany(2)
        require(len(rows) == 1, 'The exact native mesh product has not completed successfully.')
        return self.read(self.root.parent/rows[0][0])

    def view(self, node, indices=False):
        product = self.read(self.resolve(field(node, 'BufferAsset')))
        require(product['type'] == 'f6c5ea8a-1db3-456e-b970-b6e2ab262aed' and product['version'] == 4 and integer(product, 'CompressionFormat') == 0, 'Unqualified native buffer schema or compression.')
        data = field(product, 'Buffer')['value']
        descriptor = field(node, 'BufferViewDescriptor')
        start, count, size, fmt = (integer(descriptor, k) for k in ('m_elementOffset', 'm_elementCount', 'm_elementSize', 'm_elementFormat'))
        require(0 < count <= (MAX_INDICES if indices else MAX_VERTICES) and size in (4, 8, 12, 16) and (start+count)*size <= len(data), 'Invalid native buffer view bounds.')
        if indices:
            require(size == 4 and fmt == 31, 'Unqualified native index format.')
            return [v[0] for v in struct.iter_unpack('<I', data[start*size:(start+count)*size])]
        require((size, fmt) in ((8, 12), (12, 4), (16, 1)), 'Unqualified native float stream format.')
        rows = list(struct.iter_unpack('<'+str(size//4)+'f', data[start*size:(start+count)*size]))
        require(all(math.isfinite(v) for row in rows for v in row), 'Nonfinite native channel.')
        return rows


def glb_accessors(blob):
    require(28 <= len(blob) <= MAX_BYTES+1024*1024+28, 'Invalid GLB byte count.')
    require(struct.unpack_from('<III', blob) == (0x46546c67, 2, len(blob)), 'Invalid GLB header.')
    size, kind = struct.unpack_from('<II', blob, 12)
    require(kind == 0x4e4f534a and size <= 1024*1024 and 28+size <= len(blob), 'Invalid GLB JSON chunk.')
    doc = json.loads(blob[20:20+size])
    bsize, kind = struct.unpack_from('<II', blob, 20+size)
    require(kind == 0x004e4942 and bsize == len(blob)-28-size, 'Invalid GLB binary chunk.')
    data = blob[28+size:]
    def read(index):
        accessor = doc['accessors'][index]
        view = doc['bufferViews'][accessor['bufferView']]
        require(view['buffer'] == 0 and accessor['componentType'] in (5126, 5125), 'Unqualified GLB accessor.')
        dimension = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[accessor['type']]
        start, length = view.get('byteOffset', 0), view['byteLength']
        require(start >= 0 and length == accessor['count']*dimension*4 and start+length <= len(data), 'Invalid GLB accessor bounds.')
        require(not accessor.get('byteOffset') and 'byteStride' not in view and 'sparse' not in accessor, 'Unqualified GLB accessor layout.')
        require(0 < accessor['count'] <= MAX_INDICES, 'GLB accessor count exceeds its bound.')
        rows = list(struct.iter_unpack('<'+str(dimension)+('f' if accessor['componentType'] == 5126 else 'I'), data[start:start+length]))
        require(all(math.isfinite(v) for row in rows for v in row), 'Nonfinite GLB channel.')
        return rows
    return doc, read


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def unit(row):
    length = math.sqrt(sum(v*v for v in row))
    require(length > 0, 'Zero source direction.')
    return tuple(v/length for v in row)


def cyclic(row):
    return min(row, row[1:]+row[:1], row[2:]+row[:2])


def audit_projection(cache, source_name, blob, cancelled=lambda: False):
    doc, read = glb_accessors(blob)
    require(doc['nodes'] == [{'mesh': 0, 'name': 'SourceMesh', 'matrix': [-1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1]}], 'Unqualified projection root transform.')
    schema = doc.get('asset', {}).get('extras', {}).get('foaSourceMeshProjection')
    require(schema is None or (type(schema) is int and schema == 1), 'Unqualified source preservation schema.')
    preserve = schema == 1
    primitives = doc['meshes'][0]['primitives']
    attrs = {k: read(v) for k, v in primitives[0]['attributes'].items()}
    positions = attrs['POSITION']
    require(0 < len(positions) <= MAX_VERTICES and all(len(rows) == len(positions) for rows in attrs.values()), 'Projection channel lengths disagree.')
    expected = {'POSITION0': positions}
    for name in ('NORMAL', 'TANGENT'):
        if name in attrs:
            expected[name+'0'] = [unit(row[:3]) + (row[3:] if name == 'TANGENT' else ()) for row in attrs[name]]
    if 'COLOR_0' in attrs:
        expected['COLOR0'] = attrs['COLOR_0']
    for name, rows in attrs.items():
        if name.startswith('TEXCOORD_'):
            expected['UV'+name[9:]] = [(u, f32(1-f32(1-v))) for u, v in rows]
    if not preserve and 'NORMAL' in attrs and 'TANGENT' in attrs:
        bitangents = []
        for normal, tangent in zip(attrs['NORMAL'], attrs['TANGENT']):
            x,y,z = normal; a,b,c,w = tangent
            bitangents.append(unit((f32(f32(y*c)-f32(z*b))*w, f32(f32(z*a)-f32(x*c))*w, f32(f32(x*b)-f32(y*a))*w)))
        expected['BITANGENT0'] = bitangents
    model = cache.model(source_name)
    slots = {}
    for entry in field(model, 'MaterialSlots')['children']:
        slot = field(entry, 'value2')
        stable_id = integer(slot, 'StableId')
        require(integer(entry, 'value1') == stable_id and stable_id not in slots, 'Native material slot identity mismatch.')
        display = field(slot, 'DisplayName')['value'].decode()
        require(display.startswith('SourceSlot') and display[10:].isdigit(), 'Unexpected generated material slot.')
        slots[stable_id] = int(display[10:])
    require(sorted(slots.values()) == list(range(len(primitives))), 'Native material slot table differs from source.')
    lods = field(model, 'LodAssets')['children']
    require(len(lods) == 1, 'Unexpected projection LOD count.')
    meshes = field(cache.read(cache.resolve(lods[0])), 'Meshes')['children']
    require(len(meshes) == len(primitives), 'Native submesh count differs from source.')
    maxima = defaultdict(float); matched_slots = set(); total = 0
    for mesh in meshes:
        check_cancelled(cancelled)
        slot = slots[integer(mesh, 'MaterialSlotId')]
        require(slot not in matched_slots, 'Duplicate native submesh slot.'); matched_slots.add(slot)
        primitive = primitives[slot]
        require(primitive['material'] == slot and primitive['attributes'] == primitives[0]['attributes'], 'Projection primitive mapping differs.')
        source_indices = [v[0] for v in read(primitive['indices'])]
        native = {}
        for item in field(mesh, 'StreamBufferInfo')['children']:
            semantic = field(item, 'Semantic')
            name = field(semantic, 'm_name')['value'].decode()+str(integer(semantic, 'm_index'))
            require(name not in native, 'Duplicate native channel.')
            native[name] = cache.view(field(item, 'BufferAssetView'))
        require(set(expected) <= set(native), 'Source vertex channels were dropped by native processing.')
        if preserve:
            require('BITANGENT0' not in native and ('TANGENT0' in native) == ('TANGENT' in attrs), 'Source-only tangent presence was not preserved.')
        for name, rows in expected.items():
            require(all(len(row) == len(rows[0]) for row in native[name]), 'Native channel dimensions differ from source.')
        count = len(native['POSITION0'])
        require(all(len(rows) == count for rows in native.values()), 'Native channel lengths disagree.')
        indices = cache.view(field(mesh, 'IndexBufferAssetView'), indices=True)
        require(len(source_indices) == len(indices) and len(indices)%3 == 0 and all(i < count for i in indices), 'Native triangle indices differ in count or bounds.')
        candidates = defaultdict(list)
        for i in range(0, len(source_indices), 3):
            triangle = tuple(source_indices[i:i+3])
            require(all(v < len(positions) for v in triangle), 'Invalid source projection index.')
            group = candidates[cyclic(tuple(positions[v] for v in triangle))]
            require(len(group) < 256, 'Coincident triangle audit exceeds its comparison budget.')
            group.append(triangle)
        for offset in range(0, len(indices), 3):
            if offset % 3072 == 0: check_cancelled(cancelled)
            actual = indices[offset:offset+3]
            key = cyclic(tuple(native['POSITION0'][v] for v in actual))
            possible = candidates.get(key, [])
            require(possible, 'Native oriented triangle geometry differs from source.')
            match = None; mismatch = None
            for candidate_index, triangle in enumerate(possible):
                for rotation in range(3):
                    rotated = triangle[rotation:]+triangle[:rotation]; errors = {}
                    if tuple(native["POSITION0"][v] for v in actual) != tuple(positions[v] for v in rotated):
                        continue
                    valid = True
                    for name, rows in expected.items():
                        tolerance = 1e-6 if name in ('NORMAL0','TANGENT0','BITANGENT0') else 0
                        delta = max(abs(a-b) for actual_index, source_index in zip(actual, rotated) for a,b in zip(native[name][actual_index], rows[source_index]))
                        if delta > tolerance:
                            mismatch = (name, delta)
                            valid = False; break
                        errors[name] = delta
                    if valid: match = candidate_index, errors; break
                if match is not None: break
            require(match is not None, 'Native triangle corner channels differ from source: ' + str(mismatch))
            possible.pop(match[0])
            for name, error in match[1].items(): maxima[name] = max(maxima[name], error)
        require(not any(candidates.values()), 'Native processing omitted source triangles.')
        total += len(indices)//3
    extra = attrs.get('_SOURCE_NORMAL_W', [])
    return {'geometry_status': 'PASSED', 'mapped_corner_channels_status': 'PASSED',
            'triangles': total, 'material_slots': len(primitives), 'source_preservation_schema': 1 if preserve else 0, 'max_component_error': dict(maxima),
            'extra_normal_lane': {'native_mapping': 'NOT_RUN' if extra else 'NOT_APPLICABLE', 'nonzero_values': sum(row[0] != 0 for row in extra)},
            'source_shader_mapping': 'NOT_RUN', 'editable_vertex_identity': 'NOT_RUN', 'game_export': 'NOT_RUN'}
