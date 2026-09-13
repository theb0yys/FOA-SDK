# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Original DOTS Unlit program fixture with explicit synthetic rendering inputs."""
import hashlib
import struct
from foa_scene_dots_buffers import PROFILE, pack_matrix_columns, pack_streams, visibility_buffers
from foa_scene_shader_constants import pack_constants

PROGRAMS = ('449e571435b0691daf73166ef381794cf1c089972832885265cb279714b3ddb3',
            '4db425f942220454b6649dabff8e609a164142eaa2795b26a0325253378d11ad')


def cases(programs, layouts, white_asset, sampler):
    """Execute the unchanged original programs; all scene inputs here are synthetic.

    One indexed draw selects one explicit source instance. This fixture does not
    qualify multi-instance draw dispatch, game culling or complete material state.
    """
    if tuple(hashlib.sha256(p['code']).hexdigest() for p in programs) != PROGRAMS:
        raise ValueError('The exact independently inspected original programs are required.')
    source = [[.25,0,0,-.55, 0,.5,0,0, 0,0,1,0, 0,0,0,1],
              [0,-.25,0,0, .5,0,0,0, 0,0,1,0, 0,0,0,1],
              [-.25,0,0,.55, 0,.25,0,.2, 0,0,1,0, 0,0,0,1]]
    matrices = [list(struct.unpack('<16I',struct.pack('<16f',*values))) for values in source]
    columns = pack_matrix_columns(matrices)
    data, metadata = pack_streams([
        dict(name='unity_ObjectToWorld',stride=48,data=columns),
        dict(name='_UnlitColor',stride=16,data=struct.pack('<12f',1,0,0,1, 0,1,0,1, 0,0,1,1)),
        dict(name='_EmissiveColor',stride=12,data=struct.pack('<9f',1,1,0, 0,1,1, 1,0,1)),
    ],3,PROFILE)
    # Independent hand-calculated axis-aligned bounds for the three test matrices.
    bounds=[[-.75,-.4,-.35,.4],[-.2,-.4,.2,.4],[.35,0,.75,.4]]
    colors=[[255,0,0],[0,255,0],[0,0,255]]
    vp=[1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.]
    vs_globals=pack_constants(layouts[0]['ShaderVariablesGlobal'],
                             {'_WorldSpaceCameraPos_Internal':[0.,0.,0.,0.],'_ViewProjMatrix':vp})
    ps_globals=pack_constants(layouts[1]['ShaderVariablesGlobal'],
                             {'_ProbeExposureScale':1.,'_OffScreenRendering':0,'_DeExposureMultiplier':1.})
    signed=lambda word:word if word<0x80000000 else word-0x100000000
    definitions=[('direct_0',0,False,False,'override'),('direct_1',1,False,False,'override'),
                 ('direct_2',2,False,False,'override'),('indirect_2',2,True,False,'override'),
                 ('indirect_0',0,True,False,'override'),('stripped_1',1,True,True,'override'),
                 ('material_fallback',2,False,False,'fallback'),('shared_transform',2,True,False,'shared'),
                 ('emissive_override',1,True,False,'emissive')]
    for name,index,indirect,strip,mode in definitions:
        visible,raw_indices=visibility_buffers([index | (0xfa000000 if strip else 0)],3,PROFILE,
                                              indirect=indirect,strip_upper_byte=strip)
        transform=metadata['unity_ObjectToWorld'] & (0x7fffffff if mode=='shared' else 0xffffffff)
        unlit=metadata['_UnlitColor'] & (0x7fffffff if mode in ('fallback','emissive') else 0xffffffff)
        emissive=metadata['_EmissiveColor'] & (0xffffffff if mode=='emissive' else 0x7fffffff)
        builtin=pack_constants(layouts[0]['UnityDOTSInstancing_BuiltinPropertyMetadata'],
                               {'unity_DOTSInstancingF48_Metadataunity_ObjectToWorld':signed(transform)})
        material=pack_constants(layouts[1]['UnityPerMaterial'],
                                {'_UnlitColor':[1. if mode=='fallback' else 0.]*4,
                                 '_UnlitColorMap_ST':[1.,1.,0.,0.],'_EmissiveColor':[0.,0.,0.],
                                 '_EmissiveExposureWeight':0.})
        overrides=pack_constants(layouts[1]['UnityDOTSInstancing_MaterialPropertyMetadata'],
                                 {'unity_DOTSInstancingF16_Metadata_UnlitColor':signed(unlit),
                                  'unity_DOTSInstancingF12_Metadata_EmissiveColor':signed(emissive)})
        def constants(values):return [dict(slot=i,hex=value.hex()) for i,value in enumerate(values)]
        def buffer(slot,value):return dict(slot=slot,type=4,stride=4,hex=value.hex())
        stages=[dict(constants=constants([vs_globals,visible,builtin]),images=[],samplers=[],
                     buffers=[buffer(0,data),buffer(1,raw_indices)]),
                dict(constants=constants([ps_globals,visible,material,overrides]),
                     images=[dict(slot=slot,asset=white_asset) for slot in (0,3)],samplers=[dict(slot=0,values=sampler)],
                     buffers=[buffer(1,data),buffer(2,raw_indices)])]
        yield dict(name=name,stages=stages,bounds=bounds[0 if mode=='shared' else index],
                   color=[255,255,255] if mode=='fallback' else [0,255,255] if mode=='emissive' else colors[index],
                   instance=index,indirect=indirect,strip_upper_byte=strip,mode=mode,
                   raw_payload_bytes=2*(len(data)+len(raw_indices)),
                   constant_bytes=sum(len(bytes.fromhex(c['hex'])) for s in stages for c in s['constants']))
