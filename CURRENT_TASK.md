# Current Task

## Status and goal

PASSED: Pack Manager protects unsaved drafts when its pane closes. Save writes
before closing; Discard closes without writing; Cancel, Escape, closing the prompt,
or a failed Save keeps the pane open with the full draft. Clean forms close silently.

## Classification and ownership

Routine. Primary owner: workspace-and-packs; surface: PackManagerWidget and its
existing live Editor acceptance script. This reuses the established form baseline,
prompt, and Foundation save command. No public API, schema, dependency, manifest,
identity, or persistence changes; rollback is a code revert without migration.

The exact pinned host's RegisterViewPane contract documents closeEvent/ignore as
the supported veto. StyledDockWidget forwards close requests to the pane. Pack
Manager remains deletable: accepted closes destroy it and reopening constructs a
view of the active saved mod.

## Scope and boundaries

- Guard normal docked and floating pane closes using the existing draft prompt.
- Keep pane, full form, active mod, and saved bytes intact on cancellation/failure.
- Save before accepting close; discard without writing; close clean forms silently.
- Verify actual pane Close controls, destruction, and equivalent reopen in the Editor.
- Retain the existing New mod / Open selected regression coverage.

Forced programmatic host teardown, Editor shutdown coordination, workspace
switching, other panes, engine source, game installation/saves, runtime, deployment,
installer, and release remain outside this task.

## Validation

- PASSED: Profile Editor module and Catalog test target build in the existing
  isolated configuration at O3DE pin 68683f23fb747380d3efa2424bd5f30242e9c5a2.
- PASSED: foundation, Editor lifecycle, and path-policy validators; 15 focused
  Python tests; all 10 enabled pinned O3DE source validators; diff whitespace.
- PASSED: 13 compiled PackPersistenceService, PackManifestPackagePathPolicy, and
  DeveloperPreviewSmoke tests.
- PASSED: 29 live Editor scenarios: 14 existing New/Open regressions plus 15 close
  scenarios using the docked pane Close menu and rendered floating-window Close
  button. Cancellation, Escape, prompt close, invalid input, actual Windows
  file-lock save failure, unlocked retry, saved/new drafts, discard, silent clean
  close, destruction, and equivalent reopen are covered.
- PASSED: prompt and preserved-draft screenshot review. All automated transitions
  finished within the three-second fixture guard; maximum measured time: 0.328 s.
- NOT_APPLICABLE: game runtime, deployment, installation, signing, and release.

The live script uses synthetic authoring data, private generated host state and
processed engine-asset cache, NullRenderer, isolated settings and normal modal
behavior (without --autotest_mode). This proves Qt authoring behavior, not asset
processing or game runtime. Automatic setup can read installed-game discovery
metadata; no protected game or save writes were performed. Only the owned test
Editor was stopped and the temporary build-drive alias removed.

Early test runs failed while locating O3DE's managed pane controls; one unsupported
test-only layout manipulation crashed the host. The final test uses supported
Close/Undock controls and passes; those earlier attempts are not passing evidence.

## Branch and handoff

Branch: codex/pack-close-protection, based on prerequisite PR #254 head
51b816e9f7499fd03858d44b0ead43ab7679efe1. PR #254 in turn includes Save mod PR #251.
Deliver the focused DCO commit through a PR for maintainer review. Approval and
merge remain with the maintainer. Logs, results, screenshots, and machine-readable
validation evidence remain outside the source checkout.
