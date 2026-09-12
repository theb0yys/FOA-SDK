# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Read-only exact-key lookup for the inspected Addressables catalog layout.

Locations retain dependency entry IDs instead of recursively duplicating graphs.
Only string keys are interpreted. No runtime property expression is executed.
"""
import base64
import struct
from pathlib import Path
import re

import foa_heightmap_importer as h

MAX_TABLE_BYTES = 32 * 1024 * 1024
MAX_RECORDS = 500000
MAX_EDGES = 4000000
MAX_STRING_BYTES = 16384
RUNTIME_PREFIX = "{UnityEngine.AddressableAssets.Addressables.RuntimePath}"


def table(document, name):
    value = document.get(name)
    if not isinstance(value, str) or len(value) > ((MAX_TABLE_BYTES + 2) // 3) * 4:
        raise h.HeightmapImportError("Catalog table is missing or exceeds its byte limit.")
    try:
        result = base64.b64decode(value, validate=True)
    except ValueError as error:
        raise h.HeightmapImportError("Catalog table is not valid base64.") from error
    if not 4 <= len(result) <= MAX_TABLE_BYTES:
        raise h.HeightmapImportError("Catalog table size is invalid.")
    return result


class Catalog:
    def __init__(self, document, cancelled=lambda: False):
        self.document = document
        self.lookup = {}
        keys = table(document, "m_KeyDataString")
        buckets = table(document, "m_BucketDataString")
        entries = table(document, "m_EntryDataString")
        key_count = struct.unpack_from("<i", keys)[0]
        count = struct.unpack_from("<i", buckets)[0]
        entry_count = struct.unpack_from("<i", entries)[0]
        if not 0 < count == key_count <= MAX_RECORDS or not 0 < entry_count <= MAX_RECORDS:
            raise h.HeightmapImportError("Catalog record counts are invalid.")
        if len(entries) != 4 + entry_count * 28:
            raise h.HeightmapImportError("Catalog location table is truncated or has trailing data.")
        self.keys = []
        self.buckets = []
        cursor, edges = 4, 0
        for _ in range(count):
            h.check_cancelled(cancelled)
            if cursor + 8 > len(buckets):
                raise h.HeightmapImportError("Catalog bucket header is truncated.")
            offset, length = struct.unpack_from("<ii", buckets, cursor)
            cursor += 8
            edges += length
            if not 0 <= length <= entry_count or edges > MAX_EDGES or cursor + length * 4 > len(buckets):
                raise h.HeightmapImportError("Catalog bucket references exceed their bounds.")
            refs = struct.unpack_from("<" + str(length) + "i", buckets, cursor)
            cursor += length * 4
            if any(index < 0 or index >= entry_count for index in refs) or len(set(refs)) != len(refs):
                raise h.HeightmapImportError("Invalid or duplicate catalog location reference.")
            if not 4 <= offset < len(keys):
                raise h.HeightmapImportError("Catalog key offset is invalid.")
            value = None
            if keys[offset] in (0, 1):
                if offset + 5 > len(keys):
                    raise h.HeightmapImportError("Catalog key header is truncated.")
                size = struct.unpack_from("<i", keys, offset + 1)[0]
                if not 0 <= size <= MAX_STRING_BYTES or offset + 5 + size > len(keys):
                    raise h.HeightmapImportError("Catalog string exceeds its bounds.")
                try:
                    value = keys[offset + 5:offset + 5 + size].decode("ascii" if keys[offset] == 0 else "utf-16-le")
                except UnicodeError as error:
                    raise h.HeightmapImportError("Invalid catalog string encoding.") from error
                if value in self.lookup:
                    raise h.HeightmapImportError("Ambiguous catalog string key.")
                self.lookup[value] = refs
            self.keys.append(value)
            self.buckets.append(refs)
        if cursor != len(buckets):
            raise h.HeightmapImportError("Catalog bucket table has trailing data.")
        for name in ("m_InternalIds", "m_ProviderIds", "m_resourceTypes"):
            if not isinstance(document.get(name), list) or not 0 < len(document[name]) <= MAX_RECORDS:
                raise h.HeightmapImportError("Catalog location metadata is invalid.")
        self.entries = list(struct.iter_unpack("<7i", entries[4:]))
        for internal, provider, dependency, _, extra, primary, resource in self.entries:
            h.check_cancelled(cancelled)
            if (not 0 <= internal < len(document["m_InternalIds"])
                    or not 0 <= provider < len(document["m_ProviderIds"])
                    or not -1 <= dependency < count or not 0 <= primary < count
                    or not 0 <= resource < len(document["m_resourceTypes"]) or extra < -1):
                raise h.HeightmapImportError("Catalog location metadata reference is out of range.")

    def location(self, index):
        if type(index) is not int or not 0 <= index < len(self.entries):
            raise h.HeightmapImportError("Invalid catalog location ID.")
        internal, provider, dependency, _, _, primary, resource = self.entries[index]
        value = self.document["m_InternalIds"][internal]
        if not isinstance(value, str) or len(value) > MAX_STRING_BYTES:
            raise h.HeightmapImportError("Invalid catalog internal ID.")
        prefixes = self.document.get("m_InternalIdPrefixes")
        if prefixes and "#" in value:
            number, suffix = value.rsplit("#", 1)
            if re.fullmatch(r"[0-9]+", number):
                index_value = int(number)
                if not isinstance(prefixes, list) or not 0 <= index_value < len(prefixes) or not isinstance(prefixes[index_value], str):
                    raise h.HeightmapImportError("Invalid catalog internal ID prefix.")
                value = prefixes[index_value] + suffix
        return {"entry_id": index, "internal_id": value, "provider": self.document["m_ProviderIds"][provider],
                "primary_key": self.keys[primary], "resource_type": self.document["m_resourceTypes"][resource],
                "dependency_entry_ids": list(self.buckets[dependency]) if dependency >= 0 else []}

    def locate(self, key):
        return [self.location(index) for index in self.lookup.get(key, ())]

    def locate_typed(self, key, class_name, assembly_name="UnityEngine.CoreModule"):
        """Resolve an exact catalog type; never choose another type or first match."""
        matches = [location for location in self.locate(key)
                   if isinstance(location["resource_type"], dict)
                   and location["resource_type"].get("m_ClassName") == class_name
                   and location["resource_type"].get("m_AssemblyName", "").split(",", 1)[0] == assembly_name]
        if len(matches) != 1:
            raise h.HeightmapImportError("Asset key does not resolve to exactly one requested resource type.")
        if matches[0]["provider"] != "UnityEngine.ResourceManagement.ResourceProviders.BundledAssetProvider":
            raise h.HeightmapImportError("Asset key uses an unsupported resource provider.")
        return matches[0]

    def main_bundle(self, location):
        # Installed BundledAssetProvider selects the first IAssetBundleResource,
        # while loading the remaining bundles as dependencies. Order is preserved.
        for index in location["dependency_entry_ids"]:
            dependency = self.location(index)
            resource = dependency["resource_type"]
            if (isinstance(resource, dict) and resource.get("m_ClassName")
                    == "UnityEngine.ResourceManagement.ResourceProviders.IAssetBundleResource"):
                if (resource.get("m_AssemblyName", "").split(",", 1)[0] != "Unity.ResourceManager"
                        or dependency["provider"] != "UnityEngine.ResourceManagement.ResourceProviders.AssetBundleProvider"):
                    raise h.HeightmapImportError("Unsupported source bundle resource provider or assembly.")
                return dependency
        raise h.HeightmapImportError("Asset location has no source bundle dependency.")


def local_bundle_path(addressables_root, internal_id):
    if not isinstance(internal_id, str) or not internal_id.startswith(RUNTIME_PREFIX + "/") and not internal_id.startswith(RUNTIME_PREFIX + "\\"):
        raise h.HeightmapImportError("Unsupported bundle runtime path; expressions are never evaluated.")
    relative = internal_id[len(RUNTIME_PREFIX) + 1:].replace("\\", "/")
    parts = relative.split("/")
    if not parts or any(part in ("", ".", "..") or ":" in part for part in parts):
        raise h.HeightmapImportError("Bundle path escapes its addressables root.")
    root = h.require_direct_path(Path(addressables_root))
    path = h.require_direct_path(root.joinpath(*parts))
    if not path.is_relative_to(root) or path.suffix.lower() != ".bundle" or not path.is_file():
        raise h.HeightmapImportError("Source bundle is absent or outside its addressables root.")
    return path
