# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic native texture-array GPU tests; never supplies missing game globals."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
sys.path[:0]=[str(Path(__file__).resolve().parents[1]),str(Path(__file__).resolve().parent)]
import foa_scene_native_shader as shader
from foa_scene_texture import Texture,TextureArray,encode
from source_shader_native_cases import Compiler


def private(path):
    path=path.resolve(strict=True)
    if any((p/'.git').exists() for p in (path,*path.parents)):
        raise RuntimeError('Array GPU tests require private storage.')
    return path


def prepare(root,project,baseline):
    root,project,baseline=private(root),private(project),private(baseline)
    assert (project/'project.json').is_file()
    folder=project/'Assets/texture_array_gpu_v1';folder.mkdir(exist_ok=False)
    assert not (root/'draw-fixtures-v1.json').exists()
    compiler=Compiler();assets=[];cases=[];pairs=[]
    def asset(name,data,extension):
        path=folder/(name+extension);path.write_bytes(data)
        assets.append({'path':str(path),'sha256':hashlib.sha256(data).hexdigest()})
        return 'assets/texture_array_gpu_v1/'+path.name
    state=dict(zip(shader.STATE_FIELDS,(0,0,0,7,0,1,0,0,1,0,0,15)))
    vs=compiler.compile(b'cbuffer V:register(b0){float4x4 transform;} void main(float3 p:POSITION,float2 u:TEXCOORD0,out float4 q:SV_Position,out float2 v:TEXCOORD3){q=mul(transform,float4(p,1));v=u;}',b'main',b'vs_5_0')
    shaders={}
    for operation in ('load','sample'):
        body=(b'uint w,h,n,l;image.GetDimensions((uint)selection.y,w,h,n,l);return image.Load(int4(min((uint2)(uv*float2(w,h)),uint2(w-1,h-1)),(uint)selection.x,(uint)selection.y));' if operation=='load' else
              b'return image.SampleLevel(sampling,float3(uv,selection.x),selection.y);')
        source=b'cbuffer P:register(b0){float4 selection;} Texture2DArray<float4> image:register(t1);SamplerState sampling:register(s0);float4 main(float4 p:SV_Position,float2 uv:TEXCOORD3):SV_Target{'+body+b'}'
        ps=compiler.compile(source,b'main',b'ps_5_0')
        stages=[{'stage':'vertex','code':vs,'constant_buffers':[{'slot':0,'size':64}],'images':[],'samplers':[]},
                {'stage':'fragment','code':ps,'constant_buffers':[{'slot':0,'size':16}],
                 'images':[{'slot':1,'type':4}],'samplers':[{'slot':0}] if operation=='sample' else []}]
        shaders[operation]=asset(operation,shader.encode(stages,state=state,draw_list='auxgeom'),'.foashader')+'.azshader'
    baseline_draw=json.loads((baseline/'synthetic.json').read_text())
    def draw(name,value):
        path=root/(name+'.json')
        with path.open('x') as output:json.dump(value,output,indent=2)
        cases.append({'name':name,'descriptor':path.name})
    for name in ('synthetic','source_unlit'):
        draw(name,json.loads((baseline/(name+'.json')).read_text()))
    # Compute independent reference surfaces directly by coordinates, not by
    # slicing or decoding the producer's flattened array packet.
    def surface(layer,mip):
        width=max(1,4>>mip);data=bytearray()
        for y in range(width):
            for x in range(width):
                data.extend(((23+x*67+layer*71+mip*11)%256,
                             (37+y*73+layer*17+mip*61)%256,
                             (53+x*29+y*31+layer*43+mip*47)%256,255))
        return bytes(data)
    for layers in (1,3):
        payload=b''.join(surface(layer,mip) for mip in range(3) for layer in range(layers))
        array=asset(f'array{layers}',encode(TextureArray(4,4,3,4,0,payload,layers)),'.foatexture')+'.streamingimage'
        for layer in range(layers):
            for mip in range(3):
                label=f'l{layers}-slice{layer}-mip{mip}'
                width=max(1,4>>mip)
                reference=asset(label,encode(Texture(width,width,1,4,0,surface(layer,mip))),'.foatexture')+'.streamingimage'
                value=copy.deepcopy(baseline_draw);value['stages'][1]['images'][0]['asset']=reference
                draw(label+'-reference',value)
                for operation in ('load','sample'):
                    value=copy.deepcopy(baseline_draw);value['shader']=shaders[operation]
                    value['stages'][1]['images'][0]['asset']=array
                    value['stages'][1]['constants'][0]['hex']=struct.pack('<4f',layer,mip,0,0).hex()
                    if operation=='load':value['stages'][1]['samplers']=[]
                    draw(label+'-'+operation,value)
                    pairs.append({'actual':label+'-'+operation,'reference':label+'-reference','layer':layer,'mip':mip,'layers':layers})
    result={'project':str(project),'cases':cases,'array_pairs':pairs,'assets':assets,
            'compiler_sha256':hashlib.sha256(compiler.path.read_bytes()).hexdigest(),
            'scope':'Synthetic one/three-layer arrays, three mips, Load and point SampleLevel. No game inputs inferred.'}
    with (root/'draw-fixtures-v1.json').open('x') as output:json.dump(result,output,indent=2)
    print(json.dumps({'prepared':len(cases),'pairs':len(pairs),'assets':len(assets)}))


