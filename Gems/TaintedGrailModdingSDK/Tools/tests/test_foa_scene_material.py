# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
from copy import deepcopy
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import foa_scene_material as m

FILE = 'cab-'+'1'*32
OTHER = 'cab-'+'2'*32

def asset(name=FILE, path_id=7):
    return SimpleNamespace(name=name,path_id=path_id,externals=[SimpleNamespace(path='archive:/'+OTHER+'/'+OTHER)])

def pointer(path=7, file=0): return {'m_FileID':file,'m_PathID':path}

def sampler(): return {'m_FilterMode':1,'m_Aniso':8,'m_MipBias':-1.0,'m_WrapU':0,'m_WrapV':1,'m_WrapW':2}

def environment(path=9):
    return {'m_Texture':pointer(path), 'm_Scale':{'x':2.,'y':-3.},'m_Offset':{'x':.25,'y':.75}}

def prop(name, kind, flags=0):
    return {'m_Name':name,'m_Type':kind,'m_Flags':flags, **{'m_DefValue['+str(i)+']':v for i,v in enumerate((.5,0.,1.,0.))},
            'm_DefTexture':{'m_DefaultName':'white','m_TexDim':2}}

def fixtures():
    material={'m_Shader':pointer(), 'm_Name':'Display label', 'm_ValidKeywords':['OPTION'],
              'm_InvalidKeywords':['OLD'], 'm_CustomRenderQueue':3100, 'disabledShaderPasses':['Shadow'],
              'm_SavedProperties':{'m_TexEnvs':[['_Tex',environment()]],'m_Ints':[],
                                   'm_Floats':[['_Rough',.2],['_Old',8.]],'m_Colors':[]}}
    shader={'m_ParsedForm':{'m_PropInfo':{'m_Props':[prop('_Tex',4),prop('_Rough',3),prop('_Default',2)]}},
            'm_NonModifiableTextures':[]}
    return material,shader

def bind(material=None, shader=None, sa=None, lookup=None):
    ma, sh=fixtures()
    return m.bind_material(asset(),material or ma,sa or asset(),shader or sh,
                           lookup or (lambda key:{'m_TextureDimension':2,'m_TextureSettings':sampler()}))


