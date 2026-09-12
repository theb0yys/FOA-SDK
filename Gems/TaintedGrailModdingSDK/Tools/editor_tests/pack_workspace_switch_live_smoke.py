# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Exercise workspace switching through real compiled Editor dialogs and synthetic roots."""
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
    route = os.environ['FOA_SDK_PACK_WORKSPACE_ROUTE']
    result = {'status': 'PARTIAL', 'route': route, 'checks': [],
              'transition_seconds': [], 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    keep = []
    settings = QtCore.QSettings('FOA-SDK', 'TaintedGrailModdingSDK')
    setting_key = 'CatalogBrowser/LastWorkspacePath'
    had_setting = settings.contains(setting_key)
    old_setting = settings.value(setting_key)

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

    def dirty(expected=True):
        assert (control('packDraftStatus').text() == 'Unsaved changes') == expected

    def snapshot():
        return ([value.text() for value in root.findChildren(QtWidgets.QLineEdit)],
                [value.toPlainText() for value in root.findChildren(QtWidgets.QPlainTextEdit)],
                [value.currentText() for value in root.findChildren(QtWidgets.QComboBox)
                 if value.objectName() != 'packSavedMods'])

    def field(label):
        for layout in root.findChildren(QtWidgets.QFormLayout):
            for row in range(layout.rowCount()):
                item = layout.itemAt(row, QtWidgets.QFormLayout.LabelRole)
                if item and isinstance(item.widget(), QtWidgets.QLabel) and item.widget().text() == label:
                    return layout.itemAt(row, QtWidgets.QFormLayout.FieldRole).widget()
        raise AssertionError('Missing form field: ' + label)

    def active_workspace(path):
        text = details.toPlainText().replace('\\', '/')
        assert 'Workspace file: ' + path.as_posix() in text, text

    def baseline(path):
        active_workspace(path)
        assert control('packDisplayName').text() == ''
        assert control('packOwner').text() == ''
        assert control('packVersion').text() == '0.1.0'
        profile = json.loads(path.read_text(encoding='utf-8'))['GameProfiles'][0]
        assert field('Primary game version').text() == profile['GameVersion']
        assert field('Target branch').text() == profile['Branch']
        assert control('packDraftStatus').text() == 'New mod draft'
        combo = control('packSavedMods')
        for index in range(combo.count()):
            if combo.itemData(index):
                assert Path(combo.itemData(index)).resolve().is_relative_to(path.parent.resolve())

    def manifest(path):
        data = json.loads(path.read_text(encoding='utf-8'))
        return data.get('ClassData', data)

    def save():
        button('Save mod').click()
        assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        dirty(False)

    def reopen(path):
        combo = control('packSavedMods')
        combo.setCurrentIndex(next(index for index in range(combo.count())
                                   if Path(combo.itemData(index)).resolve() == path.resolve()))
        button('Open selected').click()
        assert control('packDisplayName').text() == manifest(path)['DisplayName']
        dirty(False)

    def switch(path, choice=None, blocked=False, invalid=False, picker_cancel=False):
        before = snapshot()
        current = details.toPlainText()
        active = control('packActiveSummary').text()
        seen, errors = [], []
        started = time.monotonic()
        timer = QtCore.QTimer()
        keep.append(timer)

        def answer():
            try:
                values = widgets()
                picker = next((value for value in values if isinstance(value, QtWidgets.QFileDialog)
                               and value.isVisible()), None)
                if picker:
                    if 'picker' not in seen:
                        seen.append('picker')
                        if picker_cancel:
                            picker.reject()
                        else:
                            picker.setDirectory(str(path.parent))
                            picker.selectFile(path.name)
                            picker.findChild(QtWidgets.QLineEdit, 'fileNameEdit').setText(path.name)
                        return
                    assert time.monotonic() - started < 4, ('File picker stayed open', picker.selectedFiles())
                    box = picker.findChild(QtWidgets.QDialogButtonBox)
                    keep.append(box)
                    box.button(QtWidgets.QDialogButtonBox.Open).click()
                    return
                prompt = next((value for value in values if isinstance(value, QtWidgets.QMessageBox)
                               and value.isVisible()), None)
                if prompt:
                    if prompt.objectName() == 'packUnsavedChangesDialog':
                        seen.append('draft')
                        assert choice is not None, 'Unexpected unsaved-change prompt'
                        buttons = QtWidgets.QMessageBox
                        assert prompt.standardButtons() == buttons.Save | buttons.Discard | buttons.Cancel
                        assert prompt.defaultButton() == prompt.button(buttons.Cancel)
                        assert prompt.escapeButton() == prompt.button(buttons.Cancel)
                        assert 'switching workspaces' in prompt.text()
                        if choice == 'Cancel':
                            assert prompt.grab().save(str(output.with_suffix('.prompt.png')))
                        if choice == 'Escape':
                            QtTest.QTest.keyClick(prompt, QtCore.Qt.Key_Escape)
                        elif choice == 'Close':
                            prompt.close()
                        else:
                            prompt.button(getattr(buttons, choice)).click()
                    else:
                        seen.append('error')
                        assert invalid, 'Unexpected error dialog: ' + prompt.text()
                        prompt.accept()
            except Exception:
                errors.append(traceback.format_exc())
                for value in widgets():
                    if isinstance(value, QtWidgets.QDialog) and value.isVisible():
                        value.reject()

        timer.timeout.connect(answer)
        timer.start(25)
        try:
            open_button.click()
            QtTest.QTest.qWait(100)
        finally:
            timer.stop()
        elapsed = time.monotonic() - started
        result['transition_seconds'].append(round(elapsed, 4))
        assert not errors, errors
        assert seen.count('picker') == 1, seen
        assert seen.count('draft') == (1 if choice else 0), seen
        assert seen.count('error') == int(invalid), seen
        assert elapsed < 5, 'Workspace interaction exceeded five-second hang guard'
        if blocked:
            assert snapshot() == before, 'Blocked switch changed draft fields'
            assert details.toPlainText() == current, 'Blocked switch changed workspace'
            assert control('packActiveSummary').text() == active, 'Blocked switch changed active mod'
            assert isValid(root) and root.isVisible()
        else:
            baseline(path)
        result['checks'].append(('blocked' if blocked else 'switched') + '_' +
                                (choice or ('invalid' if invalid else 'picker_cancel' if picker_cancel else 'clean')))
        stage('workspace_interaction')

    try:
        stage('fixture')
        assert not any('autotest_mode' in arg for arg in app.arguments())
        assert route in ('status', 'catalog')
        assert workspace.resolve() == (Path(os.environ['LOCALAPPDATA']) /
                                       'FOA-SDK/Workspace/foa-sdk.tgworkspace.json').resolve()
        fixture = json.loads(workspace.read_text(encoding='utf-8'))
        assert fixture['WorkspaceId'] == 'sdkqa.pack-workspace-switch'
        assert Path(fixture['RootPath']).resolve() == workspace.parent.resolve()
        assert not (workspace.parent / 'Packs').exists()
        other_root = workspace.parent.parent / 'WorkspaceB'
        assert not other_root.exists()
        # Replace only this fixture's synthetic root; never read or modify a real installation.
        other = json.loads(json.dumps(fixture).replace(workspace.parent.as_posix(), other_root.as_posix()))
        other['WorkspaceId'] += '-b'
        other['DisplayName'] = 'Synthetic second workspace'
        other['GameProfiles'][0]['GameVersion'] = '2.0.0'
        for leaf in ('Output', 'Staging', 'Deployment', 'Diagnostics', 'Extracted',
                     'Game/Managed', 'Game/BepInEx/plugins'):
            (other_root / leaf).mkdir(parents=True, exist_ok=True)
        for leaf in ('Game/Managed/Assembly-CSharp.dll', 'Game/UnityPlayer.dll'):
            (other_root / leaf).write_text('Synthetic authoring marker only.', encoding='utf-8')
        second = other_root / 'second.tgworkspace.json'
        second.write_text(json.dumps(other, indent=2), encoding='utf-8')
        invalid = other_root / 'invalid.tgworkspace.json'
        invalid.write_text('{ malformed fixture', encoding='utf-8')
        # Use Qt's real, non-native picker so the in-process test can select the file.
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        general.idle_enable(True)
        QtTest.QTest.qWait(3000)
        general.open_pane('Tainted Grail SDK Status')
        QtTest.QTest.qWait(500)
        toggle = control('foundationAdvancedToggle')
        if toggle.text() == 'Show advanced details':
            toggle.click()
        details = next(value for value in widgets() if isinstance(value, QtWidgets.QPlainTextEdit)
                       and value.toPlainText().startswith('Workspace file:'))
        active_workspace(workspace)
        general.open_pane('Tainted Grail Pack Manager')
        if route == 'catalog':
            general.open_pane('Tainted Grail Catalog Browser')
        QtTest.QTest.qWait(500)
        root = control('TaintedGrailPackManager')
        text = 'Open existing workspace...' if route == 'status' else 'Open Workspace...'
        open_button = next(value for value in widgets()
                           if isinstance(value, QtWidgets.QPushButton) and value.text() == text)
        button('New mod').click()
        switch(second)
        switch(workspace)
        control('packDisplayName').setText('Original')
        control('packOwner').setText('sdkqa')
        save()
        original = workspace.parent / 'Packs/sdkqa.original/pack.tgpack.json'
        original_bytes = original.read_bytes()
        control('packDisplayName').setText('Saved before switch')
        control('packVersion').setText('2.3.4')
        field('Primary game version').setText('1.1.0')
        for choice in ('Cancel', 'Escape', 'Close'):
            switch(second, choice, blocked=True)
            assert original.read_bytes() == original_bytes
        switch(second, blocked=True, picker_cancel=True)
        switch(invalid, blocked=True, invalid=True)
        switch(workspace, 'Cancel', blocked=True)
        control('packVersion').setText('invalid')
        switch(second, 'Save', blocked=True)
        assert original.read_bytes() == original_bytes
        control('packVersion').setText('2.3.4')
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                      ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.CreateFileW(str(original), 0x80000000, 1, None, 3, 0x80, None)
        assert handle not in (None, ctypes.c_void_p(-1).value), ctypes.get_last_error()
        try:
            switch(second, 'Save', blocked=True)
            assert original.read_bytes() == original_bytes
        finally:
            kernel.CloseHandle(handle)
        dirty()
        assert root.grab().save(str(output.with_suffix('.draft.png')))
        switch(second, 'Save')
        assert manifest(original)['DisplayName'] == 'Saved before switch'
        assert manifest(original)['Version'] == '2.3.4'
        assert not (other_root / 'Packs').exists()
        result['checks'].append('saved_into_original_workspace_before_switch')
        control('packDisplayName').setText('Second mod')
        control('packOwner').setText('sdkqa')
        save()
        second_mod = other_root / 'Packs/sdkqa.second-mod/pack.tgpack.json'
        assert second_mod.exists()
        switch(workspace)
        reopen(original)
        saved_bytes = original.read_bytes()
        control('packDisplayName').setText('Discarded edit')
        switch(second, 'Discard')
        assert original.read_bytes() == saved_bytes
        reopen(second_mod)
        button('New mod').click()
        control('packDisplayName').setText('New workspace draft')
        switch(workspace, 'Save', blocked=True)
        assert control('packStatus').text() == 'Enter an author or namespace.'
        control('packOwner').setText('sdkqa')
        switch(workspace, 'Save')
        created = other_root / 'Packs/sdkqa.new-workspace-draft/pack.tgpack.json'
        assert created.exists()
        assert not (workspace.parent / 'Packs/sdkqa.new-workspace-draft').exists()
        result['checks'].append('new_draft_saved_to_original_root_without_cross_workspace_leak')
        control('packDisplayName').setText('Never saved')
        control('packOwner').setText('sdkqa')
        switch(second, 'Discard')
        assert not (workspace.parent / 'Packs/sdkqa.never-saved').exists()
        reopen(second_mod)
        control('packVersion').setText('3.2.1')
        switch(second, 'Save')
        assert manifest(second_mod)['Version'] == '3.2.1'
        reopen(second_mod)
        switch(second)
        control('packDisplayName').setText('Same workspace discard')
        switch(second, 'Discard')
        dirty(False)
        result['checks'].append('same_workspace_save_discard_and_clean_reload')
        result['status'] = 'PASSED'
        stage('complete')
        def quitting():
            result['about_to_quit'] = True
            stage('about_to_quit')
        app.aboutToQuit.connect(quitting)
        keep.append(quitting)
        QtCore.QTimer.singleShot(100, general.exit)
    except Exception:
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
        stage('failed')
    finally:
        if had_setting:
            settings.setValue(setting_key, old_setting)
        else:
            settings.remove(setting_key)
        settings.sync()


run()