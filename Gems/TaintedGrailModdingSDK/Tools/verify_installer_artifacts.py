#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Verify the complete unsigned Windows installer artifact set before retention."""

from __future__ import annotations

import argparse
import hashlib
import re
import stat
import sys
from pathlib import Path

import developer_preview_installer as installer


VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")


class ArtifactVerificationError(RuntimeError):
    """Raised when a retained installer artifact is missing or incoherent."""


def require_version(value: str) -> str:
    if not VERSION_PATTERN.fullmatch(value):
        raise ArtifactVerificationError("Version must contain exactly three numeric components.")
    return value


def is_reparse_point(path: Path) -> bool:
    try:
        attributes = path.lstat().st_file_attributes
    except AttributeError:
        return False
    except OSError as exc:
        raise ArtifactVerificationError(f"Unable to inspect artifact identity {path}: {exc}") from exc
    return bool(attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT)


def require_regular_file(path: Path, label: str) -> Path:
    try:
        if path.is_symlink() or is_reparse_point(path):
            raise ArtifactVerificationError(
                f"{label} must not be a symlink, junction, or reparse point: {path}"
            )
        if not path.is_file():
            raise ArtifactVerificationError(f"{label} is missing or is not a regular file: {path}")
    except OSError as exc:
        raise ArtifactVerificationError(f"Unable to inspect {label}: {path}: {exc}") from exc
    return path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            while chunk := stream.read(1024 * 1024):
                digest.update(chunk)
    except OSError as exc:
        raise ArtifactVerificationError(f"Unable to hash {path}: {exc}") from exc
    return digest.hexdigest()


def parse_checksum_record(text: str, expected_filename: str) -> str:
    if not text.endswith("\n") or "\r" in text:
        raise ArtifactVerificationError(
            f"Checksum for {expected_filename} must use one canonical LF-terminated record."
        )
    line = text[:-1]
    parts = line.split("  ")
    if (
        len(parts) != 2
        or not SHA256_PATTERN.fullmatch(parts[0])
        or parts[1] != expected_filename
    ):
        raise ArtifactVerificationError(
            f"Checksum for {expected_filename} must be '<lowercase sha256>  {expected_filename}'."
        )
    return parts[0]


def verify_checksum_pair(artifact: Path) -> str:
    require_regular_file(artifact, f"Installer artifact {artifact.name}")
    sidecar = require_regular_file(
        Path(f"{artifact}.sha256"), f"Checksum sidecar for {artifact.name}"
    )
    try:
        expected = parse_checksum_record(sidecar.read_text(encoding="utf-8"), artifact.name)
    except (OSError, UnicodeError) as exc:
        raise ArtifactVerificationError(f"Unable to read checksum for {artifact.name}: {exc}") from exc
    actual = sha256_file(artifact)
    if actual != expected:
        raise ArtifactVerificationError(
            f"Installer artifact checksum mismatch for {artifact.name}: expected {expected}, got {actual}."
        )
    return actual


def artifact_paths(root: Path, version: str) -> tuple[Path, Path, Path]:
    normalized_version = require_version(version)
    base = f"Tainted-Grail-FoA-SDK-{normalized_version}-windows-x64"
    return (
        root / "FOA-SDK-Installer.exe",
        root / f"{base}.msi",
        root / f"{base}.zip",
    )


def verify_artifact_set(root: Path, version: str) -> dict[str, str]:
    try:
        if root.is_symlink() or is_reparse_point(root):
            raise ArtifactVerificationError(
                f"Artifact root must not be a symlink, junction, or reparse point: {root}"
            )
        resolved = root.resolve(strict=True)
    except OSError as exc:
        raise ArtifactVerificationError(f"Artifact root is missing or inaccessible: {root}: {exc}") from exc
    if not resolved.is_dir():
        raise ArtifactVerificationError(f"Artifact root must be a regular directory: {resolved}")

    wizard, msi, portable_zip = artifact_paths(resolved, version)
    expected_names = {
        wizard.name,
        f"{wizard.name}.sha256",
        msi.name,
        f"{msi.name}.sha256",
        portable_zip.name,
        f"{portable_zip.name}.sha256",
    }
    try:
        entries = list(resolved.iterdir())
    except OSError as exc:
        raise ArtifactVerificationError(f"Unable to enumerate artifact root {resolved}: {exc}") from exc
    actual_names = {entry.name for entry in entries}
    if actual_names != expected_names:
        extra = sorted(actual_names - expected_names)
        missing = sorted(expected_names - actual_names)
        raise ArtifactVerificationError(
            f"Retained installer artifact set mismatch; extra={extra}, missing={missing}."
        )
    for entry in entries:
        require_regular_file(entry, f"Retained installer artifact {entry.name}")

    verified = {
        wizard.name: verify_checksum_pair(wizard),
        msi.name: verify_checksum_pair(msi),
        portable_zip.name: verify_checksum_pair(portable_zip),
    }
    try:
        installer.verify_archive(portable_zip)
    except installer.InstallerError as exc:
        raise ArtifactVerificationError(f"Portable SDK archive verification failed: {exc}") from exc
    return verified


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-root", type=Path, required=True)
    parser.add_argument("--version", required=True)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        verified = verify_artifact_set(arguments.artifact_root, arguments.version)
    except ArtifactVerificationError as exc:
        print(f"Installer artifact verification failed: {exc}", file=sys.stderr)
        return 1
    for name, digest in sorted(verified.items(), key=lambda item: item[0].casefold()):
        print(f"{digest}  {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
