# Current Task

Status: implemented; validation PARTIAL pending current-main integration and lifecycle regressions.
Goal: recover unsaved Item and Recipe Editor drafts after a crash or forced shutdown.
Classification: Significant (new private recovery persistence format).
Primary owner: schemas-and-persistence; ui-framework captures and restores forms.
Catalog-and-identity retains the unchanged explicit save commands.

In scope: workspace-bound atomic recovery, typed values and dirty baselines for
all retained profiles/joins/acquisition forms, Restore/Discard/Retry, bounded
background checkpoints, ordinary Save/Discard retirement and prior close/exit/
workspace protections. No catalog, workspace or pack schema changes.
Out of scope: game data, saves, runtime, engine changes, catalog backup, power-loss
durability, unrelated panes and automatic migration of incompatible recovery data.

Design and compatibility: docs/tainted-grail-sdk/ITEM_RECIPE_DRAFT_RECOVERY.md.
Acceptance: compiled round-trip/isolation/malformed/lock/failure tests, exact-pin
build, real multi-process forced termination and recovery, prior lifecycle UI
regressions, static/source-policy checks and protected-file audit. Outputs remain
outside source. No game or installation writes.
Current branch: codex/item-recipe-draft-recovery, from verified PR #269 head
71fcce20341a3b9c943d5516c92307a7a703de34. Main unchanged at intake.
PASSED: recovery store and pane build, 508 compiled tests (2 privilege skips), static validation, source policy and forced-shutdown recovery acceptance. Final current-main verification is pending.
