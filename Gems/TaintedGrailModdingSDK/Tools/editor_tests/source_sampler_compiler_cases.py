# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Check synthetic Unity compiler encodings against declared sampler cases."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import foa_heightmap_importer as h
import foa_scene_shader as s
from foa_scene_material import inline_sampler, require
sys.path.insert(0,str(Path(__file__).resolve().parent))
from source_shader_native_cases import Compiler

def verify(root,output):
    root=root.resolve(strict=True)
    report_path=root/'ProbeOutput/result.json'
    raw=report_path.read_bytes()
    require(len(raw)<65536,'Oversize synthetic compiler receipt.')
    receipt=json.loads(raw)
    require(receipt['status']=='PASSED' and receipt['unityVersion']=='6000.0.64f1' and
            receipt['graphicsApi']=='Direct3D11','Unqualified synthetic compiler profile.')
    cases=receipt['cases']
    require(len(cases)==27 and len({c['name'] for c in cases})==27,'Missing/duplicate compiler cases.')
    expected={ 'FOA Synthetic/'+c['name']:c for c in cases}
    u,_=h.import_unitypy(h.UNITY_FALLBACK_VERSION)
    bundle=root/'ProbeOutput/sampler-probe'
    before=hashlib.sha256(bundle.read_bytes()).hexdigest()
    env=u.load(str(bundle))
    results=[];compiler=Compiler()
    for obj in env.objects:
        if obj.type.name!='Shader':continue
        tree=obj.read_typetree()
        name=tree['m_ParsedForm']['m_Name']
        require(name in expected,'Unexpected/duplicate synthetic shader.')
        case=expected.pop(name)
        entries=s.unpack_entries(tree,bytes(tree['compressedBlob']))
        programs=s.player_programs(tree,entries)
        require(len(programs)==2,'Unexpected synthetic program count.')
        encodings=[]
        for program in programs:
            code,meta=s.dxbc_container(entries[program['code_entry']])
            compiler.disassemble(code)
            params=s.parameter_record(entries[program['parameter_entry']])
            bindings=s.variant_resources(tree,program,params)
            encodings.extend(b['signature'][2] for b in bindings if b['signature'][0]==4)
        require(len(encodings)==1,'Synthetic sampler was stripped or duplicated.')
        decoded=inline_sampler(encodings[0])
        require(decoded['filter']=={'Linear':'Bilinear'}.get(case['filter'],case['filter']) and
                [decoded['wrap_'+a] for a in 'uvw']==[case[a] for a in 'uvw'] and
                decoded['comparison']==case['comparison'] and
                decoded['requested_anisotropy']==case['anisotropy'] and not decoded['unresolved_bits'],
                'Compiler output disagrees with explicitly declared sampler state.')
        results.append({'case':case,'encoded':encodings[0],'status':'PASSED'})
    require(not expected,'Not every synthetic shader was built.')
    colors=[[c[k] for k in 'rgba'] for c in receipt['samples']]
    require(colors==[[1,0,0,1],[0,1,0,1],[0,0,1,1],[1,1,0,1]],'Unity GPU orientation mismatch.')
    require(hashlib.sha256(bundle.read_bytes()).hexdigest()==before,'Synthetic bundle changed.')
    result={'status':'PASSED','cases':results,'unity_version':receipt['unityVersion'],
            'graphics_api':receipt['graphicsApi'],'device':receipt['device'],
            'bundle_sha256':before,'compiler_receipt_sha256':hashlib.sha256(raw).hexdigest(),
            'unity_raw_row_gpu_orientation':'PASSED','o3de_rendering':'NOT_RUN','game_runtime':'NOT_RUN'}
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2)
    print(json.dumps({'status':'PASSED','compiler_cases':len(results),'unity_gpu_samples':len(colors)}))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();verify(args.root,args.output)
