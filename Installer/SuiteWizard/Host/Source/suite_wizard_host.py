#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native graphical review and confirmation entry point for the FOA-SDK Suite Wizard."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Sequence

import _suite_wizard_host_impl as impl
from wizard_confirmation_controller import WizardConfirmationController

impl.WizardHostController = WizardConfirmationController


class SuiteWizardHost(impl.SuiteWizardHost):
    """Adds visible invalidation, acknowledgements, and review-only confirmation."""

    def __init__(
        self,
        root: object,
        controller: WizardConfirmationController,
        tk: object,
        ttk: object,
        messagebox: object,
    ) -> None:
        self.ack_vars: dict[str, object] = {}
        super().__init__(root, controller, tk, ttk, messagebox)
        for variable in (
            self.platform_var,
            self.architecture_var,
            self.runtime_target_var,
            self.game_version_var,
            self.branch_var,
        ):
            variable.trace_add("write", self._on_context_changed)
        self.confirmed_by_var.trace_add("write", self._on_confirmation_input_changed)
        self.confirmed_at_var.trace_add("write", self._on_confirmation_input_changed)

    @property
    def confirmation_controller(self) -> WizardConfirmationController:
        if not isinstance(self.controller, WizardConfirmationController):
            raise impl.WizardHostError("The graphical host lacks its confirmation controller.")
        return self.controller

    def _build_ui(self) -> None:
        super()._build_ui()
        self.confirmation_tab = self.ttk.Frame(self.notebook, padding=10)
        self.notebook.add(self.confirmation_tab, text="3. Confirm")
        self._build_confirmation_tab()

    def _build_confirmation_tab(self) -> None:
        ttk = self.ttk
        self.confirmation_tab.columnconfigure(0, weight=1)
        self.confirmation_tab.rowconfigure(1, weight=1)

        ttk.Label(
            self.confirmation_tab,
            text=(
                "Accept every acknowledgement for the exact current review, then provide the "
                "confirming identity and UTC timestamp. This creates a review-only record and "
                "does not acquire, install, elevate, launch, deploy, sign, or publish anything."
            ),
            wraplength=1080,
        ).grid(row=0, column=0, sticky="ew")

        body = ttk.Panedwindow(self.confirmation_tab, orient="horizontal")
        body.grid(row=1, column=0, sticky="nsew", pady=(10, 0))
        self.ack_frame = ttk.LabelFrame(body, text="Required acknowledgements", padding=10)
        confirmation_frame = ttk.LabelFrame(body, text="Exact review confirmation", padding=10)
        body.add(self.ack_frame, weight=3)
        body.add(confirmation_frame, weight=2)

        confirmation_frame.columnconfigure(1, weight=1)
        row = 0
        ttk.Label(confirmation_frame, text="Confirmed by:").grid(row=row, column=0, sticky="w")
        self.confirmed_by_var = self.tk.StringVar()
        ttk.Entry(confirmation_frame, textvariable=self.confirmed_by_var).grid(
            row=row, column=1, sticky="ew", padx=(8, 0)
        )
        row += 1
        ttk.Label(confirmation_frame, text="Confirmed at UTC:").grid(
            row=row, column=0, sticky="w", pady=(8, 0)
        )
        self.confirmed_at_var = self.tk.StringVar()
        ttk.Entry(confirmation_frame, textvariable=self.confirmed_at_var).grid(
            row=row, column=1, sticky="ew", padx=(8, 0), pady=(8, 0)
        )
        row += 1
        ttk.Label(
            confirmation_frame,
            text="Use an ISO-8601 UTC value ending in Z, for example 2026-07-22T12:00:00Z.",
            wraplength=430,
        ).grid(row=row, column=0, columnspan=2, sticky="w", pady=(4, 10))

        self.confirmation_hash_vars = {
            name: self.tk.StringVar()
            for name in ("plan_sha256", "view_model_sha256", "confirmation_sha256")
        }
        labels = {
            "plan_sha256": "Plan",
            "view_model_sha256": "View model",
            "confirmation_sha256": "Confirmation",
        }
        for name in labels:
            row += 1
            ttk.Label(confirmation_frame, text=f"{labels[name]}:").grid(row=row, column=0, sticky="w")
            ttk.Entry(
                confirmation_frame,
                textvariable=self.confirmation_hash_vars[name],
                state="readonly",
            ).grid(row=row, column=1, sticky="ew", padx=(8, 0), pady=1)

        row += 1
        ttk.Label(confirmation_frame, text="Confirmation statement:").grid(
            row=row, column=0, columnspan=2, sticky="w", pady=(10, 0)
        )
        row += 1
        self.confirmation_statement_var = self.tk.StringVar(
            value="Create the exact review confirmation to display its immutable statement."
        )
        ttk.Label(
            confirmation_frame,
            textvariable=self.confirmation_statement_var,
            wraplength=440,
        ).grid(row=row, column=0, columnspan=2, sticky="ew", pady=(4, 10))

        row += 1
        self.confirm_button = ttk.Button(
            confirmation_frame,
            text="Create review confirmation",
            command=self.create_review_confirmation,
            state="disabled",
        )
        self.confirm_button.grid(row=row, column=0, columnspan=2, sticky="ew")
        row += 1
        self.confirmation_status_var = self.tk.StringVar(
            value="Resolve a review and accept every acknowledgement first."
        )
        ttk.Label(
            confirmation_frame,
            textvariable=self.confirmation_status_var,
            wraplength=440,
        ).grid(row=row, column=0, columnspan=2, sticky="ew", pady=(8, 0))

        actions = ttk.Frame(self.confirmation_tab)
        actions.grid(row=2, column=0, sticky="ew", pady=(10, 0))
        ttk.Button(
            actions,
            text="Back to review",
            command=lambda: self.notebook.select(self.review_tab),
        ).pack(side="left")
        ttk.Label(
            actions,
            text="Review-only confirmation — no execution authority is granted.",
        ).pack(side="right")

    def _render_review(self, review: dict[str, object]) -> None:
        super()._render_review(review)
        self._render_acknowledgements(review)
        self._clear_confirmation_display()
        self.confirmation_hash_vars["plan_sha256"].set(str(review["plan_sha256"]))
        self.confirmation_hash_vars["view_model_sha256"].set(
            str(review["view_model_sha256"])
        )
        self.confirmation_status_var.set(
            "Accept every acknowledgement and provide identity plus UTC time."
        )
        self._update_confirm_button()

    def _clear_review(self) -> None:
        super()._clear_review()
        if hasattr(self, "ack_frame"):
            self._clear_children(self.ack_frame)
            self.ack_vars.clear()
        if hasattr(self, "confirmation_hash_vars"):
            self._clear_confirmation_display()
            self.confirmation_status_var.set(
                "Resolve a review and accept every acknowledgement first."
            )
            self._update_confirm_button()

    def _render_acknowledgements(self, review: dict[str, object]) -> None:
        self._clear_children(self.ack_frame)
        self.ack_vars.clear()
        rows = list(review.get("acknowledgements", []))
        if not rows:
            self.ttk.Label(
                self.ack_frame,
                text="The current review exposes no acknowledgement rows.",
            ).grid(row=0, column=0, sticky="w")
            return
        for row_index, raw in enumerate(rows):
            acknowledgement = dict(raw)
            acknowledgement_id = str(acknowledgement["acknowledgement_id"])
            variable = self.tk.BooleanVar(value=bool(acknowledgement["acknowledged"]))
            self.ack_vars[acknowledgement_id] = variable
            checkbox = self.ttk.Checkbutton(
                self.ack_frame,
                text=f"{acknowledgement['title']}  [{acknowledgement['severity']}]",
                variable=variable,
                command=lambda aid=acknowledgement_id, var=variable: self._set_acknowledgement(
                    aid, bool(var.get())
                ),
            )
            checkbox.grid(row=row_index * 2, column=0, sticky="w")
            self.ttk.Label(
                self.ack_frame,
                text=f"{acknowledgement['detail']}\nID: {acknowledgement_id}",
                wraplength=620,
            ).grid(row=row_index * 2 + 1, column=0, sticky="w", padx=(24, 0), pady=(0, 10))
        self.ack_frame.columnconfigure(0, weight=1)

    def _set_acknowledgement(self, acknowledgement_id: str, acknowledged: bool) -> None:
        try:
            state = self.confirmation_controller.set_acknowledgement(
                acknowledgement_id,
                acknowledged,
            )
            self._clear_confirmation_display(keep_review_bindings=True)
            evaluation = dict(state["acknowledgement_evaluation"])
            if evaluation["ready"]:
                self.confirmation_status_var.set(
                    "All acknowledgements are accepted; provide identity and UTC time."
                )
            else:
                self.confirmation_status_var.set(
                    f"{len(evaluation['missing_acknowledgement_ids'])} acknowledgement(s) remain."
                )
            self._update_confirm_button()
        except (impl.WizardContractError, impl.WizardHostError) as exc:
            self._show_error("Acknowledgement update failed", exc)
            self._render_acknowledgements(self.confirmation_controller.review_snapshot())

    def create_review_confirmation(self) -> None:
        try:
            confirmation = self.confirmation_controller.create_review_confirmation(
                expected_plan_sha256=self.review_hash_vars["plan_sha256"].get(),
                expected_view_model_sha256=self.review_hash_vars["view_model_sha256"].get(),
                confirmed_by=self.confirmed_by_var.get(),
                confirmed_at_utc=self.confirmed_at_var.get(),
            )
            self._render_confirmation(confirmation)
            self.notebook.select(self.confirmation_tab)
            self.status_var.set(
                "Exact review-only confirmation is current and displayed."
            )
        except (impl.WizardContractError, impl.WizardHostError) as exc:
            self._show_error("Review confirmation failed", exc)

    def _render_confirmation(self, confirmation: dict[str, object]) -> None:
        self.confirmation_hash_vars["plan_sha256"].set(str(confirmation["plan_sha256"]))
        self.confirmation_hash_vars["view_model_sha256"].set(
            str(confirmation["view_model_sha256"])
        )
        self.confirmation_hash_vars["confirmation_sha256"].set(
            str(confirmation["confirmation_sha256"])
        )
        self.confirmation_statement_var.set(str(confirmation["statement"]))
        self.confirmation_status_var.set(
            f"Current review confirmed by {confirmation['confirmed_by']} at "
            f"{confirmation['confirmed_at_utc']}."
        )
        self._update_confirm_button()

    def _clear_confirmation_display(self, *, keep_review_bindings: bool = False) -> None:
        if not hasattr(self, "confirmation_hash_vars"):
            return
        plan_hash = self.review_hash_vars["plan_sha256"].get() if keep_review_bindings else ""
        view_hash = (
            self.review_hash_vars["view_model_sha256"].get() if keep_review_bindings else ""
        )
        self.confirmation_hash_vars["plan_sha256"].set(plan_hash)
        self.confirmation_hash_vars["view_model_sha256"].set(view_hash)
        self.confirmation_hash_vars["confirmation_sha256"].set("")
        self.confirmation_statement_var.set(
            "Create the exact review confirmation to display its immutable statement."
        )

    def _update_confirm_button(self) -> None:
        if not hasattr(self, "confirm_button"):
            return
        ready = False
        try:
            state = self.confirmation_controller.confirmation_state_snapshot()
            evaluation = dict(state["acknowledgement_evaluation"])
            ready = bool(
                state["review_available"]
                and evaluation["ready"]
                and self.confirmed_by_var.get().strip()
                and self.confirmed_at_var.get().strip()
            )
        except (impl.WizardContractError, impl.WizardHostError):
            ready = False
        self.confirm_button.configure(state="normal" if ready else "disabled")

    def _on_context_changed(self, *_args: object) -> None:
        self.confirmation_controller.invalidate_review()
        self._clear_review()
        self.status_var.set(
            "Compatibility context changed; resolve again to refresh the review."
        )

    def _on_confirmation_input_changed(self, *_args: object) -> None:
        self.confirmation_controller.invalidate_confirmation()
        self._clear_confirmation_display(keep_review_bindings=True)
        self.confirmation_status_var.set(
            "Confirmation identity or UTC time changed; create the confirmation again."
        )
        self._update_confirm_button()


