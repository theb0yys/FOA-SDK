# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Resolve campaign scene cohorts through source references, without name/spatial joins.

This is the importer's source-selection layer. Its private manifest does not mean
that geometry, materials, runtime load rules or an editable scene are qualified.
"""
import argparse
import gc
import importlib.metadata
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
import foa_heightmap_importer as h
from foa_campaign_heightmap_export import SourceInventory
from foa_scene_asset_binding import embedded_tree
from foa_scene_asset_catalog import Catalog, local_bundle_path
from foa_scene_component_audit import ComponentScriptAudit, FILE_NAME, reference_key
from foa_scene_ownership import SceneOwnership

SCENE_CLASS = 'UnityEngine.ResourceManagement.ResourceProviders.SceneInstance'
LOADER_TYPE = ('TG.Main', 'Awaken.TG.Main.Scenes.SceneConstructors.SubdividedScenes', 'SubdividedScene')
MAX_SCENES = 32
MAX_OBJECTS = 500000
MAX_CATALOG_BYTES = 32 * 1024 * 1024
MAX_REPORT_BYTES = 16 * 1024 * 1024
GUID = re.compile(r'[0-9a-f]{32}')


def primary_scene_file(objects, internal_id, cancelled=lambda: False):
    """Use AssetBundle.m_SceneHashes; the scene container PPtr is normally null."""
    files, matches, count = {}, [], 0
    for obj in objects:
        h.check_cancelled(cancelled)
        count += 1
        if count > MAX_OBJECTS:
            raise h.HeightmapImportError('Scene bundle object budget exceeded.')
        name = str(obj.assets_file.name).lower()
        if not FILE_NAME.fullmatch(name):
            raise h.HeightmapImportError('Invalid serialized scene file identity.')
        if files.setdefault(name, obj.assets_file) is not obj.assets_file:
            raise h.HeightmapImportError('Ambiguous serialized scene file identity.')
        if obj.type.name != 'AssetBundle':
            continue
        tree = embedded_tree(obj, 16 * 1024 * 1024)
        if tree.get('m_IsStreamedSceneAssetBundle') is not True:
            raise h.HeightmapImportError('Selected source is not a streamed scene bundle.')
        hashes, container = tree.get('m_SceneHashes'), tree.get('m_Container')
        if not isinstance(hashes, list) or not 1 <= len(hashes) <= MAX_SCENES or not isinstance(container, list):
            raise h.HeightmapImportError('Missing or invalid scene binding tables.')
        if len(container) > MAX_SCENES:
            raise h.HeightmapImportError('Scene container budget exceeded.')
        keys = []
        for entry in container:
            if not isinstance(entry, (tuple, list)) or len(entry) != 2 or not isinstance(entry[0], str):
                raise h.HeightmapImportError('Malformed scene container entry.')
            keys.append(entry[0])
        seen = set()
        for entry in hashes:
            if (not isinstance(entry, (tuple, list)) or len(entry) != 2
                    or not isinstance(entry[0], str) or not isinstance(entry[1], str)
                    or not FILE_NAME.fullmatch(entry[1]) or entry[1].lower().endswith('.sharedassets')):
                raise h.HeightmapImportError('Malformed primary scene binding.')
            scene, file_name = entry
            if scene in seen:
                raise h.HeightmapImportError('Duplicate source scene hash key.')
            seen.add(scene)
            if scene == internal_id:
                if keys.count(scene) != 1:
                    raise h.HeightmapImportError('Scene hash and container do not agree exactly.')
                matches.append(file_name.lower())
    if len(matches) != 1 or matches[0] not in files:
        raise h.HeightmapImportError('Scene key does not bind exactly one loaded primary scene file.')
    return matches[0]


def loader_scene_references(tree):
    """Keep serialized load roles/order. Do not evaluate the runtime node policy."""
    def guid(value):
        if not isinstance(value, dict) or set(value) != {'reference'}:
            raise h.HeightmapImportError('Unsupported scene reference wrapper.')
        ref = value['reference']
        if (not isinstance(ref, dict) or set(ref) != {'address', 'subObjectName'}
                or not isinstance(ref['address'], str) or not GUID.fullmatch(ref['address'])
                or ref['subObjectName'] != ''):
            raise h.HeightmapImportError('Scene reference needs an exact GUID and no subobject.')
        return ref['address']
    result = [{'role': 'map-static', 'key': guid(tree.get('mapStaticScene'))}]
    static = tree.get('staticSubscenes')
    data = tree.get('serializedSubscenesData')
    if (not isinstance(static, list) or not isinstance(data, dict)
            or not isinstance(data.get('Scenes'), list) or not isinstance(data.get('Nodes'), list)):
        raise h.HeightmapImportError('Subscene collections are missing or malformed.')
    if 2 + len(static) + len(data['Scenes']) > MAX_SCENES or len(data['Nodes']) > MAX_SCENES:
        raise h.HeightmapImportError('Subscene cohort exceeds its diagnostic budget.')
    for index, ref in enumerate(static):
        result.append({'role': 'static-subscene', 'ordinal': index, 'key': guid(ref)})
    for index, item in enumerate(data['Scenes']):
        if not isinstance(item, dict):
            raise h.HeightmapImportError('Malformed dynamic subscene record.')
        if any(type(item.get(name)) is not int or item[name] < 0 for name in ('id', 'stableUniqueId')):
            raise h.HeightmapImportError('Invalid dynamic subscene identity.')
        result.append({'role': 'dynamic-subscene', 'ordinal': index, 'id': item['id'],
                       'stable_unique_id': item['stableUniqueId'], 'key': guid(item.get('reference'))})
    # Duplicated scene loads need explicit runtime semantics before assembly.
    keys = [row['key'] for row in result]
    if len(set(keys)) != len(keys):
        raise h.HeightmapImportError('Scene loader repeats a scene GUID; load semantics need qualification.')
    return result


def bind_scene(catalog, addressables, key):
    location = catalog.locate_typed(key, SCENE_CLASS, 'Unity.ResourceManager')
    path = local_bundle_path(addressables, catalog.main_bundle(location)['internal_id'])
    return location, path


def resolve_cohort(game_root, map_key, output, timeout_seconds=180):
    if map_key not in h.CAMPAIGN_MAPS:
        raise h.HeightmapImportError('Unknown campaign map.')
    if type(timeout_seconds) not in (int, float) or not 1 <= timeout_seconds <= 900:
        raise h.HeightmapImportError('Invalid source-selection timeout.')
    root = h.require_direct_path(Path(game_root))
    output = h.require_direct_path(Path(output))
    if (output.exists() or output.is_relative_to(root) or root.is_relative_to(output.parent)
            or any((p / '.git').exists() for p in output.parents)):
        raise h.HeightmapImportError('Use a new private manifest outside the installation and source checkouts.')
    if importlib.metadata.version('UnityPy') != '1.24.2':
        raise h.HeightmapImportError('This source selection requires UnityPy 1.24.2.')
    start = time.monotonic()
    cancelled = lambda: time.monotonic() - start > timeout_seconds
    inventory = SourceInventory(root, cancelled)
    install = h.resolve_game_install(root)
    addressables = install.catalog_path.parent
    if install.catalog_path.stat().st_size > MAX_CATALOG_BYTES:
        raise h.HeightmapImportError('Catalog byte budget exceeded.')
    inventory.add(install.catalog_path)
    catalog = Catalog(json.loads(install.catalog_path.read_text(encoding='utf-8')), cancelled)
    unity, _ = h.import_unitypy(h.UNITY_FALLBACK_VERSION)
    root_key = h.CAMPAIGN_MAPS[map_key].addressable_keys[0]
    location, bundle = bind_scene(catalog, addressables, root_key)
    inventory.add(bundle)
    environment = unity.load(str(bundle))
    objects = tuple(environment.objects)
    primary = primary_scene_file(objects, location['internal_id'], cancelled)
    resolver = h.CabDependencyResolver(unity, install.bundle_root)
    loaded = set()
    def load_dependency(name):
        cab = h.cab_id_from_value(name)
        if cab is None:
            raise h.HeightmapImportError('Unsupported loader script file identity.')
        path = resolver.resolve(cab)
        if path in loaded:
            raise h.HeightmapImportError('Requested loader script file is absent from its bundle.')
        inventory.add(path)
        loaded.add(path)
        return unity.load(str(path)).objects
    scripts = ComponentScriptAudit(objects, load_dependency, cancelled)
    loaders = []
    for obj in objects:
        h.check_cancelled(cancelled)
        if str(obj.assets_file.name).lower() != primary or obj.type.name != 'MonoBehaviour':
            continue
        tree = embedded_tree(obj, 4 * 1024 * 1024)
        scripts.observe(obj, tree)
        identity = scripts.bindings.get(reference_key(obj.assets_file, tree.get('m_Script')))
        if identity and tuple(identity[k] for k in ('assembly', 'namespace', 'class')) == LOADER_TYPE:
            if type(tree.get('m_Enabled')) is not int or tree['m_Enabled'] != 1:
                raise h.HeightmapImportError('Campaign scene loader is disabled or malformed.')
            loaders.append((obj, tree, identity))
    if scripts.report()['unresolved_components'] or len(loaders) != 1:
        raise h.HeightmapImportError('Campaign needs one exact, resolved SubdividedScene loader.')
    loader, loader_tree, loader_identity = loaders[0]
    references = [{'role': 'root', 'key': root_key}, *loader_scene_references(loader_tree)]
    rows, seen_locations, seen_files = [], set(), set()
    for ref in references:
        loc, path = bind_scene(catalog, addressables, ref['key'])
        if loc['entry_id'] in seen_locations:
            raise h.HeightmapImportError('Source cohort aliases a previously loaded scene.')
        seen_locations.add(loc['entry_id'])
        inventory.add(path)
        env = environment if path == bundle else unity.load(str(path))
        scene_objects = tuple(env.objects)
        file_name = primary_scene_file(scene_objects, loc['internal_id'], cancelled)
        if file_name in seen_files:
            raise h.HeightmapImportError('Distinct scene locations reuse a primary scene identity.')
        seen_files.add(file_name)
        graph = SceneOwnership(scene_objects, [file_name], cancelled)
        rows.append({**ref, 'catalog_entry': loc['entry_id'], 'internal_id': loc['internal_id'],
                     'bundle': path.relative_to(root).as_posix(), 'primary_file': file_name,
                     'ownership': graph.report()})
        print(json.dumps({'map': map_key, 'role': ref['role'], 'entities': len(graph.entities),
                          'components': len(graph.component_owners)}), flush=True)
        del graph, env, scene_objects
        gc.collect()
    inventory.verify()
    result = {'schema': 'foa.campaign-source-cohort', 'schema_version': 1, 'status': 'PARTIAL',
              'map': map_key, 'source_selection': 'PASSED', 'source_unchanged': True,
              'loader': {'serialized_file': primary, 'path_id': str(loader.path_id), 'script': loader_identity},
              'scenes': rows, 'source_inventory': sorted(inventory.rows.values(), key=lambda r: r['relative_path']),
              'entities': sum(row['ownership']['entities'] for row in rows),
              'components': sum(row['ownership']['components'] for row in rows),
              'elapsed_seconds': round(time.monotonic() - start, 3),
              'runtime_load_policy': 'NOT_RUN', 'record_preservation': 'NOT_RUN',
              'native_scene_assembly': 'NOT_RUN', 'visual_parity': 'NOT_RUN', 'game_export': 'NOT_RUN'}
    encoded = json.dumps(result, indent=2, allow_nan=False).encode('utf-8')
    if len(encoded) > MAX_REPORT_BYTES:
        raise h.HeightmapImportError('Source manifest byte budget exceeded.')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as stream:
        stream.write(encoded)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', required=True, type=Path)
    parser.add_argument('--map', required=True, choices=tuple(h.CAMPAIGN_MAPS))
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result = resolve_cohort(args.game_root, args.map, args.output)
    print(json.dumps({k: result[k] for k in ('map', 'source_selection', 'entities', 'components', 'elapsed_seconds')}))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
