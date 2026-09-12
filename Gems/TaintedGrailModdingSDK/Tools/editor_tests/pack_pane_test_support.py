# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Pinned Windows Editor test setup and actual pane action helpers; no product API."""
import ctypes
import os
import time
import traceback

from PySide6 import QtCore, QtGui, QtTest, QtWidgets
from shiboken6 import isValid


def loaded_library(name):
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
    kernel.GetModuleHandleW.restype = ctypes.c_void_p
    handle = kernel.GetModuleHandleW(name)
    assert handle, 'Expected already-loaded host module: ' + name
    return ctypes.PyDLL(name, handle=handle)


def open_default_pack():
    # QtViewPaneManager::OpenMode::UseDefaultState is not Python-reflected.
    # These public exports and QString's three-word storage match the pinned
    # Windows x64 Qt 6.10.2 host (qstring.h / qarraydatapointer.h).
    # This bridge only opens the registered pane; it never answers a close guard.
    assert os.name == 'nt'
    assert ctypes.sizeof(ctypes.c_void_p) == 8 and QtCore.qVersion() == '6.10.2'
    qt = loaded_library('Qt6Core.dll')
    host = loaded_library('EditorCore.dll')
    class QStringStorage(ctypes.Structure):
        _fields_ = [('data', ctypes.c_void_p), ('text', ctypes.c_void_p), ('size', ctypes.c_ssize_t)]
    ctor = getattr(qt, '??0QString@@QEAA@PEBVQChar@@_J@Z')
    ctor.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_ssize_t]
    ctor.restype = None
    dtor = getattr(qt, '??1QString@@QEAA@XZ')
    dtor.argtypes = [ctypes.c_void_p]
    dtor.restype = None
    instance = getattr(host, '?instance@QtViewPaneManager@@SAPEAV1@XZ')
    instance.argtypes = []
    instance.restype = ctypes.c_void_p
    open_pane = getattr(host, '?OpenPane@QtViewPaneManager@@QEAAPEBUQtViewPane@@AEBVQString@@V?$QFlags@W4OpenMode@QtViewPane@@@@@Z')
    open_pane.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
    open_pane.restype = ctypes.c_void_p
    name = 'Tainted Grail Pack Manager'
    string = QStringStorage()
    ctor(ctypes.byref(string), name, len(name))
    try:
        manager = instance()
        assert manager and string.size == len(name)
        assert ctypes.wstring_at(string.text, string.size) == name
        assert open_pane(manager, ctypes.byref(string), 1), 'UseDefaultState OpenPane failed'
    finally:
        dtor(ctypes.byref(string))

def pane_dock(root):
    parent = root.parentWidget()
    while parent and not isinstance(parent, QtWidgets.QDockWidget):
        parent = parent.parentWidget()
    assert parent is not None and parent.widget() == root
    return parent


def floating_container(root):
    parent = pane_dock(root)
    while parent:
        if isinstance(parent, QtWidgets.QDockWidget) and parent.isFloating():
            return parent
        parent = parent.parentWidget()
    return None


def pane_menu_action(root, verb, result, keep):
    app = QtWidgets.QApplication.instance()
    dock = pane_dock(root)
    parent = dock.parentWidget()
    while parent and not isinstance(parent, QtWidgets.QTabWidget):
        parent = parent.parentWidget()
    if parent and parent.indexOf(dock) >= 0:
        index = parent.indexOf(dock)
        surface = parent.tabBar()
        expected = verb + ' ' + parent.tabText(index)
    else:
        title = dock.titleBarWidget()
        bars = [w for w in title.findChildren(QtWidgets.QTabBar) if w.isVisible() and w.count() > 0]
        assert len(bars) == 1, 'Expected the dock titlebar tab, not its outer container'
        surface, index = bars[0], 0
        expected = verb + ' ' + dock.windowTitle()
    point = surface.tabRect(index).center()
    selected, errors = [], []
    timer = QtCore.QTimer()
    started = time.monotonic()

    def choose():
        values = app.allWidgets()
        keep.extend(values)
        menus = [w for w in values if isValid(w) and isinstance(w, QtWidgets.QMenu) and w.isVisible()]
        if not menus:
            if time.monotonic() - started > 3:
                errors.append('Pane context menu did not open')
                timer.stop()
            return
        timer.stop()
        menu = menus[-1]
        try:
            actions = [a for a in menu.actions() if a.text() == expected and a.isEnabled()]
            assert len(actions) == 1, (expected, [a.text() for a in menu.actions()])
            selected.append(actions[0].text())
            QtTest.QTest.mouseClick(menu, QtCore.Qt.LeftButton, pos=menu.actionGeometry(actions[0]).center())
        except Exception:
            errors.append(traceback.format_exc())
            menu.close()

    timer.timeout.connect(choose)
    timer.start(25)
    event = QtGui.QContextMenuEvent(QtGui.QContextMenuEvent.Mouse, point, surface.mapToGlobal(point))
    app.sendEvent(surface, event)
    timer.stop()
    if not errors and not selected:
        # Windows may dismiss native popups on an inactive private desktop.
        # Activate the actual enabled QAction created by that tab's menu event.
        # Do not emit a synthetic close signal or use general.close_pane (Force).
        menus = surface.findChildren(QtWidgets.QMenu)
        actions = [a for menu in menus for a in menu.actions() if a.text() == expected and a.isEnabled()]
        assert len(actions) == 1, (expected, [[a.text() for a in menu.actions()] for menu in menus])
        selected.append(actions[0].text())
        result.setdefault('menu_action_activation', []).append(expected)
        actions[0].trigger()
    assert not errors and selected == [expected], (errors, selected)


def request_titlebar_close(root, result, keep):
    container = floating_container(root)
    if container is None:
        assert pane_dock(root).parentWidget() is not None
        pane_menu_action(root, 'Close', result, keep)
        return
    buttons = container.findChildren(QtWidgets.QToolButton)
    keep.extend(buttons)
    visible = [b for b in buttons if isValid(b) and b.isVisible() and not b.visibleRegion().isEmpty()]
    close = [b for b in visible if b.defaultAction() and b.defaultAction().objectName() == 'closeButton']
    if not close:
        close = [b for b in visible if b.objectName() == 'closeButton'
                 and not isinstance(b.parentWidget(), QtWidgets.QTabBar)]
    assert len(close) == 1 and close[0].isEnabled(), 'Expected one actual floating titlebar close control'
    QtTest.QTest.mouseClick(close[0], QtCore.Qt.LeftButton)
