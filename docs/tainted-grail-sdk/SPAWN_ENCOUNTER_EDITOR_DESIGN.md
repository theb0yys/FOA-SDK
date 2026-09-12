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

## Boundary

The pane authors and validates plans. It does not place scene entities, spawn actors, evaluate game conditions, launch Fall of Avalon, execute cleanup/rollback, deploy files, or mutate saves. Placement, conditions, cleanup and runtime activation need separately established native contracts before runtime implementation.
