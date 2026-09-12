# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic O3DE material/UV fixture. No game data, host launch or automatic import."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from foa_scene_mesh import SourceMesh
from foa_scene_mesh_glb import encode_glb
from foa_scene_texture import Texture, encode

AZSL = """
#include <viewsrg_all.srgi>
#include <Atom/Features/PBR/DefaultObjectSrg.azsli>
#include <Atom/RPI/ShaderResourceGroups/DefaultDrawSrg.azsli>
#include <Atom/Features/InstancedTransforms.azsli>
ShaderResourceGroup MaterialSrg : SRG_PerMaterial
{
    Texture2D<float4> m_image;
    float4 m_st;
    bool m_recoverV;
    Sampler m_sampling
    {
        MinFilter = Point;
        MagFilter = Point;
        MipFilter = Point;
        AddressU = Wrap;
        AddressV = Wrap;
        AddressW = Wrap;
    };
}
struct VertexInput { float3 position : POSITION; float2 uv : UV0; };
struct VertexOutput { float4 position : SV_Position; float2 uv : UV0; };
VertexOutput MainVS(VertexInput input, uint instanceId : SV_InstanceID)
{
    VertexOutput output;
    // Synthetic clip-space swatches; input positions traverse the real mesh builder.
    float3 world = mul(GetObjectToWorldMatrix(instanceId), float4(input.position, 1)).xyz;
    output.position = float4(world.x, world.z, 0.5, 1);
    output.uv = input.uv;
    return output;
}
float4 MainPS(VertexOutput input) : SV_Target0
{
    float2 uv = input.uv;
    if (MaterialSrg::m_recoverV) uv.y = 1.0 - uv.y;
    uv = uv * MaterialSrg::m_st.xy + MaterialSrg::m_st.zw;
    return MaterialSrg::m_image.SampleLevel(MaterialSrg::m_sampling, uv, 0);
}
"""

def prepare(root):
    root=root.resolve(strict=True)
    if (root/'.git').exists() or not (root/'Project/project.json').exists():
        raise ValueError('Expected an explicit disposable native test project.')
    folder=root/'Project/Assets/material_gpu_cases'
    folder.mkdir(exist_ok=False)
    swatches=[(.25,.25),(.75,.25),(.25,.75),(.75,.75)]
    for name,st in [('base',(1.,1.,0.,0.)),('transformed',(-2.,1.5,.125,-.25))]:
        positions=[];uvs=[];triangles=[]
        for i,(u,v) in enumerate(swatches):
            x=-.6+.4*i;start=len(positions)
            positions.extend([(x-.13,.15,0.),(x+.13,.15,0.),(x+.13,.55,0.),(x-.13,.55,0.)])
            uvs.extend([((u-st[2])/st[0],(v-st[3])/st[1])]*4)
            triangles.extend([(start,start+1,start+2),(start,start+2,start+3)])
        mesh=SourceMesh({0:positions,1:[(0.,0.,-1.)]*16,4:uvs},[triangles],
            {'serialized_file':'synthetic','path_id':'1','record_sha256':'1'*64},0)
        (folder/(name+'_foamesh.glb')).write_bytes(encode_glb(mesh)[0])
    rgba=bytes([255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255])
    (folder/'orientation.foatexture').write_bytes(encode(Texture(2,2,1,4,0,rgba)))
    (folder/'probe.azsl').write_text(AZSL,encoding='utf-8')
    shader={'Source':'probe.azsl','DepthStencilState':{'Depth':{'Enable':False}},
        'RasterState':{'CullMode':'None'},'DrawList':'auxgeom','ProgramSettings':{'EntryPoints':[
            {'name':'MainVS','type':'Vertex'},{'name':'MainPS','type':'Fragment'}]}}
    (folder/'probe.shader').write_text(json.dumps(shader,indent=2),encoding='utf-8')
    props=[]
    for name,kind,value in [('image','Image',''),('st','Vector4',[1,1,0,0]),('recoverV','Bool',True)]:
        props.append({'name':name,'type':kind,'defaultValue':value,
                      'connection':{'type':'ShaderInput','name':'m_'+name}})
    mat_type={'description':'Synthetic raw-row and native UV proof; not a game material.',
        'version':1,'propertyLayout':{'propertyGroups':[{'name':'probe','properties':props}]},
        'shaders':[{'file':'probe.shader'}],'uvNameMap':{'UV0':'SourceUV0'}}
    (folder/'probe.materialtype').write_text(json.dumps(mat_type,indent=2),encoding='utf-8')
    cases=[]
    for name,mesh,st,flip,expect in [
        ('base','base',[1,1,0,0],True,True),
        ('transformed','transformed',[-2,1.5,.125,-.25],True,True),
        ('wrong_v','base',[1,1,0,0],False,False),
        ('wrong_st','transformed',[1,1,0,0],True,False)]:
        doc={'materialType':'probe.materialtype','materialTypeVersion':1,
             'propertyValues':{'probe.image':'orientation.foatexture','probe.st':st,'probe.recoverV':flip}}
        (folder/(name+'.material')).write_text(json.dumps(doc,indent=2),encoding='utf-8')
        cases.append({'name':name,'model':'assets/material_gpu_cases/'+mesh+'_foamesh.glb.azmodel',
            'material':'assets/material_gpu_cases/'+name+'.azmaterial','expect_match':expect})
    report={'cases':cases,'files':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in folder.iterdir()},
            'expected':[[255,0,0],[0,255,0],[0,0,255],[255,255,0]],'sample_centres':[[x,.325] for x in (.2,.4,.6,.8)]}
    with (root/'material-gpu-fixtures.json').open('x') as output:json.dump(report,output,indent=2)
    print(json.dumps({'prepared':len(cases)}))


