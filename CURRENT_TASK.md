# Current Task

## Status

PASSED: Pack Manager Save mod is implemented and validated in the compiled Editor.
The change is ready for maintainer review in PR #251.

## Goal

Persist a mod draft successfully before it becomes active. Failed validation or
writes retain the previous active mod, saved bytes, and editable draft.

## Classification and ownership

Significant: additive Foundation command `SavePackAndActivate`. Primary owner
`workspace-and-packs`; supporting owners `schemas-and-persistence` and
`foundation-services`. The pane forwards the draft; Foundation validates and
publishes after persistence; PackPersistenceService atomically replaces the file.

## Implemented scope

- Save-before-activation and one coherent notification after successful saving.
- Atomic replacement with existing schema-1 serialization and directory creation.
- New drafts retain the previous active mod until saved.
- Saved-list labels support plain manifests and serialization envelopes.
- Five compiled regression tests, a synthetic live Editor script, and user guide.

Other Pack Manager workflows, schema migration, game data, deployment, runtime,
installer, releases, and Spawn/Encounter authoring are outside this task.

## Validation

- PASSED: focused Foundation, Editor lifecycle, and path-policy static validators.
- PASSED: 15 Python validator tests and 10 enabled pinned O3DE source validators.
- PASSED: pinned Profile Editor and Catalog test builds.
- PASSED: 13 focused compiled tests; full Catalog suite discovered 436 tests,
  with 434 passed, two existing Windows symlink-privilege skips, and no failures.
- PASSED: five real Editor workflow scenarios: create/persist/activate, invalid
  edits, a Windows file-lock write failure and corrected retry, failed new draft
  followed by rename/retry, and equivalent reopen/deterministic repeat save.
- PASSED: seven synchronous save attempts measured at 0-32 ms in the synthetic
  fixture, below the three-second acceptance guard; this is not a large-catalog
  or asset-processing performance claim.
- PASSED: screenshot inspection shows the saved name/version, saved-mod list,
  editable details, Save mod button, and success status.
- NOT_APPLICABLE: runtime, installation, deployment, signing, and release.

The live run used the exact pinned Profile Editor, NullRenderer, a private
processed engine-asset cache, isolated local settings, and the supported
`wait_for_connect=0` option. It validates the Qt authoring pane, not Asset
Processor execution or 3D rendering. Automatic setup read installed-game
discovery metadata during early attempts; all test writes stayed in synthetic
authoring data and generated host state. No game installation or saves changed.

No manifest migration is required. Rollback is a code revert; existing
SaveActivePack callers, manifest shapes, and IDs remain compatible.

## Current branch

`codex/pack-save-isolated`, based on `origin/main` after actor/troop PR #250.

## Handoff

PR #251 is the focused maintainer-review handoff. Approval and merge remain with
the maintainer. No subsequent service or milestone is authorized by this record.