class MaterialTests(unittest.TestCase):
    def test_saved_and_default_values_preserve_source(self):
        ma,sh=fixtures();before=deepcopy(ma);b=bind(ma,sh)
        self.assertEqual(b['source'],ma)
        self.assertEqual(b['properties']['_Rough']['value'],.2)
        self.assertEqual(b['properties']['_Default']['value'],.5)
        self.assertEqual(b['undeclared_saved_properties']['m_Floats'],['_Old'])
        self.assertEqual(b['properties']['_Tex']['texture']['reference'],(FILE,9))
        b['source']['m_ValidKeywords'].clear()
        self.assertEqual(ma,before)
        self.assertEqual(b['native_shader_mapping'],'NOT_RUN')

    def test_same_label_different_shader_identity_rejects(self):
        with self.assertRaises(m.h.HeightmapImportError): bind(sa=asset(OTHER))
        with self.assertRaises(m.h.HeightmapImportError): bind(sa=asset(path_id=8))

    def test_signed_64_bit_external_texture_reference(self):
        ma,sh=fixtures();ma['m_SavedProperties']['m_TexEnvs'][0][1]['m_Texture']=pointer(-(1<<63),1)
        b=bind(ma,sh)
        self.assertEqual(b['properties']['_Tex']['texture']['reference'],(OTHER,-(1<<63)))

    def test_null_default_is_not_an_invented_image(self):
        ma,sh=fixtures();ma['m_SavedProperties']['m_TexEnvs'][0][1]=environment(0)
        b=bind(ma,sh,lookup=lambda key:self.fail('Null texture must not trigger a lookup'))
        tex=b['properties']['_Tex']['texture']
        self.assertIsNone(tex['reference'])
        self.assertEqual(tex['default']['m_DefaultName'],'white')
        self.assertEqual(tex['value_origin'],'shader-default-unresolved')
        self.assertNotIn('saved_sampler',tex)

    def test_shader_owned_texture_resolves_in_shader_file(self):
        ma,sh=fixtures();sh['m_ParsedForm']['m_PropInfo']['m_Props'][0]['m_Flags']=64
        sh['m_NonModifiableTextures']=[['_Tex',pointer(12,1)]]
        tex=bind(ma,sh)['properties']['_Tex']['texture']
        self.assertEqual(tex['reference'],(OTHER,12))
        self.assertEqual(tex['saved_reference'],(FILE,9))
        self.assertEqual(tex['value_origin'],'shader-owned')

    def test_shader_owned_texture_requires_declaration_and_flag(self):
        for name in ('_Tex','_Absent'):
            ma,sh=fixtures();sh['m_NonModifiableTextures']=[[name,pointer(12)]]
            with self.assertRaises(m.h.HeightmapImportError): bind(ma,sh)

    def test_duplicate_material_or_shader_property_rejects(self):
        ma,sh=fixtures();ma['m_SavedProperties']['m_Floats'].append(['_Rough',.8])
        with self.assertRaises(m.h.HeightmapImportError): bind(ma,sh)
        ma,sh=fixtures();sh['m_ParsedForm']['m_PropInfo']['m_Props'].append(prop('_Rough',2))
        with self.assertRaises(m.h.HeightmapImportError): bind(ma,sh)

    def test_stale_different_type_does_not_shadow_declared_property(self):
        ma,sh=fixtures();ma['m_SavedProperties']['m_Colors'].append(['_Rough',dict.fromkeys('rgba',1.)])
        b=bind(ma,sh)
        self.assertEqual(b['properties']['_Rough']['value'],.2)
        self.assertEqual(b['undeclared_saved_properties']['m_Colors'],['_Rough'])

    def test_unknown_schema_or_property_type_rejects(self):
        ma,sh=fixtures();ma['m_SavedProperties']['new_table']=[]
        with self.assertRaises(m.h.HeightmapImportError): bind(ma,sh)
        for kind in (True,-1,5,99):
            ma,sh=fixtures();sh['m_ParsedForm']['m_PropInfo']['m_Props'][0]['m_Type']=kind
            with self.assertRaises(m.h.HeightmapImportError): bind(ma,sh)

    def test_texture_dimension_and_missing_target_reject(self):
        with self.assertRaises(m.h.HeightmapImportError):
            bind(lookup=lambda key:{'m_TextureDimension':5,'m_TextureSettings':sampler()})
        with self.assertRaises(KeyError): bind(lookup={ }.__getitem__)

    def test_reference_shape_and_bounds(self):
        for ptr in ({'m_FileID':True,'m_PathID':9},pointer(9,2),pointer(1<<63),{'m_FileID':0,'m_PathID':0,'extra':0}):
            ma,sh=fixtures();ma['m_SavedProperties']['m_TexEnvs'][0][1]['m_Texture']=ptr
            with self.assertRaises(m.h.HeightmapImportError): bind(ma,sh)

    def test_no_scale_offset_keeps_source_transform(self):
        ma,sh=fixtures();sh['m_ParsedForm']['m_PropInfo']['m_Props'][0]['m_Flags']=4
        tex=bind(ma,sh)['properties']['_Tex']['texture']
        self.assertEqual(tex['scale'],(2.,-3.))
        self.assertEqual(tex['offset'],(.25,.75))

    def test_nonfinite_and_bounds_reject(self):
        for v in (float('nan'),float('inf'),1e100):
            ma,sh=fixtures();ma['m_SavedProperties']['m_Floats'][0][1]=v
            with self.assertRaises(m.h.HeightmapImportError): bind(ma,sh)
        with self.assertRaises(m.h.HeightmapImportError): m.bounded_copy([0]*4097)
        cycle=[];cycle.append(cycle)
        with self.assertRaises(m.h.HeightmapImportError): m.bounded_copy(cycle)

    def test_cancelled_before_binding(self):
        ma,sh=fixtures()
        with self.assertRaises(m.h.HeightmapImportError):
            m.bind_material(asset(),ma,asset(),sh,lambda k:{},lambda:True)


