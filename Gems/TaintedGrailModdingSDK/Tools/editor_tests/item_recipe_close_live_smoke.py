# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native pane-close acceptance with synthetic data; run without autotest_mode.

Save commands are exercised through the compiled pane. Durable catalog bytes,
close admission and reopened fields are checked independently of UI status copy.
"""
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

PANE = 'Tainted Grail Item and Recipe Editor'
DOCK = 'TaintedGrailModdingSDK.ItemRecipeEditor'
_keep = []


def run():
    output = Path(os.environ['FOA_SDK_ECONOMY_RESULT'])
    workspace = Path(os.environ['FOA_SDK_ECONOMY_WORKSPACE'])
    catalog_path = workspace.parent / 'Catalog/catalog.tgcatalog.json'
    result = {'status': 'PARTIAL', 'checks': [], 'transition_seconds': [],
              'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    root = None

    def stage(name):
        result['stage'] = name
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')

    def control(name):
        values = app.allWidgets()
        _keep.extend(values)
        return next(w for w in reversed(values) if isValid(w) and w.objectName() == name)

    def record(kind, identity):
        combo = control('economy' + kind + 'Choice')
        index = combo.findData(identity)
        assert index >= 0, identity
        combo.setCurrentIndex(index)

    def read_catalog():
        data = json.loads(catalog_path.read_text(encoding='utf-8'))
        return data.get('ClassData', data)

    def row(collection, identity):
        return next(v for v in read_catalog()[collection] if v['RecordId'] == identity)

    def close(choice=None, accepted=False, capture=False):
        dock = pane_test.pane_dock(root)
        floating = pane_test.floating_container(root)
        seen, errors = [], []
        started = time.monotonic()
        timer = QtCore.QTimer()

        def answer():
            values = app.allWidgets()
            _keep.extend(values)
            prompt = next((w for w in values if isValid(w) and isinstance(w, QtWidgets.QMessageBox)
                           and w.isVisible() and w.objectName() == 'economyUnsavedChangesDialog'), None)
            if prompt is None:
                return
            timer.stop()
            try:
                seen.append(prompt.objectName())
                buttons = QtWidgets.QMessageBox
                assert prompt.standardButtons() == buttons.Save | buttons.Discard | buttons.Cancel
                assert prompt.defaultButton() == prompt.button(buttons.Cancel)
                assert prompt.escapeButton() == prompt.button(buttons.Cancel)
                if capture:
                    assert prompt.grab().save(str(output.with_name('close-prompt.png')))
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
        pane_test.request_titlebar_close(root, result, _keep)
        timer.stop()
        elapsed = time.monotonic() - started
        result['transition_seconds'].append(round(elapsed, 4))
        assert not errors and len(seen) == (1 if choice else 0), (errors, seen, choice)
        assert elapsed < 3, 'Small fixture close exceeded three seconds'
        deadline = time.monotonic() + 5
        while accepted and time.monotonic() < deadline and (
                isValid(dock) or (floating is not None and isValid(floating))):
            QtTest.QTest.qWait(25)
        if accepted:
            assert not isValid(root) and not isValid(dock), control('economyStatus').text() if isValid(root) else 'Dock survived'
            assert floating is None or not isValid(floating)
            assert not general.is_pane_visible(PANE)
        else:
            assert isValid(root) and root.isVisible() and isValid(dock) and dock.isVisible()

    def reopen(floating=False):
        nonlocal root
        pane_test.open_default_pane(PANE)
        QtTest.QTest.qWait(250)
        dock = control(DOCK)
        root = dock.widget()
        _keep.extend((dock, root))
        control('economyRecipeSettings').setChecked(True)
        assert root.isVisible() and pane_test.floating_container(root) is None
        if floating:
            pane_test.pane_menu_action(root, 'Undock', result, _keep)
            QtTest.QTest.qWait(150)
            assert pane_test.floating_container(root) is not None

    def create(kind, name):
        seen = []
        timer = QtCore.QTimer()
        def answer():
            values = app.allWidgets()
            _keep.extend(values)
            prompt = next((w for w in values if isValid(w) and isinstance(w, QtWidgets.QInputDialog)
                           and w.isVisible()), None)
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

    try:
        stage('fixture')
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
        general.idle_enable(True)
        QtTest.QTest.qWait(3000)
        pane_test.open_default_pack()
        QtTest.QTest.qWait(350)
        control('packDisplayName').setText('Close fixture')
        control('packOwner').setText('sdkqa')
        pack = control('TaintedGrailPackManager')
        next(b for b in pack.findChildren(QtWidgets.QPushButton) if b.text() == 'Save mod').click()
        assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        assert pane_test.pane_dock(pack).close()
        QtTest.QTest.qWait(250)
        reopen()
        item_a = create('Item', 'Item A')
        item_b = create('Item', 'Item B')
        recipe_a = create('Recipe', 'Recipe A')
        recipe_b = create('Recipe', 'Recipe B')
        result['identities'] = [item_a, item_b, recipe_a, recipe_b]
        close(accepted=True)
        result['checks'].append('creating_and_browsing_definitions_starts_clean')

        for floating, mode in ((False, 'docked'), (True, 'floating')):
            stage(mode + '_cancel')
            reopen(floating)
            record('Item', item_a)
            record('Recipe', recipe_a)
            old_weight = control('economyItemWeight').value()
            new_weight = old_weight + 2.5
            original = catalog_path.read_bytes()
            control('economyItemWeight').setValue(new_weight)
            record('Item', item_b)
            record('Recipe', recipe_b)
            control('economyRecipeType').setText(mode + ' draft')
            for choice in ('Cancel', 'Escape', 'Close'):
                close(choice, capture=(mode == 'docked' and choice == 'Cancel'))
                assert control('economyRecipeType').text() == mode + ' draft'
                record('Item', item_a)
                assert control('economyItemWeight').value() == new_weight
                record('Item', item_b)
                assert catalog_path.read_bytes() == original
                result['checks'].append(mode + '_' + choice.lower() + '_retains_visible_and_hidden_drafts')
            close('Discard', accepted=True)
            assert catalog_path.read_bytes() == original
            reopen(floating)
            record('Item', item_a)
            assert control('economyItemWeight').value() == old_weight
            close(accepted=True)
            result['checks'].append(mode + '_discard_no_write_and_reopen_saved_values')

            stage(mode + '_revert_and_manual_save')
            reopen(floating)
            record('Item', item_a)
            record('Recipe', recipe_a)
            # Cover every editable control in all five owned forms. Restoring the
            # original values must leave no dirty state, including selection widgets.
            field_count = 0
            for form_name in ('economyItemProfile', 'economyRecipeSettings', 'economyIngredientForm',
                              'economyOutputForm', 'economyRelationshipForm'):
                for field in control(form_name).findChildren(QtWidgets.QWidget):
                    if isinstance(field.parentWidget(), (QtWidgets.QAbstractSpinBox, QtWidgets.QComboBox)):
                        continue
                    if isinstance(field, QtWidgets.QLineEdit) and not field.isReadOnly():
                        old = field.text()
                        field.setText(old + 'x')
                        close('Cancel')
                        field.setText(old)
                    elif isinstance(field, (QtWidgets.QSpinBox, QtWidgets.QDoubleSpinBox)):
                        old = field.value()
                        field.setValue(old - 1 if old >= field.maximum() else old + 1)
                        close('Cancel')
                        field.setValue(old)
                    elif isinstance(field, QtWidgets.QCheckBox):
                        old = field.isChecked()
                        field.setChecked(not old)
                        close('Cancel')
                        field.setChecked(old)
                    elif isinstance(field, QtWidgets.QComboBox):
                        old = field.currentIndex()
                        field.setCurrentIndex((old + 1) % field.count())
                        close('Cancel')
                        field.setCurrentIndex(old)
                    else:
                        continue
                    field_count += 1
            assert field_count >= 45, field_count
            close(accepted=True)
            result['checks'].append(mode + '_all_' + str(field_count) + '_editable_fields_prompt_and_revert_clean')
            reopen(floating)
            record('Item', item_a)
            record('Recipe', recipe_a)
            control('economyItemWeight').setValue(new_weight)
            control('economyRecipeType').setText(mode + ' manual')
            control('economySaveItem').click()
            assert row('EconomyItems', item_a)['Weight'] == new_weight
            close('Cancel')
            assert control('economyRecipeType').text() == mode + ' manual'
            assert control('economySaveRecipe').isEnabled()
            control('economySaveRecipe').click()
            assert row('EconomyRecipes', recipe_a)['RecipeType'] == mode + ' manual', control('economyStatus').text()
            close(accepted=True)
            result['checks'].append(mode + '_manual_save_clears_only_saved_form')

            stage(mode + '_all_drafts_save')
            reopen(floating)
            for identity, value in ((item_a, new_weight + 1), (item_b, new_weight + 2)):
                record('Item', identity)
                control('economyItemWeight').setValue(value)
            for identity, quantity in ((recipe_a, 3), (recipe_b, 4)):
                record('Recipe', identity)
                control('economyRecipeType').setText(mode + ' saved')
                record('Ingredient', item_a)
                control('economyIngredientQuantity').setValue(quantity)
                record('Output', item_b)
                control('economyOutputQuantity').setValue(quantity + 1)
            close('Save', accepted=True)
            assert row('EconomyItems', item_a)['Weight'] == new_weight + 1
            assert row('EconomyItems', item_b)['Weight'] == new_weight + 2
            for identity, quantity in ((recipe_a, 3), (recipe_b, 4)):
                assert row('EconomyRecipes', identity)['RecipeType'] == mode + ' saved'
                assert any(v['RecipeRecordId'] == identity and v['Quantity'] == quantity
                           for v in read_catalog()['RecipeIngredients'])
                assert any(v['RecipeRecordId'] == identity and v['Quantity'] == quantity + 1
                           for v in read_catalog()['RecipeOutputs'])
            result['checks'].append(mode + '_save_all_eight_drafts_across_four_definitions')
            reopen(floating)
            record('Item', item_a)
            assert control('economyItemWeight').value() == new_weight + 1
            record('Recipe', recipe_a)
            assert control('economyRecipeType').text() == mode + ' saved'
            control('economyIngredients').selectRow(0)
            control('economyOutputs').selectRow(0)
            close(accepted=True)
            result['checks'].append(mode + '_reopen_saved_profiles_and_select_joins_stays_clean')

            stage(mode + '_partial_failure')
            reopen(floating)
            record('Item', item_a)
            record('Recipe', recipe_a)
            control('economyItemWeight').setValue(new_weight + 7)
            evidence = control('economyRecipeEvidence').text()
            control('economyRecipeEvidence').setText('missing.evidence')
            control('economyRecipeType').setText(mode + ' retry')
            close('Save')
            assert row('EconomyItems', item_a)['Weight'] == new_weight + 7
            assert row('EconomyRecipes', recipe_a)['RecipeType'] == mode + ' saved'
            assert control('economyRecipeEvidence').text() == 'missing.evidence'
            assert control('economyRecipeType').text() == mode + ' retry'
            close('Cancel')
            control('economyRecipeEvidence').setText(evidence)
            close('Save', accepted=True)
            assert row('EconomyRecipes', recipe_a)['RecipeType'] == mode + ' retry'
            result['checks'].append(mode + '_partial_success_failed_validation_cancel_and_retry')

            stage(mode + '_write_failure')
            reopen(floating)
            record('Item', item_b)
            control('economyItemWeight').setValue(new_weight + 9)
            record('Recipe', recipe_a)
            control('economyIngredients').selectRow(0)
            control('economyOutputs').selectRow(0)
            ingredient_quantity = control('economyIngredientQuantity').value() + 10
            output_quantity = control('economyOutputQuantity').value() + 11
            control('economyIngredientQuantity').setValue(ingredient_quantity)
            control('economyOutputQuantity').setValue(output_quantity)
            saved = catalog_path.read_bytes()
            kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                          ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
            kernel.CreateFileW.restype = ctypes.c_void_p
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            handle = kernel.CreateFileW(str(catalog_path), 0x80000000, 1, None, 3, 0x80, None)
            assert handle not in (None, ctypes.c_void_p(-1).value)
            try:
                close('Save')
                assert catalog_path.read_bytes() == saved
                assert control('economyItemWeight').value() == new_weight + 9
                assert control('economyIngredientQuantity').value() == ingredient_quantity
                assert control('economyOutputQuantity').value() == output_quantity
            finally:
                kernel.CloseHandle(handle)
            close('Save', accepted=True)
            assert row('EconomyItems', item_b)['Weight'] == new_weight + 9
            assert any(v['RecipeRecordId'] == recipe_a and v['Quantity'] == ingredient_quantity
                       for v in read_catalog()['RecipeIngredients'])
            assert any(v['RecipeRecordId'] == recipe_a and v['Quantity'] == output_quantity
                       for v in read_catalog()['RecipeOutputs'])
            result['checks'].append(mode + '_real_catalog_write_failure_keeps_three_drafts_and_retry_saves')

            stage(mode + '_missing_definition')
            reopen(floating)
            control('economyItemWeight').setValue(17)
            before = catalog_path.read_bytes()
            close('Save')
            assert control('economyItemWeight').value() == 17
            assert catalog_path.read_bytes() == before
            close('Discard', accepted=True)
            result['checks'].append(mode + '_unselected_definition_failed_save_preserves_form')

            stage(mode + '_acquisition')
            reopen(floating)
            source = control('economyRelationshipSource')
            source.setCurrentIndex(source.findData(item_a))
            control('economyRelationshipId').setText('sdkqa.' + mode)
            control('economyRelationshipSubject').setText('synthetic:vendor')
            close('Cancel')
            before = catalog_path.read_bytes()
            close('Save')
            assert catalog_path.read_bytes() == before
            assert control('economyRelationshipSubject').text() == 'synthetic:vendor'
            record('Item', item_a)
            control('economyRelationshipEvidence').setText('sdkqa.evidence.' + mode)
            close('Save', accepted=True)
            assert any(v['RelationshipId'] == 'sdkqa.' + mode for v in read_catalog()['Relationships'])
            result['checks'].append(mode + '_acquisition_cancel_failed_save_and_success')

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


def initialized(_args):
    _handler.disconnect()
    QtCore.QTimer.singleShot(0, run)


_handler = editor.EditorEventBusHandler()
_handler.connect()
_handler.add_callback('NotifyEditorInitialized', initialized)
