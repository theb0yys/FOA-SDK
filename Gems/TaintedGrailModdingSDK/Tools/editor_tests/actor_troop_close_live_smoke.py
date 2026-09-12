# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native Actor/Troop close acceptance with synthetic data and saved-byte checks."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import sys
import time
import traceback

Path(os.environ['FOA_SDK_POPULATION_RESULT']).write_text(
    json.dumps({'status': 'PARTIAL', 'stage': 'loading_host_modules'}), encoding='utf-8')

import azlmbr.editor as editor
import azlmbr.legacy.general as general
from PySide6 import QtCore, QtGui, QtTest, QtWidgets
from shiboken6 import isValid

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pack_pane_test_support as pane_test

PANE = 'Tainted Grail Actor and Troop Editor'
DOCK = 'TaintedGrailModdingSDK.ActorTroopEditor'
_keep = []


def run_population_close_smoke():
    output = Path(os.environ['FOA_SDK_POPULATION_RESULT'])
    workspace = Path(os.environ['FOA_SDK_POPULATION_WORKSPACE'])
    catalog_path = workspace.parent / 'Catalog/catalog.tgcatalog.json'
    result = {'status': 'PARTIAL', 'checks': [], 'transition_seconds': [],
              'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    root = None

    def stage(name):
        result['stage'] = name
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')

    def checked(name):
        result['checks'].append(name)
        stage(name)

    def widgets():
        values = app.allWidgets()
        _keep.extend(values)
        return [w for w in values if isValid(w)]

    def control(name):
        return next(w for w in reversed(widgets()) if w.objectName() == name)

    def edit(name, value):
        field = control(name)
        assert field.isEnabled() and not field.isReadOnly(), name
        # The product deliberately listens to textEdited, not programmatic
        # textChanged. Exercise actual keyboard editing on the owning tab.
        tabs = control('populationTabs')
        for index in range(tabs.count()):
            if tabs.widget(index).isAncestorOf(field):
                tabs.setCurrentIndex(index)
                break
        field.setFocus()
        QtTest.QTest.keyClick(field, QtCore.Qt.Key_A, QtCore.Qt.ControlModifier)
        QtTest.QTest.keyClicks(field, value)
        assert field.text() == value, (name, field.text(), value)

    def choose(name, identity):
        combo = control(name)
        index = combo.findData(identity)
        assert index >= 0, (name, identity)
        combo.setCurrentIndex(index)

    def catalog():
        data = json.loads(catalog_path.read_text(encoding='utf-8'))
        return data.get('ClassData', data)

    def row(collection, identity):
        return next(v for v in catalog()[collection] if v['RecordId'] == identity)

    def members(troop):
        return [v for v in catalog()['TroopMembers'] if v['TroopRecordId'] == troop]

    def member(troop, actor):
        return next(v for v in members(troop) if v['ActorRecordId'] == actor)

    def snapshot():
        values = []
        fields = root.findChildren(QtWidgets.QWidget)
        _keep.extend(fields)
        for field in fields:
            if isinstance(field, QtWidgets.QLineEdit):
                value = field.text()
            elif isinstance(field, (QtWidgets.QSpinBox, QtWidgets.QDoubleSpinBox)):
                value = field.value()
            elif isinstance(field, QtWidgets.QCheckBox):
                value = field.isChecked()
            elif isinstance(field, QtWidgets.QComboBox):
                value = (field.currentData(), field.currentText())
            elif isinstance(field, QtWidgets.QListWidget):
                value = [(field.item(i).data(QtCore.Qt.UserRole), field.item(i).isSelected())
                         for i in range(field.count())]
            else:
                continue
            values.append((field.metaObject().className(), field.objectName(), value))
        table = control('populationMembers')
        values.append(('members', [[table.item(r, c).text() if table.item(r, c) else ''
                                    for c in range(table.columnCount())] for r in range(table.rowCount())]))
        values.append(('tab', control('populationTabs').currentIndex()))
        return values

    def close(choice=None, accepted=False, nested=False, capture=False):
        dock = pane_test.pane_dock(root)
        floating = pane_test.floating_container(root)
        seen, errors = [], []
        started = time.monotonic()
        timer = QtCore.QTimer()
        _keep.append(timer)

        def answer():
            prompt = next((w for w in widgets() if isinstance(w, QtWidgets.QMessageBox) and w.isVisible()), None)
            if prompt is None:
                return
            timer.stop()
            try:
                seen.append(prompt.objectName())
                buttons = QtWidgets.QMessageBox
                assert prompt.standardButtons() == buttons.Save | buttons.Discard | buttons.Cancel, 'Missing Save / Discard / Cancel'
                assert prompt.objectName() == 'populationUnsavedChangesDialog'
                assert prompt.defaultButton() == prompt.button(buttons.Cancel)
                assert prompt.escapeButton() == prompt.button(buttons.Cancel)
                if capture:
                    assert prompt.grab().save(str(output.with_name('close-prompt.png')))
                if nested:
                    event = QtGui.QCloseEvent()
                    app.sendEvent(root, event)
                    assert not event.isAccepted() and prompt.isVisible(), 'Nested close bypassed the open prompt'
                if choice == 'Escape':
                    QtTest.QTest.keyClick(prompt, QtCore.Qt.Key_Escape)
                elif choice == 'Close':
                    prompt.close()
                else:
                    assert choice is not None, 'Clean pane unexpectedly prompted'
                    prompt.button(getattr(buttons, choice)).click()
            except Exception:
                errors.append(traceback.format_exc())
                prompt.reject()

        timer.timeout.connect(answer)
        timer.start(25)
        pane_test.request_titlebar_close(root, result, _keep)
        timer.stop()
        elapsed = time.monotonic() - started
        result['transition_seconds'].append(round(elapsed, 4))
        assert not errors and len(seen) == int(choice is not None), (errors, seen, choice)
        assert elapsed < 5, 'Close exceeded the five-second synthetic fixture budget'
        deadline = time.monotonic() + 5
        while accepted and time.monotonic() < deadline and (isValid(dock) or (floating is not None and isValid(floating))):
            QtTest.QTest.qWait(25)
        if accepted:
            assert not isValid(root) and not isValid(dock), 'Accepted close retained the pane'
            assert floating is None or not isValid(floating)
            assert not general.is_pane_visible(PANE)
        else:
            assert isValid(root) and root.isVisible() and isValid(dock) and dock.isVisible()

    def reopen(floating=False, actor=None, troop=None):
        nonlocal root
        pane_test.open_default_pane(PANE)
        QtTest.QTest.qWait(250)
        dock = control(DOCK)
        root = dock.widget()
        _keep.extend((dock, root))
        assert root.isVisible() and pane_test.floating_container(root) is None
        if floating:
            pane_test.pane_menu_action(root, 'Undock', result, _keep)
            QtTest.QTest.qWait(150)
            assert pane_test.floating_container(root) is not None
        if actor:
            choose('populationActor', actor)
        if troop:
            choose('populationTroop', troop)

    def create(name, troop=False):
        stage('creating_' + name)
        expected = [None, name] if troop else [name]
        seen = []
        timer = QtCore.QTimer()
        _keep.append(timer)
        def answer():
            prompt = next((w for w in widgets() if isinstance(w, QtWidgets.QInputDialog) and w.isVisible()), None)
            if prompt is not None:
                assert len(seen) < len(expected)
                value = expected[len(seen)]
                seen.append(value)
                if value is not None:
                    prompt.setTextValue(value)
                prompt.accept()
        timer.timeout.connect(answer)
        timer.start(25)
        control('populationNewTroop' if troop else 'populationNewActor').click()
        timer.stop()
        assert seen == expected, (seen, expected)
        identity = control('populationTroop' if troop else 'populationActor').currentData()
        assert identity and 'Created ' in control('populationStatus').text(), control('populationStatus').text()
        return identity

    def select_member(troop, actor):
        table = control('populationMembers')
        link = member(troop, actor)['LinkId']
        index = next(r for r in range(table.rowCount()) if table.item(r, 0).text() == link)
        table.selectRow(index)
        assert control('populationMemberActor').currentData() == actor

    def new_member(actor):
        control('populationNewMember').click()
        choose('populationMemberActor', actor)
        role = control('populationMemberRole')
        assert role.findText('melee') >= 0
        role.setCurrentText('melee')
        control('populationMemberMinimum').setValue(1)
        control('populationMemberMaximum').setValue(1)
        edit('populationMemberWeight', '1')

    try:
        stage('fixture_started')
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        fixture = json.loads(workspace.read_text(encoding='utf-8-sig'))
        assert fixture['WorkspaceId'] == 'sdkqa.actor-troop-close'
        assert Path(fixture['RootPath']).resolve() == workspace.parent.resolve()
        assert workspace.resolve() == (Path(os.environ['LOCALAPPDATA']) / 'FOA-SDK/Workspace/foa-sdk.tgworkspace.json').resolve()
        assert not catalog_path.exists()
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
        kernel.GetModuleHandleW.restype = ctypes.c_void_p
        kernel.GetModuleFileNameW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint32]
        buffer = ctypes.create_unicode_buffer(32768)
        module = kernel.GetModuleHandleW('TaintedGrailModdingSDK.Editor.dll')
        assert module and kernel.GetModuleFileNameW(module, buffer, len(buffer))
        result['sdk_module_path'] = buffer.value
        result['sdk_module_sha256'] = hashlib.sha256(Path(buffer.value).read_bytes()).hexdigest()
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                      ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        general.idle_enable(True)
        QtTest.QTest.qWait(3000)
        stage('opening_pack')
        pane_test.open_default_pack()
        QtTest.QTest.qWait(350)
        stage('saving_fixture_pack')
        control('packDisplayName').setText('Population close fixture')
        control('packOwner').setText('sdkqa')
        pack = control('TaintedGrailPackManager')
        next(b for b in pack.findChildren(QtWidgets.QPushButton) if b.text() == 'Save mod').click()
        assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        assert pane_test.pane_dock(pack).close()
        QtTest.QTest.qWait(250)
        stage('opening_population_pane')
        reopen()
        actor_a, actor_b, actor_c = [create(name) for name in ('Actor A', 'Actor B', 'Actor C')]
        result['actors'] = [actor_a, actor_b, actor_c]
        close(accepted=True)
        checked('created_actor_definitions_close_cleanly')

        for floating, mode in ((False, 'docked'), (True, 'floating')):
            stage(mode + '_setup')
            reopen(floating, actor=actor_a)
            troop = create(mode + ' troop', troop=True)
            assert row('TroopProfiles', troop)['LeaderActorRecordId'] == actor_a
            close(accepted=True)
            reopen(floating, actor_a, troop)
            select_member(troop, actor_a)
            before = catalog_path.read_bytes()
            old_archetype = row('ActorProfiles', actor_a)['Archetype']
            edit('populationArchetype', mode + ' draft')
            control('populationMaximumSize').setValue(3)
            edit('populationMemberWeight', '  2.7500  ')
            original = snapshot()
            for choice in ('Cancel', 'Escape', 'Close'):
                close(choice, capture=(mode == 'docked' and choice == 'Cancel'))
                assert snapshot() == original and catalog_path.read_bytes() == before
                checked(mode + '_' + choice.lower() + '_preserves_all_three_drafts_without_writes')
            close('Cancel', nested=True)
            assert snapshot() == original and catalog_path.read_bytes() == before
            checked(mode + '_nested_close_is_refused_while_prompt_is_open')
            close('Discard', accepted=True)
            assert catalog_path.read_bytes() == before
            reopen(floating, actor_a, troop)
            assert control('populationArchetype').text() == old_archetype
            assert control('populationMaximumSize').value() == 1
            select_member(troop, actor_a)
            assert control('populationMemberWeight').text() == '1'
            close(accepted=True)
            checked(mode + '_discard_reopens_saved_values_and_stays_clean')

            reopen(floating, actor_a)
            edit('populationArchetype', mode + ' actor only')
            close('Save', accepted=True)
            assert row('ActorProfiles', actor_a)['Archetype'] == mode + ' actor only'
            checked(mode + '_actor_only_save_skips_unselected_clean_troop')
            reopen(floating, troop=troop)
            control('populationMaximumSize').setValue(2)
            close('Save', accepted=True)
            assert row('TroopProfiles', troop)['MaximumSize'] == 2
            checked(mode + '_troop_only_save_skips_unselected_clean_actor')
            reopen(floating, troop=troop)
            select_member(troop, actor_a)
            edit('populationMemberWeight', '2.5')
            close('Save', accepted=True)
            assert member(troop, actor_a)['Weight'] == 2.5
            checked(mode + '_unstaged_member_only_save_is_persisted')

            reopen(floating, actor_a, troop)
            edit('populationArchetype', mode + ' all saved')
            control('populationMaximumSize').setValue(3)
            new_member(actor_b)
            close('Save', accepted=True)
            assert row('ActorProfiles', actor_a)['Archetype'] == mode + ' all saved'
            assert row('TroopProfiles', troop)['MaximumSize'] == 3
            assert {m['ActorRecordId'] for m in members(troop)} == {actor_a, actor_b}
            reopen(floating, actor_a, troop)
            select_member(troop, actor_b)
            assert control('populationMemberWeight').text() == '1'
            close(accepted=True)
            checked(mode + '_all_three_drafts_save_and_reopen_cleanly')

            reopen(floating, actor_a, troop)
            select_member(troop, actor_a)
            control('populationMinimumLevel').setValue(10)
            control('populationMaximumLevel').setValue(2)
            control('populationMaximumSize').setValue(4)
            edit('populationMemberWeight', '  3.7500  ')
            before, original = catalog_path.read_bytes(), snapshot()
            close('Save')
            assert catalog_path.read_bytes() == before and snapshot() == original
            close('Discard', accepted=True)
            checked(mode + '_invalid_actor_stops_all_saves_and_preserves_raw_forms')

            reopen(floating, actor_a, troop)
            select_member(troop, actor_a)
            edit('populationArchetype', mode + ' partial actor')
            control('populationMaximumSize').setValue(4)
            edit('populationMemberWeight', 'not-a-number')
            old_troop = row('TroopProfiles', troop)
            close('Save')
            assert row('ActorProfiles', actor_a)['Archetype'] == mode + ' partial actor'
            assert row('TroopProfiles', troop) == old_troop
            assert control('populationMemberWeight').text() == 'not-a-number'
            assert control('populationMaximumSize').value() == 4
            before, original = catalog_path.read_bytes(), snapshot()
            close('Cancel')
            assert catalog_path.read_bytes() == before and snapshot() == original
            edit('populationMemberWeight', '3.25')
            close('Save', accepted=True)
            assert member(troop, actor_a)['Weight'] == 3.25
            checked(mode + '_partial_actor_success_invalid_member_cancel_and_retry')

            reopen(floating, troop=troop)
            select_member(troop, actor_a)
            control('populationMinimumSize').setValue(5)
            control('populationMaximumSize').setValue(2)
            edit('populationMemberWeight', '4.5')
            before = catalog_path.read_bytes()
            close('Save')
            assert catalog_path.read_bytes() == before
            assert control('populationMinimumSize').value() == 5
            assert control('populationMemberWeight').text() == '4.5'
            control('populationMinimumSize').setValue(1)
            close('Save', accepted=True)
            assert member(troop, actor_a)['Weight'] == 4.5
            checked(mode + '_failed_atomic_troop_keeps_staged_member_for_retry')

            for actor_dirty in (True, False):
                reopen(floating, actor_a if actor_dirty else None, troop)
                select_member(troop, actor_a)
                if actor_dirty:
                    edit('populationArchetype', mode + ' locked actor')
                size = 3 if actor_dirty else 4
                weight = 5.5 if actor_dirty else 6.5
                control('populationMaximumSize').setValue(size)
                edit('populationMemberWeight', str(weight))
                before = catalog_path.read_bytes()
                handle = kernel.CreateFileW(str(catalog_path), 0x80000000, 1, None, 3, 0x80, None)
                assert handle not in (None, ctypes.c_void_p(-1).value)
                try:
                    close('Save')
                    assert catalog_path.read_bytes() == before
                    assert control('populationMaximumSize').value() == size
                    assert control('populationMemberWeight').text() == str(weight)
                    if actor_dirty:
                        assert control('populationArchetype').text() == mode + ' locked actor'
                finally:
                    kernel.CloseHandle(handle)
                close('Save', accepted=True)
                assert row('TroopProfiles', troop)['MaximumSize'] == size
                assert member(troop, actor_a)['Weight'] == weight
                checked(mode + '_real_write_failure_' + ('actor' if actor_dirty else 'troop') + '_retains_drafts_and_retry_saves')

            reopen(floating, troop=troop)
            select_member(troop, actor_b)
            next(b for b in root.findChildren(QtWidgets.QPushButton) if b.text() == 'Remove selected member').click()
            new_member(actor_c)
            control('populationStageMember').click()
            select_member(troop, actor_a)
            edit('populationMemberWeight', '7.25')
            control('populationStageMember').click()
            before, original = catalog_path.read_bytes(), snapshot()
            close('Cancel')
            assert catalog_path.read_bytes() == before and snapshot() == original
            close('Save', accepted=True)
            assert {m['ActorRecordId'] for m in members(troop)} == {actor_a, actor_c}
            assert member(troop, actor_a)['Weight'] == 7.25
            checked(mode + '_staged_addition_edit_and_removal_save_atomically')

            reopen(floating, troop=troop)
            select_member(troop, actor_a)
            control('populationMemberMinimum').setValue(5)
            control('populationMemberMaximum').setValue(2)
            before, original = catalog_path.read_bytes(), snapshot()
            close('Save')
            assert catalog_path.read_bytes() == before and snapshot() == original
            close('Discard', accepted=True)
            checked(mode + '_invalid_member_count_retains_failed_save')

        assert len(result['checks']) == 33, result['checks']
        result['status'] = 'PASSED'
        stage('complete')
    except Exception:
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
        stage(result.get('stage', 'failed'))
    if result['status'] == 'PASSED':
        def quit_seen():
            result['about_to_quit'] = True
            output.write_text(json.dumps(result, indent=2), encoding='utf-8')
        app.aboutToQuit.connect(quit_seen)
        _keep.append(quit_seen)
        QtCore.QTimer.singleShot(0, general.exit)


def population_close_initialized(_args):
    _population_close_handler.disconnect()
    QtCore.QTimer.singleShot(0, run_population_close_smoke)


_population_close_handler = editor.EditorEventBusHandler()
_population_close_handler.connect()
_population_close_handler.add_callback('NotifyEditorInitialized', population_close_initialized)
Path(os.environ['FOA_SDK_POPULATION_RESULT']).write_text(
    json.dumps({'status': 'PARTIAL', 'stage': 'waiting_for_editor_initialized'}), encoding='utf-8')
