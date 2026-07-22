# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

TEST_MODULE = Path(__file__).resolve().parents[4] / "Installer/Tests/AdmissionBoundExecutionHandoff/test_admission_bound_execution_handoff.py"
SPEC = importlib.util.spec_from_file_location("foa_installer_admission_bound_execution_handoff_tests", TEST_MODULE)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load admission-bound execution handoff tests: {TEST_MODULE}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
AdmissionBoundExecutionHandoffTests = MODULE.AdmissionBoundExecutionHandoffTests
