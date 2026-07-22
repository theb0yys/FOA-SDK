# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import hashlib
import os
import shutil
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/AdmissionBoundExecutionHandoff/Source",
    REPO_ROOT / "Installer/Bootstrapper/LifecycleOperationAdmission/Source",
    REPO_ROOT / "Installer/Bootstrapper/InstallationStateRegistry/Source",
    REPO_ROOT / "Installer/Bootstrapper/Security/Source",
    REPO_ROOT / "Installer/Bootstrapper/PackageEngine/Source",
    REPO_ROOT / "Installer/Bootstrapper/ExecutionHandoff/Source",
    REPO_ROOT / "Installer/SuiteWizard/Receipt/Source",
    REPO_ROOT / "Installer/SuiteWizard/Host/Source",
    REPO_ROOT / "Installer/SuiteWizard/ViewModel/Source",
):
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))

from admission_bound_execution_handoff import build_admission_bound_handoff  # noqa: E402
from confirmation_receipt import build_receipt  # noqa: E402
from execution_security import build_operation_plan, issue_authority_proof  # noqa: E402
from installation_state_registry import build_lifecycle_eligibility, build_registry_snapshot  # noqa: E402
from lifecycle_operation_admission import admit_lifecycle_operation, build_admission_grant  # noqa: E402
from package_engine import build_capability_token, build_engine_session  # noqa: E402
from receipt_execution_handoff import build_handoff  # noqa: E402
from wizard_confirmation_controller import WizardConfirmationController  # noqa: E402

AUTHORITY_KEY = bytes.fromhex("42" * 32)


