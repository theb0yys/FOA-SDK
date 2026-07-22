# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import copy
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/AdmissionBoundExecutionHandoff/Source",
    REPO_ROOT / "Installer/Bootstrapper/LifecycleOperationAdmission/Source",
    REPO_ROOT / "Installer/Bootstrapper/ExecutionHandoff/Source",
    REPO_ROOT / "Installer/SuiteWizard/ViewModel/Source",
):
    sys.path.insert(0, str(root))

ADMISSION_TESTS = REPO_ROOT / "Installer/Tests/LifecycleOperationAdmission/test_lifecycle_operation_admission.py"
ADMISSION_SPEC = importlib.util.spec_from_file_location("foa_lifecycle_operation_admission_helpers", ADMISSION_TESTS)
if ADMISSION_SPEC is None or ADMISSION_SPEC.loader is None:
    raise RuntimeError(f"Unable to load admission helper tests: {ADMISSION_TESTS}")
ADMISSION_MODULE = importlib.util.module_from_spec(ADMISSION_SPEC)
sys.modules[ADMISSION_SPEC.name] = ADMISSION_MODULE
ADMISSION_SPEC.loader.exec_module(ADMISSION_MODULE)

from admission_bound_execution_handoff import (  # noqa: E402
    BINDING_CAPABILITY,
    AdmissionBoundExecutionHandoffError,
    build_admission_bound_handoff,
    canonical_binding_bytes,
    validate_admission_bound_handoff,
    verify_bound_handoff,
)
from lifecycle_operation_admission import admit_lifecycle_operation  # noqa: E402
from receipt_execution_handoff import build_handoff  # noqa: E402


