# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native Spawn/Encounter close acceptance with synthetic data and saved-byte checks."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import sys
import time
import traceback

Path(os.environ['FOA_SDK_ENCOUNTER_RESULT']).write_text(
    json.dumps({'status': 'PARTIAL', 'stage': 'loading_host_modules'}), encoding='utf-8')

import azlmbr.editor as editor
import azlmbr.legacy.general as general
from PySide6 import QtCore, QtGui, QtTest, QtWidgets
from shiboken6 import isValid

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pack_pane_test_support as pane_test

PANE = 'Tainted Grail Spawn and Encounter Editor'
DOCK = 'TaintedGrailModdingSDK.SpawnEncounterEditor'
_keep = []


def run_encounter_close_smoke():
    output = Path(os.environ['FOA_SDK_ENCOUNTER_RESULT'])
    workspace = Path(os.environ['FOA_SDK_ENCOUNTER_WORKSPACE'])
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

    def snapshot():
        values = []
        fields = root.findChildren(QtWidgets.QWidget)
        _keep.extend(fields)
        for field in fields:
            if isinstance(field, QtWidgets.QLineEdit): value = field.text()
            elif isinstance(field, QtWidgets.QPlainTextEdit): value = field.toPlainText()
            elif isinstance(field, QtWidgets.QSpinBox): value = field.value()
            elif isinstance(field, QtWidgets.QCheckBox): value = field.isChecked()
            elif isinstance(field, QtWidgets.QComboBox): value = (field.currentData(), field.currentText())
            else: continue
            values.append((field.metaObject().className(), field.objectName(), value))
        table = control('encounterEntries')
        values.append(('entries', [(table.item(r, 0).data(QtCore.Qt.UserRole), table.item(r, 0).toolTip(),
                                    table.cellWidget(r, 2).value(), table.cellWidget(r, 3).value())
                                   for r in range(table.rowCount())]))
        return values

    def definition(identity):
        return row('EncounterDefinitions', identity)

    def entry(target):
        table = control('encounterEntries')
        return next(r for r in range(table.rowCount()) if table.item(r, 0).toolTip() == target)

    def quantity(target, low, high):
        table = control('encounterEntries'); r = entry(target)
        table.cellWidget(r, 2).setValue(low); table.cellWidget(r, 3).setValue(high)

    def add(target):
        choose('encounterTarget', target); control('encounterAddEntry').click()
        assert entry(target) >= 0

    def remove(target):
        control('encounterEntries').selectRow(entry(target)); control('encounterRemoveEntry').click()

    def save_direct():
        control('encounterSave').click()
        assert control('encounterStatus').text() == 'Encounter saved.', control('encounterStatus').text()

    def new_encounter(name, target):
        seen, errors = [], []
        choice_index = control('encounterTarget').findData(target)
        assert choice_index >= 0
        timer = QtCore.QTimer(); _keep.append(timer)
        def answer():
            prompt = next((w for w in widgets() if isinstance(w, QtWidgets.QInputDialog) and w.isVisible()), None)
            if prompt is None: return
            try:
                if not seen: prompt.setTextValue(name)
                else:
                    assert len(seen) == 1
                    prompt.findChild(QtWidgets.QComboBox).setCurrentIndex(choice_index)
                seen.append(prompt.windowTitle()); prompt.accept()
            except Exception:
                errors.append(traceback.format_exc()); prompt.reject()
        timer.timeout.connect(answer); timer.start(25)
        try: control('encounterNew').click()
        finally: timer.stop()
        assert len(seen) == 2 and not errors, (seen, errors)
        assert control('encounterStatus').text().startswith('Encounter created and saved.')
        return control('encounterRecords').currentData()

    def reopen(floating=False, identity=None):
        nonlocal root
        pane_test.open_default_pane(PANE); QtTest.QTest.qWait(250)
        dock = control(DOCK); root = dock.widget(); _keep.extend((dock, root))
        assert root.isVisible() and pane_test.floating_container(root) is None
        if floating:
            pane_test.pane_menu_action(root, 'Undock', result, _keep); QtTest.QTest.qWait(150)
            assert pane_test.floating_container(root) is not None
        if identity: choose('encounterRecords', identity)

    def assert_unchanged(before, raw):
        assert catalog_path.read_bytes() == before, 'Rejected close changed saved bytes'
        assert snapshot() == raw, 'Rejected close changed raw fields or composition staging'
        assert control('encounterSave').isEnabled(), 'Rejected close lost the dirty draft'

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
                assert prompt.objectName() == 'encounterUnsavedChangesDialog'
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

    def create_actor(name, troop=False):
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

    try:
        stage('fixture_started')
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        fixture = json.loads(workspace.read_text(encoding='utf-8-sig'))
        assert fixture['WorkspaceId'] == 'sdkqa.encounter-close'
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
        control('packDisplayName').setText('Encounter close fixture')
        control('packOwner').setText('sdkqa')
        pack = control('TaintedGrailPackManager')
        next(b for b in pack.findChildren(QtWidgets.QPushButton) if b.text() == 'Save mod').click()
        assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        assert pane_test.pane_dock(pack).close()
        QtTest.QTest.qWait(250)
        pane_test.open_default_pane('Tainted Grail Actor and Troop Editor')
        deadline = time.monotonic() + 5
        while not control('populationTabs').isEnabled():
            assert time.monotonic() < deadline, control('populationRecoveryStatus').text()
            QtTest.QTest.qWait(25)
        actor_a, actor_b, actor_c = [create_actor(name) for name in ('Captain', 'Guard', 'Scout')]
        actor_root = control('TaintedGrailModdingSDK.ActorTroopEditor').widget()
        pane_test.request_titlebar_close(actor_root, result, _keep); QtTest.QTest.qWait(250)
        assert not isValid(actor_root)
        actors_before = catalog()['ActorProfiles']
        result['actors'] = [actor_a, actor_b, actor_c]
        clean = catalog_path.read_bytes()
        reopen(); close(accepted=True)
        assert catalog_path.read_bytes() == clean
        checked('clean_unselected_pane_closes_without_prompt_or_write')
        reopen()
        untouched = new_encounter('Untouched encounter', actor_c)
        untouched_before = definition(untouched)
        identity = new_encounter('Close fixture', actor_a)
        add(actor_b); control('encounterPopulationLimit').setValue(40); save_direct()
        close(accepted=True)
        checked('synthetic_saved_encounters_and_actors_ready')

        for floating, mode in ((False, 'docked'), (True, 'floating')):
            reopen(floating, identity)
            before = catalog_path.read_bytes(); baseline = definition(identity)
            a_id = next(e['EntryId'] for e in baseline['Entries'] if e['TargetRecordId'] == actor_a)
            # Always start the presentation with Captain and Guard only.
            for target in [e['TargetRecordId'] for e in definition(identity)['Entries'] if e['TargetRecordId'] != actor_a]:
                remove(target)
            add(actor_b); quantity(actor_a, 1, 1); control('encounterPopulationLimit').setValue(40)
            save_direct(); before = catalog_path.read_bytes(); baseline = definition(identity)
            remove(actor_b); add(actor_c); quantity(actor_a, 3, 4)
            edit('encounterName', '  ' + mode + ' patrol  ')
            edit('encounterPlacementSubject', '  North gate courtyard  ')
            edit('encounterCleanup', '  Despawn after patrol  ')
            edit('encounterRollback', '  Restore previous population  ')
            choose('encounterActivation', 'all_conditions')
            control('encounterConditions').setPlainText(chr(10).join(['  Player enters north gate  ', '', 'Quest is active']))
            control('encounterInstances').setValue(2)
            raw = snapshot()
            for choice in ('Cancel', 'Escape', 'Close'):
                close(choice, capture=not floating and choice == 'Cancel'); assert_unchanged(before, raw)
                checked(mode + '_' + choice.lower() + '_preserves_raw_fields_and_staging')
            close('Cancel', nested=True); assert_unchanged(before, raw)
            checked(mode + '_nested_close_is_refused')
            close('Discard', accepted=True); assert catalog_path.read_bytes() == before
            reopen(floating, identity)
            assert definition(identity) == baseline and control('encounterEntries').rowCount() == 2
            assert control('encounterName').text() == row('Records', identity)['DisplayName']
            checked(mode + '_discard_reopens_saved_state_without_writes')

            remove(actor_b); add(actor_c); quantity(actor_a, 3, 4)
            edit('encounterName', mode + ' saved patrol')
            edit('encounterPlacementSubject', 'North gate courtyard')
            edit('encounterCleanup', 'Despawn after patrol'); edit('encounterRollback', 'Restore previous population')
            choose('encounterActivation', 'all_conditions')
            control('encounterConditions').setPlainText(chr(10).join(['Player enters north gate', 'Quest is active']))
            control('encounterInstances').setValue(2); control('encounterPopulationLimit').setValue(40)
            expected_ids = {control('encounterEntries').item(r, 0).toolTip(): control('encounterEntries').item(r, 0).data(QtCore.Qt.UserRole)
                            for r in range(control('encounterEntries').rowCount())}
            close('Save', accepted=True); reopen(floating, identity)
            saved = definition(identity)
            assert {e['TargetRecordId']:e['EntryId'] for e in saved['Entries']} == expected_ids
            assert expected_ids[actor_a] == a_id and actor_b not in expected_ids
            assert saved['PlacementSubjectRef'] == 'North gate courtyard' and saved['MaximumActiveInstances'] == 2
            assert saved['PopulationLimit'] == 40 and saved['ActivationMode'] == 'all_conditions'
            assert saved['Conditions'] == ['Player enters north gate', 'Quest is active']
            assert saved['CleanupNotes'] == 'Despawn after patrol' and saved['RollbackNotes'] == 'Restore previous population'
            assert next(e for e in saved['Entries'] if e['TargetRecordId']==actor_a)['MaximumCount'] == 4
            assert definition(untouched) == untouched_before and catalog()['ActorProfiles'] == actors_before
            checked(mode + '_save_persists_complete_plan_and_stable_composition_ids')
            clean = catalog_path.read_bytes(); close(accepted=True)
            assert catalog_path.read_bytes() == clean; reopen(floating, identity)
            checked(mode + '_saved_reopen_closes_cleanly_without_second_save')

            edit('encounterName', mode + ' name-only save'); close('Save', accepted=True); reopen(floating, identity)
            assert control('encounterName').text() == mode + ' name-only save'
            checked(mode + '_name_only_draft_saves_on_close')
            quantity(actor_a, 5, 2); edit('encounterCleanup', '  invalid range must retain raw notes  ')
            before, raw = catalog_path.read_bytes(), snapshot(); close('Save'); assert_unchanged(before, raw)
            assert control('encounterStatus').property('error')
            close('Cancel'); assert_unchanged(before, raw)
            quantity(actor_a, 5, 5); close('Save', accepted=True); reopen(floating, identity)
            checked(mode + '_invalid_quantity_keeps_pane_and_retries')

            control('encounterPopulationLimit').setValue(1)
            before, raw = catalog_path.read_bytes(), snapshot(); close('Save'); assert_unchanged(before, raw)
            control('encounterPopulationLimit').setValue(40); close('Save', accepted=True); reopen(floating, identity)
            checked(mode + '_invalid_population_limit_keeps_pane_and_retries')

            control('encounterConditions').setPlainText('  ')
            before, raw = catalog_path.read_bytes(), snapshot(); close('Save'); assert_unchanged(before, raw)
            control('encounterConditions').setPlainText('Player enters the gate'); close('Save', accepted=True); reopen(floating, identity)
            checked(mode + '_missing_activation_conditions_keep_raw_draft')

            remove(actor_c); remove(actor_a)
            before, raw = catalog_path.read_bytes(), snapshot(); close('Save'); assert_unchanged(before, raw)
            assert control('encounterEntries').rowCount() == 0
            close('Discard', accepted=True); reopen(floating, identity)
            assert catalog_path.read_bytes() == before and control('encounterEntries').rowCount() == 2
            checked(mode + '_empty_composition_failure_preserves_staged_removals')

            remove(actor_c); add(actor_b); quantity(actor_b, 2, 3)
            edit('encounterName', mode + ' locked-save retry')
            before, raw = catalog_path.read_bytes(), snapshot()
            handle = kernel.CreateFileW(str(catalog_path), 0x80000000, 1, None, 3, 0x80, None)
            assert handle not in (None, ctypes.c_void_p(-1).value)
            try:
                close('Save'); assert_unchanged(before, raw)
                assert control('encounterStatus').property('error')
            finally: kernel.CloseHandle(handle)
            close('Save', accepted=True); reopen(floating, identity)
            assert control('encounterName').text() == mode + ' locked-save retry'
            assert {e['TargetRecordId'] for e in definition(identity)['Entries']} == {actor_a, actor_b}
            assert definition(untouched) == untouched_before and catalog()['ActorProfiles'] == actors_before
            close(accepted=True)
            checked(mode + '_real_locked_write_keeps_pane_and_staging_then_retries')

        assert len(result['checks']) == 28, result['checks']
        result['status'] = 'PASSED'; stage('complete')
    except Exception:
        result['status'] = 'FAILED'; result['error'] = traceback.format_exc(); stage(result.get('stage','failed'))
    if result['status'] == 'PASSED':
        def quit_seen():
            result['about_to_quit'] = True
            output.write_text(json.dumps(result, indent=2), encoding='utf-8')
        app.aboutToQuit.connect(quit_seen); _keep.append(quit_seen)
        QtCore.QTimer.singleShot(0, general.exit)


def encounter_close_initialized(_args):
    _encounter_close_handler.disconnect()
    QtCore.QTimer.singleShot(0, run_encounter_close_smoke)


_encounter_close_handler = editor.EditorEventBusHandler()
_encounter_close_handler.connect()
_encounter_close_handler.add_callback('NotifyEditorInitialized', encounter_close_initialized)
Path(os.environ['FOA_SDK_ENCOUNTER_RESULT']).write_text(
    json.dumps({'status':'PARTIAL','stage':'waiting_for_editor_initialized'}),encoding='utf-8')
