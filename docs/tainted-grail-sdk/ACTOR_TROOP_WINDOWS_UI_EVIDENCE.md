# Actor and Troop Windows UI Evidence Checklist

Status: checklist implemented; exact-head Windows evidence remains pending.

## Relationship to the full UI smoke

The full Developer Preview checklist currently covers **twenty-six** TG SDK panes: the FOA Development Hub plus twenty-five specialist panes. This focused checklist adds Actor/Troop acceptance detail and does not reduce or replace the full checklist.

Use the same exact commit, Windows environment, privacy attestation, screenshot tooling, and no-runtime boundary defined by `DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md`.

## Preconditions

- Windows x64 Profile build of the exact reviewed commit;
- clean source checkout;
- successful local validation and compiled catalog tests;
- generated and verified deterministic population fixture;
- project-owned synthetic data only;
- no FoA process, game files, assets, saves, BepInEx, Harmony, deployment target, or private path visible.

## Prepare the fixture

```powershell
python Gems/TaintedGrailModdingSDK/Tools/population_preview_fixture.py generate `
  --output build/tg-sdk-population-preview-fixture
python Gems/TaintedGrailModdingSDK/Tools/population_preview_fixture.py verify `
  --output build/tg-sdk-population-preview-fixture
```

Open the generated `preview-population.tgworkspace.json` using the supported Editor workflow.

## Focused checks

### 1. Pane routes

Confirm **Tainted Grail Actor and Troop Editor** opens from:

- **Tools → Tainted Grail SDK**;
- the **FOA Development Hub** route.

The same registered pane instance and title must be used. No duplicate private editor should appear.

### 2. Actor records

Confirm the actor selector exposes:

- `preview.population.actor.leader`;
- `preview.population.actor.scout`.

Confirm filters are case-insensitive presentation controls only and do not alter canonical identity.

### 3. Resolved template

Select the leader actor and confirm:

- template record `preview.population.template.guard` is resolved;
- exact subject `subject:preview:population:template:guard` agrees with the canonical record;
- actor and template evidence is visible;
- no display-name inference is shown.

### 4. Unresolved template

Select the scout actor and confirm:

- `TemplateRecordId` is empty;
- exact unresolved subject `subject:preview:population:template:scout-unresolved` remains visible;
- the editor does not silently clear, guess, or promote the subject;
- the unresolved state affects readiness visibly.

### 5. Troop composition

Select `preview.population.troop.patrol` and confirm:

- minimum size 2 and maximum size 3;
- leader actor is `preview.population.actor.leader`;
- exactly one leader member row exists;
- the scout member row has minimum 1 and maximum 2;
- member roles, required state, weight, conditions, exact actor subject, and evidence are visible;
- no actor subject is duplicated.

### 6. Action lanes

For the leader, scout, and patrol, confirm lane order is:

1. display;
2. author profile;
3. compose troop;
4. planning;
5. spawn candidate;
6. runtime spawn;
7. save mutation.

Confirm fixture governance produces:

- leader `spawn_candidate`: allowed;
- patrol `spawn_candidate`: allowed;
- scout `spawn_candidate`: forbidden.

Confirm `runtime_spawn` and `save_mutation` remain unavailable for every record.

### 7. Dirty actor draft

Change a leader field without saving, then attempt to change actor selection.

Confirm:

- selection is restored or refused;
- an actionable message explains that the actor draft is dirty;
- no draft data is lost;
- save and revert remain explicit.

### 8. Dirty troop draft

Change a troop field without saving, then attempt to change troop selection.

Confirm the same refusal and restoration behavior.

### 9. Dirty member draft

Edit a staged member without applying or reverting it, then select another member.

Confirm the member selection is refused and the draft remains intact.

### 10. Deferred Foundation refresh

With any population draft dirty, trigger a safe Foundation refresh by changing a non-population workspace-visible value through the supported workflow.

Confirm:

- the Actor/Troop pane does not overwrite the draft;
- refresh is marked pending;
- save or revert allows the pending refresh to complete;
- no partial population candidate is published.

### 11. Safe negative validation

Using synthetic data, attempt one reversible invalid edit, such as:

- member minimum greater than maximum;
- duplicate exact actor subject in the same troop;
- leader row not matching the troop profile leader;
- troop/member ranges with no overlap;
- missing exact evidence.

Confirm save fails with an actionable subject-specific message and the published catalog remains unchanged.

### 12. Save, close, reopen

Save a valid actor and troop change, close the pane and Editor, reopen the same workspace, and confirm:

- schema-2 actor profiles persist;
- troop profile persists;
- exact member rows persist;
- resolved and unresolved template bindings persist;
- canonical order remains stable;
- action lanes are re-derived from durable state;
- transient filters and selection are not treated as catalog authority.

### 13. Runtime boundary

Confirm the pane exposes no:

- spawn, despawn, encounter execution, or runtime entity control;
- FoA, BepInEx, Harmony, Mono, or IL2CPP invocation;
- deployment, file-copy, archive, launch, or target-filesystem action;
- save-game mutation;
- automatic evidence promotion, validation, governance permission, or blocker clearing.

## Evidence notes

Record focused results under the main evidence directory using notes that identify:

- exact commit;
- fixture ID `preview.population-authoring-0`;
- actor/troop IDs exercised;
- negative validation used;
- persistence result;
- runtime-boundary confirmation.

Recommended screenshot coverage:

- leader with resolved template and action lanes;
- scout with unresolved template and forbidden spawn candidacy;
- patrol troop and exact member table;
- actionable negative validation;
- save/close/reopen state.

Do not commit screenshots. Attach them only through the existing reviewed evidence workflow.

## Acceptance

This focused checklist passes only when every check above passes on the same exact commit that passed configure, build, compiled tests, fixture verification, and the full twenty-six-pane checklist.
