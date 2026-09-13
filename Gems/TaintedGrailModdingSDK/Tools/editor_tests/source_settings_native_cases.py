# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Verify exact native settings fixtures and an optional read-only game record."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import foa_scene_render_settings as settings


def verify(root,output,source=None):
    import UnityPy
    from UnityPy.helpers.TypeTreeNode import TypeTreeNode
    root=root.resolve(strict=True);output=output.resolve()
    assert not any((p/'.git').exists() for p in (root,*root.parents,output.parent,*output.parents))
    run=json.loads((root/'settings-probe.json').read_text(encoding='utf-8-sig'))
    assert run['status']=='PASSED' and run['unityVersion']==settings.UNITY_VERSION
    assert [(c['colorSpace'],c['colorSpaceValue']) for c in run['cases']]==[('Gamma',0),('Linear',1)]
    files={};objects={}
    def load(name,path):
        assert path.stat().st_size<=8*1024*1024
        raw=path.read_bytes();files[path]=raw
        matches=[o for o in UnityPy.load(raw).objects if o.class_id==129]
        assert len(matches)==1;objects[name]=matches[0]
    for c in run['cases']:
        assert len(c['files'])==1;path=Path(c['files'][0]).resolve(strict=True)
        assert path==root/c['colorSpace']/'globalgamemanagers'
        load(c['colorSpace'],path)
    if source:load('source',source.resolve(strict=True))
    schema=objects['Gamma'];results=[]
    for name,obj in objects.items():
        result=settings.stored_color_space(obj,schema)
        if name!='source':
            assert result['stored_color_space']==name
            native=(root/name/'native.txt').read_text(errors='strict')
            assert native.count('m_ActiveColorSpace '+str(result['stored_value'])+' (int)')==1
        results.append({'case':name,**result})
    original=objects['Linear'];raw=original.get_raw_data();baseline=settings.stored_color_space(original,schema)
    def proxy(obj,**changes):
        return SimpleNamespace(**(dict(class_id=obj.class_id,type=obj.type,assets_file=obj.assets_file,
                                      serialized_type=obj.serialized_type,byte_size=obj.byte_size,
                                      get_raw_data=obj.get_raw_data)|changes))
    bad=[]
    bad.append(('wrong-class',proxy(original,class_id=47),schema))
    for key,val in [('unity_version','6000.0.65f1'),('target_platform',13),('reader',SimpleNamespace(endian='>'))]:
        file=SimpleNamespace(unity_version=original.assets_file.unity_version,target_platform=19,reader=SimpleNamespace(endian='<'))
        setattr(file,key,val);bad.append(('wrong-'+key,proxy(original,assets_file=file),schema))
    typ=copy.copy(original.serialized_type);typ.old_type_hash=bytes(16)
    bad.append(('wrong-source-type-hash',proxy(original,serialized_type=typ),schema))
    bad.append(('oversized-record',proxy(original,byte_size=settings.MAX_RECORD_BYTES+1),schema))
    bad.append(('bool-record-size',proxy(original,byte_size=True),schema))
    for name,changed in [('truncated',raw[:-1]),('trailing',raw+b'\0'),('nonzero-suffix',raw[:-1]+b'\1')]:
        bad.append((name,proxy(original,byte_size=len(changed),get_raw_data=lambda b=changed:b),schema))
    invalid=bytearray(raw);struct.pack_into('<I',invalid,baseline['field_offset'],2)
    bad.append(('unknown-color-space',proxy(original,get_raw_data=lambda:bytes(invalid)),schema))
    typ=copy.copy(schema.serialized_type);typ.node=None
    bad.append(('missing-schema',original,proxy(schema,serialized_type=typ)))
    rows=[{k:v for k,v in n.to_dict().items() if k!='m_Children'} for n in schema.serialized_type.node.traverse()]
    next(n for n in rows if n['m_Name']=='m_ActiveColorSpace')['m_Name']='unqualifiedField'
    typ=copy.copy(schema.serialized_type);typ.node=TypeTreeNode.from_list(rows)
    bad.append(('changed-schema',original,proxy(schema,serialized_type=typ)))
    reads=[]
    def changing_record():
        reads.append(1);return raw if len(reads)==1 else raw[:-1]+b'\1'
    bad.append(('record-changed-during-read',proxy(original,get_raw_data=changing_record),schema))
    checks=[]
    for name,obj,node in bad:
        try:settings.stored_color_space(obj,node)
        except (ValueError,settings.HeightmapImportError,EOFError,struct.error):checks.append({'name':name,'status':'PASSED'})
        else:raise AssertionError('Malformed settings accepted: '+name)
    for path,raw in files.items():assert path.read_bytes()==raw
    result={'status':'PASSED','scope':'Stored build color-space scalar only; four opaque suffix bytes retained. No runtime execution.',
            'cases':results,'negative_controls':checks,'source_unchanged':True,
            'files':[{'path':str(p),'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()} for p,raw in files.items()]}
    with output.open('x') as stream:json.dump(result,stream,indent=2)
    print(json.dumps({'status':'PASSED','cases':len(results),'negative_controls':len(checks),
                      'stored_source_color_space':next((r['stored_color_space'] for r in results if r['case']=='source'),None)}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--source',type=Path)
    args=parser.parse_args();verify(args.root,args.output,args.source)
