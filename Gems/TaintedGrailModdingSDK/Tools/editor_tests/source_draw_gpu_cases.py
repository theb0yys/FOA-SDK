# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic native draw-selection fixture; paired with FoaDrawProbe.cs."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_scene_asset_binding import SourceAssetReference
from foa_scene_draws import drake_draw_slots, project_drake_draw_geometry, project_source_submesh_geometry
from foa_scene_mesh import SourceMesh
from foa_scene_native_mesh_audit import NativeCache, audit_projection, require


def prepare(root):
    folder = root/'Project/Assets/draw_gpu_cases_ordered'
    folder.mkdir(exist_ok=False)
    shader = '''
#include <viewsrg_all.srgi>
#include <Atom/Features/PBR/DefaultObjectSrg.azsli>
#include <Atom/RPI/ShaderResourceGroups/DefaultDrawSrg.azsli>
#include <Atom/Features/InstancedTransforms.azsli>
ShaderResourceGroup MaterialSrg : SRG_PerMaterial { float4 m_color; }
struct VertexInput { float3 position : POSITION; };
float4 MainVS(VertexInput input, uint instanceId : SV_InstanceID) : SV_Position
{
    float3 world = mul(GetObjectToWorldMatrix(instanceId), float4(input.position,1)).xyz;
    return float4(world.x, world.z, 0.5, 1);
}
float4 MainPS() : SV_Target0 { return MaterialSrg::m_color; }
'''
    (folder/'draw.azsl').write_text(shader)
    for additive in (False, True):
        name = 'additive' if additive else 'opaque'
        desc = {'Source':'draw.azsl','DepthStencilState':{'Depth':{'Enable':False}},
            'RasterState':{'CullMode':'None'},'DrawList':'auxgeom',
            'ProgramSettings':{'EntryPoints':[{'name':'MainVS','type':'Vertex'},{'name':'MainPS','type':'Fragment'}]}}
        if additive:
            desc['GlobalTargetBlendState'] = {'Enable':True,'BlendSource':'One','BlendDest':'One','BlendOp':'Add'}
        (folder/(name+'.shader')).write_text(json.dumps(desc))
        (folder/(name+'.materialtype')).write_text(json.dumps({'description':'Synthetic draw multiplicity, not a game material.', 'version':1,
            'propertyLayout':{'propertyGroups':[{'name':'probe','properties':[{'name':'color','type':'Vector4','defaultValue':[0,0,0,1], 'connection':{'type':'ShaderInput','name':'m_color'}}]}]},'shaders':[{'file':name+'.shader'}]}))
        for index, color in enumerate(([1,0,0,1],[0,1,0,1],[0,0,1,1],[0,0,0,1],[.25,.25,.25,1])):
            if additive:color = [v*.25 for v in color]
            (folder/(name+str(index)+'.material')).write_text(json.dumps({'materialType':name+'.materialtype','materialTypeVersion':1,'propertyValues':{'probe.color':color}}))
    refs = [SourceAssetReference('Material',str(i)*32,None) for i in (1,2,3)]
    cases=[]
    for name, count, draw_count, additive in [('fewer',2,1,False),('balanced',3,3,False),('repeated',1,3,False),('additive',1,3,True)]:
        positions=[];submeshes=[]
        for i in range(count):
            x=-.66+i*.66;v=len(positions)
            positions.extend([(x-.22,-.3,0),(x+.22,-.3,0),(x+.22,.3,0),(x-.22,.3,0)])
            submeshes.append([(v,v+1,v+2),(v,v+2,v+3)])
        mesh=SourceMesh({0:positions,1:[(0.,0.,-1.)]*len(positions)},submeshes,{'serialized_file':'synthetic','path_id':'1','record_sha256':'1'*64},0)
        draws=[];sources={}
        for slot in drake_draw_slots(count,refs[:draw_count],unity_version='6000.0.64f1'):
            source=name+'_submesh'+str(slot.effective_submesh)+'_foamesh.glb'
            if source not in sources:
                data,report=project_source_submesh_geometry(mesh,slot.effective_submesh)
                (folder/source).write_bytes(data);sources[source]=report
            draws.append({'ordinal':slot.material_ordinal,'source':source,'model':'assets/draw_gpu_cases_ordered/'+source+'.azmodel',
                'source_submesh':slot.effective_submesh,'sort_key':slot.material_ordinal+1,
                'material':'assets/draw_gpu_cases_ordered/'+('additive' if additive else 'opaque')+str(slot.material_ordinal)+'.azmaterial'})
        cases.append({'name':name,'draws':draws,'projections':sources})
    # Fixed black background and a flat quarter-grey reference avoid assuming native display transfer.
    mesh=SourceMesh({0:[(-1,-1,0),(1,-1,0),(1,1,0),(-1,1,0)],1:[(0.,0.,-1.)]*4},[[(0,1,2),(0,2,3)]],{'serialized_file':'synthetic','path_id':'2','record_sha256':'2'*64},0)
    data,_=project_drake_draw_geometry(mesh,refs[:1],unity_version='6000.0.64f1');(folder/'background_foamesh.glb').write_bytes(data)
    report={'cases':cases,'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir()},'sample_centres':[[.17,.5],[.5,.5],[.83,.5]]}
    with (root/'draw-gpu-fixtures.json').open('x') as f:json.dump(report,f,indent=2)
    print(json.dumps({'prepared':len(cases)}))


def verify(root, output):
    from PIL import Image
    fixture=json.loads((root/'draw-gpu-fixtures.json').read_text());receipt=json.loads((root/'draw-gpu-editor.json').read_text())
    require(receipt.get('capture_status')=='PASSED' and [r['name'] for r in receipt['cases']]==['fewer','balanced','repeated','additive'],'Native draw captures missing.')
    folder=root/'Project/Assets/draw_gpu_cases_ordered'
    for name,digest in fixture['files'].items():
        path=(folder/name).resolve(strict=True)
        require(path.parent==folder and hashlib.sha256(path.read_bytes()).hexdigest()==digest,'Synthetic native draw fixture changed.')
    captures={};pixels={}
    for name in ['black','reference','fewer','balanced','repeated','additive']:
        path=root/('draw-gpu-'+name+'.ppm');require(path.stat().st_size<64*1024*1024,'Native capture exceeds budget.')
        with Image.open(path) as frame:
            require(frame.mode=='RGB' and 100<=frame.width<=8192 and 100<=frame.height<=8192,'Unexpected draw capture.')
            pixels[name]=[[frame.getpixel((int(x*frame.width)+dx,int(y*frame.height)+dy)) for dy in (-1,0,1) for dx in (-1,0,1)] for x,y in fixture['sample_centres']]
        captures[path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
    expected={'fewer':[(255,0,0),None,None],'balanced':[(255,0,0),(0,255,0),(0,0,255)],'repeated':[(0,0,255),None,None],'additive':['reference',None,None]}
    for name, regions in expected.items():
        for i, target in enumerate(regions):
            targets=pixels['black'][i] if target is None else pixels['reference'][i] if target=='reference' else [target]*9
            require(all(max(abs(a-b) for a,b in zip(actual,wanted))<=2 for actual,wanted in zip(pixels[name][i],targets)),'Native draw pixels disagree: '+name+' region '+str(i))
    cache=NativeCache(root/'Project/Cache/pc')
    try:audits=[audit_projection(cache,'Assets/draw_gpu_cases_ordered/'+source,(folder/source).read_bytes()) for r in fixture['cases'] for source in r['projections']]
    finally:cache.close()
    report={'status':'PASSED','cases':4,'pixel_samples':108,'native_geometry':audits,'native_products':cache.products,'captures':captures,'scope':'Synthetic submesh selection, per-draw binding and explicit ordering of repeated opaque/additive draws','game_draw_bin_order':'NOT_RUN','game_materials':'NOT_RUN','game_runtime':'NOT_RUN'}
    with output.open('x') as f:json.dump(report,f,indent=2)
    print(json.dumps({'status':'PASSED','cases':4,'pixel_samples':108}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=('prepare','verify'));parser.add_argument('--root',type=Path,required=True);parser.add_argument('--output',type=Path)
    args=parser.parse_args();root=args.root.resolve(strict=True)
    require(not any((p/'.git').exists() for p in (root,*root.parents)),'Expected private synthetic fixture storage.')
    if args.action=='prepare':prepare(root)
    else:
        if args.output is None:parser.error('verify requires --output')
        verify(root,args.output)
