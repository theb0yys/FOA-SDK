# Current Task

Status: requested implementation and required local validation PASSED;
maintainer review pending.
Goal: protect unsaved Actor/Troop Editor drafts when exiting the entire Editor.
Classification: Routine UI integration using the existing pinned-host close
transaction pattern. Primary owner: ui-framework. Foundation retains population
persistence. No public contract, schema, durable recovery format or runtime changes.

## Implemented scope

Save / Discard / Cancel covers actor, troop and staged/unstaged member drafts.
Cancel, Escape, prompt dismissal and failed Save keep the Editor open. Earlier
successful saves remain saved. A later pane's exit veto restores raw fields,
record/member selections, evidence selections, member staging/removals, dirty
flags and deferred filter text/choices after the host destroys the accepted pane.
Clean panes do not prompt. Nested exit is refused while the outer prompt is open.
The snapshot exists only for the close dispatch and is forgotten on workspace
commit. Actor/Troop crash recovery remains a separate task.

Branch: codex/actor-troop-editor-exit-protection, based on PR #275 head
2bbb861ab2e9cf1e004bd164024083aebf965441. Main
8fa4601abc3326e7166c59a47948003ec1462acb is integrated.
Prerequisite PRs #269, #271, #273 and #275 remain maintainer decisions.
The original dirty SDK-client checkout was not used for writes.

## Executed validation

- PASSED: population/workspace validators, 23 focused validator tests, all 10
  enabled source-policy validators on the SDK, PowerShell parsing and diff checks.
- PASSED: pinned Windows Profile Editor and Catalog.Tests build. No build-graph
  changes required reconfiguration.
- PASSED: Catalog CTest with --no-tests=error: 530 tests passed. Two existing
  Windows symlink-privilege tests were SKIPPED and are not counted as passes.
- PASSED: 50 native whole-Editor exit checks (Save, Discard, clean and later-pane
  rollback), 33 pane-close checks, and 45 checks for each workspace picker.
  All seven final processes initialized, loaded the expected module and exited
  normally with code 0. Maximum whole-exit interaction: 1.500 seconds;
  maximum across these suites: 1.609 seconds, below the five-second
  synthetic-fixture budget. This is not a large-catalog performance claim.
- PASSED: visual inspection of the actual Save / Discard / Cancel prompt.
- FAILED (expected old-build baseline): whole-exit fixture identified pane-close
  wording on the previous build. A fixture-startup failure was corrected to
  recreate an existing saved floating layout before requiring a docked pane.
- FAILED (outside this change): whole-repository source scan found copyright,
  generated-project and Unicode policy failures in 28 unchanged installer/plugin
  files. Git confirmed none differ from this task's base; the scoped SDK scan passes.
- NOT_APPLICABLE: game/runtime, installer, deployment, release and schema migration
  evidence for this UI change. Later independent-file veto uses the same inspected
  pinned-host close dispatch; native rollback acceptance exercises a later pane.

Engine pin: 68683f23fb747380d3efa2424bd5f30242e9c5a2.
Loaded SDK Editor SHA-256:
8da7115f8e9717af24069dcbe8c37c5b256f98825fb74ee1e486c6949b94fc75.
Synthetic workspaces, screenshots, logs and machine-readable verification remain
outside source. The guide documents the exit behavior and reproducible commands.
