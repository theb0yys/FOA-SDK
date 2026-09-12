# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
from copy import deepcopy
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import foa_scene_shader as s


def envelope(kind=16, keywords=()):
    # Synthetic structural DXBC, deliberately not a compiled/native-valid shader.
    payload=struct.pack('<2I',((1 if kind in (15,16) else 0)<<16)|0x50,2)
    chunk=b'SHEX'+struct.pack('<I',len(payload))+payload
    dxbc=b'DXBC'+bytes(16)+struct.pack('<4I',1,36+len(chunk),1,36)+chunk
    data=struct.pack('<7I',202012090,kind,0,0,0,0,len(keywords))
    for kw in keywords:
        b=kw.encode();data+=struct.pack('<I',len(b))+b;data+=bytes((-len(data))%4)
    return data+struct.pack('<I',len(dxbc))+dxbc


def packed(segmented=True):
    import lz4.block
    code=envelope();parameter=b'opaque parameter'
    header=struct.pack('<I',2)
    if segmented:
        raw=[header+struct.pack('<6I',0,len(parameter),1,len(parameter),len(code),1),parameter+code]
    else:
        raw=[header+struct.pack('<6I',28,len(parameter),0,28+len(parameter),len(code),0)+parameter+code]
    blobs=[lz4.block.compress(b,store_size=False) for b in raw]
    offsets=[];cursor=0
    for b in blobs: offsets.append(cursor);cursor+=len(b)
    return {'platforms':[4],'offsets':[offsets],'compressedLengths':[[len(x) for x in blobs]],
            'decompressedLengths':[[len(x) for x in raw]]},b''.join(blobs)


def shader_tree():
    empty={'m_SubPrograms':[],'m_PlayerSubPrograms':[],'m_ParameterBlobIndices':[]}
    stage={'m_SubPrograms':[], 'm_PlayerSubPrograms':[[],[],[],[{'m_BlobIndex':1,'m_KeywordIndices':[0],
            'm_ShaderRequirements':1,'m_GpuProgramType':16}]],'m_ParameterBlobIndices':[[],[],[],[0]]}
    pa={name:deepcopy(empty) for name in s.STAGES};pa['progVertex']=stage;pa['m_NameIndices']=[['value',3]]
    return {'m_ParsedForm':{'m_KeywordNames':['OPTION'],'m_KeywordFlags':[1],'m_SubShaders':[{'m_Passes':[pa]}]}}