def verify(root, output):
    from PIL import Image
    from foa_scene_native_mesh_audit import NativeCache, audit_projection
    from foa_scene_native_texture_audit import audit_texture
    root=root.resolve(strict=True)
    if any((p/'.git').exists() for p in (root,*root.parents)):
        raise ValueError('Expected private fixture storage.')
    receipt=json.loads((root/'material-gpu-editor.json').read_text())
    if receipt.get('capture_status')!='PASSED' or len(receipt['cases'])!=4:
        raise ValueError('Native Editor did not capture every case.')
    fixture=json.loads((root/'material-gpu-fixtures.json').read_text())
    folder=root/'Project/Assets/material_gpu_cases'
    for name,digest in fixture['files'].items():
        path=(folder/name).resolve(strict=True)
        if path.parent!=folder or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            raise ValueError('Synthetic source fixture changed.')
    cache=NativeCache(root/'Project/Cache/pc')
    try:
        image=audit_texture(cache,'Assets/material_gpu_cases/orientation.foatexture',
                            (folder/'orientation.foatexture').read_bytes())
        meshes=[audit_projection(cache,'Assets/material_gpu_cases/'+name+'_foamesh.glb',
                    (folder/(name+'_foamesh.glb')).read_bytes()) for name in ('base','transformed')]
    finally:cache.close()
    expected=[(255,0,0),(0,255,0),(0,0,255),(255,255,0)]
    patterns={'base':expected,'transformed':expected,
              'wrong_v':[expected[i] for i in (2,3,0,1)],
              'wrong_st':[expected[i] for i in (1,1,3,3)]}
    seen=set();rows=[]
    for case in receipt['cases']:
        name=case['name']
        if name not in patterns or name in seen:raise ValueError('Unexpected/duplicate GPU case.')
        seen.add(name)
        path=root/('material-gpu-'+name+'.ppm')
        if path.resolve()!=path or path.stat().st_size>64*1024*1024:raise ValueError('Unqualified capture path/size.')
        with Image.open(path) as frame:
            if frame.mode!='RGB' or not (32<=frame.width<=8192 and 32<=frame.height<=8192):
                raise ValueError('Unqualified GPU capture.')
            samples=[]
            for i,x in enumerate((.2,.4,.6,.8)):
                cx,cy=int(x*frame.width),int(.325*frame.height)
                region=[frame.getpixel((cx+dx,cy+dy)) for dy in (-1,0,1) for dx in (-1,0,1)]
                if any(max(abs(a-b) for a,b in zip(pixel,patterns[name][i]))>2 for pixel in region):
                    raise ValueError('Rendered swatch disagrees with the independent expected pattern: '+name)
                samples.append(region[4])
        rows.append({'case':name,'status':'PASSED','pixels':samples,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    if seen!=set(patterns):raise ValueError('A GPU control is missing.')
    # Detect product changes between the CPU channel audit and completed readback.
    for relative,digest in cache.products.items():
        if hashlib.sha256((cache.root/relative).read_bytes()).hexdigest()!=digest:
            raise ValueError('Native fixture product changed during GPU qualification.')
    result={'status':'PASSED','cases':rows,'native_texture':image,'native_meshes':meshes,
            'native_products':cache.products,'requested_gpu_api':'DX12',
            'scope':'Synthetic raw-row image, native UV0 recovery, scale and offset',
            'game_shaders':'NOT_RUN','game_runtime':'NOT_RUN'}
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2)
    print(json.dumps({'status':'PASSED','positive_cases':2,'negative_controls':2,'pixel_samples':144}))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('prepare','verify'))
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.action=='verify':
        if args.output is None:parser.error('verify requires --output')
        verify(args.root,args.output)
    else:prepare(args.root)
