# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Read exact SM5 raw/structured SRV declarations without inferring their contents.

Token fields follow Microsoft's published DirectX tokenized program format.
This bounded declaration reader does not reinterpret buffer element data or
qualify runtime instances, light lists or shader execution.
"""
import struct
from foa_scene_material import integer, require


def buffer_declarations(code, stage):
    from foa_scene_native_shader import program_contract
    program_contract(code, stage)
    declarations = {}
    count = struct.unpack_from('<I', code, 28)[0]
    for number in range(count):
        offset = struct.unpack_from('<I', code, 32+4*number)[0]
        if code[offset:offset+4] not in (b'SHEX', b'SHDR'):
            continue
        size = struct.unpack_from('<I', code, offset+4)[0]
        start, end = offset+16, offset+8+size
        while start < end:
            token = struct.unpack_from('<I', code, start)[0]
            opcode = token & 0x7ff
            if opcode == 53:  # Custom data uses its following length word.
                require(start+8 <= end, 'Truncated shader custom-data header.')
                length = integer(struct.unpack_from('<I', code, start+4)[0], 2, (end-start)//4)
            else:
                length = integer((token >> 24) & 0x7f, 1, (end-start)//4)
            if opcode in (161, 162):
                # SM5.0 declaration, direct one-dimensional immediate t-register,
                # no extended token, component selector or unqualified flags.
                require(token == ((3 if opcode == 161 else 4) << 24) | opcode,
                        'Unsupported native buffer declaration token.')
                operand, slot = struct.unpack_from('<II', code, start+4)
                require(operand == 0x107000, 'Unsupported native buffer declaration operand.')
                integer(slot, 0, 127)
                require(slot not in declarations, 'Duplicate native buffer declaration.')
                stride = 4 if opcode == 161 else struct.unpack_from('<I', code, start+12)[0]
                integer(stride, 4, 2048)
                require(stride % 4 == 0, 'Unaligned native structured-buffer stride.')
                declarations[slot] = {'slot': slot, 'type': 4 if opcode == 161 else 2, 'stride': stride}
            start += length*4
        require(start == end, 'Native shader instructions exceed their extent.')
    return [declarations[k] for k in sorted(declarations)]
