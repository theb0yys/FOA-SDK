# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Source-bound affine hierarchy; no lossy TRS decomposition or native replacement.

Matrices are row-major mathematical affine transforms in source metres. The host
basis is (x,z,y). Original float32 TRS fields, record hashes and reciprocal source
hierarchy remain authoritative. Runtime scripts, RectTransform layout, renderer
baking offsets and native editing need their own consumers and qualification.
"""
from dataclasses import dataclass
import hashlib
import math

import foa_heightmap_importer as h
from foa_scene_asset_binding import embedded_tree
from foa_scene_ownership import MAX_DEPTH, MAX_RECORDS, identity

IDENTITY = (1.,0.,0.,0., 0.,1.,0.,0., 0.,0.,1.,0., 0.,0.,0.,1.)
MAX_TRANSFORM_BYTES = 1024 * 1024
MAX_TOTAL_BYTES = 256 * 1024 * 1024


def finite_values(values, count):
    if (not isinstance(values, (tuple, list)) or len(values) != count or
            any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 3.4028234663852886e38 for v in values)):
        raise h.HeightmapImportError('Invalid finite source transform values.')
    return tuple(values)


def source_vector(value, axes):
    if type(value) is not dict or set(value) != set(axes):
        raise h.HeightmapImportError('Unsupported source transform vector schema.')
    return finite_values([value[k] for k in axes], len(axes))


def affine(matrix):
    values = finite_values(matrix, 16)
    if values[12:] != (0., 0., 0., 1.):
        raise h.HeightmapImportError('Expected an affine matrix, not perspective.')
    return values


def multiply(a, b):
    a, b = affine(a), affine(b)
    return affine(tuple(sum(a[r*4+k]*b[k*4+c] for k in range(4)) for r in range(4) for c in range(4)))


def trs(position, rotation, scale):
    """Construct T*R*S using supplied quaternion coefficients, without normalizing."""
    p, q, s = finite_values(position, 3), finite_values(rotation, 4), finite_values(scale, 3)
    if abs(sum(v*v for v in q)-1.) > 1e-5:
        raise h.HeightmapImportError('Non-unit source quaternion requires separate qualification.')
    x,y,z,w = q
    rotation_rows = ((1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)),
                     (2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)),
                     (2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)))
    return affine(tuple(rotation_rows[r][c]*s[c] if c < 3 else p[r] for r in range(3) for c in range(4)) + (0.,0.,0.,1.))


def host_matrix(source):
    """Conjugate by the exact reflection that exchanges source Y and Z axes."""
    source = affine(source); order = (0, 2, 1, 3)
    return tuple(source[order[r]*4+order[c]] for r in range(4) for c in range(4))


def point(matrix, value):
    matrix, value = affine(matrix), finite_values(value, 3)
    return finite_values([sum(matrix[r*4+c]*value[c] for c in range(3))+matrix[r*4+3] for r in range(3)], 3)


def normal_matrix(matrix):
    """Exact mathematical inverse transpose; singular transforms have no inverse."""
    m = affine(matrix); a,b,c,d,e,f,g,h_,i = (m[k] for k in (0,1,2,4,5,6,8,9,10))
    cofactor = (e*i-f*h_, f*g-d*i, d*h_-e*g,
                c*h_-b*i, a*i-c*g, b*g-a*h_,
                b*f-c*e, c*d-a*f, a*e-b*d)
    determinant = a*cofactor[0]+b*cofactor[1]+c*cofactor[2]
    if determinant == 0.:
        raise h.HeightmapImportError('Singular source transform has no normal inverse.')
    return affine(tuple(cofactor[r*3+c]/determinant if c < 3 else 0. for r in range(3) for c in range(4))+(0.,0.,0.,1.))


def linear_properties(matrix):
    """Report degeneracy, reflection and shear; never discard or repair them."""
    m = affine(matrix)
    columns = [tuple(m[r*4+c] for r in range(3)) for c in range(3)]
    squared = [sum(x*x for x in col) for col in columns]
    determinant = (m[0]*(m[5]*m[10]-m[6]*m[9]) - m[1]*(m[4]*m[10]-m[6]*m[8]) + m[2]*(m[4]*m[9]-m[5]*m[8]))
    cosines = [sum(a*b for a,b in zip(columns[i],columns[j]))/math.sqrt(squared[i]*squared[j])
               for i,j in ((0,1),(0,2),(1,2)) if squared[i] and squared[j]]
    return {'determinant':determinant, 'degenerate':determinant == 0.,
            'reflected':determinant < 0., 'max_column_cosine':max(map(abs, cosines), default=0.)}


@dataclass(frozen=True)
class SourceTransform:
    key: tuple
    owner: tuple
    parent: tuple | None
    children: tuple
    kind: str
    record_sha256: str
    position: tuple
    rotation: tuple
    scale: tuple
    local: tuple
    world: tuple
    active_self: bool
    active_in_hierarchy: bool
    name: str

    def record(self):
        return {'transform':identity(self.key), 'owner':identity(self.owner),
                'parent':identity(self.parent) if self.parent else None,
                'children':[identity(k) for k in self.children], 'kind':self.kind,
                'record_sha256':self.record_sha256, 'position':self.position,
                'rotation':self.rotation, 'scale':self.scale,
                'source_local_matrix':self.local, 'source_world_matrix':self.world,
                'host_local_matrix':host_matrix(self.local), 'host_world_matrix':host_matrix(self.world),
                'active_self':self.active_self, 'active_in_hierarchy':self.active_in_hierarchy,
                'name':self.name, 'source_record_required_for_return':True,
                'runtime_component_mutations':'NOT_RUN', 'native_projection':'NOT_RUN'}


class SceneTransforms:
    """Snapshot every explicitly owned source Transform in parent-before-child order."""
    def __init__(self, ownership, cancelled=lambda: False):
        self.rows = {}; self.decoded_bytes = 0
        if not 0 < len(ownership.transforms) <= MAX_RECORDS:
            raise h.HeightmapImportError('Invalid source hierarchy size.')
        stack = [(key, 0) for key, link in reversed(tuple(ownership.transforms.items())) if link.parent is None]
        while stack:
            h.check_cancelled(cancelled)
            key, depth = stack.pop()
            if key in self.rows or depth > MAX_DEPTH:
                raise h.HeightmapImportError('Duplicate, cyclic or over-deep transform hierarchy.')
            link = ownership.transforms[key]; obj = ownership.objects[key]
            if obj.type.name not in ('Transform', 'RectTransform') or not 0 < obj.byte_size <= MAX_TRANSFORM_BYTES:
                raise h.HeightmapImportError('Unsupported transform record.')
            go = ownership.objects[link.owner]
            self.decoded_bytes += obj.byte_size + go.byte_size
            if self.decoded_bytes > MAX_TOTAL_BYTES:
                raise h.HeightmapImportError('Transform snapshot byte budget exceeded.')
            raw = obj.get_raw_data()
            if len(raw) != obj.byte_size:
                raise h.HeightmapImportError('Transform record size changed.')
            tree = embedded_tree(obj, MAX_TRANSFORM_BYTES); owner_tree = embedded_tree(go, MAX_TRANSFORM_BYTES)
            position = source_vector(tree.get('m_LocalPosition'), 'xyz')
            rotation = source_vector(tree.get('m_LocalRotation'), 'xyzw')
            scale = source_vector(tree.get('m_LocalScale'), 'xyz')
            name, active = owner_tree.get('m_Name'), owner_tree.get('m_IsActive')
            if type(name) is not str or len(name) > 16384 or '\0' in name or type(active) is not bool:
                raise h.HeightmapImportError('Unsupported source GameObject name/active fields.')
            local = trs(position, rotation, scale)
            parent = self.rows.get(link.parent)
            if link.parent is not None and parent is None:
                raise h.HeightmapImportError('Transform parent was not captured before its child.')
            world = multiply(parent.world, local) if parent else local
            if obj.get_raw_data() != raw:
                raise h.HeightmapImportError('Transform source changed while capturing hierarchy.')
            self.rows[key] = SourceTransform(key, link.owner, link.parent, link.children, obj.type.name,
                hashlib.sha256(raw).hexdigest(), position, rotation, scale, local, world,
                active, active and (parent.active_in_hierarchy if parent else True), name)
            stack.extend((child, depth+1) for child in reversed(link.children))
        if len(self.rows) != len(ownership.transforms):
            raise h.HeightmapImportError('Source hierarchy snapshot is incomplete.')

    def report(self):
        properties = [linear_properties(row.world) for row in self.rows.values()]
        return {'status':'PARTIAL', 'serialized_hierarchy':'PASSED', 'transforms':len(self.rows),
                'active_in_hierarchy':sum(row.active_in_hierarchy for row in self.rows.values()),
                'rect_transforms':sum(row.kind == 'RectTransform' for row in self.rows.values()),
                'reflected':sum(p['reflected'] for p in properties), 'degenerate':sum(p['degenerate'] for p in properties),
                'max_world_column_cosine':max(p['max_column_cosine'] for p in properties),
                'decoded_source_bytes':self.decoded_bytes, 'quaternions_normalized':False,
                'native_projection':'NOT_RUN', 'runtime_components':'NOT_RUN', 'game_export':'NOT_RUN'}
