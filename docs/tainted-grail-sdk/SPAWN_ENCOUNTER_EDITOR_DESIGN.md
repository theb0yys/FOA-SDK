# Spawn and Encounter Editor

Status: local implementation and compiled/live Editor acceptance passed under the owner's approved encounter-authoring request. Maintainer review and merge remain separate.

## Scope and ownership

Significant authoring and persistence change owned by `content-pack-authoring`, supported by `catalog-and-identity`, `schemas-and-persistence`, `workspace-and-packs` and `ui-framework`. Core owns typed encounter definitions and composition analysis. Foundation owns active-pack commands, exact evidence, transactions and publication. The Editor selects existing actors/troops and presents draft analysis.

The accepted workflow creates a local encounter, selects actors or troops, sets quantities, a placement reference, activation conditions and population limits, previews its composition, and saves/reopens/edits it. Acceptance includes one captain and three guards, member removal, failed-save preservation, dirty-draft protection and a responsive running Editor.

## Durable contract

Catalog schema 3 adds `EncounterDefinitions`. Each definition attaches to a pack-owned canonical `population/encounter` record and contains stable entry IDs, exact actor/troop record IDs, bounded quantity ranges, an optional resolved world placement record or an unverified placement subject, activation mode and condition descriptions, maximum active instances, population limit, uniqueness and cleanup/rollback notes. Conditions and placement subjects describe authoring intent; they are not executable expressions or discovered game identities.

Saving replaces one complete encounter definition atomically. Entry omission removes only entries from that definition. Other encounters and actor/troop records are unchanged. Core rejects duplicate identities/targets, unsupported bindings, invalid bounds and insufficient population limits. Preview reports missing placement, unresolved troop members and other planning limitations. Foundation records exact local authoring intent and combines it with the existing source evidence establishing selected actors/troops. These records confer no runtime permission.

## Compatibility and recovery

Schema 1 and 2 catalogs remain readable, with unchanged identities, economy, population, governance and evidence data. Bound validation produces the current in-memory document; subsequent successful saves write schema 3. Before replacing an existing older catalog, persistence retains its exact bytes in a sibling migration backup. Backup failure blocks replacement. Schema 1/2 documents containing encounter definitions and future versions are rejected. Older schema-2 Editors reject schema 3 rather than silently discarding encounters. Downgrade requires restoring the retained pre-migration catalog; new encounter edits are not backported.

Workspace/pack schemas, the O3DE pin, Unity provider, game installation and runtime adapters do not change. No private or game-derived fixture is committed.

## Validation

Required: Core composition/identity/boundary regressions; exact evidence and pack checks; schema 1/2 migration and schema-3 deterministic round trips; malformed/future-version and failed-write/backup tests; registration/static/source-policy checks; pinned compiled suites; real Editor creation/edit/removal/preview/dirty-draft/save/reopen/error checks. Use an isolated copy of the existing authoring catalog for migration acceptance before opening normal user data. Measure catalog refresh and interactive edits, with a maximum UI timer gap below three seconds for the tested workflow.

The September 10, 2026 acceptance exercised the captain-and-three-guards plan in a private copy with 885 actors, 3,914 items and 356 recipes. Six live check groups passed, including migration, editing, invalid input, failed writes and workspace reopening; maximum measured UI timer gap was 2.86 seconds. The compiled catalog suite discovered 440 tests: 438 passed and two explicitly skipped unavailable Windows symlink privileges. All 39 canonical-interchange tests passed. Static/source-policy checks passed. These results establish the local authoring workflow only.

## Pane-close protection completion

The pane-close follow-up is a Routine UI change owned by `ui-framework`. A dirty pane offers
Save / Discard / Cancel; Cancel is the default and escape action. Save uses the existing complete
Foundation encounter command and accepts the close only after that command succeeds. Discard accepts
without publication. Cancel, prompt dismissal, a nested close request, validation failure or write
failure leaves the pane open with raw fields and composition staging intact. A scoped prompt guard
prevents a nested close from bypassing the decision. Clean close performs no save.

The pane-close increment changed no public contract, catalog schema or save transaction. The workspace
follow-up is described below; whole-Editor exit coordination and crash recovery remain separate tasks.

The dedicated native acceptance runner creates only disposable synthetic data outside product and
engine sources and uses an inactive private Windows desktop. It requires the pinned built Editor,
an already prepared asset cache, the expected loaded SDK module hash, nonzero acceptance checks and
normal Editor exit. Run with fresh external output paths:

