# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Prepare/verify synthetic source-image cases in an explicitly selected private AP project."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from foa_scene_texture import Texture,TextureArray,encode,mip_layout
from foa_scene_native_texture_audit import NativeCache,audit_texture,field,descendant


def prepare(root):
    target=root/'Project/Assets/texture_cases'
    target.mkdir(parents=True,exist_ok=False)
    records=[]
    for fmt in (1,3,4,10,12,26):
        for color in (0,1):
            if fmt==26 and color: continue
            for width,height,count in ((4,4,3),(512,256,10)):
                mips=mip_layout(width,height,count,fmt)
                # Distinct rows, blocks and every mip. Arbitrary BC blocks are valid GPU encodings.
                payload=b''.join(bytes((i*7+n*29)%256 for i in range(m.size)) for n,m in enumerate(mips))
                name=f'f{fmt}_c{color}_{width}'
                blob=encode(Texture(width,height,count,fmt,color,payload))
                (target/(name+'.foatexture')).write_bytes(blob)
                records.append({'source_name':'assets/texture_cases/'+name+'.foatexture','valid':True,'sha256':hashlib.sha256(blob).hexdigest()})
    baseline=encode(Texture(4,4,3,4,0,bytes(range(84))))
    bad={'future_version':baseline[:8]+struct.pack('<I',3)+baseline[12:], 'bad_hash':baseline[:-1]+b'x', 'truncated':baseline[:-3], 'extra_bytes':baseline+b'x'}
    for name,(offset,value) in {'bad_format':(24,27),'zero_width':(12,0),'too_many_mips':(20,15),'bad_colour':(28,2),'oversize':(12,8193)}.items():
        part=bytearray(baseline);struct.pack_into('<I',part,offset,value);part[32:64]=hashlib.sha256(part[:32]+part[64:]).digest();bad[name]=bytes(part)
    for name,blob in bad.items():
        (target/(name+'.foatexture')).write_bytes(blob)
        records.append({'source_name':'assets/texture_cases/'+name+'.foatexture','valid':False,'sha256':hashlib.sha256(blob).hexdigest()})
    with (root/'texture-cases.json').open('x') as output: json.dump({'cases':records},output,indent=2)
    print(json.dumps({'prepared':len(records),'positive':sum(r['valid'] for r in records)}))


def prepare_arrays(root):
    target=root/'Project/Assets/texture_array_cases'
    target.mkdir(parents=True,exist_ok=False)
    records=[]
    def write(name,blob,valid):
        path=target/(name+'.foatexture');path.write_bytes(blob)
        records.append({'source_name':'assets/texture_array_cases/'+path.name,'valid':valid,
                        'sha256':hashlib.sha256(blob).hexdigest()})
    for fmt in (1,3,4,10,12,26):
        for color in (0,1):
            if fmt==26 and color:continue
            for layers,width,height,count in ((1,9,5,4),(3,9,5,4),(3,256,128,9)):
                mips=mip_layout(width,height,count,fmt)
                payload=b''.join(bytes((i*7+n*29+layer*67)%256 for i in range(m.size))
                                 for n,m in enumerate(mips) for layer in range(layers))
                write(f'f{fmt}_c{color}_l{layers}_{width}',
                      encode(TextureArray(width,height,count,fmt,color,payload,layers)),True)
    baseline=encode(TextureArray(4,4,3,4,0,bytes(range(252)),3))
    write('legacy_2d',encode(Texture(4,4,3,4,0,bytes(range(84)))),True)
    bad={'bad_hash':baseline[:-1]+b'x','truncated':baseline[:-1],
         'short_header':baseline[:64],'trailing':baseline+b'x'}
    for name,offset,value in [('future',8,3),('zero_layers',32,0),('too_many_layers',32,257),
                             ('wrong_layers',32,2),('too_many_mips',20,15),('bad_format',24,27),
                             ('bad_color',28,2),('oversized',12,8193)]:
        changed=bytearray(baseline);struct.pack_into('<I',changed,offset,value)
        changed[36:68]=hashlib.sha256(changed[:36]+changed[68:]).digest();bad[name]=bytes(changed)
    for name,blob in bad.items():write(name,blob,False)
    with (root/'texture-array-cases.json').open('x') as output:json.dump({'cases':records},output,indent=2)
    print(json.dumps({'prepared':len(records),'positive':sum(r['valid'] for r in records)}))


def verify(root, output_path=None, arrays=False):
    report=[];cache=NativeCache(root/'Project/Cache/pc')
    try:
        for case in json.loads((root/('texture-array-cases.json' if arrays else 'texture-cases.json')).read_text())['cases']:
            result={**case,'status':'FAILED'}
            try:
                blob=(root/'Project'/case['source_name']).read_bytes()
                assert hashlib.sha256(blob).hexdigest()==case['sha256']
                if case['valid']: result.update(audit_texture(cache,case['source_name'],blob))
                else:
                    rows=cache.db.execute('SELECT j.Status,(SELECT COUNT(*) FROM Products p WHERE p.JobPK=j.JobID) FROM Jobs j JOIN Sources s ON j.SourcePK=s.SourceID WHERE s.SourceName=? AND j.JobKey=? AND j.Platform=?',(case['source_name'],'FOA Source Texture','pc')).fetchall()
                    assert rows==[(2,0)],repr(rows)
                    result['status']='PASSED'
            except Exception as error: result['error']=str(error)
            report.append(result)
        auditor_checks=[]
        valid=next(r for r in report if r['valid'])
        packet=(root/'Project'/valid['source_name']).read_bytes()
        original_read=cache.read
        def mutate_field(node,name):
            value=descendant(node,name)
            value['value']=bytes([value['value'][0]^1])+value['value'][1:]
        for mutation in ('Format','m_imageData','m_mipCount','m_totalImageDataSize','MipSliceMin','IsArray','ArraySize','ArraySliceMin'):
            def corrupt(path):
                node=copy.deepcopy(original_read(path));mutate_field(node,mutation);return node
            cache.read=corrupt
            caught=False
            try: audit_texture(cache,valid['source_name'],packet)
            except Exception: caught=True
            finally: cache.read=original_read
            auditor_checks.append({'mutation':mutation,'status':'PASSED' if caught else 'FAILED'})
        result={'status':'PASSED' if all(r['status']=='PASSED' for r in report+auditor_checks) else 'FAILED','cases':report,'auditor_checks':auditor_checks,'native_products':cache.products}
        with (output_path or root/'texture-cases-acceptance.json').open('x') as output: json.dump(result,output,indent=2)
        print(json.dumps({'status':result['status'],'cases':len(report),'failures':[{'source':r['source_name'],'error':r.get('error')} for r in report if r['status']!='PASSED']}))
        if result['status']!='PASSED': raise SystemExit(1)
    finally:cache.close()

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('action',choices=('prepare','prepare-arrays','verify','verify-arrays'));p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path);args=p.parse_args()
    root=args.root.resolve(strict=True)
    assert (root/'Project/project.json').is_file()
    assert not (root/'.git').exists() and not (root.parent/'.git').exists()
    if args.action=='prepare':prepare(root)
    elif args.action=='prepare-arrays':prepare_arrays(root)
    else:verify(root,args.output,args.action=='verify-arrays')
