# Current Task

## Status and scope

PARTIAL: workspace-switch protection is implemented and its focused validation passed.
The broader Editor-exit regression suite failed and remains unresolved. Handoff is a
draft PR, not a claim that all Editor regressions are clear.

Significant: adds a trusted-host workspace admission/commit notification contract.
Primary owner: workspace-and-packs (Foundation); UI owns draft presentation.

## Design and compatibility

Foundation validates a complete LoadWorkspace candidate before synchronous admission.
Each draft owner can veto while the original workspace remains current. Save writes
there before publication. Cancel, failed draft Save, or reentrant replacement keeps
the current workspace. SetWorkspace uses the same admission boundary.
Successful publication notifies Pack Manager to reset the form and baseline using
the new profile. Invalid candidates never ask for discard. Same-workspace reloads
are protected.

LoadWorkspace retains bool/error reporting and adds an optional cancellation output
for the two opening panes. SetWorkspace now returns success. Existing call statements
remain valid. These internal host APIs are unavailable through ExtensionAPI.
No serialized format, stable identity, migration, dependency or engine-source change.

## Validation

- PASSED: configure and build of TaintedGrailModdingSDK.Editor and
  TaintedGrailModdingSDK.Catalog.Tests against clean pinned O3DE
  68683f23fb747380d3efa2424bd5f30242e9c5a2, after integration with main 7720e036eb.
- PASSED: all 33 focused workspace/persistence/preview compiled tests, including
  18 workspace-load transaction tests and two workspace-isolation tests.
- PARTIAL: expanded compiled suite: 471 passed, zero failed; two existing path-policy
  symlink tests were NOT_RUN because this Windows host lacks symlink privileges.
- PASSED: 44 actual Editor checks across SDK Status and Catalog Browser. Both owned
  processes exited with code zero and about-to-quit notifications. Loaded SDK module
  paths and hashes were recorded. Coverage includes Save/Discard/Cancel, Escape and
  prompt dismissal, picker cancellation, invalid destination, invalid and locked-file
  Save, retry, old-root writes, new/saved drafts and same-workspace reloads.
- PASSED: Foundation, Editor lifecycle and path-policy validators; 15 focused Python
  tests; all 10 enabled pinned source validators; diff whitespace.
- FAILED: the unchanged Editor-exit regression suite did not reach its final Save
  prompt in the save case. Later diagnostic attempts were inconsistent, including
  a destroyed pane assertion after a cancelled titlebar close. No root cause or fix
  is established. The owner stopped that investigation. Diagnostic-only edits to the
  shutdown test were removed from the source diff; evidence remains outside source.
- NOT_RUN: separate New/Open/pane-close live suite for this change.
- NOT_APPLICABLE: Asset Processor, game runtime, installer, deployment and release.

The workspace suite ran 38 transitions within its five-second interaction guard.
These are synthetic host measurements, not a large-workspace storage benchmark.
Generated fixtures, logs, screenshots and build output remain outside source.
Qt uses normal desktop layout preferences; Catalog Browser's remembered workspace
setting is restored by the new test. No game files or saves were modified.

## Handoff

Branch: codex/pack-workspace-switch-protection, based on main
7720e036ebd8a946364bbd5b4ad3de6bfe1912ce after the prerequisite PRs merged.
The owner requested the next useful handoff step. Deliver this focused change as a
DCO-signed commit and draft PR with the unresolved regression reported explicitly.
Approval and merge remain with the maintainer.

Local setup's game-profile detection, other panes' drafts, forced-exit recovery
and autosave are outside this feature's scope.