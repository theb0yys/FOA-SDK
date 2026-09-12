# Current Task

Status: implementation and required local validation PASSED; maintainer review pending.
Goal: finish Actor/Troop Editor pane-close protection with Save / Discard / Cancel.
Classification: Routine UI behavior inside the accepted actor/troop architecture.
Primary owner: ui-framework. Existing Foundation population authoring commands
retain catalog validation, evidence and persistence ownership.

## Implemented scope

Docked and floating pane closes offer Save / Discard / Cancel for actor, troop
and member drafts. Save runs the actor command first, then the atomic troop
command including unstaged member edits and staged additions/edits/removals.
Cancel, Escape, prompt dismissal and failed Save keep the pane open. Nested
close requests are refused while the prompt is active. Clean panes do not prompt.
Earlier successful actor saves remain saved if a later troop save fails.

No schema, public API, engine, workspace-switch, whole-Editor rollback, recovery,
game, installation, runtime, deployment or release changes.

Branch: codex/actor-troop-close-protection, based on verified PR #271 head
ac2f18befbfc436c47d89009c4d5c4d07b326f45. Prerequisite PRs #268, #269 and #271
remain separate maintainer decisions. This task changes only the Actor/Troop
widget, its native close fixture/runner, its owning guide/design and this record.
The original dirty SDK-client checkout was not used for writes.

## Executed validation

- PASSED: focused population static validator and 19 validator unit tests.
- PASSED: pinned O3DE source policy (10 enabled validators), PowerShell parsing
  and reviewed diff whitespace checks.
- PASSED: Windows Profile build of TaintedGrailModdingSDK.Editor and
  TaintedGrailModdingSDK.Catalog.Tests using the existing exact-pin configuration.
- PASSED: Catalog CTest selection with --no-tests=error: 530 compiled tests passed.
  Two existing symlink tests were skipped because symlink privileges were absent;
  these skips are not passes.
- PASSED: 33 real-Editor checks on an inactive private Windows desktop, synthetic
  workspace and exact loaded module hash. Coverage includes all three choices,
  Escape/dismissal, nested close, individual and combined drafts, failed actor,
  member and troop validation, real locked-catalog write failures, retry, staged
  membership changes, clean reopening and docked/floating native close controls.
  The Editor initialized and exited normally (exit 0, no forced stop).
- PASSED: visual inspection of the actual close prompt and its default Cancel.
  Maximum measured synthetic close transition: 0.610 seconds, below 5 seconds.
- FAILED (expected baseline regression): the old build lacks Save in its prompt.
  Earlier fixture-development failures are retained separately from the final pass.
- NOT_APPLICABLE: game/runtime, installer, deployment and release evidence.

Engine pin: 68683f23fb747380d3efa2424bd5f30242e9c5a2.
Loaded SDK Editor SHA-256:
727f8ddd3d32a99a59c39d4fa9f45393acd25efc1794c7b90342bf252da77b86.
Machine-readable results, screenshots, logs and synthetic data remain outside
source. The guide documents the native acceptance command. No later feature,
merge, approval, workflow rerun or recurring monitor is authorized by this task.
