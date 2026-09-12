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
- closing the pane, switching workspaces or exiting the Editor offers **Save / Discard / Cancel** for actor, troop and member drafts;
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
0.453 seconds against a five-second synthetic fixture budget; this does not
establish performance for large user catalogs.

### Switching workspaces

Both **SDK Status → Open existing workspace...** and **Catalog Browser → Open
Workspace...** use the same protection for docked and floating Actor/Troop panes.
Foundation validates the destination before prompting. Cancel, Escape, dismissing
the prompt, cancelling the picker or an invalid destination preserves the current
workspace and drafts.

After opening a workspace, reopen the saved mod in Pack Manager before authoring.
Population saves require an active mod; a missing active mod is a failed Save and
keeps the current workspace open.

**Save** writes dirty actor and troop/member forms to the current workspace before
switching. A failed Save keeps that workspace open for correction or retry. If an
earlier actor save succeeds, it stays saved even when the troop save fails or
another pane later cancels the switch. **Discard** grants permission to replace the
workspace, but the drafts remain in memory until Foundation commits the change.
Another pane's veto or a failed destination reload therefore preserves them.

After a successful switch, selections, member staging and all draft forms reset,
including on same-workspace reloads. Saving before reloading the same workspace
reads the newly saved catalog and evidence. Matching record IDs in another
workspace never inherit the previous workspace's drafts. Clean panes do not prompt.

### Workspace-switch acceptance

Run both picker routes with separate fresh external output directories:

```powershell
& ./Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_actor_troop_close_smoke.ps1 `
  -EditorExecutable "$BuildRoot/bin/profile/Editor.exe" `
  -EngineRoot $EngineRoot -CacheRoot $CacheRoot -OutputRoot $StatusOutputRoot `
  -Suite workspace-status
& ./Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_actor_troop_close_smoke.ps1 `
  -EditorExecutable "$BuildRoot/bin/profile/Editor.exe" `
  -EngineRoot $EngineRoot -CacheRoot $CacheRoot -OutputRoot $CatalogOutputRoot `
  -Suite workspace-catalog
```

Each route requires 45 checks spanning docked and floating layouts, retained draft
fields and saved catalog bytes, partial saves, actual locked-file failures, retries,
staged member changes, later-pane vetoes, failed post-admission reloads and shared
IDs in separate roots. The runner also verifies the loaded module and normal
Editor exit. Its five-second per-interaction budget applies to this synthetic
fixture, not arbitrary catalog sizes.

The pinned Windows Profile acceptance passed all 90 workspace checks with normal
Editor exits. Maximum measured workspace interaction was 1.204 seconds.

### Exiting the entire Editor

**File → Exit** and the main Editor window close control offer **Save / Discard /
Cancel** for dirty Actor/Troop forms, in both docked and floating layouts. Cancel,
Escape and dismissing the prompt keep the Editor open with the drafts intact.
Failed validation or persistence also cancels exit so the remaining drafts can
be corrected or saved again. A nested exit request while a prompt is open is refused.

Save includes actor, troop, staged member additions/edits/removals and the current
unstaged member form. The actor saves first; a later troop failure keeps that
successful actor save. Clean forms do not prompt or get saved again.

Discard takes effect only when the entire Editor exit succeeds. If another pane
later cancels exit, the Actor/Troop pane reopens with its raw fields (including
invalid member text), selected records/member, staged changes and dirty flags.
Successful saves remain saved if a later pane cancels. The retained state belongs
only to this close attempt and the same workspace; it is not crash recovery.

### Whole-Editor exit acceptance

Run each suite with a separate fresh external output directory:

```powershell
& ./Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_actor_troop_close_smoke.ps1 `
  -EditorExecutable "$BuildRoot/bin/profile/Editor.exe" `
  -EngineRoot $EngineRoot -CacheRoot $CacheRoot -OutputRoot $FreshOutputRoot `
  -Suite exit-save
```

Repeat with `exit-discard`, `exit-clean` and `exit-rollback`. These native suites
exercise File → Exit, the main window close control and cancellation through
legacy Python exit; they do not use forced pane-close APIs. They check raw forms,
catalog bytes, actual locked-file failure and retry, partial saves, staged member
changes, repeated later-pane rollback and clean reopening after a successful save.
Each successful final case must load the expected module and exit normally.
The five-second interaction budget applies only to the synthetic fixture.

The pinned Windows Profile acceptance passed all 50 whole-exit checks with normal
Editor exits. The maximum recorded whole-exit interaction was 1.500 seconds.
Pane-close and both workspace-picker regressions also passed (123 checks).

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
