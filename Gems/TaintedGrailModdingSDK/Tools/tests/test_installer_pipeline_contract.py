#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import hashlib
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import verify_installer_artifacts as artifacts


REPO_ROOT = Path(__file__).resolve().parents[4]
WORKFLOW = REPO_ROOT / ".github/workflows/tainted-grail-sdk-installer.yml"
VERSION = "0.1.0"


class InstallerPipelineContractTests(unittest.TestCase):
    def write_artifact(self, root: Path, name: str, contents: bytes) -> Path:
        artifact = root / name
        artifact.write_bytes(contents)
        digest = hashlib.sha256(contents).hexdigest()
        Path(f"{artifact}.sha256").write_text(
            f"{digest}  {name}\n",
            encoding="utf-8",
        )
        return artifact

    def make_artifact_set(self, root: Path) -> tuple[Path, Path, Path]:
        base = f"Tainted-Grail-FoA-SDK-{VERSION}-windows-x64"
        return (
            self.write_artifact(root, "FOA-SDK-Installer.exe", b"wizard"),
            self.write_artifact(root, f"{base}.msi", b"msi"),
            self.write_artifact(root, f"{base}.zip", b"zip"),
        )

    def test_complete_artifact_set_and_sidecars_verify(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            wizard, msi, portable_zip = self.make_artifact_set(root)
            with mock.patch.object(artifacts.installer, "verify_archive") as verify_archive:
                verified = artifacts.verify_artifact_set(root, VERSION)
            verify_archive.assert_called_once_with(portable_zip)
            self.assertEqual(
                set(verified),
                {wizard.name, msi.name, portable_zip.name},
            )

    def test_checksum_mismatch_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            wizard, _, _ = self.make_artifact_set(root)
            wizard.write_bytes(b"tampered")
            with self.assertRaisesRegex(artifacts.ArtifactVerificationError, "checksum mismatch"):
                artifacts.verify_checksum_pair(wizard)

    def test_checksum_record_must_be_canonical_and_name_exact_file(self) -> None:
        digest = "a" * 64
        self.assertEqual(
            artifacts.parse_checksum_record(f"{digest}  payload.msi\n", "payload.msi"),
            digest,
        )
        with self.assertRaisesRegex(artifacts.ArtifactVerificationError, "canonical"):
            artifacts.parse_checksum_record(f"{digest}  payload.msi\r\n", "payload.msi")
        with self.assertRaisesRegex(artifacts.ArtifactVerificationError, "lowercase sha256"):
            artifacts.parse_checksum_record(f"{digest}  other.msi\n", "payload.msi")

    def test_workflow_enforces_the_approved_pipeline_order(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        ordered_steps = (
            "Run authoritative repository and compiled-test validation",
            "Configure prebuilt O3DE SDK layout",
            "Build canonical O3DE INSTALL target",
            "Generate notices and third-party package inventory",
            "Generate exact installer inventory",
            "Upload inventory for redistribution review",
            "Bind package mode to exact redistribution review",
            "Stage reviewed payload and create portable ZIP",
            "Build standard MSI from the same staged payload",
            "Build self-contained installer wizard with the reviewed MSI",
            "Verify retained installer artifacts and checksums",
            "Smoke install, launch contract, repair, and uninstall",
            "Upload unsigned development installer artifacts",
        )
        positions = [workflow.index(f"- name: {name}") for name in ordered_steps]
        self.assertEqual(positions, sorted(positions))

    def test_zip_and_msi_are_built_from_the_same_verified_stage(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("--stage-root $env:SDK_STAGE", workflow)
        self.assertIn('-DTG_INSTALLER_PAYLOAD_ROOT="$env:SDK_STAGE"', workflow)
        self.assertIn(
            "python Gems/TaintedGrailModdingSDK/Tools/verify_installer_artifacts.py",
            workflow,
        )
        self.assertIn('--artifact-root "${{ env.SDK_ARTIFACTS }}"', workflow)


if __name__ == "__main__":
    unittest.main()
