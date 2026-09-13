# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Capture explicit private material-uniform draws in the pinned native Editor.

A separate reader compares source and independent reference images. Successful
submission alone does not establish rendering, material fidelity or game parity.
"""
import hashlib
import json
import os
from pathlib import Path
import time
import traceback
from PySide6 import QtCore
import azlmbr.atom as atom
import azlmbr.bus as bus
import azlmbr.foa as foa
import azlmbr.legacy.general as general

root = Path(os.environ['FOA_MATERIAL_UNIFORM_NATIVE_ROOT']).resolve(strict=True)
if any((p / '.git').exists() for p in (root, *root.parents)):
    raise RuntimeError('Material-uniform captures must stay outside Git.')
fixture_path = root / 'fixture.json'
if fixture_path.stat().st_size > 4 * 1024 * 1024:
    raise RuntimeError('Material-uniform fixture exceeds its bound.')
fixture = json.loads(fixture_path.read_text())
cases = fixture['cases']
if not isinstance(cases, list) or not 1 <= len(cases) <= 32:
    raise RuntimeError('Material-uniform case count exceeds its bound.')
output = root / 'native-editor.json'
if output.exists():
    raise RuntimeError('Material-uniform output already exists.')
report = dict(status='PARTIAL', stage='starting', captures=[])
handlers = []


def call(event, *args):
    return foa.SourceShaderRenderBus(bus.Broadcast, event, *args)


def record(stage):
    report['stage'] = stage
    output.write_text(json.dumps(report, indent=2))


def wait(predicate, seconds=60):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if predicate():
            return
        yield .2
    raise RuntimeError('Timed out at ' + report['stage'] + ': ' + call('GetStatus'))


def run():
    project = Path(fixture['project']).resolve(strict=True)
    if any((p / '.git').exists() for p in (project, *project.parents)):
        raise RuntimeError('Private native project required.')
    general.idle_enable(True)
    assert general.open_level(str(project / 'Levels/MaterialGpuProbe/MaterialGpuProbe.prefab'))
    yield 2
    general.set_cvar_integer('r_DisplayInfo', 0)
    general.set_viewport_expansion_policy('FixedSize')
    general.set_viewport_size(800, 450)
    yield 2
    assert call('GetStatus') == 'EMPTY'
    seen = set()
    for case in cases:
        name = case['name']
        assert type(name) is str and 0 < len(name) <= 100 and all(c.isascii() and (c.isalnum() or c in '-_') for c in name)
        assert name not in seen
        seen.add(name)
        path = (root / case['descriptor']).resolve(strict=True)
        assert path.parent == root and path.stat().st_size <= 4 * 1024 * 1024
        record('loading-' + name)
        descriptor = path.read_text()
        assert call('SetDraw', descriptor) == 'LOADING'
        yield from wait(lambda: call('GetStatus') != 'LOADING')
        assert call('GetStatus') == 'READY', call('GetStatus')
        yield 1
        record('capturing-' + name)
        capture = root / (name + '.ppm')
        assert not capture.exists()
        outcome = atom.FrameCaptureRequestBus(bus.Broadcast, 'CaptureScreenshot', str(capture))
        assert outcome.IsSuccess()
        done = []
        handler = atom.FrameCaptureNotificationBusHandler()
        handler.connect(outcome.GetValue())
        handler.add_callback('OnFrameCaptureFinished', lambda args: done.append(args))
        handlers.append(handler)
        yield from wait(lambda: bool(done), 30)
        assert done[0][0] == atom.FrameCaptureResult_Success
        handler.disconnect()
        assert 0 < capture.stat().st_size <= 64 * 1024 * 1024
        report['captures'].append(dict(name=name, path=str(capture), sha256=hashlib.sha256(capture.read_bytes()).hexdigest(),
                                       descriptor_sha256=hashlib.sha256(descriptor.encode()).hexdigest()))
    call('ClearDraw')
    assert call('GetStatus') == 'EMPTY' and json.loads(call('GetStatistics'))['resident_payload_bytes'] == 0
    report.update(native_draw='PASSED', pixel_comparison='NOT_RUN', cleanup='PASSED')
    record('captured')


job = run()


def step():
    try:
        delay = next(job)
        QtCore.QTimer.singleShot(round(delay * 1000), step)
    except StopIteration:
        general.exit_no_prompt()
    except Exception as error:
        report.update(status='FAILED', error=str(error), traceback=traceback.format_exc())
        try:
            record('failed')
        except Exception:
            traceback.print_exc()
        finally:
            try:
                call('ClearDraw')
            finally:
                general.exit_no_prompt()


QtCore.QTimer.singleShot(0, step)
