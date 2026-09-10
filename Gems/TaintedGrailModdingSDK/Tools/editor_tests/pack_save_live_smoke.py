# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Exercise the compiled Save mod path in a caller-supplied synthetic workspace."""
import ctypes
import json
import os
from pathlib import Path
import time
import traceback

import azlmbr.legacy.general as general
from PySide6 import QtCore, QtTest, QtWidgets
from shiboken6 import isValid


def run():
    output = Path(os.environ['FOA_SDK_PACK_RESULT'])
    workspace = Path(os.environ['FOA_SDK_PACK_WORKSPACE'])
    fixture = json.loads(workspace.read_text(encoding='utf-8'))
    assert fixture['WorkspaceId'] == 'sdkqa.pack-save', 'Use the synthetic pack-save fixture only'
    assert Path(fixture['RootPath']).resolve() == workspace.parent.resolve()
    result = {'status': 'FAILED', 'checks': [], 'save_seconds': []}
    app = QtWidgets.QApplication.instance()
    keep = []
    timers = []

    def stage(name):
        result['stage'] = name
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')

    def widgets():
        values = app.allWidgets()
        keep.extend(values)
        return [value for value in values if isValid(value)]

    def control(name):
        return next(value for value in reversed(widgets()) if value.objectName() == name)

    def button(text):
        return next(value for value in reversed(widgets())
                    if isinstance(value, QtWidgets.QPushButton) and value.text() == text)

    def type_text(name, text):
        edit = control(name)
        edit.setFocus()
        edit.selectAll()
        QtTest.QTest.keyClicks(edit, text)

    def save(success=True):
        started = time.monotonic()
        button('Save mod').click()
        elapsed = time.monotonic() - started
        result['save_seconds'].append(round(elapsed, 4))
        assert elapsed < 3.0, 'Save mod exceeded the three-second UI budget'
        status = control('packStatus').text()
        assert (status == 'Mod saved. You can start authoring.') == success, status
        return status

    def manifest(path):
        data = json.loads(path.read_text(encoding='utf-8'))
        return data.get('ClassData', data)

    def open_workspace():
        general.open_pane('Tainted Grail SDK Status')
        general.idle_wait(0.5)
        accepted = []
        timer = QtCore.QTimer()
        timers.append(timer)
        def accept():
            dialog = next((value for value in widgets()
                           if isinstance(value, QtWidgets.QFileDialog) and value.isVisible()), None)
            if dialog is None:
                return
            timer.stop()
            keep.append(dialog)
            dialog.setDirectory(str(workspace.parent))
            dialog.selectFile(workspace.name)
            def finish():
                edit = dialog.findChild(QtWidgets.QLineEdit, 'fileNameEdit')
                assert edit is not None
                edit.setText(str(workspace))
                assert Path(dialog.selectedFiles()[0]).resolve() == workspace.resolve()
                accepted.append(True)
                dialog.done(QtWidgets.QDialog.Accepted)
            QtCore.QTimer.singleShot(300, finish)
        timer.timeout.connect(accept)
        timer.start(100)
        QtCore.QMetaObject.invokeMethod(button('Open existing workspace...'), 'click', QtCore.Qt.QueuedConnection)
        for _ in range(50):
            general.idle_wait(0.1)
            if accepted:
                break
        timer.stop()
        assert accepted, 'Workspace dialog did not complete'
        general.idle_wait(0.5)

    try:
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        general.idle_enable(True)
        general.idle_wait(3.0)
        stage('opening_synthetic_workspace')
        open_workspace()
        general.open_pane('Tainted Grail Pack Manager')
        general.idle_wait(0.5)
        button('New mod').click()
        type_text('packDisplayName', 'Original')
        type_text('packOwner', 'sdkqa')
        save()
        original = workspace.parent / 'Packs/sdkqa.original/pack.tgpack.json'
        assert original.is_file(), 'Manifest was not written to the selected workspace'
        assert manifest(original)['DisplayName'] == 'Original'
        assert 'Original' in control('packSavedMods').currentText()
        assert 'Original' in control('packActiveSummary').text()
        result['checks'].append('create_persist_activate_and_saved_list_label')

        stage('invalid_edit')
        saved = original.read_bytes()
        button('Show advanced manifest').click()
        type_text('packDisplayName', 'Correctable draft')
        type_text('packVersion', 'invalid')
        save(False)
        assert original.read_bytes() == saved
        assert 'Original' in control('packActiveSummary').text()
        assert control('packDisplayName').text() == 'Correctable draft'
        assert control('packVersion').text() == 'invalid'
        result['checks'].append('invalid_edit_preserves_active_pack_bytes_and_draft')

        stage('locked_manifest')
        assert os.name == 'nt', 'This exact write-failure check requires Windows file sharing'
        type_text('packVersion', '1.2.3')
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                      ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.CreateFileW(str(original), 0x80000000, 1, None, 3, 0x80, None)
        assert handle not in (None, ctypes.c_void_p(-1).value), ctypes.get_last_error()
        try:
            result['write_failure'] = save(False)
            assert original.read_bytes() == saved
            assert 'Original' in control('packActiveSummary').text()
            assert control('packDisplayName').text() == 'Correctable draft'
        finally:
            kernel.CloseHandle(handle)
        save()
        assert manifest(original)['DisplayName'] == 'Correctable draft'
        assert manifest(original)['Version'] == '1.2.3'
        assert 'Correctable draft' in control('packActiveSummary').text()
        result['checks'].append('real_write_failure_preserves_manifest_and_retry_succeeds')

        stage('new_draft_failure')
        active = control('packActiveSummary').text()
        button('New mod').click()
        assert control('packActiveSummary').text() == active
        type_text('packDisplayName', 'Second')
        type_text('packOwner', 'sdkqa')
        blocked = workspace.parent / 'Packs/sdkqa.second'
        blocked.write_text('synthetic directory blocker', encoding='utf-8')
        try:
            save(False)
            assert control('packActiveSummary').text() == active
            assert control('packDisplayName').text() == 'Second'
            assert blocked.read_text(encoding='utf-8') == 'synthetic directory blocker'
        finally:
            blocked.unlink()
        # A failed first save must keep identity generation enabled.
        type_text('packDisplayName', 'Recovered')
        save()
        recovered = workspace.parent / 'Packs/sdkqa.recovered/pack.tgpack.json'
        assert recovered.is_file()
        assert not (workspace.parent / 'Packs/sdkqa.second').exists()
        assert 'Recovered' in control('packActiveSummary').text()
        result['checks'].append('new_draft_retains_previous_active_mod_and_can_be_renamed_after_failure')

        stage('reopen_saved_mod')
        combo = control('packSavedMods')
        index = next(i for i in range(combo.count()) if Path(combo.itemData(i)).resolve() == original.resolve())
        combo.setCurrentIndex(index)
        button('Open selected').click()
        assert control('packDisplayName').text() == 'Correctable draft'
        assert control('packVersion').text() == '1.2.3'
        assert 'Correctable draft' in control('packActiveSummary').text()
        saved = original.read_bytes()
        save()
        assert original.read_bytes() == saved
        result['checks'].append('saved_mod_reopens_equivalent_and_repeated_save_is_deterministic')
        button('Hide advanced manifest').click()
        general.idle_wait(0.2)
        control('TaintedGrailPackManager').grab().save(str(output.with_suffix('.png')))
        result['status'] = 'PASSED'
        stage('complete')
    except Exception:
        result['error'] = traceback.format_exc()
        stage(result.get('stage', 'failed'))
    finally:
        for timer in timers:
            timer.stop()
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')


run()
