# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Private native merged-matrix edits, candidate conversion, undo and saved reopen."""
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import time
import traceback
from PySide6 import QtCore
import azlmbr.bus as bus
import azlmbr.components as components
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.math as math
import azlmbr.legacy.general as general
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_scene_assembly_editor import start_import, matrix_bits, placement, source_key
from foa_scene_merged_candidate import matrix_candidate
from foa_scene_merged_renderers import MergedRenderers
from foa_scene_merged_assembly import expand_bits

root = Path(os.environ['FOA_MERGED_CANDIDATE_ROOT']).resolve(strict=True)
assert not any((p/'.git').exists() for p in (root, *root.parents))
reopen = os.environ.get('FOA_MERGED_CANDIDATE_REOPEN') == '1'
fixture = json.loads((root/'fixture.json').read_text())
project = Path(fixture['project']).resolve(strict=True); level = Path(fixture['level']).resolve(strict=True)
assert level.is_relative_to(project) and not any((p/'.git').exists() for p in (project, *project.parents))
output = root/('reopen.json' if reopen else 'editor.json'); assert not output.exists()
member = MergedRenderers(fixture['member_guid'], (root/'member.bin').read_bytes())
raw = (root/'placement.json').read_bytes()
assert member.sha256 == fixture['member_sha256'] and hashlib.sha256(raw).hexdigest() == fixture['placement_sha256']
doc = json.loads(raw); report = dict(status='FAILED', stage='starting', snapshots=[])
started = time.monotonic()


def record(stage):
    report['stage'] = stage; output.write_text(json.dumps(report, indent=2))


def resolve():
    wanted = {source_key(json.dumps(item['binding'])) for item in doc['entities']}; found = {}
    for eid in entity.SearchBus(bus.Broadcast, 'SearchEntities', entity.SearchFilter()):
        descriptor = placement(eid, 'GetSource')
        if descriptor and source_key(descriptor) in wanted:
            key = source_key(descriptor); assert key not in found; found[key] = eid
    assert set(found) == wanted
    return [found[source_key(json.dumps(item['binding']))] for item in doc['entities']]


def snapshot(name):
    edits = []
    for item, eid in zip(doc['entities'], resolve()):
        assert json.loads(placement(eid, 'GetSource')) == item['binding']
        edits.append(dict(identity=item['binding']['identity'], host_world_bits=list(matrix_bits(eid))))
    candidate, result = matrix_candidate(member, raw, edits, doc['source']['archive_sha256'], doc['source']['map'])
    converted = MergedRenderers(member.guid, candidate); order = (0, 2, 1, 3)
    for edit in edits:
        world = expand_bits(converted.record('instances', edit['identity']['instance_ordinal']))
        assert [world[order[r]*4+order[c]] for r in range(4) for c in range(4)] == edit['host_world_bits']
    path = root/(name+'-candidate.bin'); assert not path.exists(); path.write_bytes(candidate)
    value = dict(name=name, edits=edits, candidate_sha256=result['candidate_sha256'],
                 changed_instances=result['changed_instances'], game_export=result['game_export'])
    report['snapshots'].append(value); record(name)
    return value


def begin(name, eid):
    editor.ToolsApplicationRequestBus(bus.Broadcast, 'BeginUndoBatch', name)
    editor.ToolsApplicationRequestBus(bus.Broadcast, 'AddDirtyEntity', eid)


def run():
    general.idle_enable(True); record('opening'); assert general.open_level(str(level)); yield 2
    if reopen:
        previous = json.loads((root/'editor.json').read_text()); assert previous['status'] == 'PASSED'
        value = snapshot('reopened'); saved = previous['snapshots'][-1]
        assert value['edits'] == saved['edits'] and value['candidate_sha256'] == saved['candidate_sha256']
        report['status'] = 'PASSED'; record('complete'); return
    parent = editor.ToolsApplicationRequestBus(bus.Broadcast, 'GetCurrentLevelEntityId')
    completed = []; start_import(raw, parent, completed.append)
    while not completed:
        assert time.monotonic()-started < 180; yield .05
    assert completed[0].status == 'PASSED', completed[0].error
    baseline = snapshot('unchanged'); assert baseline['candidate_sha256'] == member.sha256
    leaf = resolve()[0]
    editor.ToolsApplicationRequestBus(bus.Broadcast, 'SetSelectedEntities', [leaf])
    assert editor.ToolsApplicationRequestBus(bus.Broadcast, 'GetSelectedEntities') == [leaf]
    position = components.TransformBus(bus.Event, 'GetWorldTranslation', leaf)
    begin('Move merged instance', leaf)
    components.TransformBus(bus.Event, 'SetWorldTranslation', leaf, math.Vector3(position.x+2, position.y-1, position.z+.5))
    editor.ToolsApplicationRequestBus(bus.Broadcast, 'EndUndoBatch'); yield .2
    moved = snapshot('moved'); assert len(moved['changed_instances']) == 1
    general.undo(); yield .2
    undone = snapshot('undo'); assert undone['candidate_sha256'] == member.sha256
    general.redo(); yield .2
    redone = snapshot('redo'); assert redone['candidate_sha256'] == moved['candidate_sha256']
    begin('Rotate and scale merged instance', leaf)
    components.TransformBus(bus.Event, 'SetLocalRotationQuaternion', leaf, math.Quaternion_CreateRotationZ(.3))
    components.TransformBus(bus.Event, 'SetLocalUniformScale', leaf, 1.5)
    editor.ToolsApplicationRequestBus(bus.Broadcast, 'EndUndoBatch'); yield .2
    rotated = snapshot('rotate-scale'); assert rotated['candidate_sha256'] != moved['candidate_sha256']
    assert len(rotated['changed_instances']) == 1
    assert general.save_level(); yield 2
    report['status'] = 'PASSED'; record('complete')


job = run()


def step():
    try:
        assert time.monotonic()-started < 300, 'Native candidate probe timed out'
        delay = next(job); QtCore.QTimer.singleShot(round(delay*1000), step)
    except StopIteration:
        general.exit_no_prompt()
    except Exception as error:
        report.update(error=str(error), traceback=traceback.format_exc()); record('failed'); general.exit_no_prompt()
QtCore.QTimer.singleShot(0, step)
