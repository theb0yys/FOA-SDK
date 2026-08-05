#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Public Alpha thumbnail extractor with mandatory source-size binding."""
from __future__ import annotations

from pathlib import Path
from typing import Any, Mapping

import foa_thumbnail_artifact_extractor_extended as _extended
from foa_thumbnail_artifact_extractor_extended import *  # noqa: F401,F403

_extended_read_source_payload = _extended._read_source_payload


def _read_source_payload(source: Path, record: Mapping[str, Any]) -> bytes:
    declared_size = record.get("ByteSize")
    if not isinstance(declared_size, int) or declared_size < 0:
        raise ThumbnailError(
            "AssetRecord ByteSize must be a non-negative integer."
        )
    return _extended_read_source_payload(source, record)


# Extended build_artifacts resolves this helper through its module globals.
_extended._read_source_payload = _read_source_payload
main = _extended.main


if __name__ == "__main__":
    raise SystemExit(main())
