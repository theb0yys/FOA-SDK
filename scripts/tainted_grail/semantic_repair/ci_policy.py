"""Negative policy checks for the read-only Semantic Hook offline workflow."""
from __future__ import annotations

import re
from pathlib import Path

from .errors import WorkflowPolicyError

_FORBIDDEN_TOKENS = (
    "actions/upload-artifact",
    "actions/upload-pages-artifact",
    "actions/attest",
    "softprops/action-gh-release",
    "docker/build-push-action",
    "gh release",
    "packages: write",
    "id-token: write",
    "actions: write",
    "contents: write",
    "pull-requests: write",
)


def _top_level_permissions(text: str) -> dict[str, str]:
    lines = text.splitlines()
    start = None
    for index, line in enumerate(lines):
        if line == "permissions:":
            start = index + 1
            break
    if start is None:
        raise WorkflowPolicyError("workflow must declare top-level permissions")
    permissions: dict[str, str] = {}
    for line in lines[start:]:
        if line and not line.startswith(" "):
            break
        match = re.fullmatch(r"  ([A-Za-z0-9_-]+):\s*(\S+)\s*", line)
        if match:
            permissions[match.group(1)] = match.group(2)
    return permissions


def validate_offline_workflow(text: str) -> None:
    lowered = text.lower()
    permissions = _top_level_permissions(text)
    if permissions != {"contents": "read"}:
        raise WorkflowPolicyError("workflow permissions must be exactly contents: read")
    if "persist-credentials: false" not in lowered:
        raise WorkflowPolicyError("checkout credentials must not persist")
    if "${{ secrets." in lowered:
        raise WorkflowPolicyError("offline workflow must not reference secrets")
    for token in _FORBIDDEN_TOKENS:
        if token in lowered:
            raise WorkflowPolicyError(f"offline workflow contains forbidden publication token: {token}")
    if re.search(r"\b(curl|wget|pip\s+install|apt-get|npm\s+publish)\b", lowered):
        raise WorkflowPolicyError("offline workflow contains network or publication command")


def validate_offline_workflow_file(path: Path) -> None:
    validate_offline_workflow(Path(path).read_text(encoding="utf-8"))
