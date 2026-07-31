#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve().parents[1] / "run_aztest_without_gtest_xml.py"
SPEC = importlib.util.spec_from_file_location("tg_run_aztest_without_gtest_xml", SCRIPT_PATH)
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = runner
SPEC.loader.exec_module(runner)


class AzTestWithoutGtestXmlTests(unittest.TestCase):
    def test_filters_only_google_test_xml_output(self) -> None:
        self.assertEqual(
            runner.filter_arguments(
                (
                    "AzTestRunner.exe",
                    "CanonicalInterchange.Tests.dll",
                    "AzRunUnitTests",
                    "--gtest_output=xml:D:/build/Testing/Gtest/result.xml",
                    "--gtest_filter=-*SUITE_smoke*",
                    "--gtest_output=json:result.json",
                )
            ),
            (
                "AzTestRunner.exe",
                "CanonicalInterchange.Tests.dll",
                "AzRunUnitTests",
                "--gtest_filter=-*SUITE_smoke*",
                "--gtest_output=json:result.json",
            ),
        )

    def test_filters_google_test_xml_output_without_path(self) -> None:
        self.assertEqual(
            runner.filter_arguments(("AzTestRunner.exe", "--gtest_output=xml")),
            ("AzTestRunner.exe",),
        )

    def test_requires_command(self) -> None:
        self.assertEqual(runner.main(["--gtest_output=xml:D:/result.xml"]), 2)

    def test_invokes_command_without_xml_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            arguments_file = Path(temporary) / "arguments.json"
            recorder = Path(temporary) / "record_arguments.py"
            recorder.write_text(
                textwrap.dedent(
                    """
                    from __future__ import annotations

                    import json
                    import sys
                    from pathlib import Path

                    Path(sys.argv[1]).write_text(json.dumps(sys.argv[2:]), encoding="utf-8")
                    """
                ),
                encoding="utf-8",
            )

            result = runner.main(
                (
                    sys.executable,
                    str(recorder),
                    str(arguments_file),
                    "AzRunUnitTests",
                    "--gtest_output=xml:D:/build/Testing/Gtest/result.xml",
                    "--gtest_filter=QuestDefinitionContractTests.*",
                )
            )

            self.assertEqual(result, 0)
            self.assertEqual(
                json.loads(arguments_file.read_text(encoding="utf-8")),
                ["AzRunUnitTests", "--gtest_filter=QuestDefinitionContractTests.*"],
            )

    def test_returns_command_exit_code(self) -> None:
        self.assertEqual(
            runner.main(
                (
                    sys.executable,
                    "-c",
                    "import sys; sys.exit(7)",
                    "--gtest_output=xml:D:/build/Testing/Gtest/result.xml",
                )
            ),
            7,
        )


if __name__ == "__main__":
    unittest.main()
