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
    / "SuitePackageResolver"
    / "test_suite_package_resolver.py"
)
SPEC = importlib.util.spec_from_file_location(
    "foa_installer_suite_package_resolver_tests", TEST_MODULE
)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load installer resolver tests: {TEST_MODULE}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)

ResolverTests = MODULE.ResolverTests
