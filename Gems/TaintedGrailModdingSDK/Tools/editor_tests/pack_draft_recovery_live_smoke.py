# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Compiled-Editor recovery acceptance; the parent owns and verifies forced exits."""
import ctypes
import hashlib
import json
import os
import sys
from pathlib import Path
import time
import traceback

import azlmbr.editor as editor
import azlmbr.legacy.general as general
from PySide6 import QtCore, QtTest, QtWidgets
from shiboken6 import isValid

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pack_pane_test_support as pane_test


def run():
    output = Path(os.environ['FOA_SDK_RECOVERY_RESULT'])
    workspace = Path(os.environ['FOA_SDK_PACK_WORKSPACE'])
    case = os.environ['FOA_SDK_RECOVERY_CASE']
    docked = os.environ.get('FOA_SDK_PACK_DOCKED') == '1'
    expected_path = os.environ.get('FOA_SDK_RECOVERY_EXPECTED', '')
    expected = json.loads(Path(expected_path).read_text(encoding='utf-8')) if expected_path else None
    result = {'status': 'PARTIAL', 'case': case, 'checks': [], 'transition_seconds': [],
              'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    keep = []

    def stage(name):
        result['stage'] = name
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')

    def widgets():
        values = app.allWidgets()
        keep.extend(values)
        return [value for value in values if isValid(value)]

    def control(name):
        return next(value for value in widgets() if value.objectName() == name)

    def button(text):
        return next(value for value in root.findChildren(QtWidgets.QPushButton) if value.text() == text)

    def wait_for(predicate, label):
        start = time.monotonic()
        while not predicate():
            assert time.monotonic() - start < 5, 'Timed out: ' + label
            QtTest.QTest.qWait(25)
        result['transition_seconds'].append(round(time.monotonic() - start, 4))

    def fields():
        values = {str(value.property('recoveryField')): value for value in root.findChildren(QtWidgets.QWidget)
                  if value.property('recoveryField')}
        assert len(values) == 18
        keep.extend(values.values())
        return values

    def snapshot():
        return {key: value.toPlainText() if isinstance(value, QtWidgets.QPlainTextEdit)
                else value.currentText() if isinstance(value, QtWidgets.QComboBox) else value.text()
                for key, value in fields().items()}

    def apply(values):
        for key, text in values.items():
            field = fields()[key]
            if isinstance(field, QtWidgets.QPlainTextEdit):
                field.setPlainText(text)
            elif isinstance(field, QtWidgets.QComboBox):
                field.setCurrentText(text)
            else:
                field.setText(text)

    def copies():
        return list((Path(os.environ['LOCALAPPDATA']) / 'FOA-SDK/Recovery/PackDrafts').glob('*.packdraft.json'))

    def checkpoint(values):
        def ready():
            paths = copies()
            if len(paths) != 1:
                return False
            try:
                return json.loads(paths[0].read_text(encoding='utf-8'))['Fields'] == values
            except (OSError, ValueError, KeyError):
                return False
        wait_for(ready, 'full draft checkpoint')
        path = copies()[0]
        result['recovery_path'] = str(path)
        result['recovery_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        result['fields'] = values
        result['record'] = json.loads(path.read_text(encoding='utf-8'))
        return path

    def close_pane(choice, accepted=False):
        seen, errors = [], []
        timer = QtCore.QTimer()
        keep.append(timer)
        dock = pane_test.pane_dock(root)
        before = snapshot()
        identity = [e.text() for e in root.findChildren(QtWidgets.QLineEdit) if e.isReadOnly()]
        if docked:
            assert pane_test.floating_container(root) is None
        def answer():
            prompt = next((value for value in widgets() if isinstance(value, QtWidgets.QMessageBox)
                           and value.isVisible()), None)
            if prompt:
                timer.stop()
                try:
                    assert prompt.objectName() == 'packUnsavedChangesDialog'
                    seen.append(prompt.objectName())
                    if choice == 'Escape':
                        QtTest.QTest.keyClick(prompt, QtCore.Qt.Key_Escape)
                    elif choice == 'Close':
                        prompt.close()
                    else:
                        prompt.button(getattr(QtWidgets.QMessageBox, choice)).click()
                except Exception:
                    errors.append(traceback.format_exc())
                    prompt.reject()
        timer.timeout.connect(answer)
        timer.start(25)
        try:
            if docked:
                pane_test.request_titlebar_close(root, result, keep)
            else:
                root.close()
            QtTest.QTest.qWait(100)
        finally:
            timer.stop()
        assert not errors and seen == ['packUnsavedChangesDialog'], (errors, seen)
        if accepted:
            assert not isValid(root) and not isValid(dock), 'Accepted close must destroy the registered pane'
        else:
            assert isValid(root) and root.isVisible() and isValid(dock) and dock.isVisible()
            assert snapshot() == before
            assert [e.text() for e in root.findChildren(QtWidgets.QLineEdit) if e.isReadOnly()] == identity
            assert control('packDraftStatus').text() == 'Unsaved changes'
            if docked:
                assert pane_test.floating_container(root) is None

    def reopen_docked():
        nonlocal root
        pane_test.open_default_pack()
        root = control('TaintedGrailPackManager')
        wait_for(lambda: control('packRecoveryStatus').text() != 'Checking draft recovery...', 'reopened recovery read')
        assert root.isVisible() and pane_test.floating_container(root) is None
        assert not control('packRecoveryPrompt').isVisible() and not copies()
        assert control('packDraftStatus').text() != 'Unsaved changes'

    def normal_exit(choice=None):
        timer = QtCore.QTimer()
        seen = []
        def answer():
            prompt = next((value for value in widgets() if isinstance(value, QtWidgets.QMessageBox)
                           and value.isVisible()), None)
            if prompt and prompt.objectName() == 'packUnsavedChangesDialog':
                seen.append(choice)
                prompt.button(getattr(QtWidgets.QMessageBox, choice)).click()
        def quit_seen():
            timer.stop()
            try:
                if choice:
                    assert seen == [choice]
                    assert not Path(expected['recovery_path']).exists()
                    assert_original()
                    result['checks'].append('explicit_editor_discard_retires_recovery_and_preserves_saved_bytes')
                result['about_to_quit'] = True
                stage('about_to_quit')
            except Exception:
                result['status'] = 'FAILED'
                result['error'] = traceback.format_exc()
                stage('failed')
        if choice:
            timer.timeout.connect(answer)
            timer.start(25)
        app.aboutToQuit.connect(quit_seen)
        keep.extend((quit_seen, answer, timer))
        result['status'] = 'PASSED'
        stage('exit_scheduled')
        QtCore.QTimer.singleShot(0, general.exit)

    def save_valid(name='Recovered mod', via_close=False):
        apply(result.get('baseline') or expected['record']['Baseline'])
        apply({'displayName': name, 'owner': 'sdkqa', 'version': '1.0.0'})
        if via_close:
            close_pane('Save', accepted=True)
        else:
            button('Save mod').click()
            assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        assert not copies(), 'Successful save must retire recovery'
        saved = list((workspace.parent / 'Packs').glob('*/pack.tgpack.json'))
        assert len(saved) == 1
        document = json.loads(saved[0].read_text(encoding='utf-8'))
        manifest = document.get('ClassData', document)
        assert manifest['DisplayName'] == name and manifest['Version'] == '1.0.0'
        result['manifest'] = str(saved[0])
        result['manifest_sha256'] = hashlib.sha256(saved[0].read_bytes()).hexdigest()
        if via_close:
            reopen_docked()
            assert control('packDisplayName').text() == name
            assert control('packDraftStatus').text() == 'All changes saved'
            result['checks'].append('docked_recovered_save_destroys_pane_retires_copy_and_reopens_saved_mod')

    def assert_original():
        if expected.get('manifest'):
            assert hashlib.sha256(Path(expected['manifest']).read_bytes()).hexdigest() == expected['manifest_sha256']
        else:
            assert not (workspace.parent / 'Packs').exists(), 'Recovery wrote a manifest'

    try:
        stage('startup')
        assert not any('autotest_mode' in arg for arg in app.arguments())
        assert workspace.resolve() == (Path(os.environ['LOCALAPPDATA']) /
                                       'FOA-SDK/Workspace/foa-sdk.tgworkspace.json').resolve()
        fixture = json.loads(workspace.read_text(encoding='utf-8'))
        assert fixture['WorkspaceId'] == 'sdkqa.pack-editor-exit'
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
        kernel.GetModuleHandleW.restype = ctypes.c_void_p
        kernel.GetModuleFileNameW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint32]
        buffer = ctypes.create_unicode_buffer(32768)
        module = kernel.GetModuleHandleW('TaintedGrailModdingSDK.Editor.dll')
        assert module and kernel.GetModuleFileNameW(module, buffer, len(buffer))
        result['sdk_module_path'] = buffer.value
        result['sdk_module_sha256'] = hashlib.sha256(Path(buffer.value).read_bytes()).hexdigest()
        general.idle_enable(True)
        QtTest.QTest.qWait(1500)
        general.open_pane('Tainted Grail SDK Status')
        QtTest.QTest.qWait(350)
        toggle = control('foundationAdvancedToggle')
        if toggle.text() == 'Show advanced details':
            toggle.click()
        details = next(value for value in widgets() if isinstance(value, QtWidgets.QPlainTextEdit)
                       and value.toPlainText().startswith('Workspace file:'))
        assert workspace.as_posix() in details.toPlainText().replace('\\', '/')
        if docked:
            initial = [w for w in widgets() if w.objectName() == 'TaintedGrailPackManager']
            for form in initial:
                assert form.findChild(QtWidgets.QLabel, 'packDraftStatus').text() != 'Unsaved changes'
                assert pane_test.pane_dock(form).close()
            QtTest.QTest.qWait(100)
            pane_test.open_default_pack()
        else:
            general.open_pane('Tainted Grail Pack Manager')
        QtTest.QTest.qWait(350)
        root = control('TaintedGrailPackManager')
        wait_for(lambda: control('packRecoveryStatus').text() != 'Checking draft recovery...', 'recovery read')

        if docked:
            assert pane_test.floating_container(root) is None and root.isVisible()
            result['docked_setup'] = 'QtViewPaneManager UseDefaultState, Windows x64 Qt 6.10.2'
            assert pane_test.pane_dock(root).grab().save(str(output.with_suffix('.docked.png')))

        if case.startswith('seed-') or case == 'failure':
            assert not control('packRecoveryPrompt').isVisible()
            assert button('New mod').isEnabled()
            button('New mod').click()
            if case == 'seed-saved' or case == 'failure':
                apply({'displayName': 'Original', 'owner': 'sdkqa'})
                button('Save mod').click()
                assert control('packStatus').text() == 'Mod saved. You can start authoring.'
                saved = workspace.parent / 'Packs/sdkqa.original/pack.tgpack.json'
                result['manifest'] = str(saved)
                result['manifest_sha256'] = hashlib.sha256(saved.read_bytes()).hexdigest()
            result['baseline'] = snapshot()
            raw = {key: '  unfinished \u03a9\n\n' + key if isinstance(field, QtWidgets.QPlainTextEdit)
                   else '  unfinished \u03a9 ' + key for key, field in fields().items()}
            raw.update({'owner': '', 'version': 'unfinished', 'saveImpact': 'migration', 'releaseChannel': 'beta'})
            button('Show advanced manifest').click()
            apply(raw)
            path = checkpoint(snapshot())
            assert control('packDraftStatus').text() == 'Unsaved changes'
            result['checks'].append('all_raw_fields_checkpointed_without_manifest_write')
            if case != 'failure':
                result['status'] = 'READY_TO_TERMINATE'
                stage('checkpoint_durable')
                return
            old_bytes = path.read_bytes()
            kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                          ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
            kernel.CreateFileW.restype = ctypes.c_void_p
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            handle = kernel.CreateFileW(str(path), 0x80000000, 1, None, 3, 0x80, None)
            assert handle not in (None, ctypes.c_void_p(-1).value)
            try:
                apply({'displayName': 'Latest unsaved name'})
                full = snapshot()
                wait_for(lambda: 'could not be updated' in control('packRecoveryStatus').text(), 'quiet failed checkpoint')
                assert path.read_bytes() == old_bytes and snapshot() == full
                close_pane('Cancel')
                assert isValid(root) and root.isVisible() and snapshot() == full
                assert path.read_bytes() == old_bytes
            finally:
                kernel.CloseHandle(handle)
            apply({'version': 'still invalid'})
            checkpoint(snapshot())
            button('Save mod').click()
            assert control('packDraftStatus').text() == 'Unsaved changes' and path.exists()
            result['checks'].append('failed_checkpoint_and_save_keep_full_draft_and_previous_good_copy')
            save_valid()
            result['checks'].append('checkpoint_retry_and_successful_save_retire_copy')
            normal_exit()
            return

        if case.startswith('verify-'):
            assert not control('packRecoveryPrompt').isVisible() and not copies()
            combo = control('packSavedMods')
            saved = Path(expected['manifest'])
            combo.setCurrentIndex(next(i for i in range(combo.count()) if Path(combo.itemData(i)).resolve() == saved.resolve()))
            button('Open selected').click()
            assert hashlib.sha256(saved.read_bytes()).hexdigest() == expected['manifest_sha256']
            document = json.loads(saved.read_text(encoding='utf-8'))
            assert control('packDisplayName').text() == document.get('ClassData', document)['DisplayName']
            assert control('packDraftStatus').text() == 'All changes saved'
            result['checks'].append('fresh_process_has_no_recovery_and_opens_unchanged_saved_mod')
            normal_exit()
            return

        assert control('packRecoveryPrompt').isVisible()
        assert not button('Save mod').isEnabled() and not button('New mod').isEnabled()
        path = Path(expected['recovery_path'])
        assert_original()
        if case == 'reject-corrupt':
            assert not control('packRestoreDraft').isEnabled()
            assert path.read_bytes() == b'{broken'
            assert 'damaged' in control('packRecoveryStatus').text()
            control('packDiscardRecovery').click()
            assert not path.exists() and button('Save mod').isEnabled()
            assert_original()
            result['checks'].append('corrupt_copy_preserved_until_explicit_discard_without_manifest_write')
            normal_exit()
            return

        if case == 'isolate':
            other_root = workspace.parent.parent / 'WorkspaceB'
            other = json.loads(json.dumps(fixture).replace(workspace.parent.as_posix(), other_root.as_posix()))
            other['WorkspaceId'] += '-b'
            for leaf in ('Output', 'Staging', 'Deployment', 'Diagnostics', 'Extracted', 'Game/Managed', 'Game/BepInEx/plugins'):
                (other_root / leaf).mkdir(parents=True, exist_ok=True)
            for leaf in ('Game/Managed/Assembly-CSharp.dll', 'Game/UnityPlayer.dll'):
                (other_root / leaf).write_text('Synthetic authoring marker only.', encoding='utf-8')
            second = other_root / 'second.tgworkspace.json'
            second.write_text(json.dumps(other), encoding='utf-8')
            app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
            open_button = next(value for value in widgets() if isinstance(value, QtWidgets.QPushButton)
                               and value.text() == 'Open existing workspace...')
            def switch(destination):
                timer = QtCore.QTimer()
                keep.append(timer)
                seen = []
                def choose():
                    picker = next((value for value in widgets() if isinstance(value, QtWidgets.QFileDialog)
                                   and value.isVisible()), None)
                    if picker:
                        if not seen:
                            seen.append(True)
                            picker.setDirectory(str(destination.parent))
                            picker.selectFile(destination.name)
                            picker.findChild(QtWidgets.QLineEdit, 'fileNameEdit').setText(destination.name)
                        else:
                            picker.findChild(QtWidgets.QDialogButtonBox).button(QtWidgets.QDialogButtonBox.Open).click()
                timer.timeout.connect(choose)
                timer.start(25)
                try:
                    open_button.click()
                finally:
                    timer.stop()
                assert seen
                wait_for(lambda: control('packRecoveryStatus').text() != 'Checking draft recovery...', 'workspace recovery binding')
                assert destination.as_posix() in details.toPlainText().replace('\\', '/')
            switch(second)
            assert not control('packRecoveryPrompt').isVisible()
            assert control('packDraftStatus').text() == 'New mod draft'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == expected['recovery_sha256']
            switch(workspace)
            assert control('packRecoveryPrompt').isVisible()
            assert hashlib.sha256(path.read_bytes()).hexdigest() == expected['recovery_sha256']
            result['checks'].append('pending_recovery_isolated_across_workspace_switch_and_return')
            normal_exit()
            return

        assert root.grab().save(str(output.with_suffix('.offer.png')))
        control('packRestoreDraft').click()
        assert snapshot() == expected['fields']
        assert control('packDraftStatus').text() == 'Unsaved changes'
        assert button('Hide advanced manifest').isVisible()
        assert path.exists(), 'Restore must retain the durable copy until Save/Discard'
        assert_original()
        assert root.grab().save(str(output.with_suffix('.restored.png')))
        result['checks'].append('fresh_process_restores_all_raw_fields_and_advanced_state_without_manifest_write')
        if docked:
            recovery_bytes = path.read_bytes()
            for choice in ('Cancel', 'Escape', 'Close', 'Save'):
                close_pane(choice)
                assert snapshot() == expected['fields'] and path.read_bytes() == recovery_bytes
                assert_original()
            result['checks'].append('docked_recovered_cancel_escape_prompt_close_and_invalid_save_keep_full_draft_and_copy')
        if case == 'restore-again':
            button('Save mod').click()
            assert snapshot() == expected['fields'] and path.exists()
            close_pane('Cancel')
            assert snapshot() == expected['fields'] and path.exists()
            assert_original()
            checkpoint(snapshot())
            result['checks'].append('invalid_save_and_cancel_keep_recovered_draft')
            result['status'] = 'READY_TO_TERMINATE'
            stage('checkpoint_durable')
            return
        if case == 'restore-save':
            save_valid(via_close=docked)
            result['checks'].append('successful_save_is_explicit_and_retires_recovery')
        elif case == 'restore-discard':
            # Exercise the host's real shutdown path; QWidget.close() only hides
            # the inner form and leaves its registered dock alive.
            result['manifest'] = expected['manifest']
            result['manifest_sha256'] = expected['manifest_sha256']
            if docked:
                apply(expected['record']['Baseline'])
                apply({'displayName': 'Recovered locked save', 'version': '1.0.0'})
                checkpoint(snapshot())
                recovery_bytes = path.read_bytes()
                kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                              ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
                kernel.CreateFileW.restype = ctypes.c_void_p
                kernel.CloseHandle.argtypes = [ctypes.c_void_p]
                handle = kernel.CreateFileW(expected['manifest'], 0x80000000, 1, None, 3, 0x80, None)
                assert handle not in (None, ctypes.c_void_p(-1).value)
                try:
                    close_pane('Save')
                    assert path.read_bytes() == recovery_bytes
                    assert_original()
                finally:
                    kernel.CloseHandle(handle)
                result['checks'].append('docked_recovered_real_manifest_write_failure_keeps_pane_fields_and_recovery')
                close_pane('Discard', accepted=True)
                assert not path.exists()
                assert_original()
                reopen_docked()
                combo = control('packSavedMods')
                saved = Path(expected['manifest'])
                combo.setCurrentIndex(next(i for i in range(combo.count()) if Path(combo.itemData(i)).resolve() == saved.resolve()))
                button('Open selected').click()
                assert control('packDisplayName').text() == expected['record']['Baseline']['displayName']
                assert_original()
                result['checks'].append('docked_recovered_discard_destroys_pane_retires_copy_and_reopens_unchanged_saved_mod')
                normal_exit()
            else:
                normal_exit('Discard')
            return
        else:
            raise AssertionError('Unknown recovery case ' + case)
        normal_exit()
    except Exception:
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
        stage('failed')


def initialized(_args):
    handler.disconnect()
    QtCore.QTimer.singleShot(0, run)


handler = editor.EditorEventBusHandler()
handler.connect()
handler.add_callback('NotifyEditorInitialized', initialized)
