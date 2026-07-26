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

MODULE_PATH = Path(__file__).resolve().parents[1] / "validate_canonical_interchange_compiled_tests.py"
SPEC = importlib.util.spec_from_file_location("validate_canonical_interchange_compiled_tests", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load validator: {MODULE_PATH}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class CanonicalInterchangeCompiledTestValidatorTests(unittest.TestCase):
    def test_current_tree_has_core_only_canonical_target(self) -> None:
        MODULE.validate(Path(__file__).resolve().parents[4])
