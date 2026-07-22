#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Plan and verify the optional official Merlin's Workshop acquisition route."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import stat
import sys
from pathlib import Path, PurePosixPath
from typing import Any, Iterable

PROVIDER_ID = "provider.merlin-workshop"
EXTENSION_ID = "extension.merlins-workshop"
REPOSITORY = "AR-Questline/merlin-workshop"
REPOSITORY_URL = "https://github.com/AR-Questline/merlin-workshop.git"
PINNED_COMMIT = "073bdab3e09d6adad5003339fc49b021738d71e6"
PINNED_TAG = "v1.1.0"
TOOL_VERSION = "1.1.0"
UNITY_VERSION = "6000.0.60f1"
UNITY_REVISION = "61dfb374e36f"
ADDRESSABLES_VERSION = "2.7.4"
LICENSE_EXPRESSION = "LicenseRef-Merlins-Workshop-1.1.0"
DESTINATION_TOKEN = "${MERLIN_DESTINATION}"
SCHEMA_VERSION = 1
MAX_ANCHOR_BYTES = 4 * 1024 * 1024
MANIFEST_PATH = Path(__file__).resolve().parents[1] / "merlin-source.json"

AUTHORITY = {
    "archive_extraction_allowed": False,
    "catalog_mutation_allowed": False,
    "deployment_allowed": False,
    "evidence_promotion_allowed": False,
    "game_launch_allowed": False,
    "network_execution_allowed": False,
    "process_execution_allowed": False,
    "publication_allowed": False,
    "runtime_execution_allowed": False,
    "save_mutation_allowed": False,
    "unity_launch_allowed": False,
}


class MerlinAcquisitionError(RuntimeError):
    """Raised when the optional Merlin acquisition contract fails closed."""


