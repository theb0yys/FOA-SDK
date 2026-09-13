#!/usr/bin/env python3
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Bounded read-only inventory of the inspected FoA A* cache entry container.

Framing was verified against the installed Pathfinding.Serialization.Zip.ZipFile
reader. Despite that type name, this container is not a standard ZIP archive.
Graph payloads remain opaque; this is not a navmesh decoder, rebuild or exporter.
"""
from dataclasses import dataclass
import hashlib
import struct

import foa_heightmap_importer as h

MAX_CACHE_BYTES = 128 * 1024 * 1024
MAX_ENTRIES = 256
MAX_ENTRY_BYTES = 64 * 1024 * 1024
MAX_NAME_BYTES = 512


@dataclass(frozen=True)
class CacheEntry:
    name: str
    offset: int
    size: int
    sha256: str


def inspect_cache(data, cancelled=lambda: False):
    if not 4 <= len(data) <= MAX_CACHE_BYTES:
        raise h.HeightmapImportError("Navigation container exceeds its size bounds.")
    view = memoryview(data).cast("B")
    cursor = 0

    def take(size):
        nonlocal cursor
        if size < 0 or size > len(view) - cursor:
            raise h.HeightmapImportError("Navigation container is truncated.")
        value = view[cursor:cursor + size]
        cursor += size
        return value

    def integer():
        return struct.unpack("<i", take(4))[0]

    def string_size():
        size = 0
        for shift in range(0, 35, 7):
            value = take(1)[0]
            if shift == 28 and value > 7:
                raise h.HeightmapImportError("Navigation entry name prefix overflows Int32.")
            size |= (value & 127) << shift
            if not value & 128:
                if not 0 < size <= MAX_NAME_BYTES:
                    raise h.HeightmapImportError("Navigation entry name exceeds its limit.")
                return size
        raise h.HeightmapImportError("Invalid navigation entry name prefix.")

    count = integer()
    if not 0 < count <= MAX_ENTRIES:
        raise h.HeightmapImportError("Navigation entry count exceeds its limit.")
    names = set()
    entries = []
    for _ in range(count):
        h.check_cancelled(cancelled)
        try:
            name = take(string_size()).tobytes().decode("utf-8", errors="strict")
        except UnicodeDecodeError as error:
            raise h.HeightmapImportError("Navigation entry name is not UTF-8.") from error
        if name in names:
            raise h.HeightmapImportError("Duplicate navigation entry identity.")
        # Names are identifiers only; no entry is ever opened as a filesystem path.
        names.add(name)
        size = integer()
        if not 0 <= size <= MAX_ENTRY_BYTES:
            raise h.HeightmapImportError("Navigation entry exceeds its byte limit.")
        offset = cursor
        payload = take(size)
        digest = hashlib.sha256()
        for start in range(0, size, 1024 * 1024):
            h.check_cancelled(cancelled)
            digest.update(payload[start:start + 1024 * 1024])
        entries.append(CacheEntry(name, offset, size, digest.hexdigest()))
    if cursor != len(view):
        raise h.HeightmapImportError("Navigation container has unexplained trailing data.")
    return entries
