# Current Task

Status: IMPLEMENTED; applicable local validation PASSED. Ready for maintainer review.
Goal: recover unsaved Item and Recipe Editor drafts after a crash or forced shutdown.
Classification: Significant (new private recovery persistence format).
Primary owner: schemas-and-persistence; ui-framework captures and restores forms.
Catalog-and-identity retains the unchanged explicit save commands.

Implemented: workspace-bound atomic recovery, typed values and dirty baselines for
all retained profiles/joins/acquisition forms, Restore/Discard/Retry and bounded
background checkpoints. Save, pane close, workspace replacement and whole-Editor
exit preserve the existing draft protections. Recovery remains available while a
later pane is deciding whether to permit exit; cancelled exits resume checkpointing.

Design and compatibility: docs/tainted-grail-sdk/ITEM_RECIPE_DRAFT_RECOVERY.md.
Catalog, workspace and pack schemas are unchanged. Unknown/incompatible copies
are retained until explicit Discard. Only completed checkpoints survive termination;
no catalog-backup, power-loss or automatic migration guarantee is made.

PASSED: exact-pin configure/build; 530 compiled tests (2 existing Windows symlink
privilege skips); 921 Python tests (33 existing skips: 9 symlink privilege and 24
unconfigured unrelated native TGE fixtures); validators, fixtures and all four
product Gems' pinned source-policy checks. Editor acceptance passed 52 recovery
checks across 13 processes, including 5 intentional forced shutdowns and 8 clean
exits, plus 137 close/exit/workspace regression checks across 7 clean processes.
Measured recovery checkpoint event gap: 125 ms maximum against a 500 ms budget.
Reviewed diff and protected-file audit PASSED. Outputs and machine-readable
verification remain outside source. No game, save, installation or engine-source
writes were performed by this task; unrelated checkout work was preserved.

Engine pin: 68683f23fb747380d3efa2424bd5f30242e9c5a2 (Windows x64, Qt 6.10.2).
Verified SDK Editor DLL SHA-256:
dd27bb7966470c35c66d45972df166be502948d9a14feb19a37bdb2591adfc8d.
Branch: codex/item-recipe-draft-recovery, based on PR #269's original feature head
71fcce20341a3b9c943d5516c92307a7a703de34 and integrated with main at
1e74bc42c9224b81890d77f9f768797c965d5e22. Final build source: cc0e494322.
The final task-status update changes documentation only. PR #268/#269 remain
separate maintainer decisions; their prerequisite behavior is included here.
GitHub CI is separate from these local results. Runtime, installer, deployment
and release validation: NOT_APPLICABLE. Next action: maintainer PR review.
