# Current Task

## Status and goal

PASSED: Pack Manager drafts are protected during normal Editor exit. Save completes
before exit; Discard exits without writing; Cancel, Escape, prompt dismissal, or
failed Save keeps the Editor and full draft open.

## Classification and scope

Routine; primary owner workspace-and-packs. Inspection and live tests confirmed
the existing PackManagerWidget closeEvent guard already covers shutdown through
the pinned Editor's MainWindow::closeEvent and ClosePanesWithRollback.
No additional production shutdown handler was needed.

This change adds a live Python acceptance script, a reusable Windows process
runner, and user/developer documentation. No C++, public API, schema, persistence,
dependency, engine-source, game-runtime, or deployment change.

Normal File > Exit, main-window Close, ordinary Python exit, floating Pack Manager,
new/saved drafts, invalid input, actual locked-file save failure, retry, discard,
clean exit and fresh-process reopen are covered. Forced termination/crashes,
autosave/recovery, other panes' draft state and workspace switching are out of scope.

## Validation

- PASSED: reusable runner completed nine Editor processes with exit code zero,
  about-to-quit notification and passing in-process assertions; no forced cleanup.
  Includes 27 recorded checks and three fresh-process saved-manifest reopens.
- PASSED: full draft, previous active mod and saved bytes survive cancelled/failed
  shutdown. Successful Save/Discard exits and silent clean/pristine exits passed.
- PASSED: prompt and preserved-draft screenshots reviewed.
- PASSED: foundation, Editor lifecycle and path-policy validators; 15 focused
  Python tests; all 10 enabled pinned source validators; diff whitespace.
- PASSED: 13 compiled PackPersistenceService, PackManifestPackagePathPolicy and
  DeveloperPreviewSmoke regression tests.
- NOT_RUN: new configure/build; only tests/docs changed. The SDK Editor module's
  SHA-256 matches the verified PR #256 build at pinned O3DE
  68683f23fb747380d3efa2424bd5f30242e9c5a2. Production source is unchanged.
- NOT_APPLICABLE: Asset Processor, game runtime, installer, deployment and release.

The nine-process suite took 127.932 seconds; longest automated interaction was
0.688 seconds against a five-second hang guard. This is host interaction evidence,
not a storage benchmark. The earlier New/Open/pane-close coverage remains in its
separate existing live script; it was not rerun for this tests/docs-only change.

Synthetic authoring workspaces, project user/log data and temporary environment
are isolated; Qt uses normal desktop layout preferences. Startup can read
installed-game discovery metadata; no game files or saves were modified.
Two exploratory harness attempts failed on menu-wrapper lifetime and an already
floating pane. Both were corrected before the complete passing runner invocation.

## Branch and handoff

Branch: codex/pack-editor-exit-protection, based on PR #256 commit
186a6396a52c2d1c6345247ad01929165d7ff623. PR #256 includes prerequisites #254 and #251.
Deliver the focused DCO commit through a PR. Maintainer approval/merge remain
outstanding. Exact commands, results, logs, screenshots and artifact hashes are
retained outside the source checkout.
