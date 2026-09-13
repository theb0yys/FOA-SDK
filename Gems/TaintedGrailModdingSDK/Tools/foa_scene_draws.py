# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Explicit Drake material ordinals and qualified Unity BRG submesh selection.

The installed source submits one draw per material ordinal. The synthetic Unity
6000.0.64f1 D3D11 BRG fixture independently observes clamping to the final submesh,
including repeated additive draws. This maps geometry only: source baking hooks,
LOD/filter state, material/shader behavior, native draw order and export are separate.
"""
from dataclasses import dataclass, replace

import foa_heightmap_importer as h
from foa_scene_asset_binding import SourceAssetReference
from foa_scene_mesh import MAX_SUBMESHES
from foa_scene_mesh_glb import encode_glb

QUALIFIED_UNITY_VERSION = '6000.0.64f1'
MAX_MATERIAL_ORDINALS = 128


@dataclass(frozen=True)
class DrawSlot:
    material_ordinal: int
    requested_submesh: int
    effective_submesh: int
    material: SourceAssetReference


def drake_draw_slots(submesh_count, materials, *, unity_version):
    """Preserve material multiplicity; unassigned mesh submeshes receive no draw.

    Callers must bind the exact source Drake implementation and supply its effective
    material list. This function does not execute source baking/modification hooks.
    Counts beyond the nonnegative signed-byte source ordinal range are unqualified.
    """
    if unity_version != QUALIFIED_UNITY_VERSION:
        raise h.HeightmapImportError('Drake submesh selection requires the qualified Unity profile.')
    if type(submesh_count) is not int or not 1 <= submesh_count <= MAX_SUBMESHES:
        raise h.HeightmapImportError('Invalid source mesh submesh count.')
    if not isinstance(materials, (list, tuple)) or len(materials) > MAX_MATERIAL_ORDINALS:
        raise h.HeightmapImportError('Material ordinals exceed the qualified source range.')
    if any(not isinstance(m, SourceAssetReference) or m.class_name != 'Material' for m in materials):
        raise h.HeightmapImportError('Draw slots require exact typed source Material references.')
    return tuple(DrawSlot(i, i, min(i, submesh_count - 1), m) for i, m in enumerate(materials))


def project_drake_draw_geometry(mesh, materials, *, unity_version, cancelled=lambda: False):
    """Project draw-selected geometry without discarding the original source record.

    A distinct native primitive is retained per draw, even when several use the
    same source submesh or Material identity. Native processing and rendering must
    verify that multiplicity/order; the GLB alone grants neither claim.
    """
    h.check_cancelled(cancelled)
    slots = drake_draw_slots(len(mesh.submeshes), materials, unity_version=unity_version)
    if not slots:
        raise h.HeightmapImportError('A renderer with zero draws has no geometry product.')
    draw_mesh = replace(mesh, submeshes=[mesh.submeshes[s.effective_submesh] for s in slots])
    data, report = encode_glb(draw_mesh, cancelled)
    report['source_submesh_count'] = len(mesh.submeshes)
    report['draw_slots'] = [{'material_ordinal': s.material_ordinal,
        'requested_submesh': s.requested_submesh, 'effective_submesh': s.effective_submesh,
        'material_key': s.material.key} for s in slots]
    report['undrawn_source_submeshes'] = sorted(set(range(len(mesh.submeshes))) - {s.effective_submesh for s in slots})
    report['source_record_required_for_return'] = True
    report['source_baking_hooks'] = 'NOT_RUN'
    report['native_draw_order'] = 'NOT_RUN'
    return data, report


def project_source_submesh_geometry(mesh, source_submesh, cancelled=lambda: False):
    """Isolate one exact primitive for an independently ordered native draw.

    Several material draws may reuse this geometry product. Keep each draw's
    material identity/ordinal separately, and do not infer runtime bin ordering.
    """
    h.check_cancelled(cancelled)
    if (not 1 <= len(mesh.submeshes) <= MAX_SUBMESHES or
            type(source_submesh) is not int or not 0 <= source_submesh < len(mesh.submeshes)):
        raise h.HeightmapImportError('Invalid exact source submesh selection.')
    selected = replace(mesh, submeshes=[mesh.submeshes[source_submesh]])
    data, report = encode_glb(selected, cancelled)
    report.update(source_submesh_count=len(mesh.submeshes), source_submesh=source_submesh,
        source_record_required_for_return=True, native_draw_order='NOT_RUN')
    return data, report
