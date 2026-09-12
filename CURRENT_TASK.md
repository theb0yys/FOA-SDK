# Current Task

Status: requested implementation and applicable local verification complete.
Maintainer PR review and remote CI remain separate from the local results.
Goal: recover unsaved Actor/Troop Editor drafts after a crash or forced shutdown.
Classification: Significant private persistence addition following the accepted
Pack/ItemRecipe recovery pattern. Primary owner: schemas-and-persistence;
ui-framework owns raw capture/restore and the existing exit transaction.

Implemented a versioned, atomically replaced local recovery copy bound to canonical
workspace identity. Restore / Discard resolves an inline offer before editing.
Raw actor/troop/member fields, selected records/evidence, dirty flags and staged
additions/edits/removals survive completed checkpoints. Restore never writes the
catalog. Unknown/future/malformed/foreign/incompatible copies are kept until
explicit Discard. One serial worker checkpoints after a 750 ms timer.

Accepted close/workspace/exit retirement preserves unresolved offers and failed
saves. A later exit veto restores editable forms and compares the recovery file
with the checkpoint retained by that exact close attempt. Only completed
checkpoints survive forced shutdown; there is no power-loss guarantee.

Branch: codex/actor-troop-draft-recovery from PR #276 at
999de1db5b8dce7b6d76868938713b9210ff04dc. Verified main remains
8fa4601abc3326e7166c59a47948003ec1462acb and is already integrated.
Open prerequisite work: #269, #271, #273, #275 and #276.
The original dirty SDK-client checkout was not used for writes.
Design: docs/tainted-grail-sdk/ACTOR_TROOP_DRAFT_RECOVERY.md.

Validation:

- PASSED: focused population/workspace validators and 23 validator unit tests;
  10 applicable SDK source validators; final fixture syntax and diff hygiene.
- PASSED: configure and build at pinned O3DE
  68683f23fb747380d3efa2424bd5f30242e9c5a2, Windows x64 Profile / Qt 6.10.2.
- PASSED: 541 Catalog compiled tests, including 11 new recovery tests.
  NOT_RUN: two existing PathPolicyServiceTests symlink privilege skips.
- PASSED: 13 native recovery phases / 44 checks, with five deliberate owned-process
  terminations and eight normal exits; maximum checkpoint UI gap 0.078 seconds.
- PASSED: 173 pane-close, both workspace-picker and whole-Editor exit regression
  checks across seven normally exiting Editor processes. Maximum interaction
  2.266 seconds against the five-second synthetic-fixture budget.
- PASSED: final loaded Editor module SHA-256
  5716fafc9aaaf8e1bd4dcf035195a58fb8edd8272b795e86777c707daa30df7c.
- Earlier failed attempts, including a non-reproducing workspace timing miss and
  the fixed close-resume regression, remain in the external evidence pack.
- NOT_APPLICABLE: game runtime, saves, deployment, release and engine edits.

Generated logs, synthetic workspaces, screenshots, source/artifact hashes and
machine-readable evidence remain outside source. No merge, approval, workflow
rerun, release or protected external-data mutation was performed.
