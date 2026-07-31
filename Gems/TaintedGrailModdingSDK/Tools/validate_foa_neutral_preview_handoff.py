#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import shutil
import tempfile
from pathlib import Path

import foa_neutral_preview_handoff as handoff


def main() -> int:
    temp_root = Path(tempfile.mkdtemp(prefix="foa-neutral-preview-handoff-validator-"))
    try:
        manifest = handoff.generate_fixture(temp_root, replace=True)
        document = handoff.verify_handoff(Path(manifest["HandoffPath"]))
        if document["PreviewStageStatus"]["FunctionCompleteAllowed"] is not False:
            raise RuntimeError("FunctionCompleteAllowed escalated.")
        if document["CoordinateConversionEvidence"]["ConversionOperationPerformed"] is not False:
            raise RuntimeError("Coordinate conversion evidence escalated.")
        if not document["SourceDependencies"]:
            raise RuntimeError("Source dependency collection is missing.")
    finally:
        shutil.rmtree(temp_root, ignore_errors=True)
    print("FoA neutral preview handoff boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
