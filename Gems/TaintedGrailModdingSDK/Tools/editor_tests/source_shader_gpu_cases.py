# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Bounded, headless native shader acceptance with explicit private fixture inputs.

The native consumer uses float textures/targets, triangle lists, POSITION/UV0,
no blend/depth/culling, and a caller-selected point/linear single-mip sampler.
Those are fixture conditions, never inferred game renderer settings.
"""
from pathlib import Path
import sys
sys.path[:0] = [str(Path(__file__).resolve().parent), str(Path(__file__).resolve().parents[1])]
import hashlib
import json
import math
import struct
import subprocess
import time

from foa_scene_material import integer, number, require
from foa_scene_shader import unpack_entries, player_programs, parameter_record, variant_resources, dxbc_container
from foa_scene_shader_constants import constant_buffers, pack_constants


def sha(data):
    return hashlib.sha256(data).hexdigest()


def uints(*values):
    return struct.pack('<' + 'I' * len(values), *values)


def packet(shaders, *, vertices=None, constants=(), textures=(), samplers=(), width=128, height=64):
    require(0 < len(shaders) <= 8192, 'Shader count exceeds native fixture bound.')
    parts = [b'FOAGPU01', uints(int(vertices is None), len(shaders))]
    for stage, code in shaders:
        integer(stage, 1, 2)
        require(type(code) is bytes and 32 <= len(code) <= 32 * 1024**2, 'Invalid fixture bytecode size.')
        parts.extend((uints(stage, len(code)), code))
    if vertices is not None:
        integer(width, 1, 1024)
        integer(height, 1, 1024)
        require(3 <= len(vertices) <= 65535 and len(vertices) % 3 == 0, 'Invalid fixture triangles.')
        require(len(constants) <= 28 and len(textures) <= 256 and len(samplers) <= 32, 'Too many fixture resources.')
        parts.append(uints(width, height, len(vertices)))
        for vertex in vertices:
            require(len(vertex) == 5, 'Fixture vertex needs position and UV0.')
            parts.append(struct.pack('<5f', *(number(v) for v in vertex)))
        parts.append(uints(len(constants)))
        for stage, slot, data in constants:
            integer(stage, 1, 2); integer(slot, 0, 13)
            require(type(data) is bytes and 16 <= len(data) <= 65536 and len(data) % 16 == 0, 'Invalid fixture constant buffer.')
            parts.extend((uints(stage, slot, len(data)), data))
        parts.append(uints(len(textures)))
        for stage, slot, texture in textures:
            integer(stage, 1, 2); integer(slot, 0, 127)
            w, h = texture['width'], texture['height']
            integer(w, 1, 1024); integer(h, 1, 1024)
            require(len(texture['pixels']) == w*h, 'Fixture texture size mismatch.')
            parts.append(uints(stage, slot, w, h))
            for pixel in texture['pixels']:
                require(len(pixel) == 4, 'Fixture texture requires float4 pixels.')
                parts.append(struct.pack('<4f', *(number(v) for v in pixel)))
        parts.append(uints(len(samplers)))
        for stage, slot, state in samplers:
            integer(stage, 1, 2); integer(slot, 0, 15)
            require(set(state) == {'filter', 'u', 'v'}, 'Unqualified fixture sampler state.')
            parts.append(uints(stage, slot, integer(state['filter'], 0, 1),
                               integer(state['u'], 1, 3), integer(state['v'], 1, 3)))
    require(sum(map(len, parts)) <= 256 * 1024**2, 'Fixture packet exceeds its bound.')
    return b''.join(parts)


def source_draw(tree, blob, stages, vertices, width=128, height=64):
    """Select exact stage tuples; require explicit input for every named register.

    Source pass selection is a caller assertion, not a guessed keyword policy.
    No game IDs, shader code, materials or captured data are stored in this module.
    """
    entries = unpack_entries(tree, blob)
    programs = player_programs(tree, entries)
    shaders, constants, textures, samplers, evidence = [], [], [], [], []
    require(len(stages) == 2, 'Expected exactly two source fixture stages.')
    for stage_number, stage in enumerate(stages, 1):
        selector = stage['selector']
        require(set(selector) == {'subshader', 'pass', 'stage', 'tier', 'variant'}, 'Incomplete source program selector.')
        require(selector['stage'] == ('progVertex' if stage_number == 1 else 'progFragment'), 'Source stage order mismatch.')
        for key in ('subshader', 'pass', 'tier', 'variant'): integer(selector[key], 0, 65535)
        matches = [p for p in programs if all(p[k] == v for k, v in selector.items())]
        require(len(matches) == 1, 'Source program selector is absent or ambiguous.')
        program = matches[0]
        require(tuple(stage['keyword_indices']) == program['keyword_indices'], 'Source program keywords disagree.')
        code, meta = dxbc_container(entries[program['code_entry']])
        shaders.append((stage_number, code))
        params = parameter_record(entries[program['parameter_entry']])
        layouts = constant_buffers(tree, program, params)
        resources = variant_resources(tree, program, params)
        consumed = {'constants': set(), 'textures': set(), 'samplers': set()}
        for resource in resources:
            namespace, slot, signature = resource['namespace'], resource['index'], resource['signature']
            name = signature[1]
            if namespace == 'cb':
                require(name in layouts and name in stage['constants'], 'Missing source buffer values.')
                constants.append((stage_number, slot, pack_constants(layouts[name], stage['constants'][name])))
                consumed['constants'].add(name)
            elif namespace == 't' and signature[0] == 0:
                require(signature[2:4] == (2, False), 'Fixture only supports non-MS Texture2D.')
                require(name in stage['textures'], 'Missing source texture input.')
                textures.append((stage_number, slot, stage['textures'][name]))
                consumed['textures'].add(name)
            elif namespace == 's' and signature[0] == 'texture-sampler':
                require(name in stage['samplers'], 'Missing source sampler input.')
                samplers.append((stage_number, slot, stage['samplers'][name]))
                consumed['samplers'].add(name)
            else:
                require(False, 'Source resource is outside this native fixture capability.')
        for kind, used in consumed.items():
            require(used == set(stage[kind]), 'Unused or missing source fixture input.')
        evidence.append({'program': program, 'dxbc_sha256': sha(code), 'dxbc_bytes': len(code),
                         'layouts': layouts, 'resources': resources, 'container': meta})
    return packet(shaders, vertices=vertices, constants=constants, textures=textures,
                  samplers=samplers, width=width, height=height), evidence


def execute(executable, data, root, label, *, expected=None, rejected=False, timeout=120):
    require(label and all(c.isalnum() or c in '_-' for c in label), 'Invalid fixture label.')
    path, pixels = root/(label+'.foagpu'), root/(label+'.foapixels')
    with path.open('xb') as stream: stream.write(data)
    command = [str(executable), str(path)]
    before = sha(executable.read_bytes())
    start = time.monotonic()
    result = subprocess.run(command, capture_output=True, timeout=timeout, creationflags=subprocess.CREATE_NO_WINDOW)
    elapsed = time.monotonic()-start
    require(sha(path.read_bytes()) == sha(data) and sha(executable.read_bytes()) == before, 'Native fixture input changed.')
    (root/(label+'.stdout')).write_bytes(result.stdout)
    (root/(label+'.stderr')).write_bytes(result.stderr)
    row = {'case': label, 'exit_code': result.returncode, 'seconds': elapsed,
           'packet_sha256': sha(data), 'executable_sha256': before}
    if rejected:
        require(result.returncode != 0 and not pixels.exists(), 'Invalid native fixture was accepted.')
        row.update(status='PASSED', expected_rejection=True)
        return row
    require(result.returncode == 0, 'Native shader fixture failed: ' + result.stderr.decode(errors='replace'))
    require(len(result.stdout) <= 1024*1024*16 + 65536, 'Native output exceeds bound.')
    native_line, _, raw = result.stdout.partition(b'\n')
    native = json.loads(native_line)
    require(native['status'] == 'PASSED' and native['hardware'] and native['debug_layer'] and native['warnings_errors'] == 0,
            'Native GPU evidence is incomplete.')
    row['native'] = native
    if expected is not None:
        require(native['draw_executed'], 'Native fixture did not draw.')
        with pixels.open('xb') as stream: stream.write(raw)
        require(raw[:8] == b'FOAPIX01' and len(raw) >= 16, 'Invalid native pixel output.')
        w, h = struct.unpack_from('<2I', raw, 8)
        require(1 <= w <= 1024 and 1 <= h <= 1024 and len(raw) == 16+w*h*16, 'Native pixel dimensions mismatch.')
        require(0 < len(expected) <= w*h, 'Expected pixel set is empty or oversized.')
        maximum = 0.0
        positions = set()
        for x, y, colour in expected:
            require(type(x) is int and type(y) is int and 0 <= x < w and 0 <= y < h, 'Expected pixel lies outside target.')
            require((x, y) not in positions and len(colour) == 4, 'Duplicate or malformed expected pixel.')
            positions.add((x, y))
            actual = struct.unpack_from('<4f', raw, 16+(y*w+x)*16)
            require(all(math.isfinite(v) for v in actual), 'GPU emitted nonfinite colour.')
            difference = max(abs(a-number(b)) for a, b in zip(actual, colour))
            maximum = max(maximum, difference)
            require(difference <= 2e-5, 'GPU pixel differs at (%s, %s): %s vs %s' % (x, y, actual, colour))
        row.update(pixel_samples=len(expected), max_absolute_error=maximum, pixels_sha256=sha(raw))
    else:
        require(not raw and not native['draw_executed'], 'Unexpected draw or trailing native output.')
    row['status'] = 'PASSED'
    return row


def synthetic_acceptance(executable, root):
    """Standalone hardware calibration and native parser controls; no game inputs."""
    from source_shader_native_cases import Compiler
    compiler = Compiler()
    vertex = compiler.compile(
        b'void main(float3 p:POSITION,float2 u:TEXCOORD0,out float4 q:SV_Position,out float2 v:TEXCOORD3)'
        b'{q=float4(p,1);v=u;}', b'main', b'vs_5_0')
    fragment = compiler.compile(
        b'Texture2D<float4> image:register(t1);SamplerState filtering:register(s0);'
        b'float4 main(float4 p:SV_Position,float2 uv:TEXCOORD3):SV_Target'
        b'{return image.SampleLevel(filtering,uv,0);}', b'main', b'ps_5_0')
    shaders = [(1, vertex), (2, fragment)]
    colours = [(1,0,0,1),(0,1,0,1),(0,0,1,1),(1,1,0,1)]
    vertices, expected = [], []
    for index, uv in enumerate(((.25,.25),(.75,.25),(.25,.75),(.75,.75))):
        centre = -.75 + index*.5
        for x, y in ((-.2,-.5),(-.2,.5),(.2,.5),(-.2,-.5),(.2,.5),(.2,-.5)):
            vertices.append((centre+x,y,.5,*uv))
        for dx in (-1,0,1):
            for dy in (-1,0,1): expected.append((16+index*32+dx,32+dy,colours[index]))
    texture = (2,1,{'width':2,'height':2,'pixels':colours})
    sampler = (2,0,{'filter':0,'u':1,'v':1})
    valid = packet(shaders, vertices=vertices, textures=[texture], samplers=[sampler])
    rows = [execute(executable, valid, root, 'calibration', expected=expected)]
    for label, data in (
        ('unknown-version', b'UNKNOWN!'+valid[8:]),
        ('truncated', valid[:-1]),
        ('trailing', valid+b'\0'),
        ('duplicate-texture', packet(shaders, vertices=vertices, textures=[texture,texture], samplers=[sampler])),
        ('duplicate-sampler', packet(shaders, vertices=vertices, textures=[texture], samplers=[sampler,sampler])),
        ('wrong-draw-stage', packet([(2,fragment),(1,vertex)], vertices=vertices)),
        ('invalid-dxbc', packet([(1,b'DXBC'+bytes(28))]))):
        rows.append(execute(executable, data, root, label, rejected=True))
    # Exact Unity direct-RenderTexture facing is measured by FoaTriangleFacingProbe.
    # Keep two winding controls: Cull None still affects SV_IsFrontFace consumers.
    facing_fragment = compiler.compile(
        b'float4 main(float4 p:SV_Position,float2 uv:TEXCOORD3,bool front:SV_IsFrontFace):SV_Target'
        b'{return float4(front?1:0,.25,.5,1);}', b'main', b'ps_5_0')
    triangle = [(-.75,-.5,.5,0,0),(.75,-.5,.5,0,0),(.75,.5,.5,0,0)]
    for reverse in (False, True):
        winding = [triangle[i] for i in ((0,2,1) if reverse else (0,1,2))]
        rows.append(execute(executable, packet([(1,vertex),(2,facing_fragment)], vertices=winding), root,
                            'facing-reversed' if reverse else 'facing-direct',
                            expected=[(96,40,(0 if reverse else 1,.25,.5,1))]))
    report = {'status':'PASSED', 'cases':rows, 'source':'synthetic',
              'compiler_sha256':sha(compiler.path.read_bytes())}
    with (root/'acceptance.json').open('x', encoding='utf-8') as stream: json.dump(report, stream, indent=2)
    return report


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output-root', required=True, type=Path, help='New private test directory outside source control.')
    args = parser.parse_args()
    args.output_root.mkdir(parents=True, exist_ok=False)
    result = synthetic_acceptance(args.executable, args.output_root)
    print(json.dumps({'status':result['status'], 'cases':len(result['cases'])}))
