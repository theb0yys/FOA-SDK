# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Compare native Alpha8 channels/mips with a flat RGBA reference and Unity GPU samples.

Use a disposable native project and source_material_gpu_editor.py for capture.
The four cases remain compatible with that runner. No game content is a fixture.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_scene_mesh import SourceMesh
from foa_scene_mesh_glb import encode_glb
from foa_scene_texture import Texture, encode
from foa_scene_native_mesh_audit import NativeCache, audit_projection, require
from foa_scene_native_texture_audit import audit_texture
sys.path.insert(0, str(Path(__file__).resolve().parent))
from source_material_gpu_cases import AZSL

RAW = bytes([0,17,34,51,68,85,102,119,136,153,170,187,204,221,238,255,43,87,131,175,219])
CASES = ('alpha_linear', 'reference', 'alpha_colour', 'zero_rgb')


def prepare(root):
    require((root/'Project/project.json').is_file(), 'Expected a disposable native project.')
    folder = root/'Project/Assets/material_gpu_cases'
    folder.mkdir(exist_ok=False)
    coordinates = [(2*m+(x+.5)/w,(y+.5)/w) for m,w in enumerate((4,2,1)) for y in range(w) for x in range(w)]
    positions, uvs, triangles, centres = [], [], [], []
    for i,(u,v) in enumerate(coordinates):
        cx,cy = .11+(i%7)*.13, .2+(i//7)*.29
        x,y = cx*2-1, 1-cy*2
        start = len(positions)
        positions.extend([(x-.075,y-.09,0.),(x+.075,y-.09,0.),(x+.075,y+.09,0.),(x-.075,y+.09,0.)])
        uvs.extend([(u,v)]*4)
        triangles.extend([(start,start+1,start+2),(start,start+2,start+3)])
        centres.append([cx,cy])
    mesh = SourceMesh({0:positions,1:[(0.,0.,-1.)]*len(positions),4:uvs},[triangles],
        {'serialized_file':'synthetic','path_id':'1','record_sha256':'1'*64},0)
    (folder/'alpha_foamesh.glb').write_bytes(encode_glb(mesh)[0])
    for name,color in (('alpha_linear',0),('alpha_colour',1)):
        (folder/(name+'.foatexture')).write_bytes(encode(Texture(4,4,3,1,color,RAW)))
    # Independent flat lookup: each expected alpha is an RGB texel with no mip/row reconstruction.
    rgba = bytes(channel for value in RAW for channel in (value,value,value,255))
    (folder/'reference.foatexture').write_bytes(encode(Texture(21,1,1,4,0,rgba)))
    shader = AZSL.replace('    bool m_recoverV;', '    bool m_recoverV;\n    bool m_reference;\n    bool m_alpha;')
    shader = shader[:shader.index('float4 MainPS')]+'''float4 MainPS(VertexOutput input) : SV_Target0
{
    float2 uv = input.uv;
    uv.y = 1.0 - uv.y;
    float mip = floor(uv.x * 0.5);
    uv.x -= 2.0 * mip;
    if (MaterialSrg::m_reference)
    {
        float width = mip == 0 ? 4 : (mip == 1 ? 2 : 1);
        float index = (mip == 0 ? 0 : (mip == 1 ? 16 : 20)) + floor(uv.y*width)*width + floor(uv.x*width);
        return float4(MaterialSrg::m_image.SampleLevel(MaterialSrg::m_sampling, float2((index+0.5)/21.0,0.5),0).rgb,1);
    }
    float4 value = MaterialSrg::m_image.SampleLevel(MaterialSrg::m_sampling,uv,mip);
    return float4(MaterialSrg::m_alpha ? value.aaa : value.rgb,1);
}
'''
    (folder/'probe.azsl').write_text(shader,encoding='utf-8')
    (folder/'probe.shader').write_text(json.dumps({'Source':'probe.azsl','DepthStencilState':{'Depth':{'Enable':False}},'RasterState':{'CullMode':'None'},'DrawList':'auxgeom','ProgramSettings':{'EntryPoints':[{'name':'MainVS','type':'Vertex'},{'name':'MainPS','type':'Fragment'}]}}),encoding='utf-8')
    props = [{'name':name,'type':kind,'defaultValue':value,'connection':{'type':'ShaderInput','name':'m_'+name}}
        for name,kind,value in [('image','Image',''),('reference','Bool',False),('alpha','Bool',True)]]
    (folder/'probe.materialtype').write_text(json.dumps({'description':'Synthetic Alpha8 channel and mip comparison.', 'version':1,'propertyLayout':{'propertyGroups':[{'name':'probe','properties':props}]},'shaders':[{'file':'probe.shader'}],'uvNameMap':{'UV0':'SourceUV0'}}),encoding='utf-8')
    cases=[]
    for name in CASES:
        image = name if name != 'zero_rgb' else 'alpha_linear'
        (folder/(name+'.material')).write_text(json.dumps({'materialType':'probe.materialtype','materialTypeVersion':1,'propertyValues':{'probe.image':image+'.foatexture','probe.reference':name=='reference','probe.alpha':name!='zero_rgb'}}),encoding='utf-8')
        cases.append({'name':name,'model':'assets/material_gpu_cases/alpha_foamesh.glb.azmodel','material':'assets/material_gpu_cases/'+name+'.azmaterial'})
    report={'cases':cases,'sample_centres':centres,'payload':list(RAW),'files':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in folder.iterdir()}}
    with (root/'material-gpu-fixtures.json').open('x') as f:json.dump(report,f,indent=2)
    print(json.dumps({'prepared':len(cases),'swatches_per_case':21}))


