# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native full-affine GPU fixture; tests arithmetic, not game shaders or scene editing."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from foa_scene_asset_binding import SourceAssetReference
from foa_scene_draws import project_source_submesh_geometry
from foa_scene_mesh import SourceMesh
from foa_scene_native_mesh_audit import NativeCache,audit_projection,require
from foa_scene_transforms import IDENTITY,host_matrix,multiply,normal_matrix,point,trs


def unit(v):
    length=math.sqrt(sum(x*x for x in v));require(length>0,'Invalid synthetic normal.')
    return tuple(x/length for x in v)


def prepare(root,project):
    folder=project/'Assets/affine_gpu_cases';folder.mkdir(exist_ok=False)
    positions=[(-.35,-.2,-.3),(.25,-.2,.1),(-.1,.3,.2)]
    a=tuple(positions[1][i]-positions[0][i] for i in range(3));b=tuple(positions[2][i]-positions[0][i] for i in range(3))
    normal=unit((a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]))
    shader='''
#include <viewsrg_all.srgi>
#include <Atom/Features/PBR/DefaultObjectSrg.azsli>
#include <Atom/RPI/ShaderResourceGroups/DefaultDrawSrg.azsli>
#include <Atom/Features/InstancedTransforms.azsli>
ShaderResourceGroup MaterialSrg : SRG_PerMaterial
{
    float4 m_row0; float4 m_row1; float4 m_row2;
    float4 m_normal0; float4 m_normal1; float4 m_normal2;
    float4 m_color; bool m_reference;
}
struct VertexInput { float3 position : POSITION; float3 normal : NORMAL; };
struct VertexOutput { float4 position : SV_Position; float3 normal : NORMAL; };
VertexOutput MainVS(VertexInput input,uint instanceId : SV_InstanceID)
{
    VertexOutput output;float4 p=float4(input.position,1);
    float3 transformed=float3(dot(MaterialSrg::m_row0,p),dot(MaterialSrg::m_row1,p),dot(MaterialSrg::m_row2,p));
    float3 world=mul(GetObjectToWorldMatrix(instanceId),float4(transformed,1)).xyz;
    output.position=float4(world.x,world.z,.5,1);
    float4 n=float4(input.normal,0);
    output.normal=normalize(float3(dot(MaterialSrg::m_normal0,n),dot(MaterialSrg::m_normal1,n),dot(MaterialSrg::m_normal2,n)));
    return output;
}
float4 MainPS(VertexOutput input) : SV_Target0
{
    if(MaterialSrg::m_reference)return MaterialSrg::m_color;
    return float4(normalize(input.normal)*.5+.5,1);
}
'''
    (folder/'affine.azsl').write_text(shader)
    (folder/'affine.shader').write_text(json.dumps({'Source':'affine.azsl','DepthStencilState':{'Depth':{'Enable':False}},'RasterState':{'CullMode':'None'},'DrawList':'auxgeom','ProgramSettings':{'EntryPoints':[{'name':'MainVS','type':'Vertex'},{'name':'MainPS','type':'Fragment'}]}}))
    properties=[{'name':name,'type':'Vector4','defaultValue':list(IDENTITY[row*4:row*4+4]),'connection':{'type':'ShaderInput','name':'m_'+name}} for prefix in ('row','normal') for row,name in enumerate(prefix+str(i) for i in range(3))]
    properties += [{'name':'color','type':'Vector4','defaultValue':[0,0,0,1],'connection':{'type':'ShaderInput','name':'m_color'}},{'name':'reference','type':'Bool','defaultValue':False,'connection':{'type':'ShaderInput','name':'m_reference'}}]
    (folder/'affine.materialtype').write_text(json.dumps({'description':'Synthetic full-affine position and inverse-transpose normal test. Not a game material.','version':1,'propertyLayout':{'propertyGroups':[{'name':'probe','properties':properties}]},'shaders':[{'file':'affine.shader'}]}))
    mesh=SourceMesh({0:positions,1:[normal]*3},[[(0,1,2)]],{'serialized_file':'synthetic','path_id':'1','record_sha256':'1'*64},0)
    data,projection=project_source_submesh_geometry(mesh,0);(folder/'triangle_foamesh.glb').write_bytes(data)
    bg=SourceMesh({0:[(-1,-1,0),(1,-1,0),(1,1,0),(-1,1,0)],1:[(0,0,1)]*4},[[(0,1,2),(0,2,3)]],{'serialized_file':'synthetic','path_id':'2','record_sha256':'2'*64},0)
    data,_=project_source_submesh_geometry(bg,0);(folder/'background_foamesh.glb').write_bytes(data)
    matrices=[('identity',IDENTITY),('nonuniform',trs((.3,.2,0),(0,0,math.sin(.2),math.cos(.2)),(1.2,.65,.85))),
      ('reflection',trs((-.25,.15,0),(0,math.sin(.15),0,math.cos(.15)),(-1.1,.9,1.2))),
      ('shear',(.8,.45,.15,.2, -.25,1.1,.35,-.15, .2,-.15,.9,.1, 0,0,0,1)),
      ('parent_scale',multiply(trs((0,.1,.2),(0,0,math.sin(.2),math.cos(.2)),(1.25,.7,1.)),trs((.15,0,0),(math.sin(.23),0,0,math.cos(.23)),(.8,1.,1.1)))),
      ('renderer_offset',multiply(trs((.1,-.2,.1),(0,math.sin(.21),0,math.cos(.21)),(.9,.8,1.1)),trs((-.2,.15,.1),(0,0,math.sin(.17),math.cos(.17)),(-1.,1.,1.))))]
    cases=[]
    for name,matrix in matrices:
        m=host_matrix(matrix);n=normal_matrix(m);v=unit(point(normal_matrix(matrix),normal));expected=[v[0]*.5+.5,v[2]*.5+.5,v[1]*.5+.5,1.]
        values={'probe.'+prefix+str(i):list(value[i*4:i*4+4]) for prefix,value in [('row',m),('normal',n)] for i in range(3)}
        (folder/(name+'.material')).write_text(json.dumps({'materialType':'affine.materialtype','materialTypeVersion':1,'propertyValues':values}))
        (folder/(name+'_reference.material')).write_text(json.dumps({'materialType':'affine.materialtype','materialTypeVersion':1,'propertyValues':{'probe.reference':True,'probe.color':expected}}))
        points=[point(matrix,p) for p in positions];centres=[]
        for weights in ((1/3,1/3,1/3),(.6,.2,.2),(.2,.6,.2),(.2,.2,.6)):
            x,y=[sum(weights[i]*points[i][j] for i in range(3)) for j in range(2)]
            centres.append([(x+1)/2,(1-y)/2])
        cases.append({'name':name,'source_matrix':matrix,'host_matrix':m,'normal_matrix':n,'expected_normal_color':expected,'sample_centres':centres,'model':'assets/affine_gpu_cases/triangle_foamesh.glb.azmodel','material':'assets/affine_gpu_cases/'+name+'.azmaterial','reference':'assets/affine_gpu_cases/'+name+'_reference.azmaterial'})
    (folder/'black.material').write_text(json.dumps({'materialType':'affine.materialtype','materialTypeVersion':1,'propertyValues':{'probe.reference':True,'probe.color':[0,0,0,1]}}))
    report={'project':str(project),'cases':cases,'projection':projection,'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir()}}
    with (root/'affine-gpu-fixtures.json').open('x') as f:json.dump(report,f,indent=2)
    print(json.dumps({'prepared':len(cases),'project':str(project)}))


def verify(root,output):
    from PIL import Image
    fixture=json.loads((root/'affine-gpu-fixtures.json').read_text());report=json.loads((root/'affine-gpu-editor.json').read_text());project=Path(fixture['project']);folder=project/'Assets/affine_gpu_cases'
    require(report.get('capture_status')=='PASSED' and [r['name'] for r in report['cases']]==[r['name'] for r in fixture['cases']],'Incomplete native affine captures.')
    for name,digest in fixture['files'].items():require(hashlib.sha256((folder/name).read_bytes()).hexdigest()==digest,'Changed native affine fixture.')
    captures={};samples=0
    for case in fixture['cases']:
        frames=[]
        for suffix in ('reference','actual'):
            path=root/(case['name']+'-'+suffix+'.ppm');require(path.stat().st_size<64*1024*1024,'Oversized affine capture.')
            with Image.open(path) as image:
                require(image.mode=='RGB' and 100<=image.width<=8192 and 100<=image.height<=8192,'Unexpected affine capture dimensions.')
                frames.append(image.copy())
            captures[path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
        for x,y in case['sample_centres']:
            for dx,dy in ((-1,-1),(0,0),(1,1)):
                require(frames[0].size==frames[1].size,'Affine reference/actual dimensions differ.');pixel=(int(x*frames[0].width)+dx,int(y*frames[0].height)+dy);a,b=(im.getpixel(pixel) for im in frames)
                require(max(abs(x-y) for x,y in zip(a,b))<=2 and max(b)>25,'Native affine position/normal mismatch: '+case['name'])
                samples+=1
    cache=NativeCache(project/'Cache/pc')
    try:geometry=audit_projection(cache,'Assets/affine_gpu_cases/triangle_foamesh.glb',(folder/'triangle_foamesh.glb').read_bytes())
    finally:cache.close()
    result={'status':'PASSED','cases':len(fixture['cases']),'pixel_samples':samples,'captures':captures,'geometry':geometry,'scope':'Synthetic full-affine shader position and inverse-transpose normal arithmetic','native_scene_component':'NOT_RUN','instancing_bounds_picking':'NOT_RUN','game_materials':'NOT_RUN','game_runtime':'NOT_RUN'}
    with output.open('x') as f:json.dump(result,f,indent=2)
    print(json.dumps({'status':'PASSED','cases':len(fixture['cases']),'pixel_samples':samples}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=['prepare','verify']);parser.add_argument('--root',type=Path,required=True);parser.add_argument('--project',type=Path);parser.add_argument('--output',type=Path)
    args=parser.parse_args();root=args.root.resolve(strict=True)
    require(not any((p/'.git').exists() for p in (root,*root.parents)),'Private GPU evidence must be outside source control.')
    if args.action=='prepare':
        if args.project is None:parser.error('prepare requires --project')
        project=args.project.resolve(strict=True);require(not any((p/'.git').exists() for p in (project,*project.parents)),'Private synthetic project required.');prepare(root,project)
    else:
        if args.output is None:parser.error('verify requires --output')
        verify(root,args.output)
