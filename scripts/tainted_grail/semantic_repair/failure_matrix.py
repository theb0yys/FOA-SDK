"""Deterministic Cartesian failure matrices without third-party generators."""
from __future__ import annotations

from dataclasses import dataclass
from itertools import product
from typing import Any, Callable, Iterable, Mapping


@dataclass(frozen=True)
class MatrixAxis:
    name: str
    values: tuple[Any, ...]

    def __post_init__(self) -> None:
        if not self.name or not self.values:
            raise ValueError("matrix axes require a name and at least one value")


@dataclass(frozen=True)
class MatrixResult:
    case: Mapping[str, Any]
    status: str
    error_type: str | None = None
    error: str | None = None


def matrix_cases(*axes: MatrixAxis) -> tuple[dict[str, Any], ...]:
    names = [axis.name for axis in axes]
    if len(names) != len(set(names)):
        raise ValueError("matrix axis names must be unique")
    return tuple(dict(zip(names, values, strict=True)) for values in product(*(axis.values for axis in axes)))


def run_matrix(
    cases: Iterable[Mapping[str, Any]],
    runner: Callable[[Mapping[str, Any]], None],
) -> tuple[MatrixResult, ...]:
    results: list[MatrixResult] = []
    for case in cases:
        frozen_case = dict(case)
        try:
            runner(frozen_case)
        except Exception as exc:
            results.append(MatrixResult(frozen_case, "failed", type(exc).__name__, str(exc)))
        else:
            results.append(MatrixResult(frozen_case, "passed"))
    return tuple(results)
