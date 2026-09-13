# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Join source constant declarations and pack only explicitly supplied values.

Qualified arrays contain float4/int4 vectors or column-major float4x4 matrices.
Variant structure arrays retain their declared stride and member-relative offsets.
This private helper does not infer runtime globals, instance policy or defaults.
"""
import struct
from foa_scene_material import integer, number, require, text


MAX_FIELDS = 4096


def _field(value, size):
    require(type(value) is dict, 'Invalid constant field.')
    require(set(value) in ({'name', 'type', 'rows', 'columns', 'matrix', 'array_size', 'index'},
                          {'name', 'type', 'rows', 'columns', 'matrix', 'array_size', 'index', 'bytes'}),
            'Invalid constant field keys.')
    field = dict(value)
    text(field['name'])
    integer(field['type'], 0, 1)
    count = integer(field['array_size'], 0, MAX_FIELDS)
    require(type(field['matrix']) is bool, 'Invalid constant matrix flag.')
    columns = integer(field['columns'], 1, 4)
    rows = integer(field['rows'], 1, 4)
    if field['matrix']:
        require(field['type'] == 0 and rows == columns == 4,
                'Only explicit float4x4 column matrices are qualified.')
        width = 64
    else:
        require(rows == 1, 'Invalid vector row count.')
        require(not count or columns == 4, 'Only float4/int4 vector arrays are qualified.')
        width = columns * 4
    length = width * (count or 1)
    offset = integer(field['index'], 0, size - length)
    require(offset % (16 if field['matrix'] or count else 4) == 0, 'Constant field offset is not aligned.')
    require(field['matrix'] or count or offset // 16 == (offset + length - 1) // 16,
            'Constant vector crosses a register boundary.')
    if 'bytes' in field:
        require(type(field['bytes']) is int and field['bytes'] == length, 'Invalid packed field size.')
    field['bytes'] = length
    return field


def _join(fields, value):
    name = value['name']
    if name in fields:
        require(fields[name] == value, 'Conflicting common and variant constant field.')
    else:
        require(len(fields) < MAX_FIELDS, 'Constant field count exceeds its bound.')
        fields[name] = value


def _disjoint(items):
    end = 0
    for start, stop in sorted((v['index'], v['index'] + v['bytes']) for v in items):
        require(start >= end, 'Constant fields or structures overlap.')
        end = stop


def _validate(layout):
    require(type(layout) is dict and set(layout) in ({'size', 'fields'}, {'size', 'fields', 'structures'}),
            'Invalid constant layout.')
    size = integer(layout['size'], 16, 65536)
    require(size % 16 == 0, 'Constant-buffer size is not aligned.')
    fields, structures = layout['fields'], layout.get('structures', {})
    require(type(fields) is dict and type(structures) is dict and len(fields) + len(structures) <= MAX_FIELDS,
            'Invalid constant layout fields.')
    require(not set(fields).intersection(structures), 'Ambiguous constant/structure identity.')
    expanded = 0
    for name, field in fields.items():
        require(_field(field, size) == field and field['name'] == name, 'Invalid packed constant field.')
        expanded += field['array_size'] or 1
        require(expanded <= MAX_FIELDS, 'Expanded constant field count exceeds its bound.')
    for name, item in structures.items():
        require(type(item) is dict and set(item) == {'name', 'index', 'size', 'array_size', 'fields', 'bytes'},
                'Invalid constant structure keys.')
        require(text(item['name']) == name, 'Invalid constant structure identity.')
        stride = integer(item['size'], 16, 65536)
        count = integer(item['array_size'], 1, MAX_FIELDS)
        require(stride % 16 == 0, 'Constant structure stride is not aligned.')
        length = stride * count
        require(type(item['bytes']) is int and item['bytes'] == length, 'Invalid constant structure extent.')
        offset = integer(item['index'], 0, size - length)
        require(offset % 16 == 0, 'Constant structure offset is not aligned.')
        members = item['fields']
        require(type(members) is dict and 0 < len(members) <= MAX_FIELDS, 'Invalid constant structure members.')
        for label, field in members.items():
            require(_field(field, stride) == field and field['name'] == label, 'Invalid constant structure member.')
            expanded += count * (field['array_size'] or 1)
            require(expanded <= MAX_FIELDS, 'Expanded constant field count exceeds its bound.')
        _disjoint(members.values())
    require(expanded <= MAX_FIELDS, 'Expanded constant field count exceeds its bound.')
    _disjoint(list(fields.values()) + list(structures.values()))


def constant_buffers(tree, program, parameters):
    """Combine exact common/variant identities, preserving source byte offsets.

    Existing non-array layouts retain their shape. Structures are supported only
    in the inspected variant parameter format, with one flat member level and an
    explicit array length/stride. Other shapes and unbound globals are rejected.
    """
    shader_pass = tree['m_ParsedForm']['m_SubShaders'][program['subshader']]['m_Passes'][program['pass']]
    names = {index: name for name, index in shader_pass['m_NameIndices']}
    common = shader_pass[program['stage']]['m_CommonParameters']
    buffers = {}

    def add(name, size, fields, structures):
        text(name)
        integer(size, 16, 65536)
        require(size % 16 == 0, 'Constant-buffer size is not aligned.')
        require(type(fields) is list and type(structures) is list and len(fields) + len(structures) <= MAX_FIELDS,
                'Invalid constant declarations.')
        require(name in buffers or len(buffers) < 256, 'Constant buffer count exceeds its bound.')
        target = buffers.setdefault(name, {'size': size, 'fields': {}})
        require(target['size'] == size, 'Common and variant constant-buffer sizes disagree.')
        declared = 0
        for value in fields:
            field = _field(value, size)
            declared += field['array_size'] or 1
            require(declared <= MAX_FIELDS, 'Expanded constant declarations exceed their bound.')
            _join(target['fields'], field)
        for value in structures:
            require(type(value) is dict and set(value) == {'name', 'index', 'array_size', 'size', 'parameters'},
                    'Unsupported constant structure declaration.')
            stride = integer(value['size'], 16, 65536)
            count = integer(value['array_size'], 1, MAX_FIELDS)
            require(type(value['parameters']) is list and 0 < len(value['parameters']) <= MAX_FIELDS,
                    'Invalid constant structure members.')
            item = {k: value[k] for k in ('name', 'index', 'array_size', 'size')}
            text(item['name'])
            item.update(bytes=stride * count, fields={})
            for raw in value['parameters']:
                field = _field(raw, stride)
                declared += count * (field['array_size'] or 1)
                require(declared <= MAX_FIELDS, 'Expanded constant declarations exceed their bound.')
                _join(item['fields'], field)
            _join(target.setdefault('structures', {}), item)
        _validate(target)

    def named(index):
        require(index in names, 'Constant field has an absent name index.')
        return names[index]

    for item in common['m_ConstantBuffers']:
        require(not item['m_StructParams'], 'Common-format constant structures are not qualified.')
        fields = []
        for matrix, key in ((False, 'm_VectorParams'), (True, 'm_MatrixParams')):
            for field in item[key]:
                fields.append({'name': named(field['m_NameIndex']), 'type': field['m_Type'],
                               'rows': field['m_RowCount'] if matrix else 1,
                               'columns': 4 if matrix else field['m_Dim'], 'matrix': matrix,
                               'array_size': field['m_ArraySize'], 'index': field['m_Index']})
        add(named(item['m_NameIndex']), item['m_Size'], fields, [])
    for group in parameters['groups']:
        if not group['name']:
            require(not group['used_size'] and not group['parameters'] and not group['structures'],
                    'Unbound global constants are not qualified.')
            continue
        add(group['name'], group['used_size'], group['parameters'], group['structures'])
    return buffers


def pack_constants(layout, values):
    """Pack all declared fields, with no inferred defaults or global state.

    Matrices are 16 column-major floats. An array value is an exact-length list
    of element values; a structure array is a list of member dictionaries. Each
    member uses its source-relative offset within the declared structure stride.
    Only undeclared/padding bytes are zeroed. Old scalar/vector inputs are unchanged.
    """
    _validate(layout)
    fields, structures = layout['fields'], layout.get('structures', {})
    require(type(values) is dict and set(values) == set(fields) | set(structures), 'Missing or extra constant values.')
    result = bytearray(layout['size'])

    def write(field, value, base=0):
        count = 16 if field['matrix'] else field['columns']
        if field['array_size']:
            require(type(value) in (list, tuple) and len(value) == field['array_size'], 'Constant array value shape mismatch.')
            elements = value
        else:
            elements = (value,)
        for index, element in enumerate(elements):
            if count == 1:
                element = (element,)
            require(type(element) in (list, tuple) and len(element) == count, 'Constant value shape mismatch.')
            if field['type'] == 0:
                data = struct.pack('<' + 'f' * count, *(number(v) for v in element))
            else:
                data = struct.pack('<' + 'i' * count, *(integer(v, -(1 << 31), (1 << 31) - 1) for v in element))
            offset = base + field['index'] + index * count * 4
            result[offset:offset + count * 4] = data

    for name, field in fields.items():
        write(field, values[name])
    for name, item in structures.items():
        elements = values[name]
        require(type(elements) in (list, tuple) and len(elements) == item['array_size'], 'Constant structure array shape mismatch.')
        for index, element in enumerate(elements):
            require(type(element) is dict and set(element) == set(item['fields']), 'Missing or extra constant structure values.')
            for label, field in item['fields'].items():
                write(field, element[label], item['index'] + index * item['size'])
    return bytes(result)
