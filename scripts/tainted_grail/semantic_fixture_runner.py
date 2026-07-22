#!/usr/bin/env python3
"""Validate Semantic Hook fixture manifests without runtime authority."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

EXIT_VALID = 0
EXIT_INVALID = 1
EXIT_USAGE_OR_IO = 2
_SHA40 = set("0123456789abcdef")


@dataclass(frozen=True)
class ManifestResult:
    path: str
    sha256: str
    fixture_set_id: str
    case_count: int
    status: str
    errors: tuple[str, ...]

    def to_dict(self) -> dict[str, Any]:
        return {
            "path": self.path,
            "sha256": self.sha256,
            "fixture_set_id": self.fixture_set_id,
            "case_count": self.case_count,
            "status": self.status,
            "errors": list(self.errors),
        }


def _nonempty_string(value: Any) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _is_sha40(value: Any) -> bool:
    return isinstance(value, str) and len(value) == 40 and all(ch in _SHA40 for ch in value)


def _require(mapping: Any, key: str, errors: list[str], where: str) -> Any:
    if not isinstance(mapping, dict):
        errors.append(f"{where}: expected object")
        return None
    if key not in mapping:
        errors.append(f"{where}: missing {key}")
        return None
    return mapping[key]


def validate_manifest_bytes(data: bytes, display_path: str) -> ManifestResult:
    digest = hashlib.sha256(data).hexdigest()
    errors: list[str] = []
    fixture_set_id = ""
    case_count = 0

    try:
        doc = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        return ManifestResult(display_path, digest, "", 0, "invalid", (f"invalid JSON: {exc}",))

    if not isinstance(doc, dict):
        errors.append("$: expected object")
        doc = {}

    if _require(doc, "schema_version", errors, "$") != 1:
        errors.append("$.schema_version: expected 1")

    for key in ("source_repository", "source_commit", "authority", "acceptance_rule"):
        value = _require(doc, key, errors, "$")
        if not _nonempty_string(value):
            errors.append(f"$.{key}: expected non-empty string")

    if not _is_sha40(doc.get("source_commit")):
        errors.append("$.source_commit: expected lowercase 40-character SHA")

    if _require(doc, "execution_state", errors, "$") != "specification-only":
        errors.append("$.execution_state: must be specification-only")

    fixture_set_id_value = _require(doc, "fixture_set_id", errors, "$")
    if _nonempty_string(fixture_set_id_value):
        fixture_set_id = fixture_set_id_value.strip()
    else:
        errors.append("$.fixture_set_id: expected non-empty string")

    profile = _require(doc, "profile", errors, "$")
    if isinstance(profile, dict):
        for key in ("profile_id", "game_version", "unity_version", "runtime", "loader", "framework", "evidence_state"):
            if not _nonempty_string(profile.get(key)):
                errors.append(f"$.profile.{key}: expected non-empty string")
        if profile.get("runtime") != "Mono":
            errors.append("$.profile.runtime: Batch 004 fixtures must remain Mono-bound")
        fingerprints = profile.get("required_runtime_fingerprints")
        if not isinstance(fingerprints, dict) or not fingerprints:
            errors.append("$.profile.required_runtime_fingerprints: expected non-empty object")
        elif any(value != "required-but-absent" for value in fingerprints.values()):
            errors.append("$.profile.required_runtime_fingerprints: runner accepts only required-but-absent specification state")

    sources = _require(doc, "sources", errors, "$")
    if not isinstance(sources, list) or not sources:
        errors.append("$.sources: expected non-empty array")
    else:
        seen_paths: set[str] = set()
        for index, source in enumerate(sources):
            where = f"$.sources[{index}]"
            if not isinstance(source, dict):
                errors.append(f"{where}: expected object")
                continue
            path_value = source.get("path")
            if not _nonempty_string(path_value):
                errors.append(f"{where}.path: expected non-empty string")
            elif path_value in seen_paths:
                errors.append(f"{where}.path: duplicate source path")
            else:
                seen_paths.add(path_value)
            if not _is_sha40(source.get("blob")):
                errors.append(f"{where}.blob: expected lowercase 40-character SHA")

    cases = _require(doc, "cases", errors, "$")
    if not isinstance(cases, list) or not cases:
        errors.append("$.cases: expected non-empty array")
    else:
        case_count = len(cases)
        seen_case_ids: set[str] = set()
        for index, case in enumerate(cases):
            where = f"$.cases[{index}]"
            if not isinstance(case, dict):
                errors.append(f"{where}: expected object")
                continue
            case_id = case.get("case_id")
            if not _nonempty_string(case_id):
                errors.append(f"{where}.case_id: expected non-empty string")
            elif case_id in seen_case_ids:
                errors.append(f"{where}.case_id: duplicate case id")
            else:
                seen_case_ids.add(case_id)
            if not _nonempty_string(case.get("expected")):
                errors.append(f"{where}.expected: expected non-empty string")
            if not _nonempty_string(case.get("promotion_effect")):
                errors.append(f"{where}.promotion_effect: expected non-empty string")

    decision = _require(doc, "decision", errors, "$")
    if not isinstance(decision, dict):
        errors.append("$.decision: expected object")
    else:
        promotion = decision.get("promotion", decision.get("adapter_record_promotion"))
        if promotion != "none":
            errors.append("$.decision: promotion must be none")
        for key, value in decision.items():
            if "promotion" in key and key not in {"promotion", "adapter_record_promotion"}:
                if value not in {"none", "source-contract-verified-only"}:
                    errors.append(f"$.decision.{key}: unsupported promotion-bearing value")

    return ManifestResult(
        display_path,
        digest,
        fixture_set_id,
        case_count,
        "valid-specification" if not errors else "invalid",
        tuple(sorted(errors)),
    )


def _discover(inputs: Iterable[str]) -> list[Path]:
    discovered: set[Path] = set()
    for raw in inputs:
        path = Path(raw)
        if path.is_dir():
            discovered.update(p for p in path.iterdir() if p.is_file() and p.suffix == ".json")
        elif path.is_file():
            if path.suffix != ".json":
                raise ValueError(f"not a JSON manifest: {path}")
            discovered.add(path)
        else:
            raise FileNotFoundError(path)
    return sorted(discovered, key=lambda p: p.as_posix())


def build_receipt(results: list[ManifestResult]) -> dict[str, Any]:
    invalid = sum(result.status != "valid-specification" for result in results)
    return {
        "schema_version": 1,
        "authority": "offline-structure-validation-only",
        "runtime_authority": "none",
        "promotion": "none",
        "manifest_count": len(results),
        "case_count": sum(result.case_count for result in results),
        "status": "valid-specifications" if invalid == 0 else "invalid",
        "invalid_manifest_count": invalid,
        "manifests": [result.to_dict() for result in results],
    }


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Validate Semantic Hook fixture manifests without loading game or mod assemblies.")
    parser.add_argument("inputs", nargs="+", help="JSON manifest files or directories")
    parser.add_argument("--output", type=Path, help="write deterministic JSON receipt")
    parser.add_argument("--pretty", action="store_true", help="pretty-print receipt")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        paths = _discover(args.inputs)
        if not paths:
            raise ValueError("no JSON manifests found")
        results = [validate_manifest_bytes(path.read_bytes(), path.as_posix()) for path in paths]
    except (OSError, ValueError) as exc:
        print(f"semantic-fixture-runner: {exc}", file=sys.stderr)
        return EXIT_USAGE_OR_IO

    receipt = build_receipt(results)
    text = json.dumps(receipt, indent=2 if args.pretty else None, sort_keys=True, separators=None if args.pretty else (",", ":")) + "\n"

    if args.output:
        try:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(text, encoding="utf-8", newline="\n")
        except OSError as exc:
            print(f"semantic-fixture-runner: {exc}", file=sys.stderr)
            return EXIT_USAGE_OR_IO
    else:
        sys.stdout.write(text)

    return EXIT_VALID if receipt["status"] == "valid-specifications" else EXIT_INVALID


if __name__ == "__main__":
    raise SystemExit(main())