def verify(root,output):
    from PIL import Image,ImageChops
    root=private(root);fixture=json.loads((root/'draw-fixtures-v1.json').read_text())
    report=json.loads((root/'native-editor-v1.json').read_text())
    assert report.get('native_draw')=='PASSED',report.get('error',report.get('stage'))
    assert report.get('finite_sampler_limits')=='PASSED'
    assert len(report['rejections'])==15 and all(r['status']=='PASSED' for r in report['rejections'])
    captures=report['captures'];expected={c['name'] for c in fixture['cases']}|{'source_unlit_recreated','cleared'}
    assert len(captures)==len(expected) and {c['name'] for c in captures}==expected
    images={}
    for row in captures:
        path=Path(row['path']);assert hashlib.sha256(path.read_bytes()).hexdigest()==row['sha256']
        images[row['name']]=Image.open(path).convert('RGB')
        assert 400<=images[row['name']].width<=8192 and 200<=images[row['name']].height<=8192
    extent=images['synthetic'].size
    assert all(im.size==extent for im in images.values())
    # FrameCapture returns the window attachment; record its actual extent
    # instead of assuming the requested logical viewport controls that surface.
    pixels=0
    for name in ('synthetic','source_unlit','source_unlit_recreated'):
        im=images[name]
        assert im.tobytes()==images['synthetic'].tobytes()
        for (x,y),color in zip(((.3,.3),(.7,.3),(.3,.7),(.7,.7)),((255,0,0),(0,255,0),(0,0,255),(255,255,0))):
            assert im.getpixel((round(x*im.width),round(y*im.height)))==color;pixels+=1
    results=[]
    for pair in fixture['array_pairs']:
        actual,reference=images[pair['actual']],images[pair['reference']]
        assert ImageChops.difference(actual,reference).getbbox() is None,pair
        width=max(1,4>>pair['mip']);layer=pair['layer'];mip=pair['mip']
        for sx in (.2,.4,.6,.8):
            for sy in (.2,.4,.6,.8):
                pos=(round(sx*actual.width),round(sy*actual.height))
                x,y=int((sx-.1)/.8*width),int((sy-.1)/.8*width)
                color=((23+x*67+layer*71+mip*11)%256,
                       (37+y*73+layer*17+mip*61)%256,
                       (53+x*29+y*31+layer*43+mip*47)%256)
                assert actual.getpixel(pos)==reference.getpixel(pos)==color,(pair,pos,color,actual.getpixel(pos))
                pixels+=2
        results.append({**pair,'status':'PASSED','full_color_buffer_equal':True})
    assert len(results)==24
    controls=[]
    for name,a,b in [('wrong-layer',images['l3-slice0-mip0-load'],images['l3-slice1-mip0-load']),
                     ('wrong-mip',images['l3-slice0-mip0-load'],images['l3-slice0-mip1-load']),
                     ('flipped-uv',images['l3-slice0-mip0-load'].transpose(Image.Transpose.FLIP_TOP_BOTTOM),images['l3-slice0-mip0-reference']),
                     ('missing-draw',images['l1-slice0-mip0-load'],images['cleared'])]:
        assert ImageChops.difference(a,b).getbbox() is not None,name
        controls.append({'name':name,'status':'PASSED'})
    result={'status':'PASSED','scope':fixture['scope'],'pairs':results,'captures':captures,
            'binding_rejections':report['rejections'],'negative_controls':controls,
            'capture_extent':extent,'sampled_pixels':pixels,
            'fixture_sha256':hashlib.sha256((root/'draw-fixtures-v1.json').read_bytes()).hexdigest(),
            'game_material_rendering':'NOT_RUN','full_scene_rendering':'NOT_RUN'}
    with output.open('x') as stream:json.dump(result,stream,indent=2)
    print(json.dumps({'status':'PASSED','pairs':len(results),'captures':len(captures),'negative_controls':len(controls),'sampled_pixels':pixels,'capture_extent':extent}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=('prepare','verify'))
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--project',type=Path)
    parser.add_argument('--baseline',type=Path);parser.add_argument('--output',type=Path);args=parser.parse_args()
    if args.action=='prepare':
        if args.project is None or args.baseline is None:parser.error('prepare requires --project and --baseline')
        prepare(args.root,args.project,args.baseline)
    else:
        if args.output is None:parser.error('verify requires --output')
        verify(args.root,args.output)