class ShaderTests(unittest.TestCase):
    def test_all_segments_and_single_segment_partition(self):
        for segmented in (False,True):
            t,b=packed(segmented);entries=s.unpack_entries(t,b)
            self.assertEqual(entries[0].data,b'opaque parameter')
            self.assertEqual(entries[1].data,envelope())

    def test_compressed_lengths_offsets_and_platform_reject(self):
        for mutate in (lambda t:t.update(platforms=[18]),lambda t:t['offsets'][0].__setitem__(1,0),
                       lambda t:t['decompressedLengths'][0].__setitem__(1,129*1024*1024),
                       lambda t:t['compressedLengths'][0].pop()):
            t,b=packed();mutate(t)
            with self.assertRaises(s.h.HeightmapImportError): s.unpack_entries(t,b)
        t,b=packed()
        for data in (b[:-1],b+b'x'):
            with self.assertRaises(s.h.HeightmapImportError): s.unpack_entries(t,data)

    def test_partition_gaps_overlap_and_bad_segment_reject(self):
        import lz4.block
        for field,value in ((4,1),(12,3),(16,0)):
            t,b=packed(False);raw=bytearray(lz4.block.decompress(b,uncompressed_size=t['decompressedLengths'][0][0]));struct.pack_into('<I',raw,field,value)
            b=lz4.block.compress(bytes(raw),store_size=False);t['compressedLengths']=[[len(b)]]
            with self.assertRaises(s.h.HeightmapImportError): s.unpack_entries(t,b)

    def test_variant_joins_exact_parameter_and_code_indices(self):
        t,b=packed();entries=s.unpack_entries(t,b);program=s.player_programs(shader_tree(),entries)[0]
        self.assertEqual((program['code_entry'],program['parameter_entry'],program['tier']),(1,0,3))
        self.assertEqual(program['keyword_indices'],(0,))

    def test_bad_variant_reference_keywords_and_stage_reject(self):
        t,b=packed();entries=s.unpack_entries(t,b)
        for field,value in (('m_BlobIndex',2),('m_BlobIndex',0),('m_GpuProgramType',18),('m_KeywordIndices',[1]),('m_KeywordIndices',[0,0])):
            tree=shader_tree();sp=tree['m_ParsedForm']['m_SubShaders'][0]['m_Passes'][0]['progVertex']['m_PlayerSubPrograms'][3][0];sp[field]=value
            with self.assertRaises(s.h.HeightmapImportError): s.player_programs(tree,entries)

    def test_parameter_count_mismatch_and_duplicate_names_reject(self):
        t,b=packed();entries=s.unpack_entries(t,b)
        tree=shader_tree();pa=tree['m_ParsedForm']['m_SubShaders'][0]['m_Passes'][0];pa['progVertex']['m_ParameterBlobIndices'][3]=[]
        with self.assertRaises(s.h.HeightmapImportError): s.player_programs(tree,entries)
        tree=shader_tree();tree['m_ParsedForm']['m_SubShaders'][0]['m_Passes'][0]['m_NameIndices'].append(['other',3])
        with self.assertRaises(s.h.HeightmapImportError): s.player_programs(tree,entries)

    def test_dxbc_stage_and_envelope_keywords(self):
        for kind,stage in ((15,'vertex'),(16,'vertex'),(17,'fragment'),(18,'fragment')):
            blob,meta=s.dxbc_container(s.ShaderEntry(0,0,envelope(kind,('OPTION','SECOND'))))
            self.assertTrue(blob.startswith(b'DXBC'))
            self.assertEqual(meta['stage'],stage)
            self.assertEqual(meta['envelope_keywords'],('OPTION','SECOND'))

    def test_dxbc_future_version_truncation_and_corrupt_chunk_reject(self):
        source=envelope()
        bad=[source[:4]+struct.pack('<I',99)+source[8:],source[:-1],source[:28]+struct.pack('<I',999999)+source[32:]]
        for offset,value in ((32+20,2),(32+24,9999),(32+32,0),(32+36+4,999999),(32+44,0x50)):
            b=bytearray(source);struct.pack_into('<I',b,offset,value);bad.append(bytes(b))
        for b in bad:
            with self.assertRaises(s.h.HeightmapImportError): s.dxbc_container(s.ShaderEntry(0,0,b))

    def test_cancelled_before_decompression(self):
        t,b=packed()
        with self.assertRaises(s.h.HeightmapImportError): s.unpack_entries(t,b,lambda:True)



def ps(value):
    b=value.encode('utf-8');return struct.pack('<I',len(b))+b+bytes((-len(b))%4)

def param_value(name='tint'):
    return ps(name)+struct.pack('<6I',0,1,4,0,0,0)

def parameter_fixture():
    root=ps('')+struct.pack('<3I',0,0,0)
    group=ps('Constants')+struct.pack('<2I',64,1)+param_value()
    group+=struct.pack('<I',1)+ps('layers')+struct.pack('<4I',16,2,16,1)+param_value('strength')
    resources=(ps('_Image')+struct.pack('<4I',0,3,2,4)+ps('Constants')+struct.pack('<3I',1,0,1)
               +ps('')+struct.pack('<3I',4,4,0x155))
    return struct.pack('<2I',202012090,2)+root+group+struct.pack('<I',3)+resources


def with_common():
    tree=shader_tree();pa=tree['m_ParsedForm']['m_SubShaders'][0]['m_Passes'][0]
    common={key:[] for key in ('m_VectorParams','m_MatrixParams','m_TextureParams','m_BufferParams',
                              'm_ConstantBuffers','m_ConstantBufferBindings','m_UAVParams','m_Samplers')}
    pa['progVertex']['m_CommonParameters']=common
    pa['m_NameIndices']=[['_Image',0],['Constants',1]]
    return tree,common


