# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Exact source material bindings; no shader substitution or game writes.

The result describes serialized declarations, not effective renderer/runtime state.
Source records retain authority. Shader program/parameter blobs, shared samplers,
quality overrides, renderer properties and GPU UV orientation need separate proof.
"""
from copy import deepcopy
from dataclasses import dataclass
import math

import foa_heightmap_importer as h
from foa_scene_component_audit import reference_key

MAX_PROPERTIES = 4096
CATEGORIES = ('m_TexEnvs', 'm_Ints', 'm_Floats', 'm_Colors')
PROPERTY_TYPES = ('Color', 'Vector', 'Float', 'Range', 'Texture')


def require(condition, message):
    if not condition:
        raise h.HeightmapImportError(message)


def number(value):
    require(type(value) in (int, float) and math.isfinite(value) and abs(value) <= 3.4028234663852886e38,
            'Invalid material number.')
    return value


def integer(value, low, high):
    require(type(value) is int and low <= value <= high, 'Invalid material integer.')
    return value


def text(value):
    require(type(value) is str and 0 < len(value) <= 1024 and '\0' not in value, 'Invalid material property name.')
    return value


def bounded_copy(value):
    # Check before deep-copying: bound aggregate nodes/text as well as depth.
    pending = [(value, 0)]
    nodes = chars = 0
    while pending:
        item, depth = pending.pop()
        nodes += 1
        require(nodes <= 100000 and depth <= 24, 'Material tree exceeds its bounds.')
        if isinstance(item, dict):
            require(len(item) <= MAX_PROPERTIES, 'Material object exceeds its bounds.')
            for k, v in item.items():
                require(type(k) is str, 'Invalid material field name.')
                chars += len(k)
                pending.append((v, depth+1))
        elif isinstance(item, (list, tuple)):
            require(len(item) <= MAX_PROPERTIES, 'Material array exceeds its bounds.')
            pending.extend((v, depth+1) for v in item)
        elif type(item) is str:
            chars += len(item)
        else:
            require(type(item) in (int, float, bool, type(None)), 'Unsupported material field value.')
            if type(item) in (int, float): number(item)
        require(chars <= 2*1024*1024, 'Material text exceeds its bounds.')
    return deepcopy(value)


def pairs(values):
    require(type(values) is list and len(values) <= MAX_PROPERTIES, 'Invalid material property table.')
    result = {}
    for pair in values:
        require(isinstance(pair, (list, tuple)) and len(pair) == 2, 'Invalid material property pair.')
        name = text(pair[0])
        require(name not in result, 'Duplicate material property name.')
        result[name] = pair[1]
    return result


def vector(value, components):
    require(type(value) is dict and set(value) == set(components), 'Invalid material vector.')
    return tuple(number(value[k]) for k in components)


def optional_reference(asset, pointer):
    require(type(pointer) is dict and set(pointer) == {'m_FileID', 'm_PathID'}, 'Invalid material reference.')
    integer(pointer['m_FileID'], 0, len(asset.externals))
    integer(pointer['m_PathID'], -(1 << 63), (1 << 63)-1)
    return reference_key(asset, pointer) if pointer['m_PathID'] else None


@dataclass(frozen=True)
class Sampler:
    filter: int
    anisotropy: int
    bias: float
    u: int
    v: int
    w: int

    def __post_init__(self):
        integer(self.filter,0,2)
        integer(self.anisotropy,0,16)
        number(self.bias)
        for value in (self.u,self.v,self.w): integer(value,0,3)

    @classmethod
    def from_tree(cls, tree):
        keys = ('m_FilterMode', 'm_Aniso', 'm_MipBias', 'm_WrapU', 'm_WrapV', 'm_WrapW')
        require(type(tree) is dict and set(tree) == set(keys), 'Unqualified material sampler schema.')
        return cls(integer(tree[keys[0]], 0, 2), integer(tree[keys[1]], 0, 16), number(tree[keys[2]]),
                   *(integer(tree[k], 0, 3) for k in keys[3:]))

    def rhi_state(self, *, effective_anisotropy, mip_count):
        """Translate explicit sampling policy, never infer quality/inline overrides.

        Caller must first prove which sampler a shader uses and the effective
        anisotropy. This is descriptor construction, not native GPU qualification.
        Enum values follow Unity GraphicsEnums and the pinned RHI SamplerState.
        """
        integer(effective_anisotropy, 0, 16)
        integer(mip_count, 1, 14)
        require(not (self.filter == 0 and effective_anisotropy > 1), 'Point plus anisotropy is unqualified.')
        address = ('Wrap', 'Clamp', 'Mirror', 'MirrorOnce')
        return {'anisotropyEnable': int(effective_anisotropy > 1),
                'anisotropyMax': max(1, effective_anisotropy),
                'filterMin': 'Point' if self.filter == 0 else 'Linear',
                'filterMag': 'Point' if self.filter == 0 else 'Linear',
                'filterMip': 'Linear' if self.filter == 2 else 'Point',
                'addressU': address[self.u], 'addressV': address[self.v], 'addressW': address[self.w],
                'mipLodMin': 0, 'mipLodMax': mip_count-1, 'mipLodBias': self.bias,
                'reductionType': 'Filter', 'comparisonFunc': 'Always'}


def source_uv_from_native(native_uv, scale, offset):
    """Recover source UV and apply source ST after the qualified mesh V reflection.

    This is subject to native float precision; original UVs retain return authority.
    It does not choose a texture, UV channel or GPU image orientation. Shader
    consumers must prove those separately. Keep this operation before nonlinear
    wrapping/procedural UV operations; it cannot be replaced by flipping an image.
    """
    require(all(isinstance(v, (list, tuple)) and len(v) == 2 for v in (native_uv, scale, offset)),
            'Invalid material UV transform.')
    u, v = map(number, native_uv)
    sx, sy = map(number, scale)
    ox, oy = map(number, offset)
    return number(u*sx+ox), number((1-v)*sy+oy)


def texture_value(asset, value):
    require(type(value) is dict and set(value) == {'m_Texture', 'm_Scale', 'm_Offset'},
            'Unqualified material texture environment.')
    return {'reference': optional_reference(asset, value['m_Texture']),
            'scale': vector(value['m_Scale'], 'xy'), 'offset': vector(value['m_Offset'], 'xy')}


def bind_material(asset, material, shader_asset, shader_tree, texture_lookup, cancelled=lambda: False):
    """Join exact Shader PPtr and property names; preserve every saved field.

    shader_asset must expose name/path_id/externals. texture_lookup(key) returns
    explicit source texture metadata (m_TextureDimension, m_TextureSettings).
    It must reject absent or ambiguous identities, rather than search by name.
    """
    h.check_cancelled(cancelled)
    source = bounded_copy(material)
    require(type(source) is dict, 'Invalid material tree.')
    shader_key = reference_key(asset, source.get('m_Shader'))
    require(shader_key == (str(shader_asset.name).lower(), shader_asset.path_id), 'Material shader identity mismatch.')
    saved = source.get('m_SavedProperties')
    require(type(saved) is dict and set(saved) == set(CATEGORIES), 'Unqualified saved material schema.')
    tables = {category: pairs(saved[category]) for category in CATEGORIES}
    require(sum(map(len, tables.values())) <= MAX_PROPERTIES, 'Material property count exceeds its bound.')
    textures = {}
    for name, value in tables['m_TexEnvs'].items():
        textures[name] = texture_value(asset, value)
    for value in tables['m_Ints'].values(): integer(value, -(1 << 31), (1 << 31)-1)
    for value in tables['m_Floats'].values(): number(value)
    for value in tables['m_Colors'].values(): vector(value, 'rgba')
    props = shader_tree.get('m_ParsedForm', {}).get('m_PropInfo', {}).get('m_Props')
    require(type(props) is list and len(props) <= MAX_PROPERTIES, 'Invalid shader property declarations.')
    locked = pairs(shader_tree.get('m_NonModifiableTextures'))
    declarations = {}
    for prop in props:
        h.check_cancelled(cancelled)
        require(type(prop) is dict, 'Invalid shader property declaration.')
        name = text(prop.get('m_Name'))
        require(name not in declarations, 'Duplicate shader property declaration.')
        kind = integer(prop.get('m_Type'), 0, 4)
        flags = integer(prop.get('m_Flags'), 0, 0xffffffff)
        defaults = tuple(number(prop.get('m_DefValue['+str(i)+']')) for i in range(4))
        category = CATEGORIES[0] if kind == 4 else ('m_Colors' if kind in (0, 1) else 'm_Floats')
        declaration = {'type': PROPERTY_TYPES[kind], 'flags': flags,
                       'declaration': bounded_copy(prop), 'saved': name in tables[category]}
        if kind == 4:
            default_texture = prop.get('m_DefTexture')
            require(type(default_texture) is dict and set(default_texture) == {'m_DefaultName','m_TexDim'},
                    'Unqualified shader texture default.')
            integer(default_texture['m_TexDim'], 0, 6)
            require(type(default_texture['m_DefaultName']) is str and len(default_texture['m_DefaultName']) <= 1024,
                    'Invalid shader texture default name.')
            binding = deepcopy(textures.get(name, {'reference':None, 'scale':(1,1), 'offset':(0,0)}))
            binding['default'] = deepcopy(default_texture)
            binding['saved_reference'] = binding['reference']
            if name in locked:
                require(flags & 64, 'Shader-owned texture lacks its nonmodifiable declaration.')
                binding['shader_reference'] = optional_reference(shader_asset, locked[name])
                binding['reference'] = binding['shader_reference']
            binding['value_origin'] = ('shader-owned' if name in locked else
                                       'material' if binding['reference'] else 'shader-default-unresolved')
            if binding['reference'] is not None:
                info = texture_lookup(binding['reference'])
                require(type(info) is dict and info.get('m_TextureDimension') == default_texture['m_TexDim'],
                        'Texture dimension disagrees with its shader declaration.')
                Sampler.from_tree(info.get('m_TextureSettings'))
                binding['saved_sampler'] = deepcopy(info['m_TextureSettings'])
            declaration['texture'] = binding
        else:
            declaration['value'] = deepcopy(tables[category][name]) if declaration['saved'] else defaults if kind in (0,1) else defaults[0]
            declaration['value_origin'] = 'material' if declaration['saved'] else 'shader-default'
        declarations[name] = declaration
    require(set(locked) <= {n for n,d in declarations.items() if d['type'] == 'Texture'},
            'Shader-owned texture has no matching texture declaration.')
    # Saved values not declared in the current shader remain intact, including
    # differently typed values under the same name. Compiled uniform usage is a
    # separate check; absence from ShaderLab Properties does not prove disuse.
    stale = {category:[name for name in values if name not in declarations or
                      (category != ('m_TexEnvs' if declarations[name]['type'] == 'Texture' else
                                    'm_Colors' if declarations[name]['type'] in ('Color','Vector') else 'm_Floats'))]
             for category,values in tables.items()}
    h.check_cancelled(cancelled)
    return {'schema':'foa-source-material-binding', 'version':1, 'shader_reference':shader_key,
            'properties':declarations, 'undeclared_saved_properties':stale, 'source':source,
            'source_binding':'PASSED', 'native_shader_mapping':'NOT_RUN', 'game_export':'NOT_RUN'}


def inline_sampler(encoded):
    """Decode qualified serialized inline filter/wrap/comparison/anisotropy bits.

    Filter/wrap/comparison agree with the archived shader reader. Anisotropy
    codes 0..4 were measured with the synthetic Unity 6000.0.64f1 compiler
    matrix (none, 2, 4, 8, 16). This is distinct from TextureSettings. Comparison function, LOD and quality policy
    are not encoded here and must not be invented by a rendering consumer.
    """
    integer(encoded,0,0xffffffff)
    filter_mode = encoded & 3
    require(filter_mode < 3, 'Unqualified inline sampler filter bits.')
    anisotropy_code = (encoded >> 9) & 7
    require(anisotropy_code <= 4, 'Unqualified inline sampler anisotropy code.')
    return {'filter':('Point','Bilinear','Trilinear')[filter_mode],
            'wrap_u':('Repeat','Clamp','Mirror','MirrorOnce')[(encoded >> 2)&3],
            'wrap_v':('Repeat','Clamp','Mirror','MirrorOnce')[(encoded >> 4)&3],
            'wrap_w':('Repeat','Clamp','Mirror','MirrorOnce')[(encoded >> 6)&3],
            'comparison':bool(encoded & 0x100), 'encoded':encoded,
            'requested_anisotropy':(0,2,4,8,16)[anisotropy_code],
            'unresolved_bits':encoded & ~0xfff,
            'encoding_status':'PARTIAL' if encoded & ~0xfff else 'PASSED'}
