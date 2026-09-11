# Current Task

## Scope and status

IMPLEMENTED: recover unsaved Pack Manager drafts after a crash or forced shutdown.
Targeted feature acceptance: PASSED. Broader validation coverage: PARTIAL as detailed below.
Significant: introduces a separate, versioned authoring-recovery document.
Owner: workspace-and-packs (Foundation persistence); Pack Manager presents recovery.
Branch: codex/pack-draft-recovery, based on PR #260 at ca615f8f612e4489540285f7e9656db79ec8369a.
PR #260 remains open; this feature does not authorize merging it.

## Behavior and compatibility

[Pack Manager draft recovery](docs/tainted-grail-sdk/PACK_DRAFT_RECOVERY.md) defines
schema 1, ownership, bounds, failure behavior, validation and rollback. Recovery stores
all 18 raw fields, their dirty baseline, saved/new identity and advanced-panel state in
a separate per-user directory. Workspace ID, canonical root and document path bind each
copy. A process lock excludes concurrent Editors. Recovery never writes a pack manifest
or activates a mod. Restore retains the copy until explicit successful Save/Discard.
Cancel and failed Save preserve recovery; workspace Discard retires it only after commit.

One 750 ms timer and one serial background worker coalesce edits. Atomic replacement
has no direct-write fallback. Files are capped at 256 KiB and fields at 16,384 UTF-16
characters. Errors appear inline. Unreadable, foreign and future-format copies are
preserved until explicit discard. Existing pack/workspace schemas and dependencies are
unchanged; earlier Editors ignore the new store. No saved-mod migration is required.
Edits after the last completed checkpoint may be lost; power-loss durability is not claimed.

## Validation performed

Evidence is external to source. Final production SDK module SHA-256:
`4CE64683E1C1A04E0DE779674B306D4093DBBD82AC742CC08CC61E9A107756DC`.
Pinned external engine: `68683f23fb747380d3efa2424bd5f30242e9c5a2`, unchanged.

- PASSED: configure and profile builds of SDK Editor and Catalog.Tests, parallelism 2.
- PASSED: 37/37 focused compiled tests: PackDraftRecoveryTests (11),
  FoundationServiceWorkspaceLoadTests (18), FoundationWorkspaceIsolationTests (2),
  PackPersistenceServiceTests (6). Includes malformed/future/foreign/oversized data,
  round trips, lock exclusion, and actual Windows locked-file write/delete failures.
- PASSED: all static validators and fixtures; 10 enabled pinned source-policy validators.
  Initial catalog inventory/platform-macro failures were corrected and rerun successfully.
- PARTIAL: Python discovery ran 835 tests: 826 passed, 9 skipped because Windows
  symlink privileges were unavailable. No skipped test is counted as passed.
- PASSED: all 11 fresh-process recovery phases on a private Windows desktop, 16 named
  aggregate checks. Four checkpoint-verified intentional terminations and seven normal
  exits. Covers raw new/saved drafts, all fields/baselines, repeated crash, workspace
  isolation, Restore/Discard, failed Save/Cancel, failed checkpoint/retry, corrupt-copy
  rejection, manifest equivalence after Save and fresh-process retirement checks.
  Twenty bounded waits; maximum observed 0.781 s on the synthetic fixture.
- PASSED: workspace-switch regression, both routes, 44 checks in two clean processes.
- PASSED: Editor-exit regression, nine clean processes and 27 checks, including three
  fresh-process reopens, failed writes and Cancel/Escape/prompt-close vetoes.
- PASSED: New/Open and floating titlebar pane-close regression, 23 checks in one clean
  process, including all-field dirty/revert, real write failures and saved/new drafts.
- PARTIAL: the expanded docked-titlebar regression could not establish its expected
  initial docked layout. An external test adapter that manually changed dock ownership
  then crashed in pinned Qt/FancyDocking titlebar handling. This is not counted as a
  passing docked-pane test or a demonstrated recovery defect; no production docking
  change was made. Docked-pane acceptance on this binary remains unverified.

The recovery runner is committed. Regression adapters, logs, images, dumps, fixtures,
source/module hashes and machine-readable evidence remain outside source. Private
Windows desktops prevented test dialogs from interrupting the user; Qt layout settings
were not independently isolated. No engine source, protected game files, saves,
credentials, installer or deployment state was changed. No runtime sign-off performed.

## Handoff

The diff is limited to recovery service/UI integration, owned build/test registration,
compiled and actual-Editor acceptance tests, and behavior documentation. DCO commit and
pull request are the delivery transition; approval and merge remain with the maintainer.
The prerequisite PR must be integrated before this dependent change can reach main.
