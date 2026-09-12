# Current Task

Status: implemented; required local validation PASSED. PR handoff pending.
Goal: finish Spawn & Encounter Editor pane-close protection.
Classification: Routine. Primary owner: ui-framework; content-pack-authoring and
Foundation retain existing encounter validation and atomic catalog persistence.

Dirty docked and floating panes offer Save / Discard / Cancel. Save calls the
existing complete encounter save command. Cancel, Escape, prompt dismissal,
nested close and failed validation/persistence keep the pane and raw drafts open.
Discard closes without publication; clean close does not save or prompt.

Branch: codex/spawn-encounter-close-protection from PR #277 at
c00b70d28662a5a1579868a2223d11deb468161b. Verified main remains
8fa4601abc3326e7166c59a47948003ec1462acb and is already integrated.
The main-targeted branch inherits the unmerged Item/Recipe and Actor/Troop draft
protection/recovery prerequisites; this task adds only seven focused files.
The original dirty SDK-client checkout is not used for writes.

Scope: SpawnEncounterEditorWidget header/implementation, dedicated native close
fixture/runner, guide/design and this task record. This increment changes no public
API, schema, build graph, workspace switching, whole-Editor exit transaction,
crash recovery, engine, game/runtime, deployment or release behavior.

Validation, September 13, 2026:
- PASSED: existing exact-pin Windows Profile Editor and Catalog.Tests build.
- PASSED: Catalog CTest, 541 passed of 543 discovered; all six EncounterAuthoringTests
  passed. Two Windows symlink-privilege cases were skipped, not passed.
- PASSED: native close runner, 28 docked/floating checks; normal Editor exit 0,
  aboutToQuit observed, no forced stop. Maximum automated close 0.593 seconds;
  full isolated fixture 66.972 seconds. Prompt image visually checked.
- PASSED: static lane, 918 Python tests plus 10 ExternalToolchain tests; 33
  conditional/platform privilege cases skipped. All selected validators passed.
- PASSED: ten SDK Gem source-policy validators, Python/PowerShell fixture syntax
  and reviewed-range whitespace.
- NOT_APPLICABLE: new configure (unchanged graph; exact-pin build cache reused),
  game runtime, deployment, installer and release proof for this increment.

Native loaded SDK DLL SHA-256:
9d4506d8893f3698102b47a509ada93b0ab9d84c9a58b51565691ad42de5ec04
Synthetic fixtures, catalog bytes, screenshot and validation logs remain outside
both source checkouts. No protected game data was read or written.

Next process: signed commit and new PR to main; report hosted CI independently.
Maintainer review and merge remain separate. No further feature is started.
