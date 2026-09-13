# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Original DOTS program buffer test with synthetic inputs; no full-scene parity claim."""
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

root=Path(os.environ['FOA_DOTS_DRAW_ROOT']).resolve(strict=True)
if any((p/'.git').exists() for p in (root,*root.parents)):
    raise RuntimeError('Source shader acceptance needs private storage.')
fixture=json.loads((root/'fixture.json').read_text())
output=root/'native-editor-v1.json'
if output.exists():raise RuntimeError('Acceptance output already exists.')
report={'status':'FAILED','stage':'starting','captures':[],'rejections':[]};handlers=[]


def record(stage):
    report['stage']=stage;output.write_text(json.dumps(report,indent=2))


def call(event,*args):return foa.SourceShaderRenderBus(bus.Broadcast,event,*args)


def wait(predicate,seconds=90):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        if predicate():return
        yield .2
    raise RuntimeError('Timed out: '+report['stage']+'; native status: '+call('GetStatus'))


def capture(name):
    record('capturing-'+name);path=root/(name+'-native.ppm')
    if path.exists():raise RuntimeError('Capture already exists.')
    outcome=atom.FrameCaptureRequestBus(bus.Broadcast,'CaptureScreenshot',str(path));assert outcome.IsSuccess()
    done=[];handler=atom.FrameCaptureNotificationBusHandler();handler.connect(outcome.GetValue())
    handler.add_callback('OnFrameCaptureFinished',lambda args:done.append(args));handlers.append(handler)
    yield from wait(lambda:bool(done),30)
    assert done[0][0]==atom.FrameCaptureResult_Success;handler.disconnect()
    report['captures'].append({'name':name,'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})


def run():
    project=Path(fixture['project']).resolve(strict=True)
    assert not any((p/'.git').exists() for p in (project,*project.parents))
    general.idle_enable(True);record('opening-private-level')
    assert general.open_level(str(project/'Levels/MaterialGpuProbe/MaterialGpuProbe.prefab'));yield 2
    general.set_cvar_integer('r_DisplayInfo',0);general.set_cvar_integer('ed_keepEditorActive',1)
    general.set_viewport_expansion_policy('FixedSize');general.set_viewport_size(800,450);yield 2
    assert call('GetStatus')=='EMPTY'
    for case in fixture['cases']:
        descriptor=json.loads((root/case['descriptor']).read_text())
        record('loading-'+case['name']);started=time.monotonic()
        assert call('SetDraw',json.dumps(descriptor))=='LOADING'
        yield from wait(lambda:call('GetStatus')!='LOADING')
        assert call('GetStatus')=='READY',call('GetStatus');yield 2
        yield from capture(case['name'])
        report['captures'][-1]['load_and_capture_seconds']=time.monotonic()-started
        stats=json.loads(call('GetStatistics'))
        assert stats['read_only_buffer_payload_bytes']==case['raw_payload_bytes']
        assert stats['constant_payload_bytes']==case['constant_bytes']
    call('ClearDraw');assert call('GetStatus')=='EMPTY';yield 2
    yield from capture('cleared')
    assert json.loads(call('GetStatistics'))['resident_payload_bytes']==0
    report.update(status='PARTIAL',native_draw='PASSED',pixel_comparison='NOT_RUN');record('captured')


job=run()


def step():
    try:
        delay=next(job);QtCore.QTimer.singleShot(round(delay*1000),step)
    except StopIteration:
        general.exit_no_prompt()
    except Exception as error:
        report.update(error=str(error),traceback=traceback.format_exc());record('failed');call('ClearDraw');general.exit_no_prompt()
QtCore.QTimer.singleShot(0,step)
