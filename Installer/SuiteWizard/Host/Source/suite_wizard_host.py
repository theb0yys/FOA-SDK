#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native graphical review entry point for the FOA-SDK Suite Wizard."""

from __future__ import annotations

from typing import Sequence

import _suite_wizard_host_impl as impl


class SuiteWizardHost(impl.SuiteWizardHost):
    """Adds visible review invalidation when compatibility context changes."""

    def __init__(
        self,
        root: object,
        controller: impl.WizardHostController,
        tk: object,
        ttk: object,
        messagebox: object,
    ) -> None:
        super().__init__(root, controller, tk, ttk, messagebox)
        for variable in (
            self.platform_var,
            self.architecture_var,
            self.runtime_target_var,
            self.game_version_var,
            self.branch_var,
        ):
            variable.trace_add("write", self._on_context_changed)

    def _on_context_changed(self, *_args: object) -> None:
        self._clear_review()
        self.status_var.set(
            "Compatibility context changed; resolve again to refresh the review."
        )


impl.SuiteWizardHost = SuiteWizardHost


def main(argv: Sequence[str] | None = None) -> int:
    return impl.main(argv)


if __name__ == "__main__":
    raise SystemExit(main())
