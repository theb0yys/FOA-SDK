# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Bind measured Unity material uploads to exact source properties and layouts.

Serialized Color values, Color.linear and actual shader uploads can have different
float bits. This path consumes direct host readback; it does not approximate that
conversion or infer shader defaults, renderer overrides or live global values.
"""
from dataclasses import dataclass
import hashlib
import json
import math
import re
import struct

from foa_scene_material import integer, number, require, text, vector
from foa_scene_shader_constants import pack_constants, _validate

PROFILE = 'unity-6000.0.64f1-d3d11'
MAX_MATERIALS = 1024
MAX_PROPERTIES = 4096
MAX_BATCH_PROPERTIES = 262144
MAX_REQUEST_BYTES = 64 * 1024 * 1024
FLAGS = {'Color': {0, 1, 16, 256}, 'Vector': {0, 1, 32},
         'Float': {0, 1, 32}, 'Range': {0, 1, 32}}


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=True, allow_nan=False).encode('ascii')


def digest(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def _key(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{32}', value), 'Exact material source key required.')
    return value


def _sha(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{64}', value), 'Invalid material upload digest.')
    return value


def _hex(value, size):
    require(type(value) is str and len(value) == size * 2 and re.fullmatch('[0-9a-f]+', value),
            'Invalid material upload bytes.')
    result = bytes.fromhex(value)
    require(all(math.isfinite(v) for v in struct.unpack('<' + 'f' * (size // 4), result)),
            'Nonfinite material upload is not qualified.')
    return result


def material_request(key, binding):
    """Describe every saved numeric property with exact little-endian input bits."""
    _key(key)
    require(type(binding) is dict, 'Invalid source material binding.')
    require(binding.get('schema') == 'foa-source-material-binding' and type(binding.get('version')) is int and
            binding['version'] == 1 and binding.get('source_binding') == 'PASSED', 'Qualified source material binding required.')
    shader = binding.get('shader_reference')
    require(type(shader) in (list, tuple) and len(shader) == 2 and type(shader[0]) is str and
            type(shader[1]) is int and -(1 << 63) <= shader[1] < (1 << 63) and shader[1] != 0, 'Exact material shader reference required.')
    props = binding.get('properties')
    require(type(props) is dict and len(props) <= MAX_PROPERTIES, 'Material property count exceeds its bound.')
    rows = []
    for name, prop in sorted(props.items()):
        text(name)
        require(type(prop) is dict, 'Invalid material declaration.')
        kind = prop.get('type')
        if kind == 'Texture':
            continue
        require(kind in FLAGS and type(prop.get('flags')) is int and prop['flags'] in FLAGS[kind],
                'Material property flags are not qualified for upload.')
        require(prop.get('saved') is True and prop.get('value_origin') == 'material',
                'Shader-default numeric uploads require separate original-host evidence.')
        values = vector(prop['value'], 'rgba') if kind in ('Color', 'Vector') else (number(prop['value']),)
        raw = struct.pack('<' + 'f' * len(values), *values)
        rows.append({'name': name, 'kind': kind, 'flags': prop['flags'], 'hex': raw.hex()})
    require(rows, 'Material has no saved numeric properties to measure.')
    source_sha = digest({'key': key, 'shader_reference': shader, 'properties': rows})
    return {'key': key, 'source_sha256': source_sha, 'properties': rows}


def upload_request(materials, *, color_space):
    require(color_space in ('Linear', 'Gamma'), 'Explicit qualified color space required.')
    require(type(materials) in (list, tuple) and 0 < len(materials) <= MAX_MATERIALS, 'Material batch exceeds its bound.')
    rows = [material_request(key, binding) for key, binding in materials]
    require(len({r['key'] for r in rows}) == len(rows), 'Duplicate material upload identity.')
    require(sum(len(r['properties']) for r in rows) <= MAX_BATCH_PROPERTIES, 'Material batch work exceeds its bound.')
    result = {'schema': 'foa-material-upload-request', 'version': 1, 'profile': PROFILE,
              'color_space': color_space, 'materials': rows}
    require(len(canonical(result)) <= MAX_REQUEST_BYTES, 'Material upload request exceeds its byte bound.')
    return result


@dataclass(frozen=True)
class MaterialUpload:
    key: str
    source_sha256: str
    color_space: str
    values: tuple


def read_uploads(request, receipt):
    """Validate an isolated GPU readback against the exact submitted request.

    The receipt is local measurement evidence, not an authenticity certificate or
    proof of game rendering. The caller retains responsibility for its producer.
    """
    require(type(request) is dict and set(request) == {'schema', 'version', 'profile', 'color_space', 'materials'} and
            request['schema'] == 'foa-material-upload-request' and type(request['version']) is int and request['version'] == 1 and
            request['profile'] == PROFILE and request['color_space'] in ('Linear', 'Gamma'), 'Invalid material upload request.')
    rows = request['materials']
    require(type(rows) is list and 0 < len(rows) <= MAX_MATERIALS and len(canonical(request)) <= MAX_REQUEST_BYTES,
            'Invalid material batch size.')
    require(type(receipt) is dict and set(receipt) == {'schema', 'version', 'profile', 'color_space', 'input_sha256',
            'status', 'api', 'threading', 'materials', 'shader_count'} and
            receipt['schema'] == 'foa-material-upload-receipt' and type(receipt['version']) is int and receipt['version'] == 1 and
            receipt['status'] == 'PASSED' and receipt['profile'] == PROFILE and receipt['api'] == 'Direct3D11' and
            receipt['threading'] == 'Direct' and receipt['color_space'] == request['color_space'] and
            receipt['input_sha256'] == digest(request), 'Material upload receipt does not match the exact request/host.')
    integer(receipt['shader_count'], 1, MAX_MATERIALS)
    measured = receipt['materials']
    require(type(measured) is list and len(measured) == len(rows), 'Material upload receipt is incomplete.')
    results = {}; total = 0
    for expected, actual in zip(rows, measured):
        require(type(expected) is dict and set(expected) == {'key', 'source_sha256', 'properties'}, 'Invalid source upload row.')
        key = _key(expected['key']); _sha(expected['source_sha256'])
        require(key not in results, 'Duplicate measured material identity.')
        props = expected['properties']; total += len(props) if type(props) is list else MAX_BATCH_PROPERTIES + 1
        require(type(props) is list and 0 < len(props) <= MAX_PROPERTIES and total <= MAX_BATCH_PROPERTIES,
                'Measured material property count exceeds its bound.')
        require(type(actual) is dict and set(actual) == {'key', 'source_sha256', 'properties'} and
                actual['key'] == key and actual['source_sha256'] == expected['source_sha256'] and
                type(actual['properties']) is list and len(actual['properties']) == len(props), 'Measured material identity/count mismatch.')
        values = []; names = set()
        for prop, value in zip(props, actual['properties']):
            require(type(prop) is dict and set(prop) == {'name', 'kind', 'flags', 'hex'}, 'Invalid requested material property.')
            name = text(prop['name']); kind = prop['kind']
            require(name not in names and kind in FLAGS and type(prop['flags']) is int and prop['flags'] in FLAGS[kind],
                    'Unqualified or duplicate material property.')
            names.add(name); size = 16 if kind in ('Color', 'Vector') else 4
            _hex(prop['hex'], size)
            require(type(value) is dict and set(value) == {'name', 'hex'} and value['name'] == name,
                    'Measured material property identity mismatch.')
            values.append((name, _hex(value['hex'], size)))
        results[key] = MaterialUpload(key, expected['source_sha256'], request['color_space'], tuple(values))
    return results


def pack_material_constants(layout, key, binding, upload, *, color_space, explicit_values):
    """Join source uniforms to measured uploads, preserving the original layout.

    Texture ST is the explicit source scale/offset. Other undeclared, per-draw or
    global fields must be provided by their owner. They cannot override measured
    source properties. Source edits invalidate the old upload fingerprint.
    """
    expected = material_request(key, binding)
    require(type(upload) is MaterialUpload and upload.key == key and upload.source_sha256 == expected['source_sha256'] and
            upload.color_space == color_space and color_space in ('Linear', 'Gamma'), 'Stale or mismatched material upload.')
    measured = dict(upload.values)
    require(len(measured) == len(upload.values) and set(measured) == {p['name'] for p in expected['properties']},
            'Measured material property set mismatch.')
    for prop in expected['properties']:
        data = measured[prop['name']]
        require(type(data) is bytes, 'Invalid measured material data.')
        _hex(data.hex(), 16 if prop['kind'] in ('Color', 'Vector') else 4)
    _validate(layout)
    require(type(explicit_values) is dict, 'Explicit non-material constant values required.')
    values = {}; props = binding['properties']
    for name, field in layout['fields'].items():
        raw = measured.get(name)
        base = name[:-3] if name.endswith('_ST') else None
        if raw is None and name not in props and base in props and props[base]['type'] == 'Texture':
            texture = props[base]['texture']
            st = tuple(texture['scale']) + tuple(texture['offset'])
            require(len(st) == 4, 'Invalid source texture scale/offset.')
            raw = struct.pack('<4f', *(number(v) for v in st))
        if raw is not None:
            require(name not in explicit_values, 'Explicit values cannot silently override measured source material data.')
            require(field['type'] == 0 and not field['matrix'] and not field['array_size'] and
                    field['columns'] * 4 <= len(raw), 'Material property and uniform shape disagree.')
            components = struct.unpack('<' + 'f' * field['columns'], raw[:field['columns'] * 4])
            values[name] = components[0] if field['columns'] == 1 else components
    require(set(explicit_values) == (set(layout['fields']) | set(layout.get('structures', {}))) - set(values),
            'Missing or extra explicitly owned shader constants.')
    values.update(explicit_values)
    return pack_constants(layout, values)
