#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Compatibility entry point for the reviewed Suite Wizard receipt host.

The former quick host silently accepted acknowledgements, synthesized confirmer identity
and time, duplicated receipt publication, and presented receipt preparation as installation.
That behavior is intentionally removed. All callers now enter the explicit review,
acknowledgement, confirmation, and canonical receipt flow owned by the reviewed host.
"""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Sequence

HOST_SOURCE = Path(__file__).resolve().parents[2] / "Host" / "Source"
if str(HOST_SOURCE) not in sys.path:
    sys.path.insert(0, str(HOST_SOURCE))

from suite_wizard_receipt_host import main as reviewed_main  # noqa: E402


def main(argv: Sequence[str] | None = None) -> int:
    """Delegate without changing, accepting, or inventing review state."""
    return reviewed_main(argv)


if __name__ == "__main__":
    raise SystemExit(main())