impl.SuiteWizardHost = SuiteWizardHost


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Open or smoke-test the graphical FOA-SDK Suite Wizard review and confirmation host."
    )
    parser.add_argument(
        "--installer-root",
        type=Path,
        default=Path(__file__).resolve().parents[3],
        help="Path to the product-owned Installer directory.",
    )
    parser.add_argument(
        "--smoke-test",
        action="store_true",
        help="Construct the native window, resolve, acknowledge, confirm, verify rows, and exit.",
    )
    return parser


def _smoke_test(installer_root: Path) -> int:
    try:
        import tkinter as tk
        from tkinter import messagebox, ttk
    except ImportError:
        print(
            "FOA-SDK Suite Wizard smoke failed: Python tkinter support is unavailable.",
            file=sys.stderr,
        )
        return 1

    root = None
    try:
        controller = WizardConfirmationController(installer_root)
        root = tk.Tk()
        root.withdraw()
        host = SuiteWizardHost(root, controller, tk, ttk, messagebox)
        root.update_idletasks()
        host.resolve_review()
        root.update_idletasks()

        package_rows = host.package_tree.get_children()
        payload_rows = host.payload_tree.get_children()
        review_hashes = [variable.get() for variable in host.review_hash_vars.values()]
        if len(package_rows) != 2 or len(payload_rows) != 2:
            raise impl.WizardHostError(
                "Default graphical review did not render two package and payload rows."
            )
        if any(len(value) != 64 for value in review_hashes):
            raise impl.WizardHostError(
                "Graphical review did not render the complete exact fingerprint chain."
            )

        for row in controller.acknowledgement_choices():
            acknowledgement_id = str(row["acknowledgement_id"])
            host.ack_vars[acknowledgement_id].set(True)
            host._set_acknowledgement(acknowledgement_id, True)
        host.confirmed_by_var.set("FOA-SDK graphical smoke")
        host.confirmed_at_var.set("2026-07-22T00:00:00Z")
        host.create_review_confirmation()
        root.update_idletasks()

        confirmation = controller.confirmation_snapshot()
        if len(host.confirmation_hash_vars["confirmation_sha256"].get()) != 64:
            raise impl.WizardHostError(
                "Graphical confirmation did not render its exact fingerprint."
            )
        if confirmation["plan_sha256"] != host.review_hash_vars["plan_sha256"].get():
            raise impl.WizardHostError("Confirmation is not bound to the displayed plan.")
        if confirmation["view_model_sha256"] != host.review_hash_vars["view_model_sha256"].get():
            raise impl.WizardHostError("Confirmation is not bound to the displayed view-model.")
        if not confirmation["statement"] or not all(
            value is False for value in confirmation["authority"].values()
        ):
            raise impl.WizardHostError(
                "Graphical confirmation statement or authority boundary is invalid."
            )
        if host.notebook.index(host.notebook.select()) != 2:
            raise impl.WizardHostError("Graphical confirmation did not activate the Confirm page.")
        print("FOA-SDK Suite Wizard native review-confirmation smoke passed.")
        return 0
    except (
        OSError,
        impl.CatalogInterfaceError,
        impl.ResolverError,
        impl.WizardContractError,
        impl.WizardHostError,
        tk.TclError,
    ) as exc:
        print(f"FOA-SDK Suite Wizard smoke failed: {exc}", file=sys.stderr)
        return 1
    finally:
        if root is not None:
            root.destroy()


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    if arguments.smoke_test:
        return _smoke_test(arguments.installer_root)
    forwarded = ["--installer-root", str(arguments.installer_root)]
    return impl.main(forwarded)


if __name__ == "__main__":
    raise SystemExit(main())
