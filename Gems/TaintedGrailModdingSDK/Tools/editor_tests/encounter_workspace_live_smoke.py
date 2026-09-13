# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native Spawn/Encounter workspace admission with synthetic roots and saved-byte checks."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import sys
import shutil
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


def run_encounter_workspace_smoke():
    output = Path(os.environ['FOA_SDK_ENCOUNTER_RESULT'])
    workspace = Path(os.environ['FOA_SDK_ENCOUNTER_WORKSPACE'])
    catalog_path = workspace.parent / 'Catalog/catalog.tgcatalog.json'
    result = {'status': 'PARTIAL', 'checks': [], 'transition_seconds': [],
              'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    root = None
    dock = None
    route = os.environ['FOA_SDK_ENCOUNTER_WORKSPACE_ROUTE']
    result['route'] = route
    result['prompt_orders'] = []
    settings = QtCore.QSettings('FOA-SDK', 'TaintedGrailModdingSDK')
    setting_key = 'CatalogBrowser/LastWorkspacePath'
    had_setting, old_setting = settings.contains(setting_key), settings.value(setting_key)
    active_path = workspace

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
        table = control('encounterEntries')
        fields = root.findChildren(QtWidgets.QWidget)
        _keep.extend(fields)
        for field in fields:
            # Removed table editors remain QObject children until deferred deletion.
            # Capture the current cell widgets below, not those retired editors.
            if table.isAncestorOf(field): continue
            if isinstance(field, QtWidgets.QLineEdit): value = field.text()
            elif isinstance(field, QtWidgets.QPlainTextEdit): value = field.toPlainText()
            elif isinstance(field, QtWidgets.QSpinBox): value = field.value()
            elif isinstance(field, QtWidgets.QCheckBox): value = field.isChecked()
            elif isinstance(field, QtWidgets.QComboBox): value = (field.currentData(), field.currentText())
            else: continue
            values.append((field.metaObject().className(), field.objectName(), value))
        table = control('encounterEntries')
        values.append(('entries', [(table.item(r, 0).data(QtCore.Qt.UserRole), table.item(r, 0).toolTip(),
                                    table.cellWidget(r, 2).value(), table.cellWidget(r, 3).value(),
                                    table.cellWidget(r, 2).findChild(QtWidgets.QLineEdit).text(),
                                    table.cellWidget(r, 3).findChild(QtWidgets.QLineEdit).text())
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
        nonlocal root, dock
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

    def assert_workspace(path):
        assert 'Workspace file: ' + path.as_posix() in details.toPlainText().replace(chr(92), '/'), details.toPlainText()
        assert isValid(root) and root.isVisible() and isValid(dock)

    def assert_reset():
        assert control('encounterRecords').currentIndex() == 0
        assert control('encounterName').text() == ''
        assert control('encounterPlacementSubject').text() == ''
        assert control('encounterConditions').toPlainText() == ''
        assert control('encounterCleanup').text() == '' and control('encounterRollback').text() == ''
        assert control('encounterEntries').rowCount() == 0
        assert not control('encounterSave').isEnabled()

    def open_saved_mod():
        # Loading a workspace does not select an authoring pack. Use the real
        # saved-mod action before editing; never inject an active pack in Core.
        choices = control('packSavedMods')
        index = choices.findData(saved_pack_path)
        assert index >= 0, (saved_pack_path, [choices.itemData(i) for i in range(choices.count())])
        choices.setCurrentIndex(index)
        button = next(b for b in pack.findChildren(QtWidgets.QPushButton) if b.text() == 'Open selected')
        assert button.isEnabled()
        button.click()
        assert control('packActiveSummary').text() != 'None selected', control('packStatus').text()

    def destination_bytes():
        return second_catalog.read_bytes() if second_catalog.exists() else None

    def clone_destination():
        # Copy only this runner's disposable synthetic sources; retain payload
        # hashes and rebind metadata locators to the second workspace root.
        assert second_root.resolve().is_relative_to(workspace.parent.parent.resolve())
        shutil.copytree(workspace.parent / 'Sources', second_root / 'Sources', dirs_exist_ok=True)
        for metadata in (second_root / 'Sources').rglob('*.tgsource.json'):
            data = json.loads(metadata.read_text(encoding='utf-8'))
            source = data.get('ClassData', data)['Source']
            locator = Path(source['Locator'])
            assert locator.resolve().is_relative_to(workspace.parent.resolve())
            source['Locator'] = (second_root / locator.relative_to(workspace.parent)).as_posix()
            metadata.write_text(json.dumps(data, indent=2), encoding='utf-8')
        cloned = catalog()
        assert cloned['WorkspaceId'] == second_data['WorkspaceId']
        for record in cloned['Records']:
            if record['RecordKind'] == 'encounter': record['DisplayName'] = 'Destination saved encounter'
        for encounter in cloned['EncounterDefinitions']:
            encounter['CleanupNotes'] = 'Destination cleanup'
        second_catalog.parent.mkdir(parents=True, exist_ok=True)
        second_catalog.write_text(json.dumps(cloned, indent=2), encoding='utf-8')

    def switch(path, choice=None, blocked=False, invalid=False, picker_cancel=False,
               pack_choice=None, on_prompt=None):
        nonlocal active_path
        before = snapshot()
        previous_path = active_path
        seen, errors = [], []
        started = time.monotonic()
        timer = QtCore.QTimer()
        _keep.append(timer)

        def answer():
            try:
                values = widgets()
                picker = next((w for w in values if isinstance(w, QtWidgets.QFileDialog) and w.isVisible()), None)
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
                    assert time.monotonic() - started < 4, picker.selectedFiles()
                    picker.findChild(QtWidgets.QDialogButtonBox).button(QtWidgets.QDialogButtonBox.Open).click()
                    return
                prompt = next((w for w in values if isinstance(w, QtWidgets.QMessageBox) and w.isVisible()), None)
                if prompt is None:
                    return
                kind = prompt.objectName()
                if kind in ('encounterUnsavedChangesDialog', 'packUnsavedChangesDialog'):
                    is_encounter = kind == 'encounterUnsavedChangesDialog'
                    seen.append('draft' if is_encounter else 'pack')
                    chosen = choice if is_encounter else pack_choice
                    assert chosen is not None, prompt.text()
                    buttons = QtWidgets.QMessageBox
                    assert prompt.standardButtons() == buttons.Save | buttons.Discard | buttons.Cancel
                    assert prompt.defaultButton() == prompt.button(buttons.Cancel)
                    assert prompt.escapeButton() == prompt.button(buttons.Cancel)
                    assert 'switching workspaces' in prompt.text(), prompt.text()
                    if is_encounter and chosen == 'Cancel':
                        assert prompt.grab().save(str(output.with_name('workspace-prompt.png')))
                    if is_encounter and on_prompt:
                        on_prompt()
                    if chosen == 'Escape':
                        QtTest.QTest.keyClick(prompt, QtCore.Qt.Key_Escape)
                    elif chosen == 'Close':
                        prompt.close()
                    else:
                        prompt.button(getattr(buttons, chosen)).click()
                else:
                    seen.append('error')
                    assert invalid, prompt.text()
                    prompt.accept()
            except Exception:
                errors.append(traceback.format_exc())
                for w in widgets():
                    if isinstance(w, QtWidgets.QDialog) and w.isVisible():
                        w.reject()

        timer.timeout.connect(answer)
        timer.start(25)
        try:
            open_button.click()
            QtTest.QTest.qWait(100)
        finally:
            timer.stop()
        elapsed = time.monotonic() - started
        result['transition_seconds'].append(round(elapsed, 4))
        result['prompt_orders'].append(seen)
        assert not errors, errors
        assert seen.count('picker') == 1, seen
        assert seen.count('draft') == int(choice is not None), seen
        assert seen.count('pack') == int(pack_choice is not None), seen
        assert seen.count('error') == int(invalid), seen
        assert elapsed < 5, 'Workspace interaction exceeded the five-second fixture budget'
        if blocked:
            assert_workspace(previous_path)
            if choice != 'Save':
                after = snapshot()
                assert after == before, ('Blocked replacement altered a retained draft',
                    len(before), len(after), [(a, b) for a, b in zip(before, after) if a != b])
        else:
            active_path = path
            assert_workspace(path)
            assert_reset()
            if path == workspace:
                open_saved_mod()
        return seen

    try:
        stage('fixture_started')
        assert route in ('status', 'catalog')
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        fixture = json.loads(workspace.read_text(encoding='utf-8-sig'))
        assert fixture['WorkspaceId'] == 'sdkqa.encounter-close'
        assert Path(fixture['RootPath']).resolve() == workspace.parent.resolve()
        assert workspace.resolve() == (Path(os.environ['LOCALAPPDATA']) / 'FOA-SDK/Workspace/foa-sdk.tgworkspace.json').resolve()
        assert not catalog_path.exists()
        second_root = workspace.parent.parent / 'WorkspaceB'
        assert not second_root.exists()
        second_data = json.loads(json.dumps(fixture).replace(workspace.parent.as_posix(), second_root.as_posix()))
        # Reuse workspace, profile and record IDs deliberately; roots differ.
        second_data['DisplayName'] = 'Synthetic second workspace'
        for leaf in ('Output', 'Staging', 'Deployment', 'Diagnostics', 'Extracted', 'Game/Managed', 'Game/BepInEx/plugins'):
            (second_root / leaf).mkdir(parents=True, exist_ok=True)
        for leaf in ('Game/Managed/Assembly-CSharp.dll', 'Game/UnityPlayer.dll'):
            (second_root / leaf).write_text('Synthetic marker only.', encoding='utf-8')
        second = second_root / 'second.tgworkspace.json'
        second.write_text(json.dumps(second_data, indent=2), encoding='utf-8')
        malformed = second_root / 'invalid.tgworkspace.json'
        malformed.write_text('{ malformed synthetic document', encoding='utf-8')
        second_catalog = second_root / 'Catalog/catalog.tgcatalog.json'
        alias = workspace.with_name('alias.tgworkspace.json')
        alias.write_text(json.dumps(fixture, indent=2), encoding='utf-8')
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
        general.open_pane('Tainted Grail SDK Status')
        QtTest.QTest.qWait(250)
        toggle = control('foundationAdvancedToggle')
        if toggle.text() == 'Show advanced details':
            toggle.click()
        details = next(w for w in widgets() if isinstance(w, QtWidgets.QPlainTextEdit)
                       and w.toPlainText().startswith('Workspace file:'))
        if route == 'catalog':
            settings.setValue(setting_key, str(workspace))
            general.open_pane('Tainted Grail Catalog Browser')
            QtTest.QTest.qWait(250)
        text = 'Open existing workspace...' if route == 'status' else 'Open Workspace...'
        open_button = next(w for w in widgets() if isinstance(w, QtWidgets.QPushButton) and w.text() == text)
        stage('opening_pack')
        pane_test.open_default_pack()
        QtTest.QTest.qWait(350)
        stage('saving_fixture_pack')
        control('packDisplayName').setText('Encounter workspace fixture')
        control('packOwner').setText('sdkqa')
        pack = control('TaintedGrailPackManager')
        next(b for b in pack.findChildren(QtWidgets.QPushButton) if b.text() == 'Save mod').click()
        assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        saved_pack_path = control('packSavedMods').currentData()
        assert saved_pack_path and Path(saved_pack_path).resolve().is_relative_to(workspace.parent.resolve())
        QtTest.QTest.qWait(250)
        pane_test.open_default_pane('Tainted Grail Actor and Troop Editor')
        deadline = time.monotonic() + 5
        while not control('populationTabs').isEnabled():
            assert time.monotonic() < deadline
            QtTest.QTest.qWait(25)
        actor_a, actor_b, actor_c = [create_actor(name) for name in ('Captain', 'Guard', 'Scout')]
        target_b = create_actor('Guard patrol', troop=True)
        actor_root = control('TaintedGrailModdingSDK.ActorTroopEditor').widget()
        pane_test.request_titlebar_close(actor_root, result, _keep); QtTest.QTest.qWait(250)
        assert not isValid(actor_root)
        actors_before, troops_before = catalog()['ActorProfiles'], catalog()['TroopProfiles']
        result['actors'], result['troop'] = [actor_a, actor_b, actor_c], target_b
        reopen()
        untouched = new_encounter('Untouched encounter', actor_c)
        untouched_before = definition(untouched)
        identity = new_encounter('Workspace patrol', actor_a)
        add(target_b); control('encounterPopulationLimit').setValue(40); save_direct()
        close(accepted=True)
        checked('synthetic_actor_troop_and_encounter_fixtures_ready')

        for floating, mode in ((False, 'docked'), (True, 'floating')):
            reopen(floating, identity)
            for target in [e['TargetRecordId'] for e in definition(identity)['Entries'] if e['TargetRecordId'] != actor_a]:
                remove(target)
            add(target_b); quantity(actor_a, 1, 1)
            control('encounterInstances').setValue(1); control('encounterPopulationLimit').setValue(40)
            choose('encounterActivation', 'manual'); save_direct()
            baseline = definition(identity)
            a_id = next(e['EntryId'] for e in baseline['Entries'] if e['TargetRecordId'] == actor_a)
            clone_destination()
            target_saved, original_saved = destination_bytes(), catalog_path.read_bytes()
            switch(second); switch(workspace)
            assert catalog_path.read_bytes() == original_saved and destination_bytes() == target_saved
            checked(mode + '_clean_round_trip_resets_forms_without_writes')

            choose('encounterRecords', identity)
            remove(target_b); add(actor_c); quantity(actor_a, 3, 4)
            edit('encounterName', '  ' + mode + ' retained patrol  ')
            edit('encounterPlacementSubject', '  North gate courtyard  ')
            edit('encounterCleanup', '  Retained cleanup notes  ')
            edit('encounterRollback', '  Retained rollback notes  ')
            choose('encounterActivation', 'all_conditions')
            control('encounterConditions').setPlainText(chr(10).join(['  Player enters gate  ', '', 'Quest is active']))
            control('encounterInstances').setValue(2)
            for choice in ('Cancel', 'Escape', 'Close'):
                switch(second, choice, blocked=True)
                assert catalog_path.read_bytes() == original_saved and destination_bytes() == target_saved
                checked(mode + '_' + choice.lower() + '_preserves_raw_fields_and_composition')
            switch(second, blocked=True, picker_cancel=True)
            checked(mode + '_picker_cancel_preserves_draft')
            switch(malformed, blocked=True, invalid=True)
            checked(mode + '_malformed_destination_preserves_draft_without_prompt')
            switch(workspace, 'Cancel', blocked=True)
            checked(mode + '_same_workspace_cancel_preserves_draft')
            def nested_close():
                event = QtGui.QCloseEvent(); app.sendEvent(root, event)
                assert not event.isAccepted() and root.isVisible()
            switch(second, 'Cancel', blocked=True, on_prompt=nested_close)
            checked(mode + '_nested_pane_close_cannot_bypass_workspace_prompt')
            switch(second, 'Discard')
            assert catalog_path.read_bytes() == original_saved and destination_bytes() == target_saved
            choose('encounterRecords', identity)
            assert control('encounterName').text() == 'Destination saved encounter'
            assert control('encounterCleanup').text() == 'Destination cleanup'
            switch(workspace); choose('encounterRecords', identity)
            assert definition(identity) == baseline and control('encounterEntries').rowCount() == 2
            checked(mode + '_discard_retires_only_on_commit_and_preserves_destination')

            remove(target_b); add(actor_c); quantity(actor_a, 3, 4)
            edit('encounterName', mode + ' saved before switch')
            edit('encounterPlacementSubject', 'North gate courtyard')
            edit('encounterCleanup', 'Cleanup saved before switch'); edit('encounterRollback', 'Restore previous population')
            choose('encounterActivation', 'all_conditions')
            control('encounterConditions').setPlainText(chr(10).join(['Player enters gate', 'Quest is active']))
            control('encounterInstances').setValue(2)
            ids = {control('encounterEntries').item(r, 0).toolTip(): control('encounterEntries').item(r, 0).data(QtCore.Qt.UserRole)
                   for r in range(control('encounterEntries').rowCount())}
            switch(second, 'Save')
            saved = definition(identity)
            assert row('Records', identity)['DisplayName'] == mode + ' saved before switch'
            assert {e['TargetRecordId']:e['EntryId'] for e in saved['Entries']} == ids and ids[actor_a] == a_id
            assert saved['PlacementSubjectRef'] == 'North gate courtyard' and saved['MaximumActiveInstances'] == 2
            assert saved['PopulationLimit'] == 40 and saved['ActivationMode'] == 'all_conditions'
            assert saved['Conditions'] == ['Player enters gate', 'Quest is active']
            assert saved['CleanupNotes'] == 'Cleanup saved before switch' and saved['RollbackNotes'] == 'Restore previous population'
            assert next(e for e in saved['Entries'] if e['TargetRecordId'] == actor_a)['MaximumCount'] == 4
            assert definition(untouched) == untouched_before and catalog()['ActorProfiles'] == actors_before
            assert catalog()['TroopProfiles'] == troops_before and destination_bytes() == target_saved
            switch(workspace)
            checked(mode + '_complete_encounter_saves_to_original_workspace')

            choose('encounterRecords', identity); quantity(actor_a, 5, 2)
            edit('encounterCleanup', '  invalid range retains raw notes  ')
            before, raw = catalog_path.read_bytes(), snapshot()
            switch(second, 'Save', blocked=True); assert_unchanged(before, raw)
            assert control('encounterStatus').property('error')
            switch(second, 'Cancel', blocked=True)
            quantity(actor_a, 5, 5); switch(second, 'Save'); switch(workspace)
            checked(mode + '_invalid_quantity_preserves_workspace_and_draft_until_retry')

            choose('encounterRecords', identity); control('encounterPopulationLimit').setValue(1)
            before, raw = catalog_path.read_bytes(), snapshot()
            switch(second, 'Save', blocked=True); assert_unchanged(before, raw)
            control('encounterPopulationLimit').setValue(40); switch(second, 'Save'); switch(workspace)
            checked(mode + '_invalid_population_limit_blocks_switch_and_retries')

            choose('encounterRecords', identity); control('encounterConditions').setPlainText('  ')
            before, raw = catalog_path.read_bytes(), snapshot()
            switch(second, 'Save', blocked=True); assert_unchanged(before, raw)
            control('encounterConditions').setPlainText('Player enters gate'); switch(second, 'Save'); switch(workspace)
            checked(mode + '_invalid_conditions_preserve_raw_draft')

            choose('encounterRecords', identity); remove(actor_a); remove(actor_c)
            before, raw = catalog_path.read_bytes(), snapshot()
            switch(second, 'Save', blocked=True); assert_unchanged(before, raw)
            switch(workspace, 'Discard')
            assert catalog_path.read_bytes() == before
            checked(mode + '_empty_composition_failure_preserves_staged_removals')

            choose('encounterRecords', identity); remove(actor_c); add(target_b); quantity(target_b, 2, 3)
            edit('encounterName', mode + ' locked-save retry')
            before, raw = catalog_path.read_bytes(), snapshot()
            handle = kernel.CreateFileW(str(catalog_path), 0x80000000, 1, None, 3, 0x80, None)
            assert handle not in (None, ctypes.c_void_p(-1).value)
            try:
                switch(second, 'Save', blocked=True); assert_unchanged(before, raw)
                assert control('encounterStatus').property('error')
            finally: kernel.CloseHandle(handle)
            switch(second, 'Save')
            assert row('Records', identity)['DisplayName'] == mode + ' locked-save retry'
            assert {e['TargetRecordId'] for e in definition(identity)['Entries']} == {actor_a, target_b}
            assert destination_bytes() == target_saved; switch(workspace)
            checked(mode + '_locked_write_preserves_original_and_destination_then_retries')

            choose('encounterRecords', identity); edit('encounterName', mode + ' reloaded latest')
            switch(workspace, 'Save'); choose('encounterRecords', identity)
            assert control('encounterName').text() == mode + ' reloaded latest'
            checked(mode + '_same_root_save_reloads_fresh_catalog')
            edit('encounterName', mode + ' alias reload')
            switch(alias, 'Save'); choose('encounterRecords', identity)
            assert control('encounterName').text() == mode + ' alias reload'
            switch(workspace)
            checked(mode + '_same_root_alias_save_reloads_fresh_catalog')
            choose('encounterRecords', identity); edit('encounterName', mode + ' discard reload')
            before = catalog_path.read_bytes(); switch(workspace, 'Discard')
            assert catalog_path.read_bytes() == before
            checked(mode + '_same_root_discard_resets_draft')

            # Reconnect after Pack; assert this pinned unordered bus actually
            # visits Encounter first before claiming a later-handler veto.
            close(accepted=True); reopen(floating, identity)
            edit('encounterName', mode + ' later veto')
            remove(target_b); add(actor_c); quantity(actor_a, 2, 3)
            control('packDisplayName').setText('Pack veto draft')
            before = catalog_path.read_bytes()
            order = switch(second, 'Discard', blocked=True, pack_choice='Cancel')
            assert order.index('draft') < order.index('pack'), order
            assert catalog_path.read_bytes() == before and control('packDisplayName').text() == 'Pack veto draft'
            checked(mode + '_discard_then_later_pack_veto_retains_draft')
            order = switch(second, 'Save', blocked=True, pack_choice='Cancel')
            assert order.index('draft') < order.index('pack'), order
            assert row('Records', identity)['DisplayName'] == mode + ' later veto'
            assert {e['TargetRecordId'] for e in definition(identity)['Entries']} == {actor_a, actor_c}
            assert not control('encounterSave').isEnabled() and control('packDisplayName').text() == 'Pack veto draft'
            switch(second, pack_choice='Discard'); switch(workspace)
            checked(mode + '_successful_save_survives_later_veto_and_clean_retry')

            choose('encounterRecords', identity); edit('encounterName', '  failed reload draft  ')
            edit('encounterCleanup', '  retain after candidate failure  ')
            saved = catalog_path.read_bytes()
            assert catalog_path.resolve().is_relative_to(workspace.parent.resolve())
            try:
                switch(workspace, 'Discard', blocked=True, invalid=True,
                       on_prompt=lambda: catalog_path.write_text('{ broken owned fixture', encoding='utf-8'))
            finally: catalog_path.write_bytes(saved)
            switch(workspace, 'Cancel', blocked=True); switch(workspace, 'Discard')
            checked(mode + '_post_admission_failure_retains_discard_until_successful_retry')

            choose('encounterRecords', identity); edit('encounterName', mode + ' no cross-root leak')
            before = catalog_path.read_bytes(); switch(second, 'Discard'); choose('encounterRecords', identity)
            assert control('encounterName').text() == 'Destination saved encounter'
            assert control('encounterCleanup').text() == 'Destination cleanup'
            assert destination_bytes() == target_saved and catalog_path.read_bytes() == before
            switch(workspace)
            checked(mode + '_matching_workspace_profile_and_record_ids_never_inherit_drafts')

            choose('encounterRecords', identity); edit('encounterName', mode + ' name-only save')
            switch(second, 'Save')
            assert row('Records', identity)['DisplayName'] == mode + ' name-only save'
            assert definition(untouched) == untouched_before and catalog()['ActorProfiles'] == actors_before
            assert catalog()['TroopProfiles'] == troops_before and destination_bytes() == target_saved
            switch(workspace); close(accepted=True)
            checked(mode + '_name_only_draft_saves_before_switch')

        assert len(result['checks']) == 47, result['checks']
        result['status'] = 'PASSED'; stage('complete')
    except Exception:
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
        try:
            if root is not None and isValid(root):
                result['failure_fields'] = snapshot()
                result['failure_messages'] = [w.text() for w in root.findChildren(QtWidgets.QLabel)
                                              if 'Error:' in w.text() or w.objectName() == 'encounterStatus']
        except Exception:
            result['diagnostic_error'] = traceback.format_exc()
        stage(result.get('stage', 'failed'))
    finally:
        current_setting = settings.value(setting_key)
        if current_setting and Path(str(current_setting)).resolve().is_relative_to(workspace.parent.parent.resolve()):
            if had_setting:
                settings.setValue(setting_key, old_setting)
            else:
                settings.remove(setting_key)
            settings.sync()
    if result['status'] == 'PASSED':
        def quit_seen():
            result['about_to_quit'] = True
            output.write_text(json.dumps(result, indent=2), encoding='utf-8')
        app.aboutToQuit.connect(quit_seen)
        _keep.append(quit_seen)
        QtCore.QTimer.singleShot(0, general.exit)


def encounter_workspace_initialized(_args):
    _encounter_workspace_handler.disconnect()
    QtCore.QTimer.singleShot(0, run_encounter_workspace_smoke)


_encounter_workspace_handler = editor.EditorEventBusHandler()
_encounter_workspace_handler.connect()
_encounter_workspace_handler.add_callback('NotifyEditorInitialized', encounter_workspace_initialized)
Path(os.environ['FOA_SDK_ENCOUNTER_RESULT']).write_text(
    json.dumps({'status': 'PARTIAL', 'stage': 'waiting_for_editor_initialized'}), encoding='utf-8')
