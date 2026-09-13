# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Independent Windows DXBC checks. Does not execute shaders or launch a game."""
import argparse
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from foa_scene_shader import ShaderEntry,dxbc_container
from foa_heightmap_importer import HeightmapImportError
from foa_scene_material import inline_sampler


from foa_native_shader_compiler import Compiler


def audit_registers(assembly, resources):
    """Every native resource declaration needs an explicit serialized owner.

    This checks binding coverage, not constant-buffer values or shader execution.
    Extra serialized resources can be unused after compiler optimization.
    """
    bindings = {(r['namespace'],r['index']):r for r in resources}
    if len(bindings) != len(resources): raise RuntimeError('Duplicate resolved shader register.')
    seen = set()
    for line in assembly.splitlines():
        if not re.match(r'^\s*dcl_(?:resource|sampler|constantbuffer|uav)',line): continue
        matches = re.findall(r'\b(cb|t|s|u)(\d+)\b',line,re.I)
        if len(matches) != 1: raise RuntimeError('Unqualified native resource declaration syntax.')
        namespace, index = matches[0]
        key = namespace.lower(), int(index)
        if key in seen or key not in bindings: raise RuntimeError('Native shader register has no unique serialized owner.')
        seen.add(key)
        signature = bindings[key]['signature']
        if key[0] == 's' and signature[0] == 4:
            compare = inline_sampler(signature[2])['comparison']
            if compare != ('mode_comparison' in line): raise RuntimeError('Inline sampler comparison disagrees with native code.')
    return len(seen)

def verify(output):
    compiler=Compiler();rows=[]
    for kind,target,entry,source in (
        (15,b'vs_4_0',b'main',b'float4 main(float4 p:POSITION):SV_Position{return p;}'),
        (16,b'vs_5_0',b'main',b'float4 main(float4 p:POSITION):SV_Position{return p;}'),
        (17,b'ps_4_0',b'main',b'float4 main(float2 uv:TEXCOORD0):SV_Target{return float4(uv,0,1);}'),
        (18,b'ps_5_0',b'main',b'Texture2D<float4> image:register(t3); SamplerState filtering:register(s2); float4 main(float2 uv:TEXCOORD0):SV_Target{return image.Sample(filtering,uv);}')
    ):
        blob=compiler.compile(source,entry,target)
        # Arbitrary synthetic wrapper prefix proves extraction uses the complete
        # validated container boundary, not the observed game prefix length.
        wrapped=b'synthetic prefix'+blob
        envelope=struct.pack('<8I',202012090,kind,0,0,0,0,0,len(wrapped))+wrapped
        recovered,meta=dxbc_container(ShaderEntry(0,0,envelope))
        if recovered!=blob: raise RuntimeError('Synthetic DXBC envelope changed its payload.')
        assembly,stage=compiler.disassemble(recovered)
        if stage!=target.decode(): raise RuntimeError('Synthetic shader stage mismatch.')
        rows.append({'case':stage,'status':'PASSED','sha256':hashlib.sha256(blob).hexdigest()})
        if target == b'ps_5_0':
            bindings=[{'namespace':'t','index':3,'signature':(0,'image')},
                      {'namespace':'s','index':2,'signature':(4,'',1)}]
            if audit_registers(assembly,bindings) != 2: raise RuntimeError('Native synthetic bindings were not exercised.')
            for label,bad in (('missing_texture',bindings[1:]),
                              ('wrong_comparison',[bindings[0],{'namespace':'s','index':2,'signature':(4,'',257)}])):
                try: audit_registers(assembly,bad)
                except RuntimeError: rows.append({'case':label,'status':'PASSED'})
                else: raise RuntimeError('Native binding corruption was not detected.')

    # Structure corruption must be rejected before the native disassembler sees it.
    for name,changed in (('truncated',envelope[:-1]),('future_envelope',struct.pack('<I',202112090)+envelope[4:])):
        try: dxbc_container(ShaderEntry(0,0,changed))
        except HeightmapImportError: rows.append({'case':name,'status':'PASSED'})
        else: raise RuntimeError('Corrupt synthetic shader was accepted.')
    report={'status':'PASSED','cases':rows,'compiler':str(compiler.path),
            'compiler_sha256':hashlib.sha256(compiler.path.read_bytes()).hexdigest(),
            'gpu_execution':'NOT_RUN','o3de_shader_mapping':'NOT_RUN'}
    with output.open('x',encoding='utf-8') as stream: json.dump(report,stream,indent=2)
    print(json.dumps({'status':'PASSED','cases':len(rows)}))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    verify(parser.parse_args().output)
