# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Measured full-resolution Texture2D uniforms for original material layouts.

The qualified null black texture reports a different resource size from its
TexelSize uniform. Keep observed constant bytes; do not derive null uniforms from
resource dimensions. Render textures, streaming views and runtime overrides need
separate measurements and are rejected here.
"""
from dataclasses import dataclass
import struct

from foa_scene_material import integer, require, text
from foa_scene_material_upload import PROFILE, canonical, digest, _hex, _sha, pack_material_constants
from foa_scene_shader_constants import _validate

MAX_RESOURCES = 64
MAX_REQUEST_BYTES = 4 * 1024 * 1024
RESOURCE_FIELDS = {'key', 'kind', 'bundle_sha256', 'record_sha256', 'asset_path', 'reference',
                   'width', 'height', 'mip_count', 'format'}
CONTROL_OPERATIONS = ('unset', 'global-with-unset', 'global-with-material-null',
                      'non-square', 'non-square-st', 'null-after-control')


def _context(color_space, mip_limit, streaming):
    require(color_space in ('Linear', 'Gamma') and type(mip_limit) is int and mip_limit == 0 and
            streaming is False, 'Only measured full-resolution, non-streaming texture views are qualified.')


def _reference(value):
    require(type(value) in (tuple, list) and len(value) == 2, 'Exact Texture2D source reference required.')
    text(value[0]); integer(value[1], -(1 << 63), (1 << 63) - 1)
    require(value[1] != 0, 'Null Texture2D source reference.')
    return tuple(value)


def _resource(value):
    require(type(value) is dict and set(value) == RESOURCE_FIELDS, 'Invalid texture uniform resource descriptor.')
    row = dict(value)
    if row['kind'] == 'black-default':
        require(row['reference'] is None and row['bundle_sha256'] == row['record_sha256'] == row['asset_path'] == '' and
                all(type(row[k]) is int and row[k] == 0 for k in ('width', 'height', 'mip_count', 'format')),
                'Default texture cannot carry source resource fields.')
    else:
        require(row['kind'] == 'asset', 'Unqualified texture resource kind.')
        row['reference'] = list(_reference(row['reference']))
        _sha(row['bundle_sha256']); _sha(row['record_sha256']); text(row['asset_path'])
        integer(row['width'], 1, 8192); integer(row['height'], 1, 8192)
        integer(row['mip_count'], 1, max(row['width'], row['height']).bit_length())
        require(type(row['format']) is int and row['format'] in (3, 26), 'Texture format has no uniform measurement lane.')
    _sha(row['key'])
    require(row['key'] == digest({k: v for k, v in row.items() if k != 'key'}), 'Stale texture resource descriptor.')
    return row


def black_default():
    row = dict(kind='black-default', bundle_sha256='', record_sha256='', asset_path='', reference=None,
               width=0, height=0, mip_count=0, format=0)
    return dict(row, key=digest(row))


def source_resource(reference, texture, *, bundle_sha256, record_sha256, asset_path):
    """The source reader owns exact bundle/container/record identity verification."""
    require(type(texture) is dict and type(texture.get('m_TextureDimension')) is int and texture['m_TextureDimension'] == 2 and
            type(texture.get('m_ColorSpace')) is int and texture['m_ColorSpace'] == 0, 'Qualified linear source Texture2D required.')
    row = dict(kind='asset', reference=list(_reference(reference)), bundle_sha256=bundle_sha256,
               record_sha256=record_sha256, asset_path=asset_path, width=texture.get('m_Width'), height=texture.get('m_Height'),
               mip_count=texture.get('m_MipCount'), format=texture.get('m_TextureFormat'))
    return _resource(dict(row, key=digest(row)))


def uniform_request(resources, *, color_space, mip_limit, streaming):
    _context(color_space, mip_limit, streaming)
    require(type(resources) in (tuple, list) and 0 < len(resources) <= MAX_RESOURCES, 'Texture uniform batch exceeds its bound.')
    rows = [_resource(r) for r in resources]
    require(len({r['key'] for r in rows}) == len(rows), 'Duplicate texture measurement identity.')
    refs = [tuple(r['reference']) for r in rows if r['kind'] == 'asset']
    require(len(set(refs)) == len(refs), 'Multiple resource versions for one source texture.')
    result = dict(schema='foa-texture-uniform-request', version=1, profile=PROFILE, color_space=color_space,
                  mip_limit=mip_limit, streaming=streaming, resources=rows)
    require(len(canonical(result)) <= MAX_REQUEST_BYTES, 'Texture uniform request exceeds its byte bound.')
    return result


@dataclass(frozen=True)
class TextureUniformUpload:
    resource_sha256: str
    color_space: str
    value: bytes
    dimensions: bytes


def _measurement(row, *, key, operation, shape):
    require(type(row) is dict and set(row) == {'key', 'operation', 'hex', 'dimensions_hex', 'width', 'height', 'mip_count', 'format'} and
            row['key'] == key and row['operation'] == operation, 'Texture measurement identity/operation mismatch.')
    for name, expected in zip(('width', 'height', 'mip_count', 'format'), shape):
        require(type(row[name]) is int and row[name] == expected, 'Measured Texture2D metadata differs from the requested source.')
    raw = _hex(row['hex'], 16); dimensions = _hex(row['dimensions_hex'], 16)
    return raw, dimensions


def _qualified_values(source, raw, dimensions):
    if source['kind'] == 'black-default':
        require(raw == struct.pack('<4f', 1, 1, 1, 1) and dimensions == struct.pack('<4f', 4, 4, 1, 1),
                'Default texture uniform does not match its pinned measurement.')
    else:
        values = struct.unpack('<4f', raw)
        require(values[0] > 0 and values[1] > 0 and values[2:] == (source['width'], source['height']) and
                dimensions == struct.pack('<4f', source['width'], source['height'], source['mip_count'], 1),
                'Measured texture view is not the requested full-resolution source.')


def read_uniforms(request, receipt):
    """Validate local GPU evidence, not authenticity or live game interpretation."""
    require(type(request) is dict and set(request) == {'schema', 'version', 'profile', 'color_space', 'mip_limit', 'streaming', 'resources'} and
            request['schema'] == 'foa-texture-uniform-request' and type(request['version']) is int and request['version'] == 1 and
            request['profile'] == PROFILE, 'Invalid texture uniform request.')
    expected = uniform_request(request['resources'], color_space=request['color_space'], mip_limit=request['mip_limit'], streaming=request['streaming'])
    require(expected == request, 'Texture request has noncanonical source identities.')
    require(type(receipt) is dict and set(receipt) == {'schema', 'version', 'profile', 'color_space', 'mip_limit', 'streaming',
            'input_sha256', 'status', 'api', 'threading', 'resources', 'controls'} and
            receipt['schema'] == 'foa-texture-uniform-receipt' and type(receipt['version']) is int and receipt['version'] == 1 and
            receipt['profile'] == PROFILE and receipt['color_space'] == request['color_space'] and receipt['status'] == 'PASSED' and
            receipt['api'] == 'Direct3D11' and receipt['threading'] == 'Direct' and receipt['input_sha256'] == digest(request),
            'Texture uniform receipt does not match its exact request/host.')
    _context(receipt['color_space'], receipt['mip_limit'], receipt['streaming'])
    controls = receipt['controls']
    require(type(controls) is list and len(controls) == len(CONTROL_OPERATIONS), 'Missing texture uniform calibration cases.')
    for control, operation in zip(controls, CONTROL_OPERATIONS):
        non_square = operation in ('non-square', 'non-square-st')
        raw, dimensions = _measurement(control, key='control', operation=operation, shape=(7, 3, 1, 4) if non_square else (0, 0, 0, 0))
        # Exact bytes from the pinned direct-GPU calibration. The 4x4 fallback
        # resource deliberately does NOT determine its (1,1,1,1) uniform.
        require(raw.hex() == ('2549123eabaaaa3e0000e04000004040' if non_square else '0000803f' * 4) and
                dimensions == struct.pack('<4f', *( (7, 3, 1, 1) if non_square else (4, 4, 1, 1))),
                'Texture uniform calibration differs from the qualified profile.')
    measured = receipt['resources']
    require(type(measured) is list and len(measured) == len(request['resources']), 'Incomplete texture uniform receipt.')
    result = {}
    for source, row in zip(request['resources'], measured):
        raw, dimensions = _measurement(row, key=source['key'], operation='material',
                                       shape=tuple(source[k] for k in ('width', 'height', 'mip_count', 'format')))
        _qualified_values(source, raw, dimensions)
        result[source['key']] = TextureUniformUpload(source['key'], request['color_space'], raw, dimensions)
    return result


def pack_textured_material_constants(layout, key, binding, numeric_upload, *, texture_resources, texture_uploads,
                                     color_space, mip_limit, streaming, explicit_values):
    """Join measured texture and numeric inputs; retain the numeric v1 contract.

    texture_resources must describe the resources selected by the current source
    texture reader. A changed reference, record, bundle or view cannot reuse an
    earlier upload. Live overrides and undeclared uniforms remain caller-owned.
    """
    _context(color_space, mip_limit, streaming)
    require(type(texture_resources) is dict and len(texture_resources) <= MAX_RESOURCES and
            type(texture_uploads) is dict and len(texture_uploads) <= MAX_RESOURCES and type(explicit_values) is dict,
            'Bounded texture resources, uploads and explicit values required.')
    _validate(layout)
    require(type(binding) is dict and type(binding.get('properties')) is dict, 'Invalid source material binding.')
    values = dict(explicit_values)
    name = '_HeightMap_TexelSize'
    if name in layout['fields']:
        require(name not in values, 'Explicit values cannot override a measured texture uniform.')
        props = binding.get('properties', {})
        prop = props.get('_HeightMap'); require(type(prop) is dict, 'Source height texture property is missing.')
        declaration = prop.get('declaration', {}); texture = prop.get('texture', {})
        require(type(declaration) is dict and type(texture) is dict, 'Invalid source texture declaration/binding.')
        require(name not in props and prop.get('type') == 'Texture' and type(prop.get('flags')) is int and prop['flags'] == 0 and
                declaration.get('m_Name') == '_HeightMap' and type(declaration.get('m_Type')) is int and declaration['m_Type'] == 4 and
                type(declaration.get('m_Flags')) is int and declaration['m_Flags'] == 0 and declaration.get('m_Attributes') == [] and
                declaration.get('m_DefTexture') == {'m_DefaultName': 'black', 'm_TexDim': 2} and
                texture.get('default') == declaration['m_DefTexture'], 'Texture declaration is outside the measured profile.')
        reference = texture.get('reference')
        if reference is None:
            require(texture.get('value_origin') == 'shader-default-unresolved' and texture.get('saved_reference') is None,
                    'Unqualified null source texture binding.')
            resource = black_default()
        else:
            reference = _reference(reference)
            require(texture.get('value_origin') == 'material' and _reference(texture.get('saved_reference')) == reference and
                    reference in texture_resources, 'Current source texture resource is missing or overridden.')
            resource = _resource(texture_resources[reference])
            require(resource['kind'] == 'asset' and tuple(resource['reference']) == reference, 'Texture source identity mismatch.')
        upload = texture_uploads.get(resource['key'])
        require(type(upload) is TextureUniformUpload and upload.resource_sha256 == resource['key'] and upload.color_space == color_space and
                type(upload.value) is bytes and type(upload.dimensions) is bytes, 'Stale, missing or mismatched texture uniform upload.')
        raw = _hex(upload.value.hex(), 16); dimensions = _hex(upload.dimensions.hex(), 16)
        _qualified_values(resource, raw, dimensions)
        field = layout['fields'][name]
        require(field['type'] == 0 and field['columns'] == 4 and not field['matrix'] and not field['array_size'],
                'Texture uniform must use its measured float4 shape.')
        values[name] = struct.unpack('<4f', raw)
    return pack_material_constants(layout, key, binding, numeric_upload, color_space=color_space, explicit_values=values)
