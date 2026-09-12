# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Test Pack Manager draft replacement in the compiled Editor with synthetic data.

Launch with --runpython and without --autotest_mode: unattended mode closes
modal dialogs before this test can exercise the user's Save/Discard/Cancel choice.
"""
import ctypes
import hashlib
import sys
import json
import os
from pathlib import Path
import time
import traceback

import azlmbr.legacy.general as general
from PySide6 import QtCore, QtGui, QtTest, QtWidgets
from shiboken6 import isValid

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pack_pane_test_support as pane_test


def run():
    output = Path(os.environ['FOA_SDK_PACK_RESULT'])
    workspace = Path(os.environ['FOA_SDK_PACK_WORKSPACE'])
    result = {'status': 'PARTIAL', 'checks': [], 'transition_seconds': [], 'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    keep = []

    def stage(name):
        result['stage'] = name
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')

    def control(name):
        values = app.allWidgets()
        keep.extend(values)
        return next(value for value in reversed(values)
                    if isValid(value) and value.objectName() == name)

    def button(text):
        return next(value for value in root.findChildren(QtWidgets.QPushButton)
                    if value.text() == text)

    def dirty(expected=True):
        label = control('packDraftStatus').text()
        assert (label == 'Unsaved changes') == expected, label

    def manifest(path):
        data = json.loads(path.read_text(encoding='utf-8'))
        return data.get('ClassData', data)

    def select(path):
        combo = control('packSavedMods')
        index = next(i for i in range(combo.count())
                     if Path(combo.itemData(i)).resolve() == path.resolve())
        combo.setCurrentIndex(index)

    def snapshot():
        return ([edit.text() for edit in root.findChildren(QtWidgets.QLineEdit)],
                [edit.toPlainText() for edit in root.findChildren(QtWidgets.QPlainTextEdit)],
                [combo.currentText() for combo in root.findChildren(QtWidgets.QComboBox)
                 if combo.objectName() != 'packSavedMods'])

    def transition(action, choice=None, capture=False):
        """Answer the actual modal prompt; record callback failures outside Qt's event loop."""
        seen = []
        errors = []
        started = time.monotonic()
        timer = QtCore.QTimer()

        def answer():
            # O3DE decorates modal windows; the active modal may be the wrapper.
            values = app.allWidgets()
            keep.extend(values)
            prompt = next((value for value in values if isValid(value)
                           and isinstance(value, QtWidgets.QMessageBox)
                           and value.objectName() == 'packUnsavedChangesDialog'
                           and value.isVisible()), None)
            if not isinstance(prompt, QtWidgets.QMessageBox):
                if time.monotonic() - started > 5:
                    errors.append('Expected prompt was not found within five seconds')
                    if prompt:
                        prompt.reject()
                    timer.stop()
                return
            timer.stop()
            try:
                assert prompt.objectName() == 'packUnsavedChangesDialog'
                seen.append(prompt.objectName())
                buttons = QtWidgets.QMessageBox
                assert prompt.standardButtons() == buttons.Save | buttons.Discard | buttons.Cancel
                assert prompt.defaultButton() == prompt.button(buttons.Cancel)
                assert prompt.escapeButton() == prompt.button(buttons.Cancel)
                if capture:
                    image_name = 'close-prompt.png' if callable(action) else 'unsaved-prompt.png'
                    assert prompt.grab().save(str(output.with_name(image_name)))
                if choice == 'Escape':
                    QtTest.QTest.keyClick(prompt, QtCore.Qt.Key_Escape)
                elif choice == 'Close':
                    prompt.close()
                else:
                    assert choice is not None, 'Clean form unexpectedly prompted'
                    prompt.button(getattr(buttons, choice)).click()
            except Exception:
                errors.append(traceback.format_exc())
                prompt.reject()

        timer.timeout.connect(answer)
        timer.start(25)
        if callable(action):
            action()
        else:
            button(action).click()
        timer.stop()
        elapsed = time.monotonic() - started
        result['transition_seconds'].append(round(elapsed, 4))
        assert not errors, errors
        assert len(seen) == (1 if choice is not None else 0), (action, choice, seen)
        assert elapsed < 3.0, 'Draft replacement exceeded the three-second fixture budget'

    def save():
        button('Save mod').click()
        assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        dirty(False)

    def edit(name, value):
        control(name).setText(value)

    def prepare(action, name):
        select(original)
        transition('Open selected')
        edit('packDisplayName', name)
        dirty()
        if action == 'Open selected':
            select(other)
        return snapshot(), original.read_bytes(), control('packActiveSummary').text()

    def retained(before):
        form, saved, active = before
        assert snapshot() == form
        assert original.read_bytes() == saved
        assert control('packActiveSummary').text() == active
        dirty()

    def replaced(action, saved_name=None):
        dirty(False)
        if action == 'New mod':
            assert control('packDisplayName').text() == ''
            assert control('packDraftStatus').text() == 'New mod draft'
            if saved_name:
                assert saved_name in control('packActiveSummary').text()
        else:
            assert control('packDisplayName').text() == 'Other'
            assert 'Other' in control('packActiveSummary').text()

    try:
        stage('validating_fixture')
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
        kernel.GetModuleHandleW.restype = ctypes.c_void_p
        kernel.GetModuleFileNameW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint32]
        buffer = ctypes.create_unicode_buffer(32768)
        module = kernel.GetModuleHandleW('TaintedGrailModdingSDK.Editor.dll')
        assert module and kernel.GetModuleFileNameW(module, buffer, len(buffer))
        result['sdk_module_path'] = buffer.value
        result['sdk_module_sha256'] = hashlib.sha256(Path(buffer.value).read_bytes()).hexdigest()
        assert not any('autotest_mode' in argument for argument in app.arguments()), (
            'Run without --autotest_mode; O3DE unattended mode dismisses modal dialogs')
        automatic = Path(os.environ['LOCALAPPDATA']) / 'FOA-SDK/Workspace/foa-sdk.tgworkspace.json'
        assert workspace.resolve() == automatic.resolve()
        fixture = json.loads(workspace.read_text(encoding='utf-8'))
        assert fixture['WorkspaceId'] == 'sdkqa.pack-unsaved'
        assert Path(fixture['RootPath']).resolve() == workspace.parent.resolve()
        assert not (workspace.parent / 'Packs').exists(), 'Use a fresh synthetic fixture'
        general.idle_enable(True)
        QtTest.QTest.qWait(3000)
        general.open_pane('Tainted Grail SDK Status')
        QtTest.QTest.qWait(500)
        values = app.allWidgets()
        keep.extend(values)
        next(w for w in values if isValid(w) and isinstance(w, QtWidgets.QPushButton)
             and w.text() == 'Show advanced details').click()
        details = next(w for w in app.allWidgets()
                       if isinstance(w, QtWidgets.QPlainTextEdit)
                       and w.toPlainText().startswith('Workspace file:'))
        assert workspace.as_posix() in details.toPlainText().replace('\\', '/')
        # Setup may close only a clean/unresolved initial form, before test edits.
        initial = [w for w in app.allWidgets() if isValid(w) and w.objectName() == 'TaintedGrailPackManager']
        for form in initial:
            assert form.findChild(QtWidgets.QLabel, 'packDraftStatus').text() != 'Unsaved changes'
            assert pane_test.pane_dock(form).close()
        QtTest.QTest.qWait(100)
        pane_test.open_default_pack()
        QtTest.QTest.qWait(500)
        root = control('TaintedGrailPackManager')

        stage('all_fields_and_revert')
        dirty(False)
        transition('New mod')
        button('Show advanced manifest').click()
        count = 0
        for field in root.findChildren(QtWidgets.QLineEdit):
            if field.isReadOnly():
                continue
            old = field.text()
            field.setText(old + 'x')
            dirty()
            field.setText(old)
            dirty(False)
            count += 1
        for field in root.findChildren(QtWidgets.QPlainTextEdit):
            old = field.toPlainText()
            field.setPlainText(old + '\nexample')
            dirty()
            field.setPlainText(old)
            dirty(False)
            count += 1
        for field in root.findChildren(QtWidgets.QComboBox):
            if field.objectName() == 'packSavedMods':
                continue
            old = field.currentIndex()
            field.setCurrentIndex((old + 1) % field.count())
            dirty()
            field.setCurrentIndex(old)
            dirty(False)
            count += 1
        assert count == 18, count
        result['checks'].append('all_18_editable_fields_track_and_revert_to_clean')
        button('Hide advanced manifest').click()
        edit('packDisplayName', 'Original')
        edit('packOwner', 'sdkqa')
        save()
        original = workspace.parent / 'Packs/sdkqa.original/pack.tgpack.json'
        transition('New mod')
        edit('packDisplayName', 'Other')
        edit('packOwner', 'sdkqa')
        save()
        other = workspace.parent / 'Packs/sdkqa.other/pack.tgpack.json'
        other_bytes = other.read_bytes()
        result['checks'].append('pristine_new_save_and_load_reset_baseline_without_prompt')

        for action in ('New mod', 'Open selected'):
            stage(action + '_cancel_discard_save')
            before = prepare(action, 'Cancelled draft')
            for choice in ('Cancel', 'Escape', 'Close'):
                transition(action, choice, capture=(action == 'New mod' and choice == 'Cancel'))
                retained(before)
            result['checks'].append(action + '_cancel_escape_close_preserve_full_draft')
            transition(action, 'Discard')
            replaced(action)
            assert original.read_bytes() == before[1]
            if action == 'New mod':
                assert control('packActiveSummary').text() == before[2]
            result['checks'].append(action + '_discard_preserves_saved_bytes')

            before = prepare(action, 'Saved before ' + action)
            transition(action, 'Save')
            replaced(action, 'Saved before ' + action)
            assert manifest(original)['DisplayName'] == 'Saved before ' + action
            assert other.read_bytes() == other_bytes
            result['checks'].append(action + '_save_then_replace_preserves_requested_target')

            stage(action + '_validation_failure')
            prepare(action, 'Invalid draft')
            edit('packVersion', 'invalid')
            before = snapshot(), original.read_bytes(), control('packActiveSummary').text()
            transition(action, 'Save')
            retained(before)
            assert control('packStatus').text() != 'Mod saved. You can start authoring.'
            edit('packVersion', '1.2.3')
            transition(action, 'Save')
            replaced(action, 'Invalid draft')
            result['checks'].append(action + '_failed_validation_stays_dirty_and_retry_completes')

            stage(action + '_locked_file')
            before = prepare(action, 'Recovered ' + action)
            kernel = ctypes.WinDLL('kernel32', use_last_error=True)
            kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                          ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32,
                                          ctypes.c_void_p]
            kernel.CreateFileW.restype = ctypes.c_void_p
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            handle = kernel.CreateFileW(str(original), 0x80000000, 1, None, 3, 0x80, None)
            assert handle not in (None, ctypes.c_void_p(-1).value), ctypes.get_last_error()
            try:
                transition(action, 'Save')
                retained(before)
            finally:
                kernel.CloseHandle(handle)
            transition(action, 'Save')
            replaced(action, 'Recovered ' + action)
            result['checks'].append(action + '_real_write_failure_retains_draft_and_retry_completes')

        stage('failed_open_retains_discarded_draft_until_load_succeeds')
        before = prepare('Open selected', 'Retained on bad load')
        try:
            other.write_text('{invalid synthetic manifest', encoding='utf-8')
            transition('Open selected', 'Discard')
            retained(before)
        finally:
            other.write_bytes(other_bytes)
        transition('Open selected', 'Discard')
        replaced('Open selected')
        result['checks'].append('failed_open_retains_full_draft_and_dirty_state')

        stage('new_draft_save_before_open')
        transition('New mod')
        edit('packDisplayName', 'Third')
        select(other)
        transition('Open selected', 'Save')
        dirty()
        assert control('packDisplayName').text() == 'Third'
        assert control('packOwner').text() == ''
        assert control('packStatus').text() == 'Enter an author or namespace.'
        edit('packOwner', 'sdkqa')
        transition('Open selected', 'Save')
        replaced('Open selected')
        third = workspace.parent / 'Packs/sdkqa.third/pack.tgpack.json'
        assert manifest(third)['DisplayName'] == 'Third'
        result['checks'].append('new_draft_missing_owner_retains_identity_and_save_opens_original_target')

        edit('packDisplayName', 'Visible unsaved draft')
        QtTest.QTest.qWait(100)
        root.grab().save(str(output.with_suffix('.png')))
        transition('New mod', 'Cancel')
        dirty()

        def pane_dock():
            return pane_test.pane_dock(root)

        def floating_container():
            return pane_test.floating_container(root)

        def place_pane(floating):
            if floating and floating_container() is None:
                pane_test.pane_menu_action(root, 'Undock', result, keep)
                QtTest.QTest.qWait(150)
            assert (floating_container() is not None) == floating, 'Unexpected pane docking state'
            assert root.isVisible()

        def request_titlebar_close():
            pane_test.request_titlebar_close(root, result, keep)

        def close_pane(choice=None, accepted=False, capture=False):
            dock = pane_dock()
            form = snapshot()
            active = control('packActiveSummary').text()
            transition(request_titlebar_close, choice, capture)
            QtTest.QTest.qWait(100)
            if accepted:
                assert not isValid(root) and not isValid(dock), 'Accepted pane close did not destroy the pane'
            else:
                assert isValid(root) and root.isVisible()
                assert isValid(dock) and dock.isVisible() and pane_dock() == dock
                assert snapshot() == form, 'Cancelled or failed close changed the draft'
                assert control('packActiveSummary').text() == active
                dirty()

        def reopen():
            nonlocal root
            if floating:
                general.open_pane('Tainted Grail Pack Manager')
            else:
                pane_test.open_default_pack()
            QtTest.QTest.qWait(150)
            reopened = control('TaintedGrailPackManager')
            assert reopened.isVisible()
            root = reopened
            place_pane(floating)
            return reopened

        # Exercise the actual titlebar menu/button and deletable dock, not only a synthetic
        # QCloseEvent or general.close_pane (the latter forces a host close).
        select(other)
        transition('Open selected', 'Discard')
        for floating, mode in ((False, 'docked'), (True, 'floating')):
            stage(mode + '_pane_close')
            place_pane(floating)
            saved = other.read_bytes()
            edit('packDisplayName', mode + ' close draft')
            edit('packVersion', '2.3.4')
            for choice in ('Cancel', 'Escape', 'Close'):
                close_pane(choice, capture=(mode == 'docked' and choice == 'Cancel'))
                assert other.read_bytes() == saved
            result['checks'].append(mode + '_titlebar_cancel_escape_close_keep_pane_and_full_draft')

            edit('packVersion', 'invalid')
            close_pane('Save')
            assert other.read_bytes() == saved
            assert control('packStatus').text() != 'Mod saved. You can start authoring.'
            result['checks'].append(mode + '_close_invalid_save_keeps_pane_and_draft')

            edit('packVersion', '2.3.4')
            handle = kernel.CreateFileW(str(other), 0x80000000, 1, None, 3, 0x80, None)
            assert handle not in (None, ctypes.c_void_p(-1).value), ctypes.get_last_error()
            try:
                close_pane('Save')
                assert other.read_bytes() == saved
                assert control('packStatus').text() != 'Mod saved. You can start authoring.'
            finally:
                kernel.CloseHandle(handle)
            result['checks'].append(mode + '_close_real_write_failure_keeps_pane_and_draft')
            close_pane('Save', accepted=True)
            assert manifest(other)['DisplayName'] == mode + ' close draft'
            assert manifest(other)['Version'] == '2.3.4'
            root = reopen()
            dirty(False)
            assert control('packDisplayName').text() == mode + ' close draft'
            assert control('packVersion').text() == '2.3.4'
            result['checks'].append(mode + '_close_retry_saves_destroys_and_reopens_equivalent')

            saved = other.read_bytes()
            edit('packDisplayName', 'Discarded close draft')
            close_pane('Discard', accepted=True)
            assert other.read_bytes() == saved
            root = reopen()
            dirty(False)
            assert control('packDisplayName').text() == mode + ' close draft'
            result['checks'].append(mode + '_close_discard_destroys_without_writing_and_reopens_saved_mod')

            close_pane(accepted=True)
            root = reopen()
            dirty(False)
            result['checks'].append(mode + '_clean_saved_pane_closes_without_prompt')

        stage('new_draft_close')
        active = control('packActiveSummary').text()
        transition('New mod')
        close_pane(accepted=True)
        root = reopen()
        assert control('packActiveSummary').text() == active
        dirty(False)
        result['checks'].append('pristine_new_pane_closes_without_prompt')

        transition('New mod')
        edit('packDisplayName', 'Close created')
        close_pane('Save')
        assert control('packOwner').text() == ''
        assert control('packStatus').text() == 'Enter an author or namespace.'
        edit('packOwner', 'sdkqa')
        close_pane('Save', accepted=True)
        created = workspace.parent / 'Packs/sdkqa.close-created/pack.tgpack.json'
        assert manifest(created)['DisplayName'] == 'Close created'
        root = reopen()
        assert control('packDisplayName').text() == 'Close created'
        dirty(False)
        result['checks'].append('new_draft_close_failed_save_preserves_identity_and_retry_reopens_saved_mod')

        saved = created.read_bytes()
        transition('New mod')
        edit('packDisplayName', 'Abandoned close')
        edit('packOwner', 'sdkqa')
        close_pane('Discard', accepted=True)
        assert not (workspace.parent / 'Packs/sdkqa.abandoned-close').exists()
        assert created.read_bytes() == saved
        root = reopen()
        assert control('packDisplayName').text() == 'Close created'
        dirty(False)
        result['checks'].append('new_draft_close_discard_preserves_previous_active_mod_without_writing')
        edit('packDisplayName', 'Draft preserved after cancelled close')
        close_pane('Cancel')
        root.grab().save(str(output.with_name('close-pane.png')))

        result['status'] = 'PASSED'
        stage('complete')
    except Exception:
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
        stage(result.get('stage', 'failed'))
    finally:
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')


