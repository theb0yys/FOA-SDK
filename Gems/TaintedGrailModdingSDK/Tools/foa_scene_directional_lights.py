# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Encode explicit HDRP directional-light records for native structured buffers.

The 176-byte layout is qualified against the installed original HDRP type. This
codec performs no lighting calculation, visibility selection or default filling.
The original builder's initial output still needs cookie, atmosphere and shadow
processing before it constitutes a final scene light list.
"""
import math
import struct
from foa_scene_lighting import HDRP_RUNTIME_SHA256, UNITY_VERSION, require

STRIDE = 176
MAX_LIGHTS = 1024
LAYOUT = (
    ('positionRWS', 0, '3f'), ('lightLayers', 12, 'I'),
    ('forward', 16, '3f'), ('cookieMode', 28, 'i'),
    ('cookieScaleOffset', 32, '4f'), ('right', 48, '3f'),
    ('shadowIndex', 60, 'i'), ('up', 64, '3f'),
    ('contactShadowIndex', 76, 'i'), ('color', 80, '3f'),
    ('contactShadowMask', 92, 'i'), ('shadowTint', 96, '3f'),
    ('shadowDimmer', 108, 'f'), ('volumetricShadowDimmer', 112, 'f'),
    ('nonLightMappedOnly', 116, 'i'), ('minRoughness', 120, 'f'),
    ('screenSpaceShadowIndex', 124, 'i'), ('shadowMaskSelector', 128, '4f'),
    ('diffuseDimmer', 144, 'f'), ('specularDimmer', 148, 'f'),
    ('lightDimmer', 152, 'f'), ('volumetricLightDimmer', 156, 'f'),
    ('penumbraTint', 160, 'f'), ('isRayTracedContactShadow', 164, 'f'),
    ('angularDiameter', 168, 'f'), ('distanceFromCamera', 172, 'f'),
)


def pack_directional_lights(records, profile):
    """Encode ordered complete records; float scalars use IEEE binary32.

    A caller must supply the qualified frame's complete fields and source profile.
    An empty list is not replaced with an invented light or dummy record.
    """
    require(profile == {'unity_version': UNITY_VERSION, 'hdrp_runtime_sha256': HDRP_RUNTIME_SHA256},
            'Directional lights require the exact independently fingerprinted source profile.')
    require(type(records) is list and 0 < len(records) <= MAX_LIGHTS, 'Invalid directional-light count.')
    payload = bytearray(len(records) * STRIDE)
    names = {name for name, _, _ in LAYOUT}
    for index, record in enumerate(records):
        require(type(record) is dict and set(record) == names, 'Directional-light fields must be complete and exact.')
        for name, offset, fmt in LAYOUT:
            value = record[name]
            if fmt in ('3f', '4f'):
                axes = 'xyzw'[:int(fmt[0])]
                require(type(value) is dict and set(value) == set(axes), 'Invalid directional-light vector.')
                values = [value[axis] for axis in axes]
            else:
                values = [value]
            if fmt.endswith('f'):
                require(all(type(v) is float and math.isfinite(v) and abs(v) <= 3.4028234663852886e38 for v in values),
                        'Directional-light scalar must be a finite float32-range value.')
            else:
                low, high = (0, 0xffffffff) if fmt == 'I' else (-0x80000000, 0x7fffffff)
                require(type(value) is int and low <= value <= high, 'Invalid directional-light integer.')
            struct.pack_into('<' + fmt, payload, index * STRIDE + offset, *values)
    return bytes(payload)


def directional_shader_buffer(records, declaration, profile):
    """Bind only to an explicit, previously validated SM5 structured declaration."""
    require(type(declaration) is dict and set(declaration) == {'slot', 'type', 'stride'},
            'Expected an exact shader buffer declaration.')
    require(all(type(declaration[key]) is int for key in declaration)
            and 0 <= declaration['slot'] <= 127 and declaration['type'] == 2 and declaration['stride'] == STRIDE,
            'Directional lights require a 176-byte structured shader buffer.')
    return {**declaration, 'hex': pack_directional_lights(records, profile).hex()}
