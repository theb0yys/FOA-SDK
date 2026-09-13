# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Exact source geometry groups for a neutral campaign-terrain inspection view.

All highest-detail Medusa draws are retained, including the surrounding cliffs.
This view does not simulate game materials, distance culling or game export.
The archive instance IDs remain explicit; authoring groups are not GameObjects.
"""
import hashlib
import re
import struct

import foa_scene_assembly as a
from foa_scene_medusa import MedusaScene, matrix_bits

SCHEMA = 'foa.campaign-terrain'
MAX_PACKET_BYTES = 128 * 1024 * 1024
MAX_GROUPS = 1024
IDENTITY_KEYS = {'archive_sha256', 'scene_name', 'manager_record_sha256', 'renderer_ordinal',
                 'draw_ordinal', 'first_instance', 'instance_count', 'instances_sha256'}
IDENTITY = [0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0, 0, 0, 0x3f800000]
SHADER_ASSET = 'assets/foa_terrain_shape/terrain.foashader.azshader'

# Original mesh vectors and packed instance matrices remain in Unity coordinates.
# Only the native camera/authoring delta supplied by the existing placement service
# performs the established axis conversion. Pixel derivatives provide neutral shading.
SHADER = b'''cbuffer Placement : register(b0) { column_major float4x4 placement; };
cbuffer Camera : register(b1) { column_major float4x4 camera; };
ByteAddressBuffer instanceMatrices : register(t0);
struct V { float4 clip:SV_Position; float3 world:TEXCOORD0; };
V VS(float3 p:POSITION, uint id:SV_InstanceID) {
 uint o=id*48; float3 c0=asfloat(instanceMatrices.Load3(o));
 float3 c1=asfloat(instanceMatrices.Load3(o+12)); float3 c2=asfloat(instanceMatrices.Load3(o+24));
 float3 c3=asfloat(instanceMatrices.Load3(o+36));
 float3 w=c0*p.x+c1*p.y+c2*p.z+c3; V v;
 v.world=mul(placement,float4(w,1)).xyz; v.clip=mul(camera,float4(v.world,1)); return v;
}
float4 PS(V v):SV_Target0 {
 float3 n=normalize(cross(ddx(v.world),ddy(v.world))); n*=n.y<0?-1:1;
 float light=.35+.65*abs(dot(n,normalize(float3(.4,.8,.3))));
 float3 color=lerp(float3(.28,.33,.29),float3(.56,.59,.50),saturate(n.y));
 return float4(color*light,1);
}'''


def sha(raw): return hashlib.sha256(raw).hexdigest()


def _binding(identity, bounds):
    return dict(schema_version=4, profile=a.PROFILE, identity=identity, source_parent=None,
                source_local_bits=IDENTITY[:], source_world_bits=IDENTITY[:], local_bounds=bounds)


def prepare(scene, meshes, map_key, archive_sha256, manager_record_sha256, cancelled=lambda: False):
    """Build complete LOD0 geometry; split only to satisfy native instance/work bounds.

    meshes maps exact (serialized file, pathID) to fully decoded original SourceMesh.
    No simplification, resampling, normal modification or missing-piece fill occurs.
    """
    a.require(type(scene) is MedusaScene and map_key in a.MAPS, 'Exact campaign terrain source required.')
    a.digest(archive_sha256); a.digest(manager_record_sha256)
    a.require(all(sha(raw) == scene.hashes[name] for name, raw in scene.payloads.items()), 'Medusa source changed.')
    rows = []; source_draws = 0; vertex_bytes = 0; unique = set()
    for renderer in scene.renderers:
        if not renderer['lod_mask'] & 1: continue
        for draw_ordinal, draw in enumerate(renderer['draws']):
            a.check_cancelled(cancelled)
            ref = draw['mesh']; a.require(ref in meshes, 'A source terrain mesh is missing.')
            mesh = meshes[ref]
            a.require((mesh.binding['serialized_file'].lower(), int(mesh.binding['path_id'])) == ref,
                      'Terrain mesh identity differs from its source draw.')
            slot = draw['submesh']; a.require(0 <= slot < len(mesh.submeshes), 'Missing source terrain submesh.')
            positions = mesh.attributes[0]; triangles = mesh.submeshes[slot]
            a.require(positions and triangles, 'Empty source terrain draw needs explicit handling.')
            vertices = b''.join(struct.pack('<3f', *p) for p in positions)
            indices = b''.join(struct.pack('<3I', *t) for t in triangles)
            if ref not in unique: vertex_bytes += len(vertices); unique.add(ref)
            capacity = min(4096, 2097152//(len(indices)//4))
            a.require(capacity > 0, 'A source terrain draw exceeds native work capacity.')
            selected = renderer['indices']
            for first in range(0, len(selected), capacity):
                a.check_cancelled(cancelled)
                ids = selected[first:first+capacity]
                matrices = b''.join(scene.matrix_record(i) for i in ids)
                ident = dict(archive_sha256=archive_sha256, scene_name=scene.scene_name,
                    manager_record_sha256=manager_record_sha256, renderer_ordinal=renderer['ordinal'],
                    draw_ordinal=draw_ordinal, first_instance=first, instance_count=len(ids),
                    instances_sha256=sha(struct.pack('<'+str(len(ids))+'I', *ids)+matrices))
                lo = [min(p[k] for p in positions) for k in range(3)]
                hi = [max(p[k] for p in positions) for k in range(3)]
                points = []
                for index in ids:
                    m = a.matrix(matrix_bits(scene.matrix_record(index)))
                    for corner in range(8):
                        p = [hi[k] if corner & (1 << k) else lo[k] for k in range(3)]
                        points.append([sum(m[r*4+k]*p[k] for k in range(3))+m[r*4+3] for r in range(3)])
                bounds = dict(min=[min(p[k] for p in points) for k in range(3)],
                              max=[max(p[k] for p in points) for k in range(3)])
                stages = [dict(constants=[dict(slot=0, hex=bytes(64).hex()), dict(slot=1, hex=bytes(64).hex())],
                               images=[], samplers=[], buffers=[dict(slot=0, type=4, stride=4, hex=matrices.hex())]),
                          dict(constants=[], images=[], samplers=[], buffers=[])]
                native = dict(version=3, shader=SHADER_ASSET, vertex_count=len(positions),
                    streams=[dict(semantic='POSITION', index=0, components=3, component_type=3, hex=vertices.hex())],
                    indices=indices.hex(), stages=stages, sort_key=len(rows), instance_count=len(ids))
                binding = dict(version=2, draw=native, matrices=[
                    dict(stage=0, slot=0, offset=0, layout='column_major', value='source_object_to_world'),
                    dict(stage=0, slot=1, offset=0, layout='column_major', value='viewport_source_world_to_clip')], vectors=[])
                rows.append(dict(placement=_binding(ident, bounds), rendering=binding,
                    source=dict(mesh=list(ref), material=list(draw['material']), submesh=slot,
                                mesh_record_sha256=mesh.binding['record_sha256'], instance_indices=list(ids),
                                matrices_sha256=sha(matrices), vertices_sha256=sha(vertices), indices_sha256=sha(indices))))
                source_draws += len(ids)
                a.require(len(rows) <= MAX_GROUPS, 'Complete campaign terrain exceeds native draw capacity.')
    a.require(rows and source_draws == sum(1 for _ in scene.draws(lod=0)), 'Incomplete terrain draw coverage.')
    doc = dict(schema=SCHEMA, version=1, map=map_key, scene_name=scene.scene_name, lod=0,
               source_draw_count=source_draws, unique_mesh_count=len(unique), vertex_bytes=vertex_bytes, groups=rows)
    raw = a.encode(doc); a.require(len(raw) <= MAX_PACKET_BYTES, 'Complete terrain packet exceeds its byte bound.')
    validate(raw)
    return raw


def validate(raw):
    doc = a.decode(raw, MAX_PACKET_BYTES)
    a.keys(doc, {'schema', 'version', 'map', 'scene_name', 'lod', 'source_draw_count', 'unique_mesh_count', 'vertex_bytes', 'groups'})
    a.require(doc['schema'] == SCHEMA and type(doc['version']) is int and doc['version'] == 1 and doc['map'] in a.MAPS
              and type(doc['lod']) is int and doc['lod'] == 0, 'Unsupported terrain packet.')
    rows = doc['groups']; a.require(type(rows) is list and 0 < len(rows) <= MAX_GROUPS, 'Invalid terrain groups.')
    a.require(type(doc['scene_name']) is str and re.fullmatch(r'[A-Za-z0-9_ -]{1,200}', doc['scene_name']), 'Invalid terrain source scene.')
    known = set(); draw_count = 0; mesh_refs = set(); source_scope = None
    mesh_sizes = {}; ranges = {}
    for ordinal, row in enumerate(rows):
        a.keys(row, {'placement', 'rendering', 'source'})
        p = row['placement']; a.keys(p, a.BINDING_KEYS); a.keys(p['identity'], IDENTITY_KEYS)
        a.matrix(p['source_local_bits']); a.matrix(p['source_world_bits'])
        i = p['identity']; a.require(type(p['schema_version']) is int and p['schema_version'] == 4
            and p['profile'] == a.PROFILE and p['source_parent'] is None
            and p['source_local_bits'] == p['source_world_bits'] == IDENTITY, 'Invalid terrain authoring placement.')
        for name in ('archive_sha256', 'manager_record_sha256', 'instances_sha256'): a.digest(i[name])
        a.require(i['scene_name'] == doc['scene_name'], 'Terrain group belongs to another scene.')
        scope = i['archive_sha256'], i['manager_record_sha256']
        if source_scope is None: source_scope = scope
        a.require(scope == source_scope, 'Terrain groups mix source managers or archives.')
        box = p['local_bounds']; a.keys(box, {'min', 'max'})
        for side in ('min', 'max'):
            a.require(type(box[side]) is list and len(box[side]) == 3 and
                      all(type(v) in (int, float) and abs(v) <= 3.402823466e38 for v in box[side]), 'Invalid terrain group bounds.')
        a.require(all(x <= y for x, y in zip(box['min'], box['max'])), 'Reversed terrain group bounds.')
        for name in ('renderer_ordinal', 'draw_ordinal', 'first_instance', 'instance_count'):
            a.require(type(i[name]) is int and 0 <= i[name] <= 1000000, 'Invalid terrain source ordinal.')
        key = i['renderer_ordinal'], i['draw_ordinal'], i['first_instance']
        a.require(key not in known, 'Duplicate terrain source group.'); known.add(key)
        source = row['source']; a.keys(source, {'mesh', 'material', 'submesh', 'mesh_record_sha256',
            'instance_indices', 'matrices_sha256', 'vertices_sha256', 'indices_sha256'})
        for name in ('mesh_record_sha256', 'matrices_sha256', 'vertices_sha256', 'indices_sha256'): a.digest(source[name])
        for kind in ('mesh', 'material'):
            ref = source[kind]
            a.require(type(ref) is list and len(ref) == 2 and type(ref[0]) is str and
                      re.fullmatch(r'cab-[0-9a-f]{32}(?:\.sharedassets)?', ref[0]) and
                      type(ref[1]) is int and -(1 << 63) <= ref[1] < (1 << 63) and ref[1] != 0, 'Invalid terrain source asset identity.')
        mesh_refs.add(tuple(source['mesh']))
        a.require(type(source['submesh']) is int and 0 <= source['submesh'] <= 65535, 'Invalid terrain source submesh.')
        r = row['rendering']; a.keys(r, {'version', 'draw', 'matrices', 'vectors'})
        a.require(type(r['version']) is int and r['version'] == 2 and r['vectors'] == [] and r['matrices'] == [
            dict(stage=0, slot=0, offset=0, layout='column_major', value='source_object_to_world'),
            dict(stage=0, slot=1, offset=0, layout='column_major', value='viewport_source_world_to_clip')], 'Invalid terrain camera binding.')
        a.keys(r['draw'], {'version', 'shader', 'vertex_count', 'streams', 'indices', 'stages', 'sort_key', 'instance_count'})
        d = r['draw']; a.require(type(d['version']) is int and d['version'] == 3 and d['shader'] == SHADER_ASSET
                  and type(d['sort_key']) is int and d['sort_key'] == ordinal,
                                'Invalid neutral terrain draw.')
        a.require(type(d['vertex_count']) is int and 3 <= d['vertex_count'] <= 1000000, 'Invalid terrain vertex count.')
        a.require(type(d['streams']) is list and len(d['streams']) == 1, 'Invalid terrain position streams.')
        channel = d['streams'][0]; a.keys(channel, {'semantic', 'index', 'components', 'component_type', 'hex'})
        a.require({k: channel[k] for k in channel if k != 'hex'} == dict(semantic='POSITION', index=0, components=3, component_type=3),
                  'Altered terrain vertex format.')
        stages = d['stages']; a.require(type(stages) is list and len(stages) == 2, 'Invalid terrain shader stages.')
        for stage in stages: a.keys(stage, {'constants', 'images', 'samplers', 'buffers'})
        a.require(stages[1] == dict(constants=[], images=[], samplers=[], buffers=[]) and
                  stages[0]['images'] == stages[0]['samplers'] == [] and
                  stages[0]['constants'] == [dict(slot=0, hex=bytes(64).hex()), dict(slot=1, hex=bytes(64).hex())] and
                  type(stages[0]['buffers']) is list and len(stages[0]['buffers']) == 1, 'Altered terrain shader inputs.')
        buf = stages[0]['buffers'][0]; a.keys(buf, {'slot', 'type', 'stride', 'hex'})
        a.require({k: buf[k] for k in buf if k != 'hex'} == dict(slot=0, type=4, stride=4), 'Altered terrain matrix buffer format.')
        a.require(type(d['instance_count']) is int, 'Invalid terrain draw instance count.')
        ids = source['instance_indices']; a.require(type(ids) is list and len(ids) == i['instance_count'] == d['instance_count']
            and 0 < len(ids) <= 4096 and len(set(ids)) == len(ids)
            and all(type(v) is int and 0 <= v < 100000 for v in ids), 'Invalid terrain instance selection.')
        matrices = bytes.fromhex(d['stages'][0]['buffers'][0]['hex'])
        vertices = bytes.fromhex(d['streams'][0]['hex']); indices = bytes.fromhex(d['indices'])
        a.require(len(matrices) == 48*len(ids) and len(vertices) == 12*d['vertex_count']
            and len(indices) > 0 and len(indices) % 12 == 0 and len(indices)//4*len(ids) <= 2097152, 'Incomplete terrain geometry buffers.')
        a.require(sha(matrices) == source['matrices_sha256'] and sha(vertices) == source['vertices_sha256']
            and sha(indices) == source['indices_sha256'] and sha(struct.pack('<'+str(len(ids))+'I', *ids)+matrices) == i['instances_sha256'],
            'Terrain geometry or placement bytes changed.')
        for n in range(len(ids)): matrix_bits(matrices[n*48:(n+1)*48])
        a.require(all(index[0] < d['vertex_count'] for index in struct.iter_unpack('<I', indices)), 'Absent terrain triangle vertex.')
        a.require(all(abs(v[0]) <= 3.402823466e38 for v in struct.iter_unpack('<f', vertices)), 'Nonfinite terrain vertex.')
        ref = tuple(source['mesh'])
        a.require(mesh_sizes.setdefault(ref, len(vertices)) == len(vertices), 'Conflicting terrain mesh vertex count.')
        group = i['renderer_ordinal'], i['draw_ordinal']
        a.require(i['first_instance'] == ranges.get(group, 0), 'Missing, overlapping or reordered terrain source instances.')
        ranges[group] = i['first_instance']+len(ids)
        draw_count += len(ids)
    a.require(type(doc['source_draw_count']) is int and draw_count == doc['source_draw_count'], 'Terrain source draw coverage changed.')
    a.require(type(doc['unique_mesh_count']) is int and len(mesh_refs) == doc['unique_mesh_count'], 'Terrain mesh coverage changed.')
    a.require(type(doc['vertex_bytes']) is int and doc['vertex_bytes'] == sum(mesh_sizes.values()), 'Terrain vertex byte coverage changed.')
    return doc


def compare_source(raw, scene, meshes, archive_sha256, manager_record_sha256):
    """Compare each submitted instance with the source draw table and mesh bytes.

    Unlike internal packet hashes, this rejects a removed/changed group even if a
    caller recomputes the packet's own counts and hashes. No tolerance is applied.
    """
    doc = validate(raw)
    a.require(doc['scene_name'] == scene.scene_name, 'Terrain packet belongs to another source scene.')
    expected = {(d['renderer_ordinal'], d['draw_ordinal'], d['instance_ordinal']): d
                for d in scene.draws(lod=0)}
    observed = set(); cached = {}
    for row in doc['groups']:
        identity = row['placement']['identity']; source = row['source']; draw = row['rendering']['draw']
        a.require(identity['archive_sha256'] == archive_sha256 and
                  identity['manager_record_sha256'] == manager_record_sha256, 'Terrain source fingerprint differs.')
        ref = tuple(source['mesh']); slot = source['submesh']; mesh = meshes.get(ref)
        a.require(mesh is not None and slot < len(mesh.submeshes), 'Original terrain mesh or submesh is missing.')
        cache_key = ref, slot
        if cache_key not in cached:
            cached[cache_key] = (b''.join(struct.pack('<3f', *p) for p in mesh.attributes[0]),
                                 b''.join(struct.pack('<3I', *t) for t in mesh.submeshes[slot]))
        vertices, indices = cached[cache_key]
        a.require(source['mesh_record_sha256'] == mesh.binding['record_sha256'] and
                  bytes.fromhex(draw['streams'][0]['hex']) == vertices and bytes.fromhex(draw['indices']) == indices,
                  'Submitted terrain geometry differs from the original mesh.')
        matrices = bytes.fromhex(draw['stages'][0]['buffers'][0]['hex'])
        renderer = scene.renderers[identity['renderer_ordinal']] if identity['renderer_ordinal'] < len(scene.renderers) else None
        a.require(renderer is not None and list(renderer['indices'][identity['first_instance']:
                  identity['first_instance']+identity['instance_count']]) == source['instance_indices'],
                  'Submitted terrain instance order differs from the source renderer.')
        for ordinal, index in enumerate(source['instance_indices']):
            key = identity['renderer_ordinal'], identity['draw_ordinal'], index
            original = expected.get(key)
            a.require(key not in observed and original is not None and original['mesh'] == ref and
                      original['material'] == tuple(source['material']) and original['submesh'] == slot,
                      'Missing, duplicated or substituted original terrain draw.')
            a.require(matrices[ordinal*48:(ordinal+1)*48] == scene.matrix_record(index),
                      'Submitted terrain position differs from the original instance.')
            observed.add(key)
    a.require(observed == set(expected), 'Incomplete original terrain assembly.')
    return dict(status='PASSED', source_draws=len(expected), compared_groups=len(doc['groups']),
                missing_draws=0, extra_draws=0, altered_geometry=0, altered_placements=0, tolerance=0)
