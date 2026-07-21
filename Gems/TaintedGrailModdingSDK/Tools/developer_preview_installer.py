#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Build and verify the reviewed Windows prebuilt SDK installer payload."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import uuid
import zipfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable


SCHEMA_VERSION = 1
PRODUCT_ID = "tainted-grail-foa-modding-sdk"
PRODUCT_NAME = "Tainted Grail: The Fall of Avalon Modding Editor and SDK"
ENGINE_NAME = "TaintedGrailFoASDK"
PROJECT_DIRECTORY = "TaintedGrailModdingEditor"
CONFIGURATION = "profile"
BIN_DIRECTORY = PurePosixPath("bin/Windows/profile/Default")
EDITOR_PATH = BIN_DIRECTORY / "Editor.exe"
LAUNCHER_PATH = BIN_DIRECTORY / "TaintedGrailModdingEditorLauncher.exe"
MANIFEST_NAME = "INSTALL_MANIFEST.json"
CHECKSUMS_NAME = "SHA256SUMS"
PROVENANCE_NAME = "BUILD_PROVENANCE.json"
SBOM_NAME = "SBOM.spdx.json"
NOTICES_NAME = "NOTICES.txt"
PACKAGES_NAME = "THIRD_PARTY_PACKAGES.json"
CHANNELS = ("development", "alpha", "beta", "release-candidate", "stable")
SOURCE_KINDS = {
    "o3de-sdk-install",
    "editor-project",
    "legal-review",
    "generated-metadata",
}
PROJECT_FILES = (
    PurePosixPath("project.json"),
    PurePosixPath("preview.png"),
    PurePosixPath("TaintedGrailModdingEditor.ico"),
)
PROJECT_DIRECTORIES = (
    PurePosixPath("Levels"),
    PurePosixPath("ShaderLib"),
)
RESERVED_ROOTS = {
    MANIFEST_NAME.casefold(),
    CHECKSUMS_NAME.casefold(),
    PROVENANCE_NAME.casefold(),
    SBOM_NAME.casefold(),
    NOTICES_NAME.casefold(),
    PACKAGES_NAME.casefold(),
}
WINDOWS_RESERVED_NAMES = {
    "con",
    "prn",
    "aux",
    "nul",
    *(f"com{number}" for number in range(1, 10)),
    *(f"lpt{number}" for number in range(1, 10)),
}
VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
UTC_PATTERN = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$")
CONTROL_PATTERN = re.compile(r"[\x00-\x1f\x7f]")
ILLEGAL_WINDOWS_CHARACTERS = frozenset('<>:"|?*')


class InstallerError(RuntimeError):
    """Raised when an installer input or artifact fails closed."""


@dataclass(frozen=True)
class PayloadSource:
    destination: PurePosixPath
    source_kind: str
    source_path: Path | None = None
    generated_bytes: bytes | None = None

    def read_bytes(self) -> bytes:
        if self.generated_bytes is not None:
            return self.generated_bytes
        if self.source_path is None:
            raise InstallerError(f"Payload source has no bytes: {self.destination}")
        try:
            return self.source_path.read_bytes()
        except OSError as exc:
            raise InstallerError(f"Unable to read {self.destination}: {exc}") from exc