class AdmissionBoundExecutionHandoffTests(unittest.TestCase):
    def admission_helper(self):
        return ADMISSION_MODULE.LifecycleOperationAdmissionTests(
            methodName="test_empty_registry_install_admission_is_deterministic_and_side_effect_free"
        )

    def registry_helper(self):
        return self.admission_helper().helper()

    def publisher_helper(self):
        return self.registry_helper().helper()

    def admission_receipt(self, operation: str, *, nonce: str | None = None) -> dict[str, object]:
        admission = self.admission_helper()
        registry = admission.helper()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            if operation != "install":
                registry.publish_state(root, "install")
            snapshot = registry.snapshot(root)
            eligibility = registry.eligibility(snapshot, operation)
        grant = admission.admission_grant(
            eligibility,
            nonce=nonce or f"grant.foa-sdk.bound-handoff-admission-{operation}",
        )
        return admit_lifecycle_operation(grant, admitted_at_utc="2026-07-22T13:03:00Z")

    def execution_handoff(self, operation: str, *, requested_at_utc: str = "2026-07-22T13:04:00Z") -> dict[str, object]:
        receipt = self.publisher_helper().accepted_receipt()
        return build_handoff(
            receipt,
            operation=operation,
            target_reference="installation.foa-sdk.default",
            prior_installation_reference=None if operation == "install" else "installation.foa-sdk.default",
            requested_by="FOA-SDK admission-bound handoff tests",
            requested_at_utc=requested_at_utc,
        )

    def binding(self, handoff: dict[str, object], admission: dict[str, object], *, nonce: str = "binding.foa-sdk.bound-handoff") -> dict[str, object]:
        return build_admission_bound_handoff(
            handoff,
            admission,
            bound_by="FOA-SDK admission-bound handoff reviewer",
            bound_at_utc="2026-07-22T13:05:00Z",
            nonce=nonce,
        )

    def test_install_handoff_binding_is_deterministic_and_side_effect_free(self) -> None:
        admission = self.admission_receipt("install")
        handoff = self.execution_handoff("install")
        first = self.binding(handoff, admission)
        second = self.binding(handoff, admission)
        self.assertEqual(first, second)
        self.assertEqual(first["capability"], BINDING_CAPABILITY)
        self.assertEqual(first["operation"], "install")
        self.assertEqual(first["handoff_sha256"], handoff["handoff_sha256"])
        self.assertEqual(first["admission_receipt_sha256"], admission["receipt_sha256"])
        self.assertEqual(validate_admission_bound_handoff(first), first)
        self.assertEqual(verify_bound_handoff(first, handoff, admission), first)
        self.assertEqual(canonical_binding_bytes(first), canonical_binding_bytes(second))
        for field in (
            "package_engine_session_created", "copy_performed", "process_launched", "elevation_requested",
            "state_published", "product_or_game_directory_mutated", "runtime_executed", "save_mutated",
            "signing_performed", "network_publication_performed", "catalog_mutated", "evidence_promoted",
        ):
            self.assertFalse(first[field])

    def test_active_state_maintenance_handoffs_bind_to_recommended_prior_state(self) -> None:
        for operation in ("repair", "upgrade", "uninstall"):
            admission = self.admission_receipt(operation, nonce=f"grant.foa-sdk.bound-handoff-admission-{operation}")
            handoff = self.execution_handoff(operation)
            binding = self.binding(handoff, admission, nonce=f"binding.foa-sdk.bound-handoff-{operation}")
            self.assertEqual(binding["operation"], operation)
            self.assertEqual(binding["prior_installation_reference"], "installation.foa-sdk.default")
            self.assertEqual(binding["current_state_reference"], "installation.foa-sdk.default")
            self.assertEqual(validate_admission_bound_handoff(binding), binding)

    def test_operation_target_and_prior_mismatches_fail_closed(self) -> None:
        install_admission = self.admission_receipt("install")
        install_handoff = self.execution_handoff("install")
        repair_handoff = self.execution_handoff("repair")
        with self.assertRaisesRegex(AdmissionBoundExecutionHandoffError, "operation"):
            self.binding(repair_handoff, install_admission)
        tampered_target = copy.deepcopy(install_handoff)
        tampered_target["target_reference"] = "installation.foa-sdk.other"
        with self.assertRaises(AdmissionBoundExecutionHandoffError):
            self.binding(tampered_target, install_admission)
        repair_admission = self.admission_receipt("repair", nonce="grant.foa-sdk.bound-handoff-admission-repair-prior")
        tampered_prior = copy.deepcopy(repair_handoff)
        tampered_prior["prior_installation_reference"] = "installation.foa-sdk.other"
        with self.assertRaises(AdmissionBoundExecutionHandoffError):
            self.binding(tampered_prior, repair_admission)

    def test_tampered_admission_and_binding_fail_closed(self) -> None:
        admission = self.admission_receipt("install")
        handoff = self.execution_handoff("install")
        tampered_admission = copy.deepcopy(admission)
        tampered_admission["operation"] = "repair"
        with self.assertRaises(AdmissionBoundExecutionHandoffError):
            self.binding(handoff, tampered_admission)
        binding = self.binding(handoff, admission)
        tampered_binding = copy.deepcopy(binding)
        tampered_binding["operation"] = "repair"
        with self.assertRaises(AdmissionBoundExecutionHandoffError):
            validate_admission_bound_handoff(tampered_binding)

    def test_temporal_bounds_fail_closed(self) -> None:
        admission = self.admission_receipt("install")
        too_early_handoff = self.execution_handoff("install", requested_at_utc="2026-07-22T13:02:00Z")
        with self.assertRaisesRegex(AdmissionBoundExecutionHandoffError, "must not precede"):
            self.binding(too_early_handoff, admission)
        handoff = self.execution_handoff("install")
        with self.assertRaisesRegex(AdmissionBoundExecutionHandoffError, "within 15 minutes"):
            build_admission_bound_handoff(
                handoff,
                admission,
                bound_by="FOA-SDK admission-bound handoff reviewer",
                bound_at_utc="2026-07-22T13:19:00Z",
                nonce="binding.foa-sdk.bound-handoff-late",
            )

    def test_source_does_not_reintroduce_execution_or_filesystem_surfaces(self) -> None:
        source = (
            REPO_ROOT / "Installer/Bootstrapper/AdmissionBoundExecutionHandoff/Source/admission_bound_execution_handoff.py"
        ).read_text(encoding="utf-8")
        for forbidden in (
            "subprocess", "ShellExecute", "ctypes", "launch_process(", "request_elevation(",
            "stage_payload(", "publish_state_record(", "query_state_registry(", "build_engine_session(",
            "build_capability_token(", "os.", "read_bytes", "write_text", "write_bytes", "unlink(",
            "mkdir(", "socket", "requests", "password", "credential",
        ):
            self.assertNotIn(forbidden, source)
        for required in (
            "package-engine.bind-admitted-handoff", "validate_handoff", "validate_admission_receipt",
            "Execution handoff request must not precede", "package_engine_session_created",
        ):
            self.assertIn(required, source)


if __name__ == "__main__":
    unittest.main()
