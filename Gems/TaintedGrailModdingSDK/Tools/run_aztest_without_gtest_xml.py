#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Run AzTestRunner while dropping CTest-injected GoogleTest XML output."""

from __future__ import annotations

import subprocess
import sys
from typing import Sequence


GTEST_XML_OUTPUT = "--gtest_output=xml"


def is_gtest_xml_output_argument(argument: str) -> bool:
    return argument == GTEST_XML_OUTPUT or argument.startswith(f"{GTEST_XML_OUTPUT}:")


def filter_arguments(arguments: Sequence[str]) -> tuple[str, ...]:
    return tuple(
        argument
        for argument in arguments
        if not is_gtest_xml_output_argument(argument)
    )


def main(argv: Sequence[str] | None = None) -> int:
    arguments = tuple(sys.argv[1:] if argv is None else argv)
    command = filter_arguments(arguments)
    if not command:
        print("run_aztest_without_gtest_xml.py requires an AzTestRunner command.", file=sys.stderr)
        return 2

    try:
        return subprocess.run(command, check=False).returncode
    except FileNotFoundError as exc:
        print(f"Unable to start AzTestRunner command: {exc}", file=sys.stderr)
        return 127


if __name__ == "__main__":
    raise SystemExit(main())
