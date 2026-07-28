#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import foa_o3de_asset_processor_import_proof as proof


class AssetProcessorImportProofTests(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="foa-ap-proof-"))

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def fixture_paths(self):
        proof.generate_fixture(self.tmp, replace=True)
        workspace = self.tmp / "workspace.tgworkspace.json"
        extracted = self.tmp / "workspace" / "Extracted"
        conversion = extracted / "PreviewArtifacts" / "O3DE" / "o3de.preview.foa.mono.fixture.synthetic" / "foa-o3de-preview-conversion.json"
        obs = extracted / "PreviewArtifacts" / "O3DE" / "o3de.preview.foa.mono.fixture.synthetic" / "AssetProcessorObservation" / "foa-o3de-asset-processor-observation.json"
        proof_root = extracted / "PreviewArtifacts" / "O3DE" / "o3de.preview.foa.mono.fixture.synthetic" / "ImportProofs"
        manifest = next(proof_root.glob("*/foa-o3de-asset-processor-import-proof.json"))
        return workspace, conversion, obs, manifest

    def test_fixture_generates_verifiable_import_proof(self):
        workspace, conversion, _obs, manifest = self.fixture_paths()
        doc = proof.verify_proof(manifest, workspace_path=workspace, conversion_path=conversion)
        self.assertEqual(doc["DocumentKind"], proof.DOCUMENT_KIND)
        self.assertTrue(doc["PreviewStageStatus"]["O3deAssetProcessorInvocationObserved"])
        self.assertTrue(doc["PreviewStageStatus"]["GeneratedO3dePreviewProductEvidence"])
        self.assertFalse(doc["PreviewStageStatus"]["FunctionCompleteAllowed"])
        self.assertEqual(doc["AssetProcessorImportRun"]["ImportState"], "observed-success")

    def test_log_tamper_is_rejected(self):
        workspace, conversion, _obs, manifest = self.fixture_paths()
        log_path = manifest.parent / "logs" / "assetprocessor.log"
        log_path.write_text("tampered\n", encoding="utf-8")
        with self.assertRaises(proof.AssetProcessorImportProofError):
            proof.verify_proof(manifest, workspace_path=workspace, conversion_path=conversion)

    def test_runtime_authority_escalation_is_rejected(self):
        workspace, conversion, obs, _manifest = self.fixture_paths()
        data = json.loads(obs.read_text())
        data["OperationalAuthority"]["RuntimePermissionGranted"] = True
        obs.write_text(json.dumps(data, indent=2) + "\n")
        with self.assertRaises(proof.AssetProcessorImportProofError):
            proof.build_proof(workspace, conversion, obs, captured_at="2026-07-28T00:03:00Z", replace=True)

    def test_unknown_source_product_is_rejected(self):
        workspace, conversion, obs, _manifest = self.fixture_paths()
        data = json.loads(obs.read_text())
        data["ImportedProducts"][0]["O3dePreviewSourceId"] = "o3de.source.missing"
        obs.write_text(json.dumps(data, indent=2) + "\n")
        with self.assertRaises(proof.AssetProcessorImportProofError):
            proof.build_proof(workspace, conversion, obs, captured_at="2026-07-28T00:03:00Z", replace=True)

    def test_product_cache_path_must_be_tokenized(self):
        workspace, conversion, obs, _manifest = self.fixture_paths()
        data = json.loads(obs.read_text())
        data["ImportedProducts"][0]["ProductCachePath"] = "/private/cache/iron.dds"
        obs.write_text(json.dumps(data, indent=2) + "\n")
        with self.assertRaises(proof.AssetProcessorImportProofError):
            proof.build_proof(workspace, conversion, obs, captured_at="2026-07-28T00:03:00Z", replace=True)

    def test_observation_must_match_conversion(self):
        workspace, conversion, obs, _manifest = self.fixture_paths()
        data = json.loads(obs.read_text())
        data["SourceConversionId"] = "o3de.preview.other.synthetic"
        obs.write_text(json.dumps(data, indent=2) + "\n")
        with self.assertRaises(proof.AssetProcessorImportProofError):
            proof.build_proof(workspace, conversion, obs, captured_at="2026-07-28T00:03:00Z", replace=True)

    def test_failure_observation_records_failure_without_function_complete(self):
        workspace, conversion, obs, _manifest = self.fixture_paths()
        data = json.loads(obs.read_text())
        data["AssetProcessorRun"]["ExitCode"] = 1
        data["ImportedProducts"] = []
        data["ImportFailures"] = [{"FailureId": "o3de.failure.fixture", "O3dePreviewSourceId": "o3de.source.fixture.iron", "Code": "fixture-failure", "Message": "Synthetic failure"}]
        obs.write_text(json.dumps(data, indent=2) + "\n")
        manifest, path = proof.build_proof(workspace, conversion, obs, captured_at="2026-07-28T00:03:00Z", replace=True)
        self.assertEqual(manifest["AssetProcessorImportRun"]["ImportState"], "observed-failure")
        self.assertFalse(manifest["PreviewStageStatus"]["FunctionCompleteAllowed"])
        proof.verify_proof(path, workspace_path=workspace, conversion_path=conversion)

    def test_cli_fixture_and_verify(self):
        out = self.tmp / "cli"
        script = TOOLS / "foa_o3de_asset_processor_import_proof.py"
        fixture = subprocess.run([sys.executable, str(script), "fixture", "--output", str(out)], text=True, capture_output=True)
        self.assertEqual(fixture.returncode, 0, fixture.stdout + fixture.stderr)
        manifest = next((out / "workspace" / "Extracted" / "PreviewArtifacts" / "O3DE" / "o3de.preview.foa.mono.fixture.synthetic" / "ImportProofs").glob("*/foa-o3de-asset-processor-import-proof.json"))
        conversion = out / "workspace" / "Extracted" / "PreviewArtifacts" / "O3DE" / "o3de.preview.foa.mono.fixture.synthetic" / "foa-o3de-preview-conversion.json"
        verify = subprocess.run([sys.executable, str(script), "verify", "--input", str(manifest), "--workspace", str(out / "workspace.tgworkspace.json"), "--conversion", str(conversion)], text=True, capture_output=True)
        self.assertEqual(verify.returncode, 0, verify.stdout + verify.stderr)


if __name__ == "__main__":
    unittest.main()
