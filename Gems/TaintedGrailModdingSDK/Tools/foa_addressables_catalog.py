#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Bounded read-only reader for the Addressables JSON key/bucket/entry layout."""
from __future__ import annotations

import base64
import struct

MAX_TABLE_ENTRIES = 200000
MAX_TABLE_BYTES = 32 * 1024 * 1024


class CatalogError(ValueError):
    """The catalog is malformed or outside the supported local preview cohort."""


def integer(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise CatalogError("Addressables table offset is out of bounds.")
    return struct.unpack_from("<i", data, offset)[0]


def count(value: int) -> int:
    if not 0 <= value <= MAX_TABLE_ENTRIES:
        raise CatalogError("Addressables table count exceeds the preview limit.")
    return value


def decode_table(document: dict, key: str) -> bytes:
    text = document.get(key)
    if not isinstance(text, str) or len(text) > MAX_TABLE_BYTES * 4 // 3 + 4:
        raise CatalogError("Addressables table is missing or too large.")
    try:
        return base64.b64decode(text, validate=True)
    except ValueError as exc:
        raise CatalogError("Addressables table is not valid base64.") from exc


class AddressablesCatalog:
    def __init__(self, document: dict):
        self.internal_ids = document.get("m_InternalIds", [])
        self.prefixes = document.get("m_InternalIdPrefixes") or []
        if not isinstance(self.internal_ids, list) or not self.internal_ids:
            raise CatalogError("Addressables internal IDs are missing.")
        count(len(self.internal_ids))
        if any(not isinstance(v, str) or len(v) > 4096 for v in self.internal_ids):
            raise CatalogError("Addressables internal ID is invalid.")
        if not isinstance(self.prefixes, list) or any(not isinstance(v, str) for v in self.prefixes):
            raise CatalogError("Addressables internal ID prefixes are invalid.")
        keys = decode_table(document, "m_KeyDataString")
        buckets = decode_table(document, "m_BucketDataString")
        entries = decode_table(document, "m_EntryDataString")
        bucket_count = count(integer(buckets, 0))
        if count(integer(keys, 0)) != bucket_count:
            raise CatalogError("Addressables key and bucket counts differ.")
        entry_count = count(integer(entries, 0))
        if len(entries) != 4 + 28 * entry_count:
            raise CatalogError("Addressables entry table has an invalid length.")
        self.entries = [struct.unpack_from("<7i", entries, 4 + 28 * n) for n in range(entry_count)]
        self.buckets: list[list[int]] = []
        self.by_key: dict[str, list[int]] = {}
        offset = 4
        for _ in range(bucket_count):
            key_offset = integer(buckets, offset)
            size = count(integer(buckets, offset + 4))
            offset += 8
            if offset + 4 * size > len(buckets):
                raise CatalogError("Addressables bucket is truncated.")
            refs = list(struct.unpack_from(f"<{size}i", buckets, offset))
            offset += 4 * size
            if any(not 0 <= ref < entry_count for ref in refs):
                raise CatalogError("Addressables bucket refers to a missing entry.")
            self.buckets.append(refs)
            if not 4 <= key_offset < len(keys):
                raise CatalogError("Addressables key offset is out of bounds.")
            # Only string keys are needed. Integer/hash/type keys are never executed.
            kind = keys[key_offset]
            if kind in (0, 1):
                length = integer(keys, key_offset + 1)
                if not 0 <= length <= 4096 or key_offset + 5 + length > len(keys):
                    raise CatalogError("Addressables string key is invalid.")
                try:
                    key = keys[key_offset + 5:key_offset + 5 + length].decode("utf-8" if kind == 0 else "utf-16-le")
                except UnicodeError as exc:
                    raise CatalogError("Addressables key encoding is invalid.") from exc
                if key in self.by_key:
                    raise CatalogError("Addressables catalog has duplicate string keys.")
                self.by_key[key] = refs
        if offset != len(buckets):
            raise CatalogError("Addressables bucket table has trailing bytes.")
        for entry in self.entries:
            if not 0 <= entry[0] < len(self.internal_ids) or not -1 <= entry[2] < bucket_count:
                raise CatalogError("Addressables entry has an invalid ID or dependency.")

    def internal_id(self, entry: int) -> str:
        value = self.internal_ids[self.entries[entry][0]]
        prefix, marker, tail = value.rpartition("#")
        if marker and prefix.isdecimal() and self.prefixes:
            index = int(prefix)
            if index >= len(self.prefixes):
                raise CatalogError("Addressables prefix index is out of bounds.")
            value = self.prefixes[index] + tail
        return value

    def icon_locations(self, address: str) -> list[tuple[str, str]]:
        result: set[tuple[str, str]] = set()
        for entry in self.by_key.get(address, []):
            dependency = self.entries[entry][2]
            if dependency < 0:
                continue
            for bundle in self.buckets[dependency]:
                internal = self.internal_id(bundle).replace("\\", "/")
                prefix = "{UnityEngine.AddressableAssets.Addressables.RuntimePath}/StandaloneWindows64/"
                if internal.startswith(prefix):
                    name = internal[len(prefix):]
                    if "/" in name or ":" in name or not name.endswith(".bundle") or name.startswith("."):
                        raise CatalogError("Addressables bundle path escapes its local bundle directory.")
                    result.add((name, self.internal_id(entry)))
        return sorted(result)
