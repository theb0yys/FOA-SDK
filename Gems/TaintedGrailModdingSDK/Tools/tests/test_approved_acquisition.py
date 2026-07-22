#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

PACKAGE_TEST = (
    Path(__file__).resolve().parents[4]
    / "Plugins/Integrations/ApprovedAcquisition/Tests/test_approved_acquisition.py"
)
SPEC = importlib.util.spec_from_file_location(
    "approved_acquisition_package_tests", PACKAGE_TEST
)
assert SPEC and SPEC.loader
package_tests = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = package_tests
SPEC.loader.exec_module(package_tests)


def load_tests(
    loader: unittest.TestLoader,
    tests: unittest.TestSuite,
    pattern: str | None,
) -> unittest.TestSuite:
    del tests, pattern
    return loader.loadTestsFromTestCase(package_tests.ApprovedAcquisitionTests)