```powershell
& Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_encounter_close_smoke.ps1 `
  -EditorExecutable '<external-build>/bin/profile/Editor.exe' `
  -EngineRoot '<pinned-engine>' -CacheRoot '<prepared-cache>' `
  -OutputRoot '<fresh-external-output>'
```

The fixture exercises actual docked and floating pane controls. It checks all three choices, Escape,
prompt dismissal, reentrancy, clean close, name-only saves, complete fields and composition edits,
stable entry identities, untouched records, invalid quantities, insufficient population limits,
missing conditions, empty composition, a real locked-file save failure and corrected retries.
Rejected saves and discarded drafts are compared with the exact previously saved catalog bytes.
Accepted closes must destroy the pane; rejected closes must preserve the visible pane and raw draft.
Each automated close has a five-second responsiveness budget including prompt interaction.

September 13, 2026 local close acceptance passed all 28 checks on the pinned Windows Profile Editor.
Maximum automated close was 0.593 seconds; the isolated fixture exited normally with code 0 after
66.972 seconds, with `aboutToQuit` observed and no forced stop. The prompt image was visually checked.
The Editor/Catalog build, static lane and ten SDK source-policy validators passed. Catalog CTest ran
543 tests: 541 passed, including all six encounter regressions, and two existing Windows symlink
privilege cases were explicitly skipped. Static Python tests passed 918 plus 10 ExternalToolchain
cases, with 33 conditional/platform skips. These results cover local authoring and pane closure.

## Workspace-switch protection

The Routine workspace follow-up is owned by `ui-framework`. Encounter Editor participates in the
existing Foundation admission/commit contract through `CanChangeWorkspace` and `OnWorkspaceChanged`.
It ignores callbacks from other Foundation service instances. The same prompt and reentrancy guard
serve pane closure and workspace admission; a nested pane close cannot bypass a workspace prompt.

Save runs the existing complete encounter command while the original workspace is still current.
Failure vetoes replacement with raw fields and add/edit/remove staging intact. Discard grants
admission without clearing the draft, so another participant's veto or a same-root candidate rebuild
failure does not lose it. An earlier successful Save remains published if a later participant vetoes.
Only the commit notification resets the form and selection, including same-workspace reloads.

Draft saving now checks the captured canonical workspace file and root as well as workspace/profile
IDs. A successful switch refreshes choices and clears old record selection even if the destination
reuses all those IDs. Foundation continues to own candidate validation, same-root freshness, admission
ordering and publication. No shared bus, schema, persistence format, build target or Foundation code
changes are needed. Reverting this follow-up needs no data migration.

Required proof: existing compiled workspace-admission and encounter-save regressions plus native
[workspace-switch acceptance](SPAWN_ENCOUNTER_EDITOR_GUIDE.md#workspace-switch-acceptance) for both
picker routes and pane layouts. The native fixture uses two roots with matching workspace/profile/
record IDs and distinct saved encounter values, an alias document for the original root, actual
write locks, raw form/catalog comparisons, observed later-handler order and a failed candidate rebuild.
The independent close suite guards the shared prompt. Generated fixtures and evidence remain external.

September 13, 2026 workspace acceptance passed 47 checks per picker route (94 total), including both
pane layouts. Maximum automated transitions were 0.984 s for Status and 0.985 s for Catalog Browser
against the five-second fixture budget. The independent close regression passed 28 checks with a
0.406 s maximum. All three final processes loaded the expected SDK module and exited normally with
code 0 and `aboutToQuit` observed. The workspace prompt image was visually checked. Initial fixture
snapshots included retired table editors awaiting Qt deferred deletion; corrected snapshots retain
exact checks on current cells and raw quantity text, and both final workspace runs passed.

The pinned Editor/Catalog build, static lane and ten SDK source-policy validators passed. Catalog
CTest passed 541 of 543 discovered tests, including 23 workspace-load tests and six encounter tests;
two existing Windows symlink-privilege cases were skipped. Static tests passed 918 Python and ten
ExternalToolchain cases with 33 conditional/platform skips. No runtime or deployment claim follows.

## Boundary

The pane authors and validates plans. It does not place scene entities, spawn actors, evaluate game conditions, launch Fall of Avalon, execute cleanup/rollback, deploy files, or mutate saves. Placement, conditions, cleanup and runtime activation need separately established native contracts before runtime implementation.
