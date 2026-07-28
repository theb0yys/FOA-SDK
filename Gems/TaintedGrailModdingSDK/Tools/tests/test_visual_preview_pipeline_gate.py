#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = TOOLS_ROOT / "validate_visual_preview_pipeline_gate.py"
SPEC = importlib.util.spec_from_file_location("validate_visual_preview_pipeline_gate", MODULE_PATH)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


class VisualPreviewPipelineGateTests(unittest.TestCase):
    def test_current_repository_gate_passes(self) -> None:
        validator.validate()

    def test_gate_rejects_missing_required_stage(self) -> None:
        with tempfile.TemporaryDirectory(prefix="visual-preview-gate-test-") as temporary:
            root = Path(temporary)
            doc_path = root / validator.DOC_PATH
            index_path = root / validator.INDEX_PATH
            doc_path.parent.mkdir(parents=True)
            index_path.parent.mkdir(parents=True, exist_ok=True)
            source = (validator.REPO_ROOT / validator.DOC_PATH).read_text(encoding="utf-8")
            doc_path.write_text(
                source.replace("Native icon and thumbnail extraction", "Native thumbnail step removed"),
                encoding="utf-8",
            )
            index_path.write_text(
                "- [Visual Game-Content Browser and Preview Pipeline Gate](VISUAL_GAME_CONTENT_BROWSER_AND_PREVIEW_PIPELINE.md) — blocks function-complete visual and item/recipe/actor/troop workflow claims.\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(validator.VisualPreviewPipelineGateError, "Native icon and thumbnail extraction"):
                validator.validate(root)

    def test_gate_rejects_approval_of_runtime_capture(self) -> None:
        with tempfile.TemporaryDirectory(prefix="visual-preview-gate-test-") as temporary:
            root = Path(temporary)
            doc_path = root / validator.DOC_PATH
            index_path = root / validator.INDEX_PATH
            doc_path.parent.mkdir(parents=True)
            index_path.parent.mkdir(parents=True, exist_ok=True)
            source = (validator.REPO_ROOT / validator.DOC_PATH).read_text(encoding="utf-8")
            doc_path.write_text(source + "\nruntime-assisted capture is approved for Alpha\n", encoding="utf-8")
            index_path.write_text(
                "- [Visual Game-Content Browser and Preview Pipeline Gate](VISUAL_GAME_CONTENT_BROWSER_AND_PREVIEW_PIPELINE.md) — blocks function-complete visual and item/recipe/actor/troop workflow claims.\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(validator.VisualPreviewPipelineGateError, "runtime-assisted capture"):
                validator.validate(root)


if __name__ == "__main__":
    unittest.main()
