# Current Task

Status: implemented; local verification passed; ready for pull-request handoff.
Goal: protect unsaved Item and Recipe Editor drafts when exiting the entire Editor.
Classification: Routine. Primary owner: ui-framework; catalog-and-identity owns
unchanged per-form save commands.

In scope: Save / Discard / Cancel through File Exit and main-window Close;
Cancel, Escape, prompt dismissal and failed Save keep the Editor open. A later
pane veto restores drafts after Discard and preserves successful saves after Save.
The retained copy exists only during the main-window close dispatch. Legacy
Python close-all-windows cancellation also retains ordinary pane-close protection.
Out of scope: crash recovery, engine changes, schema changes, game/runtime,
deployment and coordinated rollback of Python's individual floating-pane closes.

Acceptance: PASSED exact-pin SDK Editor module build; 137 real Editor checks
(38 exit, 25 pane-close, 74 workspace); 497 compiled tests; 855 Python tests;
repository static checks and all 10 enabled pinned source-policy validators.
Eleven existing Windows symlink-privilege checks were skipped (nine Python, two
compiled). Every Editor suite exited natively with code zero and no forced stop.

Current branch: codex/item-recipe-editor-exit-protection, based on PR #268.
Authority: current owner request, AGENTS.md, ENGINEERING_PROCESS.md, the owning
Item/Recipe guide, CI_AND_LOCAL_VALIDATION.md and inspected pinned host lifecycle.
Next action: submit the focused change for maintainer review; approval and merge
remain with the maintainer.