def canonical_json_bytes(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode(
        "utf-8"
    )


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            while chunk := stream.read(1024 * 1024):
                digest.update(chunk)
    except OSError as exc:
        raise InstallerError(f"Unable to hash {path}: {exc}") from exc
    return digest.hexdigest()


def spdx_sha1_file(path: Path) -> str:
    """Return the SHA-1 digest mandated by the SPDX 2.3 package-verification algorithm."""
    digest = hashlib.sha1(usedforsecurity=False)
    try:
        with path.open("rb") as stream:
            while chunk := stream.read(1024 * 1024):
                digest.update(chunk)
    except OSError as exc:
        raise InstallerError(f"Unable to calculate SPDX verification input for {path}: {exc}") from exc
    return digest.hexdigest()


def parse_json_bytes(contents: bytes, label: str) -> object:
    def reject_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise InstallerError(f"Duplicate JSON key {key!r} in {label}")
            result[key] = value
        return result

    try:
        return json.loads(contents.decode("utf-8"), object_pairs_hook=reject_duplicates)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise InstallerError(f"Unable to parse JSON {label}: {exc}") from exc


def load_json(path: Path) -> object:
    try:
        contents = path.read_bytes()
    except OSError as exc:
        raise InstallerError(f"Unable to read JSON {path}: {exc}") from exc
    return parse_json_bytes(contents, str(path))


def require_plain_text(value: str, label: str, maximum: int = 512) -> str:
    stripped = value.strip()
    if not stripped or len(stripped) > maximum or CONTROL_PATTERN.search(stripped):
        raise InstallerError(f"{label} must be non-empty printable text of at most {maximum} characters.")
    return stripped


def require_version(value: str) -> str:
    if not VERSION_PATTERN.fullmatch(value):
        raise InstallerError("Version must contain exactly three numeric components: MAJOR.MINOR.PATCH.")
    components = [int(component) for component in value.split(".")]
    if any(component > 65535 for component in components):
        raise InstallerError("Each version component must be between 0 and 65535 for Windows Installer.")
    return value


def require_commit(value: str) -> str:
    normalized = value.strip().lower()
    if not COMMIT_PATTERN.fullmatch(normalized):
        raise InstallerError("Source commit must be an exact lowercase 40-character Git SHA-1.")
    return normalized


def require_channel(value: str) -> str:
    if value not in CHANNELS:
        raise InstallerError(f"Channel must be one of: {', '.join(CHANNELS)}.")
    return value


def derive_clean_repository_commit(repo_root: Path) -> str:
    repo = require_directory(repo_root, "Repository root")
    try:
        top_level = subprocess.run(
            ("git", "-C", str(repo), "rev-parse", "--show-toplevel"),
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        ).stdout.strip()
        head = subprocess.run(
            ("git", "-C", str(repo), "rev-parse", "HEAD"),
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        ).stdout.strip()
        status = subprocess.run(
            ("git", "-C", str(repo), "status", "--porcelain=v1", "--untracked-files=all"),
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        ).stdout
    except (OSError, subprocess.CalledProcessError, UnicodeError) as exc:
        raise InstallerError(f"Unable to derive installer source commit from Git: {exc}") from exc
    if Path(top_level).resolve(strict=True) != repo:
        raise InstallerError(f"Installer repository root does not match Git top level: {top_level}")
    if status:
        raise InstallerError("Installer inventory requires a clean exact Git head.")
    return require_commit(head)


def require_sha256(value: str, label: str) -> str:
    normalized = value.strip().lower()
    if not SHA256_PATTERN.fullmatch(normalized):
        raise InstallerError(f"{label} must be an exact lowercase SHA-256 value.")
    return normalized


def require_utc_seconds(value: str) -> str:
    normalized = value.strip()
    if not UTC_PATTERN.fullmatch(normalized):
        raise InstallerError("Review time must use exact UTC seconds: YYYY-MM-DDTHH:MM:SSZ.")
    try:
        dt.datetime.strptime(normalized, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as exc:
        raise InstallerError(f"Review time is not a valid UTC timestamp: {normalized}") from exc
    return normalized


def is_reparse_point(path: Path) -> bool:
    try:
        attributes = path.lstat().st_file_attributes
    except AttributeError:
        return False
    except OSError as exc:
        raise InstallerError(f"Unable to inspect filesystem identity {path}: {exc}") from exc
    return bool(attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT)


def require_regular_file(path: Path, label: str) -> Path:
    try:
        if path.is_symlink() or is_reparse_point(path):
            raise InstallerError(f"{label} must not be a symlink, junction, or reparse point: {path}")
        if not path.is_file():
            raise InstallerError(f"{label} is missing or is not a regular file: {path}")
    except OSError as exc:
        raise InstallerError(f"Unable to inspect {label}: {path}: {exc}") from exc
    return path


def require_directory(path: Path, label: str) -> Path:
    try:
        if path.is_symlink() or is_reparse_point(path):
            raise InstallerError(f"{label} must not be a symlink, junction, or reparse point: {path}")
        if not path.is_dir():
            raise InstallerError(f"{label} is missing or is not a directory: {path}")
    except OSError as exc:
        raise InstallerError(f"Unable to inspect {label}: {path}: {exc}") from exc
    return path.resolve(strict=True)


def validate_destination(path: PurePosixPath) -> str:
    if path.is_absolute() or not path.parts or any(part in {"", ".", ".."} for part in path.parts):
        raise InstallerError(f"Unsafe installer destination: {path}")
    identity_parts: list[str] = []
    for part in path.parts:
        if CONTROL_PATTERN.search(part) or any(character in ILLEGAL_WINDOWS_CHARACTERS for character in part):
            raise InstallerError(f"Installer path contains a Windows-unsafe character: {path}")
        if part.endswith((" ", ".")):
            raise InstallerError(f"Installer path has a trailing Windows alias character: {path}")
        stem = part.split(".", 1)[0].casefold()
        if stem in WINDOWS_RESERVED_NAMES:
            raise InstallerError(f"Installer path uses a reserved Windows device name: {path}")
        identity_parts.append(part.rstrip(" .").casefold())
    return "/".join(identity_parts)


def iter_regular_files(root: Path) -> Iterable[tuple[PurePosixPath, Path]]:
    require_directory(root, "Payload input root")
    stack = [(PurePosixPath(), root)]
    while stack:
        relative_root, directory = stack.pop()
        try:
            children = sorted(os.scandir(directory), key=lambda entry: entry.name.casefold(), reverse=True)
        except OSError as exc:
            raise InstallerError(f"Unable to enumerate payload input {directory}: {exc}") from exc
        for child in children:
            relative = relative_root / child.name
            path = Path(child.path)
            try:
                if child.is_symlink() or is_reparse_point(path):
                    raise InstallerError(
                        f"Payload inputs must not contain symlinks, junctions, or reparse points: {path}"
                    )
                if child.is_dir(follow_symlinks=False):
                    stack.append((relative, path))
                elif child.is_file(follow_symlinks=False):
                    yield relative, path
                else:
                    raise InstallerError(f"Payload input is not a regular file or directory: {path}")
            except OSError as exc:
                raise InstallerError(f"Unable to inspect payload input {path}: {exc}") from exc


def installed_project_json(repo_root: Path) -> bytes:
    source = require_regular_file(repo_root / PROJECT_DIRECTORY / "project.json", "Editor project.json")
    document = load_json(source)
    if not isinstance(document, dict) or document.get("project_name") != PROJECT_DIRECTORY:
        raise InstallerError("The dedicated Editor project.json has an unexpected project identity.")
    result = dict(document)
    result["engine"] = ENGINE_NAME
    result["summary"] = "Prebuilt O3DE host project for the Tainted Grail modding editor and SDK."
    return (json.dumps(result, sort_keys=False, indent=4, ensure_ascii=False) + "\n").encode("utf-8")


def validate_sdk_identity(sdk_root: Path) -> None:
    engine = load_json(require_regular_file(sdk_root / "engine.json", "Installed SDK engine.json"))
    if not isinstance(engine, dict):
        raise InstallerError("Installed SDK engine.json must contain a JSON object.")
    if engine.get("sdk_engine") is not True or engine.get("engine_name") != ENGINE_NAME:
        raise InstallerError(
            f"Installed SDK must be generated with -DO3DE_INSTALL_ENGINE_NAME={ENGINE_NAME}."
        )
    require_regular_file(sdk_root / Path(EDITOR_PATH.as_posix()), "Installed Editor.exe")
    require_regular_file(sdk_root / Path(LAUNCHER_PATH.as_posix()), "Installed SDK launcher")
    require_regular_file(sdk_root / "LICENSE.txt", "Installed SDK licence")


def build_sources(
    repo_root: Path,
    sdk_root: Path,
    notices: Path,
    packages: Path,
) -> list[PayloadSource]:
    repo = require_directory(repo_root, "Repository root")
    sdk = require_directory(sdk_root, "Installed SDK root")
    validate_sdk_identity(sdk)
    require_regular_file(notices, "Generated NOTICES.txt")
    require_regular_file(packages, "Generated third-party package inventory")
    if not notices.read_text(encoding="utf-8").strip():
        raise InstallerError("Generated NOTICES.txt must not be empty.")
    package_document = load_json(packages)
    if not isinstance(package_document, list):
        raise InstallerError("Third-party package inventory must contain a JSON array.")

    sources: list[PayloadSource] = []
    for relative, path in iter_regular_files(sdk):
        if relative.parts[0].casefold() == PROJECT_DIRECTORY.casefold():
            continue
        if relative.parts[0].casefold() in RESERVED_ROOTS:
            raise InstallerError(f"Installed SDK collides with reserved installer metadata: {relative}")
        sources.append(PayloadSource(relative, "o3de-sdk-install", source_path=path))

    project_root = require_directory(repo / PROJECT_DIRECTORY, "Dedicated Editor project")
    for relative in PROJECT_FILES:
        source = require_regular_file(project_root / Path(relative.as_posix()), f"Project file {relative}")
        destination = PurePosixPath(PROJECT_DIRECTORY) / relative
        if relative == PurePosixPath("project.json"):
            sources.append(
                PayloadSource(destination, "editor-project", generated_bytes=installed_project_json(repo))
            )
        else:
            sources.append(PayloadSource(destination, "editor-project", source_path=source))
    for directory_relative in PROJECT_DIRECTORIES:
        directory = project_root / Path(directory_relative.as_posix())
        if not directory.exists():
            continue
        for relative, path in iter_regular_files(directory):
            sources.append(
                PayloadSource(
                    PurePosixPath(PROJECT_DIRECTORY) / directory_relative / relative,
                    "editor-project",
                    source_path=path,
                )
            )

    sources.extend(
        (
            PayloadSource(PurePosixPath(NOTICES_NAME), "legal-review", source_path=notices),
            PayloadSource(PurePosixPath(PACKAGES_NAME), "legal-review", source_path=packages),
        )
    )

    identities: dict[str, PurePosixPath] = {}
    for source in sources:
        identity = validate_destination(source.destination)
        existing = identities.get(identity)
        if existing is not None:
            raise InstallerError(
                f"Installer destination collision under Windows path identity: {existing} and {source.destination}"
            )
        identities[identity] = source.destination
    return sorted(sources, key=lambda source: source.destination.as_posix().casefold())


def inventory_entry(source: PayloadSource) -> dict[str, object]:
    if source.generated_bytes is not None:
        digest = sha256_bytes(source.generated_bytes)
        size = len(source.generated_bytes)
    elif source.source_path is not None:
        digest = sha256_file(source.source_path)
        size = source.source_path.stat().st_size
    else:
        raise InstallerError(f"Payload source has no bytes: {source.destination}")
    return {
        "path": source.destination.as_posix(),
        "sha256": digest,
        "size": size,
        "source_kind": source.source_kind,
    }


def build_inventory(
    repo_root: Path,
    sdk_root: Path,
    notices: Path,
    packages: Path,
    version: str,
    source_commit: str,
    channel: str,
) -> tuple[dict[str, object], list[PayloadSource]]:
    normalized_version = require_version(version)
    normalized_commit = require_commit(source_commit)
    normalized_channel = require_channel(channel)
    sources = build_sources(repo_root, sdk_root, notices, packages)
    inventory: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "product_id": PRODUCT_ID,
        "product_version": normalized_version,
        "channel": normalized_channel,
        "source_commit": normalized_commit,
        "configuration": CONFIGURATION,
        "engine_name": ENGINE_NAME,
        "entries": [inventory_entry(source) for source in sources],
    }
    inventory["inventory_sha256"] = sha256_bytes(canonical_json_bytes(inventory))
    return inventory, sources


def write_exclusive(path: Path, contents: bytes) -> None:
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("xb") as stream:
            stream.write(contents)
            stream.flush()
            os.fsync(stream.fileno())
    except OSError as exc:
        raise InstallerError(f"Unable to create {path}: {exc}") from exc


def copy_source(source: PayloadSource, destination: Path, expected_sha256: str) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.generated_bytes is not None:
        contents = source.generated_bytes
        if sha256_bytes(contents) != expected_sha256:
            raise InstallerError(f"Generated installer input changed unexpectedly: {source.destination}")
        write_exclusive(destination, contents)
        return
    if source.source_path is None:
        raise InstallerError(f"Installer source path is missing: {source.destination}")

    require_regular_file(source.source_path, f"Installer input {source.destination}")
    digest = hashlib.sha256()
    try:
        before = source.source_path.stat()
        with source.source_path.open("rb") as input_stream, destination.open("xb") as output_stream:
            while chunk := input_stream.read(1024 * 1024):
                digest.update(chunk)
                output_stream.write(chunk)
            output_stream.flush()
            os.fsync(output_stream.fileno())
        after = source.source_path.stat()
    except OSError as exc:
        raise InstallerError(f"Unable to stage {source.destination}: {exc}") from exc
    if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) != (
        after.st_dev,
        after.st_ino,
        after.st_size,
        after.st_mtime_ns,
    ):
        raise InstallerError(f"Installer input changed while it was copied: {source.destination}")
    if digest.hexdigest() != expected_sha256:
        raise InstallerError(f"Installer input changed after inventory review: {source.destination}")


