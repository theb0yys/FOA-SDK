# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Actual multi-process crash-recovery acceptance; synthetic data only."""
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
    output = Path(os.environ['FOA_SDK_RECOVERY_RESULT'])
    workspace = Path(os.environ['FOA_SDK_RECOVERY_WORKSPACE'])
    case = os.environ['FOA_SDK_RECOVERY_CASE']
    catalog_path = workspace.parent / 'Catalog/catalog.tgcatalog.json'
    result = {'status': 'PARTIAL', 'case': case, 'checks': [], 'transition_seconds': [],
              'prompt_orders': [], 'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    active_path = workspace
    identities = []
    expected_path = os.environ.get('FOA_SDK_RECOVERY_EXPECTED', '')
    expected = json.loads(Path(expected_path).read_text(encoding='utf-8')) if expected_path else None

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

    def wait_for(predicate, label):
        start = time.monotonic()
        while not predicate():
            assert time.monotonic() - start < 5, 'Timed out: ' + label + ': ' + control('economyRecoveryStatus').text()
            QtTest.QTest.qWait(25)
        result['transition_seconds'].append(round(time.monotonic() - start, 4))

    def copies():
        return list((Path(os.environ['LOCALAPPDATA']) / 'FOA-SDK/Recovery/ItemRecipeDrafts').glob('*.itemrecipedrafts.json'))

    def values():
        return json.loads(json.dumps(snapshot()))

    def checkpoint():
        def ready():
            paths = copies()
            if len(paths) != 1 or control('economyRecoveryStatus').text() != 'Recovery copy updated.':
                return False
            try:
                stored = json.loads(paths[0].read_text(encoding='utf-8'))['Drafts']
                live = values()
                wanted = {'acquisition:': live['acquisition']}
                for identity in identities[:2]: wanted['item:' + identity] = live[identity]
                for identity in identities[2:]:
                    for kind, fields in zip(('recipe', 'ingredient', 'output'), live[identity]):
                        wanted[kind + ':' + identity] = fields
                def normalized(fields):
                    return {name: ({'data': value[0], 'text': '' if value[0] is not None else value[1]}
                                   if isinstance(value, list) else value) for name, value in fields.items()}
                def decoded(fields):
                    return {name: ({'data': value[1], 'text': value[2]} if value[0] == 'selection' else value[1])
                            for name, value in fields.items()}
                return set(stored) == set(wanted) and all(decoded(stored[key]['Values']) == normalized(wanted[key]) for key in wanted)
            except (OSError, ValueError, KeyError):
                return False
        wait_for(ready, 'all nine draft checkpoint')
        path = copies()[0]
        result['recovery_path'] = str(path)
        result['recovery_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        result['fields'] = values()
        result['record'] = json.loads(path.read_text(encoding='utf-8'))
        return path

    def force_ready():
        path = checkpoint()
        assert len(result['record']['Drafts']) == 9
        result['status'] = 'READY_TO_TERMINATE'
        checked('durable_checkpoint_ready_for_owned_forced_termination')
        assert hashlib.sha256(path.read_bytes()).hexdigest() == result['recovery_sha256']

    def open_workspace(path, choice=None):
        nonlocal active_path
        seen, errors = [], []
        timer = QtCore.QTimer()
        _keep.append(timer)
        started = time.monotonic()
        def answer():
            try:
                picker = next((w for w in widgets() if isinstance(w, QtWidgets.QFileDialog) and w.isVisible()), None)
                if picker:
                    if 'picker' not in seen:
                        seen.append('picker')
                        picker.setDirectory(str(path.parent))
                        picker.selectFile(path.name)
                        picker.findChild(QtWidgets.QLineEdit, 'fileNameEdit').setText(path.name)
                        return
                    assert time.monotonic() - started < 4, picker.selectedFiles()
                    picker.findChild(QtWidgets.QDialogButtonBox).button(QtWidgets.QDialogButtonBox.Open).click()
                    return
                prompt = next((w for w in widgets() if isinstance(w, QtWidgets.QMessageBox) and w.isVisible()), None)
                if prompt:
                    assert prompt.objectName() == 'economyUnsavedChangesDialog' and choice, prompt.text()
                    seen.append('draft')
                    prompt.button(getattr(QtWidgets.QMessageBox, choice)).click()
            except Exception:
                errors.append(traceback.format_exc())
                for dialog in widgets():
                    if isinstance(dialog, QtWidgets.QDialog) and dialog.isVisible(): dialog.reject()
        timer.timeout.connect(answer)
        timer.start(25)
        next(b for b in widgets() if isinstance(b, QtWidgets.QPushButton) and b.text() == 'Open existing workspace...').click()
        QtTest.QTest.qWait(200)
        timer.stop()
        assert seen == (['picker', 'draft'] if choice else ['picker']) and not errors, (seen, errors)
        wait_for(lambda: str(path).replace('\\', '/') in details.toPlainText().replace('\\', '/'), 'workspace replacement')
        wait_for(lambda: control('economyRecoveryStatus').text() != 'Checking draft recovery...', 'new workspace recovery')
        active_path = path

    def make_second_workspace():
        second_root = workspace.parent.parent / 'B'
        assert not second_root.exists()
        second_data = json.loads(json.dumps(fixture).replace(workspace.parent.as_posix(), second_root.as_posix()))
        for leaf in ('Output','Staging','Deployment','Diagnostics','Extracted','Game/Managed','Game/BepInEx/plugins'):
            (second_root / leaf).mkdir(parents=True, exist_ok=True)
        for leaf in ('Game/Managed/Assembly-CSharp.dll','Game/UnityPlayer.dll'):
            (second_root / leaf).write_text('Synthetic marker only.', encoding='utf-8')
        second = second_root / 'second.tgworkspace.json'
        second.write_text(json.dumps(second_data), encoding='utf-8')
        return second

    def lock_file(path):
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                      ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.CreateFileW(str(path), 0x80000000, 1, None, 3, 0x80, None)
        assert handle not in (None, ctypes.c_void_p(-1).value)
        return handle

    try:
        stage('fixture')
        assert case in ('seed', 'seed-floating', 'seed-exit', 'restore-exit-discard', 'offer', 'restore-again', 'restore-save', 'verify-save',
                        'discard', 'verify-discard', 'failure', 'reject-corrupt')
        assert not any('autotest_mode' in a for a in app.arguments())
        fixture = json.loads(workspace.read_text(encoding='utf-8'))
        assert fixture['WorkspaceId'] == 'sdkqa.item-recipe-recovery'
        assert Path(fixture['RootPath']).resolve() == workspace.parent.resolve()
        assert workspace.resolve() == (Path(os.environ['LOCALAPPDATA']) / 'FOA-SDK/Workspace/foa-sdk.tgworkspace.json').resolve()
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
        if toggle.text() == 'Show advanced details': toggle.click()
        details = next(w for w in widgets() if isinstance(w, QtWidgets.QPlainTextEdit)
                       and w.toPlainText().startswith('Workspace file:'))
        if not expected:
            assert not catalog_path.exists()
            pane_test.open_default_pack()
            QtTest.QTest.qWait(250)
            pack = control('TaintedGrailPackManager')
            _keep.append(pack)
            control('packDisplayName').setText('Workspace fixture')
            control('packOwner').setText('sdkqa')
            next(b for b in pack.findChildren(QtWidgets.QPushButton) if b.text() == 'Save mod').click()
            assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        pane_test.open_default_pane('Tainted Grail Item and Recipe Editor')
        QtTest.QTest.qWait(250)
        dock = control('TaintedGrailModdingSDK.ItemRecipeEditor')
        root = dock.widget()
        _keep.extend((dock, root))
        main = control('MainWindow')
        wait_for(lambda: control('economyRecoveryStatus').text() != 'Checking draft recovery...', 'initial recovery read')
        if not expected:
            assert control('economyItemWeight').isEnabled()
            pane_test.request_titlebar_close(root, result, _keep)
            QtTest.QTest.qWait(250)
            assert not isValid(root)
            pane_test.open_default_pane('Tainted Grail Item and Recipe Editor')
            QtTest.QTest.qWait(250)
            dock = control('TaintedGrailModdingSDK.ItemRecipeEditor')
            root = dock.widget()
            _keep.extend((dock, root))
            wait_for(lambda: control('economyItemWeight').isEnabled(), 'fresh pane recovery')
            assert pane_test.floating_container(root) is None
            control('economyRecipeSettings').setChecked(True)
            identities = [create('Item', 'Item A'), create('Item', 'Item B'),
                          create('Recipe', 'Recipe A'), create('Recipe', 'Recipe B')]
        else:
            identities = expected['identities']
        result['identities'] = identities
        initial = catalog_path.read_bytes()
        result['catalog_sha256'] = hashlib.sha256(initial).hexdigest()
        if expected:
            assert result['catalog_sha256'] == expected['catalog_sha256'], 'Startup changed saved catalog bytes'

        def unchanged():
            assert catalog_path.read_bytes() == initial

        def clean_exit_bytes():
            unchanged()
            assert not copies(), 'Retired recovery reappeared'

        def all_saved():
            for identity, weight in zip(identities[:2], (11, 22)):
                assert row('EconomyItems', identity)['Weight'] == weight
            for identity, quantity in zip(identities[2:], (3, 4)):
                assert row('EconomyRecipes', identity)['RecipeType'] == 'Saved before switch'
                assert any(v['RecipeRecordId'] == identity and v['Quantity'] == quantity for v in catalog()['RecipeIngredients'])
                assert any(v['RecipeRecordId'] == identity and v['Quantity'] == quantity + 1 for v in catalog()['RecipeOutputs'])
            assert any(v['RelationshipId'] == 'sdkqa.docked' for v in catalog()['Relationships'])
            assert not copies(), 'Saved drafts were not retired'
            result['catalog_sha256'] = hashlib.sha256(catalog_path.read_bytes()).hexdigest()

        if case.startswith('seed'):
            if case == 'seed-floating':
                pane_test.pane_menu_action(root, 'Undock', result, _keep)
                assert pane_test.floating_container(root)
            gaps = []
            last_tick = [time.monotonic()]
            heartbeat = QtCore.QTimer()
            _keep.append(heartbeat)
            def tick():
                now = time.monotonic()
                gaps.append(now - last_tick[0])
                last_tick[0] = now
            heartbeat.timeout.connect(tick)
            heartbeat.start(20)
            edit_all()
            control('economyItemRarity').setText('  unfinished \u03a9  ')
            control('economyRecipeSettings').setChecked(True)
            # Choose the acquisition tab through its owning QTabWidget.
            control('economyTabs').setCurrentIndex(2)
            result['presentation'] = 'floating' if pane_test.floating_container(root) else 'docked'
            unchanged()
            checkpoint()
            heartbeat.stop()
            assert gaps and max(gaps) < 0.5, gaps
            result['checkpoint_max_event_gap_seconds'] = round(max(gaps), 4)
            checked('background_checkpoint_keeps_ui_responsive')
            if case == 'seed-exit':
                control('packDisplayName').setText('Later pane veto draft')
                path = checkpoint()
                timer = QtCore.QTimer()
                _keep.append(timer)
                prompts, errors = [], []
                def wait_at_later_prompt():
                    prompt = next((w for w in widgets() if isinstance(w, QtWidgets.QMessageBox) and w.isVisible()), None)
                    if prompt is None: return
                    try:
                        if prompt.objectName() == 'economyUnsavedChangesDialog':
                            prompts.append('item')
                            prompt.button(QtWidgets.QMessageBox.Discard).click()
                        else:
                            assert prompt.objectName() == 'packUnsavedChangesDialog'
                            assert prompts == ['item']
                            assert path.exists(), 'Item recovery disappeared while the later pane was still deciding exit'
                            assert hashlib.sha256(path.read_bytes()).hexdigest() == result['recovery_sha256']
                            assert Path(str(path) + '.lock').exists(), 'Exit released recovery ownership before the final vote'
                            timer.stop()
                            result['status'] = 'READY_TO_TERMINATE'
                            checked('checkpoint_and_workspace_lock_survive_pending_editor_exit')
                    except Exception:
                        errors.append(traceback.format_exc())
                        timer.stop()
                        prompt.reject()
                timer.timeout.connect(wait_at_later_prompt)
                timer.start(25)
                route_exit('file')
                timer.stop()
                assert not errors, errors
                raise AssertionError('Pending exit must remain at the Pack prompt until the parent terminates this Editor')
            force_ready()
            return

        if case in ('verify-save', 'verify-discard'):
            assert not copies() and not control('economyRecoveryPrompt').isVisible()
            if case == 'verify-save': all_saved()
            else:
                for identity in identities[:2]: assert row('EconomyItems', identity)['Weight'] == 0
            checked('fresh_editor_has_no_retired_recovery_offer')
            begin_exit(None, 'file', clean_exit_bytes)
            return

        if case == 'reject-corrupt':
            path = copies()[0]
            damaged = path.read_bytes()
            assert damaged == b'{broken'
            assert control('economyRecoveryPrompt').isVisible()
            assert not control('economyRestoreDrafts').isEnabled()
            assert not control('economyItemWeight').isEnabled()
            control('economyRetryRecovery').click()
            wait_for(lambda: control('economyRecoveryStatus').text() != 'Checking draft recovery...', 'corrupt retry')
            assert path.read_bytes() == damaged
            checked('damaged_recovery_kept_and_restore_disabled')
            assert path.resolve().is_relative_to(Path(os.environ['LOCALAPPDATA']).resolve())
            for mutation in ('unknown-field', 'missing-definition', 'out-of-range'):
                altered = json.loads(json.dumps(expected['record']))
                draft = altered['Drafts']['item:' + identities[0]]
                if mutation == 'unknown-field':
                    for part in ('Values', 'Baseline'):
                        draft[part]['economyObsoleteRarity'] = draft[part].pop('economyItemRarity')
                elif mutation == 'missing-definition':
                    altered['Item'] = 'sdkqa.missing-definition'
                else:
                    draft['Values']['economyItemWeight'] = ['number', -1]
                payload = json.dumps(altered).encode('utf-8')
                path.write_bytes(payload)
                control('economyRetryRecovery').click()
                wait_for(lambda: control('economyRecoveryStatus').text() != 'Checking draft recovery...', mutation)
                assert not control('economyRestoreDrafts').isEnabled()
                assert not control('economyItemWeight').isEnabled()
                assert path.read_bytes() == payload
                checked(mutation + '_kept_without_silent_coercion')
            control('economyDiscardRecovery').click()
            assert not copies() and control('economyItemWeight').isEnabled()
            checked('explicit_discard_unblocks_after_damaged_copy')
            begin_exit(None, 'file', clean_exit_bytes)
            return

        if case == 'failure':
            edit_all()
            path = checkpoint()
            original = path.read_bytes()
            handle = lock_file(path)
            try:
                control('economyItemWeight').setValue(77)
                wait_for(lambda: 'could not be updated' in control('economyRecoveryStatus').text(), 'real checkpoint failure')
                assert path.read_bytes() == original
                assert control('economyItemWeight').value() == 77
                checked('failed_recovery_write_preserves_last_good_copy_and_current_form')
                attempt('Discard', 'file')
                assert path.read_bytes() == original
                checked('failed_recovery_retirement_keeps_editor_open')
            finally:
                kernel.CloseHandle(handle)
            control('economyRetryRecovery').click()
            wait_for(lambda: path.read_bytes() != original, 'write retry')
            checked('recovery_write_retry_succeeds')
            second = make_second_workspace()
            handle = lock_file(path)
            try:
                open_workspace(second, 'Discard')
                assert "previous workspace's recovery copy could not be cleared" in control('economyStatus').text()
                assert path.exists() and not control('economyRecoveryPrompt').isVisible()
                checked('failed_old_workspace_retirement_is_reported_and_copy_stays_scoped')
                open_workspace(workspace)
                assert control('economyRecoveryPrompt').isVisible() and control('economyRestoreDrafts').isEnabled()
                checked('return_after_retirement_failure_offers_preserved_old_copy')
            finally:
                kernel.CloseHandle(handle)
            control('economyDiscardRecovery').click()
            assert not copies()
            begin_exit(None, 'file', clean_exit_bytes)
            return

        assert control('economyRecoveryPrompt').isVisible()
        assert control('economyRestoreDrafts').isVisible() and control('economyRestoreDrafts').isEnabled(), control('economyRecoveryStatus').text()
        assert not control('economyItemWeight').isEnabled()
        path = copies()[0]
        assert hashlib.sha256(path.read_bytes()).hexdigest() == expected['recovery_sha256']
        original_recovery = path.read_bytes()
        assert root.grab().save(str(output.with_name('recovery-offer.png')))
        checked('fresh_process_offers_workspace_bound_recovery_without_catalog_writes')

        if case == 'offer':
            second = make_second_workspace()
            open_workspace(second)
            assert control('economyItemWeight').isEnabled()
            assert not control('economyRecoveryPrompt').isVisible()
            assert path.read_bytes() == original_recovery
            open_workspace(workspace)
            assert control('economyRecoveryPrompt').isVisible()
            assert control('economyRestoreDrafts').isEnabled()
            assert path.read_bytes() == original_recovery
            checked('same_workspace_id_other_root_does_not_receive_drafts_and_return_preserves_offer')
            def pending_kept():
                unchanged()
                assert path.read_bytes() == original_recovery
            begin_exit(None, 'file', pending_kept)
            return

        if case == 'discard':
            control('economyDiscardRecovery').click()
            assert not copies() and control('economyItemWeight').isEnabled()
            for identity in identities[:2]:
                record('Item', identity)
                assert control('economyItemWeight').value() == 0
            checked('discard_offer_removes_only_recovery_and_retains_saved_forms')
            edit_all()
            checkpoint()
            begin_exit('Discard', 'file', clean_exit_bytes)
            return

        control('economyRestoreDrafts').click()
        assert values() == expected['fields'], 'Restored draft fields differ'
        assert control('economyRecipeSettings').isChecked()
        assert control('economyTabs').currentIndex() == 2
        assert path.exists()
        unchanged()
        checked('restore_preserves_all_nine_raw_forms_baselines_and_presentation')
        if case == 'restore-again':
            force_ready()
            return

        if case == 'restore-exit-discard':
            begin_exit('Discard', 'file', clean_exit_bytes)
            return

        assert case == 'restore-save'
        attempt('Cancel', 'file')
        assert path.exists()
        control('economyRelationshipEvidence').setText('missing.evidence')
        attempt('Save', 'file')
        assert path.exists() and control('economyRelationshipEvidence').text() == 'missing.evidence'
        unchanged()
        control('economyRelationshipEvidence').setText('sdkqa.evidence.docked')
        handle = lock_file(catalog_path)
        try:
            attempt('Save', 'window')
            assert path.exists()
            unchanged()
        finally:
            kernel.CloseHandle(handle)
        checked('cancel_invalid_save_and_real_catalog_write_failure_keep_recovery')
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
