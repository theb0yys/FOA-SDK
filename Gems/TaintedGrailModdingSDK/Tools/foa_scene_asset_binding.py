# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Bind an explicit Addressables subobject to its actual bundle container PPtr.

Preload dependencies, labels, collider names and nearby objects are not candidates.
This resolver does not decode geometry or confer an editable runtime mapping.
"""
from dataclasses import dataclass

import foa_heightmap_importer as h
from foa_scene_component_audit import reference_key

MAX_OBJECTS = 500000
MAX_CONTAINER_BYTES = 16 * 1024 * 1024
MAX_ASSET_BYTES = 64 * 1024 * 1024
MAX_CONTAINER_ENTRIES = 500000


def embedded_tree(obj, limit=MAX_ASSET_BYTES):
    node = getattr(obj.serialized_type, "node", None)
    if node is None or not 0 <= obj.byte_size <= limit:
        raise h.HeightmapImportError("Source asset lacks an embedded schema or exceeds its record limit.")
    return obj.read_typetree(nodes=node, check_read=True)


@dataclass(frozen=True)
class BoundSubobject:
    serialized_file: str
    path_id: int
    class_name: str
    name: str
    object_reader: object

    def identity(self):
        return {"serialized_file": self.serialized_file, "path_id": str(self.path_id),
                "class_name": self.class_name, "subobject_name": self.name}


class BundleAssets:
    """Index one explicitly selected source bundle, with no ambient dependency search."""
    def __init__(self, objects, cancelled=lambda: False):
        self.files, self.containers, self.names = {}, {}, {}
        self.cancelled = cancelled
        count, entries = 0, 0
        for obj in objects:
            h.check_cancelled(cancelled)
            count += 1
            if count > MAX_OBJECTS:
                raise h.HeightmapImportError("Source bundle object inventory exceeds its limit.")
            file = obj.assets_file
            name = str(file.name).lower()
            if self.files.setdefault(name, file) is not file:
                raise h.HeightmapImportError("Ambiguous serialized file identity in asset bundle.")
            if obj.type.name != "AssetBundle":
                continue
            tree = embedded_tree(obj, MAX_CONTAINER_BYTES)
            container = tree.get("m_Container")
            if not isinstance(container, list):
                raise h.HeightmapImportError("Asset bundle container is absent or malformed.")
            entries += len(container)
            if entries > MAX_CONTAINER_ENTRIES:
                raise h.HeightmapImportError("Asset bundle container exceeds its entry limit.")
            for key, info in container:
                h.check_cancelled(cancelled)
                if not isinstance(key, str) or not key or len(key) > 16384 or not isinstance(info, dict):
                    raise h.HeightmapImportError("Asset bundle container entry is malformed.")
                target = reference_key(file, info.get("asset"))
                self.containers.setdefault(key, set()).add(target)

    def resolve(self, internal_id, class_name, subobject_name):
        if (not isinstance(internal_id, str) or not isinstance(class_name, str)
                or (subobject_name is not None and (not isinstance(subobject_name, str) or len(subobject_name) > 16384))):
            raise h.HeightmapImportError("Source subobject selector is malformed.")
        matches = []
        # Container keys are matched exactly. Unsupported casing/aliases fail visibly.
        for file_name, path_id in sorted(self.containers.get(internal_id, ())):
            h.check_cancelled(self.cancelled)
            file = self.files.get(file_name)
            obj = file.objects.get(path_id) if file is not None else None
            if obj is None:
                raise h.HeightmapImportError("Asset container points outside the explicitly loaded source bundle.")
            if obj.type.name != class_name:
                continue
            key = (file_name, path_id)
            if key not in self.names:
                name = embedded_tree(obj).get("m_Name")
                if not isinstance(name, str) or len(name) > 16384:
                    raise h.HeightmapImportError("Source subobject name is malformed.")
                self.names[key] = name
            if subobject_name is None or self.names[key] == subobject_name:
                matches.append(BoundSubobject(file_name, path_id, class_name, self.names[key], obj))
        if len(matches) != 1:
            raise h.HeightmapImportError("Asset container does not identify exactly one requested subobject.")
        return matches[0]


@dataclass(frozen=True)
class SourceAssetReference:
    """Exact typed Addressables selector; a main asset has no subobject name."""
    class_name: str
    guid: str
    subobject_name: str | None

    def __post_init__(self):
        import re
        if (self.class_name not in ('Mesh', 'Material') or not isinstance(self.guid, str)
                or re.fullmatch(r'[0-9a-f]{32}', self.guid) is None
                or self.subobject_name is not None and (not isinstance(self.subobject_name, str)
                    or not 0 < len(self.subobject_name) <= 16384 or '\0' in self.subobject_name)):
            raise h.HeightmapImportError('Malformed typed source asset reference.')

    @classmethod
    def serialized(cls, class_name, guid, subobject_name):
        if not isinstance(subobject_name, str):
            raise h.HeightmapImportError('Serialized subobject selector must be a string.')
        return cls(class_name, guid, subobject_name if subobject_name else None)

    @classmethod
    def runtime_key(cls, class_name, key):
        import re
        if not isinstance(key, str) or len(key) > 16418:
            raise h.HeightmapImportError('Runtime asset key exceeds its bounds.')
        match = re.fullmatch(r'([0-9a-f]{32})(?:\[(.+)\])?', key)
        if match is None:
            raise h.HeightmapImportError('Unsupported exact source asset key.')
        return cls(class_name, *match.groups())

    @property
    def key(self):
        return self.guid if self.subobject_name is None else self.guid + '[' + self.subobject_name + ']'

    def locate(self, catalog, addressables_root):
        from foa_scene_asset_catalog import local_bundle_path
        location = catalog.locate_typed(self.guid, 'UnityEngine.' + self.class_name)
        bundle = local_bundle_path(addressables_root, catalog.main_bundle(location)['internal_id'])
        return location, bundle

    def bind(self, assets, location):
        resource = location.get('resource_type')
        if (not isinstance(resource, dict) or resource.get('m_ClassName') != 'UnityEngine.' + self.class_name
                or resource.get('m_AssemblyName', '').split(',', 1)[0] != 'UnityEngine.CoreModule'
                or location.get('provider') != 'UnityEngine.ResourceManagement.ResourceProviders.BundledAssetProvider'):
            raise h.HeightmapImportError('Source asset location has the wrong type or provider.')
        return assets.resolve(location['internal_id'], self.class_name, self.subobject_name)
