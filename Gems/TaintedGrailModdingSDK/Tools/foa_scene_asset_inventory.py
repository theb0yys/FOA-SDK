# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Resolve explicitly collected scene assets; no inference of missing renderers.

The result is a private source-binding manifest, not an editable scene contract.
A successful binding says nothing about native shaders or runtime appearance.
"""
from collections import defaultdict
import gc
import hashlib
import importlib.metadata
import json
from pathlib import Path
import time

import foa_heightmap_importer as h
from foa_scene_asset_binding import BundleAssets, SourceAssetReference
from foa_scene_asset_catalog import Catalog

MAX_REFERENCES = 16384
MAX_BUNDLES = 512
MAX_BUNDLE_BYTES = 64 * 1024 * 1024
MAX_TOTAL_BYTES = 3 * 1024 * 1024 * 1024
MAX_CATALOG_BYTES = 32 * 1024 * 1024
MAX_REPORT_BYTES = 32 * 1024 * 1024


def plan_references(references, catalog, addressables_root, cancelled=lambda: False):
    """Bound unique typed selectors before loading any asset bundle."""
    groups, seen = defaultdict(list), set()
    for count, ref in enumerate(references, 1):
        h.check_cancelled(cancelled)
        if count > MAX_REFERENCES or not isinstance(ref, SourceAssetReference):
            raise h.HeightmapImportError('Asset selector collection is invalid or exceeds its limit.')
        if ref in seen:
            continue
        seen.add(ref)
        location, path = ref.locate(catalog, addressables_root)
        groups[path].append((ref, location))
        if len(groups) > MAX_BUNDLES:
            raise h.HeightmapImportError('Asset bundle count exceeds its limit.')
    if not seen:
        raise h.HeightmapImportError('An empty asset selection cannot qualify source binding.')
    total = 0
    for path in groups:
        h.check_cancelled(cancelled)
        size = path.stat().st_size
        total += size
        if not 0 < size <= MAX_BUNDLE_BYTES or total > MAX_TOTAL_BYTES:
            raise h.HeightmapImportError('Selected asset bundle bytes exceed their limit.')
    return groups


def bind_references(game_root, references, output, timeout_seconds=420, progress=lambda _: None):
    """Resolve all requested records, rejecting the operation if any cannot bind."""
    if type(timeout_seconds) not in (int, float) or not 1 <= timeout_seconds <= 900:
        raise h.HeightmapImportError('Invalid asset binding timeout.')
    root, output = h.require_direct_path(Path(game_root)), h.require_direct_path(Path(output))
    if (output.exists() or output.is_relative_to(root) or root.is_relative_to(output.parent)
            or any((p / '.git').exists() for p in output.parents)):
        raise h.HeightmapImportError('Use a new private manifest outside the installation and source checkouts.')
    if importlib.metadata.version('UnityPy') != '1.24.2':
        raise h.HeightmapImportError('Source asset binding requires UnityPy 1.24.2.')
    started = time.monotonic()
    cancelled = lambda: time.monotonic() - started > timeout_seconds
    install = h.resolve_game_install(root)
    catalog_path = install.catalog_path
    before_catalog = h.sha256_file(catalog_path, cancelled, MAX_CATALOG_BYTES)
    catalog = Catalog(json.loads(catalog_path.read_text(encoding='utf-8')), cancelled)
    groups = plan_references(references, catalog, catalog_path.parent, cancelled)
    unity, _ = h.import_unitypy(h.UNITY_FALLBACK_VERSION)
    rows, sources = [], []
    for number, (path, selections) in enumerate(sorted(groups.items()), 1):
        h.check_cancelled(cancelled)
        before = h.sha256_file(path, cancelled, MAX_BUNDLE_BYTES)
        environment = unity.load(str(path))
        assets = BundleAssets(environment.objects, cancelled)
        for reference, location in selections:
            h.check_cancelled(cancelled)
            bound = reference.bind(assets, location)
            raw = bound.object_reader.get_raw_data()
            rows.append({'key': reference.key, 'source_bundle': path.name,
                         'bundle_path': path.relative_to(root).as_posix(),
                         'bundle_sha256': before[0], 'catalog_entry': location['entry_id'],
                         **bound.identity(), 'record_bytes': len(raw),
                         'record_sha256': hashlib.sha256(raw).hexdigest()})
        if h.sha256_file(path, cancelled, MAX_BUNDLE_BYTES) != before:
            raise h.HeightmapImportError('Selected source bundle changed during binding.')
        sources.append({'relative_path': path.relative_to(root).as_posix(),
                        'sha256': before[0], 'byte_size': before[1], 'unchanged': True})
        del assets, environment, bound, raw
        gc.collect()
        progress({'bundles_done': number, 'total': len(groups), 'resolved': len(rows)})
    # Check the entire consumed set again, including bundles released earlier.
    for source in sources:
        path = h.require_direct_path(root / source['relative_path'])
        if h.sha256_file(path, cancelled, MAX_BUNDLE_BYTES) != (source['sha256'], source['byte_size']):
            raise h.HeightmapImportError('Source inventory changed before manifest publication.')
    if h.sha256_file(catalog_path, cancelled, MAX_CATALOG_BYTES) != before_catalog:
        raise h.HeightmapImportError('Source catalog changed during binding.')
    result = {'schema': 'foa.campaign-source-assets', 'schema_version': 1, 'status': 'PARTIAL',
              'source_binding': 'PASSED', 'source_unchanged': True, 'resolved': rows, 'sources': sources,
              'catalog': {'sha256': before_catalog[0], 'byte_size': before_catalog[1], 'unchanged': True},
              'elapsed_seconds': round(time.monotonic() - started, 3),
              'renderer_coverage': 'NOT_RUN', 'geometry_projection': 'NOT_RUN',
              'native_scene_assembly': 'NOT_RUN', 'visual_parity': 'NOT_RUN', 'game_export': 'NOT_RUN'}
    encoded = json.dumps(result, indent=2, allow_nan=False).encode('utf-8')
    if len(encoded) > MAX_REPORT_BYTES:
        raise h.HeightmapImportError('Asset binding manifest exceeds its byte limit.')
    h.check_cancelled(cancelled)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as stream:
        stream.write(encoded)
    return result
