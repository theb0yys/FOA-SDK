# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Exercise Pack Manager drafts through the compiled Editor's real shutdown path.

Run without --autotest_mode, using an isolated synthetic sdkqa.pack-editor-exit
workspace. FOA_SDK_PACK_EXIT_CASE selects save, discard, clean, pristine,
new-save, new-discard, or reopen. The outer runner must also verify process exit
code zero; an in-process result alone does not prove completed shutdown.
"""
import ctypes
import json
import os
from pathlib import Path
import time
import traceback

import azlmbr.editor as editor
import azlmbr.legacy.general as general
from PySide6 import QtCore, QtGui, QtTest, QtWidgets
from shiboken6 import isValid


def run():
    assert _editor_initialized, 'Shutdown acceptance must run after Editor initialization'
    output = Path(os.environ['FOA_SDK_PACK_RESULT'])
    workspace = Path(os.environ['FOA_SDK_PACK_WORKSPACE'])
    case = os.environ['FOA_SDK_PACK_EXIT_CASE']
    assert case in ('save', 'discard', 'clean', 'pristine', 'new-save', 'new-discard', 'reopen')
    result = {'status': 'PARTIAL', 'case': case, 'checks': [], 'transition_seconds': [],
              'about_to_quit': False, 'editor_initialized': _editor_initialized}
    app = QtWidgets.QApplication.instance()
    keep = []
    root = None
    main = None

    def stage(name):
        result['stage'] = name
        output.write_text(json.dumps(result, indent=2), encoding='utf-8')

    def fail():
        result['status'] = 'FAILED'
        result['error'] = traceback.format_exc()
        stage('failed')

    def control(name):
        values = app.allWidgets()
        keep.extend(values)
        return next(value for value in values if isValid(value) and value.objectName() == name)

    def button(text):
        return next(value for value in root.findChildren(QtWidgets.QPushButton) if value.text() == text)

    def dirty(expected=True):
        assert (control('packDraftStatus').text() == 'Unsaved changes') == expected

    def snapshot():
        return ([value.text() for value in root.findChildren(QtWidgets.QLineEdit)],
                [value.toPlainText() for value in root.findChildren(QtWidgets.QPlainTextEdit)],
                [value.currentText() for value in root.findChildren(QtWidgets.QComboBox)
                 if value.objectName() != 'packSavedMods'])

    def manifest(path):
        data = json.loads(path.read_text(encoding='utf-8'))
        return data.get('ClassData', data)

    def save():
        button('Save mod').click()
        assert control('packStatus').text() == 'Mod saved. You can start authoring.'
        dirty(False)

    def window_close():
        # The main-window titlebar lives in O3DE's window decoration wrapper.
        buttons = main.window().findChildren(QtWidgets.QToolButton, 'closeButton')
        keep.extend(buttons)
        candidates = []
        for value in buttons:
            if not value.isVisible() or value.visibleRegion().isEmpty():
                continue
            parent = value.parentWidget()
            while parent and not isinstance(parent, QtWidgets.QDockWidget):
                parent = parent.parentWidget()
            if parent is None:
                candidates.append(value)
        assert len(candidates) == 1, 'Could not identify the main-window Close button'
        QtTest.QTest.mouseClick(candidates[0], QtCore.Qt.LeftButton)

    def file_exit():
        bar = main.menuBar()
        actions = bar.actions()
        keep.extend([bar, *actions])
        file_action = next(action for action in actions if action.text().replace('&', '') == 'File')
        menu = file_action.menu()
        keep.append(menu)
        actions = menu.actions()
        keep.extend(actions)
        action = next(action for action in actions if action.objectName() == 'o3de.action.editor.exit')
        assert action.isEnabled()
        action.trigger()

    def undock():
        dock = root.parentWidget()
        while dock and not isinstance(dock, QtWidgets.QDockWidget):
            dock = dock.parentWidget()
        assert dock and dock.widget() == root
        parent = dock
        while parent:
            if isinstance(parent, QtWidgets.QDockWidget) and parent.isFloating():
                return
            parent = parent.parentWidget()
        parent = dock.parentWidget()
        while parent and not isinstance(parent, QtWidgets.QTabWidget):
            parent = parent.parentWidget()
        if parent and parent.indexOf(dock) >= 0:
            surface = parent.tabBar()
            point = surface.tabRect(parent.indexOf(dock)).center()
        else:
            surface = dock.titleBarWidget()
            point = surface.rect().center()
        selected, errors = [], []
        timer = QtCore.QTimer()

        def choose():
            values = app.allWidgets()
            keep.extend(values)
            menu = next((value for value in values if isValid(value)
                         and isinstance(value, QtWidgets.QMenu) and value.isVisible()), None)
            if menu is None:
                return
            timer.stop()
            try:
                action = next(action for action in menu.actions()
                              if action.text().startswith('Undock ')
                              and action.text() != 'Undock Tab Group' and action.isEnabled())
                selected.append(action.text())
                QtTest.QTest.mouseClick(menu, QtCore.Qt.LeftButton, pos=menu.actionGeometry(action).center())
            except Exception:
                errors.append(traceback.format_exc())
                menu.close()

        timer.timeout.connect(choose)
        timer.start(25)
        event = QtGui.QContextMenuEvent(QtGui.QContextMenuEvent.Mouse, point, surface.mapToGlobal(point))
        app.sendEvent(surface, event)
        timer.stop()
        assert selected and not errors, errors
        QtTest.QTest.qWait(150)
        parent = dock
        while parent and not (isinstance(parent, QtWidgets.QDockWidget) and parent.isFloating()):
            parent = parent.parentWidget()
        assert parent is not None, 'Pane did not enter a floating window'

    def attempt(choice, route, accepted=False, capture=False):
        form = snapshot()
        active = control('packActiveSummary').text()
        seen, errors = [], []
        started = time.monotonic()
        timer = QtCore.QTimer()
        keep.append(timer)

        def answer():
            values = app.allWidgets()
            keep.extend(values)
            prompt = next((value for value in values if isValid(value)
                           and isinstance(value, QtWidgets.QMessageBox) and value.isVisible()
                           and value.objectName() == 'packUnsavedChangesDialog'), None)
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
                    assert prompt.grab().save(str(output.with_suffix('.prompt.png')))
                if choice == 'Escape':
                    QtTest.QTest.keyClick(prompt, QtCore.Qt.Key_Escape)
                elif choice == 'Close':
                    prompt.close()
                else:
                    assert choice is not None, 'Clean draft unexpectedly prompted'
                    prompt.button(getattr(buttons, choice)).click()
            except Exception:
                errors.append(traceback.format_exc())
                prompt.reject()

        timer.timeout.connect(answer)
        timer.start(25)
        try:
            {'file': file_exit, 'window': window_close, 'python': general.exit}[route]()
            # Python exit is deliberately posted by the host; let it reach the close handler.
            # Accepted exits use the synchronous File or window route and return to the host.
            if not accepted:
                QtTest.QTest.qWait(350)
        finally:
            timer.stop()
        elapsed = time.monotonic() - started
        result['transition_seconds'].append(round(elapsed, 4))
        assert not errors, errors
        assert len(seen) == (0 if choice is None else 1), (route, choice, seen)
        assert elapsed < 5, 'Synthetic shutdown interaction exceeded five seconds'
        if accepted:
            assert not isValid(root), 'Accepted shutdown did not destroy Pack Manager'
        else:
            assert isValid(main) and main.isVisible(), 'Cancelled shutdown closed the Editor'
            assert isValid(root) and root.isVisible(), 'Cancelled shutdown lost the Pack Manager pane'
            assert snapshot() == form, 'Cancelled shutdown changed the full draft'
            assert control('packActiveSummary').text() == active
            dirty()
            result['checks'].append(route + '_' + choice + '_keeps_editor_and_full_draft')
        stage('exit_accepted' if accepted else 'exit_cancelled')

    def begin_exit(choice, route, expected_path, expected_bytes, expected_name):
        # Return from the initial --runpython command before allowing application destruction.
        # Keep callbacks alive until the real Qt shutdown; never call quit/exit_no_prompt.
        def verify_quit():
            try:
                if expected_path:
                    if expected_bytes is not None:
                        assert expected_path.read_bytes() == expected_bytes
                    assert manifest(expected_path)['DisplayName'] == expected_name
                result['about_to_quit'] = True
                stage('about_to_quit')
            except Exception:
                fail()

        def finish():
            try:
                attempt(choice, route, accepted=True)
                if expected_path:
                    if expected_bytes is not None:
                        assert expected_path.read_bytes() == expected_bytes
                    assert manifest(expected_path)['DisplayName'] == expected_name
                result['checks'].append(case + '_accepted_exit_has_expected_saved_bytes')
                result['status'] = 'PASSED'
                stage('exit_accepted')
            except Exception:
                fail()

        app.aboutToQuit.connect(verify_quit)
        keep.extend((verify_quit, finish))
        QtCore.QTimer.singleShot(100, finish)
        stage('exit_scheduled')

    try:
        stage('validating_fixture')
        assert not any('autotest_mode' in argument for argument in app.arguments())
        assert workspace.resolve() == (Path(os.environ['LOCALAPPDATA']) /
                                       'FOA-SDK/Workspace/foa-sdk.tgworkspace.json').resolve()
        fixture = json.loads(workspace.read_text(encoding='utf-8'))
        assert fixture['WorkspaceId'] == 'sdkqa.pack-editor-exit'
        assert Path(fixture['RootPath']).resolve() == workspace.parent.resolve()
        if case != 'reopen':
            assert not (workspace.parent / 'Packs').exists(), 'Use a fresh synthetic workspace'
        general.idle_enable(True)
        QtTest.QTest.qWait(3000)
        general.open_pane('Tainted Grail SDK Status')
        QtTest.QTest.qWait(500)
        values = app.allWidgets()
        keep.extend(values)
        next(value for value in values if isinstance(value, QtWidgets.QPushButton)
             and value.text() == 'Show advanced details').click()
        details = next(value for value in app.allWidgets()
                       if isinstance(value, QtWidgets.QPlainTextEdit)
                       and value.toPlainText().startswith('Workspace file:'))
        assert workspace.as_posix() in details.toPlainText().replace('\\', '/')
        general.open_pane('Tainted Grail Pack Manager')
        QtTest.QTest.qWait(500)
        root = control('TaintedGrailPackManager')
        main = control('MainWindow')
        assert isinstance(main, QtWidgets.QMainWindow) and main.isVisible()

        if case == 'reopen':
            saved = Path(os.environ['FOA_SDK_PACK_EXPECTED_PATH'])
            name = os.environ['FOA_SDK_PACK_EXPECTED_NAME']
            # Select and reopen persisted data through the normal UI after a fresh process.
            combo = control('packSavedMods')
            combo.setCurrentIndex(next(index for index in range(combo.count())
                                       if Path(combo.itemData(index)).resolve() == saved.resolve()))
            button('Open selected').click()
            assert control('packDisplayName').text() == name
            assert manifest(saved)['DisplayName'] == name
            dirty(False)
            result['checks'].append('fresh_editor_reopens_expected_saved_manifest')
            begin_exit(None, 'file', saved, saved.read_bytes(), name)
            return

        button('New mod').click()
        if case == 'pristine':
            dirty(False)
            begin_exit(None, 'window', None, None, None)
            return
        control('packDisplayName').setText('Original')
        control('packOwner').setText('sdkqa')
        save()
        original = workspace.parent / 'Packs/sdkqa.original/pack.tgpack.json'
        original_bytes = original.read_bytes()

        if case == 'clean':
            begin_exit(None, 'file', original, original_bytes, 'Original')
            return
        if case.startswith('new-'):
            button('New mod').click()
            control('packDisplayName').setText('New exit draft')
        else:
            control('packDisplayName').setText('Saved exit draft')

        if case == 'save':
            button('Show advanced manifest').click()
            control('packVersion').setText('2.3.4')
            result['checks'].append('dirty_saved_and_advanced_fields_prepared')
            for route in ('file', 'window', 'python'):
                for choice in ('Cancel', 'Escape', 'Close'):
                    attempt(choice, route, capture=(route == 'file' and choice == 'Cancel'))
                    assert original.read_bytes() == original_bytes
            control('packVersion').setText('invalid')
            attempt('Save', 'file')
            assert original.read_bytes() == original_bytes
            control('packVersion').setText('2.3.4')
            undock()
            attempt('Cancel', 'window')
            kernel = ctypes.WinDLL('kernel32', use_last_error=True)
            kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                          ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
            kernel.CreateFileW.restype = ctypes.c_void_p
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            handle = kernel.CreateFileW(str(original), 0x80000000, 1, None, 3, 0x80, None)
            assert handle not in (None, ctypes.c_void_p(-1).value), ctypes.get_last_error()
            try:
                attempt('Save', 'file')
                assert original.read_bytes() == original_bytes
            finally:
                kernel.CloseHandle(handle)
            result['checks'].append('real_write_failure_keeps_editor_open_and_saved_bytes_unchanged')
            assert root.grab().save(str(output.with_suffix('.draft.png')))
            begin_exit('Save', 'file', original, None, 'Saved exit draft')
        elif case == 'discard':
            undock()
            begin_exit('Discard', 'window', original, original_bytes, 'Original')
        elif case == 'new-save':
            attempt('Save', 'file')
            assert control('packOwner').text() == ''
            assert control('packStatus').text() == 'Enter an author or namespace.'
            control('packOwner').setText('sdkqa')
            created = workspace.parent / 'Packs/sdkqa.new-exit-draft/pack.tgpack.json'
            begin_exit('Save', 'window', created, None, 'New exit draft')
        elif case == 'new-discard':
            control('packOwner').setText('sdkqa')
            begin_exit('Discard', 'file', original, original_bytes, 'Original')
    except Exception:
        fail()


# --runpython executes inside InitInstance, before the host enters its main loop.
# Fixed delays can still fire in a nested startup loop; wait for the actual host event.
_editor_initialized = False


def on_editor_initialized(_args):
    global _editor_initialized
    _editor_initialized = True
    _initialization_handler.disconnect()
    QtCore.QTimer.singleShot(0, run)


_initialization_handler = editor.EditorEventBusHandler()
_initialization_handler.connect()
_initialization_handler.add_callback('NotifyEditorInitialized', on_editor_initialized)
