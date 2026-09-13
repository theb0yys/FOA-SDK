# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Read-only comparison of source texture mip bytes and native streaming products."""
import hashlib
import zlib
from foa_scene_native_mesh_audit import NativeCache, field, integer, require
from foa_scene_texture import TextureArray, decode, layer_count

# Exact pinned RHI enum values; these are not DXGI numbers.
RHI_FORMATS = {(1,0):50, (1,1):50, (3,0):19, (3,1):20, (4,0):19, (4,1):20,
               (10,0):55, (10,1):56, (12,0):59, (12,1):60, (26,0):61}
IMAGE_TYPE = '3c96a826-9099-4308-a604-7b19adbf8761'
CHAIN_TYPE = 'cb403c8a-6982-4c9f-8090-78c9c36fbedb'


def descendant(node, name):
    matches=[]
    def visit(item):
        if item['name']==zlib.crc32(name.lower().encode()): matches.append(item)
        for child in item['children']: visit(child)
    visit(node)
    require(len(matches)==1, 'Missing or ambiguous native image field: '+name)
    return matches[0]


def values(node):
    require(not node['value'], 'Unexpected packed integer table.')
    return [int.from_bytes(c['value'],'big') for c in node['children']]


def size(node):
    return tuple(integer(node,k) for k in ('Width','Height','Depth'))


def expected_mip(texture, mip, layer=0):
    offset=mip.offset+layer*mip.size
    raw=texture.payload[offset:offset+mip.size]
    if texture.format==3:
        expanded=bytearray(mip.width*mip.height*4)
        expanded[0::4]=raw[0::3];expanded[1::4]=raw[1::3];expanded[2::4]=raw[2::3];expanded[3::4]=bytes([255])*(mip.width*mip.height)
        return bytes(expanded)
    return raw


def audit_texture(cache, source_name, blob):
    texture=decode(blob);layers=layer_count(texture);is_array=type(texture) is TextureArray
    source_name=source_name.replace(chr(92),'/')
    rows=cache.db.execute('SELECT p.ProductName,s.SourceGuid FROM Products p JOIN Jobs j ON p.JobPK=j.JobID JOIN Sources s ON j.SourcePK=s.SourceID WHERE s.SourceName=? AND j.JobKey=? AND j.Status=4 AND j.Platform=? AND p.SubID=1000',
                          (source_name,'FOA Source Texture','pc')).fetchmany(2)
    require(len(rows)==1, 'Exact native texture job has not succeeded uniquely.')
    image=cache.read(cache.root.parent/rows[0][0])
    require(image['type']==IMAGE_TYPE and image['version']==3, 'Unqualified native streaming image schema.')
    desc=descendant(image,'m_imageDescriptor')
    require(size(field(desc,'Size'))==(texture.width,texture.height,1), 'Native image dimensions changed.')
    require(integer(desc,'MipLevels')==texture.mip_count and integer(desc,'ArraySize')==layers,
            'Native image mip or surface count changed.')
    require(integer(desc,'Format')==RHI_FORMATS[(texture.format,texture.color_space)], 'Native image format/colour space changed.')
    require(integer(desc,'Dimension')==2 and integer(desc,'IsCubemap')==0, 'Native image surface kind changed.')
    view=descendant(image,'m_imageViewDescriptor')
    require(integer(view,'OverrideFormat')==0 and integer(view,'MipSliceMin')==0 and integer(view,'MipSliceMax')>=texture.mip_count-1, 'Native view overrides image format or mip range.')
    require(integer(view,'IsArray')==int(is_array) and integer(view,'IsCubemap')==0 and
            integer(view,'ArraySliceMin')==0 and integer(view,'ArraySliceMax')>=layers-1,
            'Native image view array kind or layer range changed.')
    mips=texture.mips();index=0;total=0;proof=[]
    chain_map=values(field(image,'m_mipLevelToChainIndex'))
    require(len(chain_map)==15, 'Unqualified native mip map capacity.')
    chains=field(image,'m_mipChains')['children']
    require(1<=len(chains)<=texture.mip_count,'Invalid native mip chain count.')
    for number,entry in enumerate(chains):
        require(integer(entry,'m_mipOffset')==index,'Native mip chain order changed.')
        count=integer(entry,'m_mipCount')
        require(chain_map[index:index+count]==[number]*count, 'Native mip chain lookup changed.')
        require(1<=count<=texture.mip_count-index,'Invalid native mip chain size.')
        if number < len(chains)-1:
            reference=field(entry,'m_asset')['value']
            require(reference[:16]==rows[0][1] and int.from_bytes(reference[16:20],'big')==1001+number, 'Native texture mip identity changed.')
        chain=field(image,'m_tailMipChain') if number==len(chains)-1 else cache.read(cache.resolve(field(entry,'m_asset')))
        require(chain['type']==CHAIN_TYPE and chain['version']==1,'Unqualified native mip chain schema.')
        require(integer(chain,'m_mipLevels')==count and integer(chain,'m_arraySize')==layers,'Native mip chain topology changed.')
        data=field(chain,'m_imageData')['value']
        offsets=values(field(chain,'m_subImageDataOffsets'))
        layouts=field(chain,'m_subImageLayouts')['children']
        require(len(offsets)==count*layers+1 and len(layouts)==15 and offsets[0]==0 and offsets[-1]==len(data), 'Native mip buffer framing changed.')
        sub_map=values(field(chain,'m_mipToSubImageOffset'))
        require(len(sub_map)==15 and sub_map[:count]==[n*layers for n in range(count)], 'Native subimage lookup changed.')
        for local in range(count):
            mip=mips[index];layout=layouts[local]
            require(size(field(layout,'m_size'))==(mip.width,mip.height,1),'Native mip dimensions changed.')
            row_count=mip.height if texture.format in (1,3,4) else (mip.height+3)//4
            for layer in range(layers):
                expected=expected_mip(texture,mip,layer);sub=local*layers+layer
                require(integer(layout,'m_rowCount')==row_count and integer(layout,'m_bytesPerRow')==len(expected)//row_count and integer(layout,'m_bytesPerImage')==len(expected), 'Native mip layout changed.')
                require(offsets[sub+1]-offsets[sub]==len(expected) and data[offsets[sub]:offsets[sub+1]]==expected,
                        'Native mip/layer bytes differ from source projection.')
                entry={'mip':index,'bytes':len(expected),'sha256':hashlib.sha256(expected).hexdigest()}
                if is_array:entry['layer']=layer
                proof.append(entry);total+=len(expected)
            index+=1
    require(index==texture.mip_count and integer(image,'m_totalImageDataSize')==total,'Native image is incomplete.')
    return {'status':'PASSED','mips':proof,'native_bytes':total,'native_format':RHI_FORMATS[(texture.format,texture.color_space)],
            'source_projection_sha256':hashlib.sha256(blob).hexdigest(), 'sampler_shader_rendering':'NOT_RUN'}
