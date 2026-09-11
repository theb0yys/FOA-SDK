# Current Task

## Status and scope

PASSED: workspace-switch protection and the focused Editor shutdown regression.
PR #260 is ready for maintainer review. Expanded suites remain PARTIAL only for
the Windows symlink tests that could not run on this host.

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
- PASSED: complete static validator/fixture pipeline and all 10 enabled pinned
  source validators. Python discovery: 826 passed, zero failed; nine unrelated
  symlink cases NOT_RUN because Windows symlink privileges are unavailable.
  All four new atomicity-validator tests passed, including missing/reordered-step
  mutations. The validator follows FinishWorkspaceChange rather than requiring
  RefreshSnapshot directly inside LoadWorkspace.
- PASSED: all nine shutdown/reopen Editor processes, 27 assertions, 22 measured
  transitions (maximum 1.922 seconds), initialization and about-to-quit events,
  exit code zero, no forced cleanup. Cancel/Escape/dismissal, invalid input and
  locked-file Save preserve the full draft; successful Save/Discard exits and
  three fresh-process reopens verify persistence.
  The shutdown harness previously ran synchronously during InitInstance. It now
  waits for NotifyEditorInitialized and queues its actions onto the event loop.
  Tests used an external private-desktop launch adapter to avoid visible dialogs;
  all committed cases and parent assertions were retained. Earlier failed runs
  remain in external evidence. No production C++ or Editor binary changed for
  this correction; the validated module hash still matches the workspace run.
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
The owner authorized resolving the shutdown regression and completing the handoff.
Deliver the focused correction as a DCO-signed commit on PR #260; preserve its
ready-for-review state. Approval and merge remain with the maintainer.

Local setup's game-profile detection, other panes' drafts, forced-exit recovery
and autosave are outside this feature's scope.