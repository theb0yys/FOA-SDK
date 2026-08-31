#
# Copyright (c) Contributors to the Open 3D Engine Project.
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

TEST_MODULE = Path(__file__).resolve().parents[4] / "Installer/Tests/WindowsControlPanel/test_windows_control_panel.py"
SPEC = importlib.util.spec_from_file_location("foa_installer_windows_control_panel_tests", TEST_MODULE)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load Windows Control Panel tests: {TEST_MODULE}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
WindowsControlPanelTests = MODULE.WindowsControlPanelTests
