# Current Task

Status: implementation and required local validation PASSED; maintainer review pending.
Goal: protect unsaved Actor/Troop Editor drafts when switching workspaces.
Classification: Routine UI integration within the accepted workspace admission
and commit contract. Primary owner: ui-framework; Foundation retains candidate
validation, workspace replacement and population persistence ownership.

## Implemented scope

Both workspace pickers offer Save / Discard / Cancel for actor, troop and member
drafts in docked and floating panes. Save uses the current workspace and existing
actor/atomic troop commands. Cancel and failed Save preserve the workspace and
remaining drafts. Earlier successful saves remain saved if a later save or
handler fails. Discard retires drafts only after successful replacement, so a
later handler veto or failed same-root reload preserves them. Successful switches
reset selections and member staging, including same-workspace reloads and shared
record IDs in another root. Pane close reuses the same prompt/save logic.

No public API, schema, game, installation, runtime, deployment or release changes.
Whole-Editor exit and Actor/Troop crash recovery remain separate tasks.

Branch: codex/actor-troop-workspace-protection, based on PR #273 head
3f2fda6b4f0da19125f4736d22c00243a0461e9b. Main at
8fa4601abc3326e7166c59a47948003ec1462acb is integrated; only the task record
conflicted. Prerequisite PRs #269, #271 and #273 remain maintainer decisions.
The original dirty SDK-client checkout was not used for writes.

## Executed validation

- PASSED: population/workspace static validators, 23 focused validator tests,
  all 10 enabled pinned source-policy validators, PowerShell parsing and diff checks.
- PASSED: exact-pin Windows Profile configure and Editor/Catalog.Tests build.
- PASSED: Catalog CTest with --no-tests=error: 530 tests passed; two existing
  Windows symlink-privilege tests were skipped, not counted as passes.
- PASSED: 45 SDK Status workspace checks, 45 Catalog Browser workspace checks,
  and 33 native pane-close checks (123 total). All three final Editor processes
  initialized, loaded the expected module, and exited normally with code 0.
- PASSED: actual prompt visual inspection. Maximum synthetic workspace
  interaction was 1.204 seconds, below the five-second fixture budget.
- FAILED (expected baseline): old build retained the previous actor selection
  when switching to another workspace with matching IDs. Fixture-development
  failures corrected an empty actor form's zero-level expectation and reopened
  the saved mod after workspace loading; missing active mod correctly vetoed Save.
- NOT_APPLICABLE: game/runtime, installer, deployment and release evidence.

Engine pin: 68683f23fb747380d3efa2424bd5f30242e9c5a2.
Loaded SDK Editor SHA-256:
d9adce4716ac9ea50794f5313de01d4f2dbe2510179ddb9a37b63b95c2455c4f.
Synthetic data, screenshots, logs and machine-readable verification remain
outside source. The guide includes reproducible commands and active-mod setup.
