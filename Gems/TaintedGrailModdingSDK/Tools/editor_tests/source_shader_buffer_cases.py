# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Explicit synthetic SRV fixtures and negative controls for native GPU acceptance."""
import copy
import struct
from foa_scene_shader_buffers import buffer_declarations


def cases(compiler, strides):
    """Exercise shared t-register numbers in both stages, element indexing and stride.

    These synthetic values are not inferred game instance or lighting data.
    Each structured record has sentinel padding and one selected trailing scalar.
    """
    vertex = compiler.compile(b'''
        cbuffer Transform : register(b0) { float4x4 transform; };
        ByteAddressBuffer offsets : register(t7);
        StructuredBuffer<float> adjustments : register(t3);
        struct V { float4 position : SV_Position; float2 uv : TEXCOORD3; };
        V main(float3 position : POSITION, float2 uv : TEXCOORD0) {
            V result;
            result.position = mul(transform, float4(position, 1));
            result.position.xyz += asfloat(offsets.Load3(0)) +
                float3(adjustments[0], adjustments[1], adjustments[2]);
            result.uv = uv; return result;
        }
    ''', b'main', b'vs_5_0')
    identity = struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1).hex()
    colours = ((1,0,0,1),(0,1,0,1),(0,0,1,1),(1,1,0,1))
    result = []
    for stride in strides:
        if type(stride) is not int or stride < 4 or stride > 2048 or stride%4:
            raise ValueError('Expected an aligned qualified structured-buffer stride.')
        width = stride//4
        fragment = compiler.compile(('''
            cbuffer Material : register(b0) { float4 tint; };
            ByteAddressBuffer raw : register(t7);
            struct Cell { float value[%d]; };
            StructuredBuffer<Cell> cells : register(t3);
            float4 main(float4 position : SV_Position, float2 uv : TEXCOORD3) : SV_Target0 {
                uint index = min((uint)(uv.x*2),1) + 2*min((uint)(uv.y*2),1);
                float4 values = float4(cells[index*4].value[%d], cells[index*4+1].value[%d],
                    cells[index*4+2].value[%d], cells[index*4+3].value[%d]);
                return (asfloat(raw.Load4(index*16)) + values)*tint;
            }
        ''' % (width, width-1, width-1, width-1, width-1)).encode(), b'main', b'ps_5_0')
        programs = []
        for stage,code,element_stride in (('vertex',vertex,4),('fragment',fragment,stride)):
            declared = buffer_declarations(code,stage)
            assert declared == [dict(slot=3,type=2,stride=element_stride),dict(slot=7,type=4,stride=4)]
            programs.append(dict(stage=stage,code=code,constant_buffers=[dict(slot=0,size=64 if stage=='vertex' else 16)],
                                 images=[],samplers=[],buffers=declared))
        raw = struct.pack('<16f',*(component*.5 for colour in colours for component in colour)).hex()
        structured = b''.join(struct.pack('<'+'f'*width,*([-100.]*(width-1)+[component*.5]))
                              for colour in colours for component in colour).hex()
        stages = [dict(constants=[dict(slot=0,hex=identity)],images=[],samplers=[],buffers=[
                    dict(slot=3,type=2,stride=4,hex='00'*16),dict(slot=7,type=4,stride=4,hex='00'*16)]),
                  dict(constants=[dict(slot=0,hex=struct.pack('<4f',1,1,1,1).hex())],images=[],samplers=[],buffers=[
                    dict(slot=3,type=2,stride=stride,hex=structured),dict(slot=7,type=4,stride=4,hex=raw)])]
        result.append(dict(name='buffer_stride_'+str(stride),programs=programs,stages=stages))
    return result


def invalid_draws(baseline):
    """The parser rejects malformed values; native layouts reject valid mismatches."""
    mutations = (
        ('missing-buffer-list',lambda d:d['stages'][0].pop('buffers'),'parse'),
        ('duplicate-data-register',lambda d:d['stages'][1]['buffers'].append(copy.deepcopy(d['stages'][1]['buffers'][0])),'parse'),
        ('texture-buffer-collision',lambda d:d['stages'][1]['images'].append(dict(slot=3,asset='assets/collision.streamingimage')),'parse'),
        ('unsupported-buffer-type',lambda d:d['stages'][1]['buffers'][0].update(type=3),'parse'),
        ('raw-stride-not-four',lambda d:d['stages'][1]['buffers'][1].update(stride=8),'parse'),
        ('unaligned-structured-stride',lambda d:d['stages'][1]['buffers'][0].update(stride=6),'parse'),
        ('boolean-buffer-stride',lambda d:d['stages'][1]['buffers'][0].update(stride=True),'parse'),
        ('empty-buffer-payload',lambda d:d['stages'][1]['buffers'][0].update(hex=''),'parse'),
        ('partial-buffer-element',lambda d:d['stages'][1]['buffers'][0].update(hex='000000'),'parse'),
        ('outside-buffer-register',lambda d:d['stages'][1]['buffers'][0].update(slot=128),'parse'),
        ('unexpected-buffer-field',lambda d:d['stages'][1]['buffers'][0].update(guess=1),'parse'),
        ('missing-data-buffer',lambda d:d['stages'][1]['buffers'].pop(),'native'),
        ('wrong-data-buffer-type',lambda d:d['stages'][1]['buffers'][1].update(type=2),'native'),
        ('wrong-data-buffer-stride',lambda d:d['stages'][1]['buffers'][0].update(stride=8),'native'),
        ('wrong-data-buffer-register',lambda d:d['stages'][1]['buffers'][0].update(slot=4),'native'),
    )
    for name,mutate,phase in mutations:
        changed=copy.deepcopy(baseline);mutate(changed);yield name,changed,phase


def wrong_inputs(baseline):
    changed=copy.deepcopy(baseline)
    changed['stages'][1]['buffers'][1]['hex']='00'*64
    yield 'wrong_raw_contents',changed
    changed=copy.deepcopy(baseline)
    changed['stages'][1]['buffers'][0]['hex']='00'*(len(changed['stages'][1]['buffers'][0]['hex'])//2)
    yield 'wrong_structured_contents',changed
    changed=copy.deepcopy(baseline)
    changed['stages'][0]['buffers'][1]['hex']=struct.pack('<4f',8,0,0,0).hex()
    yield 'wrong_vertex_buffer',changed
