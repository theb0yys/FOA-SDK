# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Read one stored rendering setting using an exact, externally generated schema.

The schema must be emitted by the qualified synthetic Unity settings fixture.
No ambient class-database fallback, runtime-setting claim, file write or game
launch occurs here. The complete raw record remains the preservation authority.
"""
import hashlib
import importlib.metadata
import json
import struct

from foa_heightmap_importer import HeightmapImportError

UNITY_VERSION = '6000.0.64f1'
TYPE_HASH = bytes.fromhex('0553c5897ec26a70a52a6124e5a5a28b')
SCHEMA_HASH = '89cdbe54a0ae36252248ccbd19ad56861106a360efe199ee8ba0ff3ee72b6a3c'
MAX_RECORD_BYTES = 64 * 1024


def require(value, message):
    if not value:raise HeightmapImportError(message)


def schema_hash(node):
    require(node is not None, 'An explicit generated PlayerSettings schema is required.')
    rows=[]
    for n in node.traverse():
        require(len(rows)<512, 'Settings schema exceeds its node bound.')
        rows.append({k:v for k,v in n.to_dict().items() if k!='m_Children'})
    return hashlib.sha256(json.dumps(rows,sort_keys=True,separators=(',',':')).encode()).hexdigest()


def stored_color_space(source, schema_source):
    """Return the stored build color space, not the live renderer's active state.

    Both exact-profile native writer cases and the inspected game record have
    four zero bytes after the schema-described prefix. They are explicitly
    retained as an opaque suffix, not interpreted as padding or discarded.
    Any different suffix or non-identical in-memory prefix roundtrip rejects.
    This qualifies only the named scalar; whole-record interpretation is partial.
    """
    from UnityPy.helpers import TypeTreeHelper
    from UnityPy.streams import EndianBinaryReader,EndianBinaryWriter
    require(importlib.metadata.version('UnityPy')=='1.24.2', 'Unqualified settings reader version.')
    for obj in (source,schema_source):
        require(obj.class_id==129 and obj.type.name=='PlayerSettings', 'Expected an explicit PlayerSettings record.')
        require(obj.assets_file.unity_version==UNITY_VERSION and obj.assets_file.target_platform==19 and
                obj.assets_file.reader.endian=='<', 'Unqualified settings source profile.')
        require(obj.serialized_type.old_type_hash==TYPE_HASH, 'PlayerSettings type hash differs from the native fixture.')
        require(type(obj.byte_size) is int and 4<=obj.byte_size<=MAX_RECORD_BYTES, 'Settings record exceeds its bound.')
    node=schema_source.serialized_type.node
    require(schema_hash(node)==SCHEMA_HASH, 'Generated settings schema content differs from the native fixture.')
    raw=source.get_raw_data()
    require(type(raw) is bytes and len(raw)==source.byte_size, 'Settings record byte count changed.')
    reader=EndianBinaryReader(raw,endian='<')
    config=TypeTreeHelper.TypeTreeConfig(True,source.assets_file,False)
    # Explicitly check consumed bytes and preserve the known opaque suffix.
    # A permissive full-record read must not hide the native four-byte discrepancy.
    value=TypeTreeHelper.read_value(node,reader,config)
    consumed=reader.Position
    require(consumed==len(raw)-4 and raw[consumed:]==bytes(4), 'Unqualified settings suffix or record framing.')
    writer=EndianBinaryWriter(endian='<')
    TypeTreeHelper.write_typetree(value,node,writer,source.assets_file)
    require(writer.bytes==raw[:consumed], 'Settings schema prefix does not roundtrip exactly.')
    require(type(value.get('m_ActiveColorSpace')) is int and value['m_ActiveColorSpace'] in (0,1), 'Unknown stored color-space value.')
    reader.Position=0;offset=None
    for child in node.m_Children:
        if child.m_Name=='m_ActiveColorSpace':
            offset=reader.Position;break
        TypeTreeHelper.read_value(child,reader,config)
    require(offset is not None and struct.unpack_from('<I',raw,offset)[0]==value['m_ActiveColorSpace'],
            'Stored color-space field offset disagrees with the generated schema.')
    require(source.get_raw_data()==raw, 'Settings record changed while reading.')
    return {'status':'PASSED','scope':'stored-build-color-space-only','unity_version':UNITY_VERSION,
            'stored_color_space':('Gamma','Linear')[value['m_ActiveColorSpace']],
            'stored_value':value['m_ActiveColorSpace'],'field_offset':offset,
            'record_sha256':hashlib.sha256(raw).hexdigest(),'record_bytes':len(raw),
            'schema_sha256':SCHEMA_HASH,'type_hash':TYPE_HASH.hex(),
            'schema_prefix_bytes':consumed,'opaque_suffix_hex':raw[consumed:].hex(),
            'whole_record_interpretation':'PARTIAL','live_runtime_color_space':'NOT_RUN'}
