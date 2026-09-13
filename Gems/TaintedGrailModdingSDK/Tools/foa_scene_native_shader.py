# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Private, versioned source-program handoff to the SDK's native shader builder.

No pass, material value, sampler or render-state policy is inferred. The caller
must bind those to source evidence. The packet carries exact SM5.0 VS/PS bytecode,
constant-buffer sizes, texture types and sampler slots, with separate stage SRGs.
It is not a canonical scene schema or a game-export format.
"""
import hashlib
import struct

from foa_scene_material import integer, require

MAX_PACKET = 64 * 1024 * 1024
HEADER_SIZE = 56
STATE_FIELDS = ('cull', 'depth_enable', 'depth_write', 'depth_func', 'blend_enable',
                'blend_source', 'blend_dest', 'blend_op', 'alpha_source', 'alpha_dest',
                'alpha_op', 'write_mask')
STATE_MAX = (2, 1, 1, 7, 1, 16, 16, 4, 16, 16, 4, 15)


def uints(*values):
    return struct.pack('<'+'I'*len(values), *values)


def program_contract(code, stage):
    require(type(code) is bytes and 32 <= len(code) <= 32*1024*1024, 'Invalid native shader program size.')
    require(stage in ('vertex', 'fragment'), 'Unsupported native shader stage.')
    require(code[:4] == b'DXBC' and struct.unpack_from('<II', code, 20) == (1, len(code)), 'Invalid native DXBC container.')
    count = integer(struct.unpack_from('<I', code, 28)[0], 1, 64)
    end = 32+4*count
    require(end <= len(code), 'Truncated native DXBC table.')
    ranges, instructions, signatures = [(0, end)], [], []
    for i in range(count):
        start = struct.unpack_from('<I', code, 32+i*4)[0]
        require(start >= end and start % 4 == 0 and start+8 <= len(code), 'Invalid native DXBC chunk.')
        size = struct.unpack_from('<I', code, start+4)[0]
        require(start+8+size <= len(code), 'Native DXBC chunk exceeds its container.')
        ranges.append((start, start+8+size))
        tag, data = code[start:start+4], code[start+8:start+8+size]
        if tag in (b'SHDR', b'SHEX'): instructions.append(data)
        if tag in ((b'ISGN', b'ISG1') if stage == 'vertex' else (b'OSGN', b'OSG1')):
            signatures.append((tag, data))
    cursor = 0
    for start, end in sorted(ranges):
        require(start == cursor, 'Native DXBC has overlapping or unclaimed bytes.')
        cursor = end
    require(cursor == len(code) and len(instructions) == len(signatures) == 1, 'Incomplete native DXBC stages/signatures.')
    words = instructions[0]
    require(len(words) >= 8 and len(words) % 4 == 0, 'Invalid native shader instructions.')
    require(struct.unpack_from('<II', words) == ((1 << 16 if stage == 'vertex' else 0) | 0x50, len(words)//4),
            'Native source shader requires the qualified SM5.0 stage.')
    tag, data = signatures[0]
    require(len(data) >= 8, 'Truncated native shader signature.')
    count = integer(struct.unpack_from('<I', data)[0], 0, 32)
    extended = tag.endswith(b'1'); stride = 32 if extended else 24
    require(8+count*stride <= len(data), 'Truncated native signature records.')
    seen, channels, outputs = set(), [], {}
    for i in range(count):
        row = 8+i*stride
        if extended:
            require(struct.unpack_from('<I', data, row)[0] == 0, 'Multiple signature streams are unsupported.')
            row += 4
        offset, index, system, component_type, register = struct.unpack_from('<5I', data, row)
        mask = data[row+20]
        require(8+count*stride <= offset < len(data) and 0 < mask <= 15 and index <= 31, 'Invalid native signature field.')
        end = data.find(b'\0', offset, min(len(data), offset+65))
        require(offset < end <= offset+64, 'Invalid native signature name.')
        try: name = data[offset:end].decode('ascii')
        except UnicodeError: require(False, 'Invalid native signature encoding.')
        key = name, index
        require(key not in seen, 'Duplicate native shader signature.')
        seen.add(key)
        if stage == 'vertex' and system == 0:
            channels.append({'semantic': name, 'index': index, 'components': mask.bit_length(),
                             'component_type': component_type, 'register': register})
        if stage == 'fragment' and name.upper() == 'SV_TARGET':
            integer(index, 0, 7); outputs[index] = mask.bit_length()
    require(not outputs or set(outputs) == set(range(max(outputs)+1)), 'Sparse native color outputs are unsupported.')
    return {'stage': stage, 'input_channels': channels, 'color_outputs': [outputs[i] for i in sorted(outputs)],
            'code_sha256': hashlib.sha256(code).hexdigest()}


def encode(stages, *, state, draw_list, profile='unity-6000.0.64f1-sm50-dx12'):
    require(profile == 'unity-6000.0.64f1-sm50-dx12', 'Unsupported native source shader profile.')
    require(type(draw_list) is str and 0 < len(draw_list) <= 64
            and all(c.isascii() and (c.isalnum() or c in '_-') for c in draw_list), 'Invalid native shader draw list.')
    require(type(state) is dict and set(state) == set(STATE_FIELDS), 'Explicit native render state is required.')
    require(type(stages) in (tuple, list) and len(stages) == 2, 'Expected one vertex and one fragment program.')
    version = 2 if any(type(s) is dict and 'buffers' in s for s in stages) else 1
    encoded_name = draw_list.encode('ascii')
    parts = [uints(len(encoded_name)), encoded_name,
             uints(*(integer(state[k], 0, maximum) for k, maximum in zip(STATE_FIELDS, STATE_MAX)))]
    for stage, expected in zip(stages, ('vertex', 'fragment')):
        require(type(stage) is dict and set(stage) in ({'stage', 'code', 'constant_buffers', 'images', 'samplers'},
                                            {'stage', 'code', 'constant_buffers', 'images', 'samplers', 'buffers'})
                and stage['stage'] == expected, 'Unexpected native shader stage schema.')
        contract = program_contract(stage['code'], expected)
        require(not (len(contract['color_outputs']) > 1 and state['blend_enable']), 'MRT blending needs a separate qualified mapping.')
        parts += [uints(len(stage['code'])), stage['code']]
        for field, maximum in (('constant_buffers', 14), ('images', 128), ('samplers', 16)):
            values = stage[field]
            require(type(values) in (list, tuple) and len(values) <= maximum, 'Native shader bindings exceed their bound.')
            parts.append(uints(len(values))); seen = set()
            for row in values:
                require(type(row) is dict and set(row) == ({'slot', 'size'} if field == 'constant_buffers'
                        else {'slot', 'type'} if field == 'images' else {'slot'}), 'Unsupported native resource schema.')
                slot = integer(row['slot'], 0, maximum-1)
                require(slot not in seen, 'Duplicate native resource register.')
                seen.add(slot); parts.append(uints(slot))
                if field == 'constant_buffers':
                    size = integer(row['size'], 16, 65536)
                    require(size % 16 == 0, 'Constant buffer needs aligned source extent.')
                    parts.append(uints(size))
                elif field == 'images': parts.append(uints(integer(row['type'], 1, 9)))
        from foa_scene_shader_buffers import buffer_declarations
        buffers = stage.get('buffers', [])
        require(type(buffers) in (list, tuple) and len(buffers) <= 128, 'Native buffers exceed their bound.')
        checked = []
        occupied = {r['slot'] for r in stage['images']}
        for row in buffers:
            require(type(row) is dict and set(row) == {'slot', 'type', 'stride'}, 'Invalid native buffer schema.')
            slot = integer(row['slot'], 0, 127)
            require(slot not in occupied, 'Duplicate native SRV register.')
            occupied.add(slot)
            kind, stride = integer(row['type'], 2, 4), integer(row['stride'], 4, 2048)
            require(kind in (2, 4) and stride % 4 == 0 and (kind != 4 or stride == 4), 'Unsupported native buffer type/stride.')
            checked.append(dict(slot=slot, type=kind, stride=stride))
        require(sorted(checked, key=lambda r:r['slot']) == buffer_declarations(stage['code'], expected),
                'Native buffer bindings disagree with compiled shader declarations.')
        if version == 2:
            parts.append(uints(len(checked)))
            for row in checked: parts.append(uints(row['slot'], row['type'], row['stride']))
    require(sum(map(len, parts))+HEADER_SIZE <= MAX_PACKET, 'Native shader packet exceeds its bound.')
    body = b''.join(parts)
    require(len(body)+HEADER_SIZE <= MAX_PACKET, 'Native shader packet exceeds its bound.')
    header = b'FOASHD01'+uints(version, 1, len(body), 0)
    return header+hashlib.sha256(header+body).digest()+body


def source_stage(tree, blob, selector):
    """Join an exact source program to supported native resource declarations.

    Source buffer/image/inline-sampler values remain separate runtime bindings.
    Raw/structured SRV types and strides come from the compiled SM5 declarations.
    UAV inputs still reject until their native resource route is implemented.
    """
    from foa_scene_shader import unpack_entries, player_programs, parameter_record, variant_resources, dxbc_container
    from foa_scene_shader_constants import constant_buffers
    require(type(selector) is dict and set(selector) == {'subshader', 'pass', 'stage', 'tier', 'variant'}, 'Exact source selector required.')
    require(selector['stage'] in ('progVertex', 'progFragment'), 'Unsupported source shader stage selector.')
    for key in ('subshader', 'pass', 'tier', 'variant'): integer(selector[key], 0, 65535)
    entries = unpack_entries(tree, blob)
    matches = [p for p in player_programs(tree, entries) if all(p.get(k) == v for k, v in selector.items())]
    require(len(matches) == 1, 'Source native program is absent or ambiguous.')
    program = matches[0]; parameters = parameter_record(entries[program['parameter_entry']])
    layouts = constant_buffers(tree, program, parameters)
    code, info = dxbc_container(entries[program['code_entry']])
    resources = variant_resources(tree, program, parameters)
    from foa_scene_shader_buffers import buffer_declarations
    buffers = {r['slot']: r for r in buffer_declarations(code, info['stage'])}
    consumed = set()
    result = {'stage': info['stage'], 'code': code, 'constant_buffers': [], 'images': [], 'samplers': []}
    for resource in resources:
        namespace, slot, signature = resource['namespace'], resource['index'], resource['signature']
        if namespace == 'cb':
            require(signature[1] in layouts, 'Source buffer lacks an exact layout.')
            layout = layouts[signature[1]]
            result['constant_buffers'].append({'slot': slot, 'size': layout['size']})
        elif namespace == 't' and signature[0] == 0:
            dimension, multisampled = signature[2:4]
            kinds = {(2, False): 3, (2, True): 5, (3, False): 7, (4, False): 8, (5, False): 4, (5, True): 6, (6, False): 9}
            require((dimension, multisampled) in kinds, 'Unsupported source image dimension.')
            result['images'].append({'slot': slot, 'type': kinds[dimension, multisampled]})
        elif namespace == 't' and signature[0] == 2:
            require(slot in buffers, 'Source buffer lacks a compiled declaration.')
            result.setdefault('buffers', []).append(buffers[slot]); consumed.add(slot)
        elif namespace == 's': result['samplers'].append({'slot': slot})
        else: require(False, 'Native source shader buffer/UAV route is not implemented.')
    require(consumed == set(buffers), 'Compiled buffer has no serialized source binding.')
    return result, {'program': program, 'resources': resources, 'layouts': layouts, 'code_sha256': hashlib.sha256(code).hexdigest()}
