# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""World Authoring joins direct source renderers to exact editable placements.

Consumes a qualified full hierarchy and reciprocal SceneOwnership graph. The
caller verifies source bundle/capture fingerprints before and after preparation.
This resolves serialized ownership, selectors and LOD membership; baking hooks,
runtime LOD/bin selection, shader qualification and export remain separate.
"""
from dataclasses import dataclass
import hashlib

import foa_scene_assembly as assembly
from foa_scene_asset_binding import SourceAssetReference, embedded_tree
from foa_scene_component_audit import reference_key
from foa_scene_material import integer
from foa_scene_ownership import SceneOwnership
from foa_scene_render_assembly import draw_count, placement_digest, prepare_render_batch, MAX_DRAWS

MAX_RENDERERS = 100000
MAX_RECORD_BYTES = 4 * 1024 * 1024
MAX_DECODE_BYTES = 256 * 1024 * 1024
MAX_GROUP_EDGES = 2000000
DRAKE_NAMESPACE = ('Awaken.ECS', 'Awaken.ECS.DrakeRenderer.Authoring')
require = assembly.require


@dataclass(frozen=True)
class DirectRenderer:
    component: tuple
    record_sha256: str
    owner: tuple
    transform: tuple
    placement_sha256: str
    parent_group: tuple | None
    lod_mask: int
    mesh: SourceAssetReference
    materials: tuple


@dataclass(frozen=True)
class RendererGroup:
    component: tuple
    record_sha256: str
    owner: tuple
    transform: tuple
    children: tuple


@dataclass(frozen=True)
class RendererScene:
    hierarchy_bytes: bytes
    bundle_sha256: str
    renderers: tuple
    groups: tuple
    decoded_source_bytes: int


def key(value):
    return value[0], str(value[1])


def selector(kind, value):
    assembly.keys(value, {'m_AssetGUID', 'm_SubObjectName', 'm_SubObjectType'})
    require(value['m_SubObjectType'] == '', 'Unqualified source subobject type selector.')
    return SourceAssetReference.serialized(kind, value['m_AssetGUID'], value['m_SubObjectName'])


def bind_direct_renderers(hierarchy_raw, ownership, scripts, bundle_sha256, cancelled=lambda: False):
    """Bind only source-owned direct Drake components, never merged archive instances.

    Script types resolve through their MonoScript PPtrs. Hierarchy and ownership
    must describe the complete same primary scene, including non-rendered objects.
    The result retains complete placement and renderer record fingerprints. It is
    immutable private preparation data, not a canonical or game scene contract.
    """
    require(isinstance(ownership, SceneOwnership), 'A validated source ownership graph is required.')
    document = assembly.decode(hierarchy_raw, assembly.MAX_SCENE_BYTES)
    rows = assembly._validate(document, assembly.MAX_SCENE_NODES, cancelled)
    require(document['source']['bundle_sha256'] == assembly.digest(bundle_sha256), 'Source bundle differs from hierarchy.')
    require(len(rows) == document['source']['scene_nodes'] == len(ownership.entities), 'A complete matching source hierarchy is required.')
    require(ownership.primary == {document['source']['serialized_file']}, 'Source primary file differs from hierarchy.')
    placements = {}; transforms = {key(k): k for k in ownership.transforms}
    for row in rows:
        assembly.check_cancelled(cancelled)
        ident = assembly.decode(row.descriptor.encode(), 8192)['identity']
        original = transforms.get(row.key)
        require(original is not None, 'Source Transform is absent from ownership graph.')
        links = ownership.transforms[original]; owner = ownership.entities[links.owner]
        require(key(owner.key) == (row.key[0], ident['gameobject_id']) and owner.transform == original,
                'Source GameObject and Transform ownership disagree.')
        expected_parent = key(links.parent) if links.parent is not None else None
        require(expected_parent == (rows[row.parent].key if row.parent >= 0 else None), 'Source parent differs from hierarchy.')
        obj = ownership.objects[original]
        require(obj.type.name == row.kind and 0 < obj.byte_size <= MAX_RECORD_BYTES, 'Source Transform type or record bound differs.')
        require(hashlib.sha256(obj.get_raw_data()).hexdigest() == ident['record_sha256'], 'Source Transform record changed.')
        placements[original] = row
    decoded = 0
    def tree(obj):
        nonlocal decoded
        assembly.check_cancelled(cancelled)
        require(0 < obj.byte_size <= MAX_RECORD_BYTES, 'Renderer source record exceeds its bound.')
        decoded += obj.byte_size
        require(decoded <= MAX_DECODE_BYTES, 'Renderer preparation decode budget exceeded.')
        return embedded_tree(obj, MAX_RECORD_BYTES)
    renderers = []; groups = []; edges = 0
    for owner in ownership.entities.values():
        for component in owner.components:
            assembly.check_cancelled(cancelled)
            if component.kind != 'MonoBehaviour':
                continue
            obj = ownership.objects[component.key]; source = None
            if component.script not in scripts.bindings:
                source = tree(obj); scripts.observe(obj, source)
            bound = scripts.bindings.get(component.script)
            require(bound is not None, 'Source component script identity is unresolved.')
            typ = bound['assembly'], bound['namespace'], bound['class']
            if typ[:2] != DRAKE_NAMESPACE or typ[2] not in ('DrakeMeshRenderer', 'DrakeLodGroup'):
                continue
            require(len(renderers)+len(groups) < MAX_RENDERERS, 'Direct renderer inventory exceeds its bound.')
            source = source if source is not None else tree(obj)
            require(reference_key(obj.assets_file, source.get('m_Script')) == component.script and
                    reference_key(obj.assets_file, source.get('m_GameObject')) == owner.key,
                    'Renderer script or reciprocal owner changed.')
            raw = obj.get_raw_data()
            require(len(raw) == obj.byte_size, 'Renderer record byte count changed.')
            sha = hashlib.sha256(raw).hexdigest()
            if typ[2] == 'DrakeLodGroup':
                children = source.get('children')
                require(type(children) is list and len(children) <= MAX_RENDERERS, 'Invalid source LOD child inventory.')
                edges += len(children); require(edges <= MAX_GROUP_EDGES, 'Source LOD edge budget exceeded.')
                members = tuple(key(reference_key(obj.assets_file, p)) for p in children)
                require(len(set(members)) == len(members), 'Duplicate source LOD member.')
                groups.append(RendererGroup(key(component.key), sha, key(owner.key), key(owner.transform), members))
                continue
            parent = source.get('parentGroup')
            if parent == {'m_FileID': 0, 'm_PathID': 0}:
                require(all(type(v) is int for v in parent.values()), 'Invalid null source LOD parent.')
                parent_key = None
            else:
                parent_key = key(reference_key(obj.assets_file, parent))
            materials = source.get('materialReferences')
            require(type(materials) is list and len(materials) <= 128, 'Source material ordinal count is unqualified.')
            renderers.append(DirectRenderer(key(component.key), sha, key(owner.key), key(owner.transform),
                placement_digest(placements[owner.transform]), parent_key, integer(source.get('lodMask'), 0, 255),
                selector('Mesh', source.get('meshReference')), tuple(selector('Material', value) for value in materials)))
    by_renderer = {row.component: row for row in renderers}; by_group = {row.component: row for row in groups}
    for row in renderers:
        assembly.check_cancelled(cancelled)
        require(row.parent_group is None or row.parent_group in by_group, 'Source renderer LOD group is absent.')
    claimed = {}
    for group in groups:
        for member in group.children:
            assembly.check_cancelled(cancelled)
            require(member in by_renderer and member not in claimed and by_renderer[member].parent_group == group.component,
                    'Source LOD membership is not reciprocal.')
            claimed[member] = group.component
    require(all(row.parent_group is None or row.component in claimed for row in renderers), 'Source renderer is absent from its LOD group.')
    return RendererScene(hierarchy_raw, bundle_sha256, tuple(renderers), tuple(groups), decoded)


def prepare_draw_import(scene, selected, cancelled=lambda: False):
    """Join explicit already-qualified native bindings to selected source renderers.

    Each supplied renderer must exactly match this prepared source record. Every
    serialized material ordinal requires its own draw, including repeated material
    references. The caller qualifies effective baking/pass/material inputs first.
    Draw order is retained exactly; it is not inferred from LODs or source labels.
    Returns the existing hierarchy/render packets for start_import.
    """
    require(isinstance(scene, RendererScene), 'Prepared direct source renderers are required.')
    require(type(selected) in (list, tuple) and 0 < len(selected) <= MAX_DRAWS, 'Invalid direct renderer selection.')
    known = {row.component: row for row in scene.renderers}; seen = set(); placements = {}; transforms = []
    total = 0
    for entry in selected:
        assembly.check_cancelled(cancelled)
        require(type(entry) in (tuple, list) and len(entry) == 2, 'Invalid qualified renderer binding.')
        renderer, native = entry
        require(isinstance(renderer, DirectRenderer) and known.get(renderer.component) == renderer and
                renderer.component not in seen, 'Unknown, stale or duplicate source renderer binding.')
        seen.add(renderer.component)
        count = draw_count(native)
        require(count == len(renderer.materials), 'Native draw multiplicity differs from source material ordinals.')
        total += count; require(total <= MAX_DRAWS, 'Selected renderers exceed native draw capacity.')
        # Freeze caller-owned dictionaries before constructing the grouped packet.
        native = assembly.decode(assembly.encode(native), 16*1024*1024)
        draws = native['draws'] if native['version'] == 3 else [native]
        if renderer.placement_sha256 not in placements:
            transforms.append(renderer.transform); placements[renderer.placement_sha256] = []
        placements[renderer.placement_sha256].extend(draws)
    hierarchy = assembly.select_batch(assembly.decode(scene.hierarchy_bytes, assembly.MAX_SCENE_BYTES), transforms, cancelled)
    bindings = [(sha, draws[0] if len(draws) == 1 else {'version': 3, 'draws': draws}) for sha, draws in placements.items()]
    rendering = prepare_render_batch(hierarchy, bindings, cancelled)
    return hierarchy, rendering
