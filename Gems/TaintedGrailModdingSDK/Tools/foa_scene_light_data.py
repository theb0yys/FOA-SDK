# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Pack complete original HDRP LightData records without inferring scene state.

The profile-specific 224-byte record includes explicit padding fields. Visibility,
light computation, cookies and shadows belong to their original producers. This
codec neither selects lights nor fills absent fields with rendering defaults.
Non-finite GPU values require explicit little-endian bits to preserve NaN payloads.
"""
import math
import struct
from foa_scene_lighting import HDRP_RUNTIME_SHA256, UNITY_VERSION, require

STRIDE = 224
MAX_LIGHTS = 4096
LAYOUT = (
    ('positionRWS', 0, '3f'), ('lightLayers', 12, 'I'),
    ('lightDimmer', 16, 'f'), ('volumetricLightDimmer', 20, 'f'),
    ('angleScale', 24, 'f'), ('angleOffset', 28, 'f'),
    ('forward', 32, '3f'), ('iesCut', 44, 'f'),
    ('lightType', 48, 'i'), ('right', 52, '3f'),
    ('penumbraTint', 64, 'f'), ('range', 68, 'f'),
    ('cookieMode', 72, 'i'), ('shadowIndex', 76, 'i'),
    ('up', 80, '3f'), ('rangeAttenuationScale', 92, 'f'),
    ('color', 96, '3f'), ('rangeAttenuationBias', 108, 'f'),
    ('cookieScaleOffset', 112, '4f'), ('shadowTint', 128, '3f'),
    ('shadowDimmer', 140, 'f'), ('volumetricShadowDimmer', 144, 'f'),
    ('nonLightMappedOnly', 148, 'i'), ('minRoughness', 152, 'f'),
    ('screenSpaceShadowIndex', 156, 'i'), ('shadowMaskSelector', 160, '4f'),
    ('size', 176, '4f'), ('contactShadowMask', 192, 'i'),
    ('diffuseDimmer', 196, 'f'), ('specularDimmer', 200, 'f'),
    ('__unused__', 204, 'f'), ('padding', 208, '2f'),
    ('isRayTracedContactShadow', 216, 'f'), ('boxLightSafeExtent', 220, 'f'),
)


def pack_light_data(records, profile):
    """Preserve ordered, complete records in the qualified little-endian ABI."""
    require(profile == {'unity_version': UNITY_VERSION, 'hdrp_runtime_sha256': HDRP_RUNTIME_SHA256},
            'LightData requires the exact independently fingerprinted source profile.')
    require(type(records) is list and 0 < len(records) <= MAX_LIGHTS, 'Invalid LightData count.')
    payload = bytearray(len(records) * STRIDE)
    names = {name for name, _, _ in LAYOUT}
    for index, record in enumerate(records):
        require(type(record) is dict and set(record) == names, 'LightData fields must be complete and exact.')
        for name, offset, fmt in LAYOUT:
            value = record[name]
            if len(fmt) == 2:
                axes = 'xyzw'[:int(fmt[0])]
                require(type(value) is dict and set(value) == set(axes), 'Invalid LightData vector.')
                values = [value[axis] for axis in axes]
            else:
                values = [value]
            if fmt.endswith('f'):
                for component, scalar in enumerate(values):
                    target = index * STRIDE + offset + component * 4
                    if type(scalar) is dict:
                        require(set(scalar) == {'$foa_float32_bits'}, 'Invalid non-finite LightData tag.')
                        bits = scalar['$foa_float32_bits']
                        require(type(bits) is str and len(bits) == 8 and all(c in '0123456789abcdef' for c in bits),
                                'Non-finite LightData requires four explicit little-endian bytes.')
                        word = int.from_bytes(bytes.fromhex(bits), 'little')
                        require(word & 0x7f800000 == 0x7f800000, 'Finite LightData values must use floats.')
                        payload[target:target + 4] = bytes.fromhex(bits)
                    else:
                        require(type(scalar) is float and math.isfinite(scalar) and abs(scalar) <= 3.4028234663852886e38,
                                'LightData scalar must be a finite float32-range value or exact non-finite bits.')
                        struct.pack_into('<f', payload, target, scalar)
                continue
            else:
                low, high = (0, 0xffffffff) if fmt == 'I' else (-0x80000000, 0x7fffffff)
                require(type(value) is int and low <= value <= high, 'Invalid LightData integer.')
            struct.pack_into('<' + fmt, payload, index * STRIDE + offset, *values)
    return bytes(payload)


def light_data_shader_buffer(records, declaration, profile):
    """Require an explicit matching structured declaration; never guess a slot."""
    require(type(declaration) is dict and set(declaration) == {'slot', 'type', 'stride'},
            'Expected an exact shader buffer declaration.')
    require(all(type(declaration[key]) is int for key in declaration)
            and 0 <= declaration['slot'] <= 127 and declaration['type'] == 2 and declaration['stride'] == STRIDE,
            'LightData requires a 224-byte structured shader buffer.')
    return {**declaration, 'hex': pack_light_data(records, profile).hex()}
