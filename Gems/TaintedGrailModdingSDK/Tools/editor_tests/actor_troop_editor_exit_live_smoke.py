# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native Actor/Troop whole-Editor exit acceptance with synthetic data and saved-byte checks."""
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


def run_population_exit_smoke():
    output = Path(os.environ['FOA_SDK_POPULATION_RESULT'])
    workspace = Path(os.environ['FOA_SDK_POPULATION_WORKSPACE'])
    catalog_path = workspace.parent / 'Catalog/catalog.tgcatalog.json'
    result = {'status': 'PARTIAL', 'checks': [], 'transition_seconds': [],
              'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    root = None
    case = os.environ["FOA_SDK_POPULATION_WORKSPACE_ROUTE"].removeprefix("exit-")
    result["case"] = case
    result["prompt_orders"] = []

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
        assert field.isEnabled() and not field.isReadOnly(), (name, control('populationRecoveryStatus').text())
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
                value = (field.currentData(), field.currentText(),
                         [(field.itemData(i), field.itemText(i)) for i in range(field.count())])
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

    def reopen(floating=False, actor=None, troop=None, allow_existing=False):
        nonlocal root
        pane_test.open_default_pane(PANE)
        QtTest.QTest.qWait(250)
        dock = control(DOCK)
        root = dock.widget()
        _keep.extend((dock, root))
        assert root.isVisible()
        if not allow_existing:
            assert pane_test.floating_container(root) is None
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

    def route_exit(route):
        if route == 'python':
            general.exit()
        elif route == 'file':
            bar = main.menuBar()
            actions = bar.actions()
            _keep.extend((bar, *actions))
            menu = next(a for a in actions if a.text().replace('&', '') == 'File').menu()
            _keep.append(menu)
            actions = menu.actions()
            _keep.extend(actions)
            action = next(a for a in actions if a.objectName() == 'o3de.action.editor.exit')
            assert action.isEnabled()
            action.trigger()
        else:
            candidates = []
            for b in main.window().findChildren(QtWidgets.QToolButton, 'closeButton'):
                if not b.isVisible() or b.visibleRegion().isEmpty():
                    continue
                parent = b.parentWidget()
                while parent and not isinstance(parent, QtWidgets.QDockWidget):
                    parent = parent.parentWidget()
                if parent is None:
                    candidates.append(b)
            _keep.extend(candidates)
            assert len(candidates) == 1
            QtTest.QTest.mouseClick(candidates[0], QtCore.Qt.LeftButton)

    def attempt(choice, route, accepted=False, pack_choice=None, nested=False):
        nonlocal root
        before = snapshot()
        result.setdefault('presentations', []).append('floating' if pane_test.floating_container(root) else 'docked')
        seen, errors = [], []
        started = time.monotonic()
        timer = QtCore.QTimer()
        _keep.append(timer)

        def answer():
            prompt = next((w for w in widgets() if isinstance(w, QtWidgets.QMessageBox) and w.isVisible()), None)
            if prompt is None:
                return
            try:
                name = prompt.objectName()
                assert name in ('populationUnsavedChangesDialog', 'packUnsavedChangesDialog'), prompt.text()
                is_population = name == 'populationUnsavedChangesDialog'
                seen.append('population' if is_population else 'pack')
                selected = choice if is_population else pack_choice
                assert selected is not None, (name, prompt.text())
                buttons = QtWidgets.QMessageBox
                assert prompt.standardButtons() == buttons.Save | buttons.Discard | buttons.Cancel
                assert prompt.defaultButton() == prompt.button(buttons.Cancel)
                assert prompt.escapeButton() == prompt.button(buttons.Cancel)
                if is_population and route != 'python':
                    assert 'exiting the Editor' in prompt.text(), prompt.text()
                # Legacy Python exit calls closeAllWindows and may ask a floating
                # pane directly before the main window. Its cancellation must
                # still preserve drafts through the existing pane-close guard.
                if is_population and nested:
                    event = QtGui.QCloseEvent()
                    app.sendEvent(main, event)
                    assert not event.isAccepted() and prompt.isVisible(), 'Nested Editor exit bypassed the prompt'
                    event = QtGui.QCloseEvent()
                    app.sendEvent(root, event)
                    assert not event.isAccepted() and prompt.isVisible(), 'Nested pane close bypassed the prompt'
                if is_population and selected == 'Cancel':
                    assert prompt.grab().save(str(output.with_name('exit-prompt.png')))
                if selected == 'Escape':
                    QtTest.QTest.keyClick(prompt, QtCore.Qt.Key_Escape)
                elif selected == 'Close':
                    prompt.close()
                else:
                    prompt.button(getattr(buttons, selected)).click()
            except Exception:
                errors.append(traceback.format_exc())
                prompt.reject()

        timer.timeout.connect(answer)
        timer.start(25)
        try:
            route_exit(route)
            if not accepted:
                QtTest.QTest.qWait(350)
        finally:
            timer.stop()
        elapsed = time.monotonic() - started
        result['transition_seconds'].append(round(elapsed, 4))
        result['prompt_orders'].append(seen)
        assert not errors, errors
        assert seen.count('population') == int(choice is not None), seen
        assert seen.count('pack') == int(pack_choice is not None), seen
        assert elapsed < 5, 'Exit exceeded five-second synthetic fixture budget'
        if accepted:
            assert not isValid(root), 'Accepted exit did not destroy the pane'
        else:
            assert isValid(main) and main.isVisible(), 'Cancelled exit closed the Editor'
            current = control('TaintedGrailModdingSDK.ActorTroopEditor').widget()
            assert current.isVisible(), 'Cancelled exit lost the Actor/Troop pane'
            root = current
            _keep.append(root)
            assert control('populationTabs').isEnabled(), control('populationRecoveryStatus').text()
            if choice != 'Save':
                assert snapshot() == before, 'Cancelled exit lost retained drafts'
        checked(route + '_' + str(choice) + ('_accepted' if accepted else '_kept_editor_and_drafts'))
        return seen

    def begin_exit(choice, route, verify, pack_choice=None):
        def quit_seen():
            try:
                verify()
                result['about_to_quit'] = True
                stage('about_to_quit')
            except Exception:
                result['status'] = 'FAILED'
                result['error'] = traceback.format_exc()
                stage('failed')

        def finish():
            try:
                attempt(choice, route, accepted=True, pack_choice=pack_choice)
                verify()
                checked('accepted_exit_has_expected_catalog_bytes')
                result['status'] = 'PASSED'
                stage('complete')
            except Exception:
                result['status'] = 'FAILED'
                result['error'] = traceback.format_exc()
                stage('failed')

        app.aboutToQuit.connect(quit_seen)
        _keep.extend((quit_seen, finish))
        QtCore.QTimer.singleShot(100, finish)
        stage('exit_scheduled')

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
        stage('opening_population_pane')
        reopen(allow_existing=True)

        assert case in ('save', 'discard', 'clean', 'rollback')
        assert not any('autotest_mode' in a for a in app.arguments())
        # Recreate a clean pane so persisted floating layout cannot affect order.
        pane_test.request_titlebar_close(root, result, _keep)
        QtTest.QTest.qWait(250)
        assert not isValid(root)
        reopen()
        main = control('MainWindow')
        actor_a, actor_b, actor_c = [create(name) for name in ('Actor A', 'Actor B', 'Actor C')]
        choose('populationActor', actor_a)
        troop = create('Exit troop', troop=True)
        select_member(troop, actor_a)
        new_member(actor_b)
        control('populationSaveTroop').click()
        assert len(members(troop)) == 2
        select_member(troop, actor_a)
        initial = catalog_path.read_bytes()
        checked('saved_fixture_with_two_members')

        def unchanged():
            assert catalog_path.read_bytes() == initial

        def edit_all():
            edit('populationArchetype', '  exit actor draft  ')
            control('populationMaximumSize').setValue(5)
            select_member(troop, actor_b)
            # Use the actual enabled remove button; its visible copy owns this action.
            button = next(b for b in root.findChildren(QtWidgets.QPushButton)
                          if b.text() == 'Remove selected member')
            button.click()
            new_member(actor_c)
            control('populationStageMember').click()
            assert control('populationMembers').rowCount() == 2
            select_member(troop, actor_a)
            edit('populationMemberWeight', '  2.7500  ')

        def all_saved():
            assert row('ActorProfiles', actor_a)['Archetype'] == 'exit actor draft'
            assert row('TroopProfiles', troop)['MaximumSize'] == 5
            assert member(troop, actor_a)['Weight'] == 2.75
            assert member(troop, actor_c)['Weight'] == 1
            assert not any(v['ActorRecordId'] == actor_b for v in members(troop))

        if case == 'clean':
            begin_exit(None, 'file', unchanged)
            return
        edit_all()
        if case == 'discard':
            pane_test.pane_menu_action(root, 'Undock', result, _keep)
            QtTest.QTest.qWait(150)
            assert pane_test.floating_container(root)
            begin_exit('Discard', 'window', unchanged)
            return
        if case == 'rollback':
            # Filtering while dirty defers refresh. Rollback must retain both the
            # raw filter text and the previously displayed record choices.
            for name in ('populationActor', 'populationTroop'):
                choice = control(name)
                field = next(w for w in choice.parentWidget().findChildren(QtWidgets.QLineEdit)
                             if w.placeholderText().startswith('Filter by record ID'))
                field.setText('deferred unmatched filter')
                assert choice.currentData()
            control('packDisplayName').setText('Pack exit veto')
            for floating, mode in ((False, 'docked'), (True, 'floating')):
                if floating and pane_test.floating_container(root) is None:
                    pane_test.pane_menu_action(root, 'Undock', result, _keep)
                    QtTest.QTest.qWait(150)
                assert bool(pane_test.floating_container(root)) == floating
                edit('populationMemberWeight', ' invalid raw weight ')
                order = attempt('Discard', 'file', pack_choice='Cancel')
                assert order == ['population', 'pack'], order
                unchanged()
                assert control('packDisplayName').text() == 'Pack exit veto'
                checked(mode + '_later_pane_veto_restores_raw_forms_and_staging')
                attempt('Cancel', 'window')
                unchanged()
                checked(mode + '_restored_drafts_remain_dirty')
            for name in ('populationActor', 'populationTroop'):
                choice = control(name)
                next(w for w in choice.parentWidget().findChildren(QtWidgets.QLineEdit)
                     if w.placeholderText().startswith('Filter by record ID')).clear()
            edit('populationMemberWeight', '  2.7500  ')
            attempt('Save', 'file', pack_choice='Cancel')
            all_saved()
            checked('save_then_later_veto_keeps_successes_clean')
            begin_exit(None, 'window', all_saved, pack_choice='Discard')
            return

        for floating, mode in ((False, 'docked'), (True, 'floating')):
            if floating and pane_test.floating_container(root) is None:
                pane_test.pane_menu_action(root, 'Undock', result, _keep)
                QtTest.QTest.qWait(150)
            assert bool(pane_test.floating_container(root)) == floating
            for route in ('file', 'window', 'python'):
                for choice in ('Cancel', 'Escape', 'Close'):
                    attempt(choice, route)
                    unchanged()
            attempt('Cancel', 'file', nested=True)
            unchanged()
            checked(mode + '_nested_exit_refused_without_writes')

        control('populationMinimumLevel').setValue(5)
        control('populationMaximumLevel').setValue(1)
        failed = snapshot()
        attempt('Save', 'file')
        assert snapshot() == failed
        unchanged()
        checked('invalid_actor_stops_save_and_keeps_all_drafts')
        control('populationMinimumLevel').setValue(1)
        edit('populationMemberWeight', 'invalid weight')
        attempt('Save', 'window')
        assert row('ActorProfiles', actor_a)['Archetype'] == 'exit actor draft'
        assert row('TroopProfiles', troop)['MaximumSize'] == 1
        assert control('populationMemberWeight').text() == 'invalid weight'
        checked('partial_save_keeps_actor_success_and_remaining_troop_member_drafts')
        edit('populationMemberWeight', '  2.7500  ')
        saved = catalog_path.read_bytes()
        handle = kernel.CreateFileW(str(catalog_path), 0x80000000, 1, None, 3, 0x80, None)
        assert handle not in (None, ctypes.c_void_p(-1).value)
        try:
            attempt('Save', 'file')
            assert catalog_path.read_bytes() == saved
            assert control('populationMaximumSize').value() == 5
            assert control('populationMembers').rowCount() == 2
        finally:
            kernel.CloseHandle(handle)
        checked('real_write_failure_keeps_editor_open_with_staging_for_retry')
        begin_exit('Save', 'file', all_saved)
    except Exception:
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
        stage('failed')


def population_exit_initialized(_args):
    _population_exit_handler.disconnect()
    QtCore.QTimer.singleShot(0, run_population_exit_smoke)


_population_exit_handler = editor.EditorEventBusHandler()
_population_exit_handler.connect()
_population_exit_handler.add_callback('NotifyEditorInitialized', population_exit_initialized)
