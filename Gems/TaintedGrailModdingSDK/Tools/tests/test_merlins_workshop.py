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
    / "Plugins/Integrations/MerlinsWorkshop/Tests/test_merlins_workshop.py"
)


def load_tests(
    loader: unittest.TestLoader,
    tests: unittest.TestSuite,
    pattern: str | None,
) -> unittest.TestSuite:
    del tests, pattern
    spec = importlib.util.spec_from_file_location(
        "merlins_workshop_package_tests",
        PACKAGE_TEST,
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load MerlinsWorkshop package tests.")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return loader.loadTestsFromTestCase(module.MerlinsWorkshopTests)
