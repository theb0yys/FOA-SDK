# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import hashlib
from pathlib import Path
import struct
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_texture as t


def tree(fmt=4, width=4, height=4, count=3, color=0):
    size = sum(m.size for m in t.mip_layout(width, height, count, fmt))
    return {'m_TextureDimension':2, 'm_ImageCount':1, 'm_IsPreProcessed':False,
            'm_Width':width, 'm_Height':height, 'm_MipCount':count, 'm_TextureFormat':fmt,
            'm_ColorSpace':color, 'm_CompleteImageSize':size,
            'm_TextureSettings':{'m_FilterMode':1,'m_Aniso':8,'m_MipBias':-1.0,'m_WrapU':0,'m_WrapV':1,'m_WrapW':2}}


def texture(fmt=4, color=0):
    data = tree(fmt=fmt, color=color)
    payload = bytes(i % 251 for i in range(data['m_CompleteImageSize']))
    return t.from_tree(data, payload)


class TextureTests(unittest.TestCase):
    def test_block_layout_includes_small_and_rectangular_mips(self):
        self.assertEqual([m.size for m in t.mip_layout(9, 5, 4, 10)], [48, 8, 8, 8])
        self.assertEqual([m.offset for m in t.mip_layout(9, 5, 4, 12)], [0, 96, 112, 128])
        self.assertEqual([m.size for m in t.mip_layout(4, 4, 3, 26)], [8,8,8])

    def test_uncompressed_no_row_padding(self):
        self.assertEqual([m.size for m in t.mip_layout(5, 3, 3, 3)], [45,6,3])

    def test_alpha8_rectangular_mips_keep_each_alpha_byte(self):
        self.assertEqual([m.size for m in t.mip_layout(5, 3, 3, 1)], [15, 2, 1])
        self.assertEqual([m.offset for m in t.mip_layout(5, 3, 3, 1)], [0, 15, 17])
        raw = bytes(range(18))
        for color in (0, 1):
            value = t.Texture(5, 3, 3, 1, color, raw)
            self.assertEqual(t.encode(value)[64:], raw)
            self.assertEqual(t.decode(t.encode(value)), value)
        with self.assertRaises(t.h.HeightmapImportError):
            t.encode(t.Texture(5, 3, 3, 1, 0, raw[:-1]))

    def test_all_formats_and_colour_spaces_roundtrip(self):
        for fmt in t.FORMATS:
            for color in (0,1):
                if fmt == 26 and color: continue
                with self.subTest(fmt=fmt, color=color):
                    source = texture(fmt, color)
                    encoded = t.encode(source)
                    self.assertEqual(t.decode(encoded), source)
                    self.assertEqual(encoded[64:], source.payload)
                    self.assertEqual(t.encode(t.decode(encoded)), encoded)

    def test_no_pixel_or_mip_replacement(self):
        source = texture()
        result = t.decode(t.encode(source))
        for mip in source.mips():
            self.assertEqual(result.payload[mip.offset:mip.offset+mip.size], source.payload[mip.offset:mip.offset+mip.size])

    def test_bounds_and_integer_types(self):
        for args in ((0,4,1,4), (8193,4,1,4), (4,4,4,4), (True,4,1,4), (4,4,1,True), (4,4,1,27), (8192,8192,1,4)):
            with self.subTest(args=args), self.assertRaises(t.h.HeightmapImportError): t.mip_layout(*args)

    def test_payload_exact_length(self):
        data = tree()
        for length in (0, data['m_CompleteImageSize']-1, data['m_CompleteImageSize']+1):
            with self.assertRaises(t.h.HeightmapImportError): t.from_tree(data, bytes(length))
        data['m_CompleteImageSize'] -= 1
        with self.assertRaises(t.h.HeightmapImportError): t.from_tree(data, bytes(data['m_CompleteImageSize']))

    def test_reject_unknown_projection_and_trailing_bytes(self):
        raw=t.encode(texture())
        for changed in (b'X'+raw[1:], raw[:8]+struct.pack('<I',2)+raw[12:], raw+b'X', raw[:-1], b''):
            with self.assertRaises(t.h.HeightmapImportError): t.decode(changed)

    def test_checksum_covers_header_and_payload(self):
        raw=t.encode(texture())
        for index in (28,32,64,len(raw)-1):
            changed=bytearray(raw);changed[index]^=1
            with self.assertRaises(t.h.HeightmapImportError): t.decode(bytes(changed))

    def test_unknown_colour_and_bc4_srgb_rejected(self):
        for fmt,color in ((4,2),(4,True),(26,1)):
            with self.assertRaises(t.h.HeightmapImportError): texture(fmt,color)

    def test_surface_and_platform_rejections(self):
        for key,value in (('m_TextureDimension',3),('m_ImageCount',6),('m_IsPreProcessed',True),('m_Depth',2),('m_PlatformBlob',[1])):
            data=tree();data[key]=value
            with self.assertRaises(t.h.HeightmapImportError): t.from_tree(data,bytes(84))

    def test_sampler_is_explicit_and_finite(self):
        for key,value in (('m_FilterMode',3),('m_Aniso',17),('m_WrapU',4),('m_MipBias',float('nan')),('unknown',0)):
            data=tree();data['m_TextureSettings'][key]=value
            with self.assertRaises(t.h.HeightmapImportError): t.from_tree(data,bytes(84))

    def test_shader_defaults_keep_native_storage_and_explicit_color_space(self):
        for space in ('Gamma', 'Linear'):
            for name, pixel in [('white', b'\xff'*4), ('black', bytes(4)),
                                ('bump', b'\x7f\x7f\xff\xff'), ('linearGrey', b'\x7f'*4), ('grey', b'\x7f'*4)]:
                value = t.shader_default_2d(name, unity_version='6000.0.64f1', rendering_color_space=space)
                self.assertEqual((value.width, value.height, value.mip_count, value.format), (4, 4, 1, 4))
                self.assertEqual(value.payload, pixel*16)
                self.assertEqual(value.color_space, int(space == 'Linear' and name in ('white','black','grey')))
                self.assertEqual(t.decode(t.encode(value)), value)

    def test_shader_defaults_reject_unknown_profile_names_and_implicit_settings(self):
        for name, version, space in [('', '6000.0.64f1', 'Linear'), ('gray', '6000.0.64f1', 'Linear'),
                                     ('white', '6000.0.63f1', 'Linear'), ('white', '6000.0.64f1', None),
                                     (None, '6000.0.64f1', 'Gamma'), ('white', '6000.0.64f1', 1)]:
            with self.assertRaises(t.h.HeightmapImportError):
                t.shader_default_2d(name, unity_version=version, rendering_color_space=space)

    def make_reader(self, fail=False):
        class Reader:
            Length=100
            Position=7
            def read_bytes(self,size):
                self.Position+=size
                if fail: raise OSError('synthetic failure')
                return bytes(size)
        reader=Reader()
        obj=SimpleNamespace(byte_size=6,type=SimpleNamespace(name='Texture2D'), assets_file=SimpleNamespace(name='CAB-test',target_platform=19,reader=SimpleNamespace(endian='<')), get_raw_data=lambda:b'record')
        data=tree();data['m_StreamData']={'path':'archive:/CAB-test/CAB-test.resS','offset':10,'size':84}
        return obj,data,reader

    def test_explicit_stream_restores_position_and_records_hashes(self):
        obj,data,reader=self.make_reader()
        with patch.object(t,'embedded_tree',return_value=data), patch.object(t.importlib.metadata,'version',return_value='1.24.2'):
            result,meta=t.read_texture(obj,{'CAB-test.resS':reader})
        self.assertEqual(reader.Position,7)
        self.assertEqual(meta['record_sha256'],hashlib.sha256(b'record').hexdigest())
        self.assertEqual(meta['sampler'],data['m_TextureSettings'])
        self.assertEqual(result.payload,bytes(84))

    def test_oversized_record_rejected_before_copy(self):
        obj,data,reader=self.make_reader();obj.byte_size=64*1024*1024+1
        obj.get_raw_data=lambda:self.fail('Oversized source copied')
        with patch.object(t.importlib.metadata,'version',return_value='1.24.2'):
            with self.assertRaises(t.h.HeightmapImportError): t.read_texture(obj,{'CAB-test.resS':reader})

    def test_stream_failure_restores_position(self):
        obj,data,reader=self.make_reader(True)
        with patch.object(t,'embedded_tree',return_value=data), patch.object(t.importlib.metadata,'version',return_value='1.24.2'):
            with self.assertRaises(OSError): t.read_texture(obj,{'CAB-test.resS':reader})
        self.assertEqual(reader.Position,7)

    def test_no_ambient_stream_lookup(self):
        for change in ('path','size','conflict'):
            obj,data,reader=self.make_reader()
            if change=='path': data['m_StreamData']['path']='../elsewhere.resS'
            elif change=='size': data['m_StreamData']['offset']=50
            else: data['image data']=[1]
            with patch.object(t,'embedded_tree',return_value=data), patch.object(t.importlib.metadata,'version',return_value='1.24.2'):
                with self.assertRaises(t.h.HeightmapImportError): t.read_texture(obj,{'CAB-test.resS':reader})
            self.assertEqual(reader.Position,7)

    def test_cancellation_before_source_read(self):
        obj,data,reader=self.make_reader()
        with self.assertRaises(t.h.HeightmapImportError): t.read_texture(obj,{'CAB-test.resS':reader},lambda:True)

