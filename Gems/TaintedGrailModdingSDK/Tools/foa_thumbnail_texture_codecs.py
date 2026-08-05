#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Public bounded DDS/TGA codecs with fail-closed resource-shape checks."""
from __future__ import annotations

import struct

import foa_thumbnail_texture_codecs_core as _core
from foa_thumbnail_texture_codecs_core import *  # noqa: F401,F403


def decode_dds(payload: bytes) -> tuple[int, int, bytes, str, str]:
    """Reject cubemap/volume shapes before delegating first-mip decoding."""

    if len(payload) >= 128 and payload[:4] == b"DDS ":
        header = payload[4:128]
        depth = struct.unpack_from("<I", header, 20)[0]
        caps2 = struct.unpack_from("<I", header, 108)[0]
        dds_caps2_cubemap = 0x00000200
        dds_caps2_volume = 0x00200000
        if depth not in {0, 1} or caps2 & (
            dds_caps2_cubemap | dds_caps2_volume
        ):
            raise UnsupportedTextureError(
                "DDS cubemaps and volume textures are outside the Alpha cohort."
            )
    return _core.decode_dds(payload)
