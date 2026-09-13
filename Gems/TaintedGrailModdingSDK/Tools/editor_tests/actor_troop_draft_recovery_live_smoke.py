# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native Actor/Troop multi-process recovery acceptance with synthetic data and saved-byte checks."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import sys
import shutil
import time
import traceback

Path(os.environ['FOA_SDK_RECOVERY_RESULT']).write_text(
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


def run_population_recovery_smoke():
    output = Path(os.environ['FOA_SDK_RECOVERY_RESULT'])
    workspace = Path(os.environ['FOA_SDK_RECOVERY_WORKSPACE'])
    catalog_path = workspace.parent / 'Catalog/catalog.tgcatalog.json'
    result = {'status': 'PARTIAL', 'checks': [], 'transition_seconds': [],
              'editor_initialized': True, 'about_to_quit': False}
    app = QtWidgets.QApplication.instance()
    root = None
    case = os.environ["FOA_SDK_RECOVERY_CASE"]
    result["case"] = case
    result["prompt_orders"] = []
    expected_file = os.environ.get('FOA_SDK_RECOVERY_EXPECTED')
    expected = json.loads(Path(expected_file).read_text(encoding='utf-8')) if expected_file else None
    recovery_root = Path(os.environ['LOCALAPPDATA']) / 'FOA-SDK/Recovery/ActorTroopDrafts'
    copy = Path(expected['recovery_path']) if expected else None
    identities = expected['identities'] if expected else []
    initial_hash = expected['initial_catalog_sha256'] if expected else None
    saved_pack = expected['saved_pack'] if expected else None

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
                value = (field.currentData(), field.currentText(),
                         [(field.itemData(i), field.itemText(i)) for i in range(field.count())])
            elif isinstance(field, QtWidgets.QListWidget):
                # Source registry entries acquire canonical order on a fresh
                # process. Compare exact evidence IDs/selections independently.
                value = sorted([(field.item(i).data(QtCore.Qt.UserRole), field.item(i).isSelected())
                                for i in range(field.count())], key=lambda pair: str(pair[0]))
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


    def wait_until(predicate, label, measure=False):
        started = time.monotonic()
        previous = started
        maximum = 0
        while not predicate():
            assert time.monotonic() - started < 5, (label, control('populationRecoveryStatus').text())
            QtTest.QTest.qWait(25)
            now = time.monotonic()
            maximum = max(maximum, now - previous)
            previous = now
        if measure:
            result.setdefault('checkpoint_event_gaps', []).append(round(maximum, 4))
            assert maximum < 0.5, (label, maximum)

    def serial_snapshot():
        return json.loads(json.dumps(snapshot()))

    def catalog_hash():
        return hashlib.sha256(catalog_path.read_bytes()).hexdigest()

    def unchanged():
        assert catalog_hash() == initial_hash

    def remember():
        result.update(identities=identities, initial_catalog_sha256=initial_hash, saved_pack=saved_pack,
                      recovery_path=str(copy), forms=serial_snapshot())

    def checkpoint(actor_dirty=True):
        nonlocal copy
        def matches():
            nonlocal copy
            files = list(recovery_root.glob('*.actortroopdrafts.json'))
            if copy is None:
                if len(files) != 1:
                    return False
                copy = files[0]
            if not copy.exists():
                return False
            try:
                document = json.loads(copy.read_text(encoding='utf-8'))
                values = document['Values']
                return (document['Actor'] == identities[0] and document['Troop'] == identities[3]
                        and document['ActorDirty'] == actor_dirty and document['TroopDirty']
                        and document['MemberDirty']
                        and values['actorArchetype'] == ['string', control('populationArchetype').text()]
                        and values['troopMaximumSize'] == ['integer', control('populationMaximumSize').value()]
                        and values['memberWeight'] == ['string', control('populationMemberWeight').text()]
                        and document['Member'] == member(identities[3], identities[0])['LinkId']
                        and len(document['RemovedMembers']) == 1
                        and {m['ActorRecordId'] for m in document['Members']} == set(identities[:1] + identities[2:3]))
            except (ValueError, KeyError, OSError):
                return False
        wait_until(matches, 'matching durable actor/troop/member checkpoint', measure=True)
        remember()
        result['checkpoint'] = json.loads(copy.read_text(encoding='utf-8'))
        result['recovery_sha256'] = hashlib.sha256(copy.read_bytes()).hexdigest()

    def ready():
        checkpoint()
        unchanged()
        checked('all_drafts_and_member_staging_match_durable_checkpoint')
        result['status'] = 'READY_TO_TERMINATE'
        stage('checkpoint_confirmed_for_owned_forced_stop')

    def seed():
        nonlocal identities, initial_hash, saved_pack
        control('packDisplayName').setText('Population recovery fixture')
        control('packOwner').setText('sdkqa')
        next(b for b in pack.findChildren(QtWidgets.QPushButton) if b.text() == 'Save mod').click()
        assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        saved_pack = control('packSavedMods').currentData()
        assert saved_pack
        identities = [create(name) for name in ('Actor A', 'Actor B', 'Actor C')]
        choose('populationActor', identities[0])
        identities.append(create('Recovery troop', troop=True))
        new_member(identities[1])
        control('populationSaveTroop').click()
        assert len(members(identities[3])) == 2
        initial_hash = catalog_hash()
        select_member(identities[3], identities[1])
        next(b for b in root.findChildren(QtWidgets.QPushButton) if b.text() == 'Remove selected member').click()
        new_member(identities[2])
        control('populationStageMember').click()
        select_member(identities[3], identities[0])
        edit('populationArchetype', '  recovery actor draft  ')
        control('populationMaximumSize').setValue(5)
        edit('populationMemberWeight', '  invalid raw weight  ')
        if case == 'seed-floating':
            pane_test.pane_menu_action(root, 'Undock', result, _keep)
            QtTest.QTest.qWait(150)
            assert pane_test.floating_container(root)
        checked('seed_has_actor_troop_unstaged_member_and_staged_add_remove')
        checkpoint()

    def offer(valid=True):
        wait_until(lambda: control('populationRecoveryPrompt').isVisible(), 'recovery offer')
        assert not control('populationTabs').isEnabled()
        assert control('populationRestoreDrafts').isEnabled() == valid
        assert control('populationDiscardRecovery').isEnabled()
        assert copy.exists()
        unchanged()

    def restore():
        offer()
        assert control('populationRecoveryPrompt').grab().save(str(output.with_name('recovery-prompt.png')))
        control('populationRestoreDrafts').click()
        wait_until(lambda: control('populationTabs').isEnabled(), 'restored forms')
        actual = serial_snapshot()
        result['restored_forms'] = actual
        result['restore_differences'] = [{'index': i, 'expected': before, 'actual': after}
                                       for i, (before, after) in enumerate(zip(expected['forms'], actual)) if before != after]
        assert actual == expected['forms'], ('Recovered forms differ', result['restore_differences'])
        unchanged()
        checked('restored_all_raw_forms_and_staging_without_catalog_writes')

    def open_saved_mod():
        choice = control('packSavedMods')
        index = choice.findData(saved_pack)
        assert index >= 0, saved_pack
        choice.setCurrentIndex(index)
        next(b for b in pack.findChildren(QtWidgets.QPushButton) if b.text() == 'Open selected').click()
        assert control('packActiveSummary').text() != 'None selected'

    def all_saved():
        a, b, c, troop = identities
        assert row('ActorProfiles', a)['Archetype'] == 'recovery actor draft'
        assert row('TroopProfiles', troop)['MaximumSize'] == 5
        assert member(troop, a)['Weight'] == 2.75
        assert member(troop, c)['Weight'] == 1
        assert not any(v['ActorRecordId'] == b for v in members(troop))
        assert not copy.exists()

    def switch_workspace(path):
        general.open_pane('Tainted Grail SDK Status')
        QtTest.QTest.qWait(150)
        toggle = control('foundationAdvancedToggle')
        if toggle.text() == 'Show advanced details':
            toggle.click()
        button = next(w for w in widgets() if isinstance(w, QtWidgets.QPushButton) and w.text() == 'Open existing workspace...')
        seen, errors = [], []
        timer = QtCore.QTimer()
        _keep.append(timer)
        def answer():
            try:
                prompt = next((w for w in widgets() if isinstance(w, QtWidgets.QFileDialog) and w.isVisible()), None)
                if prompt:
                    if not seen:
                        seen.append('picker')
                        prompt.setDirectory(str(path.parent))
                        prompt.selectFile(path.name)
                        prompt.findChild(QtWidgets.QLineEdit, 'fileNameEdit').setText(path.name)
                    prompt.findChild(QtWidgets.QDialogButtonBox).button(QtWidgets.QDialogButtonBox.Open).click()
            except Exception:
                errors.append(traceback.format_exc())
        timer.timeout.connect(answer)
        timer.start(25)
        try:
            button.click()
        finally:
            timer.stop()
        assert seen and not errors
        wait_until(lambda: control('populationRecoveryStatus').text() != 'Checking draft recovery...', 'workspace recovery binding')

    def verify_discard():
        unchanged()
        assert not copy.exists(), 'Accepted Discard left a stale recovery copy'

    try:
        stage('fixture')
        app.setAttribute(QtCore.Qt.AA_DontUseNativeDialogs, True)
        fixture = json.loads(workspace.read_text(encoding='utf-8-sig'))
        assert fixture['WorkspaceId'] == 'sdkqa.actor-troop-recovery'
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
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                      ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        general.idle_enable(True)
        QtTest.QTest.qWait(2500)
        pane_test.open_default_pack()
        QtTest.QTest.qWait(250)
        pack = control('TaintedGrailPackManager')
        reopen(allow_existing=True)
        wait_until(lambda: control('populationRecoveryStatus').text() != 'Checking draft recovery...', 'initial recovery check')
        main = control('MainWindow')
        if not expected:
            assert not catalog_path.exists() or not catalog().get('ActorProfiles')
            wait_until(lambda: control('populationTabs').isEnabled(), 'fresh authoring')
            # Apply default docking to a new clean instance after inherited layout.
            pane_test.request_titlebar_close(root, result, _keep)
            QtTest.QTest.qWait(250)
            reopen()
            wait_until(lambda: control('populationTabs').isEnabled(), 'recreated clean pane')
            seed()
        else:
            result.update(identities=identities, initial_catalog_sha256=initial_hash,
                          saved_pack=saved_pack, recovery_path=str(copy))

        if case in ('seed', 'seed-floating', 'restore-again'):
            if expected:
                restore()
            ready()
            return
        if case == 'seed-exit':
            control('packDisplayName').setText('Later pane exit veto draft')
            checkpoint()
            timer = QtCore.QTimer()
            _keep.append(timer)
            seen = []
            def answer_pending_exit():
                prompt = next((w for w in widgets() if isinstance(w, QtWidgets.QMessageBox) and w.isVisible()), None)
                if not prompt:
                    return
                try:
                    if prompt.objectName() == 'populationUnsavedChangesDialog':
                        seen.append('population')
                        assert 'exiting the Editor' in prompt.text()
                        prompt.button(QtWidgets.QMessageBox.Discard).click()
                    elif prompt.objectName() == 'packUnsavedChangesDialog':
                        timer.stop()
                        seen.append('pack')
                        assert seen == ['population', 'pack'], seen
                        # The host can defer pane deletion until the nested exit
                        # loop unwinds. The later prompt is the transaction boundary.
                        assert copy.exists()
                        actual = json.loads(copy.read_text(encoding='utf-8'))
                        assert actual == result['checkpoint'], 'Checkpoint retired before later pane decided'
                        result['prompt_orders'].append(seen)
                        result['recovery_sha256'] = hashlib.sha256(copy.read_bytes()).hexdigest()
                        checked('checkpoint_survives_later_pane_modal_exit_prompt')
                        result['status'] = 'READY_TO_TERMINATE'
                        stage('forced_stop_at_later_exit_prompt')
                except Exception:
                    timer.stop()
                    result['status'] = 'FAILED'
                    result['error'] = traceback.format_exc()
                    stage('pending_exit_failed')
                    prompt.reject()
            timer.timeout.connect(answer_pending_exit)
            timer.start(25)
            route_exit('file')
            raise AssertionError('Expected owned forced termination inside later-pane prompt')

        if case == 'offer':
            offer()
            before = copy.read_bytes()
            QtTest.QTest.keyClick(root, QtCore.Qt.Key_S, QtCore.Qt.ControlModifier)
            QtTest.QTest.keyClick(root, QtCore.Qt.Key_R, QtCore.Qt.ControlModifier)
            unchanged()
            assert copy.read_bytes() == before
            checked('unresolved_offer_preserved_through_save_and_revert_shortcuts')
            pane_test.request_titlebar_close(root, result, _keep)
            QtTest.QTest.qWait(200)
            assert not isValid(root) and copy.read_bytes() == before
            reopen(allow_existing=True)
            offer()
            checked('unresolved_offer_survives_pane_close_and_reopen')
            second_root = workspace.parent.parent / 'OtherRecoveryWorkspace'
            assert second_root.resolve().is_relative_to(workspace.parent.parent.resolve())
            def rebound(value):
                if isinstance(value, str):
                    return value.replace(workspace.parent.as_posix(), second_root.as_posix())
                if isinstance(value, list):
                    return [rebound(v) for v in value]
                if isinstance(value, dict):
                    return {k: rebound(v) for k, v in value.items()}
                return value
            second_root.mkdir()
            for leaf in ('Output', 'Staging', 'Deployment', 'Diagnostics', 'Extracted'):
                (second_root / leaf).mkdir()
            shutil.copytree(workspace.parent / 'Game', second_root / 'Game')
            second = second_root / workspace.name
            second.write_text(json.dumps(rebound(fixture), indent=2), encoding='utf-8')
            switch_workspace(second)
            assert control('populationTabs').isEnabled() and not control('populationRecoveryPrompt').isVisible()
            assert copy.read_bytes() == before
            switch_workspace(workspace)
            offer()
            assert copy.read_bytes() == before
            checked('same_id_other_root_never_restores_or_retires_unresolved_original_copy')
            def unresolved_kept():
                unchanged()
                assert copy.read_bytes() == before
            begin_exit(None, 'file', unresolved_kept)
            return

        if case == 'restore-save':
            restore()
            open_saved_mod()
            control('populationSaveActor').click()
            assert row('ActorProfiles', identities[0])['Archetype'] == 'recovery actor draft'
            checkpoint(actor_dirty=False)
            control('populationSaveTroop').click()
            assert control('populationMemberWeight').text() == '  invalid raw weight  '
            assert row('TroopProfiles', identities[3])['MaximumSize'] == 1
            checked('partial_actor_save_checkpoints_remaining_invalid_troop_member_drafts')
            edit('populationMemberWeight', '2.75')
            control('populationSaveTroop').click()
            wait_until(lambda: not copy.exists(), 'clean saved recovery retirement')
            all_saved()
            remember()
            checked('save_persists_staged_add_remove_and_retires_clean_copy')
            begin_exit(None, 'file', all_saved)
            return
        if case == 'restore-exit-discard':
            restore()
            begin_exit('Discard', 'file', lambda: verify_discard())
            return
        if case in ('verify-save', 'verify-discard'):
            wait_until(lambda: control('populationTabs').isEnabled(), 'no stale recovery offer')
            assert not control('populationRecoveryPrompt').isVisible() and not copy.exists()
            if case == 'verify-save':
                all_saved()
            else:
                unchanged()
            checked('reopen_after_save_or_discard_has_no_stale_offer')
            begin_exit(None, 'file', all_saved if case == 'verify-save' else unchanged)
            return
        if case == 'discard':
            offer()
            control('populationDiscardRecovery').click()
            wait_until(lambda: control('populationTabs').isEnabled(), 'discard enables authoring')
            assert not copy.exists()
            unchanged()
            remember()
            checked('explicit_discard_removes_copy_without_catalog_writes')
            begin_exit(None, 'file', unchanged)
            return
        if case == 'reject-corrupt':
            offer(valid=False)
            broken = copy.read_bytes()
            control('populationRetryRecovery').click()
            offer(valid=False)
            assert copy.read_bytes() == broken
            checked('malformed_copy_kept_after_retry')
            repaired = expected['checkpoint']
            modified = json.loads(json.dumps(repaired))
            modified['Values']['actorUnknownField'] = ['string', 'incompatible']
            copy.write_text(json.dumps(modified), encoding='utf-8')
            control('populationRetryRecovery').click()
            offer(valid=False)
            checked('unknown_ui_field_blocks_restore_without_overwriting_copy')
            modified = json.loads(json.dumps(repaired))
            modified['Actor'] = 'missing.actor'
            modified['Values']['actorRecord'][1] = 'missing.actor'
            copy.write_text(json.dumps(modified), encoding='utf-8')
            control('populationRetryRecovery').click()
            offer(valid=False)
            checked('unavailable_definition_blocks_restore')
            copy.write_text(json.dumps(repaired), encoding='utf-8')
            control('populationRetryRecovery').click()
            offer()
            checked('compatible_copy_can_be_retried')
            control('populationDiscardRecovery').click()
            wait_until(lambda: control('populationTabs').isEnabled(), 'discard invalid/repaired copy')
            assert not copy.exists()
            begin_exit(None, 'file', unchanged)
            return
        if case == 'failure':
            original = copy.read_bytes()
            handle = kernel.CreateFileW(str(copy), 0x80000000, 1, None, 3, 0x80, None)
            assert handle not in (None, ctypes.c_void_p(-1).value)
            try:
                edit('populationArchetype', '  retry after write failure  ')
                wait_until(lambda: 'could not be updated' in control('populationRecoveryStatus').text(), 'recovery write failure')
                assert copy.read_bytes() == original
                attempt('Discard', 'file')
                assert copy.read_bytes() == original
                checked('locked_checkpoint_keeps_previous_copy_and_editor_open_on_exit_failure')
            finally:
                kernel.CloseHandle(handle)
            control('populationRetryRecovery').click()
            checkpoint()
            checked('recovery_write_retry_keeps_all_current_drafts')
            begin_exit('Discard', 'file', lambda: verify_discard())
            return
        raise AssertionError('Unknown recovery phase: ' + case)
    except Exception:
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
        stage('failed')


def population_recovery_initialized(_args):
    _population_recovery_handler.disconnect()
    QtCore.QTimer.singleShot(0, run_population_recovery_smoke)


_population_recovery_handler = editor.EditorEventBusHandler()
_population_recovery_handler.connect()
_population_recovery_handler.add_callback('NotifyEditorInitialized', population_recovery_initialized)
