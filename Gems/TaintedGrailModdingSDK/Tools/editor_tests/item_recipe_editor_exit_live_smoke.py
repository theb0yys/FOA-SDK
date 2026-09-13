# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Actual whole-Editor exit and rollback acceptance; synthetic data only."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
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
    case = os.environ['FOA_SDK_ECONOMY_WORKSPACE_ROUTE'].removeprefix('exit-')
    catalog_path = workspace.parent / 'Catalog/catalog.tgcatalog.json'
    result = {'status': 'PARTIAL', 'case': case, 'checks': [], 'transition_seconds': [],
              'prompt_orders': [], 'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
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

    def attempt(choice, route, accepted=False, pack_choice=None):
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
                assert name in ('economyUnsavedChangesDialog', 'packUnsavedChangesDialog'), prompt.text()
                is_item = name == 'economyUnsavedChangesDialog'
                seen.append('item' if is_item else 'pack')
                selected = choice if is_item else pack_choice
                assert selected is not None, (name, prompt.text())
                buttons = QtWidgets.QMessageBox
                assert prompt.standardButtons() == buttons.Save | buttons.Discard | buttons.Cancel
                assert prompt.defaultButton() == prompt.button(buttons.Cancel)
                assert prompt.escapeButton() == prompt.button(buttons.Cancel)
                if is_item and route != 'python':
                    assert 'exiting the Editor' in prompt.text(), prompt.text()
                # Legacy Python exit calls closeAllWindows and may ask a floating
                # pane directly before the main window. Its cancellation must
                # still preserve drafts through the existing pane-close guard.
                if is_item and selected == 'Cancel':
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
        assert seen.count('item') == int(choice is not None), seen
        assert seen.count('pack') == int(pack_choice is not None), seen
        assert elapsed < 5, 'Exit exceeded five-second synthetic fixture budget'
        if accepted:
            assert not isValid(root), 'Accepted exit did not destroy the pane'
        else:
            assert isValid(main) and main.isVisible(), 'Cancelled exit closed the Editor'
            current = control('TaintedGrailModdingSDK.ItemRecipeEditor').widget()
            assert current.isVisible(), 'Cancelled exit lost the Item/Recipe pane'
            root = current
            _keep.append(root)
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
        stage('fixture')
        assert case in ('save', 'discard', 'clean', 'rollback')
        assert not any('autotest_mode' in a for a in app.arguments())
        fixture = json.loads(workspace.read_text(encoding='utf-8'))
        assert fixture['WorkspaceId'] == 'sdkqa.item-recipe-close'
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
        pane_test.open_default_pane('Tainted Grail Item and Recipe Editor')
        QtTest.QTest.qWait(250)
        dock = control('TaintedGrailModdingSDK.ItemRecipeEditor')
        root = dock.widget()
        _keep.extend((pack, dock, root))
        # A saved layout can construct a floating pane before this script starts.
        # Recreate the clean pane so UseDefaultState applies to a new instance.
        pane_test.request_titlebar_close(root, result, _keep)
        QtTest.QTest.qWait(250)
        assert not isValid(root)
        pane_test.open_default_pane('Tainted Grail Item and Recipe Editor')
        QtTest.QTest.qWait(250)
        dock = control('TaintedGrailModdingSDK.ItemRecipeEditor')
        root = dock.widget()
        _keep.extend((dock, root))
        assert pane_test.floating_container(root) is None
        control('economyRecipeSettings').setChecked(True)
        main = control('MainWindow')
        identities = [create('Item', 'Item A'), create('Item', 'Item B'),
                      create('Recipe', 'Recipe A'), create('Recipe', 'Recipe B')]
        result['identities'] = identities
        initial = catalog_path.read_bytes()

        def unchanged():
            assert catalog_path.read_bytes() == initial

        def all_saved():
            for identity, weight in zip(identities[:2], (11, 22)):
                assert row('EconomyItems', identity)['Weight'] == weight
            for identity, quantity in zip(identities[2:], (3, 4)):
                assert row('EconomyRecipes', identity)['RecipeType'] == 'Saved before switch'
                assert any(v['RecipeRecordId'] == identity and v['Quantity'] == quantity for v in catalog()['RecipeIngredients'])
                assert any(v['RecipeRecordId'] == identity and v['Quantity'] == quantity+1 for v in catalog()['RecipeOutputs'])
            assert any(v['RelationshipId'] == 'sdkqa.docked' for v in catalog()['Relationships'])

        if case == 'clean':
            control('economyItemWeight').setValue(9)
            control('economyItemWeight').setValue(0)
            checked('reverted_form_prepared')
            begin_exit(None, 'file', unchanged)
            return

        edit_all()
        if case == 'rollback':
            control('packDisplayName').setText('Exit veto draft')
            order = attempt('Discard', 'file', pack_choice='Cancel')
            assert order == ['item', 'pack'], order
            unchanged()
            assert control('packDisplayName').text() == 'Exit veto draft'
            checked('later_pack_veto_restores_all_nine_item_recipe_drafts')
            attempt('Save', 'file', pack_choice='Cancel')
            all_saved()
            checked('save_followed_by_pack_veto_keeps_saved_forms_clean')
            begin_exit(None, 'window', all_saved, pack_choice='Discard')
            return

        if case == 'discard':
            if pane_test.floating_container(root) is None:
                pane_test.pane_menu_action(root, 'Undock', result, _keep)
            QtTest.QTest.qWait(150)
            assert pane_test.floating_container(root)
            begin_exit('Discard', 'window', unchanged)
            return

        for mode in ('docked', 'floating'):
            if mode == 'floating':
                if pane_test.floating_container(root) is None:
                    pane_test.pane_menu_action(root, 'Undock', result, _keep)
                QtTest.QTest.qWait(150)
                assert pane_test.floating_container(root)
            stage(mode + '_cancel')
            for route in ('file', 'window', 'python'):
                for choice in ('Cancel', 'Escape', 'Close'):
                    attempt(choice, route)
                    unchanged()
            checked(mode + '_all_nine_drafts_retained_without_writes')

        # Acquisition sorts first: invalid acquisition must preserve every form.
        control('economyRelationshipEvidence').setText('missing.evidence')
        attempt('Save', 'file')
        unchanged()
        assert control('economyRelationshipEvidence').text() == 'missing.evidence'
        control('economyRelationshipEvidence').setText('sdkqa.evidence.docked')
        # After fixing acquisition, an item succeeds before the invalid recipe.
        record('Recipe', identities[2])
        evidence = control('economyRecipeEvidence').text()
        control('economyRecipeEvidence').setText('missing.evidence')
        attempt('Save', 'window')
        assert row('EconomyItems', identities[0])['Weight'] == 11
        assert control('economyRecipeEvidence').text() == 'missing.evidence'
        assert row('EconomyRecipes', identities[2])['RecipeType'] != 'Saved before switch'
        control('economyRecipeEvidence').setText(evidence)
        checked('partial_save_keeps_successes_and_remaining_invalid_draft')
        saved = catalog_path.read_bytes()
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                      ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.CreateFileW(str(catalog_path), 0x80000000, 1, None, 3, 0x80, None)
        assert handle not in (None, ctypes.c_void_p(-1).value)
        try:
            attempt('Save', 'file')
            assert catalog_path.read_bytes() == saved
            assert control('economyRecipeType').text() == 'Saved before switch'
        finally:
            kernel.CloseHandle(handle)
        checked('real_write_failure_keeps_editor_open_for_retry')

        begin_exit('Save', 'file', all_saved)
    except Exception:
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
        stage(result.get('stage', 'failed'))


def initialized(_args):
    _handler.disconnect()
    QtCore.QTimer.singleShot(0, run)


_handler = editor.EditorEventBusHandler()
_handler.connect()
_handler.add_callback('NotifyEditorInitialized', initialized)
