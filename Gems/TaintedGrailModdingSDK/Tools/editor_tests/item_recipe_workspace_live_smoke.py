# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Actual workspace picker/admission acceptance using synthetic external roots.

Run after Editor initialization without autotest_mode. No Foundation mock or
forced pane close answers admission. Read catalog bytes independently of UI copy.
"""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import time
import traceback

import azlmbr.editor as editor
import azlmbr.legacy.general as general
from PySide6 import QtCore, QtTest, QtWidgets
from shiboken6 import isValid

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pack_pane_test_support as pane_test

_keep = []


def run():
    output = Path(os.environ['FOA_SDK_ECONOMY_RESULT'])
    workspace = Path(os.environ['FOA_SDK_ECONOMY_WORKSPACE'])
    route = os.environ['FOA_SDK_ECONOMY_WORKSPACE_ROUTE']
    catalog_path = workspace.parent / 'Catalog/catalog.tgcatalog.json'
    result = {'status': 'PARTIAL', 'route': route, 'checks': [], 'transition_seconds': [],
              'prompt_orders': [], 'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    settings = QtCore.QSettings('FOA-SDK', 'TaintedGrailModdingSDK')
    setting_key = 'CatalogBrowser/LastWorkspacePath'
    had_setting, old_setting = settings.contains(setting_key), settings.value(setting_key)
    active_path = workspace
    identities = []

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

    def record(kind, identity):
        choice = control('economy' + kind + 'Choice')
        index = choice.findData(identity)
        assert index >= 0, identity
        choice.setCurrentIndex(index)

    def catalog():
        data = json.loads(catalog_path.read_text(encoding='utf-8'))
        return data.get('ClassData', data)

    def row(collection, identity):
        return next(v for v in catalog()[collection] if v['RecordId'] == identity)

    def form_values(name):
        values = {}
        for w in control(name).findChildren(QtWidgets.QWidget):
            if isinstance(w.parentWidget(), (QtWidgets.QComboBox, QtWidgets.QAbstractSpinBox)):
                continue
            if isinstance(w, QtWidgets.QLineEdit):
                value = w.text()
            elif isinstance(w, (QtWidgets.QSpinBox, QtWidgets.QDoubleSpinBox)):
                value = w.value()
            elif isinstance(w, QtWidgets.QCheckBox):
                value = w.isChecked()
            elif isinstance(w, QtWidgets.QComboBox):
                value = (w.currentData(), w.currentText())
            else:
                continue
            values[w.objectName()] = value
        return values

    def snapshot():
        old_item = control('economyItemChoice').currentData()
        old_recipe = control('economyRecipeChoice').currentData()
        values = {}
        if active_path == workspace and identities:
            for identity in identities[:2]:
                record('Item', identity)
                values[identity] = form_values('economyItemProfile')
            for identity in identities[2:]:
                record('Recipe', identity)
                values[identity] = [form_values(n) for n in
                                    ('economyRecipeSettings', 'economyIngredientForm', 'economyOutputForm')]
            record('Item', old_item)
            record('Recipe', old_recipe)
        values['acquisition'] = form_values('economyRelationshipForm')
        return values

    def assert_workspace(path):
        assert 'Workspace file: ' + path.as_posix() in details.toPlainText().replace('\\', '/'), details.toPlainText()
        assert isValid(root) and root.isVisible() and isValid(dock)

    def assert_reset():
        assert control('economyItemChoice').currentIndex() == 0
        assert control('economyRecipeChoice').currentIndex() == 0
        assert control('economyItemWeight').value() == 0
        assert control('economyRecipeType').text() == ''
        assert control('economyIngredientQuantity').value() == 1
        assert control('economyOutputQuantity').value() == 1
        for name in ('economyRelationshipId', 'economyRelationshipSubject', 'economyRelationshipEvidence'):
            assert control(name).text() == ''
        assert control('economyRelationshipSource').currentIndex() == 0

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
                if kind in ('economyUnsavedChangesDialog', 'packUnsavedChangesDialog'):
                    is_item = kind == 'economyUnsavedChangesDialog'
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
        checked(('blocked_' if blocked else 'switched_') +
                (choice or ('invalid' if invalid else 'picker_cancel' if picker_cancel else 'clean')))
        return seen

    def create(kind, name):
        seen = []
        timer = QtCore.QTimer()
        def answer():
            prompt = next((w for w in widgets() if isinstance(w, QtWidgets.QInputDialog) and w.isVisible()), None)
            if prompt:
                timer.stop()
                seen.append(name)
                prompt.setTextValue(name)
                prompt.accept()
        timer.timeout.connect(answer)
        timer.start(25)
        control('economyNew' + kind).click()
        timer.stop()
        assert seen == [name]
        identity = control('economy' + kind + 'Choice').currentData()
        assert identity, control('economyStatus').text()
        return identity

    def edit_all():
        for identity, weight in zip(identities[:2], (11, 22)):
            record('Item', identity)
            control('economyItemWeight').setValue(weight)
        for identity, quantity in zip(identities[2:], (3, 4)):
            record('Recipe', identity)
            control('economyRecipeType').setText('Saved before switch')
            record('Ingredient', identities[0])
            control('economyIngredientQuantity').setValue(quantity)
            record('Output', identities[1])
            control('economyOutputQuantity').setValue(quantity + 1)
        source = control('economyRelationshipSource')
        source.setCurrentIndex(source.findData(identities[0]))
        control('economyRelationshipId').setText('sdkqa.docked')
        control('economyRelationshipSubject').setText('synthetic:vendor')
        control('economyRelationshipEvidence').setText('sdkqa.evidence.docked')

    try:
        stage('fixture')
        assert route in ('status', 'catalog')
        assert not any('autotest_mode' in a for a in app.arguments())
        fixture = json.loads(workspace.read_text(encoding='utf-8'))
        assert fixture['WorkspaceId'] == 'sdkqa.item-recipe-close'
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
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
        kernel.GetModuleHandleW.restype = ctypes.c_void_p
        kernel.GetModuleFileNameW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint32]
        buffer = ctypes.create_unicode_buffer(32768)
        module = kernel.GetModuleHandleW('TaintedGrailModdingSDK.Editor.dll')
        assert module and kernel.GetModuleFileNameW(module, buffer, len(buffer))
        result['sdk_module_path'] = buffer.value
        result['sdk_module_sha256'] = hashlib.sha256(Path(buffer.value).read_bytes()).hexdigest()
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        general.idle_enable(True)
        QtTest.QTest.qWait(3000)
        general.open_pane('Tainted Grail SDK Status')
        QtTest.QTest.qWait(250)
        toggle = control('foundationAdvancedToggle')
        if toggle.text() == 'Show advanced details':
            toggle.click()
        details = next(w for w in widgets() if isinstance(w, QtWidgets.QPlainTextEdit)
                       and w.toPlainText().startswith('Workspace file:'))
        pane_test.open_default_pack()
        QtTest.QTest.qWait(250)
        pack = control('TaintedGrailPackManager')
        control('packDisplayName').setText('Workspace fixture')
        control('packOwner').setText('sdkqa')
        next(b for b in pack.findChildren(QtWidgets.QPushButton) if b.text() == 'Save mod').click()
        assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        if route == 'catalog':
            settings.setValue(setting_key, str(workspace))
            general.open_pane('Tainted Grail Catalog Browser')
            QtTest.QTest.qWait(250)
        pane_test.open_default_pane('Tainted Grail Item and Recipe Editor')
        QtTest.QTest.qWait(250)
        dock = control('TaintedGrailModdingSDK.ItemRecipeEditor')
        root = dock.widget()
        _keep.extend((pack, dock, root))
        control('economyRecipeSettings').setChecked(True)
        text = 'Open existing workspace...' if route == 'status' else 'Open Workspace...'
        open_button = next(w for w in widgets() if isinstance(w, QtWidgets.QPushButton) and w.text() == text)
        identities = [create('Item', 'Item A'), create('Item', 'Item B'),
                      create('Recipe', 'Recipe A'), create('Recipe', 'Recipe B')]
        result['identities'] = identities
        initial = catalog_path.read_bytes()
        switch(second)
        assert not (second_root / 'Catalog/catalog.tgcatalog.json').exists()
        switch(workspace)
        assert catalog_path.read_bytes() == initial
        checked('clean_switch_preserves_pane_and_resets_all_forms')

        stage('all_retained_drafts')
        edit_all()
        for choice in ('Cancel', 'Escape', 'Close'):
            switch(second, choice, blocked=True)
            assert catalog_path.read_bytes() == initial
        switch(second, blocked=True, picker_cancel=True)
        switch(malformed, blocked=True, invalid=True)
        switch(workspace, 'Cancel', blocked=True)
        switch(second, 'Discard')
        assert catalog_path.read_bytes() == initial
        assert control('economyItemChoice').count() == 1
        assert control('economyRecipeChoice').count() == 1
        switch(workspace)
        for identity in identities[:2]:
            record('Item', identity)
            assert control('economyItemWeight').value() == 0
        checked('discard_retired_all_nine_drafts_only_after_replacement')

        edit_all()
        switch(second, 'Save')
        assert not (second_root / 'Catalog/catalog.tgcatalog.json').exists()
        for identity, weight in zip(identities[:2], (11, 22)):
            assert row('EconomyItems', identity)['Weight'] == weight
        for identity, quantity in zip(identities[2:], (3, 4)):
            assert row('EconomyRecipes', identity)['RecipeType'] == 'Saved before switch'
            assert any(v['RecipeRecordId'] == identity and v['Quantity'] == quantity for v in catalog()['RecipeIngredients'])
            assert any(v['RecipeRecordId'] == identity and v['Quantity'] == quantity + 1 for v in catalog()['RecipeOutputs'])
        assert any(v['RelationshipId'] == 'sdkqa.docked' for v in catalog()['Relationships'])
        checked('all_nine_drafts_saved_to_original_root_without_cross_workspace_writes')
        switch(workspace)

        stage('failed_save_and_retry')
        record('Item', identities[0])
        record('Recipe', identities[2])
        evidence = control('economyRecipeEvidence').text()
        control('economyItemWeight').setValue(42)
        control('economyRecipeType').setText('Retried save')
        control('economyRecipeEvidence').setText('missing.evidence')
        switch(second, 'Save', blocked=True)
        assert row('EconomyItems', identities[0])['Weight'] == 42
        assert row('EconomyRecipes', identities[2])['RecipeType'] == 'Saved before switch'
        assert control('economyRecipeEvidence').text() == 'missing.evidence'
        assert control('economyRecipeType').text() == 'Retried save'
        switch(second, 'Cancel', blocked=True)
        control('economyRecipeEvidence').setText(evidence)
        saved = catalog_path.read_bytes()
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                      ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.CreateFileW(str(catalog_path), 0x80000000, 1, None, 3, 0x80, None)
        assert handle not in (None, ctypes.c_void_p(-1).value)
        try:
            switch(second, 'Save', blocked=True)
            assert catalog_path.read_bytes() == saved
            assert control('economyRecipeType').text() == 'Retried save'
        finally:
            kernel.CloseHandle(handle)
        switch(second, 'Save')
        assert row('EconomyRecipes', identities[2])['RecipeType'] == 'Retried save'
        checked('partial_save_failed_validation_and_real_write_failure_preserve_old_workspace_and_retry')
        switch(workspace)

        stage('same_root_reload')
        record('Item', identities[1])
        record('Recipe', identities[2])
        control('economyItemWeight').setValue(55)
        control('economyRecipeType').setText('Reloaded latest')
        record('Ingredient', identities[0])
        control('economyIngredientQuantity').setValue(7)
        switch(workspace, 'Save')
        record('Item', identities[1])
        record('Recipe', identities[2])
        assert control('economyItemWeight').value() == 55
        assert control('economyRecipeType').text() == 'Reloaded latest'
        table = control('economyIngredients')
        assert any(table.item(r, 2).text() == '7' for r in range(table.rowCount()))
        assert row('EconomyItems', identities[1])['Weight'] == 55
        checked('same_root_save_reloads_latest_catalog_and_new_join_evidence')
        switch(workspace)
        record('Item', identities[1])
        control('economyItemWeight').setValue(99)
        control('economyRelationshipSubject').setText('Uncommitted acquisition')
        saved = catalog_path.read_bytes()
        switch(workspace, 'Discard')
        assert catalog_path.read_bytes() == saved
        record('Item', identities[1])
        assert control('economyItemWeight').value() == 55
        checked('same_root_discard_resets_cached_drafts_and_acquisition_fields')

        stage('other_admission_handler')
        # Saved layouts can create panes before this script. Reconnect the clean
        # Item pane after Pack so the pinned unordered bus visits Item first.
        pane_test.request_titlebar_close(root, result, _keep)
        QtTest.QTest.qWait(250)
        assert not isValid(root)
        pane_test.open_default_pane('Tainted Grail Item and Recipe Editor')
        QtTest.QTest.qWait(250)
        dock = control('TaintedGrailModdingSDK.ItemRecipeEditor')
        root = dock.widget()
        _keep.extend((dock, root))
        control('economyRecipeSettings').setChecked(True)
        record('Item', identities[1])
        control('economyItemWeight').setValue(66)
        control('packDisplayName').setText('Pack veto draft')
        order = switch(second, 'Discard', blocked=True, pack_choice='Cancel')
        assert order.index('draft') < order.index('pack'), order
        assert control('economyItemWeight').value() == 66
        assert control('packDisplayName').text() == 'Pack veto draft'
        assert catalog_path.read_bytes() == saved
        checked('item_discard_followed_by_pack_veto_keeps_every_draft')
        switch(second, 'Discard', pack_choice='Discard')
        assert catalog_path.read_bytes() == saved
        switch(workspace)

        stage('failed_post_admission_reload')
        record('Item', identities[0])
        control('economyItemWeight').setValue(77)
        saved = catalog_path.read_bytes()
        assert catalog_path.resolve().is_relative_to(workspace.parent.resolve())
        try:
            switch(workspace, 'Discard', blocked=True, invalid=True,
                   on_prompt=lambda: catalog_path.write_text('{ broken owned fixture', encoding='utf-8'))
            assert control('economyItemWeight').value() == 77
        finally:
            catalog_path.write_bytes(saved)
        switch(workspace, 'Cancel', blocked=True)
        switch(workspace, 'Discard')
        checked('failed_post_admission_load_keeps_discarded_draft_until_successful_retry')

        stage('same_ids_in_other_root')
        # Clone only this runner's synthetic sources and seed different saved
        # values for the same IDs in B. Source payload bytes/hashes stay intact.
        assert second_root.resolve().is_relative_to(workspace.parent.parent.resolve())
        shutil.copytree(workspace.parent / 'Sources', second_root / 'Sources', dirs_exist_ok=True)
        for metadata in (second_root / 'Sources').rglob('*.tgsource.json'):
            data = json.loads(metadata.read_text(encoding='utf-8'))
            source = data.get('ClassData', data)['Source']
            locator = Path(source['Locator'])
            assert locator.resolve().is_relative_to(workspace.parent.resolve())
            source['Locator'] = (second_root / locator.relative_to(workspace.parent)).as_posix()
            metadata.write_text(json.dumps(data, indent=2), encoding='utf-8')
        second_catalog = second_root / 'Catalog/catalog.tgcatalog.json'
        second_catalog.parent.mkdir(exist_ok=True)
        cloned = catalog()
        cloned['WorkspaceId'] = second_data['WorkspaceId']
        for item in cloned['EconomyItems']:
            item['Weight'] = 500
        second_catalog.write_text(json.dumps(cloned, indent=2), encoding='utf-8')
        second_saved = second_catalog.read_bytes()
        original_weight = row('EconomyItems', identities[0])['Weight']
        record('Item', identities[0])
        control('economyItemWeight').setValue(88)
        switch(second, 'Discard')
        record('Item', identities[0])
        assert control('economyItemWeight').value() == 500
        assert second_catalog.read_bytes() == second_saved
        switch(workspace)
        record('Item', identities[0])
        assert control('economyItemWeight').value() == original_weight
        checked('matching_record_ids_across_roots_do_not_inherit_discarded_drafts')

        result['status'] = 'PASSED'
        stage('complete')
    except Exception:
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
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


def initialized(_args):
    _handler.disconnect()
    QtCore.QTimer.singleShot(0, run)


_handler = editor.EditorEventBusHandler()
_handler.connect()
_handler.add_callback('NotifyEditorInitialized', initialized)
