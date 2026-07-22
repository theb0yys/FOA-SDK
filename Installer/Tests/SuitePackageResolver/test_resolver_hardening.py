# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TEST_ROOT = Path(__file__).resolve().parent
SOURCE_ROOT = TEST_ROOT.parents[1] / "SuiteWizard" / "Resolver" / "Source"
if str(TEST_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_ROOT))

from test_suite_package_resolver import Fixture, package, suite  # noqa: E402


class ResolverHardeningTests(unittest.TestCase):
    def test_internal_core_refuses_direct_execution(self) -> None:
        completed = subprocess.run(
            [sys.executable, str(SOURCE_ROOT / "_resolver_core.py")],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(completed.returncode, 2)
        self.assertIn("internal import-only module", completed.stderr)
        self.assertEqual(completed.stdout, "")

    def test_unselected_optional_suite_entry_still_requires_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = Fixture(Path(temporary))
            fixture.add("Core", package("foa.core"))
            fixture.set_suite(
                suite(
                    [
                        ("foa.core", "required", 0),
                        ("foa.optional-missing", "optional", 1),
                    ]
                )
            )
            with self.assertRaisesRegex(
                RuntimeError,
                "without package.json: foa.optional-missing",
            ):
                fixture.resolve()

    def test_unknown_package_status_cannot_bypass_wrapper_validation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = Fixture(Path(temporary))
            fixture.add("Typo", package("foa.typo", status="supportd"))
            fixture.set_suite(suite([("foa.typo", "required", 0)]))
            with self.assertRaisesRegex(RuntimeError, "must be one of"):
                fixture.resolve()


if __name__ == "__main__":
    unittest.main()
