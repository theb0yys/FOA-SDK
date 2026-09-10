# Current Task

## Status

PARTIAL: Pack Manager Save mod is implemented and compiled validation passed.
Live Editor acceptance is BLOCKED during the test instance's startup;
Computer Use denied access to Editor, so the acceptance script has not run.

## Goal

Persist a mod draft successfully before it becomes active. A failed validation or
write retains the previous active mod, saved bytes, and editable draft.

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
- BLOCKED: live Editor UI acceptance and save-action timing. The first launch
  lacked processed engine assets. The retry uses a private copy of an existing
  synthetic project's cache from the same pinned engine, but is paused at the
  startup window; Computer Use was not approved to inspect or access Editor.
- NOT_APPLICABLE: runtime, installation, deployment, signing, and release.

No manifest migration is required. Rollback is a code revert; existing
SaveActivePack callers, manifest shapes, and IDs remain compatible.

## Current branch

`codex/pack-save-isolated`, based on `origin/main` after actor/troop PR #250.

## Remaining acceptance

Run `Gems/TaintedGrailModdingSDK/Tools/editor_tests/pack_save_live_smoke.py` in the
rebuilt Editor using the synthetic pack-save workspace. Verify creation, invalid
edit and real write failures, correction/retry, saved-mod reopen, and deterministic
repeat save. Record actual results before claiming this function complete or
promoting the draft PR to ready for maintainer review.