class InstallerSecurityFixture:
    def __init__(self, *, elevated: bool = False, output_limit_bytes: int = 4096, argv: list[str] | None = None) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name).resolve()
        self.claim_root = self.root / "claims"; self.claim_root.mkdir()
        self.execution_root = self.root / "execution"; self.execution_root.mkdir()
        (self.execution_root / "bin").mkdir(); (self.execution_root / "work").mkdir()
        self.request_root = self.root / "requests"; self.request_root.mkdir()
        self.bootstrap_claim_root = self.root / "bootstrap-claims"; self.bootstrap_claim_root.mkdir()
        self.bootstrap_output = self.root / "bootstrap-completion.json"
        self.authority_key = self.root / "authority.key"
        self.authority_key.write_bytes(AUTHORITY_KEY)
        os.chmod(self.authority_key, 0o600)
        shell = Path("/bin/sh")
        if not shell.is_file():
            shell = Path(sys.executable)
        self.helper_path = self.execution_root / "bin" / "operation-helper"
        shutil.copyfile(shell, self.helper_path); os.chmod(self.helper_path, 0o700)
        self.bootstrapper_path = self.execution_root / "bin" / "elevation-bootstrapper"
        shutil.copyfile(shell, self.bootstrapper_path); os.chmod(self.bootstrapper_path, 0o700)
        helper_hash = hashlib.sha256(self.helper_path.read_bytes()).hexdigest()
        bootstrapper_hash = hashlib.sha256(self.bootstrapper_path.read_bytes()).hexdigest()
        command = argv or ["-c", "printf \"$FOA_TEST\"; printf err >&2"]
        helpers: list[dict[str, object]] = [{
            "helper_reference": "helper.foa-sdk.install",
            "role": "operation-helper",
            "executable_path": "bin/operation-helper",
            "executable_sha256": helper_hash,
            "argv": command,
            "working_directory": "work",
            "environment": {"FOA_TEST": "exact-environment"},
            "operations": ["install"],
            "requires_elevation": elevated,
            "timeout_seconds": 5,
            "output_limit_bytes": output_limit_bytes,
        }]
        bootstrapper_reference = None
        if elevated:
            bootstrapper_reference = "helper.foa-sdk.elevation-bootstrapper"
            helpers.append({
                "helper_reference": bootstrapper_reference,
                "role": "elevation-bootstrapper",
                "executable_path": "bin/elevation-bootstrapper",
                "executable_sha256": bootstrapper_hash,
                "argv": [
                    "--execution-root", str(self.execution_root),
                    "--authority-key", str(self.authority_key),
                    "--claim-root", str(self.bootstrap_claim_root),
                    "--completed-at-utc", "2026-07-22T12:08:00Z",
                    "--output", str(self.bootstrap_output),
                ],
                "working_directory": "work",
                "environment": {},
                "operations": ["install"],
                "requires_elevation": False,
                "timeout_seconds": 5,
                "output_limit_bytes": 4096,
            })
        controller = WizardConfirmationController(REPO_ROOT / "Installer")
        review = controller.resolve_review()
        for row in controller.acknowledgement_choices():
            controller.set_acknowledgement(str(row["acknowledgement_id"]), True)
        controller.create_review_confirmation(
            expected_plan_sha256=str(review["plan_sha256"]),
            expected_view_model_sha256=str(review["view_model_sha256"]),
            confirmed_by="FOA-SDK security tests",
            confirmed_at_utc="2026-07-22T11:00:00Z",
        )
        result = controller.review_result; confirmation = controller.confirmation_result
        assert isinstance(result, dict) and isinstance(confirmation, dict)
        receipt = build_receipt(dict(result["plan"]), dict(result["view_model"]), dict(confirmation))
        self.handoff = build_handoff(
            receipt,
            operation="install",
            target_reference="installation.foa-sdk.default",
            prior_installation_reference=None,
            requested_by="FOA-SDK security tests",
            requested_at_utc="2026-07-22T12:00:00Z",
        )
        self.registry_snapshot = build_registry_snapshot(
            [],
            state_root_reference="state-root.foa-sdk.security",
            observed_at_utc="2026-07-22T11:55:00Z",
        )
        self.lifecycle_eligibility = build_lifecycle_eligibility(
            self.registry_snapshot,
            operation="install",
            target_reference="installation.foa-sdk.default",
            decided_at_utc="2026-07-22T11:56:00Z",
        )
        self.admission_grant = build_admission_grant(
            self.lifecycle_eligibility,
            issuer="FOA-SDK security admission reviewer",
            issued_at_utc="2026-07-22T11:57:00Z",
            expires_at_utc="2026-07-22T12:12:00Z",
            nonce="grant.foa-sdk.security-admission",
        )
        self.admission_receipt = admit_lifecycle_operation(
            self.admission_grant,
            admitted_at_utc="2026-07-22T11:58:00Z",
        )
        self.admission_bound_handoff = build_admission_bound_handoff(
            self.handoff,
            self.admission_receipt,
            bound_by="FOA-SDK security admission binder",
            bound_at_utc="2026-07-22T12:00:00Z",
            nonce="binding.foa-sdk.security-install",
        )
        owner_package_id = str(self.handoff["package_order"][0])
        for helper in helpers:
            helper["owner_package_id"] = owner_package_id
        self.operation_plan = build_operation_plan(
            plan_reference="operation-plan.foa-sdk.install",
            operation="install",
            receipt_plan_sha256=str(self.handoff["plan_sha256"]),
            package_order=list(self.handoff["package_order"]),
            helpers=helpers,
            bootstrapper_reference=bootstrapper_reference,
        )
        self.authority_proof = issue_authority_proof(
            self.handoff,
            self.operation_plan,
            authority_key_path=self.authority_key,
            issuer="FOA-SDK trusted review broker",
            issued_at_utc="2026-07-22T12:01:00Z",
            expires_at_utc="2026-07-22T12:15:00Z",
            nonce="authority.foa-sdk.install-0001",
        )
        self.token = build_capability_token(
            self.admission_bound_handoff,
            self.operation_plan,
            self.authority_proof,
            authority_key_path=self.authority_key,
            subject="FOA-SDK package engine",
            issued_at_utc="2026-07-22T12:02:00Z",
            expires_at_utc="2026-07-22T12:15:00Z",
            nonce="token.foa-sdk.install-0001",
        )
        self.session = build_engine_session(
            self.admission_bound_handoff,
            self.token,
            authority_key_path=self.authority_key,
            claim_root=self.claim_root,
            session_reference="session.foa-sdk.install",
            accepted_by="FOA-SDK package engine",
            accepted_at_utc="2026-07-22T12:03:00Z",
        )

    def close(self) -> None:
        self.temporary.cleanup()

    def __enter__(self) -> "InstallerSecurityFixture":
        return self

    def __exit__(self, *args: object) -> None:
        self.close()
