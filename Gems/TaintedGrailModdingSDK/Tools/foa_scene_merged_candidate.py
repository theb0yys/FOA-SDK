# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""In-memory merged-instance matrix candidates; never an archive/game publisher.

Source member/record identity and the original placement packet must all agree.
Only selected instances' twelve stored matrix words can change. Navigation,
lighting, LOD/occlusion products and target-game acceptance remain separate gates.
"""
import hashlib
import struct

import foa_scene_assembly as a
import foa_scene_merged_assembly as merged
from foa_scene_merged_renderers import MergedRenderers


def matrix_candidate(member, placement_packet, edits, archive_sha256, map_key, cancelled=lambda: False):
    """Return private member bytes and a change report from exact native world bits.

    Each edit contains only the complete placement identity and host_world_bits.
    Matrices come from SourceScenePlacementBus.GetWorldMatrixBits, not decomposed
    display transforms. The caller independently binds the archive and map owner.
    No original object, packet, payload, installation or file is modified.
    """
    a.check_cancelled(cancelled)
    a.require(type(member) is MergedRenderers, 'An exact merged source member is required.')
    a.digest(archive_sha256)
    a.require(map_key in a.MAPS, 'Unknown campaign map.')
    a.require(type(member.payload) is bytes and hashlib.sha256(member.payload).hexdigest() == member.sha256,
              'Merged source changed since capture.')
    # Reparse owned bytes: a caller cannot supply modified cached offsets/counts.
    source = MergedRenderers(member.guid, member.payload, cancelled)
    doc = a.decode(placement_packet, a.MAX_BATCH_BYTES)
    merged.validate_document(doc, cancelled)
    expected_source = dict(map=map_key, archive_sha256=archive_sha256, member_guid=source.guid,
                           member_sha256=source.sha256, instance_count=source.sections['instances'].count)
    a.require(doc['source'] == expected_source, 'Placement packet belongs to another source member or map.')
    entries = {}
    for item in doc['entities']:
        a.check_cancelled(cancelled)
        identity = item['binding']['identity']; ordinal = identity['instance_ordinal']
        a.require(source.record('instances', ordinal).hex() == item['record_hex'],
                  'Placement record differs from the source member.')
        entries[ordinal] = item
    a.require(type(edits) is list and 0 < len(edits) <= len(entries), 'Invalid merged edit selection.')
    pending, seen = [], set()
    for edit in edits:
        a.check_cancelled(cancelled)
        a.keys(edit, {'identity', 'host_world_bits'})
        identity = edit['identity']; a.keys(identity, merged.IDENTITY_KEYS)
        ordinal = identity['instance_ordinal']
        a.require(type(ordinal) is int and ordinal in entries and ordinal not in seen,
                  'Unknown or duplicate merged edit instance.')
        seen.add(ordinal)
        a.require(identity == entries[ordinal]['binding']['identity'], 'Merged edit source identity changed.')
        original = source.record('instances', ordinal)
        candidate = _matrix_record(original, edit['host_world_bits'])
        words = [i for i in range(12) if candidate[i*4:i*4+4] != original[i*4:i*4+4]]
        offset = source.sections['instances'].offset + ordinal*56
        pending.append((ordinal, offset, candidate, words))
    data = bytearray(source.payload)
    changes = []
    for ordinal, offset, candidate, words in pending:
        a.check_cancelled(cancelled)
        data[offset:offset+48] = candidate[:48]
        if words:
            changes.append(dict(instance_ordinal=ordinal, record_offset=offset, changed_matrix_words=words,
                                original_record_sha256=hashlib.sha256(source.record('instances', ordinal)).hexdigest(),
                                candidate_record_sha256=hashlib.sha256(candidate).hexdigest()))
    result = bytes(data)
    # Enforce preservation around each changed 48-byte slice, including both
    # reference indices, other instances, section headers and unknown fields.
    cursor = 0
    for _, offset, _, _ in sorted(pending, key=lambda item: item[1]):
        a.check_cancelled(cancelled)
        a.require(result[cursor:offset] == source.payload[cursor:offset], 'Unrelated source bytes changed.')
        cursor = offset+48
    a.require(result[cursor:] == source.payload[cursor:], 'Trailing source bytes changed.')
    verified = MergedRenderers(source.guid, result, cancelled)
    for ordinal, _, candidate, _ in pending:
        a.require(verified.record('instances', ordinal) == candidate, 'Candidate record verification failed.')
    a.check_cancelled(cancelled)
    a.require(member.payload == source.payload and member.sha256 == source.sha256, 'Source changed during candidate preparation.')
    return result, dict(status='PASSED', source=expected_source, candidate_sha256=verified.sha256,
                        selected_instances=len(edits), changed_instances=changes, source_member_unchanged=True,
                        dependent_products='NOT_RUN', game_export='BLOCKED',
                        reason='Derived products and target-game acceptance must be qualified before game return.')


def _matrix_record(original, world):
    """Pure inverse of the independently qualified four-float3-column layout."""
    merged.expand_bits(original)
    a.matrix(world)
    a.require(world[12:] == [0, 0, 0, 0x3f800000], 'Unrepresentable affine bottom-row bits.')
    order = (0, 2, 1, 3)
    columns = [world[order[row]*4+order[col]] for col in range(4) for row in range(3)]
    candidate = struct.pack('<12I', *columns) + original[48:]
    restored = merged.expand_bits(candidate)
    a.require([restored[order[r]*4+order[c]] for r in range(4) for c in range(4)] == world,
              'Candidate matrix cannot reproduce the edited native bits.')
    return candidate
