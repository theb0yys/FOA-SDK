#!/usr/bin/env python3
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Audit explicit local scene bundles without creating an editable reconstruction.

Only embedded type trees may decode source records. This diagnostic checks
source-record preservation, never guesses a component schema, and never writes
to the source. Its report is private evidence, not a scene interchange format.
"""
from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import asdict
import gc
import hashlib
import importlib.metadata
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
import foa_heightmap_importer as h
from foa_campaign_heightmap_export import SourceInventory
from foa_scene_component_audit import ComponentScriptAudit, FILE_NAME
from foa_scene_ownership import SceneOwnership
from foa_scene_navigation_audit import inspect_cache, MAX_CACHE_BYTES
from foa_scene_record_writer import null_reference_writer

SCENE_TYPES = frozenset({
    "GameObject", "Transform", "RectTransform", "MeshFilter", "MeshRenderer",
    "SkinnedMeshRenderer", "LODGroup", "Light", "LightmapSettings",
    "LightingSettings", "RenderSettings", "ReflectionProbe", "NavMeshSettings",
    "BoxCollider", "MeshCollider", "CapsuleCollider", "SphereCollider", "Rigidbody", "MonoBehaviour",
})
MAX_BUNDLE_BYTES = 256 * 1024 * 1024
MAX_TOTAL_BYTES = 768 * 1024 * 1024
MAX_OBJECTS = 500000
MAX_RECORD_BYTES = 4 * 1024 * 1024


def inspect_objects(objects, encode, cancelled=lambda: False, components=None, all_records=False):
    """Return bounded diagnostics. Uninterpreted records remain explicitly counted."""
    counts = Counter()
    checked = Counter()
    mismatches = Counter()
    unsupported = Counter()
    schemas = {}
    seen = set()
    identities = hashlib.sha256()
    nav_references = []
    lightmap_counts = []
    failures = []
    decode_failures = Counter()
    encode_failures = Counter()
    for index, obj in enumerate(objects):
        h.check_cancelled(cancelled)
        if index >= MAX_OBJECTS:
            raise h.HeightmapImportError("Scene object inventory exceeds its limit.")
        key = (str(obj.assets_file.name), int(obj.path_id))
        if key in seen:
            raise h.HeightmapImportError("Duplicate source object identity inside one serialized file.")
        seen.add(key)
        kind = obj.type.name
        identities.update(json.dumps([*key, kind], separators=(",", ":")).encode("utf-8") + b"\n")
        counts[kind] += 1
        if kind not in SCENE_TYPES and not all_records:
            unsupported[kind] += 1
            continue
        node = getattr(obj.serialized_type, "node", None)
        if node is None or obj.byte_size > MAX_RECORD_BYTES:
            unsupported[kind] += 1
            if len(failures) < 16:
                failures.append({"type": kind, "reason": "missing-embedded-schema-or-record-size-limit"})
            continue
        stage = "decode"
        tree = None
        try:
            tree = obj.read_typetree(nodes=node, check_read=True)
            schemas.setdefault(kind, sorted(tree))
            if kind == "MonoBehaviour" and components is not None:
                components.observe(obj, tree)
            if kind == "NavMeshSettings":
                nav_references.append(tree.get("m_NavMeshData"))
            if kind == "LightmapSettings":
                lightmap_counts.append(len(tree["m_Lightmaps"]) if isinstance(tree.get("m_Lightmaps"), list) else None)
            original = obj.get_raw_data()
            stage = "encode"
            encoded = encode(tree, node, obj)
            if encoded == original:
                checked[kind] += 1
            else:
                mismatches[kind] += 1
                if len(failures) < 16:
                    failures.append({"type": kind, "source_file": key[0], "path_id": str(key[1]),
                                     "reason": "re-encoded-record-differs", "source_bytes": len(original),
                                     "encoded_bytes": len(encoded)})
        except Exception as error:
            h.check_cancelled(cancelled)
            unsupported[kind] += 1
            (decode_failures if stage == "decode" else encode_failures)[kind] += 1
            if len(failures) < 16:
                failures.append({"type": kind, "source_file": key[0], "path_id": str(key[1]),
                                 "stage": stage, "reason": type(error).__name__, "detail": str(error)[:256],
                                 "script_reference": tree.get("m_Script") if isinstance(tree, dict) else None})
    return {"object_count": sum(counts.values()), "types": dict(sorted(counts.items())),
            "identity_inventory_sha256": identities.hexdigest(),
            "byte_identical_records": dict(sorted(checked.items())),
            "different_records": dict(sorted(mismatches.items())),
            "uninterpreted_records": dict(sorted(unsupported.items())),
            "observed_fields": schemas, "navmesh_data_references": nav_references,
            "lightmap_array_counts": lightmap_counts, "failures": failures,
            "decode_failures": dict(sorted(decode_failures.items())),
            "encode_failures": dict(sorted(encode_failures.items()))}


def validate_paths(source_root, bundles, output):
    source_root = h.require_direct_path(Path(source_root))
    output = h.require_direct_path(Path(output))
    if output.is_relative_to(source_root) or source_root.is_relative_to(output.parent):
        raise h.HeightmapImportError("Scene audit output must be separate from the source installation.")
    if any((p / ".git").exists() for p in (output.parent, *output.parents)):
        raise h.HeightmapImportError("Private scene evidence must remain outside source control.")
    if output.exists():
        raise h.HeightmapImportError("An audit report already exists; choose a new output.")
    if not 1 <= len(bundles) <= 32:
        raise h.HeightmapImportError("Select between 1 and 32 explicit scene bundles.")
    selected = []
    for value in bundles:
        path = h.require_direct_path(Path(value))
        if not path.is_relative_to(source_root) or not path.is_file() or path.suffix.lower() != ".bundle":
            raise h.HeightmapImportError("Each explicit bundle must be inside the source root.")
        if path.stat().st_size > MAX_BUNDLE_BYTES:
            raise h.HeightmapImportError("Scene bundle exceeds its byte limit.")
        selected.append(path)
    if len(set(selected)) != len(selected) or sum(p.stat().st_size for p in selected) > MAX_TOTAL_BYTES:
        raise h.HeightmapImportError("Duplicate bundles or source byte budget exceeded.")
    return source_root, selected, output


def audit(source_root, bundles, output, timeout_seconds=180, component_scripts=False, navigation_caches=(),
          all_records=False, primary_scene_files=()):
    source_root, selected, output = validate_paths(source_root, bundles, output)
    if (not isinstance(primary_scene_files, (tuple, list)) or len(primary_scene_files) > 32
            or any(not isinstance(name, str) or not FILE_NAME.fullmatch(name) for name in primary_scene_files)):
        raise h.HeightmapImportError("Select at most 32 explicit serialized scene file identities.")
    scene_files = {name.lower() for name in primary_scene_files}
    if len(scene_files) != len(primary_scene_files):
        raise h.HeightmapImportError("Duplicate primary scene file selection.")
    seen_scene_files = set()
    if importlib.metadata.version("UnityPy") != "1.24.2":
        raise h.HeightmapImportError("This audit requires UnityPy 1.24.2.")
    unity, _ = h.import_unitypy(h.UNITY_FALLBACK_VERSION)
    from UnityPy.helpers import TypeTreeHelper
    from UnityPy.streams import EndianBinaryWriter
    repaired_nulls = 0
    def encode(tree, node, obj):
        nonlocal repaired_nulls
        writer = EndianBinaryWriter(endian=obj.reader.endian)
        if obj.type.name == "MonoBehaviour":
            with null_reference_writer(TypeTreeHelper, "1.24.2") as repair:
                TypeTreeHelper.write_typetree(tree, node, writer, obj.assets_file)
                repaired_nulls += repair["null_references"]
        else:
            TypeTreeHelper.write_typetree(tree, node, writer, obj.assets_file)
        return writer.bytes
    start = time.monotonic()
    cancelled = lambda: time.monotonic() - start > timeout_seconds
    inventory = SourceInventory(source_root, cancelled)
    caches = []
    if len(navigation_caches) > 32:
        raise h.HeightmapImportError("Too many explicit navigation caches.")
    for value in navigation_caches:
        path = h.require_direct_path(Path(value))
        if not path.is_relative_to(source_root) or not path.is_file() or path.suffix.lower() != ".bytes":
            raise h.HeightmapImportError("Explicit navigation cache must be inside the source root.")
        if path in caches or path.stat().st_size > MAX_CACHE_BYTES:
            raise h.HeightmapImportError("Duplicate navigation cache or cache size limit exceeded.")
        caches.append(path)
        inventory.add(path)
    rows = []
    object_total = 0
    for path in selected:
        h.check_cancelled(cancelled)
        inventory.add(path)
        before = h.sha256_file(path, cancelled, MAX_BUNDLE_BYTES)
        environment = unity.load(str(path))
        objects = environment.objects
        object_total += len(objects)
        if object_total > MAX_OBJECTS:
            raise h.HeightmapImportError("Combined scene object inventory exceeds its limit.")
        assets = {str(obj.assets_file.name): obj.assets_file for obj in objects}
        components = None
        if component_scripts:
            resolver = h.CabDependencyResolver(unity, path.parent)
            loaded_paths = set()
            def load_dependency(name):
                h.check_cancelled(cancelled)
                cab = h.cab_id_from_value(name)
                if cab is None:
                    raise h.HeightmapImportError("Component dependency is not a CAB identity.")
                dependency = resolver.resolve(cab)
                if dependency in loaded_paths:
                    raise h.HeightmapImportError("Requested component file is absent from its dependency.")
                inventory.add(dependency)
                loaded_paths.add(dependency)
                return unity.load(str(dependency)).objects
            components = ComponentScriptAudit(objects, load_dependency, cancelled)
        row = inspect_objects(objects, encode, cancelled, components, all_records)
        if components is not None:
            row["component_scripts"] = components.report()
        selected_files = scene_files.intersection(name.lower() for name in assets)
        if selected_files:
            if seen_scene_files.intersection(selected_files):
                raise h.HeightmapImportError("Primary scene file occurs in more than one selected bundle.")
            row["scene_ownership"] = SceneOwnership(objects, sorted(selected_files), cancelled).report()
            seen_scene_files.update(selected_files)
        row.update({"bundle": path.relative_to(source_root).as_posix(), "source_sha256": before[0],
                    "source_bytes": before[1], "serialized_files": [
                        {"name": name, "declared_unity_version": str(asset.unity_version),
                         "external_files": [external.path for external in asset.externals]}
                        for name, asset in sorted(assets.items())]})
        if h.sha256_file(path, cancelled, MAX_BUNDLE_BYTES) != before:
            raise h.HeightmapImportError("Source bundle changed while auditing.")
        rows.append(row)
        print(json.dumps({"bundle": path.name, "objects": row["object_count"],
                          "byte_identical": sum(row["byte_identical_records"].values()),
                          "different": sum(row["different_records"].values()),
                          "uninterpreted": sum(row["uninterpreted_records"].values())}), flush=True)
        del environment, objects, assets, components
        gc.collect()
    if scene_files != seen_scene_files:
        raise h.HeightmapImportError("Selected primary scene file is absent from the explicit source bundles.")
    navigation = []
    for path in caches:
        h.check_cancelled(cancelled)
        with path.open("rb") as stream:
            data = stream.read(MAX_CACHE_BYTES + 1)
        entries = inspect_cache(data, cancelled)
        navigation.append({"source": path.relative_to(source_root).as_posix(),
                           "entries": [asdict(entry) for entry in entries],
                           "graph_decoding": "NOT_RUN", "rebuild": "NOT_RUN"})
        del data
    inventory.verify()
    report = {"schema": "foa.scene-source-audit", "schema_version": 1, "status": "PARTIAL",
              "scope": "Explicit scene bundles; source-record byte preservation only",
              "parser_version": "UnityPy 1.24.2", "container_version_fallback": h.UNITY_FALLBACK_VERSION,
              "record_schemas": "embedded-only; absent schemas are never inferred",
              "record_selection": "all-records" if all_records else "scene-component-types",
              "writer_adapter": {"name": "explicit-null-managed-reference-v1", "null_references": repaired_nulls},
              "source_files_unchanged": True, "elapsed_seconds": round(time.monotonic()-start, 3),
              "entity_projection": "NOT_RUN", "asset_dependency_resolution": "NOT_RUN",
              "game_round_trip": "NOT_RUN", "rows": rows,
              "component_script_resolution": "PARTIAL" if component_scripts else "NOT_RUN",
              "navigation_containers": navigation,
              "source_inventory": sorted(inventory.rows.values(), key=lambda row: row["relative_path"])}
    if scene_files:
        report["primary_scene_files"] = sorted(scene_files)
        report["scope"] = "Source-record preservation and explicit source ownership; no editable scene or export claim"
    encoded = json.dumps(report, indent=2, allow_nan=False).encode("utf-8")
    if len(encoded) > 16 * 1024 * 1024:
        raise h.HeightmapImportError("Audit report exceeds its byte limit.")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as stream:
        stream.write(encoded)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--bundle", required=True, action="append", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--all-records", action="store_true",
                        help="Check every embedded-schema record, including otherwise unmapped types.")
    parser.add_argument("--component-scripts", action="store_true",
                        help="Resolve component MonoScript references through bounded local CAB dependencies.")
    parser.add_argument("--navigation-cache", action="append", default=[], type=Path,
                        help="Inspect one explicit A* cache container; does not decode or rebuild graphs.")
    parser.add_argument("--scene-file", action="append", default=[],
                        help="Verify GameObject/component/Transform ownership in this exact serialized scene file; repeat for each primary file.")
    args = parser.parse_args()
    audit(args.source_root, args.bundle, args.output, component_scripts=args.component_scripts,
          navigation_caches=args.navigation_cache, all_records=args.all_records, primary_scene_files=args.scene_file)

if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