class ParameterTests(unittest.TestCase):
    def test_complete_parameter_record_and_structures(self):
        result=s.parameter_record(s.ShaderEntry(0,0,parameter_fixture()))
        self.assertEqual(len(result['groups']),2)
        group=result['groups'][1]
        self.assertEqual(group['name'],'Constants')
        self.assertEqual(group['structures'][0]['array_size'],2)
        self.assertEqual(group['structures'][0]['parameters'][0]['name'],'strength')
        image=result['resources'][0]
        self.assertEqual((image['dimension'],image['sampler_index']),(2,2))
        self.assertEqual(result['resources'][2]['extra'],0x155)

    def test_parameter_record_truncated_trailing_or_future_rejects(self):
        raw=parameter_fixture()
        for blob in (raw[:-1],raw+b'\0',struct.pack('<I',0)+raw[4:],raw[:12]):
            with self.assertRaises(s.h.HeightmapImportError): s.parameter_record(s.ShaderEntry(0,0,blob))

    def test_parameter_limits_and_utf8_padding_reject(self):
        for data in (struct.pack('<I',4097),struct.pack('<I',1025),struct.pack('<I',1)+b'\xff\0\0\0',struct.pack('<I',1)+b'xY\0\0'):
            with self.assertRaises(s.h.HeightmapImportError): s.ParameterReader(data).string()
        raw=parameter_fixture();bad=bytearray(raw);struct.pack_into('<I',bad,4,257)
        with self.assertRaises(s.h.HeightmapImportError): s.parameter_record(s.ShaderEntry(0,0,bytes(bad)))

    def test_duplicate_parameters_and_unknown_resource_kind_reject(self):
        raw=struct.pack('<2I',202012090,1)+ps('')+struct.pack('<2I',32,2)+param_value()+param_value()+struct.pack('<2I',0,0)
        with self.assertRaises(s.h.HeightmapImportError): s.parameter_record(s.ShaderEntry(0,0,raw))
        raw=struct.pack('<2I',202012090,1)+ps('')+struct.pack('<4I',0,0,0,1)+ps('bad')+struct.pack('<3I',5,0,0)
        with self.assertRaises(s.h.HeightmapImportError): s.parameter_record(s.ShaderEntry(0,0,raw))

    def test_common_and_variant_resources_include_texture_sampler_owner(self):
        tree,common=with_common();common['m_TextureParams']=[{'m_NameIndex':0,'m_Index':3,'m_SamplerIndex':2,'m_MultiSampled':False,'m_Dim':2}]
        params=s.parameter_record(s.ShaderEntry(0,0,parameter_fixture()))
        result=s.variant_resources(tree,{'subshader':0,'pass':0,'stage':'progVertex'},params)
        byreg={(x['namespace'],x['index']):x for x in result}
        self.assertEqual(byreg[('s',2)]['signature'],('texture-sampler','_Image'))
        self.assertEqual(len(byreg[('t',3)]['declarations']),2)
        self.assertTrue(byreg[('s',4)]['inline_sampler']['comparison'])

    def test_conflicting_texture_or_inline_sampler_owner_rejects(self):
        for inline in (False,True):
            tree,common=with_common()
            if inline: common['m_Samplers']=[{'sampler':1,'bindPoint':2}]
            else: common['m_TextureParams']=[{'m_NameIndex':0,'m_Index':3,'m_SamplerIndex':5,'m_MultiSampled':False,'m_Dim':2}]
            params=s.parameter_record(s.ShaderEntry(0,0,parameter_fixture()))
            with self.assertRaises(s.h.HeightmapImportError): s.variant_resources(tree,{'subshader':0,'pass':0,'stage':'progVertex'},params)

    def test_unknown_inline_bits_remain_visible_in_bound_resources(self):
        tree,common=with_common();common['m_Samplers']=[{'sampler':0x1001,'bindPoint':7}]
        result=s.variant_resources(tree,{'subshader':0,'pass':0,'stage':'progVertex'},{'resources':[]})
        self.assertEqual(result[0]['inline_sampler']['unresolved_bits'],0x1000)
        self.assertEqual(result[0]['inline_sampler']['encoding_status'],'PARTIAL')

    def test_absent_common_name_rejects(self):
        tree,common=with_common();common['m_BufferParams']=[{'m_NameIndex':100,'m_Index':0}]
        with self.assertRaises(s.h.HeightmapImportError): s.variant_resources(tree,{'subshader':0,'pass':0,'stage':'progVertex'},{'resources':[]})

if __name__ == '__main__': unittest.main()
