# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Build a binary glTF geometry projection with explicit source channel mapping.

Material slots have neutral preview materials until the source shader mapping is
qualified. Original records remain the return-path authority. This is neither a
native game exporter nor a claim that host GPU vertex IDs retain source topology.
"""
import json
import math
import struct
from foa_scene_mesh import finite_rows, MAX_VERTICES, MAX_BYTES, MAX_SUBMESHES
import foa_heightmap_importer as h


def f32(value):
    try:
        result=struct.unpack('<f',struct.pack('<f',value))[0]
    except (OverflowError,struct.error) as error:
        raise h.HeightmapImportError('Mesh value exceeds float32 range.') from error
    if not math.isfinite(result):raise h.HeightmapImportError('Nonfinite mesh projection value.')
    return result


def unit(row):
    length=math.sqrt(sum(v*v for v in row))
    if not math.isfinite(length) or length==0:
        raise h.HeightmapImportError('Zero or nonfinite direction cannot be normalized for glTF.')
    return tuple(f32(v/length) for v in row)


def encode_glb(mesh, cancelled=lambda:False):
    attrs=mesh.attributes;count=len(attrs[0])
    if not 1 <= len(mesh.submeshes) <= MAX_SUBMESHES:
        raise h.HeightmapImportError("Invalid projection material slot count.")
    if not 0<count<=MAX_VERTICES:raise h.HeightmapImportError('Invalid geometry projection vertex count.')
    if any(k not in range(12) for k in attrs):raise h.HeightmapImportError('Skin or unknown mesh attributes require another mapping.')
    doc={'asset':{'version':'2.0','generator':'FOA source mesh projection','extras':{'foaSourceMeshProjection':1}},'scene':0,
         'scenes':[{'nodes':[0]}],'nodes':[{'mesh':0,'name':'SourceMesh',
             'matrix':[-1,0,0,0, 0,0,1,0, 0,1,0,0, 0,0,0,1]}],
         'meshes':[{'name':'SourceMesh','primitives':[]}],'buffers':[{'byteLength':0}],
         'bufferViews':[],'accessors':[],'materials':[]}
    data=bytearray();mapping=[];attributes={}
    def accessor(rows,component_type=5126,target=34962):
        if not rows:raise h.HeightmapImportError('Empty glTF accessor.')
        dimension=len(rows[0]);shape={1:'SCALAR',2:'VEC2',3:'VEC3',4:'VEC4'}.get(dimension)
        if shape is None:raise h.HeightmapImportError('Unsupported glTF attribute dimension.')
        if len(data)+len(rows)*dimension*4>MAX_BYTES:
            raise h.HeightmapImportError('Mesh projection exceeds its binary budget.')
        while len(data)%4:data.append(0)
        start=len(data);pack=struct.Struct('<'+str(dimension)+('f' if component_type==5126 else 'I')).pack
        for i,row in enumerate(rows):
            if i%4096==0:h.check_cancelled(cancelled)
            data.extend(pack(*(f32(v) for v in row)) if component_type==5126 else pack(*row))
        view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':start,'byteLength':len(data)-start,'target':target})
        value={'bufferView':view,'componentType':component_type,'count':len(rows),'type':shape}
        index=len(doc['accessors']);doc['accessors'].append(value)
        return index
    # Keep every vector in canonical (x,z,y) mesh-local coordinates. The explicit
    # root rotation maps these vectors into glTF's Y-up world. At the pinned host,
    # AssImpReadRootTransform=true composes its inverse rotation with this root,
    # so positions, normals AND unconverted tangent streams retain one basis.
    positions=finite_rows(attrs[0],count,(3,),'position')
    projected=[(x,z,y) for x,y,z in positions]
    attributes['POSITION']=accessor(projected)
    pos=doc['accessors'][attributes['POSITION']]
    pos['min']=[f32(min(row[i] for row in projected)) for i in range(3)]
    pos['max']=[f32(max(row[i] for row in projected)) for i in range(3)]
    mapping.append({'source_channel':0,'attribute':'POSITION','conversion':'mesh-local (x,z,y); explicit glTF Y-up root rotation'})
    if 1 in attrs:
        normals=finite_rows(attrs[1],count,(3,4),'normal')
        attributes['NORMAL']=accessor([unit((row[0],row[2],row[1])) for row in normals])
        mapping.append({'source_channel':1,'attribute':'NORMAL','conversion':'xyz-swap-y-z-normalize; original values retained'})
        if len(normals[0])==4:
            attributes['_SOURCE_NORMAL_W']=accessor([(row[3],) for row in normals])
            mapping.append({'source_channel':1,'component':'w','attribute':'_SOURCE_NORMAL_W','host_mapping':'NOT_RUN'})
    if 2 in attrs:
        tangents=finite_rows(attrs[2],count,(4,),'tangent')
        if any(row[3] not in (-1.,1.) for row in tangents):
            raise h.HeightmapImportError('glTF tangent handedness requires an explicit source sign.')
        # Reflection and the UV V inversion each reverse tangent handedness.
        attributes['TANGENT']=accessor([(*unit((row[0],row[2],row[1])),row[3]) for row in tangents])
        mapping.append({'source_channel':2,'attribute':'TANGENT','conversion':'xyz-swap-y-z-normalize; w preserved after basis and V reflections'})
    if 3 in attrs:
        colors=finite_rows(attrs[3],count,(4,),'color')
        if any(not 0<=v<=1 for row in colors for v in row):
            raise h.HeightmapImportError('glTF vertex color is outside the qualified linear [0,1] range.')
        attributes['COLOR_0']=accessor(colors)
        mapping.append({'source_channel':3,'attribute':'COLOR_0','conversion':'linear normalized values'})
    uv_slot=0
    for source in sorted(k for k in attrs if 4<=k<12):
        values=finite_rows(attrs[source],count,(2,),'UV')
        semantic='TEXCOORD_'+str(uv_slot)
        attributes[semantic]=accessor([(u,1-v) for u,v in values])
        mapping.append({'source_channel':source,'attribute':semantic,'conversion':'V to 1-V; explicit source UV ordinal retained'})
        uv_slot+=1
    for slot,triangles in enumerate(mesh.submeshes):
        if not triangles:
            raise h.HeightmapImportError('An empty source material slot needs an explicit host mapping.')
        if any(len(t)!=3 or any(type(i) is not int or not 0<=i<count for i in t) for t in triangles):
            raise h.HeightmapImportError('Invalid source triangle in geometry projection.')
        indices=accessor([(i,) for a,b,c in triangles for i in (a,c,b)],5125,34963)
        doc['meshes'][0]['primitives'].append({'attributes':dict(attributes),'indices':indices,'mode':4,'material':slot})
        doc['materials'].append({'name':'SourceSlot'+str(slot),
            'pbrMetallicRoughness':{'metallicFactor':0.,'roughnessFactor':1.},
            'extras':{'source_slot':slot,'source_material_mapping':'NOT_RUN'}})
    doc['buffers'][0]['byteLength']=len(data)
    doc['meshes'][0]['extras']={'source_binding':mesh.binding,'channel_mapping':mapping,
        'original_record_required_for_return':True,'source_material_mapping':'NOT_RUN'}
    encoded=json.dumps(doc,separators=(',',':'),allow_nan=False).encode('utf-8')
    if len(encoded)>1024*1024:raise h.HeightmapImportError('Mesh metadata exceeds its bound.')
    encoded+=b' '*((-len(encoded))%4);data.extend(b'\0'*((-len(data))%4))
    result=(struct.pack('<III',0x46546C67,2,12+8+len(encoded)+8+len(data))+
            struct.pack('<II',len(encoded),0x4E4F534A)+encoded+
            struct.pack('<II',len(data),0x004E4942)+data)
    return result,{'vertices':count,'triangles':sum(map(len,mesh.submeshes)),
        'material_slots':len(mesh.submeshes),'channel_mapping':mapping,
        'source_material_mapping':'NOT_RUN','native_processing':'NOT_RUN','required_source_suffix':'_foamesh.glb','source_preservation_schema':1,'game_export':'NOT_RUN'}
