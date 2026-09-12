# Current Task

## Scope and status

PASSED: finish docked Pack Manager close-and-recovery verification on PR #262.
The verification gap is closed. Routine harness correction inside the accepted recovery
design; the containing recovery PR remains Significant because it adds persistence.
Primary owner: workspace-and-packs; consumer: Editor acceptance.
Branch: codex/pack-draft-recovery. Follow-up base: 8b60308743e6af7363db8ce360f615c569fdb240.
The user explicitly requested this follow-up. PR #262 still depends on #260; no merge
or approval is authorized by this verification task.

## Changes

- Open the registered pane through QtViewPaneManager UseDefaultState for every docked
  test open, including reopens. This avoids stale floating-layout caches without manually
  changing dock ownership. The test-only bridge checks Windows x64 / Qt 6.10.2 and uses
  inspected public exports from already-loaded host modules. Pin/Qt migrations must
  review this bridge; it does not add a product API.
- Target the actual embedded dock tab's Close action. If a native popup is dismissed on
  the inactive private desktop, activate its exact enabled QAction and record the route.
  Floating closure still uses the actual titlebar button. Forced Python pane closure
  is not used to prove the guard.
- Gate the complete New/Open/docked/floating smoke on NotifyEditorInitialized and require
  actual clean process exit. Add `-Suite docked` to the recovery runner to include this
  regression alongside all recovered-draft checks.
- Verify recovered raw fields, baseline/identity retention, Cancel/Escape/prompt-close,
  failed validation, real locked-manifest write failure, successful Save, Discard,
  registered dock destruction, recovery retirement and correct saved-state reopening.

No production C++, schemas, engine files, dependencies, installer or runtime behavior
changed. The separate schema and recovery guarantee remain as documented in
[Pack Manager draft recovery](docs/tainted-grail-sdk/PACK_DRAFT_RECOVERY.md).

## Executed validation

- PASSED: `run_pack_draft_recovery_smoke.ps1 -Suite docked` on the private Windows desktop:
  12 processes, 50 named aggregate checks, four checkpoint-verified intentional stops,
  eight clean exits, zero unintended crashes or forced cleanup. The docked/floating
  regression contributes 29 checks. All child receipts match the loaded SDK module hash.
- PASSED: 83 recorded fixture waits/transitions, maximum 0.781 s. These are bounded
  synthetic interaction measurements, not general storage throughput or layout-quality
  acceptance. Recovery action screenshots are included in the external evidence.
- PASSED: all static validators using `run_local_validation.py --keep-going --static-only
  --skip-source-policy --skip-unit-tests --skip-fixtures`.
- PASSED: all 10 enabled pinned source-policy validators; Python AST and PowerShell syntax;
  focused diff/protected-file review and whitespace check.
- NOT_RUN: new configure/build/compiled tests for this follow-up; production and compiled
  test sources are unchanged. The existing 37/37 compiled-test and Editor build evidence
  remains tied to the identical SDK module SHA-256 below. No new compiled pass is claimed.

Pinned external engine: `68683f23fb747380d3efa2424bd5f30242e9c5a2`.
SDK module SHA-256: `4CE64683E1C1A04E0DE779674B306D4093DBBD82AC742CC08CC61E9A107756DC`.

The earlier docking probes failed on remembered layout, outer-titlebar lookup, private
popup handling and generic Qt docking mutation. The committed runner passed using the
host's registered default-open route and actual pane actions. Historical failed receipts
and the earlier Qt/FancyDocking crash remain retained; they are superseded for this
functional acceptance by the complete 12-process pass, not relabelled as passing runs.

## Evidence and handoff

Machine-readable `docked-verification.json`, per-process receipts, source/module hashes,
logs and screenshots remain outside source. Private desktops were never activated; Qt
layout preferences are not independently isolated. No protected game data, saves,
credentials, user-owned Editor process, installation or external engine source was modified.
Runtime, installer, deployment and release sign-off: NOT_APPLICABLE; none performed.

This follow-up changes only the four Editor test/helper scripts and their two owning
guides plus this task record. Deliver through the existing PR #262 with DCO sign-off.
Maintainer review and merge remain outstanding. Historical unrelated Python symlink
privilege skips from the initial recovery validation are not converted into passes.
