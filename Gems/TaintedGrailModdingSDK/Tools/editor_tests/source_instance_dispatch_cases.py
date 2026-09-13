# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Original shader multi-instance fixtures; visibility and scene values are synthetic."""
import copy
import struct
from foa_scene_dots_buffers import PROFILE, pack_matrix_columns, pack_streams, visibility_buffers
from foa_scene_shader_constants import pack_constants
from source_dots_shader_cases import cases as single_cases


def cases(programs, layouts, white_asset, sampler):
    legacy = list(single_cases(programs, layouts, white_asset, sampler))
    for case in legacy:
        yield dict(case, version=2, rectangles=[dict(bounds=case['bounds'], color=case['color'])])
    template = legacy[0]
    matrices = [[.25,0,0,-.55, 0,.5,0,0, 0,0,1,0, 0,0,0,1],
                [0,-.25,0,0, .5,0,0,0, 0,0,1,0, 0,0,0,1],
                [-.25,0,0,.55, 0,.25,0,.2, 0,0,1,0, 0,0,0,1]]
    rectangles = [dict(bounds=b, color=c) for b,c in zip(
        [[-.75,-.4,-.35,.4],[-.2,-.4,.2,.4],[.35,0,.75,.4]], [[255,0,0],[0,255,0],[0,0,255]])]

    def build(name, transforms, rects, visible, indirect, strip=False, slots=None):
        slots = list(range(len(transforms))) if slots is None else slots
        capacity = max(slots)+1
        # Gaps contain explicit sentinel bytes; they must never be selected.
        columns = bytearray(b'\x55'*(capacity*48)); colors = bytearray(b'\x55'*(capacity*16))
        for matrix,rect,slot in zip(transforms,rects,slots):
            words = list(struct.unpack('<16I',struct.pack('<16f',*matrix)))
            columns[slot*48:(slot+1)*48] = pack_matrix_columns([words])
            colors[slot*16:(slot+1)*16] = struct.pack('<4f',*[v/255 for v in rect['color']],1.)
        data, metadata = pack_streams([
            dict(name='unity_ObjectToWorld',stride=48,data=bytes(columns)),
            dict(name='_UnlitColor',stride=16,data=bytes(colors)),
            dict(name='_EmissiveColor',stride=12,data=bytes(capacity*12)),
        ],capacity,PROFILE)
        indices = [i | (0xfa000000 if strip else 0) for i in visible]
        constants, raw = visibility_buffers(indices,capacity,PROFILE,indirect=indirect,strip_upper_byte=strip)
        stages = copy.deepcopy(template['stages'])
        signed = lambda x:x if x<0x80000000 else x-0x100000000
        for stage in stages:stage['constants'][1]['hex'] = constants.hex()
        stages[0]['constants'][2]['hex'] = pack_constants(layouts[0]['UnityDOTSInstancing_BuiltinPropertyMetadata'],
            {'unity_DOTSInstancingF48_Metadataunity_ObjectToWorld':signed(metadata['unity_ObjectToWorld'])}).hex()
        stages[1]['constants'][3]['hex'] = pack_constants(layouts[1]['UnityDOTSInstancing_MaterialPropertyMetadata'],
            {'unity_DOTSInstancingF16_Metadata_UnlitColor':signed(metadata['_UnlitColor']),
             'unity_DOTSInstancingF12_Metadata_EmissiveColor':metadata['_EmissiveColor'] & 0x7fffffff}).hex()
        for stage in stages:
            stage['buffers'][0]['hex'] = data.hex(); stage['buffers'][1]['hex'] = raw.hex()
        expected = {slot:rect for slot,rect in zip(slots,rects)}
        return dict(name=name,version=3,instance_count=len(visible),stages=stages,
                    rectangles=[expected[i] for i in visible],explicit_slots=slots,visible=indices,
                    raw_payload_bytes=2*(len(data)+len(raw)),
                    constant_bytes=sum(len(bytes.fromhex(c['hex'])) for s in stages for c in s['constants']))

    for name,visible,indirect,strip in [('direct_all',[0,1,2],False,False),
            ('direct_subset',[2,0],False,False),('indirect_permuted',[2,0,1],True,False),
            ('indirect_subset',[1,2],True,False),('indirect_flagged',[2,0],True,True)]:
        yield build(name,matrices,rectangles,visible,indirect,strip)
    yield build('capacity_gaps',matrices,rectangles,[4,0],True,slots=[0,2,4])
    for side,indirect in ((16,False),(64,True)):
        step=1.8/side; transforms=[]; rects=[]
        for i in range(side*side):
            x=-.9+step*(i%side+.5); y=-.9+step*(i//side+.5); scale=step*.375
            transforms.append([scale,0,0,x, 0,scale,0,y, 0,0,1,0, 0,0,0,1])
            rgb=([255,0,0],[0,255,0],[0,0,255])[i%3]
            rects.append(dict(bounds=[x-step*.3,y-step*.3,x+step*.3,y+step*.3],color=rgb))
        yield build('indirect_4096' if indirect else 'direct_256',transforms,rects,
                    list(reversed(range(side*side))),indirect)
