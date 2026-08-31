#!/usr/bin/env python3
"""Embedded-Editor adapter for the shared FoA Asset Browser pane-model generator.

This module intentionally owns no pane-model schema, normalization, validation,
or output policy. It only adapts O3DE's in-process Python runner arguments to
the existing foa_asset_browser_pane_model implementation without using the
CLI's SystemExit-based process contract.
"""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Sequence

import foa_asset_browser_pane_model as pane


class EmbeddedPaneRefreshError(RuntimeError):
    pass


def _parse_args(argv: Sequence[str]) -> tuple[Path, Path, bool]:
    args = list(argv)
    if len(args) not in {4, 5}:
        raise EmbeddedPaneRefreshError(
            "Expected --workspace <path> --import-proof <path> [--replace]."
        )
    if args[0] != "--workspace" or args[2] != "--import-proof":
        raise EmbeddedPaneRefreshError(
            "Embedded pane refresh received an unsupported argument contract."
        )
    replace = len(args) == 5
    if replace and args[4] != "--replace":
        raise EmbeddedPaneRefreshError(
            "Embedded pane refresh received an unsupported trailing argument."
        )
    return Path(args[1]), Path(args[3]), replace


def refresh(argv: Sequence[str] | None = None) -> Path:
    workspace, import_proof, replace = _parse_args(
        sys.argv[1:] if argv is None else argv
    )
    try:
        _, model_path = pane.build_model(
            workspace,
            import_proof,
            replace=replace,
        )
        pane.verify_model(
            model_path,
            workspace_path=workspace,
            import_proof_path=import_proof,
        )
    except pane.AssetBrowserPaneError as exc:
        raise EmbeddedPaneRefreshError(str(exc)) from exc

    print(f"FOA-SDK Item Viewer refreshed pane model: {model_path}")
    return model_path


if __name__ == "__main__":
    refresh()
