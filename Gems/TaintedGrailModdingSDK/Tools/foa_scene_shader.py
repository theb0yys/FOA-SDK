# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Bounded inspection of explicit PC shader segments and player program references.

No HLSL reconstruction or executable shader conversion. Parameter records have
bounded structural decoding; the GPU wrapper prefix remains opaque. The embedded type tree owns pass/variant joins;
DXBC containers are validated independently, never identified by display names.
"""
from dataclasses import dataclass
import struct

import foa_heightmap_importer as h
from foa_scene_material import integer, require, text, inline_sampler

MAX_BLOB = 32*1024*1024
MAX_DECOMPRESSED = 128*1024*1024
MAX_ENTRIES = 65536
STAGES = ('progVertex','progFragment','progGeometry','progHull','progDomain','progRayTracing')


@dataclass(frozen=True)
class ShaderEntry:
    segment: int
    offset: int
    data: bytes


def unpack_entries(tree, compressed_blob, cancelled=lambda: False):
    """Read every declared segment and account for every decompressed byte.

    Only the directly observed nested PC layout with 12-byte entry descriptors is
    supported. Strict partition checks reject overlaps, holes and unclaimed tails.
    Unity version fallback is deliberately not consulted to choose this layout.
    """
    require(tree.get('platforms') == [4], 'Unqualified shader platform set.')
    require(type(compressed_blob) is bytes and 0 < len(compressed_blob) <= MAX_BLOB, 'Shader blob exceeds its bound.')
    arrays = []
    for key in ('offsets','compressedLengths','decompressedLengths'):
        value = tree.get(key)
        require(type(value) is list and len(value) == 1 and type(value[0]) is list and 0 < len(value[0]) <= 64,
                'Unqualified shader segment schema.')
        arrays.append(value[0])
    offsets, sizes, expanded = arrays
    require(len(offsets) == len(sizes) == len(expanded), 'Shader segment arrays disagree.')
    total = cursor = 0
    for offset, size, count in zip(offsets, sizes, expanded):
        integer(offset, 0, MAX_BLOB)
        integer(size, 1, MAX_BLOB)
        integer(count, 1, 64*1024*1024)
        require(offset == cursor and offset+size <= len(compressed_blob), 'Shader compressed segments overlap or leave a gap.')
        total += count
        require(total <= MAX_DECOMPRESSED, 'Shader decompressed data exceeds its bound.')
        cursor += size
    require(cursor == len(compressed_blob), 'Shader compressed blob has unclaimed trailing data.')
    import lz4.block
    segments = []
    for offset, size, count in zip(offsets, sizes, expanded):
        h.check_cancelled(cancelled)
        try:
            data = lz4.block.decompress(compressed_blob[offset:offset+size], uncompressed_size=count)
        except lz4.block.LZ4BlockError as error:
            raise h.HeightmapImportError('Invalid shader segment compression.') from error
        require(len(data) == count, 'Shader decompressed length disagrees with its declaration.')
        segments.append(data)
    require(len(segments[0]) >= 4, 'Shader entry table is truncated.')
    count = integer(struct.unpack_from('<I', segments[0])[0], 1, MAX_ENTRIES)
    end = 4+12*count
    require(end <= len(segments[0]), 'Shader entry table exceeds its segment.')
    occupied = [[(0, end)]] + [[] for _ in segments[1:]]
    entries = []
    for i in range(count):
        h.check_cancelled(cancelled)
        offset, size, segment = struct.unpack_from('<3I', segments[0], 4+12*i)
        require(segment < len(segments) and size > 0 and offset+size <= len(segments[segment]),
                'Shader entry exceeds its segment.')
        occupied[segment].append((offset, offset+size))
        entries.append(ShaderEntry(segment, offset, segments[segment][offset:offset+size]))
    for ranges, data in zip(occupied, segments):
        cursor = 0
        for start, stop in sorted(ranges):
            require(start == cursor, 'Shader entries overlap or leave an unclaimed gap.')
            cursor = stop
        require(cursor == len(data), 'Shader segment has unclaimed trailing data.')
    return tuple(entries)


def player_programs(tree, entries, cancelled=lambda: False):
    """Bind all variants and their parameter entry by explicit paired indices.

    Retain tier and keyword indices without selecting a runtime variant. Common
    parameters and each variant's parameter record are both required by consumers.
    """
    parsed = tree.get('m_ParsedForm')
    require(type(parsed) is dict, 'Missing embedded shader form.')
    keywords = parsed.get('m_KeywordNames')
    flags = parsed.get('m_KeywordFlags')
    require(type(keywords) is list and len(keywords) <= 4096,
            'Invalid shader keyword table.')
    for keyword in keywords: text(keyword)
    require(len(set(keywords)) == len(keywords), 'Duplicate shader keyword.')
    require(type(flags) is list and len(flags) == len(keywords), 'Shader keyword flags disagree with names.')
    subshaders = parsed.get('m_SubShaders')
    require(type(subshaders) is list and 0 < len(subshaders) <= 64, 'Invalid shader subshader table.')
    records = []
    pass_total = name_total = 0
    for si, subshader in enumerate(subshaders):
        passes = subshader.get('m_Passes')
        require(type(passes) is list and len(passes) <= 128, 'Invalid shader pass table.')
        pass_total += len(passes)
        require(pass_total <= 256, 'Shader pass count exceeds its bound.')
        for pi, shader_pass in enumerate(passes):
            h.check_cancelled(cancelled)
            names = shader_pass.get('m_NameIndices')
            require(type(names) is list and len(names) <= 16384, 'Invalid shader parameter name table.')
            name_total += len(names)
            require(name_total <= 131072, 'Shader parameter names exceed their aggregate bound.')
            indices, labels = set(), set()
            for pair in names:
                require(isinstance(pair,(list,tuple)) and len(pair) == 2, 'Invalid shader parameter name entry.')
                label, index = text(pair[0]), integer(pair[1],0,65535)
                require(index not in indices and label not in labels, 'Ambiguous shader parameter identity.')
                indices.add(index)
                labels.add(label)
            for stage in STAGES:
                program = shader_pass.get(stage)
                require(type(program) is dict and program.get('m_SubPrograms') == [], 'Unqualified shader program schema.')
                tiers, parameters = program.get('m_PlayerSubPrograms'), program.get('m_ParameterBlobIndices')
                require(type(tiers) is list and type(parameters) is list and len(tiers) == len(parameters) and len(tiers) in (0,4),
                        'Shader player/parameter tiers disagree.')
                for tier, (variants, parameter_ids) in enumerate(zip(tiers,parameters)):
                    require(type(variants) is list and type(parameter_ids) is list and len(variants) == len(parameter_ids),
                            'Shader variant/parameter counts disagree.')
                    for vi, (variant, parameter) in enumerate(zip(variants, parameter_ids)):
                        h.check_cancelled(cancelled)
                        require(len(records) < MAX_ENTRIES, 'Shader program references exceed their bound.')
                        require(type(variant) is dict and set(variant) == {'m_BlobIndex','m_KeywordIndices','m_ShaderRequirements','m_GpuProgramType'},
                                'Unqualified player shader variant.')
                        code = integer(variant['m_BlobIndex'],0,len(entries)-1)
                        parameter = integer(parameter,0,len(entries)-1)
                        require(code != parameter, 'Shader code aliases its parameter record.')
                        ki = variant['m_KeywordIndices']
                        require(type(ki) is list and len(ki) <= len(keywords), 'Invalid shader variant keywords.')
                        for index in ki: integer(index,0,len(keywords)-1)
                        require(len(set(ki)) == len(ki), 'Duplicate shader variant keyword.')
                        integer(variant['m_ShaderRequirements'],0,(1 << 64)-1)
                        allowed = {'progVertex':(15,16), 'progFragment':(17,18)}.get(stage, ())
                        expected = variant['m_GpuProgramType']
                        require(type(expected) is int and expected in allowed,
                                'Unqualified player GPU program type.')
                        require(len(entries[code].data) >= 8 and struct.unpack_from('<2I',entries[code].data) == (202012090,expected),
                                'Shader code record disagrees with its variant.')
                        records.append({'subshader':si,'pass':pi,'stage':stage,'tier':tier,'variant':vi,
                                        'code_entry':code,'parameter_entry':parameter,'keyword_indices':tuple(ki),
                                        'requirements':variant['m_ShaderRequirements']})
    return tuple(records)


def dxbc_container(entry):
    """Locate and validate the single complete DXBC in an observed program envelope.

    Envelope bytes are preserved opaque, not parsed into guessed semantics. This
    is not shader translation, resource binding or a renderer-equivalence check.
    """
    data = entry.data
    require(len(data) >= 32, 'Truncated shader program envelope.')
    version, kind = struct.unpack_from('<2I',data)
    require(version == 202012090 and kind in (15,16,17,18), 'Unqualified shader program envelope.')
    keyword_count = integer(struct.unpack_from('<I',data,24)[0],0,4096)
    cursor, keywords = 28, []
    for _ in range(keyword_count):
        require(cursor+4 <= len(data), 'Truncated shader envelope keyword.')
        size = integer(struct.unpack_from('<I',data,cursor)[0],1,1024)
        cursor += 4
        end = cursor+size
        aligned = (end+3)&~3
        require(aligned <= len(data) and not any(data[end:aligned]), 'Invalid shader envelope keyword size or padding.')
        try:
            keywords.append(text(data[cursor:end].decode('utf-8')))
        except UnicodeError as error:
            raise h.HeightmapImportError('Invalid shader envelope keyword encoding.') from error
        cursor = aligned
    require(cursor+4 <= len(data), 'Truncated shader program size.')
    size = integer(struct.unpack_from('<I',data,cursor)[0],32,MAX_BLOB)
    cursor += 4
    require(cursor+size <= len(data), 'Shader program code exceeds its record.')
    program = data[cursor:cursor+size]
    program_end = cursor+size
    # The opaque Unity prefix varies. Require one valid complete container ending
    # exactly at the declared code boundary, rather than accepting any magic hit.
    candidates = []
    for offset in range(min(1024,len(program)-31)):
        if program[offset:offset+4] != b'DXBC': continue
        candidate = program[offset:]
        if struct.unpack_from('<I',candidate,24)[0] == len(candidate): candidates.append((offset,candidate))
    require(len(candidates) == 1, 'Shader code lacks one complete bounded DXBC container.')
    offset, blob = candidates[0]
    require(struct.unpack_from('<I',blob,20)[0] == 1, 'Unqualified DXBC container version.')
    count = integer(struct.unpack_from('<I',blob,28)[0],1,64)
    end = 32+4*count
    require(end <= len(blob), 'DXBC chunk table exceeds its container.')
    ranges = [(0,end)]
    chunks = []
    for i in range(count):
        start = struct.unpack_from('<I',blob,32+4*i)[0]
        require(start >= end and start+8 <= len(blob) and start % 4 == 0, 'Invalid DXBC chunk address.')
        size = struct.unpack_from('<I',blob,start+4)[0]
        require(start+8+size <= len(blob), 'DXBC chunk exceeds its container.')
        ranges.append((start,start+8+size))
        chunks.append((blob[start:start+4],blob[start+8:start+8+size]))
    cursor = 0
    for start,stop in sorted(ranges):
        require(start == cursor, 'DXBC chunks overlap or leave an unclaimed gap.')
        cursor = stop
    require(cursor == len(blob), 'DXBC container has unclaimed trailing data.')
    code = [payload for tag,payload in chunks if tag in (b'SHDR',b'SHEX')]
    require(len(code) == 1 and len(code[0]) >= 8 and len(code[0]) % 4 == 0, 'Missing or ambiguous DXBC instruction chunk.')
    token, words = struct.unpack_from('<2I',code[0])
    require(words*4 == len(code[0]) and token >> 16 == (1 if kind in (15,16) else 0), 'DXBC program stage or word count mismatch.')
    return blob, {'opaque_prefix_bytes':offset,'opaque_trailer_bytes':len(data)-program_end,
                  'envelope_keywords':tuple(keywords), 'chunks':tuple(tag.decode('ascii') for tag,_ in chunks),'stage':'vertex' if kind in (15,16) else 'fragment'}


class ParameterReader:
    """Length-prefixed scalar/UTF-8 reader with per-record and per-field bounds."""
    def __init__(self, data):
        require(type(data) is bytes and 4 <= len(data) <= 4*1024*1024, 'Shader parameter record exceeds its bound.')
        self.data, self.cursor = data, 0

    def uint(self):
        require(self.cursor+4 <= len(self.data), 'Truncated shader parameter scalar.')
        value = struct.unpack_from('<I',self.data,self.cursor)[0]
        self.cursor += 4
        return value

    def count(self, maximum=4096):
        return integer(self.uint(),0,maximum)

    def string(self, empty=False):
        count = self.count(1024)
        end = self.cursor+count
        aligned = (end+3)&~3
        require(aligned <= len(self.data) and not any(self.data[end:aligned]), 'Invalid shader parameter string size or padding.')
        try: value = self.data[self.cursor:end].decode('utf-8')
        except UnicodeError as error:
            raise h.HeightmapImportError('Invalid shader parameter string encoding.') from error
        self.cursor = aligned
        if not empty or value: text(value)
        return value


def parameter_record(entry, cancelled=lambda: False):
    """Read the observed 202012090 parameter layout, retaining raw binding values.

    This format is cross-checked against the archived AssetRipper reader and the
    source's embedded common parameters. It does not select GPU uniforms, infer
    inline sampler bit meanings, or convert packed material/normal semantics.
    Structured fields retain their own source offsets and array sizes.
    """
    reader = ParameterReader(entry.data)
    require(reader.uint() == 202012090, 'Unqualified shader parameter record version.')
    total = 0
    def values():
        nonlocal total
        count = reader.count()
        total += count
        require(total <= 16384, 'Shader parameter count exceeds its bound.')
        result, names = [], set()
        for _ in range(count):
            name = reader.string()
            require(name not in names, 'Duplicate shader parameter in a group.')
            names.add(name)
            kind = reader.count(5)
            rows, columns = reader.count(4), reader.count(4)
            matrix = reader.count(1)
            array_size, offset = reader.count(1 << 20), reader.count(16*1024*1024)
            require(columns > 0 and (not matrix or rows > 0), 'Invalid shader parameter dimensions.')
            result.append({'name':name,'type':kind,'rows':rows,'columns':columns,
                           'matrix':bool(matrix),'array_size':array_size,'index':offset})
        return result
    groups = []
    for gi in range(integer(reader.uint(),1,256)):
        h.check_cancelled(cancelled)
        name = reader.string(empty=gi == 0)
        size = reader.count(16*1024*1024)
        fields = values()
        structures, names = [], set()
        for _ in range(reader.count(256)):
            name_value = reader.string()
            require(name_value not in names, 'Duplicate shader parameter structure.')
            names.add(name_value)
            index, array_size, struct_size = reader.count(16*1024*1024), reader.count(1 << 20), reader.count(16*1024*1024)
            structures.append({'name':name_value,'index':index,'array_size':array_size,
                               'size':struct_size,'parameters':values()})
        groups.append({'name':name,'used_size':size,'parameters':fields,'structures':structures})
    resources = []
    for _ in range(reader.count()):
        h.check_cancelled(cancelled)
        name = reader.string(empty=True)
        kind, index, extra = reader.count(4), reader.count(65535), reader.uint()
        resource = {'name':name,'kind':kind,'index':index,'extra':extra}
        if kind == 0:
            text(name)
            flags = reader.uint()
            require(flags >> 1 in (2,3,4,5,6), 'Unqualified shader texture dimension flags.')
            require(extra == 0xffffffff or extra <= 65535, 'Invalid shader texture sampler index.')
            resource.update(dimension=flags >> 1,multisampled=bool(flags & 1),sampler_index=-1 if extra == 0xffffffff else extra)
        resources.append(resource)
    require(reader.cursor == len(reader.data), 'Shader parameter record has unclaimed trailing data.')
    return {'groups':groups,'resources':resources}


def variant_resources(tree, program, parameters):
    """Combine embedded common and variant resources by register namespace/index.

    Duplicate declarations must agree. Sampler integers remain opaque until their
    encoding is independently qualified; texture-owned sampler indices stay exact.
    """
    shader_pass = tree['m_ParsedForm']['m_SubShaders'][program['subshader']]['m_Passes'][program['pass']]
    names = {index:name for name,index in shader_pass['m_NameIndices']}
    common = shader_pass[program['stage']]['m_CommonParameters']
    require(type(common) is dict and set(common) == {'m_VectorParams','m_MatrixParams','m_TextureParams','m_BufferParams',
            'm_ConstantBuffers','m_ConstantBufferBindings','m_UAVParams','m_Samplers'}, 'Unqualified common shader parameter schema.')
    resources = [dict(r) for r in parameters['resources']]
    for category,kind in (('m_TextureParams',0),('m_ConstantBufferBindings',1),('m_BufferParams',2),('m_UAVParams',3),('m_Samplers',4)):
        values = common[category]
        require(type(values) is list and len(values) <= 4096, 'Common shader resources exceed their bound.')
        for value in values:
            if kind == 4:
                resource = {'kind':4,'name':'','index':integer(value.get('bindPoint'),0,65535),
                            'extra':integer(value.get('sampler'),0,0xffffffff)}
            else:
                name_index = integer(value.get('m_NameIndex'),0,65535)
                require(name_index in names, 'Common shader resource has an absent name index.')
                resource = {'kind':kind,'name':names[name_index],'index':integer(value.get('m_Index'),0,65535)}
                if kind == 0:
                    sampler = integer(value.get('m_SamplerIndex'),-1,65535)
                    require(type(value.get('m_MultiSampled')) is bool, 'Invalid common texture multisample flag.')
                    resource.update(sampler_index=sampler,dimension=integer(value.get('m_Dim'),2,6),multisampled=value['m_MultiSampled'])
            resources.append(resource)
    joined = {}
    for resource in resources:
        kind = resource['kind']
        namespace = ('t','cb','t','u','s')[kind]
        key = (namespace,resource['index'])
        signature = (kind,resource['name'])
        if kind == 0: signature += (resource['dimension'],resource['multisampled'],resource['sampler_index'])
        if kind == 4:
            inline_sampler(resource['extra'])
            signature += (resource['extra'],)
        if key in joined:
            require(joined[key]['signature'] == signature, 'Conflicting common and variant shader resource declarations.')
            joined[key]['declarations'].append(resource)
        else:
            joined[key] = {'namespace':namespace,'index':resource['index'],'signature':signature,'declarations':[resource]}
    # A nonnegative TextureParameter sampler index explicitly binds this texture's
    # sampler. Other textures can use that register in code without owning it.
    for resource in resources:
        if resource['kind'] != 0 or resource['sampler_index'] < 0: continue
        key = ('s',resource['sampler_index'])
        signature = ('texture-sampler',resource['name'])
        declaration = {'texture_name':resource['name'],'texture_register':resource['index']}
        if key in joined:
            require(joined[key]['signature'] == signature, 'Shader sampler has conflicting texture/inline owners.')
            joined[key]['declarations'].append(declaration)
        else:
            joined[key] = {'namespace':'s','index':key[1],'signature':signature,'declarations':[declaration]}
    for value in joined.values():
        if value['namespace'] == 's' and value['signature'][0] == 4:
            value['inline_sampler'] = inline_sampler(value['signature'][2])
    return tuple(joined[key] for key in sorted(joined))
