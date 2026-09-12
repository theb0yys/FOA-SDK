# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native GPU word comparisons against independently captured HDRP record bytes."""
from foa_scene_directional_lights import STRIDE, pack_directional_lights
from foa_scene_shader_buffers import buffer_declarations


def cases(compiler, records, reference, profile):
    """One cell per word/light; green is equal and red is different.

    The reference must come from original-assembly execution, independently of
    the codec. Deliberate corruptions cover every word and one distinct word per
    row. These comparisons prove buffer transfer, not a game's lighting result.
    """
    if type(records) is not list or not 1 <= len(records) <= 256:
        raise ValueError('GPU fixture needs 1..256 explicit directional records.')
    packed = pack_directional_lights(records, profile)
    if type(reference) is not bytes or packed != reference:
        raise ValueError('Codec bytes do not match the independent HDRP reference.')
    count = len(records)
    vertex = compiler.compile(b'''
        struct V { float4 position : SV_Position; float2 uv : TEXCOORD3; };
        V main(float3 position : POSITION, float2 uv : TEXCOORD0) {
            V result; result.position = float4(position, 1); result.uv = uv; return result;
        }
    ''', b'main', b'vs_5_0')
    fragment = compiler.compile(('''
        cbuffer Reference : register(b0) { uint4 expected[%d]; };
        struct Cell { uint words[44]; };
        StructuredBuffer<Cell> lights : register(t3);
        float4 main(float4 position : SV_Position, float2 uv : TEXCOORD3) : SV_Target0 {
            uint row = min((uint)(uv.y * %d), %d);
            uint word = min((uint)(uv.x * 44), 43);
            uint index = row * 44 + word;
            return lights[row].words[word] == expected[index / 4][index %% 4]
                ? float4(0, 1, 0, 1) : float4(1, 0, 0, 1);
        }
    ''' % (count * 11, count, count - 1)).encode(), b'main', b'ps_5_0')
    declaration = [dict(slot=3, type=2, stride=STRIDE)]
    if buffer_declarations(fragment, 'fragment') != declaration:
        raise ValueError('Compiler did not preserve the qualified structured layout.')
    programs = [dict(stage='vertex', code=vertex, constant_buffers=[], images=[], samplers=[], buffers=[]),
                dict(stage='fragment', code=fragment, constant_buffers=[dict(slot=0, size=len(reference))],
                     images=[], samplers=[], buffers=declaration)]
    for name, changed_words in (
            ('original', set()),
            ('all_words_changed', set(range(count * 44))),
            ('one_word_per_light_changed', {row * 44 + row % 44 for row in range(count)})):
        data = bytearray(packed)
        for index in changed_words:
            data[index * 4] ^= 1
        stages = [dict(constants=[], images=[], samplers=[], buffers=[]),
                  dict(constants=[dict(slot=0, hex=reference.hex())], images=[], samplers=[],
                       buffers=[dict(slot=3, type=2, stride=STRIDE, hex=data.hex())])]
        yield dict(name=name, programs=programs, stages=stages, rows=count, columns=44,
                   changed_words=sorted(changed_words))
