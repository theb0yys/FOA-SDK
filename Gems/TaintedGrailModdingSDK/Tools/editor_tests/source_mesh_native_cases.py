# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic acceptance cases for the compiled source-preservation adapter.

Prepare in a disposable O3DE project, process with the pinned Asset Processor,
then verify. Neither command starts the host or touches a game installation.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_scene_mesh import SourceMesh
from foa_scene_mesh_glb import encode_glb
from foa_scene_native_mesh_audit import NativeCache, audit_projection, glb_accessors, require


def fixture():
    return SourceMesh({0:[(1.,2.,3.),(4.,5.,6.),(2.,8.,10.)],
        1:[(0.,2.,0.,.25)]*3,2:[(3.,0.,0.,-1.)]*3,3:[(0.,.25,.5,1.)]*3,
        4:[(.125,.25),(.75,.875),(1.,0.)],8:[(.1,.2),(.3,.4),(.5,.6)]},
        [[(0,1,2)],[(2,1,0)]],{'serialized_file':'synthetic','path_id':'-9007199254740993','record_sha256':'1'*64},0)


def prepare(root):
    root=Path(root).resolve();folder=root/'Project/Assets/preservation_cases'
    folder.mkdir(parents=True,exist_ok=True)
    cases={name:fixture() for name in ('full_channels','parallel_frame','no_tangents','close_vertices')}
    cases['parallel_frame'].attributes[2]=[(0.,1.,0.,-1.)]*3
    cases['no_tangents'].attributes.pop(2);cases['no_tangents'].attributes[4]=[(0.,0.)]*3
    cases['close_vertices'].attributes[0]=[(0.,0.,0.),(.00001,0.,0.),(0.,0.,.00001)]
    blobs={name:encode_glb(mesh)[0] for name,mesh in cases.items()}
    original=blobs['full_channels'];doc,_=glb_accessors(original)
    json_size=struct.unpack_from('<I',original,12)[0];data=original[28+json_size:]
    for name in ('reject_missing_marker','reject_unknown_schema','reject_root_offset'):
        changed=copy.deepcopy(doc)
        if name=='reject_missing_marker':changed['asset'].pop('extras')
        elif name=='reject_unknown_schema':changed['asset']['extras']['foaSourceMeshProjection']=2
        else:changed['nodes'][0]['matrix'][12]=10
        raw=json.dumps(changed,separators=(',',':')).encode();raw+=b' '*((-len(raw))%4)
        blobs[name]=(struct.pack('<III',0x46546c67,2,28+len(raw)+len(data))+
            struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(data),0x004e4942)+data)
    rows=[]
    for name,blob in blobs.items():
        path=folder/(name+'_foamesh.glb')
        if path.exists():require(path.read_bytes()==blob,'An existing native fixture differs.')
        else:
            with path.open('xb') as output:output.write(blob)
        rows.append({'source_name':path.relative_to(root/'Project').as_posix(),
            'sha256':hashlib.sha256(blob).hexdigest(),'expect':'FAILED' if name.startswith('reject_') else 'PASSED'})
    with (root/'source-preservation-cases.json').open('x') as output:json.dump(rows,output,indent=2)
    return {'prepared':len(rows)}


def verify(root):
    root=Path(root).resolve();cases=json.loads((root/'source-preservation-cases.json').read_text())
    require(len(cases)==7,'Expected all seven native cases.')
    cache=NativeCache(root/'Project/Cache/pc');results=[]
    try:
        for case in cases:
            path=(root/'Project'/case['source_name']).resolve()
            require(path.is_relative_to(root/'Project/Assets/preservation_cases'),'Fixture escaped its source directory.')
            blob=path.read_bytes();require(hashlib.sha256(blob).hexdigest()==case['sha256'],'Native fixture changed.')
            jobs=cache.db.execute('SELECT j.Status,j.JobID FROM Jobs j JOIN Sources s ON j.SourcePK=s.SourceID WHERE s.SourceName=? AND j.Platform=? AND j.JobKey=?',
                (case['source_name'],'pc','Scene compilation')).fetchmany(2)
            require(len(jobs)==1,'Native fixture has no unique compilation job.')
            if case['expect']=='FAILED':
                require(jobs[0][0]==2,'Invalid projection was not rejected by the compiled native builder.')
                outputs=cache.db.execute('SELECT count(*) FROM Products WHERE JobPK=?',(jobs[0][1],)).fetchone()[0]
                require(outputs==0,'Rejected projection left published native products.')
                result={'expected_rejection':'PASSED'}
            else:
                require(jobs[0][0]==4,'Valid source projection failed native processing.')
                result=audit_projection(cache,case['source_name'],blob)
            results.append({'source_name':case['source_name'],**result})
    finally:cache.close()
    report={'status':'PASSED','cases':results,'products':cache.products,'game_export':'NOT_RUN'}
    with (root/'source-preservation-acceptance.json').open('x') as output:json.dump(report,output,indent=2)
    return {'status':'PASSED','positive_cases':4,'negative_cases':3,'game_export':'NOT_RUN'}


def main():
    parser=argparse.ArgumentParser();parser.add_argument('command',choices=('prepare','verify'));parser.add_argument('--root',type=Path,required=True)
    args=parser.parse_args();print(json.dumps((prepare if args.command=='prepare' else verify)(args.root)))

if __name__=='__main__':main()
