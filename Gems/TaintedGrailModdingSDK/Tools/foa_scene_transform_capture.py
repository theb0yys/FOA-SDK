# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Validate a source-bound Unity Transform capture without promoting runtime proof.

The Unity fixture evaluates directly assigned serialized TRS. Original source bits
remain separate from evaluated matrices; signed zero may be canonicalized by the
Editor. Arithmetic bounds are sanity checks, not a bit-identical host evaluator.
No native transform decomposition or game-return contract is defined here.
"""
import hashlib
import json
import struct

import foa_heightmap_importer as h
from foa_scene_transforms import IDENTITY, affine, finite_values, host_matrix, multiply, source_vector, trs

MAX_INPUT_BYTES = 32 * 1024 * 1024
MAX_NODES = 100000
UNIT_ROUNDOFF = 2. ** -24


def require(condition, message):
    if not condition:
        raise h.HeightmapImportError(message)


def f32(value):
    finite_values([value], 1)
    return struct.unpack('<f', struct.pack('<f', value))[0]


def bits(values):
    return tuple(struct.unpack('<i', struct.pack('<f', f32(v)))[0] for v in values)


def same_number(a, b):
    return a == b or a in (0, -(1 << 31)) and b in (0, -(1 << 31))


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate transform input field.')
        result[key] = value
    return result


def column_matrix(value):
    """Read the inspected serialized float4x4 column schema, preserving all fields."""
    require(type(value) is dict and set(value) == {'c0','c1','c2','c3'}, 'Unsupported source float4x4 schema.')
    columns = [source_vector(value['c'+str(c)], 'xyzw') for c in range(4)]
    return affine(tuple(columns[c][r] for r in range(4) for c in range(4)))


def captured_matrix(row, field):
    values = row.get(field+'Bits')
    require(type(values) is list and len(values) == 16
            and all(type(v) is int and -(1 << 31) <= v < (1 << 31) for v in values), 'Invalid matrix bit capture.')
    decoded = affine(tuple(struct.unpack('<f',struct.pack('<i',v))[0] for v in values))
    json_bits = bits(affine(row[field]))
    require(all(same_number(a,b) for a,b in zip(values,json_bits)), 'Matrix JSON and raw float32 capture disagree.')
    return decoded


def audit_capture(input_bytes, capture, cancelled=lambda: False):
    require(type(input_bytes) is bytes and 0 < len(input_bytes) <= MAX_INPUT_BYTES, 'Transform input exceeds bounds.')
    try:
        document = json.loads(input_bytes, object_pairs_hook=unique_object)
    except (ValueError, UnicodeError) as error:
        raise h.HeightmapImportError('Invalid transform input JSON.') from error
    require(type(document) is dict and set(document) == {'nodes'}, 'Unexpected transform input schema.')
    nodes = document['nodes']
    require(type(nodes) is list and 0 < len(nodes) <= MAX_NODES, 'Invalid transform input count.')
    require(type(capture) is dict and capture.get('schemaVersion') == 3
            and type(capture.get('schemaVersion')) is int and capture.get('status') == 'PASSED'
            and capture.get('unityVersion') == '6000.0.64f1', 'Unqualified Unity Transform capture profile.')
    require(capture.get('inputSha256') == hashlib.sha256(input_bytes).hexdigest(), 'Stale transform input binding.')
    rows = capture.get('rows')
    require(type(rows) is list and len(rows) == len(nodes), 'Incomplete or extra Unity Transform rows.')
    worlds, locals_ = [], []
    zero_changes = rects = 0
    max_local_error = max_parent_error = 0.
    for index, (node, row) in enumerate(zip(nodes, rows)):
        h.check_cancelled(cancelled)
        require(type(node) is dict and set(node) == {'parent','p','q','s','kind'}, 'Unexpected transform node schema.')
        parent = node['parent']
        require(type(parent) is int and -1 <= parent < index and node['kind'] in ('Transform','RectTransform'), 'Invalid ordered transform hierarchy.')
        rects += node['kind'] == 'RectTransform'
        p, q, s = (source_vector(node[k], axes) for k, axes in (('p','xyz'),('q','xyzw'),('s','xyz')))
        expected_bits = bits((*p,*q,*s))
        require(type(row) is dict and set(row) == {'world','local','p','s','q','inputBits','assignedBits','localBits','worldBits'}, 'Unexpected Unity Transform row schema.')
        for field in ('inputBits','assignedBits'):
            values = row[field]
            require(type(values) is list and len(values) == 10
                    and all(type(v) is int and -(1 << 31) <= v < (1 << 31) for v in values), 'Invalid Transform bit capture.')
        require(tuple(row['inputBits']) == expected_bits, 'Unity input parsing changed source TRS bits.')
        require(all(same_number(a,b) for a,b in zip(expected_bits,row['assignedBits'])), 'Unity assignment changed source TRS values.')
        zero_changes += sum(a != b for a,b in zip(expected_bits,row['assignedBits']))
        actual = tuple(v for k, axes in (('p','xyz'),('q','xyzw'),('s','xyz')) for v in source_vector(row[k],axes))
        require(all(same_number(a,b) for a,b in zip(bits(actual),row['assignedBits'])), 'Transform readback disagrees with captured bits.')
        local = captured_matrix(row,'local')
        world = captured_matrix(row,'world')
        expected = trs(p,q,s)
        for r in range(3):
            require(local[r*4+3] == f32(p[r]), 'Local matrix changed source translation.')
            for c in range(3):
                error = abs(local[r*4+c]-expected[r*4+c])
                require(error <= 8*UNIT_ROUNDOFF*max(abs(s[c]),1e-38), 'Local matrix disagrees with source TRS.')
                max_local_error = max(max_local_error,error)
        if parent < 0:
            require(all(same_number(a,b) for a,b in zip(bits(local),bits(world))), 'Root local and world matrices disagree.')
        else:
            parent_world = worlds[parent]
            product = multiply(parent_world,local)
            # Unity may combine uniform parent TRS before materializing a matrix.
            # A norm-based check permits floating operation ordering/cancellation;
            # the evaluated Unity float32 matrix, not this product, is retained.
            for r in range(3):
                parent_norm = sum(abs(parent_world[r*4+k]) for k in range(4))
                for c in range(4):
                    magnitude = parent_norm*max(abs(local[k*4+c]) for k in range(4))
                    error = abs(world[r*4+c]-product[r*4+c])
                    require(error <= 32*UNIT_ROUNDOFF*max(magnitude,1e-38), 'World matrix disagrees with its source parent.')
                    max_parent_error = max(max_parent_error,error)
        locals_.append(local);worlds.append(world)
    require(type(capture.get('rectTransforms')) is int and capture['rectTransforms'] == rects, 'RectTransform capture accounting disagrees.')
    return {'status':'PASSED','source_trs_values':'PASSED','transforms':len(nodes),
            'signed_zero_canonicalizations':zero_changes,'rect_transforms':rects,
            'max_local_arithmetic_error':max_local_error,'max_parent_arithmetic_error':max_parent_error,
            'source_local_matrices':locals_,'source_world_matrices':worlds,
            'host_world_matrices':[host_matrix(m) for m in worlds],
            'matrix_authority':'Unity serialized Transform evaluation with raw float32 bits; original source records remain required',
            'native_scene_projection':'NOT_RUN','runtime_scripts':'NOT_RUN','game_export':'NOT_RUN'}
