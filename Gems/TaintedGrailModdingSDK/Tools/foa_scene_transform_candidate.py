# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Prepare one source-bound local-translation record candidate in memory.

No source mutation, bundle publication or runtime compatibility claim. The caller
must independently qualify the edited entity, its parent mapping and dependent
products before any game-return operation. Unsupported edits are not inferred.
"""
import copy
import hashlib
import importlib.metadata
import math
import re
import struct

import foa_heightmap_importer as h
from foa_scene_asset_binding import embedded_tree

MAX_TRANSFORM_BYTES = 1024 * 1024


def finite_vector(value):
    if (not isinstance(value, (list, tuple)) or len(value) != 3
            or any(type(v) not in (int, float) or (type(v) is float and not math.isfinite(v)) for v in value)):
        raise h.HeightmapImportError("Translation must contain exactly three finite numbers.")
    return tuple(value)


def translation_candidate(obj, binding, baseline_host_local, edited_host_local):
    """Host (x,y,z) maps to source (x,z,y); only m_LocalPosition can change."""
    if importlib.metadata.version("UnityPy") != "1.24.2":
        raise h.HeightmapImportError("Transform candidates require UnityPy 1.24.2.")
    if (not isinstance(binding, dict) or set(binding) != {"serialized_file", "path_id", "record_sha256"}
            or binding["serialized_file"] != str(obj.assets_file.name)
            or binding["path_id"] != str(obj.path_id)
            or not isinstance(binding["record_sha256"], str)
            or re.fullmatch(r"[0-9a-f]{64}", binding["record_sha256"]) is None):
        raise h.HeightmapImportError("Transform candidate has a missing or mismatched source identity.")
    if obj.type.name != "Transform" or not 0 < obj.byte_size <= MAX_TRANSFORM_BYTES:
        raise h.HeightmapImportError("Only a bounded Transform record can receive a translation candidate.")
    original = obj.get_raw_data()
    if len(original) != obj.byte_size or hashlib.sha256(original).hexdigest() != binding["record_sha256"]:
        raise h.HeightmapImportError("The source Transform changed since the editor binding was created.")
    baseline, edited = finite_vector(baseline_host_local), finite_vector(edited_host_local)
    tree = embedded_tree(obj, MAX_TRANSFORM_BYTES)
    position = tree.get("m_LocalPosition")
    if not isinstance(position, dict) or set(position) != {"x", "y", "z"}:
        raise h.HeightmapImportError("Unsupported source Transform position.")
    source_position = finite_vector(tuple(position[k] for k in "xyz"))
    if baseline != (source_position[0], source_position[2], source_position[1]):
        raise h.HeightmapImportError("Editor translation baseline does not match the bound source Transform.")
    node = obj.serialized_type.node
    fields = [child for child in node.m_Children if child.m_Name == "m_LocalPosition"]
    if (len(fields) != 1 or [(child.m_Name, child.m_Type) for child in fields[0].m_Children]
            != [(axis, "float") for axis in "xyz"]):
        raise h.HeightmapImportError("Source position is not the qualified embedded float32 vector schema.")
    from UnityPy.helpers import TypeTreeHelper
    from UnityPy.streams import EndianBinaryReader, EndianBinaryWriter

    def encode(value):
        writer = EndianBinaryWriter(endian=obj.reader.endian)
        TypeTreeHelper.write_typetree(value, node, writer, obj.assets_file)
        return writer.bytes

    if encode(tree) != original:
        raise h.HeightmapImportError("Source Transform does not re-encode exactly; edit refused.")
    result_tree = copy.deepcopy(tree)
    desired = (edited[0], edited[2], edited[1])
    try:
        stored = struct.unpack("<3f", struct.pack("<3f", *desired))
    except (OverflowError, struct.error) as error:
        raise h.HeightmapImportError("Edited position exceeds source float32 range.") from error
    if not all(math.isfinite(value) for value in stored):
        raise h.HeightmapImportError("Edited position exceeds source float32 range.")
    if edited != baseline and stored == source_position:
        raise h.HeightmapImportError("Translation is below the source position's float32 precision.")
    result_tree["m_LocalPosition"] = dict(zip("xyz", stored))
    candidate = original if edited == baseline else encode(result_tree)
    if len(candidate) != len(original):
        raise h.HeightmapImportError("Translation candidate changed the source record size.")
    decoded = TypeTreeHelper.read_typetree(node, EndianBinaryReader(candidate, endian=obj.reader.endian),
        as_dict=True, byte_size=len(candidate), check_read=True, assetsfile=obj.assets_file)
    if decoded != result_tree:
        raise h.HeightmapImportError("Translation candidate did not decode to the requested record.")
    changed_fields = [key for key in tree if decoded[key] != tree[key]]
    if changed_fields not in ([], ["m_LocalPosition"]):
        raise h.HeightmapImportError("Translation candidate changed an unrelated source field.")
    if obj.get_raw_data() != original:
        raise h.HeightmapImportError("Source reader was modified while preparing the candidate.")
    return candidate, {"status": "PASSED", "source_binding": dict(binding),
        "candidate_sha256": hashlib.sha256(candidate).hexdigest(), "changed_fields": changed_fields,
        "source_local_position": list(source_position), "candidate_local_position": list(stored),
        "float32_rounding_error": max(abs(a-b) for a,b in zip(desired, stored)),
        "source_record_unchanged": True, "dependent_products": "NOT_RUN", "game_export": "NOT_RUN"}