import azlmbr.editor as recovery_editor

_adapter_keep = []
def _exercise_ready():
    run()
    output = Path(os.environ['FOA_SDK_PACK_RESULT'])
    result = json.loads(output.read_text(encoding='utf-8'))
    result['editor_initialized'] = True
    if result['status'] != 'PASSED':
        result['status'] = 'FAILED'
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')
        return
    app = QtWidgets.QApplication.instance()
    timer = QtCore.QTimer()
    def answer():
        values = app.allWidgets()
        _adapter_keep.extend(values)
        for value in values:
            if isValid(value) and isinstance(value, QtWidgets.QMessageBox) and value.isVisible():
                assert value.objectName() == 'packUnsavedChangesDialog'
                value.button(QtWidgets.QMessageBox.Discard).click()
    def quit_seen():
        timer.stop()
        result['about_to_quit'] = True
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    timer.timeout.connect(answer)
    timer.start(25)
    _adapter_keep.extend((timer, answer, quit_seen))
    app.aboutToQuit.connect(quit_seen)
    output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    QtCore.QTimer.singleShot(0, general.exit)

def _initialized(_args):
    _handler.disconnect()
    QtCore.QTimer.singleShot(0, _exercise_ready)

_handler = recovery_editor.EditorEventBusHandler()
_handler.connect()
_handler.add_callback('NotifyEditorInitialized', _initialized)