def spdx_id_for_path(path: str) -> str:
    return "SPDXRef-File-" + hashlib.sha256(path.encode("utf-8")).hexdigest()[:24]


def build_sbom(
    stage_root: Path,
    entries: list[dict[str, object]],
    version: str,
    source_commit: str,
    created_at_utc: str,
) -> bytes:
    files = [
        {
            "SPDXID": spdx_id_for_path(str(entry["path"])),
            "fileName": f"./{entry['path']}",
            "checksums": [{"algorithm": "SHA256", "checksumValue": entry["sha256"]}],
            "licenseConcluded": "NOASSERTION",
            "copyrightText": "NOASSERTION",
        }
        for entry in entries
    ]
    file_sha1_values = sorted(
        spdx_sha1_file(stage_root / Path(str(entry["path"]))) for entry in entries
    )
    verification_code = hashlib.sha1(
        "".join(file_sha1_values).encode("ascii"), usedforsecurity=False
    ).hexdigest()
    document = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"{PRODUCT_ID}-{version}",
        "documentNamespace": f"https://github.com/theb0yys/FOA-SDK/spdx/{source_commit}/{version}",
        "creationInfo": {
            "creators": ["Tool: developer_preview_installer.py"],
            "created": created_at_utc,
        },
        "packages": [
            {
                "SPDXID": "SPDXRef-Package",
                "name": PRODUCT_NAME,
                "versionInfo": version,
                "downloadLocation": "NOASSERTION",
                "filesAnalyzed": True,
                "packageVerificationCode": {
                    "packageVerificationCodeValue": verification_code,
                    "packageVerificationCodeExcludedFiles": [
                        f"./{SBOM_NAME}",
                        f"./{MANIFEST_NAME}",
                        f"./{CHECKSUMS_NAME}",
                    ],
                },
                "licenseConcluded": "NOASSERTION",
                "licenseDeclared": "NOASSERTION",
                "copyrightText": "NOASSERTION",
            }
        ],
        "files": files,
        "relationships": [
            {
                "spdxElementId": "SPDXRef-Package",
                "relationshipType": "CONTAINS",
                "relatedSpdxElement": file_entry["SPDXID"],
            }
            for file_entry in files
        ],
    }
    return canonical_json_bytes(document)