def canonical_json_bytes(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def sha256_hex(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def git_blob_sha1(payload: bytes) -> str:
    header = f"blob {len(payload)}\0".encode("ascii")
    return hashlib.sha1(header + payload).hexdigest()


def stable_id(prefix: str, value: object) -> str:
    return f"{prefix}:sha256:{sha256_hex(canonical_json_bytes(value))}"


def require_mapping(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise MerlinAcquisitionError(f"{label} must be an object.")
    return value


def safe_relative_path(value: str, label: str) -> str:
    if not isinstance(value, str) or not value or len(value) > 256 or "\\" in value:
        raise MerlinAcquisitionError(f"{label} is not a bounded canonical relative path.")
    path = PurePosixPath(value)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise MerlinAcquisitionError(f"{label} is not a safe relative path.")
    return value


def parse_lfs_pointer(payload: bytes) -> tuple[str, int] | None:
    try:
        text = payload.decode("ascii")
    except UnicodeDecodeError:
        return None
    lines = text.splitlines()
    if len(lines) != 3 or lines[0] != "version https://git-lfs.github.com/spec/v1":
        return None
    if not lines[1].startswith("oid sha256:") or not lines[2].startswith("size "):
        return None
    digest = lines[1][len("oid sha256:") :]
    try:
        size = int(lines[2][len("size ") :])
    except ValueError:
        return None
    if len(digest) != 64 or any(character not in "0123456789abcdef" for character in digest):
        return None
    if size <= 0:
        return None
    return digest, size


def load_manifest(path: Path = MANIFEST_PATH) -> dict[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8", errors="strict"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise MerlinAcquisitionError("Unable to load the canonical Merlin source manifest.") from exc
    root = require_mapping(document, "Merlin source manifest")
    expected = {
        "anchors",
        "authority",
        "licenses",
        "provider_id",
        "schema_version",
        "source",
        "toolchain",
    }
    if set(root) != expected or root.get("schema_version") != SCHEMA_VERSION:
        raise MerlinAcquisitionError("Merlin source manifest schema is unsupported or incomplete.")
    if root.get("provider_id") != PROVIDER_ID or root.get("authority") != AUTHORITY:
        raise MerlinAcquisitionError("Merlin source manifest identity or authority drifted.")

    source = require_mapping(root.get("source"), "source")
    if source != {
        "commit": PINNED_COMMIT,
        "license_expression": LICENSE_EXPRESSION,
        "release": TOOL_VERSION,
        "repository": REPOSITORY,
        "tag": PINNED_TAG,
    }:
        raise MerlinAcquisitionError("Merlin source identity differs from the reviewed exact release.")

    toolchain = require_mapping(root.get("toolchain"), "toolchain")
    if toolchain != {
        "addressables": ADDRESSABLES_VERSION,
        "unity_editor": UNITY_VERSION,
        "unity_revision": UNITY_REVISION,
    }:
        raise MerlinAcquisitionError("Merlin toolchain identity drifted.")

    anchors = root.get("anchors")
    licenses = root.get("licenses")
    if not isinstance(anchors, list) or not anchors or len(anchors) > 16:
        raise MerlinAcquisitionError("Merlin anchor set is empty or unbounded.")
    if not isinstance(licenses, list) or len(licenses) != 2:
        raise MerlinAcquisitionError("Both reviewed Merlin licence documents are required.")

    seen: set[str] = set()
    for label, rows in (("anchor", anchors), ("licence", licenses)):
        previous = ""
        for row in rows:
            entry = require_mapping(row, label)
            path_value = safe_relative_path(entry.get("path"), f"{label} path")
            if path_value in seen or path_value <= previous:
                raise MerlinAcquisitionError(
                    f"Merlin {label} paths must be unique and sorted."
                )
            seen.add(path_value)
            previous = path_value
            blob = entry.get("git_blob_sha1")
            if not isinstance(blob, str) or len(blob) != 40 or any(c not in "0123456789abcdef" for c in blob):
                raise MerlinAcquisitionError(f"{label} Git blob fingerprint is invalid.")
            if label == "licence":
                digest = entry.get("lfs_sha256")
                size = entry.get("bytes")
                if (
                    not isinstance(digest, str)
                    or len(digest) != 64
                    or any(c not in "0123456789abcdef" for c in digest)
                    or not isinstance(size, int)
                    or size <= 0
                    or size > 2 * 1024 * 1024
                ):
                    raise MerlinAcquisitionError("Merlin licence LFS identity is invalid.")
    return root


def build_work_order(manifest: dict[str, Any]) -> dict[str, Any]:
    source = dict(manifest["source"])
    steps = [
        {
            "argv": [
                "clone",
                "--filter=blob:none",
                "--no-checkout",
                "--origin",
                "origin",
                REPOSITORY_URL,
                DESTINATION_TOKEN,
            ],
            "execution_allowed": False,
            "id": "clone-official-repository",
            "tool": "git",
        },
        {
            "argv": ["-C", DESTINATION_TOKEN, "checkout", "--detach", PINNED_COMMIT],
            "execution_allowed": False,
            "id": "checkout-exact-commit",
            "tool": "git",
        },
        {
            "argv": ["-C", DESTINATION_TOKEN, "lfs", "install", "--local"],
            "execution_allowed": False,
            "id": "enable-local-git-lfs",
            "tool": "git",
        },
        {
            "argv": ["-C", DESTINATION_TOKEN, "lfs", "pull"],
            "execution_allowed": False,
            "id": "materialize-reviewed-lfs-content",
            "tool": "git",
        },
    ]
    order: dict[str, Any] = {
        "authority": dict(AUTHORITY),
        "destination": {
            "must_be_detached_head": True,
            "must_be_external_to_foa_sdk": True,
            "must_not_exist_before_execution": True,
            "placeholder": DESTINATION_TOKEN,
        },
        "expected": {
            "addressables": ADDRESSABLES_VERSION,
            "commit": PINNED_COMMIT,
            "license_documents_materialized": True,
            "tag": PINNED_TAG,
            "unity_editor": UNITY_VERSION,
        },
        "generated_archive_route_allowed": False,
        "github_autogenerated_zip_allowed": False,
        "operator_license_acceptance_required": True,
        "prerequisites": [
            {"id": "git", "minimum_version": "2.40.0", "required": True},
            {"id": "git-lfs", "minimum_version": "3.4.0", "required": True},
        ],
        "provider_id": PROVIDER_ID,
        "release_asset_route_allowed": False,
        "route": "official-git-lfs-work-order",
        "schema_version": SCHEMA_VERSION,
        "source": source,
        "steps": steps,
        "work_order_id": "",
    }
    identity = dict(order)
    identity.pop("work_order_id")
    order["work_order_id"] = stable_id("merlin-work-order", identity)
    return order


def contained_regular_file(root: Path, relative: str, maximum_bytes: int = MAX_ANCHOR_BYTES) -> bytes:
    current = root
    for part in PurePosixPath(relative).parts:
        current = current / part
        try:
            metadata = current.lstat()
        except OSError as exc:
            raise MerlinAcquisitionError(f"Required Merlin path is missing: {relative}.") from exc
        if stat.S_ISLNK(metadata.st_mode):
            raise MerlinAcquisitionError(f"Merlin checkout path may not be a symlink: {relative}.")
    try:
        resolved = current.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError) as exc:
        raise MerlinAcquisitionError(f"Merlin checkout path escapes the canonical root: {relative}.") from exc
    if not resolved.is_file():
        raise MerlinAcquisitionError(f"Merlin checkout path is not a regular file: {relative}.")
    size = resolved.stat().st_size
    if size < 0 or size > maximum_bytes:
        raise MerlinAcquisitionError(f"Merlin checkout file exceeds its review bound: {relative}.")
    with resolved.open("rb") as stream:
        payload = stream.read(maximum_bytes + 1)
    if len(payload) > maximum_bytes:
        raise MerlinAcquisitionError(f"Merlin checkout file exceeds its review bound: {relative}.")
    return payload


def validate_checkout_root(checkout: Path) -> Path:
    try:
        if checkout.is_symlink():
            raise MerlinAcquisitionError("Merlin checkout root may not be a symlink.")
        root = checkout.resolve(strict=True)
    except OSError as exc:
        raise MerlinAcquisitionError("Merlin checkout root is unavailable.") from exc
    if not root.is_dir():
        raise MerlinAcquisitionError("Merlin checkout root must be a directory.")
    git_directory = root / ".git"
    if git_directory.is_symlink() or not git_directory.is_dir():
        raise MerlinAcquisitionError(
            "Merlin verification requires a standalone checkout with a real .git directory."
        )
    head = contained_regular_file(root, ".git/HEAD", 256).decode("ascii", errors="strict").strip()
    if head != PINNED_COMMIT:
        raise MerlinAcquisitionError(
            "Merlin checkout must use the exact pinned commit in detached-HEAD form."
        )
    return root


def verify_anchor(root: Path, row: dict[str, Any]) -> dict[str, Any]:
    path = row["path"]
    payload = contained_regular_file(root, path)
    actual = git_blob_sha1(payload)
    if actual != row["git_blob_sha1"]:
        raise MerlinAcquisitionError(f"Merlin anchor differs from the exact pinned source: {path}.")
    return {"git_blob_sha1": actual, "path": path}


def verify_license(root: Path, row: dict[str, Any]) -> dict[str, Any]:
    path = row["path"]
    payload = contained_regular_file(root, path, 2 * 1024 * 1024)
    pointer = parse_lfs_pointer(payload)
    if pointer is not None:
        digest, byte_count = pointer
        if digest != row["lfs_sha256"] or byte_count != row["bytes"]:
            raise MerlinAcquisitionError(f"Merlin licence pointer identity drifted: {path}.")
        return {
            "bytes": byte_count,
            "git_blob_sha1": git_blob_sha1(payload),
            "lfs_sha256": digest,
            "materialized": False,
            "path": path,
        }
    if len(payload) != row["bytes"] or sha256_hex(payload) != row["lfs_sha256"]:
        raise MerlinAcquisitionError(f"Merlin licence PDF differs from its exact LFS object: {path}.")
    if not payload.startswith(b"%PDF-"):
        raise MerlinAcquisitionError(f"Merlin licence material is not a PDF: {path}.")
    return {
        "bytes": len(payload),
        "git_blob_sha1": row["git_blob_sha1"],
        "lfs_sha256": sha256_hex(payload),
        "materialized": True,
        "path": path,
    }


def inspect_checkout(checkout: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    root = validate_checkout_root(checkout)
    anchors = [verify_anchor(root, row) for row in manifest["anchors"]]
    licenses = [verify_license(root, row) for row in manifest["licenses"]]
    materialized = all(row["materialized"] for row in licenses)
    receipt: dict[str, Any] = {
        "anchors": anchors,
        "authority": dict(AUTHORITY),
        "license_acceptance_recorded": False,
        "licenses": licenses,
        "private_checkout_path_recorded": False,
        "provider_id": PROVIDER_ID,
        "qualification": (
            "ready_for_operator_license_acceptance"
            if materialized
            else "blocked_lfs_materialization_required"
        ),
        "qualified_for_observation": False,
        "receipt_id": "",
        "schema_version": SCHEMA_VERSION,
        "source": dict(manifest["source"]),
        "toolchain": dict(manifest["toolchain"]),
    }
    identity = dict(receipt)
    identity.pop("receipt_id")
    receipt["receipt_id"] = stable_id("merlin-checkout", identity)
    return receipt


def accept_license(receipt: dict[str, Any], *, accepted: bool) -> dict[str, Any]:
    if not accepted:
        raise MerlinAcquisitionError("Explicit operator licence acceptance is required.")
    if receipt.get("qualification") != "ready_for_operator_license_acceptance":
        raise MerlinAcquisitionError("Merlin checkout is not ready for licence acceptance.")
    accepted_receipt = dict(receipt)
    accepted_receipt["license_acceptance_recorded"] = True
    accepted_receipt["qualification"] = "exact_checkout_qualified"
    accepted_receipt["qualified_for_observation"] = True
    accepted_receipt["receipt_id"] = ""
    identity = dict(accepted_receipt)
    identity.pop("receipt_id")
    accepted_receipt["receipt_id"] = stable_id("merlin-checkout", identity)
    return accepted_receipt


def verify_receipt(document: dict[str, Any]) -> None:
    receipt = require_mapping(document, "Merlin receipt")
    identifier = receipt.get("receipt_id")
    if not isinstance(identifier, str):
        raise MerlinAcquisitionError("Merlin receipt identity is missing.")
    identity = dict(receipt)
    identity.pop("receipt_id", None)
    if identifier != stable_id("merlin-checkout", identity):
        raise MerlinAcquisitionError("Merlin receipt identity does not match canonical content.")
    if receipt.get("provider_id") != PROVIDER_ID or receipt.get("source", {}).get("commit") != PINNED_COMMIT:
        raise MerlinAcquisitionError("Merlin receipt source binding drifted.")
    if receipt.get("authority") != AUTHORITY:
        raise MerlinAcquisitionError("Merlin receipt attempts to escalate authority.")
    if receipt.get("private_checkout_path_recorded") is not False:
        raise MerlinAcquisitionError("Merlin receipt may not retain a private checkout path.")
    qualified = receipt.get("qualified_for_observation")
    accepted = receipt.get("license_acceptance_recorded")
    materialized = all(
        isinstance(row, dict) and row.get("materialized") is True
        for row in receipt.get("licenses", [])
    )
    if qualified is True and (accepted is not True or not materialized):
        raise MerlinAcquisitionError("Qualified Merlin receipts require materialized and accepted licences.")


def load_json(path: Path) -> dict[str, Any]:
    try:
        return require_mapping(
            json.loads(path.read_text(encoding="utf-8", errors="strict")),
            path.name,
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise MerlinAcquisitionError(f"Unable to read canonical JSON from {path.name}.") from exc


def write_canonical(path: Path | None, document: object) -> None:
    payload = canonical_json_bytes(document)
    if path is None:
        sys.stdout.buffer.write(payload)
        return
    if path.exists():
        raise MerlinAcquisitionError(f"Refusing to overwrite existing output: {path}.")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as stream:
        stream.write(payload)
        stream.flush()
        os.fsync(stream.fileno())


def parse_arguments(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=MANIFEST_PATH)
    commands = parser.add_subparsers(dest="command", required=True)

    commands.add_parser("describe")

    plan = commands.add_parser("plan")
    plan.add_argument("--output", type=Path)

    inspect = commands.add_parser("inspect-local")
    inspect.add_argument("--checkout", type=Path, required=True)
    inspect.add_argument("--output", type=Path)

    accept = commands.add_parser("accept-license")
    accept.add_argument("--receipt", type=Path, required=True)
    accept.add_argument("--i-accept-the-official-license", action="store_true")
    accept.add_argument("--output", type=Path)

    verify = commands.add_parser("verify-receipt")
    verify.add_argument("--receipt", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    try:
        arguments = parse_arguments(argv)
        manifest = load_manifest(arguments.manifest)
        if arguments.command == "describe":
            write_canonical(None, manifest)
        elif arguments.command == "plan":
            write_canonical(arguments.output, build_work_order(manifest))
        elif arguments.command == "inspect-local":
            write_canonical(arguments.output, inspect_checkout(arguments.checkout, manifest))
        elif arguments.command == "accept-license":
            receipt = load_json(arguments.receipt)
            verify_receipt(receipt)
            write_canonical(
                arguments.output,
                accept_license(
                    receipt,
                    accepted=arguments.i_accept_the_official_license,
                ),
            )
        elif arguments.command == "verify-receipt":
            verify_receipt(load_json(arguments.receipt))
        else:
            raise MerlinAcquisitionError("Unknown Merlin acquisition command.")
        return 0
    except (MerlinAcquisitionError, OSError, UnicodeDecodeError) as exc:
        print(f"Merlin's Workshop acquisition failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
