#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
REPORT = REPO_ROOT / "docs/tainted-grail-sdk/WINDOWS_INSTALLER_COMPLIANCE_REPORT.md"
DESIGN = "WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md"
AUDITED_MAIN = "9203e75728292a5e0c18bebe7d48056fa0a8f95a"


class InstallerComplianceReportTests(unittest.TestCase):
    def test_report_uses_the_approved_design_as_sole_authority(self) -> None:
        report = REPORT.read_text(encoding="utf-8")
        self.assertIn("sole requirements authority", report)
        self.assertIn(DESIGN, report)
        self.assertIn(AUDITED_MAIN, report)
        self.assertIn("pull request **#179**", report)

    def test_report_maps_requirements_implementation_tests_and_gaps(self) -> None:
        report = REPORT.read_text(encoding="utf-8")
        self.assertIn(
            "| Research requirement | Implementing file and code | Enforcing validator or test | Remaining gap |",
            report,
        )
        for fragment in (
            "O3DE `INSTALL` owns the payload",
            "Human review supplies exact fingerprint",
            "Deterministic portable ZIP",
            "MSI consumes the same verified staging root",
            "`FOA-SDK-Installer.exe` is a self-contained Windows Forms x64 executable",
            "Repair restores exact product-owned launcher bytes",
            "No alternate or legacy installer route",
            "Explicit exclusions remain absent",
        ):
            self.assertIn(fragment, report)

    def test_report_cannot_claim_an_exe_before_reviewed_package_evidence(self) -> None:
        report = REPORT.read_text(encoding="utf-8")
        self.assertIn("A compliant `FOA-SDK-Installer.exe` does not yet exist", report)
        self.assertIn("pending human review", report)
        self.assertIn("Package workflow run: **pending**", report)
        self.assertIn("Install / launcher / repair / uninstall result: **pending**", report)
        self.assertIn("No automated actor may invent or impersonate", report)

    def test_post_build_evidence_does_not_mutate_the_reviewed_source(self) -> None:
        report = REPORT.read_text(encoding="utf-8")
        self.assertIn("Immutable completion evidence template", report)
        self.assertIn("would create a new source commit and invalidate the reviewed inventory", report)
        self.assertIn("durable PR #179 conversation", report)
        self.assertNotIn("This section must be updated only after", report)


if __name__ == "__main__":
    unittest.main()