class TextureArrayTests(unittest.TestCase):
    def test_v1_packet_stays_byte_identical(self):
        payload=bytes(range(16))
        prefix=struct.pack('<8s6I',b'FOATEX01',1,2,2,1,4,0)
        legacy=prefix+hashlib.sha256(prefix+payload).digest()+payload
        self.assertEqual(t.encode(t.Texture(2,2,1,4,0,payload)),legacy)
        self.assertIs(type(t.decode(legacy)),t.Texture)
        self.assertEqual(t.encode(t.decode(legacy)),legacy)

    def test_single_layer_array_is_explicit(self):
        value=t.TextureArray(2,2,1,4,0,bytes(range(16)),1)
        encoded=t.encode(value)
        prefix=struct.pack('<8s7I',b'FOATEX01',2,2,2,1,4,0,1)
        self.assertEqual(encoded,prefix+hashlib.sha256(prefix+value.payload).digest()+value.payload)
        self.assertIs(type(t.decode(encoded)),t.TextureArray)
        self.assertEqual(t.decode(encoded),value)

    def test_mip_major_layers_keep_distinct_rows_blocks_and_mips(self):
        for fmt in t.FORMATS:
            for layers in (1,2,7):
                mips=t.mip_layout(9,5,4,fmt)
                chunks=[bytes((n*61+layer*23+i)%256 for i in range(m.size))
                        for n,m in enumerate(mips) for layer in range(layers)]
                value=t.TextureArray(9,5,4,fmt,0,b''.join(chunks),layers)
                decoded=t.decode(t.encode(value));self.assertEqual(decoded,value)
                for n,m in enumerate(decoded.mips()):
                    self.assertEqual(m.offset,mips[n].offset*layers)
                    self.assertEqual(m.size,mips[n].size)
                    for layer in range(layers):
                        offset=m.offset+layer*m.size
                        self.assertEqual(decoded.payload[offset:offset+m.size],chunks[n*layers+layer])

    def test_aggregate_bounds_cover_rgb_expansion_and_layers(self):
        for layers in (0,True,257,-1):
            with self.assertRaises(t.h.HeightmapImportError):
                t.encode(t.TextureArray(1,1,1,4,0,bytes(4),layers))
        # Both source byte count and the RGB24-to-RGBA32 native allocation are bounded.
        for fmt,layers in ((4,33),(3,40)):
            with self.assertRaises(t.h.HeightmapImportError):
                t.TextureArray(1024,1024,1,fmt,0,b'',layers).mips()
        self.assertEqual(t.TextureArray(1,1,1,4,0,b'',256).mips()[-1].size,4)

    def test_framing_and_digest_cover_layer_header(self):
        raw=t.encode(t.TextureArray(2,2,2,4,0,bytes(range(40)),2))
        for changed in (raw[:-1],raw+b'x',raw[:64],raw[:8]+struct.pack('<I',3)+raw[12:]):
            with self.assertRaises(t.h.HeightmapImportError):t.decode(changed)
        for index in (28,32,36,68,len(raw)-1):
            changed=bytearray(raw);changed[index]^=1
            with self.assertRaises(t.h.HeightmapImportError):t.decode(bytes(changed))
        for layers in (0,1,257):
            changed=bytearray(raw);struct.pack_into('<I',changed,32,layers)
            changed[36:68]=hashlib.sha256(changed[:36]+changed[68:]).digest()
            with self.assertRaises(t.h.HeightmapImportError):t.decode(bytes(changed))

    def test_array_packets_do_not_authorize_source_array_decoding(self):
        data=tree();data['m_TextureDimension']=5;data['m_ImageCount']=2
        with self.assertRaises(t.h.HeightmapImportError):t.from_tree(data,bytes(168))


if __name__=='__main__': unittest.main()
