#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import hashlib
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "validation_receipt.py"
SPEC = importlib.util.spec_from_file_location("tg_validation_receipt_streaming", SCRIPT)
assert SPEC and SPEC.loader
receipt = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = receipt
SPEC.loader.exec_module(receipt)


class ValidationReceiptStreamingTests(unittest.TestCase):
    def test_process_streams_distinct_stdout_and_stderr_with_incremental_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stdout = root / "stdout.log"
            stderr = root / "stderr.log"
            result = receipt.stream_gate_process(
                (
                    sys.executable,
                    "-c",
                    (
                        "import sys; "
                        "sys.stdout.buffer.write(b'out-1\\nout-2\\n'); "
                        "sys.stderr.buffer.write(b'err-1\\n')"
                    ),
                ),
                root,
                stdout,
                stderr,
                maximum_stream_bytes=1024,
            )

            self.assertEqual(result.exit_code, 0)
            self.assertFalse(result.limit_exceeded)
            self.assertFalse(result.stdout.truncated)
            self.assertFalse(result.stderr.truncated)
            self.assertEqual(stdout.read_bytes(), b"out-1\nout-2\n")
            self.assertEqual(stderr.read_bytes(), b"err-1\n")
            self.assertEqual(
                result.stdout.sha256,
                "sha256:" + hashlib.sha256(stdout.read_bytes()).hexdigest(),
            )
            self.assertEqual(
                result.stderr.sha256,
                "sha256:" + hashlib.sha256(stderr.read_bytes()).hexdigest(),
            )
            self.assertEqual(result.stdout.byte_count, stdout.stat().st_size)
            self.assertEqual(result.stderr.byte_count, stderr.stat().st_size)

    def test_stream_limit_terminates_gate_and_keeps_only_hashed_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stdout = root / "stdout.log"
            stderr = root / "stderr.log"
            result = receipt.stream_gate_process(
                (
                    sys.executable,
                    "-c",
                    (
                        "import os, sys; "
                        "chunk=b'x'*4096; "
                        "[(sys.stdout.buffer.write(chunk), sys.stdout.buffer.flush()) "
                        "for _ in range(256)]"
                    ),
                ),
                root,
                stdout,
                stderr,
                maximum_stream_bytes=64,
            )

            self.assertTrue(result.limit_exceeded)
            self.assertNotEqual(result.exit_code, 0)
            self.assertTrue(result.stdout.truncated)
            self.assertEqual(result.stdout.byte_count, 64)
            self.assertEqual(stdout.stat().st_size, 64)
            self.assertEqual(
                result.stdout.sha256,
                "sha256:" + hashlib.sha256(stdout.read_bytes()).hexdigest(),
            )
            self.assertLessEqual(result.stderr.byte_count, 64)

    def test_existing_log_destination_is_never_overwritten(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            logs = root / receipt.LOG_DIRECTORY
            logs.mkdir()
            existing = logs / receipt.safe_log_name("git-diff-check", "stdout")
            existing.write_bytes(b"existing evidence")

            with self.assertRaisesRegex(
                receipt.ValidationReceiptError,
                "will not be overwritten",
            ):
                receipt.prepare_log_destination(
                    root,
                    "git-diff-check",
                    "stdout",
                )
            self.assertEqual(existing.read_bytes(), b"existing evidence")


if __name__ == "__main__":
    unittest.main()
