#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Transient acknowledgement and review-confirmation state for the Suite Wizard host."""

from __future__ import annotations

from typing import Mapping

from wizard_host_controller import WizardHostController, WizardHostError
from wizard_view_model import (
    WizardContractError,
    create_confirmation,
    evaluate_acknowledgements,
    verify_confirmation,
)


class WizardConfirmationController(WizardHostController):
    """Binds graphical acknowledgements to the exact current review-only result."""

    def __init__(self, installer_root: object) -> None:
        self._acknowledged_ids: set[str] = set()
        self._confirmation: dict[str, object] | None = None
        super().__init__(installer_root)

    @property
    def confirmation_result(self) -> dict[str, object] | None:
        return self._confirmation

    def invalidate_review(self) -> dict[str, object]:
        self._result = None
        self._clear_confirmation_state(clear_acknowledgements=True)
        return self.host_snapshot()

    def invalidate_confirmation(self) -> dict[str, object]:
        self._confirmation = None
        return self.confirmation_state_snapshot()

    def set_package_included(self, package_id: str, included: bool) -> dict[str, object]:
        self._clear_confirmation_state(clear_acknowledgements=True)
        return super().set_package_included(package_id, included)

    def set_feature_selected(self, feature_id: str, selected: bool) -> dict[str, object]:
        self._clear_confirmation_state(clear_acknowledgements=True)
        return super().set_feature_selected(feature_id, selected)

    def set_context(
        self,
        *,
        platform: str,
        architecture: str,
        runtime_target: str,
        game_version: str = "",
        branch: str = "",
    ) -> dict[str, object]:
        self._clear_confirmation_state(clear_acknowledgements=True)
        return super().set_context(
            platform=platform,
            architecture=architecture,
            runtime_target=runtime_target,
            game_version=game_version,
            branch=branch,
        )

    def resolve_review(self) -> dict[str, object]:
        self._clear_confirmation_state(clear_acknowledgements=True)
        return super().resolve_review()

    def acknowledgement_choices(self) -> list[dict[str, object]]:
        view_model = self._current_view_model()
        rows = view_model.get("required_acknowledgements")
        if not isinstance(rows, list):
            raise WizardHostError("The current view-model acknowledgement rows are malformed.")
        result: list[dict[str, object]] = []
        for index, raw in enumerate(rows):
            row = self._object(raw, f"required_acknowledgements[{index}]")
            acknowledgement_id = self._non_empty_text(
                row.get("acknowledgement_id"),
                f"required_acknowledgements[{index}].acknowledgement_id",
            )
            result.append({
                "acknowledgement_id": acknowledgement_id,
                "title": self._non_empty_text(row.get("title"), f"{acknowledgement_id}.title"),
                "detail": self._non_empty_text(row.get("detail"), f"{acknowledgement_id}.detail"),
                "severity": self._non_empty_text(row.get("severity"), f"{acknowledgement_id}.severity"),
                "required": row.get("required") is True,
                "acknowledged": acknowledgement_id in self._acknowledged_ids,
            })
        return result

    def set_acknowledgement(self, acknowledgement_id: str, acknowledged: bool) -> dict[str, object]:
        requested = self._non_empty_text(acknowledgement_id, "acknowledgement_id")
        known = {row["acknowledgement_id"] for row in self.acknowledgement_choices()}
        if requested not in known:
            raise WizardHostError(f"Unknown acknowledgement ID {requested!r}.")
        if type(acknowledged) is not bool:
            raise WizardHostError("acknowledged must be a boolean.")
        if acknowledged:
            self._acknowledged_ids.add(requested)
        else:
            self._acknowledged_ids.discard(requested)
        self._confirmation = None
        return self.confirmation_state_snapshot()

    def acknowledgement_evaluation(self) -> dict[str, object]:
        return evaluate_acknowledgements(
            self._current_view_model(),
            sorted(self._acknowledged_ids),
        )

    def create_review_confirmation(
        self,
        *,
        expected_plan_sha256: str,
        expected_view_model_sha256: str,
        confirmed_by: str,
        confirmed_at_utc: str,
    ) -> dict[str, object]:
        result = self._current_result()
        plan = self._object(result.get("plan"), "result.plan")
        view_model = self._object(result.get("view_model"), "result.view_model")
        current_plan_sha256 = self._non_empty_text(plan.get("plan_sha256"), "plan.plan_sha256")
        current_view_model_sha256 = self._non_empty_text(
            view_model.get("view_model_sha256"),
            "view_model.view_model_sha256",
        )
        if expected_plan_sha256 != current_plan_sha256:
            raise WizardHostError("The displayed plan fingerprint is stale; resolve the review again.")
        if expected_view_model_sha256 != current_view_model_sha256:
            raise WizardHostError("The displayed view-model fingerprint is stale; resolve the review again.")

        confirmation = create_confirmation(
            plan,
            view_model,
            expected_plan_sha256=current_plan_sha256,
            acknowledged_ids=sorted(self._acknowledged_ids),
            confirmed_by=confirmed_by,
            confirmed_at_utc=confirmed_at_utc,
        )
        self._confirmation = verify_confirmation(plan, view_model, confirmation)
        return self.confirmation_snapshot()

    def confirmation_snapshot(self) -> dict[str, object]:
        result = self._current_result()
        if self._confirmation is None:
            raise WizardHostError("Create the current review confirmation first.")
        plan = self._object(result.get("plan"), "result.plan")
        view_model = self._object(result.get("view_model"), "result.view_model")
        verified = verify_confirmation(plan, view_model, self._confirmation)
        return {
            "confirmation_scope": verified["confirmation_scope"],
            "plan_sha256": verified["plan_sha256"],
            "view_model_sha256": verified["view_model_sha256"],
            "confirmation_sha256": verified["confirmation_sha256"],
            "acknowledged_ids": list(verified["acknowledged_ids"]),
            "confirmed_by": verified["confirmed_by"],
            "confirmed_at_utc": verified["confirmed_at_utc"],
            "statement": verified["statement"],
            "authority": dict(self._object(verified.get("authority"), "confirmation.authority")),
            "current": True,
        }

    def confirmation_state_snapshot(self) -> dict[str, object]:
        if self.review_result is None:
            return {
                "review_available": False,
                "confirmation_available": False,
                "acknowledgements": [],
                "acknowledgement_evaluation": {
                    "ready": False,
                    "acknowledged_ids": [],
                    "missing_acknowledgement_ids": [],
                },
            }
        return {
            "review_available": True,
            "confirmation_available": self._confirmation is not None,
            "acknowledgements": self.acknowledgement_choices(),
            "acknowledgement_evaluation": self.acknowledgement_evaluation(),
        }

    def review_snapshot(self) -> dict[str, object]:
        snapshot = super().review_snapshot()
        snapshot.update(self.confirmation_state_snapshot())
        if self._confirmation is not None:
            snapshot["confirmation"] = self.confirmation_snapshot()
        return snapshot

    def host_snapshot(self) -> dict[str, object]:
        snapshot = super().host_snapshot()
        snapshot["confirmation_available"] = self._confirmation is not None
        return snapshot

    def _reset_choices(self) -> None:
        super()._reset_choices()
        self._clear_confirmation_state(clear_acknowledgements=True)

    def _clear_confirmation_state(self, *, clear_acknowledgements: bool) -> None:
        if clear_acknowledgements:
            self._acknowledged_ids.clear()
        self._confirmation = None

    def _current_result(self) -> dict[str, object]:
        result = self.review_result
        if not isinstance(result, dict):
            raise WizardHostError("Resolve the current review-only selection first.")
        return result

    def _current_view_model(self) -> dict[str, object]:
        return self._object(self._current_result().get("view_model"), "result.view_model")

    @staticmethod
    def _object(value: object, label: str) -> dict[str, object]:
        if not isinstance(value, dict):
            raise WizardHostError(f"{label} must be an object.")
        return value

    @staticmethod
    def _non_empty_text(value: object, label: str) -> str:
        if not isinstance(value, str) or not value.strip():
            raise WizardHostError(f"{label} must be a non-empty string.")
        return value


__all__ = [
    "WizardConfirmationController",
    "WizardContractError",
    "WizardHostError",
]