class SamplingTests(unittest.TestCase):
    def test_all_filter_and_wrap_mappings(self):
        for f,filters in enumerate((('Point','Point'),('Linear','Point'),('Linear','Linear'))):
            for wrap,address in enumerate(('Wrap','Clamp','Mirror','MirrorOnce')):
                s=m.Sampler(f,0,-1.,wrap,wrap,wrap).rhi_state(effective_anisotropy=0,mip_count=10)
                self.assertEqual((s['filterMin'],s['filterMip']),filters)
                self.assertEqual([s[k] for k in ('addressU','addressV','addressW')],[address]*3)
                self.assertEqual(s['mipLodBias'],-1.)
                self.assertEqual(s['mipLodMax'],9)

    def test_anisotropy_requires_explicit_effective_policy(self):
        s=m.Sampler.from_tree(sampler())
        with self.assertRaises(TypeError): s.rhi_state(mip_count=3)
        self.assertEqual(s.rhi_state(effective_anisotropy=0,mip_count=3)['anisotropyEnable'],0)
        self.assertEqual(s.rhi_state(effective_anisotropy=16,mip_count=3)['anisotropyMax'],16)
        with self.assertRaises(m.h.HeightmapImportError): s.rhi_state(effective_anisotropy=17,mip_count=3)

    def test_sampler_missing_unknown_and_nonfinite_rejects(self):
        for key,value in (('m_FilterMode',3),('m_WrapV',4),('m_MipBias',float('nan')),('m_Aniso',True)):
            s=sampler();s[key]=value
            with self.assertRaises(m.h.HeightmapImportError): m.Sampler.from_tree(s)
        s=sampler();s['future']=0
        with self.assertRaises(m.h.HeightmapImportError): m.Sampler.from_tree(s)

    def test_uv_reconstruction_preserves_corners_tiling_offset_and_reflections(self):
        # Independent source expression over asymmetric UVs, negative tiling and
        # out-of-range coordinates. A bare image flip would fail these cases.
        for u,v in ((0,0),(0,1),(1,0),(1,1),(.25,.75),(-2.25,4.5)):
            for sx,sy,ox,oy in ((1,1,0,0),(2,-3,.25,.75),(-1.5,0,7,-.125)):
                actual=m.source_uv_from_native((u,1-v),(sx,sy),(ox,oy))
                self.assertEqual(actual,(u*sx+ox,v*sy+oy))



class InlineSamplerTests(unittest.TestCase):
    def test_inline_sampler_preserves_distinct_axes_and_comparison(self):
        result=m.inline_sampler(2 | (1<<2) | (2<<4) | (3<<6) | 0x100)
        self.assertEqual((result['filter'],result['wrap_u'],result['wrap_v'],result['wrap_w']),('Trilinear','Clamp','Mirror','MirrorOnce'))
        self.assertTrue(result['comparison'])
        self.assertEqual(result['unresolved_bits'],0)

    def test_unknown_inline_bits_are_never_silently_dropped(self):
        result=m.inline_sampler(0x1001)
        self.assertEqual(result['encoded'],0x1001)
        self.assertEqual(result['unresolved_bits'],0x1000)
        self.assertEqual(result['encoding_status'],'PARTIAL')
        for value in (3,-1,True,1<<32,0xa01,0xc01,0xe01):
            with self.assertRaises(m.h.HeightmapImportError): m.inline_sampler(value)

    def test_compiler_observed_inline_anisotropy_codes(self):
        # Independently measured fixture values, not values generated by the decoder.
        for encoded, expected in ((0x1,0),(0x201,2),(0x401,4),(0x601,8),(0x801,16)):
            result=m.inline_sampler(encoded)
            self.assertEqual(result['requested_anisotropy'],expected)
            self.assertEqual(result['unresolved_bits'],0)
            self.assertEqual(result['encoding_status'],'PASSED')
        result=m.inline_sampler(0x4e5)
        self.assertEqual((result['wrap_u'],result['wrap_v'],result['wrap_w']),('Clamp','Mirror','MirrorOnce'))
        self.assertEqual(result['requested_anisotropy'],4)

    def test_direct_sampler_construction_validates_fields(self):
        for args in ((3,0,0,0,0,0),(1,17,0,0,0,0),(1,0,float('nan'),0,0,0),(1,0,0,4,0,0)):
            with self.assertRaises(m.h.HeightmapImportError): m.Sampler(*args)

if __name__ == '__main__': unittest.main()
