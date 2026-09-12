# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Read source mesh channels without resampling, welding or inferring geometry.

Source records/stream bytes remain authoritative. Decoded channels are separate
values, not a game-native edit payload. Embedded schemas and explicit bundle
members only; no ambient resource search or proprietary fixtures.
"""
from dataclasses import dataclass
import hashlib
import importlib.metadata
import math
import struct

import foa_heightmap_importer as h
from foa_scene_asset_binding import embedded_tree

MAX_VERTICES = 1000000
MAX_INDICES = 6000000
MAX_BYTES = 64 * 1024 * 1024
MAX_SUBMESHES = 4096
FORMATS = (("f",4), ("e",2), ("B",1), ("b",1), ("H",2), ("h",2),
           ("B",1), ("b",1), ("H",2), ("h",2), ("I",4), ("i",4))


def integer(value, lower, upper, label):
    if type(value) is not int or not lower <= value <= upper:
        raise h.HeightmapImportError("Invalid mesh " + label + ".")
    return value


def finite_rows(rows, count, dimensions, label):
    if len(rows) != count:
        raise h.HeightmapImportError("Mesh channel count mismatch: " + label)
    result = []
    for row in rows:
        if len(row) not in dimensions or any(not math.isfinite(v) for v in row):
            raise h.HeightmapImportError("Invalid mesh channel values: " + label)
        result.append(tuple(row))
    return result


def vertex_channels(vertex_data, payload, cancelled=lambda: False):
    """Modern Unity channel layouts, 16-byte stream starts; little-endian PC payload."""
    count = integer(vertex_data.get("m_VertexCount"), 1, MAX_VERTICES, "vertex count")
    channels = vertex_data.get("m_Channels")
    if not isinstance(channels, list) or not 1 <= len(channels) <= 14 or len(payload) > MAX_BYTES:
        raise h.HeightmapImportError("Invalid mesh vertex channel table or payload size.")
    active, strides = [], [0] * 4
    for semantic, channel in enumerate(channels):
        if not isinstance(channel, dict) or set(channel) != {"stream", "offset", "format", "dimension"}:
            raise h.HeightmapImportError("Unqualified vertex channel schema.")
        dimension = integer(channel["dimension"], 0, 255, "channel dimension") & 15
        if dimension == 0:
            continue
        integer(dimension, 1, 4, "channel dimension")
        stream = integer(channel["stream"], 0, 3, "vertex stream")
        fmt = integer(channel["format"], 0, len(FORMATS)-1, "vertex format")
        offset = integer(channel["offset"], 0, 255, "channel offset")
        width = FORMATS[fmt][1] * dimension
        active.append((semantic, stream, offset, fmt, dimension, width))
        strides[stream] += width
    if not active or active[0][0] != 0:
        raise h.HeightmapImportError("Mesh has no explicit position channel.")
    starts, total = [], 0
    for stride in strides:
        starts.append(total)
        total = (total + count * stride + 15) & ~15
    if total > MAX_BYTES or total - len(payload) not in range(16):
        raise h.HeightmapImportError("Vertex stream payload length disagrees with its channel layout.")
    occupied = [set() for _ in strides]
    result = {}
    for semantic, stream, offset, fmt, dimension, width in active:
        if offset + width > strides[stream] or any(i in occupied[stream] for i in range(offset, offset+width)):
            raise h.HeightmapImportError("Overlapping or out-of-stride vertex channel.")
        occupied[stream].update(range(offset, offset+width))
        end = starts[stream] + (count-1)*strides[stream] + offset + width
        if end > len(payload):
            raise h.HeightmapImportError("Truncated vertex channel payload.")
        unpack = struct.Struct("<" + str(dimension) + FORMATS[fmt][0]).unpack_from
        rows = []
        for i in range(count):
            if i % 4096 == 0:
                h.check_cancelled(cancelled)
            values = unpack(payload, starts[stream] + i*strides[stream] + offset)
            if fmt in (2, 4):
                divisor = 255 if fmt == 2 else 65535
                values = tuple(v / divisor for v in values)
            elif fmt in (3, 5):
                divisor = 127 if fmt == 3 else 32767
                values = tuple(max(-1.0, v / divisor) for v in values)
            rows.append(values)
        result[semantic] = finite_rows(rows, count, (dimension,), str(semantic))
    if len(result[0][0]) != 3:
        raise h.HeightmapImportError("Position channel is not a three-component source vector.")
    return result


def packed_values(packet, floating=False, cancelled=lambda: False, max_items=MAX_INDICES):
    """Checked LSB-first packed vectors, preserving source quantization parameters."""
    count = integer(packet.get("m_NumItems"), 0, max_items, "packed value count")
    bits = integer(packet.get("m_BitSize"), 0, 32, "packed bit count")
    data = bytes(packet.get("m_Data", ()))
    if len(data) > MAX_BYTES or (count*bits+7)//8 > len(data):
        raise h.HeightmapImportError("Truncated or oversized packed mesh vector.")
    if not count:
        return []
    if floating:
        start, extent = packet.get("m_Start"), packet.get("m_Range")
        if (type(start) not in (float,int) or type(extent) not in (float,int)
                or not math.isfinite(start) or not math.isfinite(extent) or extent < 0 or bits == 0):
            raise h.HeightmapImportError("Unsupported packed float quantization.")
    result = []
    mask = (1 << bits) - 1
    for i in range(count):
        if i % 4096 == 0:
            h.check_cancelled(cancelled)
        first = i * bits
        byte, shift = divmod(first, 8)
        value = (int.from_bytes(data[byte:byte+(shift+bits+7)//8], "little") >> shift) & mask
        result.append(value / mask * extent + start if floating else value)
    return result


def compressed_channels(tree, cancelled=lambda: False):
    """Decode the inspected packed-vector layout; no absent channels are generated."""
    c = tree["m_CompressedMesh"]
    positions = packed_values(c["m_Vertices"], True, cancelled, MAX_VERTICES * 3)
    if len(positions) % 3:
        raise h.HeightmapImportError("Packed position count is not divisible by three.")
    count = integer(len(positions)//3, 1, MAX_VERTICES, "compressed vertex count")
    chunks = lambda data, size: [tuple(data[i:i+size]) for i in range(0, len(data), size)]
    result = {0: chunks(positions, 3)}
    uv = packed_values(c["m_UV"], True, cancelled, count * 8 * 4)
    info = integer(c["m_UVInfo"], 0, (1 << 32)-1, "packed UV layout")
    if uv:
        offset = 0
        if info == 0:
            if len(uv) not in (count*2, count*4):
                raise h.HeightmapImportError("Unsupported legacy packed UV channel count.")
            info = 5 if len(uv) == count*2 else 0x55
        for channel in range(8):
            field = (info >> (channel*4)) & 15
            if field & 4:
                dimension = (field & 3) + 1
                end = offset + count*dimension
                if end > len(uv):
                    raise h.HeightmapImportError("Packed UV layout exceeds its payload.")
                result[4+channel] = chunks(uv[offset:end], dimension)
                offset = end
            elif field:
                raise h.HeightmapImportError("Unsupported packed UV layout flags.")
        if offset != len(uv):
            raise h.HeightmapImportError("Packed UV payload has unaccounted values.")
    for semantic, name, sign_name in ((1,"m_Normals","m_NormalSigns"),(2,"m_Tangents","m_TangentSigns")):
        xy = packed_values(c[name], True, cancelled, count * 2)
        signs = packed_values(c[sign_name], False, cancelled, count * 2)
        sign_width = 1 if semantic == 1 else 2
        if not xy:
            if signs:
                raise h.HeightmapImportError("Packed signs exist without their source channel.")
            continue
        if len(xy) != count*2 or len(signs) != count*sign_width or any(v not in (0,1) for v in signs):
            raise h.HeightmapImportError("Packed normal/tangent values and signs disagree.")
        rows = []
        for i, (x,y) in enumerate(chunks(xy,2)):
            z2 = 1-x*x-y*y
            if z2 < 0:
                length = math.sqrt(x*x+y*y); x,y,z = x/length,y/length,0.
            else:
                z = math.sqrt(z2)
            if signs[i*sign_width] == 0:
                z = -z
            rows.append((x,y,z) if semantic == 1 else (x,y,z,1. if signs[i*2+1] else -1.))
        result[semantic] = rows
    colors = packed_values(c["m_FloatColors"], True, cancelled, count * 4)
    if colors:
        if len(colors) != count*4:
            raise h.HeightmapImportError("Packed color count differs from vertex count.")
        result[3] = chunks(colors,4)
    if any(c.get(name, {}).get("m_NumItems", 0) for name in ("m_Weights", "m_BoneIndices", "m_Colors")):
        raise h.HeightmapImportError("Packed skin or legacy color data requires a separate mapping.")
    for semantic, rows in result.items():
        finite_rows(rows,count,(len(rows[0]),),str(semantic))
    return result, packed_values(c["m_Triangles"], False, cancelled)


def triangles(submeshes, indices, vertex_count, index_format):
    if not isinstance(submeshes,list) or not 1 <= len(submeshes) <= MAX_SUBMESHES:
        raise h.HeightmapImportError("Invalid mesh submesh table.")
    integer(index_format,0,1,"index format")
    if not 0 < len(indices) <= MAX_INDICES:
        raise h.HeightmapImportError("Invalid mesh index buffer.")
    width = 2 if index_format == 0 else 4
    groups = []
    for s in submeshes:
        if s.get("topology") != 0:
            raise h.HeightmapImportError("Only explicit source triangle topology is qualified.")
        first = integer(s.get("firstByte"),0,MAX_INDICES*width,"submesh byte offset")
        count = integer(s.get("indexCount"),0,MAX_INDICES,"submesh index count")
        base = integer(s.get("baseVertex"),0,MAX_VERTICES,"base vertex")
        if first % width or count % 3 or first//width+count > len(indices):
            raise h.HeightmapImportError("Submesh index range is misaligned or truncated.")
        values = [integer(i,0,(1 << (width*8))-1,"vertex index")+base for i in indices[first//width:first//width+count]]
        if any(i >= vertex_count for i in values):
            raise h.HeightmapImportError("Source triangle references an absent vertex.")
        groups.append([tuple(values[i:i+3]) for i in range(0,len(values),3)])
    return groups


def stream_payload(obj, tree, members):
    """members is the explicit bundle member-name -> binary reader mapping."""
    stream = tree["m_StreamData"]
    if not stream["path"]:
        payload = bytes(tree["m_VertexData"]["m_DataSize"])
        if len(payload) > MAX_BYTES:
            raise h.HeightmapImportError("Inline mesh data exceeds its byte budget.")
        return payload, None
    name = str(obj.assets_file.name) + ".resS"
    if stream["path"] != "archive:/" + str(obj.assets_file.name) + "/" + name or name not in members:
        raise h.HeightmapImportError("Mesh vertex stream is not an explicit member of its source bundle.")
    resource = members[name]
    offset = integer(stream["offset"],0,resource.Length,"stream offset")
    size = integer(stream["size"],1,MAX_BYTES,"stream size")
    if offset+size > resource.Length:
        raise h.HeightmapImportError("Mesh stream exceeds its source member bounds.")
    previous = resource.Position
    try:
        resource.Position=offset; payload=resource.read_bytes(size)
    finally:
        resource.Position=previous
    if len(payload) != size:
        raise h.HeightmapImportError("Truncated external mesh stream.")
    return payload, {"member":name,"offset":offset,"size":size,"sha256":hashlib.sha256(payload).hexdigest()}


@dataclass
class SourceMesh:
    attributes: dict
    submeshes: list
    binding: dict
    compression: int


def read_mesh(obj, members, cancelled=lambda:False):
    if importlib.metadata.version("UnityPy") != "1.24.2" or obj.type.name != "Mesh" or obj.reader.endian != "<":
        raise h.HeightmapImportError("Mesh decoding requires a qualified little-endian Mesh record and UnityPy 1.24.2.")
    original = obj.get_raw_data(); tree = embedded_tree(obj,MAX_BYTES)
    if tree["m_BindPose"] or tree["m_Shapes"]["shapes"]:
        raise h.HeightmapImportError("Animated mesh channels require their own source mapping.")
    payload, stream = stream_payload(obj,tree,members)
    compression=integer(tree["m_MeshCompression"],0,3,"compression mode")
    if compression:
        if payload or tree["m_VertexData"]["m_VertexCount"]:
            raise h.HeightmapImportError("Mixed compressed and streamed mesh layout is not qualified.")
        attributes, indices = compressed_channels(tree, cancelled)
    else:
        if any(isinstance(packet,dict) and packet.get("m_NumItems",0) for packet in tree["m_CompressedMesh"].values()):
            raise h.HeightmapImportError("Packed values occur in an uncompressed mesh; mixed layout needs qualification.")
        attributes=vertex_channels(tree["m_VertexData"],payload,cancelled)
        raw=bytes(tree["m_IndexBuffer"]); width=2 if tree["m_IndexFormat"]==0 else 4
        if len(raw)>MAX_INDICES*width or len(raw)%width:
            raise h.HeightmapImportError("Mesh index payload is misaligned or exceeds its budget.")
        indices=[value[0] for value in struct.iter_unpack("<H" if width==2 else "<I",raw)]
    groups=triangles(tree["m_SubMeshes"],indices,len(attributes[0]),tree["m_IndexFormat"])
    h.check_cancelled(cancelled)
    if obj.get_raw_data()!=original:
        raise h.HeightmapImportError("Mesh source record changed during decoding.")
    binding={"serialized_file":str(obj.assets_file.name),"path_id":str(obj.path_id),
             "record_sha256":hashlib.sha256(original).hexdigest(),"stream":stream}
    return SourceMesh(attributes,groups,binding,compression)
