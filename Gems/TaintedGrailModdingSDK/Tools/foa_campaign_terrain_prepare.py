# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Read-only campaign terrain preparation into a private SDK workspace.

This worker resolves the original scene/renderer references and preserves every
LOD0 Medusa mesh and instance. It never edits an installation, launches a game,
fills terrain gaps or publishes content to source control.
"""
import argparse
import gc
import hashlib
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
import foa_heightmap_importer as h
from foa_campaign_heightmap_export import SourceInventory
from foa_campaign_terrain import prepare, compare_source, SHADER
from foa_scene_cohort import resolve_cohort
from foa_scene_component_audit import ComponentScriptAudit, reference_key
from foa_scene_asset_binding import embedded_tree
from foa_scene_medusa import MedusaScene, manager_renderers, MANAGER_TYPE, MEMBERS, PROFILE
from foa_scene_mesh import read_mesh
from foa_scene_native_shader import encode
from foa_native_shader_compiler import Compiler

UTILITY_SHA256 = 'bb887211905ac644701083d6b8d24d3b802b6cd60a3d008098d32e94ca5f0519'


def private(path):
    value = h.require_direct_path(Path(path))
    if any((p/'.git').exists() for p in (value, *value.parents)):
        raise h.HeightmapImportError('Campaign terrain must stay in a private workspace outside source control.')
    return value


def write_new(path, raw):
    with path.open('xb') as output: output.write(raw)


def run(game_root, workspace, map_key, operation, timeout=900):
    root = h.require_direct_path(Path(game_root)); workspace = private(workspace); operation = private(operation)
    if (not workspace.is_dir() or not operation.is_dir() or not operation.is_relative_to(workspace/'Staging'/'CampaignTerrain')
            or workspace.is_relative_to(root) or root.is_relative_to(workspace)):
        raise h.HeightmapImportError('Invalid private campaign terrain work order.')
    start = time.monotonic()
    cancelled = lambda: (operation/'cancel.flag').exists() or time.monotonic()-start > timeout
    inventory = SourceInventory(root, cancelled)
    for name, expected in (('Awaken.ECS.dll', PROFILE['awaken_ecs_sha256']),
                           ('Unity.Entities.Graphics.dll', PROFILE['entities_graphics_sha256']),
                           ('Awaken.Utility.dll', UTILITY_SHA256)):
        path = root/'Fall of Avalon_Data/Managed'/name
        if h.sha256_file(path, cancelled)[0].removeprefix('sha256:') != expected:
            raise h.HeightmapImportError('The installed terrain source profile is not qualified: '+name)
        inventory.add(path)
    cohort = resolve_cohort(root, map_key, operation/'cohort.private.json', min(timeout, 180))
    for row in cohort['source_inventory']: inventory.add(root/row['relative_path'])
    scene_row = next(row for row in cohort['scenes'] if row['role'] == 'map-static')
    unity, _ = h.import_unitypy(h.UNITY_FALLBACK_VERSION)
    install = h.resolve_game_install(root); resolver = h.CabDependencyResolver(unity, install.bundle_root)
    bundle = root/scene_row['bundle']; env = unity.load(str(bundle)); objects = tuple(env.objects)
    loaded = set()
    def dependency(name):
        path = resolver.resolve(name)
        if path in loaded: raise h.HeightmapImportError('Unresolved repeated terrain script dependency.')
        inventory.add(path); loaded.add(path)
        return unity.load(str(path)).objects
    scripts = ComponentScriptAudit(objects, dependency, cancelled); matches = []
    for obj in objects:
        h.check_cancelled(cancelled)
        if str(obj.assets_file.name).lower() != scene_row['primary_file'] or obj.type.name != 'MonoBehaviour': continue
        tree = embedded_tree(obj); scripts.observe(obj, tree)
        identity = scripts.bindings.get(reference_key(obj.assets_file, tree['m_Script']))
        if identity and tuple(identity[k] for k in ('assembly', 'namespace', 'class')) == MANAGER_TYPE:
            matches.append((obj, tree))
    if len(matches) != 1: raise h.HeightmapImportError('Campaign terrain needs one exact Medusa manager.')
    manager, tree = matches[0]; manager_sha = hashlib.sha256(manager.get_raw_data()).hexdigest()
    renderers = manager_renderers(manager.assets_file, tree, MANAGER_TYPE)
    scene_name = Path(scene_row['internal_id']).stem
    archive = root/'Fall of Avalon_Data/StreamingAssets/Medusa/medusa.arch'; inventory.add(archive)
    archive_sha = h.sha256_file(archive, cancelled)[0].removeprefix('sha256:')
    archive_env = unity.load(str(archive)); names = {scene_name+'/'+n for n in MEMBERS}; members = {}
    for archive_bundle in archive_env.files.values():
        for name, data in archive_bundle.files.items():
            if name in names:
                if name in members: raise h.HeightmapImportError('Duplicate Medusa archive member.')
                members[name] = data.bytes
    scene = MedusaScene(scene_name, renderers, tree['_transformsCount'], tree['_allRenderersCount'],
                        tree['_allUvDistributionsCount'], members, PROFILE, cancelled)
    required = {draw['mesh'] for draw in scene.draws(lod=0)}; files = {}
    for file, path_id in required: files.setdefault(file, []).append(path_id)
    meshes = {}
    for file, ids in sorted(files.items()):
        h.check_cancelled(cancelled)
        path = resolver.resolve(file); inventory.add(path); mesh_env = unity.load(str(path))
        lookup = {(str(obj.assets_file.name).lower(), obj.path_id): obj for obj in mesh_env.objects}
        resources = {n: value for b in mesh_env.files.values() for n, value in b.files.items()}
        for path_id in ids:
            if (file, path_id) not in lookup: raise h.HeightmapImportError('Original terrain mesh is absent from its declared source file.')
            meshes[file, path_id] = read_mesh(lookup[file, path_id], resources, cancelled)
        del mesh_env, lookup, resources; gc.collect()
    raw = prepare(scene, meshes, map_key, archive_sha, manager_sha, cancelled)
    comparison = compare_source(raw, scene, meshes, archive_sha, manager_sha)
    inventory.verify(); h.check_cancelled(cancelled)
    packet = operation/'terrain.private.json'; write_new(packet, raw)
    compiler = Compiler(); stages = []
    for stage, entry, target in (('vertex', b'VS', b'vs_5_0'), ('fragment', b'PS', b'ps_5_0')):
        stages.append(dict(stage=stage, code=compiler.compile(SHADER, entry, target),
            constant_buffers=[dict(slot=0, size=64), dict(slot=1, size=64)] if stage == 'vertex' else [],
            images=[], samplers=[], buffers=[dict(slot=0, type=4, stride=4)] if stage == 'vertex' else []))
    state = dict(cull=0, depth_enable=1, depth_write=1, depth_func=6, blend_enable=0,
                 blend_source=1, blend_dest=0, blend_op=0, alpha_source=1, alpha_dest=0, alpha_op=0, write_mask=15)
    shader = encode(stages, state=state, draw_list='forward')
    asset_root = workspace/'EditorAssets'; folder = asset_root/'Assets/foa_terrain_shape'
    private(folder); folder.mkdir(parents=True, exist_ok=True)
    shader_path = folder/'terrain.foashader'
    if shader_path.exists():
        if private(shader_path).read_bytes() != shader: raise h.HeightmapImportError('Existing terrain inspection shader differs; preserve it and use another workspace.')
    else: write_new(shader_path, shader)
    inventory.verify()
    doc = json.loads(raw)
    result = dict(status='PASSED', map=map_key, packet=str(packet), packet_sha256=hashlib.sha256(raw).hexdigest(),
        asset_root=str(asset_root), shader_sha256=hashlib.sha256(shader).hexdigest(),
        groups=len(doc['groups']), source_draws=doc['source_draw_count'], unique_meshes=doc['unique_mesh_count'],
        source_files=sorted(inventory.rows.values(), key=lambda row: row['relative_path']),
        source_unchanged=True, source_comparison=comparison, original_material_rendering='NOT_RUN', native_assembly='NOT_RUN', seconds=time.monotonic()-start)
    write_new(operation/'prepared.json', json.dumps(result, indent=2).encode())
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', required=True); parser.add_argument('--workspace', required=True)
    parser.add_argument('--map', required=True, choices=tuple(h.CAMPAIGN_MAPS)); parser.add_argument('--operation', required=True)
    args = parser.parse_args(); result = run(args.game_root, args.workspace, args.map, args.operation)
    print(json.dumps({k: result[k] for k in ('status', 'map', 'groups', 'source_draws', 'seconds')}))


if __name__ == '__main__':
    try: main()
    except Exception as error:
        print(str(error), file=sys.stderr); sys.exit(1)
