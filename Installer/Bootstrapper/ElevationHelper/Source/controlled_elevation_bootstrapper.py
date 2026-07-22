#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Controlled elevated bootstrapper for one authenticated exact execution request."""
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path
from typing import Mapping, Sequence

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
for root in (
    INSTALLER_ROOT / "Bootstrapper" / "ElevationHelper" / "Source",
    INSTALLER_ROOT / "Bootstrapper" / "Security" / "Source",
):
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))

from capability_elevation_helper import ElevationError, validate_bootstrap_request  # noqa: E402
from execution_security import (  # noqa: E402
    ExecutionSecurityError,
    canonical_json,
    claim_once,
    copy_reviewed_bundle,
    file_sha256,
    publish_bytes_create_once,
    read_strict_json_file,
    remove_tree,
    resolve_reviewed_path,
    run_bounded_process,
    seal_authenticated_record,
    sha256,
    utc_datetime,
    verify_sealed_record,
)

COMPLETION_SCOPE = "package-elevation-bootstrap-completion"
COMPLETION_STATEMENT = (
    "This receipt records one exact authenticated elevated helper execution with an explicit environment, "
    "reviewed support-file bundle, combined bounded output, process-tree timeout enforcement, and no shell."
)


class ControlledBootstrapperError(RuntimeError):
    pass


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ControlledBootstrapperError(f"{label} must be an object.")
    return value


def execute_bootstrap_request(
    request: Mapping[str, object], execution_root: Path, *, authority_key_path: Path,
    claim_root: Path, completed_at_utc: str,
) -> dict[str, object]:
    private_directory: Path | None = None
    try:
        checked = validate_bootstrap_request(request, authority_key_path=authority_key_path)
        completed = utc_datetime(completed_at_utc, "completed_at_utc")
        requested = utc_datetime(checked["requested_at_utc"], "request.requested_at_utc")
        if completed < requested:
            raise ControlledBootstrapperError("Completion time must not precede the elevation request.")
        helper = _object(checked["helper"], "request.helper")
        if helper.get("requires_elevation") is not True or helper.get("role") != "operation-helper":
            raise ControlledBootstrapperError("Bootstrap request target is not a reviewed elevated operation helper.")
        working_directory = resolve_reviewed_path(
            execution_root, str(helper["working_directory"]), file_required=False,
            label="Elevated reviewed working directory",
        )
        private_executable, private_directory = copy_reviewed_bundle(
            Path(execution_root), helper, Path(claim_root), str(checked["request_sha256"])
        )
        actual_hash, executable_size = file_sha256(private_executable)
        if actual_hash != helper["executable_sha256"]:
            raise ControlledBootstrapperError("Elevated private executable hash mismatch.")
    except (ElevationError, ExecutionSecurityError) as exc:
        if private_directory is not None:
            remove_tree(private_directory)
        raise ControlledBootstrapperError(f"Bootstrap request preflight failed: {exc}") from exc

    try:
        try:
            claim = claim_once(
                claim_root, authority_key_path=authority_key_path,
                claim_kind="claim.elevation-bootstrap-request", artifact_sha256=str(checked["request_sha256"]),
                nonce=str(checked["request_reference"]), claimed_at_utc=completed_at_utc,
            )
        except ExecutionSecurityError as exc:
            raise ControlledBootstrapperError(f"Bootstrap request consumption failed: {exc}") from exc
        try:
            outcome = run_bounded_process(
                [str(private_executable), *list(helper["argv"])], cwd=working_directory,
                environment=dict(helper["environment"]), timeout_seconds=int(helper["timeout_seconds"]),
                output_limit_bytes=int(helper["output_limit_bytes"]),
            )
        except (OSError, ExecutionSecurityError) as exc:
            raise ControlledBootstrapperError(f"Elevated reviewed helper execution failed: {exc}") from exc
    finally:
        if private_directory is not None:
            remove_tree(private_directory)

    stdout = bytes(outcome.pop("stdout"))
    stderr = bytes(outcome.pop("stderr"))
    base = {
        "schema_version": 2,
        "completion_scope": COMPLETION_SCOPE,
        "request_sha256": checked["request_sha256"],
        "elevation_grant_sha256": checked["elevation_grant_sha256"],
        "launch_grant_sha256": checked["launch_grant_sha256"],
        "claim_sha256": claim["claim_sha256"],
        "helper_reference": helper["helper_reference"],
        "executable_sha256": actual_hash,
        "executable_size_bytes": executable_size,
        "support_files_sha256": sha256(helper.get("support_files", [])),
        "environment_sha256": sha256(helper["environment"]),
        "argv_sha256": sha256(helper["argv"]),
        "completed_at_utc": completed_at_utc,
        **outcome,
        "stdout_sha256": hashlib.sha256(stdout).hexdigest(),
        "stdout_size_bytes": len(stdout),
        "stderr_sha256": hashlib.sha256(stderr).hexdigest(),
        "stderr_size_bytes": len(stderr),
        "process_completion_observed": True,
        "shell_used": False,
        "exact_environment_applied": True,
        "immutable_private_copy_used": True,
        "private_bundle_removed": True,
        "statement": COMPLETION_STATEMENT,
        "bootstrap_request": checked,
    }
    try:
        return seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="completion_sha256")
    except ExecutionSecurityError as exc:
        raise ControlledBootstrapperError(f"Completion receipt authentication failed: {exc}") from exc


