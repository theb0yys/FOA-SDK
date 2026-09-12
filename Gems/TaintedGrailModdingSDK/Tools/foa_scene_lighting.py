# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Bind stored Unity 6000 lights to their exact scene owners and HDRP companions.

The qualified HDRP v13 fields for intensity/units/shape are legacy migration
storage. Current values belong to the native Light record. This snapshot keeps
both originals and every controller; it does not infer a live lighting state,
photometric conversion, GPU light list, shadow/bake result or export permission.
"""
import copy
import hashlib
import math
import struct

from foa_heightmap_importer import HeightmapImportError, check_cancelled
from foa_scene_asset_binding import embedded_tree
from foa_scene_component_audit import reference_key
from foa_scene_ownership import identity

HD_LIGHT = ('Unity.RenderPipelines.HighDefinition.Runtime',
            'UnityEngine.Rendering.HighDefinition', 'HDAdditionalLightData')
UNITY_VERSION = '6000.0.64f1'
HDRP_RUNTIME_SHA256 = 'ff37e967ea27e617bcd6a1987c2d9d1ad2ea29533c054323e2b12574a2d6bd63'
MAX_LIGHTS = 32768
MAX_RECORD_BYTES = 4 * 1024 * 1024
MAX_CAPTURE_BYTES = 64 * 1024 * 1024
# These are stored field names, not a conversion to HDRP's final GPU values.
NATIVE_FIELDS = ('m_Enabled', 'm_Type', 'm_Color', 'm_Intensity', 'm_Range',
                 'm_SpotAngle', 'm_InnerSpotAngle', 'm_LightUnit', 'm_LuxAtDistance',
                 'm_EnableSpotReflector', 'm_ColorTemperature', 'm_UseColorTemperature')


def require(value, message):
    if not value:
        raise HeightmapImportError(message)


def presentation_tree(value, depth=0):
    """JSON-safe inspection view; raw_hex remains the source-byte authority.

    Unknown controller data can contain nonfinite values. Preserve the decoded
    float64 representation in an explicit tag, never replace it with zero/null.
    This view must not be used to reserialize the source record.
    """
    require(depth <= 256, 'Source lighting tree exceeds its depth bound.')
    if type(value) is float and not math.isfinite(value):
        return {'$foa_float64_bits': struct.pack('<d', value).hex()}
    if type(value) is bytes:
        return {'$foa_bytes_hex': value.hex()}
    if type(value) is dict:
        return {key: presentation_tree(item, depth+1) for key, item in value.items()}
    if type(value) in (list, tuple):
        return [presentation_tree(item, depth+1) for item in value]
    require(type(value) in (str, int, float, bool, type(None)), 'Unsupported source lighting view value.')
    return value


def stored_light_values(tree, additional=None):
    """Read the migrated native Light fields; never fall back to obsolete HDRP data."""
    require(type(tree) is dict and all(k in tree for k in NATIVE_FIELDS),
            'Native Light fields are incomplete; obsolete HDRP values cannot fill them.')
    if additional is not None:
        require(type(additional) is dict and type(additional.get('m_Version')) is int
                and additional['m_Version'] == 13, 'Unqualified HDRP light migration version.')
    for field in ('m_Type', 'm_LightUnit'):
        require(type(tree[field]) is int and 0 <= tree[field] <= 0xffffffff,
                'Invalid stored native Light enum.')
    require(type(tree['m_Enabled']) is int and tree['m_Enabled'] in (0, 1),
            'Invalid stored native Light enabled flag.')
    for field in ('m_EnableSpotReflector', 'm_UseColorTemperature'):
        require(type(tree[field]) is bool, 'Invalid stored native Light boolean.')
    for field in ('m_Intensity', 'm_Range', 'm_SpotAngle', 'm_InnerSpotAngle',
                  'm_LuxAtDistance', 'm_ColorTemperature'):
        require(type(tree[field]) is float and math.isfinite(tree[field]),
                'Invalid stored native Light scalar.')
    color = tree['m_Color']
    require(type(color) is dict and set(color) == set('rgba')
            and all(type(v) is float and math.isfinite(v) for v in color.values()),
            'Invalid stored native Light color.')
    # Preserve source values without clipping, enum-name guesses or color conversion.
    return {field: copy.deepcopy(tree[field]) for field in NATIVE_FIELDS}



def project_light_values(tree, additional, profile):
    """Return a separate value projection, keeping pre-migration source immutable.

    The exact HDRP v12-to-v13 Point-light migration was executed in the pinned
    Unity host. Its three native setters preserve intensity. Other v12 shapes
    remain unqualified (Pyramid also changes areaSize); no obsolete intensity is
    consumed. This is stored-state projection, not a live controller/GPU state.
    """
    require(profile == {'unity_version': UNITY_VERSION, 'hdrp_runtime_sha256': HDRP_RUNTIME_SHA256},
            'Light projection requires the exact independently fingerprinted game profile.')
    require(additional is None or type(additional) is dict, 'Malformed HDRP light companion.')
    version = additional.get('m_Version') if additional is not None else None
    require(additional is None or type(version) is int, 'Malformed HDRP light migration version.')
    if additional is None or version == 13:
        return {'values': stored_light_values(tree, additional), 'source_version': version,
                'projected_version': version, 'changed_fields': [], 'migration': 'NOT_APPLICABLE'}
    require(version == 12, 'Unqualified HDRP light migration version.')
    values = stored_light_values(tree)
    require(values['m_Type'] == 2, 'Only the qualified HDRP v12 Point-light projection is supported.')
    # Serialized MonoBehaviour booleans are integers; native Light booleans are bools.
    reflector = additional.get('m_EnableSpotReflector')
    unit = additional.get('m_LightUnit')
    distance = additional.get('m_LuxAtDistance')
    require(type(reflector) is int and reflector in (0, 1), 'Invalid HDRP migration reflector flag.')
    require(type(unit) is int and unit in (0, 1, 2, 3, 4)
            and values['m_LightUnit'] in (0, 1, 2, 3, 4), 'Unqualified HDRP migration light unit.')
    require(type(distance) is float and math.isfinite(distance) and 0.0 < distance <= 3.4028234663852886e38,
            'Invalid HDRP migration lux distance.')
    require(struct.unpack('<f', struct.pack('<f', distance))[0] == distance,
            'HDRP migration lux distance must retain its stored float32 value.')
    replacement = {'m_EnableSpotReflector': bool(reflector), 'm_LuxAtDistance': distance, 'm_LightUnit': unit}
    changed = [key for key, value in replacement.items() if values[key] != value]
    values.update(replacement)
    return {'values': values, 'source_version': 12, 'projected_version': 13,
            'changed_fields': changed, 'migration': 'PASSED'}


def capture_scene_lights(graph, scripts, profile, cancelled=lambda: False, *, include_projection=False):
    """Capture explicit primary-scene lights after reciprocal ownership validation.

    graph is SceneOwnership; scripts is ComponentScriptAudit with its explicit
    dependency loader. The result is private source data, never a public fixture.
    """
    require(profile == {'unity_version': UNITY_VERSION, 'hdrp_runtime_sha256': HDRP_RUNTIME_SHA256},
            'Source lighting requires the exact independently fingerprinted game profile.')
    require(type(include_projection) is bool, 'Light projection opt-in must be a boolean.')
    rows = []
    used_bytes = 0
    records = {}

    def capture(key, owner=None):
        nonlocal used_bytes
        check_cancelled(cancelled)
        if key in records:
            return records[key]
        obj = graph.objects[key]
        # Shipped scenes explicitly store 0.0.0. Accept that observed stripped
        # marker only with the caller's separately fingerprinted exact profile;
        # use embedded trees, never a guessed or ambient class database.
        require(obj.assets_file.unity_version in (UNITY_VERSION, '0.0.0'),
                'Unqualified source Light Unity version.')
        require(type(obj.byte_size) is int and 0 < obj.byte_size <= MAX_RECORD_BYTES,
                'Source lighting record exceeds its byte bound.')
        used_bytes += obj.byte_size
        require(used_bytes <= MAX_CAPTURE_BYTES, 'Source lighting capture exceeds its byte bound.')
        raw = obj.get_raw_data()
        require(type(raw) is bytes and len(raw) == obj.byte_size,
                'Source lighting record byte count differs.')
        tree = embedded_tree(obj, MAX_RECORD_BYTES)
        if owner is not None:
            require(reference_key(obj.assets_file, tree.get('m_GameObject')) == owner,
                    'Source lighting owner changed after ownership validation.')
        require(obj.get_raw_data() == raw, 'Source lighting record changed during capture.')
        record = {'identity': identity(key), 'kind': obj.type.name,
                  'stored_unity_version': obj.assets_file.unity_version,
                  'raw_sha256': hashlib.sha256(raw).hexdigest(), 'raw_hex': raw.hex(), 'tree': presentation_tree(tree)}
        records[key] = record
        return record

    for entity in graph.entities.values():
        check_cancelled(cancelled)
        lights = [c for c in entity.components if c.kind == 'Light']
        if not lights:
            continue
        require(len(lights) == 1, 'Multiple native Light components have an ambiguous companion binding.')
        require(len(rows) < MAX_LIGHTS, 'Source lighting count exceeds its bound.')
        light = capture(lights[0].key, entity.key)
        owner = capture(entity.key)
        transform = capture(entity.transform, entity.key)
        # Recheck the reciprocal list against the captured GameObject, not its display name.
        component_rows = owner['tree'].get('m_Component')
        require(type(component_rows) is list
                and all(type(row) is dict and set(row) == {'component'} for row in component_rows),
                'Source lighting component list is malformed.')
        declared = [reference_key(graph.objects[entity.key].assets_file, row['component'])
                    for row in component_rows]
        require(declared == [c.key for c in entity.components],
                'Source lighting component list changed after ownership validation.')
        companions, controllers = [], []
        for component in entity.components:
            if component.kind != 'MonoBehaviour':
                continue
            record = capture(component.key, entity.key)
            obj = graph.objects[component.key]
            scripts.observe(obj, record['tree'])
            script_key = reference_key(obj.assets_file, record['tree'].get('m_Script'))
            require(script_key == component.script, 'Source lighting script changed after ownership validation.')
            script = scripts.bindings.get(script_key)
            require(script is not None, 'Source lighting controller script could not be resolved.')
            value = {**record, 'script': copy.deepcopy(script)}
            if tuple(script[k] for k in ('assembly', 'namespace', 'class')) == HD_LIGHT:
                companions.append(value)
            else:
                controllers.append(value)
        require(len(companions) <= 1, 'Multiple HDRP Light companions are ambiguous.')
        additional = companions[0] if companions else None
        version = additional['tree'].get('m_Version') if additional else None
        require(additional is None or type(version) is int, 'Malformed HDRP light migration version.')
        qualified = additional is None or version == 13
        # Preserve older/future records explicitly; applying their migration is
        # a separate operation. Never label pre-migration fields as render inputs.
        values = stored_light_values(light['tree'], additional['tree'] if additional else None) if qualified else None
        rows.append({'entity': owner, 'transform': transform, 'light': light,
                     'additional': additional, 'controllers': controllers, 'stored_values': values,
                     'stored_values_status': 'PASSED' if qualified else 'BLOCKED',
                     'migration_required': None if qualified else version,
                     'hdrp_companion': 'PASSED' if additional else 'NOT_RUN',
                     'live_controller_state': 'NOT_RUN', 'gpu_light_data': 'NOT_RUN'})
        if include_projection:
            projectable = qualified or (version == 12 and light['tree'].get('m_Type') == 2)
            rows[-1]['value_projection'] = project_light_values(
                light['tree'], additional['tree'] if additional else None, profile) if projectable else None
            rows[-1]['projected_values_status'] = 'PASSED' if projectable else 'BLOCKED'
    packet = {'schema': 'foa.private.source-lighting', 'version': 2 if include_projection else 1, 'source_capture': 'PASSED',
            'source_profile': copy.deepcopy(profile), 'light_count': len(rows), 'source_record_bytes': used_bytes, 'lights': rows,
            'stored_values_status': 'PASSED' if all(r['stored_values_status'] == 'PASSED' for r in rows) else 'PARTIAL',
            'rendering': 'NOT_RUN', 'game_export': 'NOT_RUN'}

    if include_projection:
        packet['projected_values_status'] = 'PASSED' if all(r['projected_values_status'] == 'PASSED' for r in rows) else 'PARTIAL'
    return packet
