# Actor and Troop Editor Guide

Status: implemented authoring surface; exact-head Windows host verification remains pending.

## Purpose

The **Tainted Grail Actor and Troop Editor** is the governed population-authoring surface for project-owned catalog data. It edits durable actor profiles, troop profiles, and troop-member rows through the shared workspace, pack, source/evidence, catalog, validation, governance, blocker, and persistence systems.

It is not a runtime actor spawner, encounter executor, game-data extractor, deployment tool, or save editor.

## Open the pane

1. Build and launch the dedicated TG SDK Editor project.
2. Open or create a workspace with one active profile and pack.
3. Import project-owned source/evidence before authoring population records.
4. Open **Tools → Tainted Grail SDK → Tainted Grail Actor and Troop Editor**.

The pane is also reachable from the **FOA Development Hub**.

## Actor workflow

The actor side of the pane provides:

- canonical actor-record selection and filtering;
- typed actor kind and archetype fields;
- level range, uniqueness, persistence, and essential-state fields;
- model, portrait, localisation-name, and localisation-description references;
- resolved template selection by canonical record ID;
- unresolved exact template-subject preservation when no canonical template exists;
- exact evidence details, governance summary, blockers, relationships, and action lanes;
- explicit save and revert controls.

### Author an actor profile

1. Select a canonical population record whose `RecordKind` is `actor`.
2. Confirm that the record belongs to the active pack and exact workspace/profile context.
3. Choose an actor kind and enter the typed profile fields.
4. Select either a resolved actor-template record or an exact unresolved template subject. Do not invent a display-name-based binding.
5. Bind evidence proving every required exact subject.
6. Review validation, staleness, governance, blockers, and the seven action lanes.
7. Save only when the candidate remains complete and the persistence transaction succeeds.

A failed evidence, integrity, path, or persistence check leaves the published catalog unchanged.

## Troop workflow

The troop side provides:

- canonical troop-record selection and filtering;
- typed troop kind, formation, minimum size, and maximum size;
- exact leader actor record and subject bindings;
- a draft membership table;
- typed member role, required state, count range, weight, conditions, actor binding, and evidence;
- evidence, governance, blocker, relationship, and action-lane summaries;
- atomic troop-definition save and explicit revert.

### Compose a troop

1. Select a canonical `troop` record.
2. Choose a typed leader actor and confirm its exact subject.
3. Stage each member row with a stable link ID, typed role, required state, minimum/maximum count, finite non-negative weight, sorted conditions, exact actor record/subject, and evidence.
4. Confirm that an actor subject appears no more than once in the troop.
5. Confirm that exactly one typed leader row matches the troop profile leader.
6. Confirm that member count ranges overlap the troop size range.
7. Save the complete definition as one transaction.

Omitted existing member rows are not silently deleted by the bootstrap authoring service. Removal requires a separately reviewed contract.

## Action lanes

Every selected actor or troop displays the same deterministic lane order:

1. `display`;
2. `author_profile`;
3. `compose_troop`;
4. `planning`;
5. `spawn_candidate`;
6. `runtime_spawn`;
7. `save_mutation`.

Possible states are derived from canonical data, evidence, validation, staleness, governance, blockers, and typed profile completeness. A forbidden usage takes precedence over an allowed usage.

`runtime_spawn` and `save_mutation` remain unavailable in this editor slice. An allowed `spawn_candidate` lane is metadata readiness only; it does not spawn anything.

## Deterministic population fixture

The repository includes `Preview/PopulationTemplate`, a project-owned schema-2 fixture with:

- two typed actors;
- one actor template;
- one patrol troop;
- two exact member rows;
- one resolved template relationship;
- one unresolved template subject;
- exact source/evidence bindings;
- allowed leader and patrol `spawn_candidate` examples;
- one forbidden scout `spawn_candidate` example.

Generate and verify it with:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/population_preview_fixture.py generate `
  --output build/tg-sdk-population-preview-fixture
python Gems/TaintedGrailModdingSDK/Tools/population_preview_fixture.py verify `
  --output build/tg-sdk-population-preview-fixture
```

Generation writes canonical UTF-8 JSON and a sorted SHA-256 manifest. Verification rejects drift, unsafe paths, symlinks, unexpected files, schema downgrade, incorrect evidence subjects, malformed composition, and governance mismatch.

## Dirty-draft protection

The pane protects unsaved work:

- record, troop, and member selection changes are refused while the corresponding draft is dirty;
- Foundation refreshes are deferred until drafts are saved or reverted;
- closing the pane prompts before discarding unsaved drafts;
- save failures do not publish partial state.

## Persistence expectations

Durable data belongs in the canonical workspace catalog. Actor profiles, troop profiles, and troop-member rows are schema-2 catalog collections. Save, close, and reopen must preserve byte-equivalent typed state after canonical ordering.

Transient UI state, filters, selection, and draft values are not durable catalog authority.

## Current limits

The current editor does not:

- inspect or copy proprietary FoA content;
- resolve actor/template identity from display names;
- create encounters, routes, pools, lifecycle rules, or density systems;
- spawn or despawn runtime actors;
- invoke BepInEx, Harmony, Mono, IL2CPP, or a game process;
- deploy files or mutate saves;
- grant validation or governance permission automatically.

The later Spawn and Encounter Editor and separately reviewed runtime adapters must consume these records without weakening this boundary.
