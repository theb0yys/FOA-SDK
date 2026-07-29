#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations
import tempfile
from pathlib import Path
from foa_3d_preview_viewport import generate_fixture, PreviewViewportError

def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        result = generate_fixture(Path(tmp) / "fixture")
        if result["ViewportEntryCount"] < 2:
            raise PreviewViewportError("fixture did not produce expected viewport entries")
    print("FoA 3D preview viewport boundary passed.")
    return 0

if __name__ == "__main__": raise SystemExit(main())
