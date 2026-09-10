# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Test Pack Manager draft replacement in the compiled Editor with synthetic data.

Launch with --runpython and without --autotest_mode: unattended mode closes
modal dialogs before this test can exercise the user's Save/Discard/Cancel choice.
"""
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
    result = {'status': 'FAILED', 'checks': [], 'transition_seconds': []}
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
                    prompt.grab().save(str(output.with_name('unsaved-prompt.png')))
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
        button(action).click()
        timer.stop()
        elapsed = time.monotonic() - started
        result['transition_seconds'].append(round(elapsed, 4))
        assert not errors, errors
        assert bool(seen) == (choice is not None), (action, choice, seen)
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
        general.open_pane('Tainted Grail Pack Manager')
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
        result['status'] = 'PASSED'
        stage('complete')
    except Exception:
        result['error'] = traceback.format_exc()
        stage(result.get('stage', 'failed'))
    finally:
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')


run()
