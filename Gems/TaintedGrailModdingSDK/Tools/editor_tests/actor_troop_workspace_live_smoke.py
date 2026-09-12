# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native Actor/Troop workspace admission with synthetic roots and saved-byte checks."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import sys
import shutil
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


def run_population_workspace_smoke():
    output = Path(os.environ['FOA_SDK_POPULATION_RESULT'])
    workspace = Path(os.environ['FOA_SDK_POPULATION_WORKSPACE'])
    catalog_path = workspace.parent / 'Catalog/catalog.tgcatalog.json'
    result = {'status': 'PARTIAL', 'checks': [], 'transition_seconds': [],
              'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    root = None
    route = os.environ['FOA_SDK_POPULATION_WORKSPACE_ROUTE']
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

    def assert_workspace(path):
        assert 'Workspace file: ' + path.as_posix() in details.toPlainText().replace('\\', '/'), details.toPlainText()
        assert isValid(root) and root.isVisible() and isValid(dock)

    def assert_reset():
        assert control('populationActor').currentIndex() == 0
        assert control('populationTroop').currentIndex() == 0
        assert control('populationArchetype').text() == ''
        assert control('populationMinimumLevel').value() == 0
        assert control('populationMaximumSize').value() == 1
        assert control('populationMembers').rowCount() == 0
        assert control('populationMemberWeight').text() == '1'
        assert control('populationMemberActor').currentIndex() == 0

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

    def select_drafts(actor, troop):
        choose('populationActor', actor)
        choose('populationTroop', troop)
        select_member(troop, actor)

    def destination_bytes():
        return second_catalog.read_bytes() if second_catalog.exists() else None

    def clone_destination():
        # These are only this runner's synthetic authoring sources. Preserve
        # source payload hashes while rebinding metadata locators to B.
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
        cloned['WorkspaceId'] = second_data['WorkspaceId']
        for actor in cloned['ActorProfiles']:
            actor['Archetype'] = 'Destination saved actor'
        second_catalog.parent.mkdir(exist_ok=True)
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
                if kind in ('populationUnsavedChangesDialog', 'packUnsavedChangesDialog'):
                    is_item = kind == 'populationUnsavedChangesDialog'
                    seen.append('draft' if is_item else 'pack')
                    chosen = choice if is_item else pack_choice
                    assert chosen is not None, prompt.text()
                    buttons = QtWidgets.QMessageBox
                    assert prompt.standardButtons() == buttons.Save | buttons.Discard | buttons.Cancel
                    assert prompt.defaultButton() == prompt.button(buttons.Cancel)
                    assert prompt.escapeButton() == prompt.button(buttons.Cancel)
                    assert 'switching workspaces' in prompt.text(), prompt.text()
                    if is_item and chosen == 'Cancel':
                        assert prompt.grab().save(str(output.with_name('workspace-prompt.png')))
                    if is_item and on_prompt:
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
                assert snapshot() == before, 'Blocked replacement altered a retained draft'
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
        assert fixture['WorkspaceId'] == 'sdkqa.actor-troop-close'
        assert Path(fixture['RootPath']).resolve() == workspace.parent.resolve()
        assert workspace.resolve() == (Path(os.environ['LOCALAPPDATA']) / 'FOA-SDK/Workspace/foa-sdk.tgworkspace.json').resolve()
        assert not catalog_path.exists()
        second_root = workspace.parent.parent / 'WorkspaceB'
        assert not second_root.exists()
        second_data = json.loads(json.dumps(fixture).replace(workspace.parent.as_posix(), second_root.as_posix()))
        second_data['WorkspaceId'] += '-b'
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
        control('packDisplayName').setText('Population close fixture')
        control('packOwner').setText('sdkqa')
        pack = control('TaintedGrailPackManager')
        next(b for b in pack.findChildren(QtWidgets.QPushButton) if b.text() == 'Save mod').click()
        assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        saved_pack_path = control('packSavedMods').currentData()
        assert saved_pack_path and Path(saved_pack_path).resolve().is_relative_to(workspace.parent.resolve())
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
            dock = pane_test.pane_dock(root)
            troop = create(mode + ' workspace troop', troop=True)
            clone_destination()
            target_saved = destination_bytes()
            original_saved = catalog_path.read_bytes()
            switch(second)
            switch(workspace)
            assert catalog_path.read_bytes() == original_saved and destination_bytes() == target_saved
            checked(mode + '_clean_round_trip_resets_forms_without_writes')

            select_drafts(actor_a, troop)
            edit('populationArchetype', mode + ' retained actor')
            control('populationMaximumSize').setValue(3)
            edit('populationMemberWeight', '  2.7500  ')
            for choice in ('Cancel', 'Escape', 'Close'):
                switch(second, choice, blocked=True)
                assert catalog_path.read_bytes() == original_saved and destination_bytes() == target_saved
                checked(mode + '_' + choice.lower() + '_preserves_actor_troop_and_member')
            switch(second, blocked=True, picker_cancel=True)
            checked(mode + '_picker_cancel_preserves_drafts')
            switch(malformed, blocked=True, invalid=True)
            checked(mode + '_malformed_destination_preserves_drafts_without_prompt')
            switch(workspace, 'Cancel', blocked=True)
            checked(mode + '_same_workspace_cancel_preserves_drafts')
            def nested_close():
                event = QtGui.QCloseEvent()
                app.sendEvent(root, event)
                assert not event.isAccepted() and root.isVisible()
            switch(second, 'Cancel', blocked=True, on_prompt=nested_close)
            checked(mode + '_nested_pane_close_cannot_bypass_workspace_prompt')
            switch(second, 'Discard')
            assert catalog_path.read_bytes() == original_saved and destination_bytes() == target_saved
            choose('populationActor', actor_a)
            assert control('populationArchetype').text() == 'Destination saved actor'
            switch(workspace)
            checked(mode + '_discard_retires_drafts_only_after_commit_and_preserves_destination')

            select_drafts(actor_a, troop)
            edit('populationArchetype', mode + ' saved before switch')
            control('populationMaximumSize').setValue(3)
            new_member(actor_b)
            switch(second, 'Save')
            assert row('ActorProfiles', actor_a)['Archetype'] == mode + ' saved before switch'
            assert row('TroopProfiles', troop)['MaximumSize'] == 3
            assert {m['ActorRecordId'] for m in members(troop)} == {actor_a, actor_b}
            assert destination_bytes() == target_saved
            switch(workspace)
            checked(mode + '_all_drafts_saved_to_original_workspace')

            select_drafts(actor_a, troop)
            control('populationMinimumLevel').setValue(10)
            control('populationMaximumLevel').setValue(2)
            edit('populationMemberWeight', '  3.7500  ')
            before, original = catalog_path.read_bytes(), snapshot()
            switch(second, 'Save', blocked=True)
            assert catalog_path.read_bytes() == before and snapshot() == original
            switch(workspace, 'Discard')
            checked(mode + '_invalid_actor_stops_all_saves_and_keeps_workspace')

            select_drafts(actor_a, troop)
            edit('populationArchetype', mode + ' partial actor')
            control('populationMaximumSize').setValue(4)
            edit('populationMemberWeight', 'invalid weight')
            old_troop = row('TroopProfiles', troop)
            switch(second, 'Save', blocked=True)
            assert row('ActorProfiles', actor_a)['Archetype'] == mode + ' partial actor'
            assert row('TroopProfiles', troop) == old_troop
            assert control('populationMemberWeight').text() == 'invalid weight'
            assert control('populationMaximumSize').value() == 4
            switch(second, 'Cancel', blocked=True)
            edit('populationMemberWeight', '3.25')
            switch(second, 'Save')
            assert member(troop, actor_a)['Weight'] == 3.25 and destination_bytes() == target_saved
            switch(workspace)
            checked(mode + '_partial_actor_save_member_failure_cancel_and_retry')

            for actor_dirty in (True, False):
                choose('populationTroop', troop)
                select_member(troop, actor_a)
                if actor_dirty:
                    choose('populationActor', actor_a)
                    edit('populationArchetype', mode + ' lock retry')
                weight = 5.5 if actor_dirty else 6.5
                edit('populationMemberWeight', str(weight))
                before = catalog_path.read_bytes()
                handle = kernel.CreateFileW(str(catalog_path), 0x80000000, 1, None, 3, 0x80, None)
                assert handle not in (None, ctypes.c_void_p(-1).value)
                try:
                    switch(second, 'Save', blocked=True)
                    assert catalog_path.read_bytes() == before
                    assert control('populationMemberWeight').text() == str(weight)
                    if actor_dirty:
                        assert control('populationArchetype').text() == mode + ' lock retry'
                finally:
                    kernel.CloseHandle(handle)
                switch(second, 'Save')
                assert member(troop, actor_a)['Weight'] == weight and destination_bytes() == target_saved
                switch(workspace)
                checked(mode + '_locked_' + ('actor' if actor_dirty else 'troop') + '_save_keeps_workspace_and_retries')

            select_drafts(actor_a, troop)
            edit('populationArchetype', mode + ' reloaded latest')
            edit('populationMemberWeight', '7.25')
            switch(workspace, 'Save')
            select_drafts(actor_a, troop)
            assert control('populationArchetype').text() == mode + ' reloaded latest'
            assert control('populationMemberWeight').text() == '7.25'
            checked(mode + '_same_root_save_reloads_latest_catalog')
            before = catalog_path.read_bytes()
            edit('populationArchetype', mode + ' discard reload')
            edit('populationMemberWeight', '8.25')
            switch(workspace, 'Discard')
            assert catalog_path.read_bytes() == before
            checked(mode + '_same_root_discard_resets_all_forms')

            choose('populationTroop', troop)
            select_member(troop, actor_b)
            next(b for b in root.findChildren(QtWidgets.QPushButton) if b.text() == 'Remove selected member').click()
            new_member(actor_c)
            control('populationStageMember').click()
            select_member(troop, actor_a)
            edit('populationMemberWeight', '9.25')
            control('populationStageMember').click()
            before = catalog_path.read_bytes()
            switch(second, 'Cancel', blocked=True)
            assert catalog_path.read_bytes() == before
            switch(second, 'Save')
            assert {m['ActorRecordId'] for m in members(troop)} == {actor_a, actor_c}
            assert member(troop, actor_a)['Weight'] == 9.25 and destination_bytes() == target_saved
            switch(workspace)
            checked(mode + '_staged_add_edit_and_remove_save_atomically_before_switch')

            # Reconnect the clean Actor pane after Pack so this pinned unordered
            # bus visits Actor first; assert the actual order, never assume it.
            close(accepted=True)
            reopen(floating)
            dock = pane_test.pane_dock(root)
            select_drafts(actor_a, troop)
            edit('populationArchetype', mode + ' later veto')
            edit('populationMemberWeight', '10.25')
            control('packDisplayName').setText('Pack veto draft')
            before = catalog_path.read_bytes()
            order = switch(second, 'Discard', blocked=True, pack_choice='Cancel')
            assert order.index('draft') < order.index('pack'), order
            assert catalog_path.read_bytes() == before
            assert control('packDisplayName').text() == 'Pack veto draft'
            checked(mode + '_discard_then_later_pack_veto_keeps_all_drafts')
            order = switch(second, 'Save', blocked=True, pack_choice='Cancel')
            assert order.index('draft') < order.index('pack'), order
            assert row('ActorProfiles', actor_a)['Archetype'] == mode + ' later veto'
            assert member(troop, actor_a)['Weight'] == 10.25
            assert control('packDisplayName').text() == 'Pack veto draft'
            switch(second, pack_choice='Discard')
            switch(workspace)
            checked(mode + '_successful_save_survives_later_veto_and_clean_retry')

            select_drafts(actor_a, troop)
            edit('populationArchetype', mode + ' failed reload draft')
            edit('populationMemberWeight', ' 11.2500 ')
            saved = catalog_path.read_bytes()
            assert catalog_path.resolve().is_relative_to(workspace.parent.resolve())
            try:
                switch(workspace, 'Discard', blocked=True, invalid=True,
                       on_prompt=lambda: catalog_path.write_text('{ broken owned fixture', encoding='utf-8'))
            finally:
                catalog_path.write_bytes(saved)
            switch(workspace, 'Cancel', blocked=True)
            switch(workspace, 'Discard')
            checked(mode + '_post_admission_failure_retains_discard_until_successful_retry')

            choose('populationActor', actor_a)
            edit('populationArchetype', mode + ' no cross root leak')
            before = catalog_path.read_bytes()
            switch(second, 'Discard')
            choose('populationActor', actor_a)
            assert control('populationArchetype').text() == 'Destination saved actor'
            assert destination_bytes() == target_saved and catalog_path.read_bytes() == before
            switch(workspace)
            checked(mode + '_matching_ids_in_other_root_never_inherit_drafts')

            choose('populationTroop', troop)
            select_member(troop, actor_a)
            edit('populationMemberWeight', '12.25')
            switch(second, 'Save')
            assert member(troop, actor_a)['Weight'] == 12.25 and destination_bytes() == target_saved
            switch(workspace)
            checked(mode + '_member_only_draft_admits_only_after_save')
            close(accepted=True)

        assert len(result['checks']) == 45, result['checks']
        result['status'] = 'PASSED'
        stage('complete')
    except Exception:
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
        try:
            if root is not None and isValid(root):
                result['failure_fields'] = snapshot()
                result['failure_messages'] = [w.text() for w in root.findChildren(QtWidgets.QLabel)
                                              if 'Error:' in w.text() or w.objectName() == 'populationStatus']
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


def population_workspace_initialized(_args):
    _population_workspace_handler.disconnect()
    QtCore.QTimer.singleShot(0, run_population_workspace_smoke)


_population_workspace_handler = editor.EditorEventBusHandler()
_population_workspace_handler.connect()
_population_workspace_handler.add_callback('NotifyEditorInitialized', population_workspace_initialized)
Path(os.environ['FOA_SDK_POPULATION_RESULT']).write_text(
    json.dumps({'status': 'PARTIAL', 'stage': 'waiting_for_editor_initialized'}), encoding='utf-8')
