# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

TEST_MODULE = (
    Path(__file__).resolve().parents[4]
    / "Installer"
    / "Tests"
    / "SuitePackageResolver"
    / "test_production_catalog.py"
)
SPEC = importlib.util.spec_from_file_location(
    "foa_installer_production_catalog_tests", TEST_MODULE
)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load installer production catalogue tests: {TEST_MODULE}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)

ProductionCatalogTests = MODULE.ProductionCatalogTests
