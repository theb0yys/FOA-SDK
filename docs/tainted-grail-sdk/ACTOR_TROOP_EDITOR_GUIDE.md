# Actor and Troop Editor Guide

Status: authoring completion implemented and validated in the running Windows Editor.

## Purpose

The **Tainted Grail Actor and Troop Editor** is the governed population-authoring surface for project-owned catalog data. It edits durable actor profiles, troop profiles, and troop-member rows through the shared workspace, pack, source/evidence, catalog, validation, governance, blocker, and persistence systems.

It reads supported serialized NPC templates from the configured installation and creates local authoring definitions. Spawning, encounters, deployment and save editing are separate features.

## Open the pane

1. Build and launch the dedicated TG SDK Editor project.
2. Open or create a workspace with one active profile and pack.
3. Use **Choose or create mod** to save your authoring mod. Game loading works without a mod; local creation and edits require one.
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

1. Press **Load game actors**, or **New actor** and enter a name. Search for an existing actor using the actor filter.
2. Choose a saved compatible mod. Native actor edits are local catalog changes; custom actors belong to the mod that created them.
3. Choose an actor kind and enter the typed profile fields.
4. Select either a resolved actor-template record or an exact unresolved template subject. Do not invent a display-name-based binding.
5. Imported and newly created actors already have source evidence. Expand **Exact-subject Actor Evidence** when reviewing or changing template bindings.
6. Review validation, staleness, governance, blockers, and the seven action lanes.
7. Save only when the candidate remains complete and the persistence transaction succeeds.

A failed evidence, integrity, path, or persistence check leaves the published catalog unchanged.

### Portrait preview

Place an authoring PNG or JPEG inside the workspace (for example, `Assets/Portraits/guard.png`). Press **Choose workspace portrait**, select it, then save the actor. The catalog stores a portable `$workspace/` reference. The preview accepts images up to 8 MiB and 4096 by 4096 pixels. Missing, corrupt, unsupported and escaping paths clear the previous image and show a reason.

The supported NPC template component contains no portrait, model or localisation binding. Its `npcData` reference contains perception/alert data. Imported actors therefore show an explicit missing-portrait state until a local image is assigned. This slice does not reconstruct a game character's equipped appearance or render Unity prefabs.

### Supported game fields

The isolated reader recognizes NPC components by their serialized field shape in `templates.npc_assets_all.bundle`. It imports exact identities, levels from 1 to 1000, and tags; abstract templates remain tagged. Native enum meanings are not guessed: actor kind starts as `other` and archetype as `native-npc-template`. Shop entries, unsupported levels and malformed fields are listed in the private reader report. Refresh preserves existing canonical IDs and authored values. **Cancel loading** leaves the saved catalog unchanged.

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

1. Press **New troop**, choose its first leader from saved actors, and enter a name; or select an existing troop. Creation saves a valid one-member troop.
2. Choose a typed leader actor and confirm its exact subject.
3. Press **New member / clear form**, choose an actor, role, required state, count range, finite non-negative weight and conditions, then **Stage Member in Definition**. Link IDs and exact authoring-intent evidence are generated automatically. Select an existing row to change it.
4. Confirm that an actor subject appears no more than once in the troop.
5. Confirm that exactly one typed leader row matches the troop profile leader.
6. Confirm that member count ranges overlap the troop size range.
7. Use **Remove selected member** to stage a removal. Adjust the leader and size range as needed, then save the complete definition as one transaction. Revert restores the saved composition.

The existing upsert API remains additive. Only IDs explicitly listed in the command's removal collection are removed; missing, duplicate, conflicting and cross-troop removals are rejected. A troop must still have a valid leader and composition after removal.

## Action lanes

Expand the actor or troop action-lane group to inspect the current saved definition. Collapsed review tables are populated when opened, so catalog intake does not rebuild invisible tables. Every selected actor or troop uses the same deterministic lane order:

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
- closing the pane offers **Save / Discard / Cancel** for actor, troop and member drafts;
- each save command publishes only after its validation and persistence succeed.

Save includes the current unstaged member form and staged member additions, edits
and removals. It saves the actor first, then the complete troop definition through
the existing atomic troop command. These are separate transactions: if the troop
save fails after the actor succeeds, the actor remains saved and the troop draft
stays open for correction or retry.

Cancel, Escape and closing the prompt keep the pane open without saving or
discarding anything. Failed validation or persistence also keeps it open.
Discard closes the pane without saving its remaining drafts. Clean panes close
without a prompt. Both docked and floating pane close controls use this protection.

### Pane-close acceptance

Run the native regression against a built pinned Windows Profile Editor and a
prepared asset cache. Use a fresh output directory outside the product and engine
source trees:

```powershell
& ./Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_actor_troop_close_smoke.ps1 `
  -EditorExecutable "$BuildRoot/bin/profile/Editor.exe" `
  -EngineRoot $EngineRoot -CacheRoot $CacheRoot -OutputRoot $FreshOutputRoot
```

The runner uses an inactive private desktop and synthetic workspace. Its 33
checks cover docked/floating native close controls, individual and combined
drafts, cancellation without writes, failed validation, actual locked-catalog
write failures and retries, staged membership changes, and clean reopening.
A pass requires the expected loaded module hash and normal Editor exit. The
recorded acceptance completed all checks with a maximum close transition of
0.610 seconds against a five-second synthetic fixture budget; this does not
establish performance for large user catalogs.

## Persistence expectations

Durable data belongs in the canonical workspace catalog. Actor profiles, troop profiles, and troop-member rows are schema-2 catalog collections. Save, close, and reopen must preserve byte-equivalent typed state after canonical ordering.

Transient UI state, filters, selection, and draft values are not durable catalog authority.

## Verified completion

The pinned Windows Profile Editor loaded 885 supported NPC templates alongside 3,914 items and 356 recipes. Eight live checks covered initial intake, expanded review tables, local creation, portrait pixels, troop member changes and removals, invalid-save rollback, dirty drafts, cancellation, reopening and pane width. First intake took 3.046 seconds with a maximum UI timer gap of 1.938 seconds, below the three-second acceptance limit. Static validation and both mandatory compiled suites also passed; explicit platform/privilege skips remain reported separately in the task evidence.

The supported NPC bundle also contained nine level-zero templates that cannot map to the positive authoring level range and 64 non-NPC entries. These remain explicitly unsupported. The measured result concerns this inspected profile and authoring workflow.

## Current limits

The current editor does not:

- copy game assets into the source repository or modify the installation;
- resolve actor/template identity from display names;
- create encounters, routes, pools, lifecycle rules, or density systems;
- spawn or despawn runtime actors;
- invoke BepInEx, Harmony, Mono, IL2CPP, or a game process;
- deploy files or mutate saves;
- grant validation or governance permission automatically.

The [Spawn and Encounter Editor](SPAWN_ENCOUNTER_EDITOR_GUIDE.md) consumes these saved records for local composition plans. Separately reviewed runtime adapters must preserve the same boundary.
