# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Exercise the production hierarchy importer on private four-map batches and reopen.

No rendered-scene or game-return claim is made. Every imported source matrix,
identity, parent, name and effective visibility is checked in both processes.
"""
import copy
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
from foa_scene_assembly import AssemblyJob, HeightmapImportError, validate_batch
from foa_scene_assembly_editor import NativeHierarchyAdapter, info, matrix_bits, placement, source_key, start_import

root = Path(os.environ['FOA_SCENE_ASSEMBLY_ROOT']).resolve(strict=True)
assert not any((p/'.git').exists() for p in (root, *root.parents))
fixture = json.loads((root/'fixture.json').read_text())
reopen = os.environ.get('FOA_SCENE_ASSEMBLY_REOPEN') == '1'
output = root/('reopen.json' if reopen else 'editor.json')
assert not output.exists()
report = {'status': 'FAILED', 'stage': 'starting', 'scenes': [], 'controls': []}
started = time.monotonic()
import foa_scene_assembly_editor as native_module
report['loaded_modules'] = {str(Path(module.__file__).resolve()): hashlib.sha256(Path(module.__file__).read_bytes()).hexdigest()
                            for module in (native_module, sys.modules['foa_scene_assembly'])}


def record(stage):
    report['stage'] = stage
    output.write_text(json.dumps(report, indent=2))


def all_entities():
    return entity.SearchBus(bus.Broadcast, 'SearchEntities', entity.SearchFilter())


def resolve(rows):
    wanted = {source_key(row.descriptor) for row in rows}; result = {}
    for eid in all_entities():
        descriptor = placement(eid, 'GetSource')
        if descriptor and source_key(descriptor) in wanted:
            key = source_key(descriptor); assert key not in result
            result[key] = eid
    return result


def audit(rows, adapter):
    found = resolve(rows); assert len(found) == len(rows)
    records = []; ids = []
    for row in rows:
        eid = found[source_key(row.descriptor)]
        parent = ids[row.parent] if row.parent >= 0 else None
        adapter.verify(eid, row, parent); ids.append(eid)
        records.append({'source': hashlib.sha256(placement(eid, 'GetSource').encode()).hexdigest(),
                        'parent': row.parent, 'name': info(eid, 'GetName'), 'visible': info(eid, 'IsVisible'),
                        'matrix_bits': matrix_bits(eid)})
    digest = hashlib.sha256(json.dumps(records, separators=(',', ':'), ensure_ascii=True).encode()).hexdigest()
    return {'entities': len(records), 'state_sha256': digest}, ids


def wait(predicate, timeout=120):
    deadline = time.monotonic()+timeout
    while not predicate():
        assert time.monotonic() < deadline, 'Native assembly wait expired'
        yield .02


def run():
    general.idle_enable(True)
    previous = json.loads((root/'editor.json').read_text()) if reopen else None
    if reopen: assert previous['status'] == 'PASSED'
    for number, entry in enumerate(fixture['scenes']):
        raw = Path(entry['batch']).read_bytes()
        assert hashlib.sha256(raw).hexdigest() == entry['batch_sha256']
        rows = validate_batch(raw); level = Path(entry['level']).resolve(strict=True)
        project = Path(fixture['project']).resolve(strict=True)
        assert level.is_relative_to(project) and not any((p/'.git').exists() for p in (project, *project.parents))
        record('opening-'+entry['name']); assert general.open_level(str(level)); yield 1
        parent = editor.ToolsApplicationRequestBus(bus.Broadcast, 'GetCurrentLevelEntityId')
        adapter = NativeHierarchyAdapter(parent)
        if reopen:
            actual, ids = audit(rows, adapter)
            before = next(s for s in previous['scenes'] if s['name'] == entry['name'])
            assert actual == before['saved'], 'Native saved hierarchy changed on reopening'
            report['scenes'].append(dict(name=entry['name'], saved=actual)); record('reopened-'+entry['name']); continue
        assert not resolve(rows)
        sentinel = editor.ToolsApplicationRequestBus(bus.Broadcast, 'CreateNewEntityAtPosition', math.Vector3(7., 8., 9.), parent)
        editor.EditorEntityAPIBus(bus.Event, 'SetName', sentinel, 'Unrelated native entity')
        initial_ids = set(e.ToString() for e in all_entities())
        if number == 0:
            record('cancelling-production-import')
            completed = []
            controller = start_import(raw, parent, completed.append)
            def cancel_after_creation():
                if completed: return
                if controller['job'].created: controller['dialog'].cancel()
                else: QtCore.QTimer.singleShot(1, cancel_after_creation)
            QtCore.QTimer.singleShot(1, cancel_after_creation)
            yield from wait(lambda: bool(completed))
            cancelled = completed[0]
            report['cancel_result'] = {'status': cancelled.status, 'created': len(cancelled.created), 'error': cancelled.error, 'cleanup_errors': [(str(e), message) for e, message in cancelled.cleanup_errors]}
            record('cancellation-finished')
            assert cancelled.status == 'CANCELLED' and cancelled.created and not cancelled.cleanup_errors
            assert not resolve(rows) and set(e.ToString() for e in all_entities()) == initial_ids
            report['controls'].append({'name': 'modal-cancellation-rollback', 'status': 'PASSED', 'created': len(cancelled.created)})
            class FailingAdapter(NativeHierarchyAdapter):
                configured = 0
                def configure(self, eid, row):
                    super().configure(eid, row); self.configured += 1
                    if self.configured == 7: raise RuntimeError('Injected failure after native binding')
            editor.ToolsApplicationRequestBus(bus.Broadcast, 'BeginUndoBatch', 'Assembly rollback control')
            failed = AssemblyJob(raw, FailingAdapter(parent))
            while failed.step() not in ('FAILED', 'CLEANUP_FAILED', 'PASSED'): yield .01
            editor.ToolsApplicationRequestBus(bus.Broadcast, 'EndUndoBatch')
            assert failed.status == 'FAILED' and len(failed.created) == 7 and not failed.cleanup_errors
            assert not resolve(rows) and set(e.ToString() for e in all_entities()) == initial_ids
            report['controls'].append({'name': 'late-native-failure-rollback', 'status': 'PASSED', 'created': len(failed.created)})
        record('importing-'+entry['name']); completed = []; phase_start = time.monotonic()
        controller = start_import(raw, parent, completed.append)
        yield from wait(lambda: bool(completed), timeout=240)
        job = completed[0]
        assert job.status == 'PASSED', (job.status, job.error, job.cleanup_errors)
        elapsed = time.monotonic()-phase_start
        current, ids = audit(rows, adapter)
        assert job.peak_step_entities <= 32
        assert info(sentinel, 'GetName') == 'Unrelated native entity'
        if number == 0:
            record('undo-redo-production-import')
            general.undo(); yield .5
            assert not resolve(rows) and set(e.ToString() for e in all_entities()) == initial_ids
            general.redo()
            def redo_ready():
                counts = {'native': len(all_entities()), 'source': len(resolve(rows))}
                if not report.get('redo_counts') or report['redo_counts'][-1] != counts:
                    report.setdefault('redo_counts', []).append(counts); record('waiting-for-redo-propagation')
                return counts['source'] == len(rows)
            yield from wait(redo_ready, timeout=30)
            assert audit(rows, adapter)[0] == current
            report['controls'].append({'name': 'single-operation-undo-redo', 'status': 'PASSED'})
            ids = audit(rows, adapter)[1]
            before_ids = set(e.ToString() for e in all_entities())
            try: start_import(raw, parent, lambda _: None)
            except HeightmapImportError: pass
            else: raise AssertionError('Duplicate source hierarchy accepted')
            assert before_ids == set(e.ToString() for e in all_entities())
            report['controls'].append({'name': 'duplicate-preflight-no-mutation', 'status': 'PASSED'})
            # A source parent with the same CAB/path ID but different bundle is not interchangeable.
            child_index = next(i for i, row in enumerate(rows) if row.parent >= 0)
            child = rows[child_index]; bad = json.loads(child.descriptor)
            bad['identity']['bundle_sha256'] = 'f'*64
            eid = adapter.create(ids[child.parent])
            world = struct.unpack('<16f', struct.pack('<16I', *child.host_world_bits))
            components.TransformBus(bus.Event, 'SetWorldTranslation', eid, math.Vector3(world[3], world[7], world[11]))
            assert editor.EditorComponentAPIBus(bus.Broadcast, 'AddComponentsOfType', eid, [adapter.component_type]).IsSuccess()
            assert not placement(eid, 'BindSource', json.dumps(bad)), 'Cross-bundle native parent accepted'
            adapter.remove(eid)
            report['controls'].append({'name': 'cross-bundle-parent-rejected', 'status': 'PASSED'})
            rejected = 0
            for mode in ('mismatch', 'nonfinite', 'future'):
                bad = json.loads(child.descriptor)
                if mode == 'mismatch': bad['native_anchor_bits'][0] ^= 0x00800000
                if mode == 'nonfinite': bad['native_anchor_bits'][0] = 0x7fc00000
                if mode == 'future': bad['schema_version'] = 3
                eid = adapter.create(ids[child.parent])
                components.TransformBus(bus.Event, 'SetWorldTranslation', eid, math.Vector3(world[3], world[7], world[11]))
                assert editor.EditorComponentAPIBus(bus.Broadcast, 'AddComponentsOfType', eid, [adapter.component_type]).IsSuccess()
                assert not placement(eid, 'BindSource', json.dumps(bad)), 'Malformed native anchor binding accepted'
                adapter.remove(eid); rejected += 1
            report['controls'].append({'name': 'native-anchor-negatives', 'status': 'PASSED', 'rejected': rejected})
        rounded = [i for i, row in enumerate(rows) if struct.unpack('<3f', struct.pack('<3I', *row.native_anchor_bits)) !=
                   tuple(struct.unpack('<f', struct.pack('<I', row.host_world_bits[k]))[0] for k in (3, 7, 11))]
        if rounded and not any(c['name'] == 'rounded-anchor-edit-undo-redo' for c in report['controls']):
            index = rounded[0]; eid = ids[index]; row = rows[index]
            native = components.TransformBus(bus.Event, 'GetWorldTranslation', eid)
            editor.ToolsApplicationRequestBus(bus.Broadcast, 'BeginUndoBatch', 'Move exact source placement')
            editor.ToolsApplicationRequestBus(bus.Broadcast, 'AddDirtyEntity', eid)
            components.TransformBus(bus.Event, 'SetWorldTranslation', eid, math.Vector3(native.x+1., native.y, native.z))
            editor.ToolsApplicationRequestBus(bus.Broadcast, 'EndUndoBatch')
            moved = struct.unpack('<16f', struct.pack('<16I', *matrix_bits(eid)))
            original = struct.unpack('<16f', struct.pack('<16I', *row.host_world_bits))
            expected_x = struct.unpack('<f', struct.pack('<f', original[3]+1.))[0]
            assert moved[3] == expected_x and all(moved[k] == original[k] for k in range(16) if k != 3)
            assert placement(eid, 'GetSource') == row.descriptor
            general.undo(); yield .3
            assert audit(rows, adapter)[0] == current
            general.redo(); yield .3
            assert struct.unpack('<16f', struct.pack('<16I', *matrix_bits(eid))) == moved
            general.undo(); yield .3
            assert audit(rows, adapter)[0] == current
            report['controls'].append({'name': 'rounded-anchor-edit-undo-redo', 'status': 'PASSED', 'source': list(row.key)})
        assert not placement(ids[0], 'CommitAssemblyState'), 'Assembly commit accepted without an undo transaction'
        editor.ToolsApplicationRequestBus(bus.Broadcast, 'SetSelectedEntities', [ids[-1]])
        assert editor.ToolsApplicationRequestBus(bus.Broadcast, 'GetSelectedEntities') == [ids[-1]]
        assert general.save_level(); yield .3
        saved, _ = audit(rows, adapter); assert saved == current
        report['scenes'].append({'name': entry['name'], 'saved': saved, 'peak_step_entities': job.peak_step_entities,
                                 'elapsed_seconds': elapsed, 'complete_hierarchy': entry['complete_hierarchy']})
        record('saved-'+entry['name'])
    record('reading-version-one-saved-level')
    legacy = fixture['legacy']; level = Path(legacy['level']).resolve(strict=True)
    assert level.is_relative_to(Path(fixture['project']).resolve(strict=True))
    assert hashlib.sha256(level.read_bytes()).hexdigest() == legacy['sha256']
    assert general.open_level(str(level)); yield 1
    found, bindings = {}, {}
    for eid in all_entities():
        descriptor = placement(eid, 'GetSource')
        if descriptor:
            assert json.loads(descriptor)['schema_version'] == 1
            key = hashlib.sha256(descriptor.encode()).hexdigest()
            assert key not in bindings, 'Duplicate version-one source binding'
            bindings[key] = descriptor
            if key in legacy['matrices']: found[key] = list(matrix_bits(eid))
    report['legacy_readback'] = {'matrices': found, 'binding_hashes': sorted(bindings)}
    assert bindings == legacy['bindings'], 'Version-one saved source bindings changed'
    assert found == legacy['matrices'], 'Version-one edited placements changed'
    assert hashlib.sha256(level.read_bytes()).hexdigest() == legacy['sha256']
    report['legacy_version_one'] = {'status': 'PASSED', 'bindings': len(bindings), 'edited_matrices': len(found)}
    report['subnormal_coefficients'] = sum(
        0 < (value & 0x7fffffff) < 0x00800000
        for entry in fixture['scenes'] for row in validate_batch(Path(entry['batch']).read_bytes())
        for value in row.host_world_bits)
    assert report['subnormal_coefficients'] > 0, 'Subnormal native matrix coverage is missing'
    report.update(status='PASSED', elapsed_seconds=time.monotonic()-started,
                  entities=sum(s['saved']['entities'] for s in report['scenes']),
                  complete_rendered_scenes='NOT_RUN', game_export='NOT_RUN')
    record('complete')


job = run()


def step():
    try:
        assert time.monotonic()-started < 1800, 'Native source assembly deadline exceeded'
        delay = next(job); QtCore.QTimer.singleShot(round(delay*1000), step)
    except StopIteration:
        general.exit_no_prompt()
    except Exception as error:
        report.update(error=str(error), traceback=traceback.format_exc()); record('failed'); general.exit_no_prompt()
QtCore.QTimer.singleShot(0, step)
