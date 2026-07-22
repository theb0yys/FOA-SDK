"""Deterministic dependency graphs for synthetic publication intents."""
from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from typing import Any, Iterable

from .errors import PublicationIntentError

_ALLOWED_STATUSES = {
    "intent-prepared",
    "intent-committed",
    "intent-rolled-back",
}


def _canonical(doc: dict[str, Any]) -> bytes:
    return (
        json.dumps(doc, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        + "\n"
    ).encode("utf-8")


@dataclass(frozen=True)
class IntentDependencyNode:
    intent_id: str
    status: str
    dependencies: tuple[str, ...] = ()

    def __post_init__(self) -> None:
        if not isinstance(self.intent_id, str) or not self.intent_id.strip():
            raise PublicationIntentError(
                "intent dependency ID must be non-empty"
            )
        if self.status not in _ALLOWED_STATUSES:
            raise PublicationIntentError(
                "intent dependency status is unsupported"
            )
        if (
            self.dependencies != tuple(sorted(self.dependencies))
            or len(self.dependencies) != len(set(self.dependencies))
        ):
            raise PublicationIntentError(
                "intent dependencies must be unique and sorted"
            )
        if self.intent_id in self.dependencies:
            raise PublicationIntentError(
                "publication intent cannot depend on itself"
            )

    def to_dict(self) -> dict[str, Any]:
        return {
            "intent_id": self.intent_id,
            "status": self.status,
            "dependencies": list(self.dependencies),
        }


@dataclass(frozen=True)
class IntentDependencyGraph:
    nodes: tuple[IntentDependencyNode, ...]
    topological_order: tuple[str, ...]
    runnable_intents: tuple[str, ...]
    blocked_intents: tuple[str, ...]

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "authority": "synthetic-publication-intent-dependency-graph-only",
            "runtime_authority": "none",
            "promotion": "none",
            "nodes": [node.to_dict() for node in self.nodes],
            "topological_order": list(self.topological_order),
            "runnable_intents": list(self.runnable_intents),
            "blocked_intents": list(self.blocked_intents),
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.to_bytes()).hexdigest()


def build_intent_dependency_graph(
    nodes: Iterable[IntentDependencyNode],
) -> IntentDependencyGraph:
    ordered = tuple(sorted(nodes, key=lambda item: item.intent_id))
    if not ordered:
        raise PublicationIntentError(
            "intent dependency graph requires nodes"
        )
    by_id = {node.intent_id: node for node in ordered}
    if len(by_id) != len(ordered):
        raise PublicationIntentError(
            "intent dependency graph contains duplicate IDs"
        )
    for node in ordered:
        missing = [
            dependency
            for dependency in node.dependencies
            if dependency not in by_id
        ]
        if missing:
            raise PublicationIntentError(
                f"intent {node.intent_id} has missing dependencies"
            )

    indegree = {
        node.intent_id: len(node.dependencies)
        for node in ordered
    }
    dependents: dict[str, list[str]] = {
        node.intent_id: [] for node in ordered
    }
    for node in ordered:
        for dependency in node.dependencies:
            dependents[dependency].append(node.intent_id)
    ready = sorted(
        intent_id
        for intent_id, count in indegree.items()
        if count == 0
    )
    topological: list[str] = []
    while ready:
        current = ready.pop(0)
        topological.append(current)
        for dependent in sorted(dependents[current]):
            indegree[dependent] -= 1
            if indegree[dependent] == 0:
                ready.append(dependent)
                ready.sort()
    if len(topological) != len(ordered):
        raise PublicationIntentError(
            "intent dependency graph contains a cycle"
        )

    runnable: list[str] = []
    blocked: list[str] = []
    for intent_id in topological:
        node = by_id[intent_id]
        if node.status != "intent-prepared":
            continue
        dependency_statuses = tuple(
            by_id[dependency].status
            for dependency in node.dependencies
        )
        if all(
            status == "intent-committed"
            for status in dependency_statuses
        ):
            runnable.append(intent_id)
        else:
            blocked.append(intent_id)
    return IntentDependencyGraph(
        ordered,
        tuple(topological),
        tuple(runnable),
        tuple(blocked),
    )
