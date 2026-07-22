#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Acquire only exact, reviewed FOA-SDK knowledge and project-owned fallback assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import stat
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Callable, Mapping, Sequence

PACKAGE_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = Path(__file__).resolve().parents[4]
DEFAULT_MANIFEST = PACKAGE_ROOT / "approved-sources.json"
PLAN_NAME = "ACQUISITION_PLAN.json"
RECEIPT_NAME = "ACQUISITION_RECEIPT.json"
PROVIDER_ID = "extension.approved-acquisition"
GITHUB_REPOSITORY = "theb0yys/FOA-SDK"
GITHUB_RAW_HOST = "raw.githubusercontent.com"
AUTHORITY_FIELDS = (
    "catalog_mutation_allowed",
    "deployment_allowed",
    "evidence_promotion_allowed",
    "publication_allowed",
    "runtime_execution_allowed",
)
ID_RE = re.compile(r"^[a-z0-9]+(?:[.-][a-z0-9]+)*$")
REASON_RE = re.compile(r"^[a-z0-9]+(?:[._-][a-z0-9]+)*$")
SHA_RE = re.compile(r"^[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
SEMVER_RE = re.compile(
    r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    r"(?:-[0-9A-Za-z.-]+)?$"
)
TIME_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
MEDIA_TYPES = {"application/json", "image/svg+xml"}
HARD_LIMITS = {
    "maximum_files": 64,
    "maximum_file_bytes": 8 << 20,
    "maximum_total_bytes": 32 << 20,
}


class AcquisitionError(RuntimeError):
    """Raised when acquisition input, retrieval, or publication fails closed."""


@dataclass(frozen=True)
class ApprovedFile:
    package_id: str
    package_version: str
    license_expression: str
    source_path: str
    bundle_path: str
    byte_count: int
    sha256: str
    media_type: str


@dataclass(frozen=True)
class ApprovedManifest:
    raw: Mapping[str, object]
    repository: str
    commit: str
    maximum_files: int
    maximum_file_bytes: int
    maximum_total_bytes: int
    blocked_patterns: tuple[str, ...]
    packages: tuple[Mapping[str, object], ...]
    files: tuple[ApprovedFile, ...]


class NoRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise AcquisitionError("Pinned-GitHub acquisition refuses redirects.")


def canonical_json_bytes(value: object) -> bytes:
    return (
        json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n"
    ).encode()


def sha256_hex(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def is_relative_to(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def exact_keys(value: Mapping[str, object], keys: set[str], label: str) -> None:
    if set(value) != keys:
        raise AcquisitionError(f"{label} has an unsupported shape.")


def relative_path(raw: object, label: str) -> str:
    if not isinstance(raw, str) or not raw or "\\" in raw or "\x00" in raw:
        raise AcquisitionError(f"{label} must be a canonical relative POSIX path.")
    path = PurePosixPath(raw)
    if (
        path.is_absolute()
        or path.as_posix() != raw
        or any(part in {"", ".", ".."} for part in path.parts)
    ):
        raise AcquisitionError(f"{label} escapes the approved path boundary.")
    return raw


def blocked_pattern(raw: object) -> str:
    if not isinstance(raw, str) or not raw or "\\" in raw or "\x00" in raw:
        raise AcquisitionError("Blocked-source pattern must be a canonical POSIX glob.")
    path = PurePosixPath(raw)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise AcquisitionError("Blocked-source pattern escapes the approved path boundary.")
    if not any(character in raw for character in "*?["):
        raise AcquisitionError("Blocked-source pattern must contain an explicit glob token.")
    return raw


def path_is_blocked(path: str, patterns: Sequence[str]) -> bool:
    candidate = PurePosixPath(path)
    return any(candidate.match(pattern) for pattern in patterns)


def load_manifest(path: Path = DEFAULT_MANIFEST) -> ApprovedManifest:
    try:
        payload = path.read_bytes()
        document = json.loads(payload.decode())
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise AcquisitionError("Unable to read approved acquisition manifest.") from exc
    if not isinstance(document, dict) or payload != canonical_json_bytes(document):
        raise AcquisitionError("Approved acquisition manifest must be canonical JSON.")
    exact_keys(
        document,
        {
            "authority",
            "blocked_sources",
            "limits",
            "packages",
            "provider_id",
            "schema_version",
            "source",
        },
        "manifest",
    )
    if document["schema_version"] != 1 or document["provider_id"] != PROVIDER_ID:
        raise AcquisitionError("Unsupported acquisition manifest identity.")

    source = document["source"]
    if not isinstance(source, dict):
        raise AcquisitionError("source must be an object.")
    exact_keys(source, {"commit", "display_branch", "repository"}, "source")
    repository, commit = source["repository"], source["commit"]
    if (
        repository != GITHUB_REPOSITORY
        or not isinstance(commit, str)
        or COMMIT_RE.fullmatch(commit) is None
    ):
        raise AcquisitionError("Source repository or exact commit is not approved.")
    if (
        not isinstance(source["display_branch"], str)
        or not source["display_branch"]
        or len(source["display_branch"]) > 128
    ):
        raise AcquisitionError("display_branch is invalid.")

    authority = document["authority"]
    if not isinstance(authority, dict):
        raise AcquisitionError("authority must be an object.")
    exact_keys(authority, set(AUTHORITY_FIELDS), "authority")
    if any(authority[name] is not False for name in AUTHORITY_FIELDS):
        raise AcquisitionError("Acquisition may not grant operational authority.")

    limits = document["limits"]
    if not isinstance(limits, dict):
        raise AcquisitionError("limits must be an object.")
    exact_keys(limits, set(HARD_LIMITS), "limits")
    for name, hard_limit in HARD_LIMITS.items():
        if (
            isinstance(limits[name], bool)
            or not isinstance(limits[name], int)
            or not 1 <= limits[name] <= hard_limit
        ):
            raise AcquisitionError(f"{name} exceeds its hard boundary.")

    blocked = document["blocked_sources"]
    if not isinstance(blocked, list) or not blocked:
        raise AcquisitionError("blocked_sources is required.")
    pairs: list[tuple[str, str]] = []
    for entry in blocked:
        if not isinstance(entry, dict):
            raise AcquisitionError("Blocked-source records must be objects.")
        exact_keys(entry, {"pattern", "reason"}, "blocked source")
        pattern = blocked_pattern(entry["pattern"])
        reason = entry["reason"]
        if not isinstance(reason, str) or REASON_RE.fullmatch(reason) is None:
            raise AcquisitionError("Blocked-source reason is invalid.")
        pairs.append((pattern, reason))
    if pairs != sorted(pairs) or len({pattern for pattern, _ in pairs}) != len(pairs):
        raise AcquisitionError("blocked_sources must be unique and sorted.")
    patterns = tuple(pattern for pattern, _ in pairs)

    packages = document["packages"]
    if not isinstance(packages, list) or not packages:
        raise AcquisitionError("At least one approved package is required.")
    all_files: list[ApprovedFile] = []
    package_ids: set[str] = set()
    source_paths: set[str] = set()
    bundle_paths: set[str] = set()
    previous_package = ""
    total_bytes = 0
    for package in packages:
        if not isinstance(package, dict):
            raise AcquisitionError("Package records must be objects.")
        exact_keys(
            package,
            {"files", "id", "license", "redistribution_reviewed", "version"},
            "package",
        )
        package_id = package["id"]
        version = package["version"]
        license_expression = package["license"]
        if (
            not isinstance(package_id, str)
            or ID_RE.fullmatch(package_id) is None
            or package_id <= previous_package
            or package_id in package_ids
        ):
            raise AcquisitionError("Package IDs must be valid, unique, and sorted.")
        previous_package = package_id
        package_ids.add(package_id)
        if not isinstance(version, str) or SEMVER_RE.fullmatch(version) is None:
            raise AcquisitionError(
                f"Package {package_id} has an invalid semantic version."
            )
        if (
            not isinstance(license_expression, str)
            or license_expression in {"", "NOASSERTION"}
            or package["redistribution_reviewed"] is not True
        ):
            raise AcquisitionError(
                f"Package {package_id} lacks redistribution approval."
            )
        files = package["files"]
        if not isinstance(files, list) or not files:
            raise AcquisitionError(f"Package {package_id} has no approved files.")
        previous_bundle = ""
        for item in files:
            if not isinstance(item, dict):
                raise AcquisitionError("Approved file records must be objects.")
            exact_keys(
                item,
                {"bundle_path", "bytes", "media_type", "sha256", "source_path"},
                "approved file",
            )
            source_path = relative_path(item["source_path"], "source_path")
            bundle_path = relative_path(item["bundle_path"], "bundle_path")
            if path_is_blocked(source_path, patterns) or path_is_blocked(
                bundle_path, patterns
            ):
                raise AcquisitionError(
                    f"Approved path is prohibited by blocked_sources: {source_path}."
                )
            byte_count = item["bytes"]
            digest = item["sha256"]
            media_type = item["media_type"]
            if (
                source_path in source_paths
                or bundle_path in bundle_paths
                or bundle_path <= previous_bundle
            ):
                raise AcquisitionError(
                    "Approved paths must be unique and bundle paths sorted."
                )
            previous_bundle = bundle_path
            source_paths.add(source_path)
            bundle_paths.add(bundle_path)
            if (
                isinstance(byte_count, bool)
                or not isinstance(byte_count, int)
                or not 1 <= byte_count <= limits["maximum_file_bytes"]
            ):
                raise AcquisitionError(f"{source_path} exceeds the file-size limit.")
            if (
                not isinstance(digest, str)
                or SHA_RE.fullmatch(digest) is None
                or media_type not in MEDIA_TYPES
            ):
                raise AcquisitionError(
                    f"{source_path} has invalid digest or media metadata."
                )
            total_bytes += byte_count
            all_files.append(
                ApprovedFile(
                    package_id,
                    version,
                    license_expression,
                    source_path,
                    bundle_path,
                    byte_count,
                    digest,
                    media_type,
                )
            )
    if (
        len(all_files) > limits["maximum_files"]
        or total_bytes > limits["maximum_total_bytes"]
    ):
        raise AcquisitionError("Approved inventory exceeds aggregate limits.")
    return ApprovedManifest(
        document,
        repository,
        commit,
        limits["maximum_files"],
        limits["maximum_file_bytes"],
        limits["maximum_total_bytes"],
        patterns,
        tuple(packages),
        tuple(all_files),
    )


def selected_files(
    manifest: ApprovedManifest,
    package_ids: Sequence[str],
) -> tuple[ApprovedFile, ...]:
    if len(package_ids) != len(set(package_ids)):
        raise AcquisitionError("Package selection contains duplicates.")
    known = {str(package["id"]) for package in manifest.packages}
    selected = known if not package_ids else set(package_ids)
    unknown = sorted(selected - known)
    if unknown:
        raise AcquisitionError(
            "Unknown approved package IDs: " + ", ".join(unknown)
        )
    return tuple(item for item in manifest.files if item.package_id in selected)


def build_plan(
    manifest: ApprovedManifest,
    *,
    provider: str,
    package_ids: Sequence[str] = (),
) -> dict[str, object]:
    if provider not in {"local", "pinned-github"}:
        raise AcquisitionError("Provider must be local or pinned-github.")
    files = selected_files(manifest, package_ids)
    plan: dict[str, object] = {
        "authority": dict(manifest.raw["authority"]),
        "files": [
            {
                "bundle_path": item.bundle_path,
                "bytes": item.byte_count,
                "license": item.license_expression,
                "media_type": item.media_type,
                "package_id": item.package_id,
                "sha256": item.sha256,
                "source_path": item.source_path,
            }
            for item in files
        ],
        "packages": sorted({item.package_id for item in files}),
        "provider_id": PROVIDER_ID,
        "route": provider,
        "schema_version": 1,
        "source": {"commit": manifest.commit, "repository": manifest.repository},
        "totals": {
            "files": len(files),
            "bytes": sum(item.byte_count for item in files),
        },
    }
    plan["plan_id"] = "sha256:" + sha256_hex(canonical_json_bytes(plan))
    return plan


def validate_plan(
    plan: Mapping[str, object],
    manifest: ApprovedManifest,
) -> tuple[ApprovedFile, ...]:
    exact_keys(
        plan,
        {
            "authority",
            "files",
            "packages",
            "plan_id",
            "provider_id",
            "route",
            "schema_version",
            "source",
            "totals",
        },
        "plan",
    )
    body = dict(plan)
    plan_id = body.pop("plan_id")
    if plan_id != "sha256:" + sha256_hex(canonical_json_bytes(body)):
        raise AcquisitionError("Plan fingerprint is invalid.")
    packages = plan["packages"]
    if (
        not isinstance(packages, list)
        or any(
            not isinstance(item, str) or ID_RE.fullmatch(item) is None
            for item in packages
        )
        or packages != sorted(set(packages))
    ):
        raise AcquisitionError("Plan package IDs are invalid.")
    route = plan["route"]
    if (
        plan["schema_version"] != 1
        or plan["provider_id"] != PROVIDER_ID
        or route not in {"local", "pinned-github"}
    ):
        raise AcquisitionError("Plan identity is invalid.")
    expected = build_plan(
        manifest,
        provider=str(route),
        package_ids=packages,
    )
    if any(
        plan[name] != expected[name]
        for name in ("authority", "files", "packages", "source", "totals")
    ):
        raise AcquisitionError("Plan differs from the approved manifest.")
    return selected_files(manifest, packages)


def validate_timestamp(value: str | None) -> str:
    if value is None:
        return (
            datetime.now(timezone.utc)
            .replace(microsecond=0)
            .strftime("%Y-%m-%dT%H:%M:%SZ")
        )
    if TIME_RE.fullmatch(value) is None:
        raise AcquisitionError("captured-at must use YYYY-MM-DDTHH:MM:SSZ.")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as exc:
        raise AcquisitionError("captured-at is invalid.") from exc
    return value


def validate_payload(payload: bytes, approved: ApprovedFile) -> None:
    if len(payload) != approved.byte_count:
        raise AcquisitionError(f"Byte count differs for {approved.source_path}.")
    if sha256_hex(payload) != approved.sha256:
        raise AcquisitionError(f"SHA-256 differs for {approved.source_path}.")


def is_link_or_reparse(path: Path) -> bool:
    try:
        metadata = path.lstat()
    except OSError as exc:
        raise AcquisitionError(f"Unable to inspect local source path: {path}.") from exc
    if stat.S_ISLNK(metadata.st_mode):
        return True
    attributes = getattr(metadata, "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    return bool(reparse_flag and attributes & reparse_flag)


def reject_linked_path_components(path: Path) -> None:
    absolute = path.expanduser().absolute()
    current = Path(absolute.anchor)
    for part in absolute.parts[1:]:
        current /= part
        if is_link_or_reparse(current):
            raise AcquisitionError(f"Local source path uses a symlink or reparse point: {current}.")


def contained_regular_file(root: Path, relative: str) -> Path:
    resolved_root = root.resolve(strict=True)
    current = resolved_root
    for part in PurePosixPath(relative).parts:
        current /= part
        if is_link_or_reparse(current):
            raise AcquisitionError(f"Approved source uses a symlink: {relative}.")
    resolved = current.resolve(strict=True)
    if not is_relative_to(resolved, resolved_root) or not resolved.is_file():
        raise AcquisitionError(f"Approved source escapes containment: {relative}.")
    return resolved


def read_exact_posix(root: Path, approved: ApprovedFile) -> bytes:
    flags = os.O_RDONLY | getattr(os, "O_CLOEXEC", 0) | getattr(os, "O_NOFOLLOW", 0)
    directory_flags = flags | getattr(os, "O_DIRECTORY", 0)
    descriptors: list[int] = []
    try:
        current = os.open(root, directory_flags)
        descriptors.append(current)
        parts = PurePosixPath(approved.source_path).parts
        for part in parts[:-1]:
            current = os.open(part, directory_flags, dir_fd=current)
            descriptors.append(current)
        file_descriptor = os.open(parts[-1], flags, dir_fd=current)
        descriptors.append(file_descriptor)
        metadata = os.fstat(file_descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise AcquisitionError(
                f"Approved source is not a regular file: {approved.source_path}."
            )
        with os.fdopen(os.dup(file_descriptor), "rb") as stream:
            payload = stream.read(approved.byte_count + 1)
    except OSError as exc:
        raise AcquisitionError(
            f"Descriptor-safe local read failed for {approved.source_path}."
        ) from exc
    finally:
        for descriptor in reversed(descriptors):
            try:
                os.close(descriptor)
            except OSError:
                pass
    validate_payload(payload, approved)
    return payload


def _normalise_windows_final_path(value: str) -> Path:
    if value.startswith("\\\\?\\UNC\\"):
        value = "\\\\" + value[8:]
    elif value.startswith("\\\\?\\"):
        value = value[4:]
    return Path(value)


def read_exact_windows(root: Path, approved: ApprovedFile) -> bytes:
    import ctypes
    import msvcrt
    from ctypes import wintypes

    path = contained_regular_file(root, approved.source_path)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    create_file = kernel32.CreateFileW
    create_file.argtypes = (
        wintypes.LPCWSTR,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.LPVOID,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.HANDLE,
    )
    create_file.restype = wintypes.HANDLE
    get_final_path = kernel32.GetFinalPathNameByHandleW
    get_final_path.argtypes = (
        wintypes.HANDLE,
        wintypes.LPWSTR,
        wintypes.DWORD,
        wintypes.DWORD,
    )
    get_final_path.restype = wintypes.DWORD
    close_handle = kernel32.CloseHandle
    close_handle.argtypes = (wintypes.HANDLE,)
    close_handle.restype = wintypes.BOOL

    generic_read = 0x80000000
    share_all = 0x00000001 | 0x00000002 | 0x00000004
    open_existing = 3
    open_reparse_point = 0x00200000
    invalid_handle = ctypes.c_void_p(-1).value
    handle = create_file(
        str(path),
        generic_read,
        share_all,
        None,
        open_existing,
        open_reparse_point,
        None,
    )
    if handle == invalid_handle:
        raise AcquisitionError(
            f"Descriptor-safe local read failed for {approved.source_path}."
        )
    transferred = False
    try:
        size = 32768
        buffer = ctypes.create_unicode_buffer(size)
        count = get_final_path(handle, buffer, size, 0)
        if count == 0 or count >= size:
            raise AcquisitionError("Unable to resolve the opened local source handle.")
        final_path = _normalise_windows_final_path(buffer.value).resolve(strict=True)
        resolved_root = root.resolve(strict=True)
        if not is_relative_to(final_path, resolved_root):
            raise AcquisitionError(
                f"Opened local source escaped containment: {approved.source_path}."
            )
        descriptor = msvcrt.open_osfhandle(int(handle), os.O_RDONLY | os.O_BINARY)
        transferred = True
        with os.fdopen(descriptor, "rb") as stream:
            metadata = os.fstat(stream.fileno())
            if not stat.S_ISREG(metadata.st_mode):
                raise AcquisitionError(
                    f"Approved source is not a regular file: {approved.source_path}."
                )
            payload = stream.read(approved.byte_count + 1)
    finally:
        if not transferred:
            close_handle(handle)
    validate_payload(payload, approved)
    return payload


def read_exact_local(root: Path, approved: ApprovedFile) -> bytes:
    reject_linked_path_components(root)
    contained_regular_file(root, approved.source_path)
    if os.name == "posix" and hasattr(os, "O_NOFOLLOW"):
        return read_exact_posix(root, approved)
    if os.name == "nt":
        return read_exact_windows(root, approved)
    with contained_regular_file(root, approved.source_path).open("rb") as stream:
        payload = stream.read(approved.byte_count + 1)
    validate_payload(payload, approved)
    return payload


def pinned_github_url(
    manifest: ApprovedManifest,
    approved: ApprovedFile,
) -> str:
    owner, repository = manifest.repository.split("/", 1)
    return (
        f"https://{GITHUB_RAW_HOST}/{owner}/{repository}/{manifest.commit}/"
        f"{urllib.parse.quote(approved.source_path, safe='/')}"
    )


def fetch_pinned_github(
    manifest: ApprovedManifest,
    approved: ApprovedFile,
) -> bytes:
    request = urllib.request.Request(
        pinned_github_url(manifest, approved),
        headers={"User-Agent": "FOA-SDK-ApprovedAcquisition/0.1.0"},
        method="GET",
    )
    try:
        with urllib.request.build_opener(NoRedirectHandler()).open(
            request, timeout=30
        ) as response:
            final = urllib.parse.urlparse(response.geturl())
            if final.scheme != "https" or final.hostname != GITHUB_RAW_HOST:
                raise AcquisitionError(
                    "Pinned-GitHub response escaped the approved host."
                )
            length = response.headers.get("Content-Length")
            if length is not None and int(length) != approved.byte_count:
                raise AcquisitionError(
                    f"Pinned-GitHub size differs for {approved.source_path}."
                )
            payload = response.read(approved.byte_count + 1)
    except AcquisitionError:
        raise
    except (OSError, ValueError, urllib.error.URLError) as exc:
        raise AcquisitionError(
            f"Pinned-GitHub retrieval failed for {approved.source_path}."
        ) from exc
    validate_payload(payload, approved)
    return payload


def validate_external_output_path(output: Path) -> Path:
    candidate = output.expanduser().resolve(strict=False)
    repository = REPO_ROOT.resolve(strict=True)
    if candidate == repository or is_relative_to(candidate, repository):
        raise AcquisitionError(
            "Acquisition output must remain outside the FOA-SDK checkout."
        )
    if candidate.exists():
        raise AcquisitionError(
            "Acquisition output already exists; overwrite is prohibited."
        )
    candidate.parent.mkdir(parents=True, exist_ok=True)
    return candidate


def write_exclusive(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as stream:
        stream.write(payload)
        stream.flush()
        os.fsync(stream.fileno())


def fsync_directory(path: Path) -> None:
    if os.name != "posix":
        return
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def make_receipt(
    plan: Mapping[str, object],
    captured: str,
    files: Sequence[ApprovedFile],
) -> dict[str, object]:
    receipt: dict[str, object] = {
        "authority": dict(plan["authority"]),
        "candidate_evidence_created": False,
        "captured_at_utc": captured,
        "files": [
            {
                "bundle_path": item.bundle_path,
                "bytes": item.byte_count,
                "license": item.license_expression,
                "media_type": item.media_type,
                "package_id": item.package_id,
                "sha256": item.sha256,
            }
            for item in files
        ],
        "plan_id": plan["plan_id"],
        "provider_id": PROVIDER_ID,
        "route": plan["route"],
        "schema_version": 1,
        "source": dict(plan["source"]),
        "verification": {
            "all_files_verified": True,
            "bytes": sum(item.byte_count for item in files),
            "files": len(files),
        },
    }
    receipt["receipt_id"] = "sha256:" + sha256_hex(
        canonical_json_bytes(receipt)
    )
    return receipt


PayloadReader = Callable[[ApprovedManifest, ApprovedFile], bytes]


def acquire(
    manifest: ApprovedManifest,
    *,
    provider: str,
    output: Path,
    package_ids: Sequence[str] = (),
    source_root: Path | None = None,
    captured_at_utc: str | None = None,
    github_reader: PayloadReader = fetch_pinned_github,
) -> dict[str, object]:
    plan = build_plan(manifest, provider=provider, package_ids=package_ids)
    files = validate_plan(plan, manifest)
    destination = validate_external_output_path(output)
    captured = validate_timestamp(captured_at_utc)
    local_root: Path | None = None
    if provider == "local":
        if source_root is None:
            raise AcquisitionError("The local provider requires --source-root.")
        expanded = source_root.expanduser().absolute()
        reject_linked_path_components(expanded)
        local_root = expanded.resolve(strict=True)
        if not local_root.is_dir():
            raise AcquisitionError("Local source root must be a directory.")
    elif source_root is not None:
        raise AcquisitionError("--source-root is valid only for the local provider.")

    temporary = Path(
        tempfile.mkdtemp(prefix=f".{destination.name}.tmp-", dir=destination.parent)
    )
    try:
        for approved in files:
            payload = (
                read_exact_local(local_root, approved)
                if local_root is not None
                else github_reader(manifest, approved)
            )
            write_exclusive(temporary / approved.bundle_path, payload)
        write_exclusive(temporary / PLAN_NAME, canonical_json_bytes(plan))
        receipt = make_receipt(plan, captured, files)
        write_exclusive(temporary / RECEIPT_NAME, canonical_json_bytes(receipt))
        fsync_directory(temporary)
        os.replace(temporary, destination)
        fsync_directory(destination.parent)
        return receipt
    except Exception:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def read_canonical_json(path: Path, label: str) -> dict[str, object]:
    try:
        payload = path.read_bytes()
        value = json.loads(payload.decode())
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise AcquisitionError(f"Unable to read {label}.") from exc
    if not isinstance(value, dict) or payload != canonical_json_bytes(value):
        raise AcquisitionError(f"{label} must be canonical JSON.")
    return value


def verify_bundle(
    bundle: Path,
    manifest: ApprovedManifest,
) -> dict[str, object]:
    root = bundle.expanduser().resolve(strict=True)
    if not root.is_dir():
        raise AcquisitionError("Acquisition bundle must be a directory.")
    plan = read_canonical_json(root / PLAN_NAME, "acquisition plan")
    files = validate_plan(plan, manifest)
    receipt = read_canonical_json(root / RECEIPT_NAME, "acquisition receipt")
    exact_keys(
        receipt,
        {
            "authority",
            "candidate_evidence_created",
            "captured_at_utc",
            "files",
            "plan_id",
            "provider_id",
            "receipt_id",
            "route",
            "schema_version",
            "source",
            "verification",
        },
        "receipt",
    )
    body = dict(receipt)
    receipt_id = body.pop("receipt_id")
    if receipt_id != "sha256:" + sha256_hex(canonical_json_bytes(body)):
        raise AcquisitionError("Receipt fingerprint is invalid.")
    expected = make_receipt(
        plan,
        validate_timestamp(str(receipt["captured_at_utc"])),
        files,
    )
    if (
        any(
            receipt[name] != expected[name]
            for name in expected
            if name != "receipt_id"
        )
        or receipt["receipt_id"] != expected["receipt_id"]
    ):
        raise AcquisitionError("Receipt differs from the approved plan.")
    if receipt["candidate_evidence_created"] is not False:
        raise AcquisitionError(
            "Acquisition may not claim candidate-evidence creation."
        )

    expected_files = {
        PLAN_NAME,
        RECEIPT_NAME,
        *(item.bundle_path for item in files),
    }
    expected_dirs = {
        parent.as_posix()
        for name in expected_files
        for parent in PurePosixPath(name).parents
        if parent.as_posix() != "."
    }
    actual_files: set[str] = set()
    actual_dirs: set[str] = set()
    for current, directories, filenames in os.walk(root, followlinks=False):
        current_path = Path(current)
        for directory in directories:
            path = current_path / directory
            if is_link_or_reparse(path):
                raise AcquisitionError("Acquisition bundle contains a symlink.")
            actual_dirs.add(path.relative_to(root).as_posix())
        for filename in filenames:
            path = current_path / filename
            if is_link_or_reparse(path):
                raise AcquisitionError("Acquisition bundle contains a symlink.")
            actual_files.add(path.relative_to(root).as_posix())
    if actual_files != expected_files or actual_dirs != expected_dirs:
        raise AcquisitionError(
            "Acquisition bundle contains missing or unexpected paths."
        )
    for approved in files:
        validate_payload(
            contained_regular_file(root, approved.bundle_path).read_bytes(),
            approved,
        )
    return receipt


def list_packages(manifest: ApprovedManifest) -> dict[str, object]:
    return {
        "packages": [
            {
                "files": len(package["files"]),
                "id": package["id"],
                "license": package["license"],
                "version": package["version"],
            }
            for package in manifest.packages
        ],
        "provider_id": PROVIDER_ID,
        "source": {"commit": manifest.commit, "repository": manifest.repository},
    }


def write_json_output(value: object, output: Path | None) -> None:
    payload = canonical_json_bytes(value)
    if output is None:
        sys.stdout.buffer.write(payload)
    else:
        write_exclusive(validate_external_output_path(output), payload)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    commands = parser.add_subparsers(dest="command", required=True)
    listing = commands.add_parser("list")
    listing.add_argument("--output", type=Path)
    planning = commands.add_parser("plan")
    planning.add_argument(
        "--provider", choices=("local", "pinned-github"), required=True
    )
    planning.add_argument("--package", action="append", default=[])
    planning.add_argument("--output", type=Path)
    acquiring = commands.add_parser("acquire")
    acquiring.add_argument(
        "--provider", choices=("local", "pinned-github"), required=True
    )
    acquiring.add_argument("--package", action="append", default=[])
    acquiring.add_argument("--source-root", type=Path)
    acquiring.add_argument("--output", type=Path, required=True)
    acquiring.add_argument("--captured-at")
    verifying = commands.add_parser("verify")
    verifying.add_argument("--bundle", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        manifest = load_manifest(arguments.manifest)
        if arguments.command == "list":
            write_json_output(list_packages(manifest), arguments.output)
        elif arguments.command == "plan":
            write_json_output(
                build_plan(
                    manifest,
                    provider=arguments.provider,
                    package_ids=arguments.package,
                ),
                arguments.output,
            )
        elif arguments.command == "acquire":
            receipt = acquire(
                manifest,
                provider=arguments.provider,
                output=arguments.output,
                package_ids=arguments.package,
                source_root=arguments.source_root,
                captured_at_utc=arguments.captured_at,
            )
            sys.stdout.buffer.write(canonical_json_bytes(receipt))
        else:
            sys.stdout.buffer.write(
                canonical_json_bytes(verify_bundle(arguments.bundle, manifest))
            )
        return 0
    except (AcquisitionError, OSError) as exc:
        print(f"Approved acquisition failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
