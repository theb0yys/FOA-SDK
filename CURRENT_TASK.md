# Current Task

## Status

PASSED: Pack Manager unsaved-change protection is implemented and verified in
the compiled Editor. The focused change is ready for maintainer review.

## Goal and ownership

Protect unsaved Pack Manager edits before **New mod** or **Open selected** replaces
the form. Primary owner: `workspace-and-packs`. Classification: Routine, within the
existing thin Qt pane and Foundation save/load commands.

## Scope

- Track all 18 editable normal and advanced fields against the new/load/save baseline.
- Display unsaved state and clear it when edits are reverted.
- Offer Save, Discard, or Cancel before either replacement; default and Escape cancel.
- Continue after Save only when persistence succeeds; preserve the full draft on
  validation failure, write failure, cancellation, or a failed target load.
- Remember the requested Open selected target across a successful save/list refresh.

No public API, schema, manifest, identity, dependency, or persistence changes.
Pane/Editor shutdown, workspace switching, other authoring panes, engine source,
game files, runtime, deployment, installers, and releases are outside this scope.
Rollback is a code revert; no document migration is needed.

## Validation

- PASSED: focused Foundation, Editor lifecycle, and path-policy static checks.
- PASSED: 15 focused Python tests and 10 enabled pinned O3DE source validators.
- PASSED: pinned Profile configure and affected Editor module build.
- PASSED: 13 focused compiled pack-persistence, path-policy, and Editor smoke tests.
- PASSED: 14 live Editor scenarios cover all 18 fields, revert-to-clean, both
  replacement commands, Save/Discard/Cancel, Escape and prompt close, validation
  failures, real Windows file-lock failures, retry, failed target load, and
  preservation of the requested Open selected target across save/list refresh.
- PASSED: all 35 measured transitions took 0-437 ms, including automated prompt
  response, below the three-second synthetic-fixture guard.
- PASSED: screenshots show the unsaved indicator and readable Save/Discard/Cancel
  choices with Cancel as the default.
- NOT_APPLICABLE: runtime, installation, deployment, signing, and release.

Live acceptance uses caller-owned synthetic authoring data, isolated settings,
a private processed engine-asset cache, NullRenderer, and the pinned host.
The accepted run omitted --autotest_mode because the pinned Editor deliberately
dismisses modal dialogs in unattended mode. The harness rejects that flag.
Earlier unattended attempts are recorded as FAILED, not accepted UI evidence.
Automatic setup reads installed-game discovery metadata; test writes stayed in
synthetic authoring data and generated host state. No installation or saves changed.
This is Qt authoring evidence; it does not prove asset processing or game runtime.

## Branch and handoff

`codex/pack-unsaved-changes` is based on Save mod PR #251's head
`0b474e9880fb12410f34d254b6c0b085b4fde712`. The Save mod prerequisite remains a
separate reviewed change. A focused PR will identify that dependency.
Approval and merge remain with the maintainer.