def verify(root,unity,output):
    from PIL import Image
    fixture=json.loads((root/'material-gpu-fixtures.json').read_text())
    receipt=json.loads((root/'material-gpu-editor.json').read_text())
    observed=json.loads(unity.read_text())
    require(observed['status']=='PASSED' and observed['alpha8Enum']==1 and observed['unityVersion']=='6000.0.64f1' and observed['graphicsApi']=='Direct3D11','Unexpected Unity Alpha8 profile.')
    require(len(observed['rows'])==2 and {r['linear'] for r in observed['rows']}=={False,True},'Missing source colour-space cases.')
    for row in observed['rows']:
        require((row['width'],row['height'],row['mipCount'])==(4,4,3) and row['payload']==list(RAW) and len(row['samples'])==21,'Source GPU inputs changed.')
        for sample,value in zip(row['samples'],RAW):
            require(all(sample[k]==0 for k in 'rgb') and abs(sample['a']-value/255)<1e-6,'Unity Alpha8 sample differs from the explicit channel oracle.')
    require(receipt.get('capture_status')=='PASSED' and [r['name'] for r in receipt['cases']]==list(CASES),'Native GPU cases missing or reordered.')
    require(len(fixture['sample_centres'])==21 and fixture['payload']==list(RAW),'Native GPU fixture changed.')
    folder=root/'Project/Assets/material_gpu_cases'
    for name,digest in fixture['files'].items():
        path=(folder/name).resolve(strict=True)
        require(path.parent==folder and hashlib.sha256(path.read_bytes()).hexdigest()==digest,'Native GPU source changed.')
    regions={};captures=[]
    for name in CASES:
        path=root/('material-gpu-'+name+'.ppm')
        require(path.stat().st_size<64*1024*1024,'GPU capture exceeds bounds.')
        with Image.open(path) as frame:
            require(frame.mode=='RGB' and 100<=frame.width<=8192 and 100<=frame.height<=8192,'Unexpected native capture.')
            regions[name]=[[frame.getpixel((int(x*frame.width)+dx,int(y*frame.height)+dy)) for dy in (-1,0,1) for dx in (-1,0,1)] for x,y in fixture['sample_centres']]
        captures.append({'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    reference=regions['reference']
    require(len({r[4] for r in reference[:16]})>=16 and all(reference[i][4][0]<reference[i+1][4][0] for i in range(15)), 'Flat native reference lost the explicit ramp.')
    for name in ('alpha_linear','alpha_colour'):
        for expected,actual in zip(reference,regions[name]):
            require(all(max(abs(a-b) for a,b in zip(x,y))<=2 for x,y in zip(expected,actual)), 'Native Alpha8 mip/alpha sampling differs from flat RGBA reference: '+name)
    black=reference[0][4]
    require(all(max(abs(a-b) for a,b in zip(pixel,black))<=2 for region in regions['zero_rgb'] for pixel in region),'Native Alpha8 exposes nonzero RGB.')
    cache=NativeCache(root/'Project/Cache/pc')
    try:
        textures=[audit_texture(cache,'Assets/material_gpu_cases/'+n+'.foatexture',(folder/(n+'.foatexture')).read_bytes()) for n in ('alpha_linear','alpha_colour','reference')]
        mesh=audit_projection(cache,'Assets/material_gpu_cases/alpha_foamesh.glb',(folder/'alpha_foamesh.glb').read_bytes())
    finally:cache.close()
    result={'status':'PASSED','cases':4,'pixel_samples':756,'native_textures':textures,'native_mesh':mesh,'native_products':cache.products,'captures':captures,'unity_receipt_sha256':hashlib.sha256(unity.read_bytes()).hexdigest(),'scope':'Synthetic Alpha8 RGB/alpha channels and all three mip levels; both data colour-space flags','game_shader_materials':'NOT_RUN','game_runtime':'NOT_RUN'}
    with output.open('x') as f:json.dump(result,f,indent=2)
    print(json.dumps({'status':'PASSED','cases':4,'pixel_samples':756}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=('prepare','verify'));parser.add_argument('--root',type=Path,required=True);parser.add_argument('--unity-receipt',type=Path);parser.add_argument('--output',type=Path)
    args=parser.parse_args();root=args.root.resolve(strict=True)
    require(not any((p/'.git').exists() for p in (root,*root.parents)), 'Use private synthetic fixture storage.')
    if args.action=='prepare':prepare(root)
    else:
        if args.unity_receipt is None or args.output is None:parser.error('verify needs --unity-receipt and --output')
        verify(root,args.unity_receipt,args.output)