def validate_completion_receipt(receipt: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(receipt)
    if (
        document.get("schema_version") != 2
        or document.get("completion_scope") != COMPLETION_SCOPE
        or document.get("statement") != COMPLETION_STATEMENT
        or document.get("process_completion_observed") is not True
        or document.get("shell_used") is not False
        or document.get("exact_environment_applied") is not True
        or document.get("immutable_private_copy_used") is not True
        or document.get("private_bundle_removed") is not True
    ):
        raise ControlledBootstrapperError("Completion receipt contract is invalid.")
    try:
        verify_sealed_record(document, authority_key_path=authority_key_path, digest_field="completion_sha256")
        checked_request = validate_bootstrap_request(
            _object(document.get("bootstrap_request"), "completion.bootstrap_request"),
            authority_key_path=authority_key_path,
        )
    except (ExecutionSecurityError, ElevationError) as exc:
        raise ControlledBootstrapperError(f"Completion receipt authentication failed: {exc}") from exc
    helper = _object(checked_request.get("helper"), "request.helper")
    expected_bindings = {
        "request_sha256": checked_request["request_sha256"],
        "elevation_grant_sha256": checked_request["elevation_grant_sha256"],
        "launch_grant_sha256": checked_request["launch_grant_sha256"],
        "helper_reference": helper["helper_reference"],
        "executable_sha256": helper["executable_sha256"],
        "support_files_sha256": sha256(helper.get("support_files", [])),
        "environment_sha256": sha256(helper["environment"]),
        "argv_sha256": sha256(helper["argv"]),
    }
    for field, expected in expected_bindings.items():
        if document.get(field) != expected:
            raise ControlledBootstrapperError(f"Completion receipt binding is invalid: {field}.")
    utc_datetime(document.get("completed_at_utc"), "completion.completed_at_utc")
    return document


def load_request(path: Path, *, authority_key_path: Path) -> dict[str, object]:
    try:
        value = read_strict_json_file(Path(path), "Bootstrap request", require_canonical=True)
        return validate_bootstrap_request(_object(value, "request"), authority_key_path=authority_key_path)
    except (ExecutionSecurityError, ElevationError) as exc:
        raise ControlledBootstrapperError(str(exc)) from exc


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--request", type=Path, required=True)
    parser.add_argument("--execution-root", type=Path, required=True)
    parser.add_argument("--authority-key", type=Path, required=True)
    parser.add_argument("--claim-root", type=Path, required=True)
    parser.add_argument("--completed-at-utc", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        result = execute_bootstrap_request(
            load_request(args.request, authority_key_path=args.authority_key),
            args.execution_root,
            authority_key_path=args.authority_key,
            claim_root=args.claim_root,
            completed_at_utc=args.completed_at_utc,
        )
        checked = validate_completion_receipt(result, authority_key_path=args.authority_key)
        publish_bytes_create_once(args.output, canonical_json(checked), label="Elevated completion receipt")
        return 0
    except (OSError, ControlledBootstrapperError, ElevationError, ExecutionSecurityError) as exc:
        print(f"Controlled elevation bootstrapper failed: {exc}", file=sys.stderr)
        return 1


__all__ = ["ControlledBootstrapperError", "execute_bootstrap_request", "load_request", "validate_completion_receipt"]

if __name__ == "__main__":
    raise SystemExit(main())