def manifest_entries(stage_root: Path, source_kinds: dict[str, str]) -> list[dict[str, object]]:
    entries: list[dict[str, object]] = []
    identities: set[str] = set()
    for relative, path in iter_regular_files(stage_root):
        if relative.as_posix() in {MANIFEST_NAME, CHECKSUMS_NAME}:
            continue
        identity = validate_destination(relative)
        if identity in identities:
            raise InstallerError(f"Staged payload contains a Windows path collision: {relative}")
        identities.add(identity)
        entries.append(
            {
                "path": relative.as_posix(),
                "sha256": sha256_file(path),
                "size": path.stat().st_size,
                "source_kind": source_kinds.get(relative.as_posix(), "generated-metadata"),
            }
        )
    return sorted(entries, key=lambda entry: str(entry["path"]).casefold())


def stage_payload(
    inventory: dict[str, object],
    sources: list[PayloadSource],
    output: Path,
    approved_inventory: str,
    reviewed_by: str,
    reviewed_at: str,
    review_evidence: str,
) -> Path:
    actual_inventory = require_sha256(str(inventory["inventory_sha256"]), "Inventory fingerprint")
    if require_sha256(approved_inventory, "Approved inventory fingerprint") != actual_inventory:
        raise InstallerError(
            "Redistribution approval does not bind to the exact current inventory fingerprint."
        )
    reviewer = require_plain_text(reviewed_by, "Redistribution reviewer", 128)
    review_time = require_utc_seconds(reviewed_at)
    evidence = require_plain_text(review_evidence, "Redistribution evidence reference")

    output = output.resolve(strict=False)
    if output.exists():
        raise InstallerError(f"Staging output must not already exist: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.parent / f".{output.name}.tmp-{uuid.uuid4().hex}"
    temporary.mkdir()
    try:
        expected_entries = {
            str(entry["path"]): entry for entry in inventory["entries"] if isinstance(entry, dict)
        }
        source_kinds: dict[str, str] = {}
        for source in sources:
            entry = expected_entries.get(source.destination.as_posix())
            if entry is None:
                raise InstallerError(f"Inventory entry is missing for {source.destination}")
            destination = temporary / Path(source.destination.as_posix())
            copy_source(source, destination, str(entry["sha256"]))
            source_kinds[source.destination.as_posix()] = source.source_kind

        provenance = {
            "schema_version": SCHEMA_VERSION,
            "product_id": PRODUCT_ID,
            "product_version": inventory["product_version"],
            "channel": inventory["channel"],
            "source_commit": inventory["source_commit"],
            "configuration": CONFIGURATION,
            "engine_name": ENGINE_NAME,
            "inventory_sha256": actual_inventory,
        }
        write_exclusive(temporary / PROVENANCE_NAME, canonical_json_bytes(provenance))
        source_kinds[PROVENANCE_NAME] = "generated-metadata"

        entries_before_sbom = manifest_entries(temporary, source_kinds)
        write_exclusive(
            temporary / SBOM_NAME,
            build_sbom(
                temporary,
                entries_before_sbom,
                str(inventory["product_version"]),
                str(inventory["source_commit"]),
                review_time,
            ),
        )
        source_kinds[SBOM_NAME] = "generated-metadata"
        entries = manifest_entries(temporary, source_kinds)
        manifest = {
            "schema_version": SCHEMA_VERSION,
            "product_id": PRODUCT_ID,
            "product_name": PRODUCT_NAME,
            "product_version": inventory["product_version"],
            "channel": inventory["channel"],
            "source_commit": inventory["source_commit"],
            "configuration": CONFIGURATION,
            "engine_name": ENGINE_NAME,
            "inventory_sha256": actual_inventory,
            "redistribution_review": {
                "reviewed_by": reviewer,
                "reviewed_at_utc": review_time,
                "evidence_refs": [evidence],
            },
            "manifest_excludes": [MANIFEST_NAME, CHECKSUMS_NAME],
            "entries": entries,
        }
        manifest_bytes = canonical_json_bytes(manifest)
        write_exclusive(temporary / MANIFEST_NAME, manifest_bytes)

        checksum_entries = entries + [
            {
                "path": MANIFEST_NAME,
                "sha256": sha256_bytes(manifest_bytes),
                "size": len(manifest_bytes),
                "source_kind": "generated-metadata",
            }
        ]
        checksum_lines = "".join(
            f"{entry['sha256']}  {entry['path']}\n"
            for entry in sorted(checksum_entries, key=lambda entry: str(entry["path"]).casefold())
        )
        write_exclusive(temporary / CHECKSUMS_NAME, checksum_lines.encode("utf-8"))
        verify_stage(temporary)
        os.replace(temporary, output)
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise
    return output


def parse_manifest_document(document: object) -> dict[str, object]:
    if not isinstance(document, dict):
        raise InstallerError("Install manifest must contain a JSON object.")
    required = {
        "schema_version",
        "product_id",
        "product_name",
        "product_version",
        "channel",
        "source_commit",
        "configuration",
        "engine_name",
        "inventory_sha256",
        "redistribution_review",
        "manifest_excludes",
        "entries",
    }
    missing = sorted(required - document.keys())
    if missing:
        raise InstallerError(f"Install manifest is missing required fields: {', '.join(missing)}")
    unexpected = sorted(document.keys() - required)
    if unexpected:
        raise InstallerError(f"Install manifest contains unexpected fields: {', '.join(unexpected)}")
    if (
        document["schema_version"] != SCHEMA_VERSION
        or document["product_id"] != PRODUCT_ID
        or document["product_name"] != PRODUCT_NAME
    ):
        raise InstallerError("Install manifest schema or product identity is unsupported.")
    require_version(str(document["product_version"]))
    require_channel(str(document["channel"]))
    require_commit(str(document["source_commit"]))
    require_sha256(str(document["inventory_sha256"]), "Inventory fingerprint")
    if document["configuration"] != CONFIGURATION or document["engine_name"] != ENGINE_NAME:
        raise InstallerError("Install manifest configuration or engine identity is unsupported.")
    if document["manifest_excludes"] != [MANIFEST_NAME, CHECKSUMS_NAME]:
        raise InstallerError("Install manifest exclusions do not match the format contract.")
    review = document["redistribution_review"]
    if not isinstance(review, dict) or set(review) != {
        "reviewed_by",
        "reviewed_at_utc",
        "evidence_refs",
    }:
        raise InstallerError("Install manifest redistribution review is malformed.")
    require_plain_text(str(review.get("reviewed_by", "")), "Redistribution reviewer", 128)
    require_utc_seconds(str(review.get("reviewed_at_utc", "")))
    evidence = review.get("evidence_refs")
    if not isinstance(evidence, list) or len(evidence) != 1:
        raise InstallerError("Install manifest requires one redistribution evidence reference.")
    require_plain_text(str(evidence[0]), "Redistribution evidence reference")
    if not isinstance(document["entries"], list) or not document["entries"]:
        raise InstallerError("Install manifest entries must be a non-empty array.")
    return document


def validate_manifest_entries(entries: list[object]) -> list[dict[str, object]]:
    parsed: list[dict[str, object]] = []
    identities: set[str] = set()
    paths: list[str] = []
    for entry in entries:
        if not isinstance(entry, dict) or set(entry) != {"path", "sha256", "size", "source_kind"}:
            raise InstallerError("Install manifest entry has an unexpected shape.")
        path = str(entry["path"])
        identity = validate_destination(PurePosixPath(path))
        if identity in identities:
            raise InstallerError(f"Install manifest contains a Windows path collision: {path}")
        identities.add(identity)
        if not isinstance(entry["size"], int) or entry["size"] < 0:
            raise InstallerError(f"Install manifest size is invalid: {path}")
        require_sha256(str(entry["sha256"]), f"Checksum for {path}")
        source_kind = require_plain_text(str(entry["source_kind"]), f"Source kind for {path}", 64)
        if source_kind not in SOURCE_KINDS:
            raise InstallerError(f"Install manifest source kind is unsupported: {source_kind}")
        parsed.append(entry)
        paths.append(path)
    if paths != sorted(paths, key=str.casefold):
        raise InstallerError("Install manifest entries are not in deterministic Windows path order.")
    required_paths = {
        EDITOR_PATH.as_posix(),
        LAUNCHER_PATH.as_posix(),
        f"{PROJECT_DIRECTORY}/project.json",
        "engine.json",
        "LICENSE.txt",
        NOTICES_NAME,
        PACKAGES_NAME,
        PROVENANCE_NAME,
        SBOM_NAME,
    }
    missing = sorted(required_paths - set(paths))
    if missing:
        raise InstallerError(f"Install manifest lacks required payload files: {', '.join(missing)}")
    return parsed


def verify_stage(stage_root: Path) -> dict[str, object]:
    root = require_directory(stage_root, "Staged installer payload")
    manifest_path = require_regular_file(root / MANIFEST_NAME, "Install manifest")
    checksums_path = require_regular_file(root / CHECKSUMS_NAME, "Install checksum list")
    manifest = parse_manifest_document(load_json(manifest_path))
    entries = validate_manifest_entries(manifest["entries"])
    expected_paths = {str(entry["path"]) for entry in entries} | {MANIFEST_NAME, CHECKSUMS_NAME}
    actual_paths = {relative.as_posix() for relative, _ in iter_regular_files(root)}
    if actual_paths != expected_paths:
        extra = sorted(actual_paths - expected_paths)
        missing = sorted(expected_paths - actual_paths)
        raise InstallerError(f"Staged payload file set mismatch; extra={extra}, missing={missing}")
    for entry in entries:
        path = root / Path(str(entry["path"]))
        if path.stat().st_size != entry["size"] or sha256_file(path) != entry["sha256"]:
            raise InstallerError(f"Staged payload checksum mismatch: {entry['path']}")

    checksum_entries = entries + [
        {
            "path": MANIFEST_NAME,
            "sha256": sha256_file(manifest_path),
            "size": manifest_path.stat().st_size,
            "source_kind": "generated-metadata",
        }
    ]
    expected_checksums = "".join(
        f"{entry['sha256']}  {entry['path']}\n"
        for entry in sorted(checksum_entries, key=lambda entry: str(entry["path"]).casefold())
    )
    try:
        actual_checksums = checksums_path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        raise InstallerError(f"Unable to read staged SHA256SUMS: {exc}") from exc
    if actual_checksums != expected_checksums:
        raise InstallerError("Staged SHA256SUMS does not match the exact manifest payload.")
    return manifest


def archive_payload(stage_root: Path, output: Path) -> tuple[Path, Path]:
    manifest = verify_stage(stage_root)
    output = output.resolve(strict=False)
    checksum_output = Path(f"{output}.sha256")
    if output.exists() or checksum_output.exists():
        raise InstallerError(f"Archive output must not already exist: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    root_name = f"Tainted-Grail-FoA-SDK-{manifest['product_version']}-windows-x64"
    temporary = output.with_name(f".{output.name}.tmp-{uuid.uuid4().hex}")
    try:
        with zipfile.ZipFile(
            temporary,
            "x",
            compression=zipfile.ZIP_DEFLATED,
            compresslevel=9,
            allowZip64=True,
        ) as archive:
            for relative, path in sorted(
                iter_regular_files(stage_root), key=lambda pair: pair[0].as_posix().casefold()
            ):
                member = f"{root_name}/{relative.as_posix()}"
                info = zipfile.ZipInfo(member, date_time=(1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.create_system = 3
                info.external_attr = (0o100644 & 0xFFFF) << 16
                with path.open("rb") as input_stream, archive.open(info, "w", force_zip64=True) as output_stream:
                    shutil.copyfileobj(input_stream, output_stream, length=1024 * 1024)
        os.replace(temporary, output)
        archive_digest = sha256_file(output)
        write_exclusive(checksum_output, f"{archive_digest}  {output.name}\n".encode("utf-8"))
        verify_archive(output)
    except BaseException:
        temporary.unlink(missing_ok=True)
        output.unlink(missing_ok=True)
        checksum_output.unlink(missing_ok=True)
        raise
    return output, checksum_output


def verify_archive(archive_path: Path) -> dict[str, object]:
    require_regular_file(archive_path, "Portable SDK archive")
    try:
        with zipfile.ZipFile(archive_path, "r") as archive:
            members = [member for member in archive.infolist() if not member.is_dir()]
            if not members:
                raise InstallerError("Portable SDK archive is empty.")
            roots = {PurePosixPath(member.filename).parts[0] for member in members}
            if len(roots) != 1:
                raise InstallerError("Portable SDK archive must contain exactly one product root.")
            root = next(iter(roots))
            relative_members: dict[str, zipfile.ZipInfo] = {}
            identities: set[str] = set()
            for member in members:
                path = PurePosixPath(member.filename)
                if len(path.parts) < 2 or "\\" in member.filename:
                    raise InstallerError(f"Portable SDK archive member is unsafe: {member.filename}")
                relative = PurePosixPath(*path.parts[1:])
                identity = validate_destination(relative)
                if identity in identities:
                    raise InstallerError(f"Portable SDK archive contains a Windows path collision: {relative}")
                identities.add(identity)
                relative_members[relative.as_posix()] = member
            manifest_member = relative_members.get(MANIFEST_NAME)
            if manifest_member is None:
                raise InstallerError("Portable SDK archive is missing INSTALL_MANIFEST.json.")
            try:
                manifest_bytes = archive.read(manifest_member)
                manifest = parse_manifest_document(
                    parse_json_bytes(manifest_bytes, "portable SDK INSTALL_MANIFEST.json")
                )
            except (UnicodeError, json.JSONDecodeError) as exc:
                raise InstallerError(f"Portable SDK manifest is invalid JSON: {exc}") from exc
            entries = validate_manifest_entries(manifest["entries"])
            expected = {str(entry["path"]) for entry in entries} | {MANIFEST_NAME, CHECKSUMS_NAME}
            if set(relative_members) != expected:
                raise InstallerError("Portable SDK archive does not match the exact staged manifest file set.")
            for entry in entries:
                member = relative_members[str(entry["path"])]
                digest = hashlib.sha256()
                size = 0
                with archive.open(member, "r") as stream:
                    while chunk := stream.read(1024 * 1024):
                        digest.update(chunk)
                        size += len(chunk)
                if size != entry["size"] or digest.hexdigest() != entry["sha256"]:
                    raise InstallerError(f"Portable SDK archive checksum mismatch: {entry['path']}")
            checksum_entries = entries + [
                {
                    "path": MANIFEST_NAME,
                    "sha256": sha256_bytes(manifest_bytes),
                    "size": len(manifest_bytes),
                    "source_kind": "generated-metadata",
                }
            ]
            expected_checksums = "".join(
                f"{entry['sha256']}  {entry['path']}\n"
                for entry in sorted(
                    checksum_entries, key=lambda entry: str(entry["path"]).casefold()
                )
            ).encode("utf-8")
            if archive.read(relative_members[CHECKSUMS_NAME]) != expected_checksums:
                raise InstallerError("Portable SDK SHA256SUMS does not match the exact manifest payload.")
            expected_root = f"Tainted-Grail-FoA-SDK-{manifest['product_version']}-windows-x64"
            if root != expected_root:
                raise InstallerError(f"Portable SDK archive root is {root!r}; expected {expected_root!r}.")
            return manifest
    except (OSError, UnicodeError, json.JSONDecodeError, zipfile.BadZipFile) as exc:
        if isinstance(exc, InstallerError):
            raise
        raise InstallerError(f"Unable to verify portable SDK archive {archive_path}: {exc}") from exc


def add_common_inputs(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--notices", type=Path, required=True)
    parser.add_argument("--packages", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument(
        "--channel",
        choices=CHANNELS,
        default="development",
    )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    inventory = subparsers.add_parser("inventory", help="Create a reviewable exact payload inventory.")
    add_common_inputs(inventory)
    inventory.add_argument("--output", type=Path, required=True)

    stage = subparsers.add_parser("stage", help="Stage an exact redistribution-reviewed payload.")
    add_common_inputs(stage)
    stage.add_argument("--output", type=Path, required=True)
    stage.add_argument("--approved-inventory-sha256", required=True)
    stage.add_argument("--reviewed-by", required=True)
    stage.add_argument("--reviewed-at-utc", required=True)
    stage.add_argument("--review-evidence", required=True)

    verify = subparsers.add_parser("verify-stage", help="Verify a staged payload and manifest.")
    verify.add_argument("--stage-root", type=Path, required=True)

    archive = subparsers.add_parser("archive", help="Create a deterministic portable ZIP.")
    archive.add_argument("--stage-root", type=Path, required=True)
    archive.add_argument("--output", type=Path, required=True)

    verify_zip = subparsers.add_parser("verify-archive", help="Verify a portable ZIP.")
    verify_zip.add_argument("--archive", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        if arguments.command in {"inventory", "stage"}:
            source_commit = derive_clean_repository_commit(arguments.repo_root)
            inventory, sources = build_inventory(
                arguments.repo_root,
                arguments.sdk_root,
                arguments.notices,
                arguments.packages,
                arguments.version,
                source_commit,
                arguments.channel,
            )
            if arguments.command == "inventory":
                write_exclusive(arguments.output.resolve(strict=False), canonical_json_bytes(inventory))
                print(inventory["inventory_sha256"])
            else:
                staged = stage_payload(
                    inventory,
                    sources,
                    arguments.output,
                    arguments.approved_inventory_sha256,
                    arguments.reviewed_by,
                    arguments.reviewed_at_utc,
                    arguments.review_evidence,
                )
                print(staged)
        elif arguments.command == "verify-stage":
            manifest = verify_stage(arguments.stage_root)
            print(f"Verified {len(manifest['entries'])} staged payload files.")
        elif arguments.command == "archive":
            archive, checksum = archive_payload(arguments.stage_root, arguments.output)
            print(f"{archive}\n{checksum}")
        elif arguments.command == "verify-archive":
            manifest = verify_archive(arguments.archive)
            print(f"Verified portable archive for {manifest['product_version']}.")
    except InstallerError as exc:
        print(f"Installer tooling failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
