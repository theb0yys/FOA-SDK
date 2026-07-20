#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Load the production exact-head coordinator for colocated contract tests."""

from pathlib import Path

_TARGET = Path(__file__).resolve().parents[1] / "developer_preview_verification.py"
_SOURCE = _TARGET.read_text(encoding="utf-8")
exec(compile(_SOURCE, str(_TARGET), "exec"), globals(), globals())
