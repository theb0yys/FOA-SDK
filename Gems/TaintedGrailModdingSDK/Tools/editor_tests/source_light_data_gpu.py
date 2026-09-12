# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native word comparisons against independently captured HDRP LightData bytes."""
from foa_scene_light_data import STRIDE, MAX_LIGHTS, pack_light_data
from foa_scene_shader_buffers import buffer_declarations


def cases(compiler, records, reference, profile):
    """Compare all 56 words, preserving separate codec and original buffers.

    Four records per row keep the bounded 4,096-record fixture legible in a
    native viewport. Unused last-row cells are blue. Corruptions must produce
    red cells; unchanged words must be green. This proves transfer, not lighting.
    """
    if type(records) is not list or not 1 <= len(records) <= MAX_LIGHTS:
        raise ValueError('GPU fixture needs 1..4096 explicit LightData records.')
    packed = pack_light_data(records, profile)
    if type(reference) is not bytes or packed != reference:
        raise ValueError('Codec bytes do not match the independent HDRP reference.')
    count = len(records)
    columns, rows = 224, (count + 3) // 4
    vertex = compiler.compile(b'''
        struct V { float4 position : SV_Position; float2 uv : TEXCOORD3; };
        V main(float3 position : POSITION, float2 uv : TEXCOORD0) {
            V result; result.position = float4(position, 1); result.uv = uv; return result;
        }
    ''', b'main', b'vs_5_0')
    fragment = compiler.compile(('''
        struct Cell { uint words[56]; };
        StructuredBuffer<Cell> lights : register(t3);
        StructuredBuffer<Cell> original : register(t4);
        float4 main(float4 position : SV_Position, float2 uv : TEXCOORD3) : SV_Target0 {
            uint row = min((uint)(uv.y * %d), %d);
            uint column = min((uint)(uv.x * 224), 223);
            uint index = row * 224 + column;
            if (index >= %d) return float4(0, 0, 1, 1);
            return lights[index / 56].words[index %% 56] == original[index / 56].words[index %% 56]
                ? float4(0, 1, 0, 1) : float4(1, 0, 0, 1);
        }
    ''' % (rows, rows - 1, count * 56)).encode(), b'main', b'ps_5_0')
    declarations = [dict(slot=slot, type=2, stride=STRIDE) for slot in (3, 4)]
    if buffer_declarations(fragment, 'fragment') != declarations:
        raise ValueError('Compiler did not preserve both qualified structured layouts.')
    programs = [dict(stage='vertex', code=vertex, constant_buffers=[], images=[], samplers=[], buffers=[]),
                dict(stage='fragment', code=fragment, constant_buffers=[], images=[], samplers=[], buffers=declarations)]
    for name, changed_words in (
            ('original', set()),
            ('all_words_changed', set(range(count * 56))),
            ('one_word_per_light_changed', {light * 56 + light % 56 for light in range(count)})):
        data = bytearray(packed)
        for index in changed_words:
            data[index * 4] ^= 1
        stages = [dict(constants=[], images=[], samplers=[], buffers=[]),
                  dict(constants=[], images=[], samplers=[], buffers=[
                      dict(slot=3, type=2, stride=STRIDE, hex=data.hex()),
                      dict(slot=4, type=2, stride=STRIDE, hex=reference.hex())])]
        yield dict(name=name, programs=programs, stages=stages, rows=rows, columns=columns,
                   record_count=count, changed_words=sorted(changed_words))
