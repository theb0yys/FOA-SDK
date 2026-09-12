# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Hardware constant-array/structure proof with synthetic, explicitly valued data."""
import re
import json
from pathlib import Path
import struct
import sys
sys.path[:0] = [str(Path(__file__).resolve().parent), str(Path(__file__).resolve().parents[1]),
                str(Path(__file__).resolve().parents[1] / 'tests')]
from foa_heightmap_importer import HeightmapImportError
from foa_scene_shader_constants import constant_buffers, pack_constants
from source_shader_native_cases import Compiler
from source_shader_gpu_cases import execute, packet, sha
from test_foa_scene_shader_constant_arrays import array_fixture, explicit_values


def verify(executable, root):
    compiler = Compiler()
    vertex = compiler.compile(
        b'void main(float3 p:POSITION,float2 u:TEXCOORD0,out float4 q:SV_Position,out float2 v:TEXCOORD3)'
        b'{q=float4(p,1);v=u;}', b'main', b'vs_5_0')
    source = b'''
struct Item { float2 pair; float3 triple; float factor;
              column_major float4x4 transform; float4 colours[2]; };
cbuffer Data : register(b3) {
    float4 lead; float4 vectors[2]; int4 integers[2];
    column_major float4x4 matrices[2]; float4 reserved; Item items[2];
};
float4 main(float4 p:SV_Position,float2 uv:TEXCOORD3):SV_Target {
    uint i = uv.x > 0.5 ? 1 : 0;
    return lead + vectors[i] + float4(integers[i]) * 0.25
      + float4(matrices[i]._m03, matrices[i]._m12, matrices[i]._m21, matrices[i]._m30)
      + float4(items[i].pair, items[i].triple.z, items[i].factor)
      + float4(items[i].transform._m21, items[i].transform._m32,
               items[i].transform._m00, items[i].transform._m13)
      + items[i].colours[0] + items[i].colours[1];
}
'''
    fragment = compiler.compile(source, b'main', b'ps_5_0')
    assembly, stage = compiler.disassemble(fragment)
    assert stage == 'ps_5_0'
    for declaration, offset, size in (
        ('float4 lead;', 0, 16), ('float4 vectors[2];', 16, 32),
        ('int4 integers[2];', 48, 32), ('float4x4 matrices[2];', 80, 128),
        ('} items[2];', 224, 256)):
        assert re.search(re.escape(declaration) + r'\s*// Offset:\s*' + str(offset)
                         + r' Size:\s*' + str(size) + r'\b', assembly), declaration
    for declaration, offset in (('float2 pair;',224), ('float3 triple;',240),
                                ('float factor;',252), ('float4x4 transform;',256),
                                ('float4 colours[2];',320)):
        assert re.search(re.escape(declaration) + r'\s*// Offset:\s*' + str(offset) + r'\b', assembly), declaration
    (root/'synthetic.hlsl').write_bytes(source)
    (root/'synthetic.asm.txt').write_text(assembly)
    layout = constant_buffers(*array_fixture())['Data']
    values = explicit_values()
    packed = pack_constants(layout, values)
    # This independent buffer uses fixed compiler-register offsets, not producer
    # layout metadata. Full pixel equality therefore cannot bless a wrong stride.
    reference = bytearray(480)
    for offset, fmt, data in (
        (0, '4f', values['lead']), (16, '8f', sum(values['vectors'], [])),
        (48, '8i', sum(values['integers'], [])), (80, '32f', sum(values['matrices'], []))):
        struct.pack_into('<'+fmt, reference, offset, *data)
    for offset, item in zip((224,352), values['items']):
        struct.pack_into('<2f', reference, offset, *item['pair'])
        struct.pack_into('<4f', reference, offset+16, *item['triple'], item['factor'])
        struct.pack_into('<16f', reference, offset+32, *item['transform'])
        struct.pack_into('<8f', reference, offset+96, *sum(item['colours'], []))
    assert packed == reference
    vertices = [(-1,-1,.5,0,1),(-1,1,.5,0,0),(1,1,.5,1,0),
                (-1,-1,.5,0,1),(1,1,.5,1,0),(1,-1,.5,1,1)]
    # Independent expected arithmetic of the HLSL expression above.
    expected = [(43.75,51.5,43.25,60), (-2,-13.75,-5.5,-26.25)]
    points = [(x+dx,32+dy,colour) for x,colour in zip((32,96),expected)
              for dx in (-2,0,2) for dy in (-2,0,2)]
    def draw(label, data, oracle=points):
        return execute(executable, packet([(1,vertex),(2,fragment)], vertices=vertices,
                       constants=[(2,3,bytes(data))]), root, label, expected=oracle)
    rows = [draw('packed', packed), draw('independent-registers', reference)]
    assert (root/'packed.foapixels').read_bytes() == (root/'independent-registers.foapixels').read_bytes()
    wrong = bytearray(packed)
    wrong[352:480] = packed[224:352]
    try:
        draw('wrong-structure-stride', wrong)
    except HeightmapImportError as error:
        assert str(error).startswith('GPU pixel differs at')
        rows.append(dict(case='wrong-structure-stride', status='PASSED', expected_rejection=True, reason=str(error)))
    else:
        raise AssertionError('Wrong structure stride passed the pixel oracle.')
    wrong = bytearray(packed)
    for offset in (80,144,256,384):
        m = struct.unpack_from('<16f', wrong, offset)
        struct.pack_into('<16f', wrong, offset, *(m[r*4+c] for c in range(4) for r in range(4)))
    try:
        draw('wrong-matrix-orientation', wrong)
    except HeightmapImportError as error:
        assert str(error).startswith('GPU pixel differs at')
        rows.append(dict(case='wrong-matrix-orientation', status='PASSED', expected_rejection=True, reason=str(error)))
    else:
        raise AssertionError('Wrong matrix orientation passed the pixel oracle.')
    report = dict(status='PASSED', cases=rows, layout=layout, values=values,
                  compiler_sha256=sha(compiler.path.read_bytes()), packed_sha256=sha(packed),
                  scope='Synthetic hardware DXBC packing proof; game globals and rendering policy are not inferred.')
    (root/'acceptance.json').write_text(json.dumps(report, indent=2))
    return report


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output-root', required=True, type=Path)
    args = parser.parse_args()
    args.output_root.mkdir(parents=True, exist_ok=False)
    print(json.dumps({'status': verify(args.executable, args.output_root)['status']}))
