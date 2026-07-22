#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

TEST_MODULE = (
    Path(__file__).resolve().parents[4]
    / "Installer"
    / "Tests"
    / "SuiteWizardCatalog"
    / "test_wizard_catalog.py"
)
SPEC = importlib.util.spec_from_file_location(
    "foa_installer_suite_wizard_catalog_tests", TEST_MODULE
)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load Suite Wizard catalogue tests: {TEST_MODULE}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)

SuiteWizardCatalogTests = MODULE.SuiteWizardCatalogTests
