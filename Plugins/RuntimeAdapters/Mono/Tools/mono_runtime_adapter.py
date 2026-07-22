#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Deterministic, non-executable contracts for the FOA Mono runtime adapter."""
from __future__ import annotations
import argparse
import sys
from pathlib import Path
from typing import Sequence
TOOLS_ROOT = Path(__file__).resolve().parent
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))
from _mono_contract import *
from _mono_gate import evaluate_execution_gate, validate_gate
from _mono_result import build_result, validate_result


def write_json(document: dict[str, object]) -> None:
    sys.stdout.buffer.write(canonical_json(document))


def profile_from(args: argparse.Namespace) -> dict[str, object]:
    return {key: getattr(args, key) for key in PROFILE}


def parse_arguments(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("describe")
    commands.add_parser("plan-build")
    commands.add_parser("verify-package")
    gate = commands.add_parser("evaluate-gate")
    gate.add_argument("--binary-sha256", required=True)
    gate.add_argument("--binary-bytes", required=True, type=int)
    for key, value in PROFILE.items():
        gate.add_argument("--" + key.replace("_", "-"), default=value)
    gate.add_argument("--identity-evidence-id", action="append", required=True)
    gate.add_argument("--parity-evidence-id", action="append", required=True)
    gate.add_argument("--confirmation-receipt-sha256", required=True)
    gate.add_argument("--requested-by", required=True)
    gate.add_argument("--requested-at-utc", required=True)
    gate.add_argument("--expires-at-utc", required=True)
    verify = commands.add_parser("verify-result")
    verify.add_argument("--gate", type=Path, required=True)
    verify.add_argument("--result", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_arguments(argv)
    try:
        if args.command == "describe":
            write_json(validate_manifest(load_json(PACKAGE_ROOT / "mono-adapter.json")))
        elif args.command == "plan-build":
            write_json(build_plan())
        elif args.command == "verify-package":
            validate_package(); write_json({"status": "verified", "package_id": PACKAGE_ID})
        elif args.command == "evaluate-gate":
            write_json(evaluate_execution_gate(
                binary_sha256=args.binary_sha256, binary_bytes=args.binary_bytes,
                profile=profile_from(args), identity_evidence_ids=args.identity_evidence_id,
                parity_evidence_ids=args.parity_evidence_id,
                confirmation_receipt_sha256=args.confirmation_receipt_sha256,
                requested_by=args.requested_by, requested_at_utc=args.requested_at_utc,
                expires_at_utc=args.expires_at_utc,
            ))
        else:
            gate = load_json(args.gate); result = load_json(args.result)
            validate_result(result, gate); write_json({"status": "verified", "result_sha256": result["result_sha256"]})
    except (MonoAdapterError, OSError) as exc:
        print(f"Mono runtime-adapter validation failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
