"""Engine-neutral diagnostic path, serialization, and atomic publication utilities."""
from __future__ import annotations

import csv
import io
import json
import os
import re
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Mapping

from .errors import RepairError

_RESERVED_WINDOWS_NAMES = {
    "con", "prn", "aux", "nul",
    *(f"com{i}" for i in range(1, 10)),
    *(f"lpt{i}" for i in range(1, 10)),
}
_FORMULA_PREFIXES = ("=", "+", "-", "@")


@dataclass(frozen=True)
class DiagnosticLimits:
    field_bytes: int = 256
    row_bytes: int = 4096
    session_bytes: int = 65536
    session_segment_chars: int = 64

    def __post_init__(self) -> None:
        if min(
            self.field_bytes,
            self.row_bytes,
            self.session_bytes,
            self.session_segment_chars,
        ) <= 0:
            raise RepairError("diagnostic limits must be positive")
        if self.field_bytes > self.row_bytes or self.row_bytes > self.session_bytes:
            raise RepairError("diagnostic limits must be field <= row <= session")


@dataclass
class DiagnosticSession:
    root: Path
    session_name: str
    limits: DiagnosticLimits = field(default_factory=DiagnosticLimits)
    _published_bytes: int = 0

    def __post_init__(self) -> None:
        self.root = self.root.resolve(strict=False)
        self.session_name = validate_session_segment(
            self.session_name,
            self.limits.session_segment_chars,
        )

    @property
    def session_dir(self) -> Path:
        candidate = (self.root / self.session_name).resolve(strict=False)
        _require_descendant(self.root, candidate)
        return candidate

    @property
    def published_bytes(self) -> int:
        return self._published_bytes

    def encode_support_csv(
        self,
        values: Mapping[str, Any],
        *,
        sensitive_fields: set[str],
    ) -> bytes:
        unknown_sensitive = sensitive_fields.difference(values)
        if unknown_sensitive:
            raise RepairError("sensitive field classification references unknown fields")
        sanitized: list[str] = []
        for key in sorted(values):
            raw = "[REDACTED]" if key in sensitive_fields else str(values[key])
            neutralized = "'" + raw if raw.startswith(_FORMULA_PREFIXES) else raw
            data = neutralized.encode("utf-8")
            if len(data) > self.limits.field_bytes:
                marker = "…[TRUNCATED]"
                budget = max(
                    0,
                    self.limits.field_bytes - len(marker.encode("utf-8")),
                )
                neutralized = data[:budget].decode("utf-8", errors="ignore") + marker
            sanitized.append(neutralized)
        stream = io.StringIO(newline="")
        csv.writer(stream, lineterminator="\n").writerow(sanitized)
        encoded = stream.getvalue().encode("utf-8")
        if len(encoded) > self.limits.row_bytes:
            raise RepairError("diagnostic row exceeds byte limit")
        if self._published_bytes + len(encoded) > self.limits.session_bytes:
            raise RepairError("diagnostic session exceeds byte limit")
        return encoded

    def publish(
        self,
        filename: str,
        payload: bytes,
        summary: Mapping[str, Any],
    ) -> tuple[Path, Path]:
        if not filename or Path(filename).name != filename:
            raise RepairError("unsafe diagnostic filename")
        target_dir = self.session_dir
        target_dir.mkdir(parents=True, exist_ok=True)
        row_path = target_dir / filename
        summary_path = target_dir / "diagnostic-summary.json"
        new_total = self._published_bytes + len(payload)
        if new_total > self.limits.session_bytes:
            raise RepairError("diagnostic session exceeds byte limit")

        row_before = row_path.read_bytes() if row_path.exists() else None
        summary_before = summary_path.read_bytes() if summary_path.exists() else None
        summary_bytes = (
            json.dumps(summary, sort_keys=True, separators=(",", ":")) + "\n"
        ).encode("utf-8")
        try:
            _atomic_write(row_path, payload)
            _atomic_write(summary_path, summary_bytes)
        except Exception:
            _restore(row_path, row_before)
            _restore(summary_path, summary_before)
            raise
        self._published_bytes = new_total
        return row_path, summary_path


def validate_session_segment(value: str, max_chars: int = 64) -> str:
    if not isinstance(value, str):
        raise RepairError("session segment must be text")
    segment = value.strip()
    if not segment or segment in {".", ".."}:
        raise RepairError("unsafe session segment")
    if Path(segment).is_absolute() or "/" in segment or "\\" in segment:
        raise RepairError("session segment must be one relative component")
    if len(segment) > max_chars:
        raise RepairError("session segment too long")
    if segment.rstrip(" .").lower() in _RESERVED_WINDOWS_NAMES:
        raise RepairError("reserved session segment")
    if re.search(r"[\x00-\x1f<>:\"|?*]", segment):
        raise RepairError("invalid session segment characters")
    return segment


def _require_descendant(root: Path, candidate: Path) -> None:
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise RepairError("path escapes configured root") from exc
    if candidate == root:
        raise RepairError("session path must be a strict descendant")


def _atomic_write(path: Path, data: bytes) -> None:
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_name, path)
    except Exception:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def _restore(path: Path, previous: bytes | None) -> None:
    if previous is None:
        path.unlink(missing_ok=True)
    else:
        _atomic_write(path, previous)
