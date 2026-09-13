# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Bounded source GameObject/component/hierarchy ownership, without spatial joins.

Explicit serialized scene files only. Shared prefab assets are not automatically
promoted to scene instances. This private diagnostic is not a scene interchange
schema, editable component mapping, or permission to export a transform edit.
"""
from collections import Counter
from dataclasses import dataclass

import foa_heightmap_importer as h
from foa_scene_asset_binding import embedded_tree
from foa_scene_component_audit import FILE_NAME, reference_key

MAX_RECORDS = 500000
MAX_COMPONENTS_PER_OBJECT = 4096
MAX_EDGES = 2000000
MAX_DECODED_SOURCE_BYTES = 256 * 1024 * 1024
MAX_DEPTH = 256


def identity(key):
    return {"serialized_file": key[0], "path_id": str(key[1])}


@dataclass(frozen=True)
class Component:
    key: tuple
    kind: str
    script: tuple | None


@dataclass(frozen=True)
class OwnedEntity:
    key: tuple
    transform: tuple
    components: tuple


@dataclass(frozen=True)
class TransformLinks:
    owner: tuple
    parent: tuple | None
    children: tuple


class SceneOwnership:
    """Validate reciprocal source references before exposing an ownership graph."""

    def __init__(self, objects, primary_files, cancelled=lambda: False):
        self.cancelled = cancelled
        self.objects = {}
        self.entities = {}
        self.transforms = {}
        self.component_owners = {}
        self.decoded_bytes = 0
        self.edge_count = 0
        files = {}
        for count, obj in enumerate(objects, 1):
            h.check_cancelled(cancelled)
            if count > MAX_RECORDS:
                raise h.HeightmapImportError("Scene ownership record budget exceeded.")
            name = str(obj.assets_file.name).lower()
            if not FILE_NAME.fullmatch(name):
                raise h.HeightmapImportError("Unsupported serialized scene file identity.")
            if files.setdefault(name, obj.assets_file) is not obj.assets_file:
                raise h.HeightmapImportError("Ambiguous serialized scene file identity.")
            if type(obj.path_id) is not int or not -(1 << 63) <= obj.path_id < (1 << 63) or obj.path_id == 0:
                raise h.HeightmapImportError("Invalid scene record path ID.")
            key = (name, obj.path_id)
            if key in self.objects:
                raise h.HeightmapImportError("Duplicate serialized scene record identity.")
            self.objects[key] = obj
        if (not isinstance(primary_files, (list, tuple)) or not 1 <= len(primary_files) <= 32
                or any(not isinstance(name, str) for name in primary_files)):
            raise h.HeightmapImportError("Select explicit primary scene file identities.")
        self.primary = frozenset(name.lower() for name in primary_files)
        if len(self.primary) != len(primary_files) or not self.primary <= files.keys():
            raise h.HeightmapImportError("Primary scene files are duplicated or absent.")
        self._build()

    def _tree(self, obj):
        h.check_cancelled(self.cancelled)
        self.decoded_bytes += obj.byte_size
        if self.decoded_bytes > MAX_DECODED_SOURCE_BYTES:
            raise h.HeightmapImportError("Scene ownership decode byte budget exceeded.")
        return embedded_tree(obj, 4 * 1024 * 1024)

    def _pointer(self, obj, pointer, nullable=False):
        # A null parent still needs the complete, valid PPtr representation.
        if (nullable and isinstance(pointer, dict) and set(pointer) == {"m_FileID", "m_PathID"}
                and type(pointer["m_FileID"]) is int and type(pointer["m_PathID"]) is int
                and pointer["m_FileID"] == 0 and pointer["m_PathID"] == 0):
            return None
        self.edge_count += 1
        if self.edge_count > MAX_EDGES:
            raise h.HeightmapImportError("Scene ownership reference budget exceeded.")
        key = reference_key(obj.assets_file, pointer)
        if key not in self.objects:
            raise h.HeightmapImportError("Scene ownership reference target is not explicitly loaded.")
        return key

    def _build(self):
        for key, obj in self.objects.items():
            h.check_cancelled(self.cancelled)
            if key[0] not in self.primary or obj.type.name != "GameObject":
                continue
            tree = self._tree(obj)
            refs = tree.get("m_Component")
            if not isinstance(refs, list) or not 1 <= len(refs) <= MAX_COMPONENTS_PER_OBJECT:
                raise h.HeightmapImportError("Scene GameObject has an invalid component list.")
            components, transform_ids = [], []
            for entry in refs:
                if not isinstance(entry, dict) or set(entry) != {"component"}:
                    raise h.HeightmapImportError("Unsupported GameObject component reference schema.")
                target = self._pointer(obj, entry["component"])
                if target in self.component_owners:
                    raise h.HeightmapImportError("A component is duplicated or claimed by multiple GameObjects.")
                component = self.objects[target]
                ct = self._tree(component)
                if self._pointer(component, ct.get("m_GameObject")) != key:
                    raise h.HeightmapImportError("Component owner disagrees with the GameObject component list.")
                self.component_owners[target] = key
                script = reference_key(component.assets_file, ct.get("m_Script")) if component.type.name == "MonoBehaviour" else None
                components.append(Component(target, component.type.name, script))
                if component.type.name in ("Transform", "RectTransform"):
                    transform_ids.append(target)
                    parent = self._pointer(component, ct.get("m_Father"), nullable=True)
                    child_refs = ct.get("m_Children")
                    if not isinstance(child_refs, list) or len(child_refs) > MAX_RECORDS:
                        raise h.HeightmapImportError("Invalid source Transform child list.")
                    children = tuple(self._pointer(component, p) for p in child_refs)
                    if len(set(children)) != len(children):
                        raise h.HeightmapImportError("Duplicate source Transform child reference.")
                    self.transforms[target] = TransformLinks(key, parent, children)
            if len(transform_ids) != 1:
                raise h.HeightmapImportError("Each source GameObject needs exactly one Transform.")
            self.entities[key] = OwnedEntity(key, transform_ids[0], tuple(components))
        if not self.entities:
            raise h.HeightmapImportError("Explicit scene files contain no owned GameObjects.")
        # Every Transform in selected scene files must belong to an accounted-for GameObject.
        for key, obj in self.objects.items():
            if key[0] in self.primary and obj.type.name in ("Transform", "RectTransform") and key not in self.transforms:
                raise h.HeightmapImportError("Source scene contains an unowned Transform.")
        self.ownerless_records = Counter()
        for key, obj in self.objects.items():
            h.check_cancelled(self.cancelled)
            if key[0] not in self.primary or key in self.component_owners:
                continue
            node = getattr(obj.serialized_type, "node", None)
            fields = getattr(node, "m_Children", ())
            if not any(field.m_Name == "m_GameObject" and field.m_Type == "PPtr<GameObject>" for field in fields):
                continue
            tree = self._tree(obj)
            if self._pointer(obj, tree.get("m_GameObject"), nullable=True) is not None:
                raise h.HeightmapImportError("A source component names an owner but is absent from its component list.")
            self.ownerless_records[obj.type.name] += 1
        incoming = Counter()
        for key, links in self.transforms.items():
            h.check_cancelled(self.cancelled)
            if links.parent is not None:
                parent = self.transforms.get(links.parent)
                if parent is None:
                    raise h.HeightmapImportError("Transform parent is outside the qualified scene ownership graph.")
            for child in links.children:
                if child not in self.transforms or self.transforms[child].parent != key:
                    raise h.HeightmapImportError("Transform parent and child references disagree.")
                incoming[child] += 1
        for key, links in self.transforms.items():
            if incoming[key] != (0 if links.parent is None else 1):
                raise h.HeightmapImportError("Transform child is missing or repeated in its parent list.")
        # Linear traversal also rejects reciprocal cycles and excessive hierarchy depth.
        stack = [(key, 0) for key, links in self.transforms.items() if links.parent is None]
        visited = set()
        self.max_depth = 0
        while stack:
            h.check_cancelled(self.cancelled)
            key, depth = stack.pop()
            if key in visited or depth > MAX_DEPTH:
                raise h.HeightmapImportError("Scene hierarchy cycle or depth limit exceeded.")
            visited.add(key)
            self.max_depth = max(self.max_depth, depth)
            stack.extend((child, depth + 1) for child in self.transforms[key].children)
        if len(visited) != len(self.transforms):
            raise h.HeightmapImportError("Scene hierarchy contains a parent cycle.")

    def affected_entities(self, transform_key):
        """Source owners moved by this Transform, including descendants, not bake dependencies."""
        if transform_key not in self.transforms:
            raise h.HeightmapImportError("Edited Transform is not in the qualified ownership graph.")
        result, stack = [], [transform_key]
        while stack:
            h.check_cancelled(self.cancelled)
            key = stack.pop()
            links = self.transforms[key]
            result.append(self.entities[links.owner])
            stack.extend(reversed(links.children))
        return tuple(result)

    def report(self):
        kinds = Counter(c.kind for entity in self.entities.values() for c in entity.components)
        return {"status": "PASSED", "scope": "Reciprocal GameObject/component/Transform ownership only",
                "primary_files": sorted(self.primary), "entities": len(self.entities),
                "transforms": len(self.transforms), "components": len(self.component_owners),
                "component_types": dict(sorted(kinds.items())), "max_depth": self.max_depth,
                "ownerless_component_records": dict(sorted(self.ownerless_records.items())),
                "roots": sum(links.parent is None for links in self.transforms.values()),
                "decoded_source_bytes": self.decoded_bytes, "reference_edges": self.edge_count,
                "component_semantics": "NOT_RUN", "derived_product_impact": "NOT_RUN", "game_export": "NOT_RUN"}
