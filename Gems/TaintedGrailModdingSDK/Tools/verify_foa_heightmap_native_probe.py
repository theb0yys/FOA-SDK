#!/usr/bin/env python3
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Independently check M6 native fixture outputs. Never launches tools or grants authority."""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import stat
import struct

RESOLUTION = 33
SAMPLES = RESOLUTION ** 2
TOLERANCE = 2 / 65535 + 0.0000001
AUTHORITY = ("runtimeUseAllowed", "deploymentAllowed", "publicationAllowed", "packagingAllowed", "gameWriteAllowed", "evidencePromotionAllowed")
STAGES = ("created", "asset-reopened", "bundle-a", "bundle-b", "fresh-process")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sample(x: int, row: int) -> int:
    corners = {(0, 0): 0, (32, 0): 16384, (0, 32): 49151, (32, 32): 65535}
    return corners.get((x, row), (977 * x + 331 * row + 17 * x * row + 123) % 65536)


def source_bytes() -> bytes:
    return struct.pack('<1089H', *(sample(x, row) for row in range(33) for x in range(33)))


def read(root: Path, relative: str, limit: int) -> bytes:
    require(relative and not relative.startswith('/') and '\\' not in relative and ':' not in relative,
            "Unsafe evidence path")
    parts = relative.split('/')
    require(all(p not in ('', '.', '..') for p in parts), "Unsafe evidence path")
    path = root
    for part in parts:
        path = path / part
        info = path.lstat()
        require(not stat.S_ISLNK(info.st_mode) and not getattr(info, 'st_file_attributes', 0) & 0x400,
                "Linked evidence path")
    require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and 0 < info.st_size <= limit,
            "Invalid evidence file or size")
    with path.open('rb') as file:
        data = file.read(limit + 1)
    require(len(data) == info.st_size, "Evidence changed or exceeds its bound")
    return data


def _pairs(pairs: list) -> dict:
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate report field")
        result[key] = value
    return result


def report(root: Path, name: str) -> dict:
    result = json.loads(read(root, name, 65536), object_pairs_hook=_pairs,
                        parse_constant=lambda _: (_ for _ in ()).throw(ValueError('Nonfinite JSON')))
    require(type(result) is dict, "Report is not an object")
    require(result.get('schema') == 'foa.m6.native-heightmap-qualification' and
            type(result.get('schemaVersion')) is int and result['schemaVersion'] == 1, "Unsupported report version")
    require(result.get('status') == 'PASSED' and result.get('error') == '', "Native probe did not pass")
    require(result.get('unityVersion') == '6000.0.64f1', "Wrong qualification Editor")
    require(all(result.get(key) is False for key in AUTHORITY), "Evidence cannot grant authority")
    require(result.get('width') == 33 and result.get('height') == 33 and result.get('sampleSpacingMetres') == 1 and
            result.get('minimumHeightMetres') == -2 and result.get('maximumHeightMetres') == 6, "Fixture geometry changed")
    require(result.get('normalizedTolerance') == TOLERANCE, "Tolerance changed")
    require(result.get('rejectionChecks') == 4, "Required malformed-length checks missing")
    require(result.get('sourceSha256') == sha(source_bytes()), "Wrong source fixture identity")
    return result


def verify(root: Path) -> dict:
    root = Path(root).absolute()
    for ancestor in (root, *root.parents):
        info = ancestor.lstat()
        require(not stat.S_ISLNK(info.st_mode) and not getattr(info, 'st_file_attributes', 0) & 0x400,
                "Linked evidence root")
    require(read(root, 'source.u16le', 2178) == source_bytes(), "Source bytes changed")
    initial, reopened = report(root, 'result.json'), report(root, 'reopen.json')
    require(re.fullmatch('[0-9a-f]{32}', initial.get('terrainGuid', '')) is not None and
            re.fullmatch('[0-9a-f]{32}', initial.get('prefabGuid', '')) is not None and
            initial['terrainGuid'] != initial['prefabGuid'], "Missing distinct native GUIDs")
    require([o.get('stage') for o in initial.get('observations', [])] == list(STAGES[:4]) and
            [o.get('stage') for o in reopened.get('observations', [])] == ['fresh-process'], "Missing or duplicate readback stages")
    expected = [sample(x, 32 - z) / 65535 for z in range(33) for x in range(33)]
    measured = []
    for observation in initial['observations'] + reopened['observations']:
        stage = observation['stage']
        data = read(root, stage + '.f32le', SAMPLES * 4)
        require(len(data) == SAMPLES * 4 and sha(data) == observation.get('decodedSha256'), "Invalid readback bytes/hash")
        actual = struct.unpack('<1089f', data)
        require(all(math.isfinite(value) and 0 <= value <= 1 for value in actual), "Invalid native height")
        error = max(abs(a - e) for a, e in zip(actual, expected))
        require(error <= TOLERANCE, "Native height or orientation differs from source")
        require(observation.get('comparedSamples') == SAMPLES and
                abs(observation.get('maxNormalizedError', -1) - error) < 1e-12, "False comparison summary")
        require(observation.get('cornerHeights') == [actual[0], actual[32], actual[1056], actual[1088]], "Corner summary differs")
        require(observation.get('size') == {'x': 32, 'y': 8, 'z': 32} and
                observation.get('origin') == {'x': 0, 'y': -2, 'z': 0}, "Native extent/origin differs")
        require(observation.get('collisionProbes') == (0 if stage == 'asset-reopened' else 5), "Missing collision observations")
        for field, bound in [('maxElevationErrorMetres', TOLERANCE * 8), ('maxCollisionErrorMetres', TOLERANCE * 8 + 0.00001)]:
            value = observation.get(field, -1)
            require(type(value) in (int, float) and math.isfinite(value) and 0 <= value <= bound, "Invalid metre observation")
        measured.append(error)
    expected_paths = {folder + '/' + name for folder in ('build-a', 'build-b')
                      for name in ('foa-m6-terrain', 'foa-m6-terrain.manifest', folder, folder + '.manifest')}
    artifacts = initial.get('artifacts', [])
    require(len(artifacts) == len(expected_paths) and {a.get('path') for a in artifacts} == expected_paths,
            "Unexpected, missing or duplicate native artifact")
    for artifact in artifacts:
        data = read(root, artifact['path'], 64 * 1024 * 1024)
        require(len(data) == artifact.get('bytes') and sha(data) == artifact.get('sha256'), "Native artifact drift")
    equal = read(root, 'build-a/foa-m6-terrain', 64 * 1024 * 1024) == read(root, 'build-b/foa-m6-terrain', 64 * 1024 * 1024)
    require(initial.get('repeatedBundleBytesEqual') is equal, "False repeatability summary")
    return {'status': 'PASSED', 'scope': 'native authoring candidate evidence only', 'readback_stages': 5,
            'samples_per_stage': SAMPLES, 'collision_probes': 20, 'max_normalized_error': max(measured),
            'max_height_error_metres': max(measured) * 8, 'repeated_bundle_bytes_equal': equal,
            'runtime_signoff': 'NOT_RUN', 'm2_isolation_qualification': 'NOT_RUN'}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        result = verify(args.output)
    except (ValueError, OSError, KeyError, TypeError, struct.error) as error:
        print(json.dumps({'status': 'FAILED', 'error': str(error)}))
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
