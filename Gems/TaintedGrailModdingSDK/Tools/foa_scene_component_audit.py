#!/usr/bin/env python3
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Private scene diagnostics: resolve component scripts by serialized references.

This module does not identify components by their field names or display labels.
Resolution does not imply that a component has an editable O3DE/game mapping.
"""
from collections import Counter
import re

import foa_heightmap_importer as h

MAX_SCRIPT_TYPES = 4096
MAX_SCRIPT_BYTES = 1024 * 1024
FILE_NAME = re.compile(r"cab-[0-9a-f]{32}(?:\.sharedassets)?", re.IGNORECASE)


def reference_key(asset, pointer):
    if not isinstance(pointer, dict) or set(pointer) != {"m_FileID", "m_PathID"}:
        raise h.HeightmapImportError("Invalid serialized component reference.")
    file_id, path_id = pointer["m_FileID"], pointer["m_PathID"]
    if (type(file_id) is not int or type(path_id) is not int or file_id < 0
            or file_id > len(asset.externals) or not -(1 << 63) <= path_id < (1 << 63)):
        raise h.HeightmapImportError("Serialized component reference is out of range.")
    if path_id == 0:
        raise h.HeightmapImportError("Component script reference is null.")
    name = str(asset.name) if file_id == 0 else str(asset.externals[file_id - 1].path).rsplit("/", 1)[-1]
    if not FILE_NAME.fullmatch(name):
        raise h.HeightmapImportError("Unsupported serialized component file identity.")
    return name.lower(), path_id


class ComponentScriptAudit:
    """One scene environment, explicit bounded dependency loader, no ambient file search."""
    def __init__(self, objects, load_dependency, cancelled=lambda: False):
        self.files = {}
        self.register(objects)
        self.load_dependency = load_dependency
        self.cancelled = cancelled
        self.bindings = {}
        self.counts = Counter()
        self.unresolved = Counter()
        self.failures = []

    def register(self, objects):
        for obj in objects:
            asset = obj.assets_file
            name = str(asset.name).lower()
            previous = self.files.setdefault(name, asset)
            if previous is not asset:
                raise h.HeightmapImportError("Ambiguous serialized file identity in component dependencies.")

    def observe(self, obj, tree):
        h.check_cancelled(self.cancelled)
        try:
            key = reference_key(obj.assets_file, tree.get("m_Script"))
            if key not in self.bindings:
                if len(self.bindings) >= MAX_SCRIPT_TYPES:
                    raise h.HeightmapImportError("Component script inventory exceeds its limit.")
                # Cache failures too, so one bad shared dependency cannot cause thousands of loads.
                self.bindings[key] = None
                if key[0] not in self.files:
                    self.register(self.load_dependency(key[0]))
                target_file = self.files.get(key[0])
                target = target_file.objects.get(key[1]) if target_file is not None else None
                if target is None or target.type.name != "MonoScript":
                    raise h.HeightmapImportError("Component script target is missing or has the wrong type.")
                node = getattr(target.serialized_type, "node", None)
                if node is None or not 0 <= target.byte_size <= MAX_SCRIPT_BYTES:
                    raise h.HeightmapImportError("Component script lacks an embedded schema or exceeds its limit.")
                script = target.read_typetree(nodes=node, check_read=True)
                names = [script.get(k) for k in ("m_AssemblyName", "m_Namespace", "m_ClassName")]
                if (any(not isinstance(v, str) or len(v) > 1024 for v in names)
                        or not names[0] or not names[2]):
                    raise h.HeightmapImportError("Component script type identity is incomplete.")
                self.bindings[key] = {"serialized_file": key[0], "path_id": str(key[1]),
                                      "assembly": names[0], "namespace": names[1], "class": names[2]}
            if self.bindings[key] is None:
                self.unresolved["unresolved-script-reference"] += 1
            else:
                self.counts[key] += 1
        except Exception as error:
            h.check_cancelled(self.cancelled)
            self.unresolved["unresolved-script-reference"] += 1
            if len(self.failures) < 16:
                self.failures.append({"source_file": str(obj.assets_file.name), "path_id": str(obj.path_id),
                                      "reason": str(error)[:256]})

    def report(self):
        return {"resolved_components": sum(self.counts.values()),
                "unresolved_components": sum(self.unresolved.values()),
                "types": [{**self.bindings[key], "component_count": count}
                          for key, count in sorted(self.counts.items())],
                "failures": self.failures, "editable_mapping": "NOT_RUN"}
