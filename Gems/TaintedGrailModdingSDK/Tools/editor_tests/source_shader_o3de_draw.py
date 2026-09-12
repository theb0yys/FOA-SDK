# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native unchanged-bytecode draws with explicit private inputs; no game parity claim."""
import copy
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

root=Path(os.environ['FOA_NATIVE_SHADER_DRAW_ROOT']).resolve(strict=True)
if any((p/'.git').exists() for p in (root,*root.parents)):
    raise RuntimeError('Source shader acceptance needs private storage.')
fixture=json.loads((root/'draw-fixtures-v1.json').read_text())
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
    general.set_cvar_integer('r_DisplayInfo',0);general.set_viewport_expansion_policy('FixedSize');general.set_viewport_size(800,450);yield 2
    assert call('GetStatus')=='EMPTY'
    descriptors={case['name']:json.loads((root/case['descriptor']).read_text()) for case in fixture['cases']}
    for name,descriptor in descriptors.items():
        record('loading-'+name);started=time.monotonic()
        result=call('SetDraw',json.dumps(descriptor));assert result=='LOADING',result
        yield from wait(lambda:call('GetStatus')!='LOADING')
        assert call('GetStatus')=='READY',call('GetStatus');yield 2
        yield from capture(name)
        report['captures'][-1]['load_and_capture_seconds']=time.monotonic()-started
    valid=descriptors['synthetic'];bad=[]
    # The exact finite float32 extrema occur in captured source samplers.
    changed=copy.deepcopy(valid)
    changed['stages'][1]['samplers'][0]['values'][10:12]=[-3.4028234663852886e38,3.4028234663852886e38]
    assert call('SetDraw',json.dumps(changed))=='LOADING'
    yield from wait(lambda:call('GetStatus')!='LOADING');assert call('GetStatus')=='READY'
    report['finite_sampler_limits']='PASSED'
    for name,value in [('overflow-lod',3.4028235e38),('infinite-lod',float('inf')),('nan-lod',float('nan'))]:
        changed=copy.deepcopy(valid);changed['stages'][1]['samplers'][0]['values'][11]=value
        bad.append((name,json.dumps(changed)))
    changed=copy.deepcopy(valid);changed['version']=3;bad.append(('future-version',json.dumps(changed)))
    changed=copy.deepcopy(valid);changed['vertex_count']=True;bad.append(('bool-count',json.dumps(changed)))
    changed=copy.deepcopy(valid);changed['indices']='ffffffff'*3;bad.append(('out-of-range-index',json.dumps(changed)))
    changed=copy.deepcopy(valid);changed['streams'][0]['hex']='00';bad.append(('short-stream',json.dumps(changed)))
    changed=copy.deepcopy(valid);changed['stages'][1]['constants']*=2;bad.append(('duplicate-buffer',json.dumps(changed)))
    changed=copy.deepcopy(valid);changed['stages'][1]['samplers'][0]['values'][7]=5;bad.append(('invalid-address-mode',json.dumps(changed)))
    changed=copy.deepcopy(valid);changed['stages'][1]['images'][0]['asset']='../escape.streamingimage';bad.append(('escaping-image-path',json.dumps(changed)))
    bad.append(('duplicate-json-key',json.dumps(valid)[:-1]+',"version":1}'))
    bad.append(('unknown-field',json.dumps(valid)[:-1]+',"unexpected":1}'))
    for name,text in bad:
        status=call('SetDraw',text);assert status.startswith('REJECTED:'),(name,status)
        assert call('GetStatus')=='READY','Invalid descriptor replaced the active draw'
        report['rejections'].append({'name':name,'status':'PASSED','native_status':status})
    for name in ('missing-buffer','wrong-buffer-size','wrong-semantic'):
        changed=copy.deepcopy(valid)
        if name=='missing-buffer':changed['stages'][1]['constants']=[]
        elif name=='wrong-buffer-size':changed['stages'][1]['constants'][0]['hex']*=2
        else:changed['streams'][1]['semantic']='UNKNOWN'
        assert call('SetDraw',json.dumps(changed))=='LOADING'
        yield from wait(lambda:call('GetStatus')!='LOADING')
        status=call('GetStatus');assert status.startswith('FAILED:'),(name,status)
        report['rejections'].append({'name':name,'status':'PASSED','native_status':status})
    buffer_cases=[(name,value) for name,value in descriptors.items() if value['version']==2]
    if buffer_cases:
        from source_shader_buffer_cases import invalid_draws, wrong_inputs
        name,baseline=buffer_cases[0]
        assert call('SetDraw',json.dumps(baseline))=='LOADING'
        yield from wait(lambda:call('GetStatus')!='LOADING');assert call('GetStatus')=='READY'
        for label,changed,phase in invalid_draws(baseline):
            status=call('SetDraw',json.dumps(changed))
            if phase=='parse':
                assert status.startswith('REJECTED:'),(label,status)
                assert call('GetStatus')=='READY'
            else:
                assert status=='LOADING',(label,status)
                yield from wait(lambda:call('GetStatus')!='LOADING')
                status=call('GetStatus');assert status.startswith('FAILED:'),(label,status)
                assert call('SetDraw',json.dumps(baseline))=='LOADING'
                yield from wait(lambda:call('GetStatus')!='LOADING');assert call('GetStatus')=='READY'
            report['rejections'].append(dict(name=label,status='PASSED',native_status=status))
        for label,changed in wrong_inputs(baseline):
            assert call('SetDraw',json.dumps(changed))=='LOADING'
            yield from wait(lambda:call('GetStatus')!='LOADING');assert call('GetStatus')=='READY';yield 2
            yield from capture(label)
        assert call('SetDraw',json.dumps(baseline))=='LOADING'
        yield from wait(lambda:call('GetStatus')!='LOADING');assert call('GetStatus')=='READY';yield 2
        yield from capture('buffers_recreated')
        stats=json.loads(call('GetStatistics'))
        expected_data=sum(len(bytes.fromhex(row['hex'])) for stage in baseline['stages'] for row in stage['buffers'])
        expected_constants=sum(len(bytes.fromhex(row['hex'])) for stage in baseline['stages'] for row in stage['constants'])
        assert stats['retired_draws']==0 and stats['read_only_buffer_payload_bytes']==expected_data
        assert stats['constant_payload_bytes']==expected_constants
        report['buffer_statistics']=stats
    if 'failed_pipeline_descriptor' in fixture:
        changed=json.loads((root/fixture['failed_pipeline_descriptor']).read_text())
        assert call('SetDraw',json.dumps(changed))=='LOADING'
        yield from wait(lambda:call('GetStatus')!='LOADING')
        status=call('GetStatus');assert status.startswith('FAILED:'),status
        report['rejections'].append(dict(name='native-pipeline-compilation-failure',status='PASSED',native_status=status))
    # Recreate after failed input, capture again, then prove removal removes the draw.
    assert call('SetDraw',json.dumps(descriptors['source_unlit']))=='LOADING'
    yield from wait(lambda:call('GetStatus')!='LOADING');assert call('GetStatus')=='READY';yield 2
    yield from capture('source_unlit_recreated')
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
